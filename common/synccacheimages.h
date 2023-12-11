/****************************************************************************************
** Copyright (c) 2019 Open Mobile Platform LLC.
** Copyright (c) 2023 Jolla Ltd.
**
** All rights reserved.
**
** This file is part of Sailfish Nextcloud account package.
**
** You may use this file under the terms of BSD license as follows:
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice, this
**    list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
**    this list of conditions and the following disclaimer in the documentation
**    and/or other materials provided with the distribution.
**
** 3. Neither the name of the copyright holder nor the names of its
**    contributors may be used to endorse or promote products derived from
**    this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHEIMAGES_H
#define NEXTCLOUD_SYNCCACHEIMAGES_H

#include "synccachedatabase.h"

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <QtCore/QDateTime>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkRequest>

namespace SyncCache {

class ProcessMutex;

struct User {
    User() = default;
    User(const User &other) = default;
    User& operator=(const User &other);

    int accountId = 0;
    QString userId;
    QString displayName;
    QUrl thumbnailUrl;
    QUrl thumbnailPath;
    QString thumbnailFileName;
};

struct Album {
    Album() = default;
    Album(const Album &other) = default;
    Album& operator=(const Album &other);

    int accountId = 0;
    QString userId;
    QString albumId;
    QString parentAlbumId;
    QString albumName;
    int photoCount = 0;
    QUrl thumbnailUrl;
    QUrl thumbnailPath;
    QString thumbnailFileName;
    QString etag;
};

struct Photo {
    Photo() = default;
    Photo(const Photo &other) = default;
    Photo& operator=(const Photo &other);

    int accountId = 0;
    QString userId;
    QString albumId;
    QString photoId;
    QDateTime createdTimestamp;
    QDateTime updatedTimestamp;
    QString fileName;
    QString albumPath;
    QString description;
    QUrl thumbnailUrl;
    QUrl thumbnailPath;
    QUrl imageUrl;
    QUrl imagePath;
    int imageWidth = 0;
    int imageHeight = 0;
    int fileSize = 0;
    QString fileType;
    QString etag;
};

struct PhotoCounter {
    int count = 0;
};

class ImageDatabase : public Database
{
    Q_OBJECT

public:
    ImageDatabase(QObject *parent = nullptr, bool emitCrossProcessChangeNotifications = true);

    QVector<SyncCache::User> users(SyncCache::DatabaseError *error) const;
    QVector<SyncCache::Album> albums(int accountId, const QString &userId, SyncCache::DatabaseError *error, const QString &parentAlbumId = QString()) const;
    QVector<SyncCache::Photo> photos(int accountId, const QString &userId, const QString &albumId, SyncCache::DatabaseError *error) const;

    SyncCache::User user(int accountId, SyncCache::DatabaseError *error) const;
    SyncCache::Album album(int accountId, const QString &userId, const QString &albumId, SyncCache::DatabaseError *error) const;
    SyncCache::Photo photo(int accountId, const QString &userId, const QString &albumId, const QString &photoId, SyncCache::DatabaseError *error) const;

    SyncCache::PhotoCounter photoCount(int accountId, const QString &userId, SyncCache::DatabaseError *error) const;
    QString findThumbnailForAlbum(int accountId, const QString &userId, const QString &albumId, SyncCache::DatabaseError *error) const;

    void storeUser(const SyncCache::User &user, SyncCache::DatabaseError *error);
    void storeAlbum(const SyncCache::Album &album, SyncCache::DatabaseError *error);
    void storePhoto(const SyncCache::Photo &photo, SyncCache::DatabaseError *error);

    void deleteUser(const SyncCache::User &user, SyncCache::DatabaseError *error);
    void deleteAlbum(const SyncCache::Album &album, SyncCache::DatabaseError *error);
    void deletePhoto(const SyncCache::Photo &photo, SyncCache::DatabaseError *error);

Q_SIGNALS:
    void usersStored(const QVector<SyncCache::User> &users);
    void albumsStored(const QVector<SyncCache::Album> &albums);
    void photosStored(const QVector<SyncCache::Photo> &photos);

    void usersDeleted(const QVector<SyncCache::User> &users);
    void albumsDeleted(const QVector<SyncCache::Album> &albums);
    void photosDeleted(const QVector<SyncCache::Photo> &photos);

    void dataChanged();
};

class ImageCachePrivate;
class ImageCache : public QObject
{
    Q_OBJECT

public:
    ImageCache(QObject *parent = nullptr);
    ~ImageCache();

    static QString imageCacheDir(int accountId);
    static QString imageCacheRootDir();

public Q_SLOTS:
    virtual void openDatabase(const QString &accountType); // e.g. "nextcloud"

    virtual void requestUser(int accountId, const QString &userId);
    virtual void requestUsers();
    virtual void requestAlbums(int accountId, const QString &userId);
    virtual void requestPhotos(int accountId, const QString &userId, const QString &albumId);
    virtual void requestPhotoCount(int accountId, const QString &userId);

    virtual void populateUserThumbnail(int idempToken, int accountId, const QString &userId, const QNetworkRequest &requestTemplate);
    virtual void populateAlbumThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QNetworkRequest &requestTemplate);
    virtual void populatePhotoThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate);
    virtual void populatePhotoImage(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &requestTemplate);

Q_SIGNALS:
    void openDatabaseFailed(const QString &errorMessage);
    void openDatabaseFinished();

    void requestUserFailed(int accountId, const QString &userId, const QString &errorMessage);
    void requestUserFinished(int accountId, const QString &userId, const SyncCache::User &user);

    void requestUsersFailed(const QString &errorMessage);
    void requestUsersFinished(const QVector<SyncCache::User> &users);

    void requestAlbumsFailed(int accountId, const QString &userId, const QString &errorMessage);
    void requestAlbumsFinished(int accountId, const QString &userId, const QVector<SyncCache::Album> &albums);

    void requestPhotosFailed(int accountId, const QString &userId, const QString &albumId, const QString &errorMessage);
    void requestPhotosFinished(int accountId, const QString &userId, const QString &albumId, const QVector<SyncCache::Photo> &photos);

    void requestPhotoCountFailed(int accountId, const QString &userId, const QString &errorMessage);
    void requestPhotoCountFinished(int accountId, const QString &userId, int photoCount);

    void populateUserThumbnailFailed(int idempToken, const QString &errorMessage);
    void populateUserThumbnailFinished(int idempToken, const QString &path);

    void populateAlbumThumbnailFailed(int idempToken, const QString &errorMessage);
    void populateAlbumThumbnailFinished(int idempToken, const QString &path);

    void populatePhotoThumbnailFailed(int idempToken, const QString &errorMessage);
    void populatePhotoThumbnailFinished(int idempToken, const QString &path);

    void populatePhotoImageFailed(int idempToken, const QString &errorMessage);
    void populatePhotoImageFinished(int idempToken, const QString &path);

    void usersStored(const QVector<SyncCache::User> &users);
    void albumsStored(const QVector<SyncCache::Album> &albums);
    void photosStored(const QVector<SyncCache::Photo> &photos);

    void usersDeleted(const QVector<SyncCache::User> &users);
    void albumsDeleted(const QVector<SyncCache::Album> &albums);
    void photosDeleted(const QVector<SyncCache::Photo> &photos);

    void dataChanged();

private:
    Q_DECLARE_PRIVATE(ImageCache)
    QScopedPointer<ImageCachePrivate> d_ptr;
};

} // namespace SyncCache

Q_DECLARE_METATYPE(SyncCache::User)
Q_DECLARE_METATYPE(QVector<SyncCache::User>)
Q_DECLARE_TYPEINFO(SyncCache::User, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(SyncCache::Album)
Q_DECLARE_METATYPE(QVector<SyncCache::Album>)
Q_DECLARE_TYPEINFO(SyncCache::Album, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(SyncCache::Photo)
Q_DECLARE_METATYPE(QVector<SyncCache::Photo>)
Q_DECLARE_TYPEINFO(SyncCache::Photo, Q_MOVABLE_TYPE);

#endif // NEXTCLOUD_SYNCCACHEIMAGES_H
