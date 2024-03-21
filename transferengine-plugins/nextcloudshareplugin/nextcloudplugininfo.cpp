/****************************************************************************************
**
** Copyright (c) 2019 - 2021 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudplugininfo.h"
#include "nextcloudshareservicestatus.h"

NextcloudPluginInfo::NextcloudPluginInfo()
    : SharingPluginInfo()
    , m_nextcloudShareServiceStatus(new NextcloudShareServiceStatus(this))
{
    m_capabilities << QLatin1String("application/*")
                   << QLatin1String("audio/*")
                   << QLatin1String("image/*")
                   << QLatin1String("video/*")
                   << QLatin1String("text/x-vnote")
                   << QLatin1String("text/xml")
                   << QLatin1String("text/plain");

    connect(m_nextcloudShareServiceStatus, &NextcloudShareServiceStatus::serviceReady, this, &NextcloudPluginInfo::serviceReady);
    connect(m_nextcloudShareServiceStatus, &NextcloudShareServiceStatus::serviceError, this, &NextcloudPluginInfo::infoError);
}

NextcloudPluginInfo::~NextcloudPluginInfo()
{
}

QList<SharingMethodInfo> NextcloudPluginInfo::info() const
{
    return m_info;
}

void NextcloudPluginInfo::query()
{
    m_nextcloudShareServiceStatus->queryStatus(NextcloudShareServiceStatus::PassiveMode); // don't perform sign-in.
}

void NextcloudPluginInfo::serviceReady()
{
    m_info.clear();
    for (int i = 0; i < m_nextcloudShareServiceStatus->count(); ++i) {
        SharingMethodInfo info;

        info.setDisplayName(m_nextcloudShareServiceStatus->details(i).providerName);
        info.setSubtitle(m_nextcloudShareServiceStatus->details(i).displayName);
        info.setAccountId(m_nextcloudShareServiceStatus->details(i).accountId);

        info.setMethodId(QLatin1String("Nextcloud"));
        info.setMethodIcon(QLatin1String("image://theme/graphic-m-service-nextcloud"));
        info.setShareUIPath(QLatin1String("/usr/share/nemo-transferengine/plugins/sharing/NextcloudShareFile.qml"));
        info.setCapabilities(m_capabilities);

        m_info << info;
    }

    emit infoReady();
}
