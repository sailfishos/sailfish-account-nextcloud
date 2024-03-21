/****************************************************************************************
**
** Copyright (c) 2020 Open Mobile Platform LLC
** Copyright (c) 2021 Jolla Ltd.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudbackupqueryclient.h"
#include "syncer_p.h"

// Buteo
#include <PluginCbInterface.h>
#include <SyncProfile.h>

Buteo::ClientPlugin* NextcloudBackupQueryClientLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new NextcloudBackupQueryClient(pluginName, profile, cbInterface);
}


NextcloudBackupQueryClient::NextcloudBackupQueryClient(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface *cbInterface)
    : NextcloudBackupOperationClient(pluginName, profile, cbInterface)
{
}

NextcloudBackupQueryClient::~NextcloudBackupQueryClient()
{
}

Syncer *NextcloudBackupQueryClient::newSyncer()
{
    return new Syncer(this, &profile(), Syncer::BackupQuery);
}
