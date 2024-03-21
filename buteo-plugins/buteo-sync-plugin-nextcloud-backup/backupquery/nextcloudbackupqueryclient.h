/****************************************************************************************
**
** Copyright (c) 2020 Open Mobile Platform LLC
** Copyright (c) 2021 Jolla Ltd.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_BACKUPQUERY_CLIENT_H
#define NEXTCLOUD_BACKUPQUERY_CLIENT_H

#include "nextcloudbackupoperationclient.h"

#include <buteosyncfw5/SyncPluginLoader.h>

class Q_DECL_EXPORT NextcloudBackupQueryClient : public NextcloudBackupOperationClient
{
    Q_OBJECT

public:
    NextcloudBackupQueryClient(
            const QString &pluginName,
            const Buteo::SyncProfile &profile,
            Buteo::PluginCbInterface *cbInterface);
    ~NextcloudBackupQueryClient();

protected:
    Syncer *newSyncer() override;
};

class NextcloudBackupQueryClientLoader : public Buteo::SyncPluginLoader
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.plugins.sync.NextcloudBackupQueryClientLoader")
    Q_INTERFACES(Buteo::SyncPluginLoader)

public:
    Buteo::ClientPlugin* createClientPlugin(const QString& pluginName,
                                            const Buteo::SyncProfile& profile,
                                            Buteo::PluginCbInterface* cbInterface) override;
};

#endif // NEXTCLOUD_BACKUPQUERY_CLIENT_H
