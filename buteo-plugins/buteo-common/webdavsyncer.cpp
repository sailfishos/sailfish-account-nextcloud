/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "webdavsyncer_p.h"
#include "networkreplyparser_p.h"
#include "networkrequestgenerator_p.h"
#include "logging.h"

// buteo
#include <SyncProfile.h>

static const int HTTP_UNAUTHORIZED_ACCESS = 401;

WebDavSyncer::WebDavSyncer(QObject *parent, Buteo::SyncProfile *syncProfile, const QString &serviceName)
    : QObject(parent)
    , m_syncProfile(syncProfile)
    , m_serviceName(serviceName)
{
}

WebDavSyncer::~WebDavSyncer()
{
    delete m_requestGenerator;
    delete m_auth;
}

void WebDavSyncer::abortSync()
{
    qCDebug(lcNextcloud) << Q_FUNC_INFO << "Aborting sync for" << m_serviceName << "sync with account" << m_accountId;
    m_syncAborted = true;
}

void WebDavSyncer::startSync(int accountId)
{
    Q_ASSERT(accountId != 0);
    m_accountId = accountId;
    delete m_auth;
    m_auth = new AccountAuthenticator(this);
    connect(m_auth, &AccountAuthenticator::signInCompleted,
            this, &WebDavSyncer::sync);
    connect(m_auth, &AccountAuthenticator::signInError,
            this, &WebDavSyncer::signInError);
    qCDebug(lcNextcloud) << Q_FUNC_INFO << "starting" << m_serviceName << "sync with account" << m_accountId;
    m_auth->signIn(accountId, m_serviceName);
}

void WebDavSyncer::signInError(int accountId, const QString &serviceName, const QString &errorString)
{
    qCDebug(lcNextcloud) << Q_FUNC_INFO << "Sign-in failed for"
              << serviceName
              << "sync with account" << accountId
              << "error:" << errorString;
    emit syncFailed();
}

void WebDavSyncer::sync(int, const QString &, const AccountAuthenticatorCredentials &credentials)
{
    qCDebug(lcNextcloud) << Q_FUNC_INFO << "Auth succeeded, start sync for service" << m_serviceName << "with account" << m_accountId;

    const bool ignoreSslErrors = credentials.serviceSettings.value(QStringLiteral("ignore_ssl_errors")).toBool();
    const QString serverAddress = credentials.serviceSettings.value(QStringLiteral("server_address")).toString();
    const QString webdavPath = credentials.serviceSettings.value(QStringLiteral("webdav_path")).toString();

    m_serverUrl = serverAddress;
    m_webdavPath = webdavPath.isEmpty()
            ? QStringLiteral("/remote.php/dav/files/") + credentials.username
            : webdavPath;
    m_username = credentials.username;
    m_password = credentials.password;
    m_accessToken = credentials.accessToken;
    m_ignoreSslErrors = ignoreSslErrors;

    delete m_requestGenerator;
    m_requestGenerator = m_accessToken.isEmpty()
                       ? new NetworkRequestGenerator(&m_qnam, m_serverUrl, m_username, m_password)
                       : new NetworkRequestGenerator(&m_qnam, m_serverUrl, m_accessToken);

    beginSync();
}

void WebDavSyncer::finishWithHttpError(const QString &errorMessage, int httpCode)
{
    m_syncError = true;
    if (httpCode == HTTP_UNAUTHORIZED_ACCESS) {
        m_auth->setCredentialsNeedUpdate(m_accountId, m_serviceName);
    }
    finishWithError(QString("%1 (http status=%2)").arg(errorMessage).arg(httpCode));
}

void WebDavSyncer::finishWithError(const QString &errorMessage)
{
    qCWarning(lcNextcloud) << "Nextcloud" << m_serviceName << "sync for account" << m_accountId << "finished with error:" << errorMessage;
    m_syncError = true;
    cleanUp();
    emit syncFailed();
}

void WebDavSyncer::finishWithSuccess()
{
    qCDebug(lcNextcloud) << Q_FUNC_INFO << "Nextcloud" << m_serviceName << "sync with account" << m_accountId << "finished successfully!";
    cleanUp();
    emit syncSucceeded();
}
