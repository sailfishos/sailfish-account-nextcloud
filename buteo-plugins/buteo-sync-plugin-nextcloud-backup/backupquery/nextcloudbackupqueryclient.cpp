// SPDX-FileCopyrightText: 2020 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2020 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

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
