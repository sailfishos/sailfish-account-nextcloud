/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUDAPI_H
#define NEXTCLOUDAPI_H

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

class NextcloudApi : public QObject
{
    Q_OBJECT

public:
    enum API_CALL {
        NONE,
        REQUEST_FOLDER,
        CREATE_FOLDER,
        UPLOAD_FILE,
    };

    NextcloudApi(QNetworkAccessManager *qnam, QObject *parent = 0);
    ~NextcloudApi();

    void propfindPath(const QString &accessToken,
                      const QString &username,
                      const QString &password,
                      const QString &serverAddress,
                      const QString &uploadPath);

    void uploadFile(const QString &filePath,
                    const QString &accessToken,
                    const QString &username,
                    const QString &password,
                    const QString &serverAddress,
                    const QString &uploadPath,
                    bool ignoreSslErrors = false);

    void cancelUpload();

Q_SIGNALS:
    void propfindFinished();
    void transferProgressUpdated(qreal progress);
    void transferFinished();
    void transferError();
    void transferCanceled();
    void credentialsExpired();

private Q_SLOTS:
    void replyError(QNetworkReply::NetworkError error);
    void replyErrorTimerExpired();
    void finished();
    void networkError(QNetworkReply::NetworkError error);
    void uploadProgress(qint64 received, qint64 total);
    void timedOut();

private:
    void finishTransfer(QNetworkReply::NetworkError error, int httpCode, const QByteArray &data);
    void clearPendingErrors();
    QMap<QNetworkReply*, API_CALL> m_replies;
    QMap<QTimer*, QNetworkReply*> m_timeouts;
    QNetworkAccessManager *m_qnam;
    QTimer m_replyErrorTimer;
    QNetworkReply::NetworkError m_error;
};

#endif // NEXTCLOUDAPI_H
