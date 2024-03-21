/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "syncer_p.h"

#include "networkrequestgenerator_p.h"
#include "networkreplyparser_p.h"
#include "logging.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QStandardPaths>
#include <QtCore/QMimeDatabase>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

// buteo
#include <SyncProfile.h>

// libaccounts-qt5
#include <Accounts/Account>
#include <Accounts/Service>

Syncer::Syncer(QObject *parent, Buteo::SyncProfile *syncProfile, Syncer::Operation operation)
    : WebDavSyncer(parent, syncProfile, QStringLiteral("nextcloud-backup"))
    , m_sailfishBackup(new QDBusInterface("org.sailfishos.backup", "/sailfishbackup", "org.sailfishos.backup", QDBusConnection::sessionBus(), this))
    , m_operation(operation)
{
    m_sailfishBackup->connection().connect(
                m_sailfishBackup->service(), m_sailfishBackup->path(), m_sailfishBackup->interface(),
                "cloudBackupStatusChanged", this, SLOT(cloudBackupStatusChanged(int,QString)));
    m_sailfishBackup->connection().connect(
                m_sailfishBackup->service(), m_sailfishBackup->path(), m_sailfishBackup->interface(),
                "cloudBackupError", this, SLOT(cloudBackupError(int,QString,QString)));
    m_sailfishBackup->connection().connect(
                m_sailfishBackup->service(), m_sailfishBackup->path(), m_sailfishBackup->interface(),
                "cloudRestoreStatusChanged", this, SLOT(cloudRestoreStatusChanged(int,QString)));
    m_sailfishBackup->connection().connect(
                m_sailfishBackup->service(), m_sailfishBackup->path(), m_sailfishBackup->interface(),
                "cloudRestoreError", this, SLOT(cloudRestoreError(int,QString,QString)));
}

Syncer::~Syncer()
{
}

void Syncer::beginSync()
{
    qCDebug(lcNextcloud) << "Starting Nextcloud operation:" << m_operation;

    m_remoteBackupDirPath = initBackupDir();
    if (m_remoteBackupDirPath.isEmpty()) {
        WebDavSyncer::finishWithError("Unable to initialize remote backups directory!");
        return;
    }

    switch (m_operation) {
    case Backup:
    {
        QDBusReply<QString> createBackupReply =
                m_sailfishBackup->call("createBackupForSyncProfile", m_syncProfile->name());
        if (!createBackupReply.isValid() || createBackupReply.value().isEmpty()) {
            WebDavSyncer::finishWithError("Call to createBackupForSyncProfile() failed: "
                                          + createBackupReply.error().name()
                                          + createBackupReply.error().message());
            return;
        }

        // Save the file path, then wait for org.sailfish.backup service to finish creating the
        // backup before continuing in cloudBackupStatusChanged().
        m_localFileInfo = QFileInfo(createBackupReply.value());
        break;
    }
    case BackupQuery:
    {
        if (!performDirListingRequest(m_remoteBackupDirPath)) {
            WebDavSyncer::finishWithError("Directory list request failed");
        }
        break;
    }
    case BackupRestore:
    {
        const QString filePath = m_syncProfile->key(QStringLiteral("sfos-backuprestore-file"));
        if (filePath.isEmpty()) {
           WebDavSyncer::finishWithError("No backup file to restore for: " + m_syncProfile->name());
           return;
        }

        m_localFileInfo = QFileInfo(filePath);
        if (!performDirListingRequest(m_remoteBackupDirPath)) {
            WebDavSyncer::finishWithError("Directory list request failed");
        }
        break;
    }
    }
}

void Syncer::cloudBackupStatusChanged(int accountId, const QString &status)
{
    if (accountId != m_accountId) {
        return;
    }

    qCDebug(lcNextcloud) << "Backup status changed:" << status << "for file:" << m_localFileInfo.absoluteFilePath();

    if (status == QLatin1String("UploadingBackup")) {

        if (!m_localFileInfo.exists()) {
            WebDavSyncer::finishWithError("Backup finished, but cannot find the backup file: " + m_localFileInfo.absoluteFilePath());
            return;
        }

        qCDebug(lcNextcloud) << "Will upload" << m_localFileInfo.absoluteFilePath();

        // Verify the remote dir exists before uploading the file.
        if (!performDirListingRequest(m_remoteBackupDirPath)) {
            WebDavSyncer::finishWithError("Directory list request failed");
        }

    } else if (status == QLatin1String("Canceled")) {
        WebDavSyncer::finishWithError("Cloud backup restore was canceled");

    } else if (status == QLatin1String("Error")) {
        WebDavSyncer::finishWithError("Failed to create backup file: " + m_localFileInfo.absoluteFilePath());
    }
}

void Syncer::cloudBackupError(int accountId, const QString &error, const QString &errorString)
{
    if (accountId != m_accountId) {
        return;
    }

    WebDavSyncer::finishWithError("Cloud backup error was: " + error + " " + errorString);
}

void Syncer::cloudRestoreStatusChanged(int accountId, const QString &status)
{
    if (accountId != m_accountId) {
        return;
    }

    qCDebug(lcNextcloud) << "Backup restore status changed:" << status << "for file:" << m_localFileInfo.absoluteFilePath();

    if (status == QLatin1String("Canceled")) {
        WebDavSyncer::finishWithError("Cloud backup restore was canceled");

    } else if (status == QLatin1String("Error")) {
        WebDavSyncer::finishWithError("Cloud backup restore failed");
    }
}

void Syncer::cloudRestoreError(int accountId, const QString &error, const QString &errorString)
{
    if (accountId != m_accountId) {
        return;
    }

    qCDebug(lcNextcloud) << "Cloud backup restore error was:" << error << errorString;
}

bool Syncer::performDirCreationRequest(const QStringList &remotePathParts, int remotePathPartsIndex)
{
    const QString remoteDirPath = remotePathParts.mid(0, remotePathPartsIndex + 1).join('/');
    QNetworkReply *reply = m_requestGenerator->dirCreation(remoteDirPath);
    if (reply) {
        reply->setProperty("remotePathParts", remotePathParts);
        reply->setProperty("remotePathPartsIndex", remotePathPartsIndex);
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleDirCreationReply);
        return true;
    }

    return false;
}

void Syncer::handleDirCreationReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QStringList remotePathParts = reply->property("remotePathParts").toStringList();
    int remotePathPartsIndex = reply->property("remotePathPartsIndex").toInt();

    if ((remotePathPartsIndex + 1) >= remotePathParts.length()) {
        // This was the last sub-directory creation request.
        if (reply->error() == QNetworkReply::NoError) {
            // Full directory path has been created. The backup file can now be uploaded.
            performUploadRequest(m_localFileInfo.fileName());
        } else {
            WebDavSyncer::finishWithError("Dir creation request returned HTTP status: " + httpCode);
        }
    } else {
        // Continue creating the rest of the remote directories in the remote path. Do this even
        // if the request failed, because it will fail with e.g. HTTP error 405 if the directory
        // already exists, or 401/403 if the directory exists and is above the level at which
        // the user has permissions to create directories.
        performDirCreationRequest(remotePathParts, remotePathPartsIndex + 1);
    }
}

bool Syncer::performDirListingRequest(const QString &remoteDirPath)
{
    QNetworkReply *reply = m_requestGenerator->dirListing(remoteDirPath);
    if (reply) {
        reply->setProperty("remoteDirPath", remoteDirPath);
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleDirListingReply);
        return true;
    }

    return false;
}

void Syncer::handleDirListingReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const QByteArray replyData = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString remoteDirPath = reply->property("remoteDirPath").toString();

    bool dirNotFound = (httpCode == 404);
    if (m_operation == Backup && dirNotFound) {
        // Remote backup directory doesn't exist, so create it.
        QStringList pathParts = remoteDirPath.split('/');
        for (QStringList::iterator it = pathParts.begin(); it != pathParts.end();) {
            const QString part = (*it).remove('/');
            if (part.isEmpty()) {
                it = pathParts.erase(it);
            } else {
                ++it;
            }
        }
        performDirCreationRequest(pathParts, 0);
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (m_operation == BackupQuery
                && dirNotFound) {
            // No backups created yet, that's OK
        } else {
            WebDavSyncer::finishWithHttpError("Remote directory listing failed", httpCode);
            return;
        }
    }

    if (m_operation == BackupQuery) {
        const QList<NetworkReplyParser::Resource> resourceList = XmlReplyParser::parsePropFindResponse(replyData);
        QStringList fileNames;
        for (const NetworkReplyParser::Resource &resource : resourceList) {
            qCDebug(lcNextcloud) << "Found remote file or dir:" << resource.href;
            if (!resource.isCollection) {
                fileNames.append(resource.href.toUtf8());
            }
        }

        QDBusReply<void> setCloudBackupsReply =
                m_sailfishBackup->call("setCloudBackups", m_syncProfile->name(), fileNames);
        if (!setCloudBackupsReply.isValid()) {
            qCWarning(lcNextcloud) << "Call to setCloudBackups() failed:" << setCloudBackupsReply.error().name()
                        << setCloudBackupsReply.error().message();
        }
        WebDavSyncer::finishWithSuccess();

    } else if (m_operation == Backup) {
        // Remote backup directory exists, so upload the file
        if (!performUploadRequest(m_localFileInfo.fileName())) {
            WebDavSyncer::finishWithError("Upload request failed");
        }

    } else if (m_operation == BackupRestore) {
        bool fileFound = false;
        const QList<NetworkReplyParser::Resource> resourceList = XmlReplyParser::parsePropFindResponse(replyData);
        for (const NetworkReplyParser::Resource &resource : resourceList) {
            if (!resource.isCollection) {
                int lastDirSep = resource.href.lastIndexOf('/');
                const QString fileName = resource.href.mid(lastDirSep + 1);
                if (fileName == m_localFileInfo.fileName()) {
                    fileFound = true;
                    break;
                }
            }
        }
        if (fileFound) {
            qCDebug(lcNextcloud) << "Will download file:" << m_localFileInfo.fileName();
            if (!performDownloadRequest(m_localFileInfo.fileName())) {
                WebDavSyncer::finishWithError("Download request failed");
            }
        } else {
            WebDavSyncer::finishWithError("Cannot find file " + m_localFileInfo.fileName()
                                          + " in remote directory: " + remoteDirPath);
        }
    } else {
        WebDavSyncer::finishWithError("Unexpected operation after dir listing finished: " + m_operation);
    }
}

bool Syncer::performUploadRequest(const QString &fileName)
{
    if (fileName.isEmpty()) {
        qCWarning(lcNextcloud) << "No files to upload!";
        return false;
    }

    const QString localFilePath = m_localFileInfo.dir().absoluteFilePath(fileName);
    QFile file(localFilePath);
    if (!file.open(QFile::ReadOnly)) {
        qCWarning(lcNextcloud) << "Cannot open local file to be uploaded:" << file.fileName();
        return false;
    }
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForData(&file);
    const QByteArray fileData = file.readAll();
    file.close();

    const QString remotePath = m_remoteBackupDirPath + '/' + fileName;
    QNetworkReply *reply = m_requestGenerator->upload(mimeType.name(), fileData, remotePath);
    if (reply) {
        connect(reply, &QNetworkReply::uploadProgress,
                this, &Syncer::handleUploadProgress);
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleUploadReply);
        return true;
    }

    return false;
}

void Syncer::handleUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    qCDebug(lcNextcloud) << "Nextcloud uploaded" << bytesSent << "bytes of" << bytesTotal;
}

void Syncer::handleUploadReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        WebDavSyncer::finishWithHttpError("Upload failed", httpCode);
        return;
    }

    WebDavSyncer::finishWithSuccess();
}

bool Syncer::performDownloadRequest(const QString &fileName)
{
    if (fileName.isEmpty()) {
        qCWarning(lcNextcloud) << "No files to download!";
        return false;
    }

    const QString localFilePath = m_localFileInfo.dir().absoluteFilePath(fileName);
    if (m_downloadedFile) {
        delete m_downloadedFile;
    }
    m_downloadedFile = new QFile(localFilePath);
    if (!m_downloadedFile->open(QFile::WriteOnly)) {
        qCWarning(lcNextcloud) << "Cannot open file for writing: " + m_downloadedFile->fileName();
        delete m_downloadedFile;
        m_downloadedFile = nullptr;
        return false;
    }

    const QString remoteFilePath = m_remoteBackupDirPath + '/' + fileName;
    QNetworkReply *reply = m_requestGenerator->download(remoteFilePath);
    if (reply) {
        connect(reply, &QNetworkReply::downloadProgress,
                this, &Syncer::handleDownloadProgress);
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleDownloadReply);
        return true;
    }

    return false;
}

void Syncer::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    qCDebug(lcNextcloud) << "Nextcloud downloaded" << bytesReceived << "bytes of" << bytesTotal;

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!m_downloadedFile) {
        qCWarning(lcNextcloud) << "Download file is not set!";
        reply->abort();
        return;
    }

    if (reply->bytesAvailable() > 0
            && m_downloadedFile->write(reply->readAll()) < 0) {
        qCWarning(lcNextcloud) << "Failed to write" << reply->bytesAvailable()
                    << "bytes to file:" << m_downloadedFile->fileName();
        reply->abort();
    }
}

void Syncer::handleDownloadReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (m_downloadedFile) {
        m_downloadedFile->close();
        delete m_downloadedFile;
        m_downloadedFile = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        WebDavSyncer::finishWithHttpError("Download failed", httpCode);
        return;
    }

    WebDavSyncer::finishWithSuccess();
}

void Syncer::cleanUp()
{
    if (m_operation == Backup) {
        qCDebug(lcNextcloud) << "Deleting created backup file" << m_localFileInfo.absoluteFilePath();
        QFile::remove(m_localFileInfo.absoluteFilePath());
        QDir().rmdir(m_localFileInfo.absolutePath());
    }
}

void Syncer::purgeAccount(int)
{
    // Don't delete backup blobs from server.
    // Nothing to do.
}

QString Syncer::initBackupDir()
{
    QDBusReply<QString> backupDeviceIdReply = m_sailfishBackup->call("backupFileDeviceId");
    const QString backupDeviceId = backupDeviceIdReply.value();
    if (backupDeviceId.isEmpty()) {
        qCWarning(lcNextcloud) << "Backup device ID is invalid! D-Bus error was:"
                    << backupDeviceIdReply.error().message();
        return QString();
    }

    if (!m_manager) {
        m_manager = new Accounts::Manager(this);
    }

    Accounts::Account *account = m_manager->account(m_accountId);
    if (account) {
        const Accounts::ServiceList services = account->services();
        for (const Accounts::Service &service : services) {
            if (service.name() == m_serviceName) {
                account->selectService(service);
                QString backupDir = account->value(QStringLiteral("backups_path")).toString().trimmed();
                account->selectService(Accounts::Service());
                return QDir(backupDir).filePath(backupDeviceId);
            }
        }
    }

    return QDir(webDavPath()).filePath("Sailfish OS/Backups/" + backupDeviceId);
}
