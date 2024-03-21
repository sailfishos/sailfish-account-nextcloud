/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_EVENTSVIEW_EVENTCACHE_H
#define NEXTCLOUD_EVENTSVIEW_EVENTCACHE_H

#include "synccacheevents.h"

// sailfishaccounts
#include <accountauthenticator.h>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtNetwork/QNetworkRequest>

class NextcloudEventCache : public SyncCache::EventCache
{
    Q_OBJECT
public:
    explicit NextcloudEventCache(QObject *parent = nullptr);

    void openDatabase(const QString &) override;
    void populateEventImage(int idempToken, int accountId, const QString &eventId, const QNetworkRequest &) override;

    enum PendingRequestType {
        PopulateEventImageType
    };

    struct PendingRequest {
        PendingRequestType type;
        int idempToken;
        int accountId;
        QString eventId;
    };

private Q_SLOTS:
    void performRequests();
    void signOnResponse(int accountId, const QString &serviceName, const AccountAuthenticatorCredentials &credentials);
    void signOnError(int accountId, const QString &serviceName, const QString &errorString);

private:
    QList<PendingRequest> m_pendingRequests;
    QList<int> m_pendingAccountRequests;
    QHash<int, int> m_signOnFailCount;
    QHash<int, QPair<QString, QString> > m_accountIdCredentials;
    QHash<int, QString> m_accountIdAccessTokens;
    AccountAuthenticator *m_auth = nullptr;

    void signIn(int accountId);
    void performRequest(const PendingRequest &request);
    QNetworkRequest templateRequest(int accountId) const;
};

Q_DECLARE_METATYPE(NextcloudEventCache::PendingRequest)
Q_DECLARE_TYPEINFO(NextcloudEventCache::PendingRequest, Q_MOVABLE_TYPE);

#endif // NEXTCLOUD_EVENTSVIEW_EVENTCACHE_H
