/****************************************************************************************
**
** Copyright (c) 2020 Open Mobile Platform LLC
** Copyright (c) 2021 Jolla Ltd.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudbackuprestoreclient.h"
#include "syncer_p.h"

// Buteo
#include <PluginCbInterface.h>
#include <SyncProfile.h>

Buteo::ClientPlugin* NextcloudBackupRestoreClientLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new NextcloudBackupRestoreClient(pluginName, profile, cbInterface);
}


NextcloudBackupRestoreClient::NextcloudBackupRestoreClient(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface *cbInterface)
    : NextcloudBackupOperationClient(pluginName, profile, cbInterface)
{
}

NextcloudBackupRestoreClient::~NextcloudBackupRestoreClient()
{
}

Syncer *NextcloudBackupRestoreClient::newSyncer()
{
    return new Syncer(this, &profile(), Syncer::BackupRestore);
}


