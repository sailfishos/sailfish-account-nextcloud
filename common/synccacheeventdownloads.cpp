/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "synccacheeventdownloads_p.h"

#include <QtCore/QSaveFile>
#include <QtCore/QFile>
#include <QtCore/QDir>

using namespace SyncCache;

namespace {

const int MaxActiveImageRequests = 10;

}

//-----------------------------------------------------------------------------

EventImageDownloadWatcher::EventImageDownloadWatcher(int idempToken, const QUrl &imageUrl, QObject *parent)
    : QObject(parent), m_idempToken(idempToken), m_imageUrl(imageUrl)
{
}

EventImageDownloadWatcher::~EventImageDownloadWatcher()
{
}

int EventImageDownloadWatcher::idempToken() const
{
    return m_idempToken;
}

QUrl EventImageDownloadWatcher::imageUrl() const
{
    return m_imageUrl;
}

//-----------------------------------------------------------------------------

EventImageDownload::EventImageDownload(
        int idempToken,
        const QUrl &imageUrl,
        const QString &filePath,
        const QNetworkRequest &templateRequest,
        EventImageDownloadWatcher *watcher)
    : m_idempToken(idempToken)
    , m_imageUrl(imageUrl)
    , m_filePath(filePath)
    , m_templateRequest(templateRequest)
    , m_watcher(watcher)
{
    m_timeoutTimer = new QTimer;
}

EventImageDownload::~EventImageDownload()
{
    m_timeoutTimer->deleteLater();
    if (m_reply) {
        m_reply->deleteLater();
    }
}

//-----------------------------------------------------------------------------

EventImageDownloader::EventImageDownloader(QObject *parent)
    : QObject(parent)
{
}

EventImageDownloader::~EventImageDownloader()
{
}

EventImageDownloadWatcher *EventImageDownloader::downloadImage(int idempToken,
                                                               const QUrl &imageUrl,
                                                               const QString &filePath,
                                                               const QNetworkRequest &templateRequest)
{
    EventImageDownloadWatcher *watcher = new EventImageDownloadWatcher(idempToken, imageUrl, this);
    EventImageDownload *download = new EventImageDownload(idempToken, imageUrl, filePath, templateRequest, watcher);
    m_pending.enqueue(download);
    QMetaObject::invokeMethod(this, "triggerDownload", Qt::QueuedConnection);
    return watcher; // caller takes ownership.
}

void EventImageDownloader::triggerDownload()
{
    while (m_active.size() && (m_active.head() == nullptr || !m_active.head()->m_timeoutTimer->isActive())) {
        delete m_active.dequeue();
    }

    while (m_active.size() < MaxActiveImageRequests && m_pending.size()) {
        EventImageDownload *download = m_pending.dequeue();
        m_active.enqueue(download);

        connect(download->m_timeoutTimer, &QTimer::timeout, this, [this, download] {
            if (download->m_watcher) {
                emit download->m_watcher->downloadFailed(QStringLiteral("Event Image download timed out"));
            }
            eraseActiveDownload(download);
        });

        download->m_timeoutTimer->setInterval(20 * 1000);
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
                    if (download->m_watcher) {
                        emit download->m_watcher->downloadFailed(QStringLiteral("Event Image download error: %1").arg(reply->error()));
                    }
                } else {
                    // save the file to the appropriate location.
                    const QByteArray replyData = reply->readAll();
                    if (replyData.isEmpty()) {
                        if (download->m_watcher) {
                            emit download->m_watcher->downloadFailed(QStringLiteral("Empty image data received, aborting"));
                        }
                    } else {
                        QDir dir = QFileInfo(download->m_filePath).dir();
                        if (!dir.exists()) {
                            dir.mkpath(QStringLiteral("."));
                        }
                        QSaveFile file(download->m_filePath);
                        if (!file.open(QFile::WriteOnly)) {
                            if (download->m_watcher) {
                                emit download->m_watcher->downloadFailed(QStringLiteral("Error opening event image file %1 for writing: %2")
                                                                                   .arg(download->m_filePath, file.errorString()));
                            }
                        } else {
                            file.write(replyData);
                            if (file.commit()) {
                                emit download->m_watcher->downloadFinished(download->m_filePath);
                            } else {
                                emit download->m_watcher->downloadFailed(QStringLiteral("Error writing event image file %1: %2")
                                                                                   .arg(download->m_filePath, file.errorString()));
                            }
                        }
                    }
                }

                eraseActiveDownload(download);
            });
        }
    }
}

void EventImageDownloader::eraseActiveDownload(EventImageDownload *download)
{
    QQueue<EventImageDownload*>::iterator it = m_active.begin();
    QQueue<EventImageDownload*>::iterator end = m_active.end();
    for ( ; it != end; ++it) {
        if (*it == download) {
            m_active.erase(it);
            break;
        }
    }

    delete download;

    QMetaObject::invokeMethod(this, "triggerDownload", Qt::QueuedConnection);
}
