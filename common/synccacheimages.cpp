/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "synccacheimages.h"
#include "synccacheimages_p.h"
#include "synccacheimagedownloads_p.h"

#include <QtCore/QThread>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QDebug>
#include <QtGui/QImage>

using namespace SyncCache;

User& User::operator=(const User &other)
{
    if (this == &other) {
        return *this;
    }

    accountId = other.accountId;
    userId = other.userId;
    displayName = other.displayName;
    thumbnailUrl = other.thumbnailUrl;
    thumbnailPath = other.thumbnailPath;
    thumbnailFileName = other.thumbnailFileName;

    return *this;
}

Album& Album::operator=(const Album &other)
{
    if (this == &other) {
        return *this;
    }

    accountId = other.accountId;
    userId = other.userId;
    albumId = other.albumId;
    parentAlbumId = other.parentAlbumId;
    albumName = other.albumName;
    photoCount = other.photoCount;
    thumbnailUrl = other.thumbnailUrl;
    thumbnailPath = other.thumbnailPath;
    thumbnailFileName = other.thumbnailFileName;
    etag = other.etag;

    return *this;
}

Photo& Photo::operator=(const Photo &other)
{
    if (this == &other) {
        return *this;
    }

    accountId = other.accountId;
    userId = other.userId;
    albumId = other.albumId;
    photoId = other.photoId;
    createdTimestamp = other.createdTimestamp;
    updatedTimestamp = other.updatedTimestamp;
    fileName = other.fileName;
    albumPath = other.albumPath;
    description = other.description;
    thumbnailUrl = other.thumbnailUrl;
    thumbnailPath = other.thumbnailPath;
    imageUrl = other.imageUrl;
    imagePath = other.imagePath;
    imageWidth = other.imageWidth;
    imageHeight = other.imageHeight;
    fileSize = other.fileSize;
    fileType = other.fileType;
    etag = other.etag;

    return *this;
}

//-----------------------------------------------------------------------------

ImageCacheThreadWorker::ImageCacheThreadWorker(QObject *parent)
    : QObject(parent)
    , m_db(nullptr, false) // don't emit changes to other processes
    , m_downloader(nullptr)
{
}

ImageCacheThreadWorker::~ImageCacheThreadWorker()
{
}

void ImageCacheThreadWorker::openDatabase(const QString &accountType)
{
    DatabaseError error;
    m_db.openDatabase(
            QStringLiteral("%1/%2.db").arg(ImageCache::imageCacheRootDir(), accountType),
            &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit openDatabaseFailed(error.errorMessage);
    } else {
        connect(&m_db, &ImageDatabase::usersStored,
                this, &ImageCacheThreadWorker::usersStored);
        connect(&m_db, &ImageDatabase::albumsStored,
                this, &ImageCacheThreadWorker::albumsStored);
        connect(&m_db, &ImageDatabase::photosStored,
                this, &ImageCacheThreadWorker::photosStored);
        connect(&m_db, &ImageDatabase::usersDeleted,
                this, &ImageCacheThreadWorker::usersDeleted);
        connect(&m_db, &ImageDatabase::albumsDeleted,
                this, &ImageCacheThreadWorker::albumsDeleted);
        connect(&m_db, &ImageDatabase::photosDeleted,
                this, &ImageCacheThreadWorker::photosDeleted);
        connect(&m_db, &ImageDatabase::dataChanged,
                this, &ImageCacheThreadWorker::dataChanged);
        emit openDatabaseFinished();
    }
}

void ImageCacheThreadWorker::requestUsers()
{
    DatabaseError error;
    QVector<SyncCache::User> users = m_db.users(&error);
    if (error.errorCode != DatabaseError::NoError) {
        emit requestUsersFailed(error.errorMessage);
    } else {
        emit requestUsersFinished(users);
    }
}

void ImageCacheThreadWorker::requestUser(int accountId, const QString &userId)
{
    DatabaseError error;
    SyncCache::User user = m_db.user(accountId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit requestUserFailed(accountId, userId, error.errorMessage);
    } else {
        emit requestUserFinished(accountId, userId, user);
    }
}

void ImageCacheThreadWorker::requestAlbums(int accountId, const QString &userId)
{
    DatabaseError error;
    QVector<SyncCache::Album> albums = m_db.albums(accountId, userId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit requestAlbumsFailed(accountId, userId, error.errorMessage);
    } else {
        emit requestAlbumsFinished(accountId, userId, albums);
    }
}

void ImageCacheThreadWorker::requestPhotos(int accountId, const QString &userId, const QString &albumId)
{
    DatabaseError error;
    QVector<SyncCache::Photo> photos = m_db.photos(accountId, userId, albumId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit requestPhotosFailed(accountId, userId, albumId, error.errorMessage);
    } else {
        emit requestPhotosFinished(accountId, userId, albumId, photos);
    }
}

void ImageCacheThreadWorker::requestPhotoCount(int accountId, const QString &userId)
{
    DatabaseError error;
    SyncCache::PhotoCounter photoCounter = m_db.photoCount(accountId, userId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit requestPhotoCountFailed(accountId, userId, error.errorMessage);
    } else {
        emit requestPhotoCountFinished(accountId, userId, photoCounter.count);
    }
}

void ImageCacheThreadWorker::populateUserThumbnail(int idempToken, int accountId, const QString &userId, const QNetworkRequest &requestTemplate)
{
    Q_UNUSED(requestTemplate)

    DatabaseError error;
    User user = m_db.user(accountId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit populateUserThumbnailFailed(idempToken, QStringLiteral("Error occurred while reading user %1 info from db: %2")
                                         .arg(userId)
                                         .arg(error.errorMessage));
        return;
    }

    // the thumbnail already exists.
    const QString thumbnailPath = user.thumbnailPath.toString();
    if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath)) {
        emit populateUserThumbnailFinished(idempToken, thumbnailPath);
        return;
    }

    // Thumbnail downloading is not supported at the moment. This is not an error,
    // so just return an empty string.
    emit populateUserThumbnailFinished(idempToken, QString());
}

void ImageCacheThreadWorker::populateAlbumThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QNetworkRequest &requestTemplate)
{
    Q_UNUSED(requestTemplate)

    DatabaseError error;
    Album album = m_db.album(accountId, userId, albumId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit populateAlbumThumbnailFailed(idempToken, QStringLiteral("Error occurred while reading album %1 info from db: %2")
                                          .arg(albumId)
                                          .arg(error.errorMessage));
        return;
    }

    // the thumbnail already exists.
    QString thumbnailPath = album.thumbnailPath.toString();
    if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath)) {
        emit populateAlbumThumbnailFinished(idempToken, thumbnailPath);
        return;
    }

    thumbnailPath = m_db.findThumbnailForAlbum(accountId, userId, albumId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        qWarning() << "Unable to fetch album thumbnail" << thumbnailPath << ":"
                   << error.errorCode
                   << error.errorMessage;
    } else if (!thumbnailPath.isEmpty()) {
        album.thumbnailPath = thumbnailPath;
        m_db.storeAlbum(album, &error);
        if (error.errorCode != DatabaseError::NoError) {
            emit populateAlbumThumbnailFinished(idempToken, thumbnailPath);
        } else {
            emit populateAlbumThumbnailFailed(idempToken, QStringLiteral("Cannot save thumbnail %1 for album %2 to db: %3")
                                              .arg(thumbnailPath)
                                              .arg(albumId)
                                              .arg(error.errorMessage));
        }
        return;
    }

    // Thumbnail downloading is not supported at the moment. This is not an error,
    // so just return an empty string.
    emit populateAlbumThumbnailFinished(idempToken, QString());
}

void ImageCacheThreadWorker::populatePhotoThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate)
{
    Q_UNUSED(requestTemplate)

    DatabaseError error;
    Photo photo = m_db.photo(accountId, userId, albumId, photoId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit populatePhotoThumbnailFailed(idempToken, QStringLiteral("Error occurred while reading photo %1 info from db: %2")
                                          .arg(photoId)
                                          .arg(error.errorMessage));
        return;
    }

    // the thumbnail already exists.
    QString thumbnailPath = photo.thumbnailPath.toString();
    if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath)) {
        emit populatePhotoThumbnailFinished(idempToken, thumbnailPath);
        return;
    }

    // the full-size photo exists, so use that.
    if (QFile::exists(photo.imagePath.toString())) {
        emit populatePhotoThumbnailFinished(idempToken, photo.imagePath.toString());
        return;
    }

    // Thumbnail downloading is not supported at the moment. This is not an error,
    // so just return an empty string.
    emit populatePhotoThumbnailFinished(idempToken, QString());
}

void ImageCacheThreadWorker::photoThumbnailDownloadFinished(int idempToken, const SyncCache::Photo &photo, const QUrl &filePath)
{
    DatabaseError storeError;
    Photo photoToStore = photo;
    photoToStore.thumbnailPath = filePath;
    m_db.storePhoto(photoToStore, &storeError);

    if (storeError.errorCode != DatabaseError::NoError) {
        QFile::remove(filePath.toString());
        emit populatePhotoThumbnailFailed(idempToken, storeError.errorMessage);
    } else {
        emit populatePhotoThumbnailFinished(idempToken, filePath.toString());
    }
}

void ImageCacheThreadWorker::populatePhotoImage(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate)
{
    DatabaseError error;
    Photo photo = m_db.photo(accountId, userId, albumId, photoId, &error);
    if (error.errorCode != DatabaseError::NoError) {
        emit populatePhotoImageFailed(idempToken, QStringLiteral("Error occurred while reading photo info from db: %1").arg(error.errorMessage));
        return;
    }

    // the image already exists.
    const QString imagePath = photo.imagePath.toString();
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        emit populatePhotoImageFinished(idempToken, imagePath);
        return;
    }

    // download the image.
    if (photo.imageUrl.isEmpty()) {
        emit populatePhotoImageFailed(idempToken, QStringLiteral("Empty image url specified for photo %1").arg(photoId));
        return;
    }

    if (!m_downloader) {
        m_downloader = new ImageDownloader(this);
    }

    ImageDownloadWatcher *watcher = m_downloader->downloadImage(
                idempToken,
                photo.imageUrl,
                photo.fileName,
                SyncCache::albumImageDownloadDir(accountId, photo.albumPath, false),
                requestTemplate);

    connect(watcher, &ImageDownloadWatcher::downloadFailed, this, [this, watcher, idempToken] (const QString &errorMessage) {
        emit populatePhotoImageFailed(idempToken, errorMessage);
        watcher->deleteLater();
    });

    connect(watcher, &ImageDownloadWatcher::downloadFinished, this, [this, watcher, photo, idempToken, accountId] (const QUrl &filePath) {
        // the file has been downloaded to disk.  attempt to update the database.
        DatabaseError storeError;
        Photo photoToStore = photo;
        photoToStore.imagePath = filePath;

        bool thumbnailAdded = false;
        if (photoToStore.thumbnailPath.isEmpty()) {
            // Use the full-size photo as the thumbnail as well.
            photoToStore.thumbnailPath = photoToStore.imagePath;
            thumbnailAdded = true;
        }

        m_db.storePhoto(photoToStore, &storeError);

        if (storeError.errorCode != DatabaseError::NoError) {
            QFile::remove(filePath.toString());
            emit populatePhotoImageFailed(idempToken, storeError.errorMessage);
        } else {
            if (thumbnailAdded) {
                emit populatePhotoThumbnailFinished(idempToken, photoToStore.thumbnailPath.toString());
            }
            emit populatePhotoImageFinished(idempToken, filePath.toString());

            // If the album doesn't have a thumbnail yet, use this photo as the thumbnail.
            Album album = m_db.album(accountId, photo.userId, photo.albumId, &storeError);
            if (storeError.errorCode == DatabaseError::NoError&& album.thumbnailPath.isEmpty()) {
                album.thumbnailPath = photoToStore.thumbnailPath;
                m_db.storeAlbum(album, &storeError);
                if (storeError.errorCode == DatabaseError::NoError) {
                    emit populateAlbumThumbnailFinished(idempToken, photoToStore.thumbnailPath.toString());
                } else {
                    qWarning() << "Unable to store photo as album thumbnail"
                               << storeError.errorCode << storeError.errorMessage;
                }
            }
        }

        watcher->deleteLater();
    });
}

//-----------------------------------------------------------------------------

ImageCachePrivate::ImageCachePrivate(ImageCache *parent)
    : QObject(parent), m_worker(new ImageCacheThreadWorker)
{
    qRegisterMetaType<SyncCache::User>();
    qRegisterMetaType<SyncCache::Album>();
    qRegisterMetaType<SyncCache::Photo>();
    qRegisterMetaType<QVector<SyncCache::User> >();
    qRegisterMetaType<QVector<SyncCache::User> >();
    qRegisterMetaType<QVector<SyncCache::User> >();

    m_worker->moveToThread(&m_dbThread);
    connect(&m_dbThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &ImageCachePrivate::openDatabase, m_worker, &ImageCacheThreadWorker::openDatabase);
    connect(this, &ImageCachePrivate::requestUser, m_worker, &ImageCacheThreadWorker::requestUser);
    connect(this, &ImageCachePrivate::requestUsers, m_worker, &ImageCacheThreadWorker::requestUsers);
    connect(this, &ImageCachePrivate::requestAlbums, m_worker, &ImageCacheThreadWorker::requestAlbums);
    connect(this, &ImageCachePrivate::requestPhotos, m_worker, &ImageCacheThreadWorker::requestPhotos);
    connect(this, &ImageCachePrivate::requestPhotoCount, m_worker, &ImageCacheThreadWorker::requestPhotoCount);

    connect(this, &ImageCachePrivate::populateUserThumbnail, m_worker, &ImageCacheThreadWorker::populateUserThumbnail);
    connect(this, &ImageCachePrivate::populateAlbumThumbnail, m_worker, &ImageCacheThreadWorker::populateAlbumThumbnail);
    connect(this, &ImageCachePrivate::populatePhotoThumbnail, m_worker, &ImageCacheThreadWorker::populatePhotoThumbnail);
    connect(this, &ImageCachePrivate::populatePhotoImage, m_worker, &ImageCacheThreadWorker::populatePhotoImage);

    connect(m_worker, &ImageCacheThreadWorker::openDatabaseFailed, parent, &ImageCache::openDatabaseFailed);
    connect(m_worker, &ImageCacheThreadWorker::openDatabaseFinished, parent, &ImageCache::openDatabaseFinished);
    connect(m_worker, &ImageCacheThreadWorker::requestUserFailed, parent, &ImageCache::requestUserFailed);
    connect(m_worker, &ImageCacheThreadWorker::requestUserFinished, parent, &ImageCache::requestUserFinished);
    connect(m_worker, &ImageCacheThreadWorker::requestUsersFailed, parent, &ImageCache::requestUsersFailed);
    connect(m_worker, &ImageCacheThreadWorker::requestUsersFinished, parent, &ImageCache::requestUsersFinished);
    connect(m_worker, &ImageCacheThreadWorker::requestAlbumsFailed, parent, &ImageCache::requestAlbumsFailed);
    connect(m_worker, &ImageCacheThreadWorker::requestAlbumsFinished, parent, &ImageCache::requestAlbumsFinished);
    connect(m_worker, &ImageCacheThreadWorker::requestPhotosFailed, parent, &ImageCache::requestPhotosFailed);
    connect(m_worker, &ImageCacheThreadWorker::requestPhotosFinished, parent, &ImageCache::requestPhotosFinished);
    connect(m_worker, &ImageCacheThreadWorker::requestPhotoCountFailed, parent, &ImageCache::requestPhotoCountFailed);
    connect(m_worker, &ImageCacheThreadWorker::requestPhotoCountFinished, parent, &ImageCache::requestPhotoCountFinished);

    connect(m_worker, &ImageCacheThreadWorker::populateUserThumbnailFailed, parent, &ImageCache::populateUserThumbnailFailed);
    connect(m_worker, &ImageCacheThreadWorker::populateUserThumbnailFinished, parent, &ImageCache::populateUserThumbnailFinished);
    connect(m_worker, &ImageCacheThreadWorker::populateAlbumThumbnailFailed, parent, &ImageCache::populateAlbumThumbnailFailed);
    connect(m_worker, &ImageCacheThreadWorker::populateAlbumThumbnailFinished, parent, &ImageCache::populateAlbumThumbnailFinished);
    connect(m_worker, &ImageCacheThreadWorker::populatePhotoThumbnailFailed, parent, &ImageCache::populatePhotoThumbnailFailed);
    connect(m_worker, &ImageCacheThreadWorker::populatePhotoThumbnailFinished, parent, &ImageCache::populatePhotoThumbnailFinished);
    connect(m_worker, &ImageCacheThreadWorker::populatePhotoImageFailed, parent, &ImageCache::populatePhotoImageFailed);
    connect(m_worker, &ImageCacheThreadWorker::populatePhotoImageFinished, parent, &ImageCache::populatePhotoImageFinished);

    connect(m_worker, &ImageCacheThreadWorker::usersStored, parent, &ImageCache::usersStored);
    connect(m_worker, &ImageCacheThreadWorker::albumsStored, parent, &ImageCache::albumsStored);
    connect(m_worker, &ImageCacheThreadWorker::photosStored, parent, &ImageCache::photosStored);
    connect(m_worker, &ImageCacheThreadWorker::usersDeleted, parent, &ImageCache::usersDeleted);
    connect(m_worker, &ImageCacheThreadWorker::albumsDeleted, parent, &ImageCache::albumsDeleted);
    connect(m_worker, &ImageCacheThreadWorker::photosDeleted, parent, &ImageCache::photosDeleted);
    connect(m_worker, &ImageCacheThreadWorker::dataChanged, parent, &ImageCache::dataChanged);

    m_dbThread.start();
    m_dbThread.setPriority(QThread::IdlePriority);
}

ImageCachePrivate::~ImageCachePrivate()
{
    m_dbThread.quit();
    m_dbThread.wait();
}

//-----------------------------------------------------------------------------

ImageCache::ImageCache(QObject *parent)
    : QObject(parent), d_ptr(new ImageCachePrivate(this))
{
}

ImageCache::~ImageCache()
{
}

QString ImageCache::imageCacheDir(int accountId)
{
    return SyncCache::imageDownloadDir(accountId);
}

QString ImageCache::imageCacheRootDir()
{
    return QStringLiteral("%1/system/privileged/Images").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
}

void ImageCache::openDatabase(const QString &databaseFile)
{
    Q_D(ImageCache);
    emit d->openDatabase(databaseFile);
}

void ImageCache::requestUser(int accountId, const QString &userId)
{
    Q_D(ImageCache);
    emit d->requestUser(accountId, userId);
}

void ImageCache::requestUsers()
{
    Q_D(ImageCache);
    emit d->requestUsers();
}

void ImageCache::requestAlbums(int accountId, const QString &userId)
{
    Q_D(ImageCache);
    emit d->requestAlbums(accountId, userId);
}

void ImageCache::requestPhotos(int accountId, const QString &userId, const QString &albumId)
{
    Q_D(ImageCache);
    emit d->requestPhotos(accountId, userId, albumId);
}

void ImageCache::requestPhotoCount(int accountId, const QString &userId)
{
    Q_D(ImageCache);
    emit d->requestPhotoCount(accountId, userId);
}

void ImageCache::populateUserThumbnail(int idempToken, int accountId, const QString &userId, const QNetworkRequest &requestTemplate)
{
    Q_D(ImageCache);
    emit d->populateUserThumbnail(idempToken, accountId, userId, requestTemplate);
}

void ImageCache::populateAlbumThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QNetworkRequest &requestTemplate)
{
    Q_D(ImageCache);
    emit d->populateAlbumThumbnail(idempToken, accountId, userId, albumId, requestTemplate);
}

void ImageCache::populatePhotoThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate)
{
    Q_D(ImageCache);
    emit d->populatePhotoThumbnail(idempToken, accountId, userId, albumId, photoId, requestTemplate);
}

void ImageCache::populatePhotoImage(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate)
{
    Q_D(ImageCache);
    emit d->populatePhotoImage(idempToken, accountId, userId, albumId, photoId, requestTemplate);
}
