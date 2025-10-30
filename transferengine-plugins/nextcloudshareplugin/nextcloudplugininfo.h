/*
 * SPDX-FileCopyrightText: 2019 - 2021 Open Mobile Platform LLC
 * SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
 * SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NEXTCLOUDPLUGININFO_H
#define NEXTCLOUDPLUGININFO_H

#include <sharingplugininfo.h>
#include <QStringList>

class NextcloudShareServiceStatus;
class NextcloudPluginInfo : public SharingPluginInfo
{
    Q_OBJECT

public:
    NextcloudPluginInfo();
    ~NextcloudPluginInfo();

    QList<SharingMethodInfo> info() const;
    void query();

private Q_SLOTS:
    void serviceReady();

private:
    NextcloudShareServiceStatus *m_nextcloudShareServiceStatus;
    QList<SharingMethodInfo> m_info;
    QStringList m_capabilities;
};

#endif // NEXTCLOUDPLUGININFO_H
