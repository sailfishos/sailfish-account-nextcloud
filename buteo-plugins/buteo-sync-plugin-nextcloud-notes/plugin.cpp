/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "plugin.h"

#include "logging.h"

#include <ProfileEngineDefs.h>

Buteo::ClientPlugin* NextcloudNotesClientLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new NextcloudNotesClient(pluginName, profile, cbInterface);
}

NextcloudNotesClient::NextcloudNotesClient(const QString& pluginName,
                                           const Buteo::SyncProfile& profile,
                                           Buteo::PluginCbInterface *cbInterface)
    : ClientPlugin(pluginName, profile, cbInterface)
{
}

NextcloudNotesClient::~NextcloudNotesClient()
{
}

bool NextcloudNotesClient::init()
{
    m_auth = new Authenticator(iProfile.key(Buteo::KEY_ACCOUNT_ID), this);
    if (!m_auth->isValid()) {
        qCWarning(lcNextcloud) << "cannot create an authenticator for account id"
                               << iProfile.key(Buteo::KEY_ACCOUNT_ID);
        delete m_auth;
        m_auth = nullptr;
        return false;
    }
    if (!m_auth->isEnabled()) {
        qCWarning(lcNextcloud) << "service or account not enabled.";
        delete m_auth;
        m_auth = nullptr;
        return false;
    }

    const QString serverAddress = m_auth->serverAddress();
    if (serverAddress.isEmpty()) {
        qCWarning(lcNextcloud) << "server_address not found in account settings.";
        delete m_auth;
        m_auth = nullptr;
        return false;
    }
    m_syncer = new NotesSyncer(serverAddress,
                               iProfile.key(Buteo::KEY_ACCOUNT_ID),
                               getPluginName(), this);

    connect(m_auth, &Authenticator::authLoginAvailable,
            m_syncer, &NotesSyncer::setAuthLogin);
    connect(m_auth, &Authenticator::authTokenAvailable, this,
            [this] (const QString &token) {
                Q_UNUSED(token); // OAuth2 auth not implemented in note client yet.
                // m_syncer->setAuthToken(token);
            });
    connect(m_auth, &Authenticator::finished, this,
            [this] () {
                m_syncer->sync(iProfile.syncDirection(),
                               iProfile.conflictResolutionPolicy());
            });
    connect(m_auth, &Authenticator::errorOccurred, this,
            [this] (const QString &message) {
                emit error(getProfileName(), message,
                           Buteo::SyncResults::INTERNAL_ERROR);
            });

    connect(m_syncer, &NotesSyncer::syncFinished, this,
            [this] (Buteo::SyncResults results) {
                m_results = results;
                if (results.majorCode() == Buteo::SyncResults::SYNC_RESULT_SUCCESS) {
                    emit success(getProfileName(), QStringLiteral("sync terminated"));
                } else if (results.majorCode() == Buteo::SyncResults::SYNC_RESULT_CANCELLED) {
                    emit error(getProfileName(), QStringLiteral("sync aborted"), Buteo::SyncResults::ABORTED);
                } else {
                    emit error(getProfileName(), QStringLiteral("sync failed"), results.minorCode());
                }
            });

    return true;
}

bool NextcloudNotesClient::uninit()
{
    delete m_auth;
    m_auth = nullptr;
    delete m_syncer;
    m_syncer = nullptr;

    return true;
}

bool NextcloudNotesClient::startSync()
{
    if (!m_auth)
        return false;

    m_auth->start();

    return true;
}

void NextcloudNotesClient::abortSync(Sync::SyncStatus status)
{
    Q_UNUSED(status);

    disconnect(m_auth, 0, this, 0);

    m_syncer->abortSync();
    disconnect(m_syncer, 0, this, 0);
}

Buteo::SyncResults NextcloudNotesClient::getSyncResults() const
{
    return m_results;
}

bool NextcloudNotesClient::cleanUp()
{
    return m_syncer->cleanup();
}

void NextcloudNotesClient::connectivityStateChanged(Sync::ConnectivityType type, bool state)
{
    if (type == Sync::CONNECTIVITY_INTERNET && !state) {
        // we lost connectivity during sync.
        abortSync(Sync::SYNC_CONNECTION_ERROR);
    }
}
