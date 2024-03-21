/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_IMAGES_SYNCER_P_H
#define NEXTCLOUD_IMAGES_SYNCER_P_H

#include "webdavsyncer_p.h"
#include "replyparser_p.h"
#include "synccacheimages.h"

// libaccounts-qt5
#include <Accounts/Manager>

#include <QList>
#include <QHash>
#include <QVector>

class QNetworkReply;

class ReplyParser;
class JsonRequestGenerator;

class Syncer : public WebDavSyncer
{
    Q_OBJECT

public:
    Syncer(QObject *parent, Buteo::SyncProfile *profile);
   ~Syncer();

    void purgeAccount(int accountId) override;

private:
    void handleUserInfoReply();
    bool performDirListingRequest(const QString &remoteDirPath);
    void handleDirListingReply();

    void purgeDeletedAccounts();
    void deleteFilesForAccount(int accountId);

    void beginSync() override;

    bool processQueriedAlbum(const SyncCache::Album &mainAlbum,
                             const QVector<SyncCache::Photo> &photos,
                             const QVector<SyncCache::Album> &subAlbums);

    bool calculateAndApplyDelta(const SyncCache::Album &mainAlbum,
                                const QVector<SyncCache::Photo> &photos,
                                const QVector<SyncCache::Album> &subAlbums,
                                SyncCache::ImageDatabase *db,
                                SyncCache::DatabaseError *error);

    class SyncProgressInfo
    {
    public:
        void reset();

        int addedAlbumCount = 0;
        int modifiedAlbumCount = 0;
        int removedAlbumCount = 0;

        int addedPhotoCount = 0;
        int modifiedPhotoCount = 0;
        int removedPhotoCount = 0;

        QStringList pendingAlbumListings;
    };

    SyncProgressInfo m_syncProgressInfo;
    Accounts::Manager *m_manager = nullptr;
    ReplyParser *m_replyParser = nullptr;
    QString m_userId;
    QString m_dirListingRootPath;
    bool m_forceFullSync = false;
};

#endif // NEXTCLOUD_IMAGES_SYNCER_P_H
