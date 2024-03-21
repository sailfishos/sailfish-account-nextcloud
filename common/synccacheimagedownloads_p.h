/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHEIMAGEDOWNLOADS_P_H
#define NEXTCLOUD_SYNCCACHEIMAGEDOWNLOADS_P_H

#include <QtCore/QQueue>
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace SyncCache {

QString imageDownloadDir(int accountId);
QString userImageDownloadDir(int accountId, const QString &userId, bool thumbnail);
QString albumImageDownloadDir(int accountId, const QString &albumName, bool thumbnail);


class ImageDownloadWatcher : public QObject
{
    Q_OBJECT

public:
    ImageDownloadWatcher(int idempToken, QObject *parent = nullptr);
    ~ImageDownloadWatcher();

    int idempToken() const;

Q_SIGNALS:
    void downloadFailed(const QString &errorMessage);
    void downloadFinished(const QUrl &filePath);

private:
    int m_idempToken;
};

class ImageDownload
{
public:
    enum Status {
        InProgress,
        Downloaded,
        Error
    };
    ImageDownload(int idempToken = 0,
            const QUrl &imageUrl = QUrl(),
            const QString &fileName = QString(),
            const QString &fileDirPath = QString(),
            const QNetworkRequest &templateRequest = QNetworkRequest(QUrl()),
            ImageDownloadWatcher *watcher = nullptr);
    ~ImageDownload();

    void setStatus(Status status, const QString &error = QString());

    Status m_status = InProgress;
    int m_idempToken = 0;
    QUrl m_imageUrl;
    QString m_fileName;
    QString m_fileDirPath;
    QString m_errorString;
    QNetworkRequest m_templateRequest;
    QTimer *m_timeoutTimer = nullptr;
    QNetworkReply *m_reply = nullptr;
    QPointer<SyncCache::ImageDownloadWatcher> m_watcher;
};

class ImageDownloader : public QObject
{
    Q_OBJECT

public:
    ImageDownloader(QObject *parent = nullptr);
    ~ImageDownloader();

    ImageDownloadWatcher *downloadImage(int idempToken,
            const QUrl &imageUrl,
            const QString &fileName,
            const QString &fileDirPath,
            const QNetworkRequest &templateRequest);

private Q_SLOTS:
    void triggerDownload();
    void eraseInactiveDownloads();

private:
    QNetworkAccessManager m_qnam;
    QQueue<ImageDownload*> m_pending;
    QQueue<ImageDownload*> m_active;
    int m_maxActive;
};

}

#endif
