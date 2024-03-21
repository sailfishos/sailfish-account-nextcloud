/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "synccacheimages.h"
#include "synccacheimages_p.h"
#include "synccacheimagedownloads_p.h"

#include <QtCore/QThread>
#include <QtCore/QSaveFile>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QDebug>
#include <QtCore/QUrlQuery>

using namespace SyncCache;

namespace {

const int ImageDownloadTimeout = 60 * 1000;
const int MaxActiveImageRequests = 10;

}

QString SyncCache::imageDownloadDir(int accountId)
{
    return QStringLiteral("%1/nextcloud/account-%2")
            .arg(ImageCache::imageCacheRootDir()).arg(accountId);
}

QString SyncCache::userImageDownloadDir(int accountId, const QString &userId, bool thumbnail)
{
    const QString path = thumbnail
            ? SyncCache::imageDownloadDir(accountId) + "/users-thumbnails"
            : SyncCache::imageDownloadDir(accountId) + "/users";
    return userId.isEmpty() ? path : path + "/" + userId;
}

QString SyncCache::albumImageDownloadDir(int accountId, const QString &albumName, bool thumbnail)
{
    const QString path = thumbnail
            ? SyncCache::imageDownloadDir(accountId) + "/albums-thumbnails"
            : SyncCache::imageDownloadDir(accountId) + "/albums";
    return albumName.isEmpty() ? path : path + "/" + albumName;
}

//-----------------------------------------------------------------------------

ImageDownloadWatcher::ImageDownloadWatcher(int idempToken, QObject *parent)
    : QObject(parent)
    , m_idempToken(idempToken)
{
}

ImageDownloadWatcher::~ImageDownloadWatcher()
{
}

int ImageDownloadWatcher::idempToken() const
{
    return m_idempToken;
}

//-----------------------------------------------------------------------------

ImageDownload::ImageDownload(int idempToken,
        const QUrl &imageUrl,
        const QString &fileName,
        const QString &fileDirPath,
        const QNetworkRequest &templateRequest,
        ImageDownloadWatcher *watcher)
    : m_idempToken(idempToken)
    , m_imageUrl(imageUrl)
    , m_fileName(fileName)
    , m_fileDirPath(fileDirPath)
    , m_templateRequest(templateRequest)
    , m_watcher(watcher)
{
    m_timeoutTimer = new QTimer;
}

ImageDownload::~ImageDownload()
{
    m_timeoutTimer->deleteLater();
    if (m_reply) {
        m_reply->deleteLater();
    }
}

void ImageDownload::setStatus(Status status, const QString &error)
{
    m_status = status;
    m_errorString = error;
}

//-----------------------------------------------------------------------------

ImageDownloader::ImageDownloader(QObject *parent)
    : QObject(parent)
{
}

ImageDownloader::~ImageDownloader()
{
}

ImageDownloadWatcher *ImageDownloader::downloadImage(int idempToken,
                                                     const QUrl &imageUrl,
                                                     const QString &fileName,
                                                     const QString &fileDirPath,
                                                     const QNetworkRequest &templateRequest)
{
    ImageDownloadWatcher *watcher = new ImageDownloadWatcher(idempToken, this);
    ImageDownload *download = new ImageDownload(idempToken, imageUrl, fileName, fileDirPath, templateRequest, watcher);
    m_pending.enqueue(download);
    QMetaObject::invokeMethod(this, "triggerDownload", Qt::QueuedConnection);
    return watcher; // caller takes ownership.
}

void ImageDownloader::triggerDownload()
{
    while (m_active.size() && (m_active.head() == nullptr || !m_active.head()->m_timeoutTimer->isActive())) {
        delete m_active.dequeue();
    }

    while (m_active.size() < MaxActiveImageRequests && m_pending.size()) {
        ImageDownload *download = m_pending.dequeue();
        m_active.enqueue(download);

        connect(download->m_timeoutTimer, &QTimer::timeout, this, [this, download] {
            download->setStatus(ImageDownload::Error,
                                QStringLiteral("Image download timed out for %1").arg(download->m_imageUrl.toString()));
            QMetaObject::invokeMethod(this, "eraseInactiveDownloads", Qt::QueuedConnection);
        });

        download->m_timeoutTimer->setInterval(ImageDownloadTimeout);
        download->m_timeoutTimer->setSingleShot(true);
        download->m_timeoutTimer->start();

        QNetworkRequest request(download->m_templateRequest);
        QUrl imageUrl(download->m_imageUrl);
        imageUrl.setUserName(download->m_templateRequest.url().userName());
        imageUrl.setPassword(download->m_templateRequest.url().password());
        request.setUrl(imageUrl);

        QNetworkReply *reply = m_qnam.get(request);
        download->m_reply = reply;
        if (reply) {
            connect(reply, &QNetworkReply::finished, this, [this, reply, download] {
                if (reply->error() != QNetworkReply::NoError) {
                    download->setStatus(ImageDownload::Error, QStringLiteral("Image download error: %1").arg(reply->errorString()));
                } else {
                    // save the file to the appropriate location.
                    const QByteArray replyData = reply->readAll();
                    if (replyData.isEmpty()) {
                        download->setStatus(ImageDownload::Error, QStringLiteral("Empty image data received, aborting"));
                    } else {
                        QDir dir(download->m_fileDirPath);
                        if (!dir.exists()) {
                            dir.mkpath(QStringLiteral("."));
                        }
                        QSaveFile file(dir.absoluteFilePath(download->m_fileName));
                        if (!file.open(QFile::WriteOnly)) {
                            download->setStatus(ImageDownload::Error,
                                                QStringLiteral("Error opening image file %1 for writing: %2")
                                                        .arg(file.fileName(), file.errorString()));
                        } else {
                            qint64 bytesToWrite = replyData.size();
                            while (bytesToWrite > 0) {
                                const qint64 written = file.write(replyData.mid(replyData.size() - bytesToWrite));
                                if (written >= 0) {
                                    bytesToWrite -= written;
                                } else {
                                    // error occurred while writing file
                                    break;
                                }
                            }
                            if (bytesToWrite == 0 && file.commit()) {
                                download->setStatus(ImageDownload::Downloaded);
                            } else {
                                download->setStatus(ImageDownload::Error,
                                                    QStringLiteral("Error writing image file %1 data: %2")
                                                              .arg(file.fileName(), file.errorString()));
                            }
                        }
                    }
                }

                QMetaObject::invokeMethod(this, "eraseInactiveDownloads", Qt::QueuedConnection);
            });
        }
    }
}

void ImageDownloader::eraseInactiveDownloads()
{
    for (QQueue<ImageDownload*>::iterator it = m_active.begin() ; it != m_active.end();) {
        ImageDownload *download = *it;
        if (download->m_status == ImageDownload::InProgress) {
            ++it;
        } else if (download->m_status == ImageDownload::Downloaded) {
            if (download->m_watcher) {
                emit download->m_watcher->downloadFinished(download->m_fileDirPath + '/' + download->m_fileName);
            }
            it = m_active.erase(it);
            delete download;
        } else if (download->m_status == ImageDownload::Error) {
            if (download->m_watcher) {
                emit download->m_watcher->downloadFailed(download->m_errorString);
            }
            it = m_active.erase(it);
            delete download;
        }
    }

    QMetaObject::invokeMethod(this, "triggerDownload", Qt::QueuedConnection);
}
