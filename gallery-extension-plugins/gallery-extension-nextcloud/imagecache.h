/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_GALLERY_IMAGECACHE_H
#define NEXTCLOUD_GALLERY_IMAGECACHE_H

#include "synccacheimages.h"

// sailfishaccounts
#include <accountauthenticator.h>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtNetwork/QNetworkRequest>

class NextcloudImageCache : public SyncCache::ImageCache
{
    Q_OBJECT

public:
    explicit NextcloudImageCache(QObject *parent = nullptr);

    void openDatabase(const QString &) override;
    void populateUserThumbnail(int idempToken, int accountId, const QString &userId, const QNetworkRequest &) override;
    void populateAlbumThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QNetworkRequest &) override;
    void populatePhotoThumbnail(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &) override;
    void populatePhotoImage(int idempToken, int accountId, const QString &userId, const QString &albumId, const QString &photoId, const QNetworkRequest &) override;

    enum PendingRequestType {
        PopulateUserThumbnailType,
        PopulateAlbumThumbnailType,
        PopulatePhotoThumbnailType,
        PopulatePhotoImageType
    };

    struct PendingRequest {
        PendingRequestType type;
        int idempToken;
        int accountId;
        QString userId;
        QString albumId;
        QString photoId;
    };

    QNetworkRequest templateRequest(int accountId, bool requiresBasicAuth = false) const;

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
};

Q_DECLARE_METATYPE(NextcloudImageCache::PendingRequest)
Q_DECLARE_TYPEINFO(NextcloudImageCache::PendingRequest, Q_MOVABLE_TYPE);

#endif // NEXTCLOUD_GALLERY_IMAGECACHE_H
