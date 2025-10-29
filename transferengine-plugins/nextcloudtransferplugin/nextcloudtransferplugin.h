/*
 * SPDX-FileCopyrightText: 2019 - 2021 Open Mobile Platform LLC
 * SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
 * SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NEXTCLOUDTRANSFERPLUGIN_H
#define NEXTCLOUDTRANSFERPLUGIN_H

#include <QtCore/QObject>

#include <transferplugininterface.h>

class QNetworkAccessManager;
class Q_DECL_EXPORT NextcloudSharePlugin : public QObject, public TransferPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.transfer.plugin.nextcloud")
    Q_INTERFACES(TransferPluginInterface)

public:
    NextcloudSharePlugin();
    ~NextcloudSharePlugin();

    MediaTransferInterface * transferObject();
    QString pluginId() const;

private:
    QNetworkAccessManager *m_qnam;
};

#endif // NEXTCLOUDTRANSFERPLUGIN_H
