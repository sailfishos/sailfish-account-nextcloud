/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextclouduploader.h"
#include "nextcloudshareservicestatus.h"
#include "nextcloudapi.h"

// transferengine-qt5
#include <mediaitem.h>
#include <imageoperation.h>

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QtDebug>

NextcloudUploader::NextcloudUploader(QNetworkAccessManager *qnam, QObject *parent)
    : MediaTransferInterface(parent)
    , m_qnam(qnam)
{
}

NextcloudUploader::~NextcloudUploader()
{
    cleanUp();
}

QString NextcloudUploader::displayName() const
{
    return tr("Nextcloud");
}

QUrl NextcloudUploader::serviceIcon() const
{
    return QUrl("image://theme/graphic-s-service-nextcloud");
}

bool NextcloudUploader::cancelEnabled() const
{
    return true;
}

bool NextcloudUploader::restartEnabled() const
{
    return true;
}

void NextcloudUploader::start()
{
    // This is the point where we would actually send the file for update
    if (!mediaItem()) {
        qWarning() << Q_FUNC_INFO << "NULL MediaItem. Can't continue";
        setStatus(MediaTransferInterface::TransferInterrupted);
        return;
    }

    // Query status and get access token within the response
    if (!m_nextcloudShareServiceStatus) {
        m_nextcloudShareServiceStatus = new NextcloudShareServiceStatus(this);
        connect(m_nextcloudShareServiceStatus, &NextcloudShareServiceStatus::serviceReady, this, &NextcloudUploader::startUploading);
    }
    m_nextcloudShareServiceStatus->queryStatus();
}

void NextcloudUploader::cancel()
{
    if (m_api != 0) {
        m_api->cancelUpload();
    } else {
        qWarning() << Q_FUNC_INFO << "Can't cancel. NULL Nextcloud API object!";
    }
}

void NextcloudUploader::startUploading()
{
    if (!m_nextcloudShareServiceStatus) {
        qWarning() << Q_FUNC_INFO << "NULL NextcloudShareServiceStatus object!";
        return;
    }

    // In a case of having multiple accounts get the token based on account id
    const quint32 accountId = mediaItem()->value(MediaItem::AccountId).toInt();
    m_accountDetails = m_nextcloudShareServiceStatus->detailsByIdentifier(accountId);
    postFile();
}

void NextcloudUploader::transferFinished()
{
    setStatus(MediaTransferInterface::TransferFinished);
    cleanUp();
}

void NextcloudUploader::transferProgress(qreal progress)
{
    setProgress(progress);
}

void NextcloudUploader::transferError()
{
    setStatus(MediaTransferInterface::TransferInterrupted);
    qWarning() << Q_FUNC_INFO << "Transfer interrupted";
    cleanUp();
}

void NextcloudUploader::transferCanceled()
{
    setStatus(MediaTransferInterface::TransferCanceled);
    qWarning() << Q_FUNC_INFO << "Transfer canceled";
    cleanUp();
}

void NextcloudUploader::credentialsExpired()
{
    quint32 accountId = mediaItem()->value(MediaItem::AccountId).toInt();
    m_nextcloudShareServiceStatus->setCredentialsNeedUpdate(accountId, QStringLiteral("nextcloud-sharing"));
}

void NextcloudUploader::postFile()
{
    m_filePath = mediaItem()->value(MediaItem::Url).toUrl().toLocalFile();

    if (m_filePath.isEmpty()) {
        QByteArray content = mediaItem()->value(MediaItem::ContentData).toByteArray();
        if (content.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "No file name and no content specified, nothing to send";
            transferError();
            return;
        }

        static const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                + QStringLiteral("/.local/share/system/privileged/Sharing/tmp");
        QDir dir(dirPath);
        if (!dir.mkpath(".")) {
            qWarning() << "Cannot make path:" << dir.absolutePath();
            transferError();
            return;
        }

        m_contentFile = new QFile(dir.absoluteFilePath(mediaItem()->value(MediaItem::ResourceName).toString()), this);
        if (!m_contentFile->open(QFile::WriteOnly)) {
            qWarning() << Q_FUNC_INFO << "Cannot open file for writing:" << m_contentFile->fileName();
            transferError();
            return;
        }
        if (m_contentFile->write(content) < 0) {
            qWarning() << Q_FUNC_INFO << "Cannot write content data to file:" << m_contentFile->fileName();
            transferError();
            return;
        }
        m_contentFile->close();
        m_filePath = m_contentFile->fileName();
    }

    const QVariantMap userData = mediaItem()->value(MediaItem::UserData).toMap();

    QString remoteDirPath = m_accountDetails.webdavPath;
    if (!remoteDirPath.endsWith("/")) {
        remoteDirPath += '/';
    }
    QString remoteDirName = userData.value("remoteDirName").toString();
    while (remoteDirName.startsWith('/')) {
        remoteDirName = remoteDirName.mid(1);
    }
    remoteDirPath += remoteDirName;
    if (!remoteDirName.isEmpty() && !remoteDirPath.endsWith('/')) {
        remoteDirPath += '/';
    }

    if (!m_api) {
        m_api = new NextcloudApi(m_qnam, this);
        connect(m_api, &NextcloudApi::transferProgressUpdated, this, &NextcloudUploader::transferProgress);
        connect(m_api, &NextcloudApi::transferFinished, this, &NextcloudUploader::transferFinished);
        connect(m_api, &NextcloudApi::transferError, this, &NextcloudUploader::transferError);
        connect(m_api, &NextcloudApi::transferCanceled, this, &NextcloudUploader::transferCanceled);
        connect(m_api, &NextcloudApi::credentialsExpired, this, &NextcloudUploader::credentialsExpired);
    }
    setStatus(MediaTransferInterface::TransferStarted);

    QObject *contextObj = new QObject(this);
    connect(m_api, &NextcloudApi::propfindFinished, contextObj, [this, contextObj, remoteDirPath] {
        contextObj->deleteLater();
        m_api->uploadFile(m_filePath,
                          m_accountDetails.accessToken,
                          m_accountDetails.username,
                          m_accountDetails.password,
                          m_accountDetails.serverAddress,
                          remoteDirPath,
                          m_accountDetails.ignoreSslErrors);
    });
    m_api->propfindPath(m_accountDetails.accessToken,
                        m_accountDetails.username,
                        m_accountDetails.password,
                        m_accountDetails.serverAddress,
                        remoteDirPath);
}

void NextcloudUploader::cleanUp()
{
    if (m_contentFile) {
        if (!m_contentFile->remove()) {
            qWarning() << "unable to remove file" << m_contentFile->fileName();
        }
        delete m_contentFile;
        m_contentFile = nullptr;
    }
}
