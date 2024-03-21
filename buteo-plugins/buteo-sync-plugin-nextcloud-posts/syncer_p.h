/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_POSTS_SYNCER_P_H
#define NEXTCLOUD_POSTS_SYNCER_P_H

#include "webdavsyncer_p.h"

#include "synccacheevents.h"

#include <QObject>
#include <QString>

class JsonRequestGenerator;

class Syncer : public WebDavSyncer
{
    Q_OBJECT

public:
    Syncer(QObject *parent, Buteo::SyncProfile *profile);
   ~Syncer();

    void purgeAccount(int accountId) override;

private:
    void beginSync() override;
    bool performCapabilitiesRequest();
    void handleCapabilitiesReply();
    bool performNotificationListRequest();
    void handleNotificationListReply();
    bool performNotificationDeleteRequest(const QStringList &notificationIds);
    void handleNotificationDeleteReply();
    bool performNotificationDeleteAllRequest();
    void handleNotificationDeleteAllReply();

    bool m_deleteAllNotifsSupported = false;
    QSet<QString> m_currentDeleteNotificationIds;
};

#endif // NEXTCLOUD_POSTS_SYNCER_P_H
