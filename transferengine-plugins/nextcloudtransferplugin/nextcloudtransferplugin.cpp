// SPDX-FileCopyrightText: 2019 - 2021 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

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
