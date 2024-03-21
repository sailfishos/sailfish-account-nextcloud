/****************************************************************************************
**
** Copyright (c) 2019 - 2021 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudtransferplugin.h"
#include "nextclouduploader.h"

#include <QtPlugin>
#include <QNetworkAccessManager>

NextcloudSharePlugin::NextcloudSharePlugin()
    : QObject(), TransferPluginInterface()
    , m_qnam(new QNetworkAccessManager(this))
{
}

NextcloudSharePlugin::~NextcloudSharePlugin()
{
}

MediaTransferInterface * NextcloudSharePlugin::transferObject()
{
    return new NextcloudUploader(m_qnam, this);
}

QString NextcloudSharePlugin::pluginId() const
{
    return QLatin1String("Nextcloud");
}
