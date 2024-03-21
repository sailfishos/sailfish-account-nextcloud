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

#include "synccacheimagechangenotifier_p.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUuid>
#include <QtCore/QDebug>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

using namespace SyncCache;

namespace {

QString constructAlbumIdentifier(int accountId, const QString &userId, const QString &albumId)
{
    return QStringLiteral("%1|%2|%3").arg(accountId).arg(userId, albumId);
}

bool upgradeVersion1to2Fn(QSqlDatabase &database)
{
    QSqlQuery addFileSizeQuery(QStringLiteral("ALTER TABLE Photos ADD fileSize INTEGER;"), database);
    if (addFileSizeQuery.lastError().isValid()) {
        qWarning() << "Failed to add Photos fileSize column to images database schema for version 2:" << addFileSizeQuery.lastError().text();
        return false;
    }
    addFileSizeQuery.finish();

    QSqlQuery addFileTypeQuery(QStringLiteral("ALTER TABLE Photos ADD fileType TEXT;"), database);
    if (addFileTypeQuery.lastError().isValid()) {
        qWarning() << "Failed to add Photos fileType column to images database schema for version 2:" << addFileTypeQuery.lastError().text();
        return false;
    }
    addFileTypeQuery.finish();

    QSqlQuery addUserThumbnailFileNameQuery(QStringLiteral("ALTER TABLE Users ADD thumbnailFileName TEXT;"), database);
    if (addUserThumbnailFileNameQuery.lastError().isValid()) {
        qWarning() << "Failed to add Users thumbnailFileName column to images database schema for version 2:" << addUserThumbnailFileNameQuery.lastError().text();
        return false;
    }
    addUserThumbnailFileNameQuery.finish();

    QSqlQuery addAlbumThumbnailFileNameQuery(QStringLiteral("ALTER TABLE Albums ADD thumbnailFileName TEXT;"), database);
    if (addAlbumThumbnailFileNameQuery.lastError().isValid()) {
        qWarning() << "Failed to add Albums thumbnailFileName column to images database schema for version 2:" << addAlbumThumbnailFileNameQuery.lastError().text();
        return false;
    }
    addAlbumThumbnailFileNameQuery.finish();

    return true;
}

bool upgradeVersion2to3Fn(QSqlDatabase &database)
{
    QSqlQuery addUserDisplayNameQuery(QStringLiteral("ALTER TABLE Users ADD displayName TEXT;"), database);
    if (addUserDisplayNameQuery.lastError().isValid()) {
        qWarning() << "Failed to add Users displayName column to images database schema for version 2:" << addUserDisplayNameQuery.lastError().text();
        return false;
    }
    addUserDisplayNameQuery.finish();

    return true;
}

bool upgradeVersion3to4Fn(QSqlDatabase &database)
{
    QSqlQuery addAlbumEtagQuery(QStringLiteral("ALTER TABLE Albums ADD etag TEXT;"), database);
    if (addAlbumEtagQuery.lastError().isValid()) {
        qWarning() << "Failed to add Albums etag column to images database schema for version 3:" << addAlbumEtagQuery.lastError().text();
        return false;
    }
    addAlbumEtagQuery.finish();

    QSqlQuery addPhotoEtagQuery(QStringLiteral("ALTER TABLE Photos ADD etag TEXT;"), database);
    if (addPhotoEtagQuery.lastError().isValid()) {
        qWarning() << "Failed to add Photos etag column to images database schema for version 3:" << addPhotoEtagQuery.lastError().text();
        return false;
    }
    addPhotoEtagQuery.finish();

    return true;
}

}

ImageDatabasePrivate::ImageDatabasePrivate(ImageDatabase *parent, bool emitCrossProcessChangeNotifications)
    : DatabasePrivate(parent)
    , m_imageDbParent(parent)
    , m_emitCrossProcessChangeNotifications(emitCrossProcessChangeNotifications)
{
    m_changeNotifier.reset(new ImageChangeNotifier(parent));
}

int ImageDatabasePrivate::currentSchemaVersion() const
{
    return 4;
}

QVector<const char *> ImageDatabasePrivate::createStatements() const
{
    static const char *createUsersTable =
            "\n CREATE TABLE Users ("
            "\n accountId INTEGER,"
            "\n userId TEXT,"
            "\n displayName TEXT,"
            "\n thumbnailUrl TEXT,"
            "\n thumbnailPath TEXT,"
            "\n thumbnailFileName TEXT,"
            "\n PRIMARY KEY (accountId, userId));";

    static const char *createAlbumsTable =
            "\n CREATE TABLE Albums ("
            "\n accountId INTEGER,"
            "\n userId TEXT,"
            "\n albumId TEXT,"
            "\n photoCount INTEGER,"
            "\n thumbnailUrl TEXT,"
            "\n thumbnailPath TEXT,"
            "\n parentAlbumId TEXT,"
            "\n albumName TEXT,"
            "\n thumbnailFileName TEXT,"
            "\n etag TEXT,"
            "\n PRIMARY KEY (accountId, userId, albumId),"
            "\n FOREIGN KEY (accountId, userId) REFERENCES Users (accountId, userId) ON DELETE CASCADE);";

    static const char *createPhotosTable =
            "\n CREATE TABLE Photos ("
            "\n accountId INTEGER,"
            "\n userId TEXT,"
            "\n albumId TEXT,"
            "\n photoId TEXT,"
            "\n fileName TEXT,"
            "\n albumPath TEXT,"
            "\n description TEXT,"
            "\n createdTimestamp TEXT,"
            "\n updatedTimestamp TEXT,"
            "\n thumbnailUrl TEXT,"
            "\n thumbnailPath TEXT,"
            "\n imageUrl TEXT,"
            "\n imagePath TEXT,"
            "\n imageWidth INTEGER,"
            "\n imageHeight INTEGER,"
            "\n fileSize INTEGER,"
            "\n fileType TEXT,"
            "\n etag TEXT,"
            "\n PRIMARY KEY (accountId, userId, albumId, photoId),"
            "\n FOREIGN KEY (accountId, userId, albumId) REFERENCES Albums (accountId, userId, albumId) ON DELETE CASCADE);";

    static QVector<const char *> retn { createUsersTable, createAlbumsTable, createPhotosTable };
    return retn;
}

QVector<UpgradeOperation> ImageDatabasePrivate::upgradeVersions() const
{
    static const char *upgradeVersion0to1[] = {
        "PRAGMA user_version=1",
        0 // NULL-terminated
    };

    static const char *upgradeVersion1to2[] = {
         "PRAGMA user_version=2",
         0 // NULL-terminated
    };

    static const char *upgradeVersion2to3[] = {
         "PRAGMA user_version=3",
         0 // NULL-terminated
    };

    static const char *upgradeVersion3to4[] = {
         "PRAGMA user_version=4",
         0 // NULL-terminated
    };

    static QVector<UpgradeOperation> retn {
        { 0, upgradeVersion0to1 },
        { upgradeVersion1to2Fn, upgradeVersion1to2 },
        { upgradeVersion2to3Fn, upgradeVersion2to3 },
        { upgradeVersion3to4Fn, upgradeVersion3to4 },
    };

    return retn;
}

bool ImageDatabasePrivate::preTransactionCommit()
{
    // Fixup album thumbnails if required.
    SyncCache::DatabaseError thumbnailError;
    if (!m_deletedPhotos.isEmpty()
            || !m_storedPhotos.isEmpty()) {
        // potentially need to update album thumbnails.
        QSet<QString> doomedAlbums;
        Q_FOREACH (const SyncCache::Album &doomed, m_deletedAlbums) {
            doomedAlbums.insert(constructAlbumIdentifier(doomed.accountId, doomed.userId, doomed.albumId));
        }
        QHash<QString, SyncCache::Album> thumbnailAlbums;
        Q_FOREACH (const SyncCache::Photo &photo, m_deletedPhotos) {
            const QString albumIdentifier = constructAlbumIdentifier(photo.accountId, photo.userId, photo.albumId);
            if (!thumbnailAlbums.contains(albumIdentifier) && !doomedAlbums.contains(albumIdentifier)) {
                SyncCache::Album album = m_imageDbParent->album(photo.accountId, photo.userId, photo.albumId, &thumbnailError);
                if (thumbnailError.errorCode == SyncCache::DatabaseError::NoError && !album.albumId.isEmpty()) {
                    thumbnailAlbums.insert(albumIdentifier, album);
                }
            }
        }
        thumbnailError.errorCode = SyncCache::DatabaseError::NoError;
        Q_FOREACH (const SyncCache::Photo &photo, m_storedPhotos) {
            const QString albumIdentifier = constructAlbumIdentifier(photo.accountId, photo.userId, photo.albumId);
            if (!thumbnailAlbums.contains(albumIdentifier)) {
                SyncCache::Album album = m_imageDbParent->album(photo.accountId, photo.userId, photo.albumId, &thumbnailError);
                if (thumbnailError.errorCode == SyncCache::DatabaseError::NoError && !album.albumId.isEmpty()) {
                    thumbnailAlbums.insert(albumIdentifier, album);
                }
            }
        }
        thumbnailError.errorCode = SyncCache::DatabaseError::NoError;
        Q_FOREACH (const SyncCache::Album &album, thumbnailAlbums) {
            // if the album uses a photo image as its thumbnail
            // and if that photo image is set to be deleted,
            // use another photo's image (or null if album is empty).
            if (album.thumbnailUrl.isEmpty()
                    && (album.thumbnailPath.isEmpty()
                          || m_filesToDelete.contains(album.thumbnailPath.toString()))) {
                QUrl updatedAlbumThumbnailPath;
                const QVector<SyncCache::Photo> photos = m_imageDbParent->photos(
                        album.accountId, album.userId, album.albumId, &thumbnailError);
                Q_FOREACH (const SyncCache::Photo &photo, photos) {
                    if (!photo.imagePath.isEmpty()) {
                        updatedAlbumThumbnailPath = photo.imagePath;
                        break;
                    }
                }

                // update the album in the database if it previously didn't have a valid
                // thumbnail but now does, or if its previous thumbnail path is no longer
                // valid and must be changed.
                if (!updatedAlbumThumbnailPath.isEmpty() || !album.thumbnailPath.isEmpty()) {
                    SyncCache::Album updateAlbum = album;
                    updateAlbum.thumbnailPath = updatedAlbumThumbnailPath;
                    m_imageDbParent->storeAlbum(updateAlbum, &thumbnailError);
                    // also update the cached copy in the m_storedAlbums vector
                    // as the change signals depend on its contents.
                    if (thumbnailError.errorCode == SyncCache::DatabaseError::NoError) {
                        bool found = false;
                        for (SyncCache::Album &storedAlbum : m_storedAlbums) {
                            if (storedAlbum.accountId == updateAlbum.accountId
                                    && storedAlbum.userId == updateAlbum.userId
                                    && storedAlbum.albumId == updateAlbum.albumId) {
                                storedAlbum.thumbnailPath = updateAlbum.thumbnailPath;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            m_storedAlbums.append(updateAlbum);
                        }
                    }
                }
            }
        }
    }

    return thumbnailError.errorCode == SyncCache::DatabaseError::NoError;
}

void ImageDatabasePrivate::transactionCommittedPreUnlock()
{
    m_tempFilesToDelete = m_filesToDelete;
    m_tempDeletedUsers = m_deletedUsers;
    m_tempDeletedAlbums = m_deletedAlbums;
    m_tempDeletedPhotos = m_deletedPhotos;
    m_tempStoredUsers = m_storedUsers;
    m_tempStoredAlbums = m_storedAlbums;
    m_tempStoredPhotos = m_storedPhotos;

    m_filesToDelete.clear();
    m_deletedUsers.clear();
    m_deletedAlbums.clear();
    m_deletedPhotos.clear();
    m_storedUsers.clear();
    m_storedAlbums.clear();
    m_storedPhotos.clear();
}

void ImageDatabasePrivate::transactionCommittedPostUnlock()
{
    for (const QString &doomed : m_tempFilesToDelete) {
        if (QFile::exists(doomed)) {
            QFile::remove(doomed);
        }
    }

    bool dataChanged = false;
    if (!m_tempDeletedUsers.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->usersDeleted(m_tempDeletedUsers);
    }
    if (!m_tempDeletedAlbums.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->albumsDeleted(m_tempDeletedAlbums);
    }
    if (!m_tempDeletedPhotos.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->photosDeleted(m_tempDeletedPhotos);
    }

    if (!m_tempStoredUsers.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->usersStored(m_tempStoredUsers);
    }
    if (!m_tempStoredAlbums.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->albumsStored(m_tempStoredAlbums);
    }
    if (!m_tempStoredPhotos.isEmpty()) {
        dataChanged = true;
        emit m_imageDbParent->photosStored(m_tempStoredPhotos);
    }

    if (dataChanged && m_changeNotifier && m_emitCrossProcessChangeNotifications) {
        m_changeNotifier->dataChanged(); // emit cross-process change signal.
    }
}

void ImageDatabasePrivate::transactionRolledBackPreUnlocked()
{
    m_filesToDelete.clear();
    m_deletedUsers.clear();
    m_deletedAlbums.clear();
    m_deletedPhotos.clear();
    m_storedUsers.clear();
    m_storedAlbums.clear();
    m_storedPhotos.clear();
}

//-----------------------------------------------------------------------------

ImageDatabase::ImageDatabase(QObject *parent, bool emitCrossProcessChangeNotifications)
    : Database(new ImageDatabasePrivate(this, emitCrossProcessChangeNotifications), parent)
{
    qRegisterMetaType<SyncCache::User>();
    qRegisterMetaType<SyncCache::Album>();
    qRegisterMetaType<SyncCache::Photo>();
    qRegisterMetaType<QVector<SyncCache::User> >();
    qRegisterMetaType<QVector<SyncCache::Album> >();
    qRegisterMetaType<QVector<SyncCache::Photo> >();
}

QVector<SyncCache::User> ImageDatabase::users(DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    const QString queryString = QStringLiteral("SELECT accountId, userId, displayName, thumbnailUrl, thumbnailPath, thumbnailFileName FROM USERS"
                                               " ORDER BY accountId ASC, userId ASC");

    const QList<QPair<QString, QVariant> > bindValues;

    auto resultHandler = [](DatabaseQuery &selectQuery) -> SyncCache::User {
        int whichValue = 0;
        User currUser;
        currUser.accountId = selectQuery.value(whichValue++).toInt();
        currUser.userId = selectQuery.value(whichValue++).toString();
        currUser.displayName = selectQuery.value(whichValue++).toString();
        currUser.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currUser.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currUser.thumbnailFileName = selectQuery.value(whichValue++).toString();
        return currUser;
    };

    return DatabaseImpl::fetchMultiple<SyncCache::User>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("users"),
            error);
}

QVector<SyncCache::Album> ImageDatabase::albums(int accountId, const QString &userId, DatabaseError *error, const QString &parentAlbumId) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    if (accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch albums, invalid accountId: %1").arg(accountId));
        return QVector<SyncCache::Album>();
    }
    if (userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch albums, userId is empty"));
        return QVector<SyncCache::Album>();
    }

    const QString parentAlbumIdClause = parentAlbumId.isEmpty()
            ? QString()
            : QStringLiteral(" AND parentAlbumId = :parentAlbumId");
    const QString queryString = QStringLiteral("SELECT albumId, photoCount, thumbnailUrl, thumbnailPath, parentAlbumId, albumName, thumbnailFileName, etag FROM ALBUMS"
                                               " WHERE accountId = :accountId AND userId = :userId"
                                               "%1"
                                               " ORDER BY accountId ASC, userId ASC, albumId ASC").arg(parentAlbumIdClause);

    QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId)
    };
    if (!parentAlbumIdClause.isEmpty()) {
        bindValues.append(qMakePair<QString, QVariant>(QStringLiteral(":parentAlbumId"), parentAlbumId));
    }

    auto resultHandler = [accountId, userId](DatabaseQuery &selectQuery) -> SyncCache::Album {
        int whichValue = 0;
        Album currAlbum;
        currAlbum.accountId = accountId;
        currAlbum.userId = userId;
        currAlbum.albumId = selectQuery.value(whichValue++).toString();
        currAlbum.photoCount = selectQuery.value(whichValue++).toInt();
        currAlbum.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currAlbum.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currAlbum.parentAlbumId = selectQuery.value(whichValue++).toString();
        currAlbum.albumName = selectQuery.value(whichValue++).toString();
        currAlbum.thumbnailFileName = selectQuery.value(whichValue++).toString();
        currAlbum.etag = selectQuery.value(whichValue++).toString();
        return currAlbum;
    };

    return DatabaseImpl::fetchMultiple<SyncCache::Album>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("albums"),
            error);
}

QVector<SyncCache::Photo> ImageDatabase::photos(int accountId, const QString &userId, const QString &albumId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    QString queryString = QStringLiteral("SELECT albumId, photoId, createdTimestamp, updatedTimestamp, fileName, albumPath, description,"
                                         " thumbnailUrl, thumbnailPath, imageUrl, imagePath, imageWidth, imageHeight, fileSize, fileType, etag FROM Photos");
    QStringList conditions;
    QList<QPair<QString, QVariant> > bindValues;
    if (accountId > 0) {
        conditions << QStringLiteral("accountId = :accountId");
        bindValues << qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId);
    }
    if (!userId.isEmpty()) {
        conditions << QStringLiteral("userId = :userId");
        bindValues << qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId);
    }
    if (!albumId.isEmpty()) {
        conditions << QStringLiteral("albumId = :albumId");
        bindValues << qMakePair<QString, QVariant>(QStringLiteral(":albumId"), albumId);
    }
    if (!conditions.isEmpty()) {
        queryString += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    }
    queryString += QStringLiteral(" ORDER BY accountId ASC, userId ASC, albumId ASC, createdTimestamp DESC");

    auto resultHandler = [accountId, userId, albumId](DatabaseQuery &selectQuery) -> SyncCache::Photo {
        int whichValue = 0;
        Photo currPhoto;
        currPhoto.accountId = accountId;
        currPhoto.userId = userId;
        currPhoto.albumId = selectQuery.value(whichValue++).toString();
        currPhoto.photoId = selectQuery.value(whichValue++).toString();
        currPhoto.createdTimestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currPhoto.updatedTimestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currPhoto.fileName = selectQuery.value(whichValue++).toString();
        currPhoto.albumPath = selectQuery.value(whichValue++).toString();
        currPhoto.description = selectQuery.value(whichValue++).toString();
        currPhoto.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imageUrl = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imagePath = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imageWidth = selectQuery.value(whichValue++).toInt();
        currPhoto.imageHeight = selectQuery.value(whichValue++).toInt();
        currPhoto.fileSize = selectQuery.value(whichValue++).toInt();
        currPhoto.fileType = selectQuery.value(whichValue++).toString();
        currPhoto.etag = selectQuery.value(whichValue++).toString();
        return currPhoto;
    };

    return DatabaseImpl::fetchMultiple<SyncCache::Photo>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("photos"),
            error);
}

User ImageDatabase::user(int accountId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    if (accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch user, invalid accountId: %1").arg(accountId));
        return User();
    }

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId)
    };

    const QString queryString = QStringLiteral("SELECT userId, displayName, thumbnailUrl, thumbnailPath, thumbnailFileName FROM USERS"
                                               " WHERE accountId = :accountId");

    auto resultHandler = [accountId](DatabaseQuery &selectQuery) -> SyncCache::User {
        int whichValue = 0;
        User currUser;
        currUser.accountId = accountId;
        currUser.userId = selectQuery.value(whichValue++).toString();
        currUser.displayName = selectQuery.value(whichValue++).toString();
        currUser.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currUser.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currUser.thumbnailFileName = selectQuery.value(whichValue++).toString();
        return currUser;
    };

    return DatabaseImpl::fetch<SyncCache::User>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("user"),
            error);
}

Album ImageDatabase::album(int accountId, const QString &userId, const QString &albumId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    if (accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch album, invalid accountId: %1").arg(accountId));
        return Album();
    }
    if (userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch album, userId is empty"));
        return Album();
    }
    if (albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch album, albumId is empty"));
        return Album();
    }

    const QString queryString = QStringLiteral("SELECT photoCount, thumbnailUrl, thumbnailPath, parentAlbumId, albumName, thumbnailFileName, etag FROM ALBUMS"
                                               " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), albumId)
    };

    auto resultHandler = [accountId, userId, albumId](DatabaseQuery &selectQuery) -> SyncCache::Album {
        int whichValue = 0;
        Album currAlbum;
        currAlbum.accountId = accountId;
        currAlbum.userId = userId;
        currAlbum.albumId = albumId;
        currAlbum.photoCount = selectQuery.value(whichValue++).toInt();
        currAlbum.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currAlbum.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currAlbum.parentAlbumId = selectQuery.value(whichValue++).toString();
        currAlbum.albumName = selectQuery.value(whichValue++).toString();
        currAlbum.thumbnailFileName = selectQuery.value(whichValue++).toString();
        currAlbum.etag = selectQuery.value(whichValue++).toString();
        return currAlbum;
    };

    return DatabaseImpl::fetch<SyncCache::Album>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("album"),
            error);
}

Photo ImageDatabase::photo(int accountId, const QString &userId, const QString &albumId, const QString &photoId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    if (accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch photo, invalid accountId: %1").arg(accountId));
        return Photo();
    }
    if (userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch photo, userId is empty"));
        return Photo();
    }
    if (albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch photo, albumId is empty"));
        return Photo();
    }
    if (photoId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch photo, photoId is empty"));
        return Photo();
    }

    const QString queryString = QStringLiteral("SELECT createdTimestamp, updatedTimestamp, fileName, albumPath, description,"
                                               " thumbnailUrl, thumbnailPath, imageUrl, imagePath, imageWidth, imageHeight, fileSize, fileType, etag FROM Photos"
                                               " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId AND photoId = :photoId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), albumId),
        qMakePair<QString, QVariant>(QStringLiteral(":photoId"), photoId)
    };

    auto resultHandler = [accountId, userId, albumId, photoId](DatabaseQuery &selectQuery) -> SyncCache::Photo {
        int whichValue = 0;
        Photo currPhoto;
        currPhoto.accountId = accountId;
        currPhoto.userId = userId;
        currPhoto.albumId = albumId;
        currPhoto.photoId = photoId;
        currPhoto.createdTimestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currPhoto.updatedTimestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currPhoto.fileName = selectQuery.value(whichValue++).toString();
        currPhoto.albumPath = selectQuery.value(whichValue++).toString();
        currPhoto.description = selectQuery.value(whichValue++).toString();
        currPhoto.thumbnailUrl = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.thumbnailPath = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imageUrl = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imagePath = QUrl(selectQuery.value(whichValue++).toString());
        currPhoto.imageWidth = selectQuery.value(whichValue++).toInt();
        currPhoto.imageHeight = selectQuery.value(whichValue++).toInt();
        currPhoto.fileSize = selectQuery.value(whichValue++).toInt();
        currPhoto.fileType = selectQuery.value(whichValue++).toString();
        currPhoto.etag = selectQuery.value(whichValue++).toString();
        return currPhoto;
    };

    return DatabaseImpl::fetch<SyncCache::Photo>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("photo"),
            error);
}

SyncCache::PhotoCounter ImageDatabase::photoCount(int accountId, const QString &userId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    QString queryString = QStringLiteral("SELECT COUNT(*) FROM Photos");

    QStringList conditions;
    QList<QPair<QString, QVariant> > bindValues;

    if (accountId > 0) {
        conditions << QStringLiteral("accountId = :accountId");
        bindValues << qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId);
    }
    if (!userId.isEmpty()) {
        conditions << QStringLiteral("userId = :userId");
        bindValues << qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId);
    }
    if (!conditions.isEmpty()) {
        queryString += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    }

    auto resultHandler = [](DatabaseQuery &selectQuery) -> SyncCache::PhotoCounter {
        PhotoCounter counter;
        counter.count = selectQuery.value(0).toInt();
        return counter;
    };

    return DatabaseImpl::fetch<SyncCache::PhotoCounter>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("photoCount"),
            error);
}

QString ImageDatabase::findThumbnailForAlbum(int accountId, const QString &userId, const QString &albumId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const ImageDatabase);

    if (accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch thumbnail, invalid accountId: %1").arg(accountId));
        return QString();
    }
    if (albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot fetch thumbnail, albumId is empty"));
        return QString();
    }

    const QString queryString = QStringLiteral("SELECT thumbnailPath FROM PHOTOS"
        " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId AND thumbnailPath !='' ORDER BY updatedTimestamp DESC LIMIT 1");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), albumId)
    };

    auto resultHandler = [](DatabaseQuery &selectQuery) -> QString {
        return selectQuery.value(0).toString();
    };

    return DatabaseImpl::fetch<QString>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("findThumbnailForAlbum"),
            error);
}

void ImageDatabase::storeUser(const User &user, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (user.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store user, invalid accountId: %1").arg(user.accountId));
        return;
    }
    if (user.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store user, userId is empty"));
        return;
    }

    DatabaseError err;
    const User existingUser = this->user(user.accountId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing user %1 for store: %2")
                                   .arg(user.userId, err.errorMessage));
        return;
    }

    const QString insertString = QStringLiteral("INSERT INTO Users (accountId, userId, displayName, thumbnailUrl, thumbnailPath, thumbnailFileName)"
                                                " VALUES(:accountId, :userId, :displayName, :thumbnailUrl, :thumbnailPath, :thumbnailFileName)");
    const QString updateString = QStringLiteral("UPDATE Users SET userId = :userId, displayName = :displayName, thumbnailUrl = :thumbnailUrl, thumbnailPath = :thumbnailPath, thumbnailFileName = :thumbnailFileName"
                                                " WHERE accountId = :accountId");

    const bool insert = existingUser.userId.isEmpty();
    const QString queryString = insert ? insertString : updateString;

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), user.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), user.userId),
        qMakePair<QString, QVariant>(QStringLiteral(":displayName"), user.displayName),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailUrl"), user.thumbnailUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailPath"), user.thumbnailPath),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailFileName"), user.thumbnailFileName)
    };

    auto storeResultHandler = [d, user]() -> void {
        d->m_storedUsers.append(user);
    };

    DatabaseImpl::store<SyncCache::User>(
            d,
            queryString,
            bindValues,
            storeResultHandler,
            QStringLiteral("user"),
            error);
}

void ImageDatabase::storeAlbum(const Album &album, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (album.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store album, invalid accountId: %1").arg(album.accountId));
        return;
    }
    if (album.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store album, userId is empty"));
        return;
    }
    if (album.albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store album, albumId is empty"));
        return;
    }

    DatabaseError err;
    const Album existingAlbum = this->album(album.accountId, album.userId, album.albumId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing album %1 for store: %2")
                                   .arg(album.albumId, err.errorMessage));
        return;
    }

    const QString insertString = QStringLiteral("INSERT INTO Albums (accountId, userId, albumId, photoCount, thumbnailUrl, thumbnailPath, parentAlbumId, albumName, thumbnailFileName, etag)"
                                                " VALUES(:accountId, :userId, :albumId, :photoCount, :thumbnailUrl, :thumbnailPath, :parentAlbumId, :albumName, :thumbnailFileName, :etag)");
    const QString updateString = QStringLiteral("UPDATE Albums SET photoCount = :photoCount, thumbnailUrl = :thumbnailUrl, thumbnailPath = :thumbnailPath, "
                                                "parentAlbumId = :parentAlbumId, albumName = :albumName, thumbnailFileName = :thumbnailFileName, etag = :etag"
                                                " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId");

    const bool insert = existingAlbum.albumId.isEmpty();
    const QString queryString = insert ? insertString : updateString;

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), album.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), album.userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), album.albumId),
        qMakePair<QString, QVariant>(QStringLiteral(":photoCount"), album.photoCount),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailUrl"), album.thumbnailUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailPath"), album.thumbnailPath),
        qMakePair<QString, QVariant>(QStringLiteral(":parentAlbumId"), album.parentAlbumId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumName"), album.albumName),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailFileName"), album.thumbnailFileName),
        qMakePair<QString, QVariant>(QStringLiteral(":etag"), album.etag),
    };

    auto storeResultHandler = [d, album]() -> void {
        d->m_storedAlbums.append(album);
    };

    DatabaseImpl::store<SyncCache::Album>(
            d,
            queryString,
            bindValues,
            storeResultHandler,
            QStringLiteral("album"),
            error);
}

void ImageDatabase::storePhoto(const Photo &photo, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (photo.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store photo, invalid accountId: %1").arg(photo.accountId));
        return;
    }
    if (photo.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store photo, userId is empty"));
        return;
    }
    if (photo.albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store photo, albumId is empty"));
        return;
    }
    if (photo.photoId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot store photo, photoId is empty"));
        return;
    }

    DatabaseError err;
    const Photo existingPhoto = this->photo(photo.accountId, photo.userId, photo.albumId, photo.photoId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing photo %1 for store: %2")
                                   .arg(photo.photoId, err.errorMessage));
        return;
    }

    const QString insertString = QStringLiteral("INSERT INTO Photos (accountId, userId, albumId, photoId, createdTimestamp, updatedTimestamp, "
                                                                    "fileName, albumPath, description, thumbnailUrl, thumbnailPath, "
                                                                    "imageUrl, imagePath, imageWidth, imageHeight, fileSize, fileType, etag)"
                                                " VALUES(:accountId, :userId, :albumId, :photoId, :createdTimestamp, :updatedTimestamp, "
                                                        ":fileName, :albumPath, :description, :thumbnailUrl, :thumbnailPath, :imageUrl, :imagePath, :imageWidth, :imageHeight, :fileSize, :fileType, :etag)");
    const QString updateString = QStringLiteral("UPDATE Photos SET createdTimestamp = :createdTimestamp, updatedTimestamp = :updatedTimestamp, "
                                                                  "fileName = :fileName, albumPath = :albumPath, description = :description, "
                                                                  "thumbnailUrl = :thumbnailUrl, thumbnailPath = :thumbnailPath, "
                                                                  "imageUrl = :imageUrl, imagePath = :imagePath, imageWidth = :imageWidth, imageHeight = :imageHeight,"
                                                                  "fileSize = :fileSize, fileType = :fileType, etag = :etag"
                                                " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId AND photoId = :photoId");

    const bool insert = existingPhoto.photoId.isEmpty();
    const QString queryString = insert ? insertString : updateString;

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), photo.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), photo.userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), photo.albumId),
        qMakePair<QString, QVariant>(QStringLiteral(":photoId"), photo.photoId),
        qMakePair<QString, QVariant>(QStringLiteral(":createdTimestamp"), photo.createdTimestamp.toString(Qt::ISODate)),
        qMakePair<QString, QVariant>(QStringLiteral(":updatedTimestamp"), photo.updatedTimestamp.toString(Qt::ISODate)),
        qMakePair<QString, QVariant>(QStringLiteral(":fileName"), photo.fileName),
        qMakePair<QString, QVariant>(QStringLiteral(":albumPath"), photo.albumPath),
        qMakePair<QString, QVariant>(QStringLiteral(":description"), photo.description),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailUrl"), photo.thumbnailUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":thumbnailPath"), photo.thumbnailPath),
        qMakePair<QString, QVariant>(QStringLiteral(":imageUrl"), photo.imageUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":imagePath"), photo.imagePath),
        qMakePair<QString, QVariant>(QStringLiteral(":imageWidth"), photo.imageWidth),
        qMakePair<QString, QVariant>(QStringLiteral(":imageHeight"), photo.imageHeight),
        qMakePair<QString, QVariant>(QStringLiteral(":fileSize"), photo.fileSize),
        qMakePair<QString, QVariant>(QStringLiteral(":fileType"), photo.fileType),
        qMakePair<QString, QVariant>(QStringLiteral(":etag"), photo.etag)
    };

    auto storeResultHandler = [d, photo, existingPhoto, insert]() -> void {
        d->m_storedPhotos.append(photo);
        if (!insert) {
            if (!existingPhoto.imagePath.isEmpty()
                    && existingPhoto.imagePath != photo.imagePath) {
                d->m_filesToDelete.append(existingPhoto.imagePath.toString());
            }
            if (!existingPhoto.thumbnailPath.isEmpty()
                    && existingPhoto.thumbnailPath != photo.thumbnailPath) {
                d->m_filesToDelete.append(existingPhoto.thumbnailPath.toString());
            }
        }
    };

    DatabaseImpl::store<SyncCache::Photo>(
            d,
            queryString,
            bindValues,
            storeResultHandler,
            QStringLiteral("photo"),
            error);
}

void ImageDatabase::deleteUser(const User &user, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (user.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete user, invalid accountId: %1").arg(user.accountId));
        return;
    }
    if (user.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete user, userId is empty"));
        return;
    }

    DatabaseError err;
    const User existingUser = this->user(user.accountId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing user %1 for delete: %2")
                                   .arg(user.userId, err.errorMessage));
        return;
    }

    if (existingUser.userId.isEmpty()) {
        // does not exist in database, already deleted.
        return;
    }

    // we need to delete all albums for this user, to ensure artifacts (e.g. downloaded files) are also deleted.
    QVector<SyncCache::Album> userAlbums = this->albums(user.accountId, user.userId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying albums for user %1 for delete: %2")
                                   .arg(user.userId, err.errorMessage));
        return;
    }

    auto deleteRelatedValues = [this, userAlbums] (DatabaseError *error) -> void {
        DatabaseError err;
        for (const Album &doomed : userAlbums) {
            this->deleteAlbum(doomed, &err);
            if (err.errorCode != DatabaseError::NoError) {
                setDatabaseError(error, err.errorCode,
                                 QStringLiteral("Error while deleting album %1 for user %2: %3")
                                           .arg(doomed.albumId, doomed.userId, err.errorMessage));
                return;
            }
        }
    };

    const QString queryString = QStringLiteral("DELETE FROM Users"
                                               " WHERE accountId = :accountId AND userId = :userId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), user.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), user.userId)
    };

    auto deleteResultHandler = [d, existingUser]() -> void {
        d->m_deletedUsers.append(existingUser);
        if (!existingUser.thumbnailPath.isEmpty()) {
            d->m_filesToDelete.append(existingUser.thumbnailPath.toString());
        }
    };

    DatabaseImpl::deleteValue<SyncCache::User>(
            d,
            deleteRelatedValues,
            queryString,
            bindValues,
            deleteResultHandler,
            QStringLiteral("user"),
            error);
}

void ImageDatabase::deleteAlbum(const Album &album, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (album.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete album, invalid accountId: %1").arg(album.accountId));
        return;
    }
    if (album.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete album, userId is empty"));
        return;
    }
    if (album.albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete album, albumId is empty"));
        return;
    }

    DatabaseError err;
    const Album existingAlbum = this->album(album.accountId, album.userId, album.albumId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing album %1 for delete: %2")
                                   .arg(album.albumId, err.errorMessage));
        return;
    }

    if (existingAlbum.albumId.isEmpty()) {
        // does not exist in database, already deleted.
        return;
    }

    // we need to delete all photos in this album, to ensure artifacts (e.g. downloaded files) are also deleted.
    QVector<SyncCache::Photo> albumPhotos = this->photos(album.accountId, album.userId, album.albumId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying photos from album %1 for delete: %2")
                                   .arg(album.albumId, err.errorMessage));
        return;
    }

    auto deleteRelatedValues = [this, albumPhotos] (DatabaseError *error) -> void {
        DatabaseError err;
        for (const Photo &doomed : albumPhotos) {
            this->deletePhoto(doomed, &err);
            if (err.errorCode != DatabaseError::NoError) {
                setDatabaseError(error, err.errorCode,
                                 QStringLiteral("Error while deleting photo %1 from album %2: %3")
                                           .arg(doomed.photoId, doomed.albumId, err.errorMessage));
                return;
            }
        }
    };

    const QString queryString = QStringLiteral("DELETE FROM Albums"
                                               " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), album.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), album.userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), album.albumId)
    };

    auto deleteResultHandler = [d, existingAlbum]() -> void {
        d->m_deletedAlbums.append(existingAlbum);
        if (!existingAlbum.thumbnailUrl.isEmpty() && !existingAlbum.thumbnailPath.isEmpty()) {
            // only delete the thumbnail if it was downloaded especially from url,
            // but not if it is from a photo (in that case, thumbnailUrl will be empty).
            d->m_filesToDelete.append(existingAlbum.thumbnailPath.toString());
        }
    };

    DatabaseImpl::deleteValue<SyncCache::Album>(
            d,
            deleteRelatedValues,
            queryString,
            bindValues,
            deleteResultHandler,
            QStringLiteral("album"),
            error);
}

void ImageDatabase::deletePhoto(const Photo &photo, DatabaseError *error)
{
    SYNCCACHE_DB_D(ImageDatabase);

    if (photo.accountId <= 0) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete photo, invalid accountId: %1").arg(photo.accountId));
        return;
    }
    if (photo.userId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete photo, userId is empty"));
        return;
    }
    if (photo.albumId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete photo, albumId is empty"));
        return;
    }
    if (photo.photoId.isEmpty()) {
        setDatabaseError(error, DatabaseError::InvalidArgumentError,
                         QStringLiteral("Cannot delete photo, photoId is empty"));
        return;
    }

    DatabaseError err;
    const Photo existingPhoto = this->photo(photo.accountId, photo.userId, photo.albumId, photo.photoId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing photo %1 for delete: %2")
                                   .arg(photo.photoId, err.errorMessage));
        return;
    }

    if (existingPhoto.photoId.isEmpty()) {
        // does not exist in database, already deleted.
        return;
    }

    auto deleteRelatedValues = [] (DatabaseError *) -> void { };

    const QString queryString = QStringLiteral("DELETE FROM Photos"
                                               " WHERE accountId = :accountId AND userId = :userId AND albumId = :albumId AND photoId = :photoId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), photo.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":userId"), photo.userId),
        qMakePair<QString, QVariant>(QStringLiteral(":albumId"), photo.albumId),
        qMakePair<QString, QVariant>(QStringLiteral(":photoId"), photo.photoId)
    };

    auto deleteResultHandler = [d, existingPhoto]() -> void {
        d->m_deletedPhotos.append(existingPhoto);
        if (!existingPhoto.thumbnailPath.isEmpty()) {
            d->m_filesToDelete.append(existingPhoto.thumbnailPath.toString());
        }
        if (!existingPhoto.imagePath.isEmpty()) {
            d->m_filesToDelete.append(existingPhoto.imagePath.toString());
        }
    };

    DatabaseImpl::deleteValue<SyncCache::Photo>(
            d,
            deleteRelatedValues,
            queryString,
            bindValues,
            deleteResultHandler,
            QStringLiteral("photo"),
            error);
}
