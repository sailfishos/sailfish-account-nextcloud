/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "authenticator.h"

#include <logging.h>

#include <SignOn/AuthSession>
#include <Accounts/Manager>

#include <oauth2data.h>

Authenticator::Authenticator(const QString &accountId, QObject *parent)
    : QObject(parent)
{
    bool ok = false;
    int id = accountId.toInt(&ok);
    if (!ok) {
        qCWarning(lcNextcloud) << accountId << "is not an account id.";
        return;
    }

    Accounts::Manager accManager(QStringLiteral("sync"));

    Accounts::Account *account(Accounts::Account::fromId(&accManager, id));
    if (!account) {
        qCWarning(lcNextcloud) << "cannot find account" << accountId;
        return;
    }
    m_accountService = new Accounts::AccountService
        (account, accManager.service(QStringLiteral("nextcloud-notes")));
    if (!m_accountService) {
        delete account;
        qCWarning(lcNextcloud) << "no nextcloud-notes service for account.";
        return;
    }
    const Accounts::AuthData &authData = m_accountService->authData();
    m_identity = SignOn::Identity::existingIdentity(authData.credentialsId());
    if (!m_identity) {
        qCWarning(lcNextcloud) << "Cannot get existing identity for credentials:" << authData.credentialsId();
    }
}

Authenticator::~Authenticator()
{
    if (m_accountService)
        delete m_accountService->account();
    delete m_accountService;
    delete m_identity;
}

bool Authenticator::isValid() const
{
    return m_identity != nullptr;
}

bool Authenticator::isEnabled() const
{
    return m_accountService
        && m_accountService->isEnabled() && m_accountService->account()->isEnabled();
}

QString Authenticator::serverAddress() const
{
    Accounts::AccountService global(m_accountService->account(), Accounts::Service());
    return m_accountService->value("server_address",
                                   global.value("server_address")).toString();
}

bool Authenticator::start()
{
    if (!isValid())
        return false;

    const Accounts::AuthData &authData = m_accountService->authData();
    SignOn::AuthSessionP session = m_identity->createSession(authData.method());
    if (!session) {
        qCDebug(lcNextcloud) << "Signon session could not be created with method" << authData.method();
        return false;
    }

    SignOn::SessionData data(authData.parameters());
    data.setUiPolicy(SignOn::NoUserInteractionPolicy);

    qCDebug(lcNextcloud) << "starting authentication session with mechanism" << authData.mechanism();
    connect(session.data(), &SignOn::AuthSession::response, this,
            [this, session] (const SignOn::SessionData &sessionData) {
                const Accounts::AuthData &authData = m_accountService->authData();
                qCDebug(lcNextcloud) << "got a response from sso for method" << authData.method();
                if (authData.method() == QStringLiteral("password")) {
                    emit authLoginAvailable(sessionData.UserName(),
                                            sessionData.Secret());
                    emit finished();
                } else if (authData.method() == QStringLiteral("oauth2")) {
                    OAuth2PluginNS::OAuth2PluginTokenData response = sessionData.data<OAuth2PluginNS::OAuth2PluginTokenData>();
                    emit authTokenAvailable(response.AccessToken());
                    emit finished();
                } else {
                    emit errorOccurred(QStringLiteral("Unsupported authentication method."));
                }
                m_identity->destroySession(session);
            });
    connect(session.data(), &SignOn::AuthSession::error, this,
            [this, session] (const SignOn::Error &error) {
                qCDebug(lcNextcloud) << "got an error from sso.";
                emit errorOccurred(error.message());
                m_identity->destroySession(session);
            });
    session->process(data, authData.mechanism());
    return true;
}
