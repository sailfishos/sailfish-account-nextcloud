/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUDSHARESERVICESTATUS_H
#define NEXTCLOUDSHARESERVICESTATUS_H

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

// sailfishaccounts
#include <accountauthenticator.h>

// libaccounts-qt5
#include <Accounts/Manager>
#include <SignOn/SessionData>
#include <SignOn/Error>

class NextcloudShareServiceStatus : public QObject
{
    Q_OBJECT

public:
    NextcloudShareServiceStatus(QObject *parent = 0);

    enum QueryStatusMode {
        PassiveMode = 0, // query account information but don't sign in
        SignInMode = 1   // query account information and sign in
    };
    void queryStatus(QueryStatusMode mode = SignInMode);

    struct AccountDetails {
        int accountId;
        QString providerName;
        QString serviceName;
        QString displayName;
        QString accessToken;
        QString username;
        QString password;
        QString serverAddress;
        QString webdavPath;
        bool ignoreSslErrors;
    };
    AccountDetails details(int index = 0) const;
    AccountDetails detailsByIdentifier(int accountIdentifier) const;
    int count() const;

    bool setCredentialsNeedUpdate(int accountId, const QString &serviceName);

Q_SIGNALS:
    void serviceReady();
    void serviceError(const QString &message);

private Q_SLOTS:
    void signInResponseHandler(int accountId, const QString &serviceName, const AccountAuthenticatorCredentials &credentials);
    void signInErrorHandler(int accountId, const QString &serviceName);

private:
    AccountAuthenticator *m_auth;
    QString m_serviceName;
    enum AccountDetailsState {
        Waiting,
        Populated,
        Error
    };
    void setAccountDetailsState(int accountId, AccountDetailsState state);
    QVector<AccountDetails> m_accountDetails;
    QHash<int, int> m_accountIdToDetailsIdx;
    QHash<int, AccountDetailsState> m_accountDetailsState;
};

#endif // NEXTCLOUDSHARESERVICESTATUS_H
