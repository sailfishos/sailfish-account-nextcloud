/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLUGIN_H
#define PLUGIN_H

#include <ClientPlugin.h>
#include <SyncPluginLoader.h>

#include <QObject>

#include "authenticator.h"
#include "syncer.h"

class Q_DECL_EXPORT NextcloudNotesClient : public Buteo::ClientPlugin
{
    Q_OBJECT

public:
    NextcloudNotesClient(
            const QString &pluginName,
            const Buteo::SyncProfile &profile,
            Buteo::PluginCbInterface *cbInterface);
    ~NextcloudNotesClient();

    bool init() override;
    bool uninit() override;
    bool startSync() override;
    void abortSync(Sync::SyncStatus status) override;
    Buteo::SyncResults getSyncResults() const override;
    bool cleanUp() override;

    void connectivityStateChanged(Sync::ConnectivityType type, bool state);

private:
    Authenticator *m_auth = nullptr;
    NotesSyncer *m_syncer = nullptr;
    Buteo::SyncResults m_results;
};

class NextcloudNotesClientLoader : public Buteo::SyncPluginLoader
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.plugins.sync.NextcloudNotesClientLoader")
    Q_INTERFACES(Buteo::SyncPluginLoader)

public:
    Buteo::ClientPlugin* createClientPlugin(const QString& pluginName,
                                            const Buteo::SyncProfile& profile,
                                            Buteo::PluginCbInterface* cbInterface) override;
};

#endif
