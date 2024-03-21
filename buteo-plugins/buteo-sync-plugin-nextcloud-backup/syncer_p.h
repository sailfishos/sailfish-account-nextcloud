/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_BACKUP_SYNCER_P_H
#define NEXTCLOUD_BACKUP_SYNCER_P_H

#include "webdavsyncer_p.h"

// libaccounts-qt5
#include <Accounts/Manager>

#include <QFileInfo>
#include <QDir>

class QFile;
class WebDavRequestGenerator;
class QDBusInterface;

class Syncer : public WebDavSyncer
{
    Q_OBJECT

public:
    enum Operation {
        Backup,
        BackupQuery,
        BackupRestore
    };
    Q_ENUM(Operation)

    Syncer(QObject *parent, Buteo::SyncProfile *profile, Operation operation);
   ~Syncer();

    void purgeAccount(int accountId) override;

private Q_SLOTS:
    void cloudBackupStatusChanged(int accountId, const QString &status);
    void cloudBackupError(int accountId, const QString &error, const QString &errorString);
    void cloudRestoreStatusChanged(int accountId, const QString &status);
    void cloudRestoreError(int accountId, const QString &error, const QString &errorString);

private:
    void beginSync() override;

    bool performDirCreationRequest(const QStringList &remotePathParts, int remotePathPartsIndex);
    void handleDirCreationReply();

    bool performDirListingRequest(const QString &remoteDirPath);
    void handleDirListingReply();

    bool performUploadRequest(const QString &fileNameList);
    void handleUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void handleUploadReply();

    bool performDownloadRequest(const QString &fileName);
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleDownloadReply();

    void finishWithHttpError(const QString &errorMessage, int httpCode);
    void finishWithError(const QString &errorMessage);
    void finishWithSuccess();

    void cleanUp();

    QString initBackupDir();

    Accounts::Manager *m_manager = nullptr;
    QFile *m_downloadedFile = nullptr;
    QDBusInterface *m_sailfishBackup = nullptr;
    QFileInfo m_localFileInfo;
    QString m_remoteBackupDirPath;
    Operation m_operation = BackupQuery;
};

#endif // NEXTCLOUD_BACKUP_SYNCER_P_H
