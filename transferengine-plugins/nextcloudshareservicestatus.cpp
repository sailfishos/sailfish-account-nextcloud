/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudshareservicestatus.h"

#include <Accounts/Manager>

#include <QtDebug>

NextcloudShareServiceStatus::NextcloudShareServiceStatus(QObject *parent)
    : QObject(parent)
    , m_auth(new AccountAuthenticator(this))
    , m_serviceName(QStringLiteral("nextcloud-sharing"))
{
    connect(m_auth, &AccountAuthenticator::signInCompleted,
            this, &NextcloudShareServiceStatus::signInResponseHandler);
    connect(m_auth, &AccountAuthenticator::signInError,
            this, &NextcloudShareServiceStatus::signInErrorHandler);
}

void NextcloudShareServiceStatus::signInResponseHandler(int accountId, const QString &, const AccountAuthenticatorCredentials &credentials)
{
    if (!m_accountIdToDetailsIdx.contains(accountId)) {
        return;
    }

    const bool ignoreSslErrors = credentials.serviceSettings.value(QStringLiteral("ignore_ssl_errors")).toBool();
    const QString serverAddress = credentials.serviceSettings.value(QStringLiteral("server_address")).toString();
    const QString webdavPath = credentials.serviceSettings.value(QStringLiteral("webdav_path")).toString();

    AccountDetails &accountDetails(m_accountDetails[m_accountIdToDetailsIdx[accountId]]);
    accountDetails.accessToken = credentials.accessToken;
    accountDetails.username = credentials.username;
    accountDetails.password = credentials.password;
    accountDetails.serverAddress = serverAddress;
    accountDetails.webdavPath = webdavPath.isEmpty()
            ? QStringLiteral("/remote.php/dav/files/") + credentials.username
            : webdavPath;
    accountDetails.ignoreSslErrors = ignoreSslErrors;

    setAccountDetailsState(accountId, Populated);
}

void NextcloudShareServiceStatus::signInErrorHandler(int accountId, const QString &)
{
    setAccountDetailsState(accountId, Error);
}

void NextcloudShareServiceStatus::setAccountDetailsState(int accountId, AccountDetailsState state)
{
    if (!m_accountIdToDetailsIdx.contains(accountId)) {
        return;
    }

    m_accountDetailsState[accountId] = state;

    bool anyWaiting = false;
    bool anyPopulated = false;
    Q_FOREACH (const int accountId, m_accountDetailsState.keys()) {
        AccountDetailsState accState = m_accountDetailsState.value(accountId, NextcloudShareServiceStatus::Waiting);
        if (accState == NextcloudShareServiceStatus::Waiting) {
            anyWaiting = true;
        } else if (accState == NextcloudShareServiceStatus::Populated) {
            anyPopulated = true;
        }
    }

    if (!anyWaiting) {
        if (anyPopulated) {
            emit serviceReady();
        } else {
            emit serviceError(QStringLiteral("Unable to retrieve Nextcloud account credentials"));
        }
    }
}

int NextcloudShareServiceStatus::count() const
{
    return m_accountDetails.count();
}

bool NextcloudShareServiceStatus::setCredentialsNeedUpdate(int accountId, const QString &serviceName)
{
    return m_auth->setCredentialsNeedUpdate(accountId, serviceName);
}

void NextcloudShareServiceStatus::queryStatus(QueryStatusMode mode)
{
    m_accountDetails.clear();
    m_accountIdToDetailsIdx.clear();
    m_accountDetailsState.clear();

    Accounts::Manager manager;

    bool signInActive = false;
    for (Accounts::AccountId id : manager.accountList()) {
        Accounts::Account *acc = manager.account(id);

        if (!acc) {
            qWarning() << Q_FUNC_INFO << "Failed to get account for id: " << id;
            continue;
        }

        acc->selectService(Accounts::Service());
        const Accounts::Service service(manager.service(m_serviceName));
        const Accounts::ServiceList services = acc->services();
        bool found = false;
        Q_FOREACH (const Accounts::Service &s, services) {
            if (s.name() == m_serviceName) {
                found = true;
                break;
            }
        }

        if (acc->enabled() && service.isValid() && found) {
            if (acc->value(QStringLiteral("CredentialsNeedUpdate")).toBool() == true) {
                // credentials need update for global service, skip the account
                qWarning() << Q_FUNC_INFO << "Credentials need update for account id: " << id;
                continue;
            }
            acc->selectService(service);
            if (acc->value(QStringLiteral("CredentialsNeedUpdate")).toBool() == true) {
                // credentials need update for sharing service, skip the account
                qWarning() << Q_FUNC_INFO << "Credentials need update for account id: " << id;
                acc->selectService(Accounts::Service());
                continue;
            }

            if (acc->enabled()) {
                if (!m_accountIdToDetailsIdx.contains(id)) {
                    AccountDetails details;
                    details.accountId = id;
                    details.providerName = manager.provider(acc->providerName()).displayName();
                    details.serviceName = manager.service(m_serviceName).displayName();
                    details.displayName = acc->displayName();
                    m_accountIdToDetailsIdx.insert(id, m_accountDetails.size());
                    m_accountDetails.append(details);
                }

                if (mode == NextcloudShareServiceStatus::SignInMode) {
                    signInActive = true;
                    m_accountDetailsState.insert(id, NextcloudShareServiceStatus::Waiting);
                    m_auth->signIn(id, m_serviceName);
                }
            }
            acc->selectService(Accounts::Service());
        }
    }

    // either no enabled accounts, or mode was passive (no sign in required).
    if (!signInActive) {
        emit serviceReady();
    }
}

NextcloudShareServiceStatus::AccountDetails NextcloudShareServiceStatus::details(int index) const
{
    if (index < 0 || m_accountDetails.size() <= index) {
        qWarning() << Q_FUNC_INFO << "Index out of range";
        return AccountDetails();
    }

    return m_accountDetails.at(index);
}

NextcloudShareServiceStatus::AccountDetails NextcloudShareServiceStatus::detailsByIdentifier(int accountIdentifier) const
{
    if (!m_accountIdToDetailsIdx.contains(accountIdentifier)) {
        qWarning() << Q_FUNC_INFO << "No details known for account with identifier" << accountIdentifier;
        return AccountDetails();
    }

    return m_accountDetails[m_accountIdToDetailsIdx[accountIdentifier]];
}
