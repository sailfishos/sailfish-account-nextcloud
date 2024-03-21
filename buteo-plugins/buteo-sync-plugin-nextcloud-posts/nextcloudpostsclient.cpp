/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** Copyright (C) 2021 Jolla Ltd.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudpostsclient.h"
#include "syncer_p.h"
#include "networkrequestgenerator_p.h"
#include "networkreplyparser_p.h"
#include "logging.h"

// Buteo
#include <PluginCbInterface.h>
#include <ProfileEngineDefs.h>
#include <ProfileManager.h>

Buteo::ClientPlugin* NextcloudPostsClientLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new NextcloudPostsClient(pluginName, profile, cbInterface);
}


NextcloudPostsClient::NextcloudPostsClient(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface *cbInterface)
    : ClientPlugin(pluginName, profile, cbInterface)
    , m_syncer(0)
    , m_accountId(0)
{
    if (!lcNextcloudProtocol().isDebugEnabled()) {
        NetworkRequestGenerator::debugEnabled = true;
        NetworkReplyParser::debugEnabled = true;
    }
}

NextcloudPostsClient::~NextcloudPostsClient()
{
}

void NextcloudPostsClient::connectivityStateChanged(Sync::ConnectivityType type, bool state)
{
    qCDebug(lcNextcloud) << "Received connectivity change event:" << type << " changed to " << state;
    if (type == Sync::CONNECTIVITY_INTERNET && !state) {
        // we lost connectivity during sync.
        abortSync(Buteo::SyncResults::CONNECTION_ERROR);
    }
}

bool NextcloudPostsClient::init()
{
    QString accountIdString = iProfile.key(Buteo::KEY_ACCOUNT_ID);
    m_accountId = accountIdString.toInt();
    if (m_accountId == 0) {
        qCCritical(lcNextcloud) << "Nextcloud Posts profile does not specify" << Buteo::KEY_ACCOUNT_ID;
        return false;
    }

    m_syncDirection = iProfile.syncDirection();
    m_conflictResPolicy = iProfile.conflictResolutionPolicy();

    if (!m_syncer) {
        m_syncer = new Syncer(this, &iProfile);
        connect(m_syncer, &Syncer::syncSucceeded,
                this, &NextcloudPostsClient::syncSucceeded);
        connect(m_syncer, &Syncer::syncFailed,
                this, &NextcloudPostsClient::syncFailed);
    }

    return true;
}

bool NextcloudPostsClient::uninit()
{
    delete m_syncer;
    m_syncer = 0;
    return true;
}

bool NextcloudPostsClient::startSync()
{
    if (m_accountId == 0) {
        return false;
    }

    m_syncer->startSync(m_accountId);
    return true;
}

void NextcloudPostsClient::syncSucceeded()
{
    syncFinished(Buteo::SyncResults::NO_ERROR, QString());
}

void NextcloudPostsClient::syncFailed()
{
    syncFinished(Buteo::SyncResults::INTERNAL_ERROR, QString());
}

void NextcloudPostsClient::abortSync(Buteo::SyncResults::MinorCode minorErrorCode)
{
    m_syncer->abortSync();
    syncFinished(minorErrorCode, QStringLiteral("Sync aborted"));
}

void NextcloudPostsClient::syncFinished(Buteo::SyncResults::MinorCode minorErrorCode, const QString &message)
{
    if (minorErrorCode == Buteo::SyncResults::NO_ERROR) {
        qCDebug(lcNextcloud) << "Nextcloud Posts sync succeeded!" << message;
        m_results = Buteo::SyncResults(QDateTime::currentDateTimeUtc(),
                                       Buteo::SyncResults::SYNC_RESULT_SUCCESS,
                                       Buteo::SyncResults::NO_ERROR);
        emit success(getProfileName(), message);
    } else {
        qCCritical(lcNextcloud) << "Nextcloud Posts sync failed:" << minorErrorCode << message;
        m_results = Buteo::SyncResults(iProfile.lastSuccessfulSyncTime(), // don't change the last sync time
                                       Buteo::SyncResults::SYNC_RESULT_FAILED,
                                       minorErrorCode);
        emit error(getProfileName(), message, minorErrorCode);
    }
}

Buteo::SyncResults NextcloudPostsClient::getSyncResults() const
{
    return m_results;
}

bool NextcloudPostsClient::cleanUp()
{
    // This function is called after the account has been deleted.
    QString accountIdString = iProfile.key(Buteo::KEY_ACCOUNT_ID);
    m_accountId = accountIdString.toInt();
    if (m_accountId == 0) {
        qCCritical(lcNextcloud) << "Nextcloud Posts profile does not specify" << Buteo::KEY_ACCOUNT_ID;
        return false;
    }

    if (!m_syncer) {
        m_syncer = new Syncer(this, &iProfile);
    }

    m_syncer->purgeAccount(m_accountId);
    delete m_syncer;
    m_syncer = 0;

    return true;
}
