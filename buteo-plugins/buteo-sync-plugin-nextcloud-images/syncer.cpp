/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "syncer_p.h"
#include "logging.h"

#include "networkrequestgenerator_p.h"
#include "networkreplyparser_p.h"
#include "replyparser_p.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QStandardPaths>

// buteo
#include <SyncProfile.h>

// libaccounts-qt5
#include <Accounts/Account>
#include <Accounts/Service>

void Syncer::SyncProgressInfo::reset()
{
    addedAlbumCount = 0;
    modifiedAlbumCount = 0;
    removedAlbumCount = 0;

    addedPhotoCount = 0;
    modifiedAlbumCount = 0;
    removedPhotoCount = 0;

    pendingAlbumListings.clear();
}


Syncer::Syncer(QObject *parent, Buteo::SyncProfile *syncProfile)
    : WebDavSyncer(parent, syncProfile, QStringLiteral("nextcloud-images"))
    , m_manager(new Accounts::Manager(this))
{
}

Syncer::~Syncer()
{
}

void Syncer::purgeDeletedAccounts()
{
    SyncCache::ImageDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Images/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << "Failed to open database:" << error.errorMessage;
        return;
    }

    QVector<SyncCache::User> usersToDelete;
    const QVector<SyncCache::User> users = db.users(&error);
    for (const SyncCache::User &user : users) {
        if (!m_manager->account(user.accountId)) {
            usersToDelete.append(user);
        }
    }

    if (usersToDelete.count() > 0) {
        if (!db.beginTransaction(&error)) {
            qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to begin transaction:" << error.errorCode << error.errorMessage;
            return;
        }

        for (const SyncCache::User &user : usersToDelete) {
            qCDebug(lcNextcloud) << Q_FUNC_INFO << "Account" << user.accountId
                      << "has been deleted, purge associated user:" << user.userId << user.displayName;
            db.deleteUser(user, &error);
            if (error.errorCode != SyncCache::DatabaseError::NoError) {
                qCWarning(lcNextcloud) << "Failed to delete user for account:" << user.accountId
                            << ":" << error.errorMessage;
            }
            deleteFilesForAccount(user.accountId);
        }

        if (error.errorCode != SyncCache::DatabaseError::NoError) {
            db.rollbackTransaction(&error);
        } else if (!db.commitTransaction(&error)) {
            qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to commit transaction:" << error.errorCode << error.errorMessage;
        }
    }
}

void Syncer::beginSync()
{
    // In case a previous purge was interrupted or failed, ensure the db is up-to-date.
    purgeDeletedAccounts();

    QNetworkReply *reply = m_requestGenerator->userInfo(NetworkRequestGenerator::JsonContentType);
    if (reply) {
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleUserInfoReply);
    } else {
        finishWithError(QStringLiteral("Failed to start user info request"));
    }
}

void Syncer::handleUserInfoReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        finishWithHttpError("User info request failed", httpCode);
        return;
    }

    NetworkReplyParser::User user = JsonReplyParser::parseUserResponse(reply->readAll());
    if (user.userId.isEmpty()) {
        finishWithError("No user id found in server response");
        return;
    }

    // Store the user.
    SyncCache::ImageDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Images/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);
    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << "Failed to open database to store user for account:" << m_accountId
                    << ":" << error.errorMessage;
        return;
    }
    SyncCache::User currentUser;
    currentUser.accountId = m_accountId;
    currentUser.userId = user.userId;
    currentUser.displayName = user.displayName;
    db.storeUser(currentUser, &error);
    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << "Failed to store user:" << currentUser.userId
                    << error.errorCode << error.errorMessage;
    }

    Accounts::Account *account = m_manager->account(m_accountId);
    if (account) {
        const Accounts::ServiceList services = account->services();
        for (const Accounts::Service &service : services) {
            if (service.name() == m_serviceName) {
                account->selectService(service);
                m_dirListingRootPath = account->value(QStringLiteral("images_path")).toString().trimmed();
                break;
            }
        }
    }
    account->selectService(Accounts::Service());

    m_userId = user.userId;
    if (m_dirListingRootPath.isEmpty()) {
        m_dirListingRootPath = QString("/remote.php/dav/files/%1/Photos/").arg(user.userId);
    }

    m_forceFullSync = !m_syncProfile->lastSuccessfulSyncTime().isValid();
    m_syncProgressInfo.reset();

    qCDebug(lcNextcloud) << "Starting sync for account:" << m_accountId
              << "user:" << m_userId
              << "root path:" << m_dirListingRootPath
              << "force full sync?" << m_forceFullSync
              << "last sync was:" << m_syncProfile->lastSuccessfulSyncTime().toString();

    if (!performDirListingRequest(m_dirListingRootPath)) {
        WebDavSyncer::finishWithError("Directory list request failed");
    }
}

bool Syncer::performDirListingRequest(const QString &remoteDirPath)
{
    qCDebug(lcNextcloud) << "Fetching directory listing for" << remoteDirPath;

    QNetworkReply *reply = m_requestGenerator->dirListing(remoteDirPath);
    if (reply) {
        reply->setProperty("remoteDirPath", remoteDirPath);
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleDirListingReply);
        return true;
    }

    return false;
}

void Syncer::handleDirListingReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const QByteArray replyData = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString remoteDirPath = reply->property("remoteDirPath").toString();

    if (reply->error() != QNetworkReply::NoError) {
        WebDavSyncer::finishWithHttpError("Remote directory listing failed", httpCode);
        return;
    }

    const QList<NetworkReplyParser::Resource> resourceList = XmlReplyParser::parsePropFindResponse(replyData);
    const ReplyParser::GalleryMetadata metadata =
            ReplyParser::galleryMetadataFromResources(this, m_dirListingRootPath, remoteDirPath, resourceList);

    if (processQueriedAlbum(metadata.album, metadata.photos, metadata.subAlbums)) {
        if (m_syncProgressInfo.pendingAlbumListings.count() > 0) {
            performDirListingRequest(m_syncProgressInfo.pendingAlbumListings.takeLast());
        }
    }
}

bool Syncer::processQueriedAlbum(const SyncCache::Album &queriedAlbum,
                                 const QVector<SyncCache::Photo> &photos,
                                 const QVector<SyncCache::Album> &subAlbums)
{
    qCDebug(lcNextcloud) << Q_FUNC_INFO << queriedAlbum.albumId
              << "with" << photos.count() << "photos and"
              << subAlbums.count() << "sub-albums";

    SyncCache::ImageDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Images/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to open database:" << error.errorCode << error.errorMessage;
        emit syncFailed();
        return false;
    }

    if (!db.beginTransaction(&error)) {
        qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to begin transaction:" << error.errorCode << error.errorMessage;
        emit syncFailed();
        return false;
    }

    // Update the db for the main fetched album and its photos
    if (calculateAndApplyDelta(queriedAlbum, photos, subAlbums, &db, &error)) {

        // Look for sub-albums that have changed and need to be refreshed from the server.
        for (QVector<SyncCache::Album>::ConstIterator it = subAlbums.constBegin();
             it != subAlbums.constEnd(); ++it) {
            const SyncCache::Album &serverAlbum = *it;
            SyncCache::Album dbAlbum = db.album(m_accountId, m_userId, serverAlbum.albumId, &error);
            if (error.errorCode != SyncCache::DatabaseError::NoError) {
                qCWarning(lcNextcloud) << Q_FUNC_INFO << "db album() failed for:"
                            << dbAlbum.albumId
                            << error.errorCode << error.errorMessage;
                break;
            }

            // Note that sub-albums do not yet have the correct photoCount set until they are fetched
            // individually, so don't save them to db here.
            const bool isNewAlbum = dbAlbum.albumId.isEmpty();
            const bool isModifiedAlbum = serverAlbum.etag != dbAlbum.etag;

            qCDebug(lcNextcloud) << Q_FUNC_INFO << "Sub-album:" << serverAlbum.albumId
                      << "etag:" << serverAlbum.etag
                      << "new?" << isNewAlbum
                      << "modified?" << isModifiedAlbum;

            if (m_forceFullSync || isNewAlbum || isModifiedAlbum) {
                m_syncProgressInfo.pendingAlbumListings.append(serverAlbum.albumId);
            }
        }
    }

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        db.rollbackTransaction(&error);
        emit syncFailed();
        return false;

    } else if (!db.commitTransaction(&error)) {
        qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to commit transaction:" << error.errorCode << error.errorMessage;
        db.rollbackTransaction(&error);
        emit syncFailed();
        return false;
    }

    const bool allRequestsDone = m_syncProgressInfo.pendingAlbumListings.isEmpty();
    if (allRequestsDone) {
        qCDebug(lcNextcloud) << Q_FUNC_INFO << "Nextcloud images albums A/M/R:"
                  << m_syncProgressInfo.addedAlbumCount
                  << "/" << m_syncProgressInfo.modifiedAlbumCount
                  << "/" << m_syncProgressInfo.removedAlbumCount;
        qCDebug(lcNextcloud) << Q_FUNC_INFO << "Nextcloud images photos A/M/R:"
                  << m_syncProgressInfo.addedPhotoCount
                  << "/" << m_syncProgressInfo.modifiedPhotoCount
                  << "/" << m_syncProgressInfo.removedPhotoCount;
        qCDebug(lcNextcloud) << Q_FUNC_INFO << "Nextcloud images sync with account" << m_accountId << "finished successfully!";

        // Sync was successful. Next time, can do incremental sync based on known etags instead
        // of doing a complete sync of the full remote directory tree.
        WebDavSyncer::finishWithSuccess();
    } else {
        qCDebug(lcNextcloud) << Q_FUNC_INFO << "Remaining albums to fetch:" << m_syncProgressInfo.pendingAlbumListings.count();
    }

    return true;
}

bool Syncer::calculateAndApplyDelta(const SyncCache::Album &mainAlbum,
                                    const QVector<SyncCache::Photo> &photos,
                                    const QVector<SyncCache::Album> &subAlbums,
                                    SyncCache::ImageDatabase *db,
                                    SyncCache::DatabaseError *error)
{
    SyncCache::Album dbAlbum = db->album(m_accountId, m_userId, mainAlbum.albumId, error);
    if (error->errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << Q_FUNC_INFO << "db album() failed for:"
                    << mainAlbum.albumId
                    << error->errorCode << error->errorMessage;
        return false;
    }

    const bool isNewAlbum = dbAlbum.albumId.isEmpty();
    const bool isModifiedAlbum = mainAlbum.etag != dbAlbum.etag
            || mainAlbum.photoCount != dbAlbum.photoCount;

    qCDebug(lcNextcloud) << Q_FUNC_INFO << "Album:" << mainAlbum.albumId
              << "photoCount:" << mainAlbum.photoCount
              << "etag:" << mainAlbum.etag
              << "new?" << isNewAlbum
              << "modified?" << isModifiedAlbum;

    // Check if album is new or modified
    if (isNewAlbum || isModifiedAlbum) {
        db->storeAlbum(mainAlbum, error);
        if (error->errorCode != SyncCache::DatabaseError::NoError) {
            qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to update album:"
                        << mainAlbum.albumId
                        << error->errorCode << error->errorMessage;
            return false;
        }
        if (isNewAlbum) {
            m_syncProgressInfo.addedAlbumCount++;
        } else {
            m_syncProgressInfo.modifiedAlbumCount++;
        }
    }

    if (isModifiedAlbum) {
        // Check for deleted sub-albums.
        // Delete any db sub-albums of this album that are not present on the server.
        QSet<QString> serverSubAlbumIds;
        for (const SyncCache::Album &serverSubAlbum : subAlbums) {
            serverSubAlbumIds.insert(serverSubAlbum.albumId);
        }
        const QVector<SyncCache::Album> dbSubAlbums = db->albums(m_accountId, m_userId, error, mainAlbum.albumId);
        for (const SyncCache::Album &dbSubAlbum : dbSubAlbums) {
            if (!serverSubAlbumIds.remove(dbSubAlbum.albumId)) {
                qCDebug(lcNextcloud) << Q_FUNC_INFO << "Delete album:" << dbSubAlbum.albumId
                          << "with parent" << mainAlbum.albumId;
                db->deleteAlbum(dbSubAlbum, error);  // this deletes all photos in the album as well
                if (error->errorCode != SyncCache::DatabaseError::NoError) {
                    qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to delete album:"
                                << dbSubAlbum.albumId
                                << error->errorCode << error->errorMessage;
                    return false;
                }
                m_syncProgressInfo.removedAlbumCount++;
            }
        }
    }

    // Check for new and modified photos
    QSet<QString> serverPhotoIds;
    for (const SyncCache::Photo &serverPhoto : photos) {
        SyncCache::Photo dbPhoto = db->photo(m_accountId, m_userId, serverPhoto.albumId, serverPhoto.photoId, nullptr);
        const bool isNewPhoto = dbPhoto.photoId.isEmpty();
        if (dbPhoto.etag != serverPhoto.etag) {
            db->storePhoto(serverPhoto, error);
            if (error->errorCode != SyncCache::DatabaseError::NoError) {
                qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to update photo:"
                            << serverPhoto.photoId
                            << serverPhoto.fileName
                            << serverPhoto.albumId
                            << error->errorCode << error->errorMessage;
                return false;
            }

            if (isNewPhoto) {
                m_syncProgressInfo.addedPhotoCount++;
            } else {
                m_syncProgressInfo.modifiedPhotoCount++;
            }
        }

        serverPhotoIds.insert(serverPhoto.photoId);
    }

    // Check for photos deleted from this album.
    // Delete any db photos in this album that are not present on the server.
    const QVector<SyncCache::Photo> dbPhotos = db->photos(m_accountId, m_userId, mainAlbum.albumId, error);
    for (const SyncCache::Photo &dbPhoto : dbPhotos) {
        if (!serverPhotoIds.remove(dbPhoto.photoId)) {
            qCDebug(lcNextcloud) << Q_FUNC_INFO << "Delete photo:" << dbPhoto.photoId << dbPhoto.fileName;
            db->deletePhoto(dbPhoto, error);
            if (error->errorCode != SyncCache::DatabaseError::NoError) {
                qCWarning(lcNextcloud) << Q_FUNC_INFO << "failed to delete photo:"
                            << dbPhoto.photoId
                            << error->errorCode << error->errorMessage;
                return false;
            }
            m_syncProgressInfo.removedPhotoCount++;
        }
    }

    return true;
}

void Syncer::purgeAccount(int accountId)
{
    SyncCache::ImageDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Images/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << "Failed to open database in order to purge Nextcloud images for account:" << accountId
                    << ":" << error.errorMessage;
    } else {
        SyncCache::User user = db.user(accountId, &error);
        if (error.errorCode == SyncCache::DatabaseError::NoError) {
            if (user.userId.isEmpty()) {
                qCWarning(lcNextcloud) << "Failed to find Nextcloud user ID for account:" << accountId
                            << ":" << error.errorMessage;
            } else {
                db.deleteUser(user, &error);
                if (error.errorCode != SyncCache::DatabaseError::NoError) {
                    qCWarning(lcNextcloud) << "Failed to delete user for account:" << accountId
                                << ":" << error.errorMessage;
                }
            }
        } else {
            qCWarning(lcNextcloud) << "Failed to find Nextcloud user for account:" << accountId
                        << ":" << error.errorMessage;
        }
    }

    deleteFilesForAccount(accountId);

    qCDebug(lcNextcloud) << "Purged Nextcloud images for account:" << accountId;
}

void Syncer::deleteFilesForAccount(int accountId)
{
    QDir dir(SyncCache::ImageCache::imageCacheDir(accountId));
    if (dir.exists() && !dir.removeRecursively()) {
        qCWarning(lcNextcloud) << "Failed to purge Nextcloud image cache for account:" << accountId
                    << "in dir:" << dir.absolutePath();
    }
}
