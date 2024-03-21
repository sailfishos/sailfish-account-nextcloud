/****************************************************************************************
**
** Copyright (c) 2019 - 2021 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

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
