/*
 * SPDX-FileCopyrightText: 2019 - 2021 Open Mobile Platform LLC
 * SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
 * SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NEXTCLOUDSHAREPLUGIN_H
#define NEXTCLOUDSHAREPLUGIN_H

#include <QtCore/QObject>

#include <sharingplugininterface.h>

class QNetworkAccessManager;
class Q_DECL_EXPORT NextcloudSharePlugin : public QObject, public SharingPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.share.plugin.nextcloud")
    Q_INTERFACES(SharingPluginInterface)

public:
    NextcloudSharePlugin();
    ~NextcloudSharePlugin();

    SharingPluginInfo *infoObject();
    QString pluginId() const;
};

#endif // NEXTCLOUDSHAREPLUGIN_H
