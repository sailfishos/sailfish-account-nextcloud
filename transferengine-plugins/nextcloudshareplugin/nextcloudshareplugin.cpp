// SPDX-FileCopyrightText: 2019 - 2021 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include "nextcloudshareplugin.h"
#include "nextcloudplugininfo.h"

#include <QtPlugin>

NextcloudSharePlugin::NextcloudSharePlugin()
    : QObject(), SharingPluginInterface()
{
}

NextcloudSharePlugin::~NextcloudSharePlugin()
{
}

SharingPluginInfo *NextcloudSharePlugin::infoObject()
{
    return new NextcloudPluginInfo;
}

QString NextcloudSharePlugin::pluginId() const
{
    return QLatin1String("Nextcloud");
}
