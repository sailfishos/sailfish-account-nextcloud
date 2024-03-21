/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_WEBDAVSYNCER_P_H
#define NEXTCLOUD_WEBDAVSYNCER_P_H

// sailfishaccounts
#include <accountauthenticator.h>

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class WebDavRequestGenerator;
class NetworkRequestGenerator;
namespace Buteo { class SyncProfile; }

class WebDavSyncer : public QObject
{
    Q_OBJECT

public:
    WebDavSyncer(QObject *parent, Buteo::SyncProfile *profile, const QString &serviceName);
   ~WebDavSyncer();

    void startSync(int accountId);
    virtual void abortSync();
    virtual void purgeAccount(int accountId) = 0;

    int accountId() const { return m_accountId; }
    QString serverUrl() const { return m_serverUrl; }
    QString webDavPath() const { return m_webdavPath; }

Q_SIGNALS:
    void syncSucceeded();
    void syncFailed();

protected Q_SLOTS:
    void sync(int accountId, const QString &serviceName, const AccountAuthenticatorCredentials &credentials);
    void signInError(int accountId, const QString &serviceName, const QString &errorString);

protected:
    virtual void beginSync() = 0;
    virtual void cleanUp() {}
    void finishWithHttpError(const QString &errorMessage, int httpCode);
    void finishWithError(const QString &errorMessage);
    void finishWithSuccess();

    Buteo::SyncProfile *m_syncProfile = nullptr;
    AccountAuthenticator *m_auth = nullptr;
    NetworkRequestGenerator *m_requestGenerator = nullptr;
    QNetworkAccessManager m_qnam;
    bool m_syncAborted = false;
    bool m_syncError = false;

    // auth related
    int m_accountId = 0;
    QString m_serviceName;
    QString m_serverUrl;
    QString m_webdavPath;
    QString m_username;
    QString m_password;
    QString m_accessToken;
    bool m_ignoreSslErrors = false;
};

#endif // NEXTCLOUD_WEBDAVSYNCER_P_H
