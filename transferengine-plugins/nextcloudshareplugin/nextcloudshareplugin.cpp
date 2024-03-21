/****************************************************************************************
**
** Copyright (c) 2019 - 2021 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

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
