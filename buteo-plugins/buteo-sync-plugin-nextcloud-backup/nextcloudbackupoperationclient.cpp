/****************************************************************************************
**
** Copyright (c) 2020 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudbackupoperationclient.h"
#include "syncer_p.h"
#include "networkrequestgenerator_p.h"
#include "networkreplyparser_p.h"
#include "logging.h"

// Buteo
#include <PluginCbInterface.h>
#include <ProfileEngineDefs.h>
#include <ProfileManager.h>

NextcloudBackupOperationClient::NextcloudBackupOperationClient(const QString& pluginName,
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

NextcloudBackupOperationClient::~NextcloudBackupOperationClient()
{
}

void NextcloudBackupOperationClient::connectivityStateChanged(Sync::ConnectivityType type, bool state)
{
    qCDebug(lcNextcloud) << "Received connectivity change event:" << type << " changed to " << state;
    if (type == Sync::CONNECTIVITY_INTERNET && !state) {
        // we lost connectivity during sync.
        abortSync(Buteo::SyncResults::CONNECTION_ERROR);
    }
}

bool NextcloudBackupOperationClient::init()
{
    QString accountIdString = iProfile.key(Buteo::KEY_ACCOUNT_ID);
    m_accountId = accountIdString.toInt();
    if (m_accountId == 0) {
        qCCritical(lcNextcloud) << "Nextcloud Backup profile does not specify" << Buteo::KEY_ACCOUNT_ID;
        return false;
    }

    m_syncDirection = iProfile.syncDirection();
    m_conflictResPolicy = iProfile.conflictResolutionPolicy();

    if (!m_syncer) {
        m_syncer = newSyncer();
        connect(m_syncer, &Syncer::syncSucceeded,
                this, &NextcloudBackupOperationClient::syncSucceeded);
        connect(m_syncer, &Syncer::syncFailed,
                this, &NextcloudBackupOperationClient::syncFailed);
    }

    return true;
}

bool NextcloudBackupOperationClient::uninit()
{
    delete m_syncer;
    m_syncer = 0;
    return true;
}

bool NextcloudBackupOperationClient::startSync()
{
    if (m_accountId == 0) {
        return false;
    }

    m_syncer->startSync(m_accountId);
    return true;
}

void NextcloudBackupOperationClient::syncSucceeded()
{
    syncFinished(Buteo::SyncResults::NO_ERROR, QString());
}

void NextcloudBackupOperationClient::syncFailed()
{
    syncFinished(Buteo::SyncResults::INTERNAL_ERROR, QString());
}

void NextcloudBackupOperationClient::abortSync(Buteo::SyncResults::MinorCode minorErrorCode)
{
    m_syncer->abortSync();
    syncFinished(minorErrorCode, QStringLiteral("Sync aborted"));
}

void NextcloudBackupOperationClient::syncFinished(Buteo::SyncResults::MinorCode minorErrorCode, const QString &message)
{
    if (minorErrorCode == Buteo::SyncResults::NO_ERROR) {
        qCDebug(lcNextcloud) << "Nextcloud Backup sync succeeded!" << message;
        m_results = Buteo::SyncResults(QDateTime::currentDateTimeUtc(),
                                       Buteo::SyncResults::SYNC_RESULT_SUCCESS,
                                       Buteo::SyncResults::NO_ERROR);
        emit success(getProfileName(), message);
    } else {
        qCCritical(lcNextcloud) << "Nextcloud Backup sync failed:" << minorErrorCode << message;
        m_results = Buteo::SyncResults(iProfile.lastSuccessfulSyncTime(), // don't change the last sync time
                                       Buteo::SyncResults::SYNC_RESULT_FAILED,
                                       minorErrorCode);
        emit error(getProfileName(), message, minorErrorCode);
    }
}

Buteo::SyncResults NextcloudBackupOperationClient::getSyncResults() const
{
    return m_results;
}

bool NextcloudBackupOperationClient::cleanUp()
{
    // This function is called after the account has been deleted.
    QString accountIdString = iProfile.key(Buteo::KEY_ACCOUNT_ID);
    m_accountId = accountIdString.toInt();
    if (m_accountId == 0) {
        qCCritical(lcNextcloud) << "Nextcloud Backup profile does not specify" << Buteo::KEY_ACCOUNT_ID;
        return false;
    }

    if (!m_syncer) {
        m_syncer = newSyncer();
    }

    m_syncer->purgeAccount(m_accountId);
    delete m_syncer;
    m_syncer = 0;

    return true;
}
