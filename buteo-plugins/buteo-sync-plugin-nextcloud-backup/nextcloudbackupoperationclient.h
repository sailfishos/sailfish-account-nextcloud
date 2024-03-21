/****************************************************************************************
**
** Copyright (c) 2020 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_BACKUPOPERATIONCLIENT_H
#define NEXTCLOUD_BACKUPOPERATIONCLIENT_H

// Buteo
#include <ClientPlugin.h>
#include <SyncProfile.h>
#include <SyncResults.h>
#include <SyncCommonDefs.h>

class Syncer;
class Q_DECL_EXPORT NextcloudBackupOperationClient : public Buteo::ClientPlugin
{
    Q_OBJECT

public:
    NextcloudBackupOperationClient(
            const QString &pluginName,
            const Buteo::SyncProfile &profile,
            Buteo::PluginCbInterface *cbInterface);
    ~NextcloudBackupOperationClient();

    bool init();
    bool uninit();
    bool startSync();
    void abortSync(Buteo::SyncResults::MinorCode minorErrorCode);
    Buteo::SyncResults getSyncResults() const;
    bool cleanUp();

public Q_SLOTS:
    void connectivityStateChanged(Sync::ConnectivityType aType, bool aState);

protected:
    virtual Syncer *newSyncer() = 0;

private Q_SLOTS:
    void syncSucceeded();
    void syncFailed();

private:
    void syncFinished(Buteo::SyncResults::MinorCode minorErrorCode, const QString &message);
    Buteo::SyncProfile::SyncDirection syncDirection();
    Buteo::SyncProfile::ConflictResolutionPolicy conflictResolutionPolicy();

    Syncer* m_syncer;
    Sync::SyncStatus m_syncStatus;
    Buteo::SyncResults m_results;
    Buteo::SyncProfile::SyncDirection m_syncDirection;
    Buteo::SyncProfile::ConflictResolutionPolicy m_conflictResPolicy;
    int m_accountId;
};

#endif // NEXTCLOUD_BACKUPOPERATIONCLIENT_H
