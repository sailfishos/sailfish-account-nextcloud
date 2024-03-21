/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHEEVENTDOWNLOADS_P_H
#define NEXTCLOUD_SYNCCACHEEVENTDOWNLOADS_P_H

#include <QtCore/QPointer>
#include <QtCore/QQueue>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace SyncCache {

class EventImageDownloadWatcher : public QObject
{
    Q_OBJECT

public:
    EventImageDownloadWatcher(int idempToken, const QUrl &imageUrl, QObject *parent = nullptr);
    ~EventImageDownloadWatcher();

    int idempToken() const;
    QUrl imageUrl() const;

Q_SIGNALS:
    void downloadFailed(const QString &errorMessage);
    void downloadFinished(const QUrl &filePath);

private:
    int m_idempToken;
    QUrl m_imageUrl;
};

class EventImageDownload
{
public:
    EventImageDownload(int idempToken,
            const QUrl &imageUrl,
            const QString &filePath,
            const QNetworkRequest &templateRequest,
            EventImageDownloadWatcher *watcher);
    ~EventImageDownload();

    int m_idempToken = 0;
    QUrl m_imageUrl;
    QString m_filePath;
    QNetworkRequest m_templateRequest;
    QTimer *m_timeoutTimer = nullptr;
    QNetworkReply *m_reply = nullptr;
    QPointer<SyncCache::EventImageDownloadWatcher> m_watcher;
};

class EventImageDownloader : public QObject
{
    Q_OBJECT

public:
    EventImageDownloader(QObject *parent = nullptr);
    ~EventImageDownloader();

    EventImageDownloadWatcher *downloadImage(int idempToken,
            const QUrl &imageUrl,
            const QString &filePath,
            const QNetworkRequest &requestTemplate);

private Q_SLOTS:
    void triggerDownload();

private:
    void eraseActiveDownload(EventImageDownload *download);

    QNetworkAccessManager m_qnam;
    QQueue<EventImageDownload*> m_pending;
    QQueue<EventImageDownload*> m_active;
};

}

#endif
