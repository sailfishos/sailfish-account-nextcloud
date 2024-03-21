/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "nextcloudapi.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QUrlQuery>
#include <QtCore/QBuffer>

#include <QtCore/QMimeDatabase>
#include <QtCore/QMimeData>

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>

#include <QAuthenticator>
#include <QNetworkProxy>

#include <QtDebug>

static const int API_ERROR_TIMER_TIMEOUT = 5000;

NextcloudApi::NextcloudApi(QNetworkAccessManager *qnam, QObject *parent)
    : QObject(parent)
    , m_qnam(qnam)
    , m_error(QNetworkReply::NoError)
{
    connect(&m_replyErrorTimer, &QTimer::timeout, this, &NextcloudApi::replyErrorTimerExpired);
    m_replyErrorTimer.setSingleShot(true);
}

NextcloudApi::~NextcloudApi()
{
}

// This method exists to work around an issue which QNetworkAccessManager
// sometimes hits with nginx proxy configuration when SSL is enabled,
// resulting in auth request being interpreted as unexpected end-of-file,
// which eventually causes the connection to be aborted with error reported
// as RemoteHostClosedError.
// By performing the propfind first, the authentication cache is pre-filled
// with appropriate credentials which are then re-used for the following
// upload.
void NextcloudApi::propfindPath(const QString &accessToken,
                                const QString &username,
                                const QString &password,
                                const QString &serverAddress,
                                const QString &uploadPath)
{
    if (accessToken.isEmpty() && (username.isEmpty() || password.isEmpty())) {
        emit this->propfindFinished();
        return;
    }

    if (serverAddress.isEmpty() || uploadPath.isEmpty()) {
        emit this->propfindFinished();
        return;
    }

    const QByteArray requestData = QByteArrayLiteral(
        "<d:propfind xmlns:d=\"DAV:\">"
          "<d:propname/>"
        "</d:propfind>");

    const QString remotePath = QStringLiteral("%1%2")
            .arg(uploadPath,
                 accessToken.isEmpty() ? QString() : QStringLiteral("?access_token=%1").arg(accessToken));
    QUrl url(serverAddress);
    url.setPath(remotePath);
    if (accessToken.isEmpty() && !username.isEmpty()) {
        url.setUserName(username);
        url.setPassword(password);
    }

    QNetworkRequest request(url);
    if (!accessToken.isEmpty()) {
        request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                             QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
    }
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/xml; charset=utf-8"));
    request.setHeader(QNetworkRequest::ContentLengthHeader,
                      requestData.length());
    request.setRawHeader(QByteArrayLiteral("Depth"), QByteArrayLiteral("1"));

    QBuffer *buffer = new QBuffer(this);
    buffer->setData(requestData);
    QNetworkReply *propfindReply = m_qnam->sendCustomRequest(request, QByteArrayLiteral("PROPFIND"), buffer);

    if (!propfindReply) {
        delete buffer;
        emit this->propfindFinished();
    } else {
        buffer->setParent(propfindReply);
        connect(propfindReply, &QNetworkReply::finished, [this, propfindReply] {
            if (propfindReply->error() != QNetworkReply::NoError) {
                qWarning() << "Propfind request failed prior to upload:" << propfindReply->error();
            }
            emit this->propfindFinished();
            propfindReply->deleteLater();
        });
    }
}

void NextcloudApi::uploadFile(const QString &filePath,
                              const QString &accessToken,
                              const QString &username,
                              const QString &password,
                              const QString &serverAddress,
                              const QString &uploadPath,
                              bool ignoreSslErrors)
{
    if (accessToken.isEmpty() && (username.isEmpty() || password.isEmpty())) {
        qWarning() << Q_FUNC_INFO << "no access token or username/password";
        emit transferError();
        return;
    }

    if (serverAddress.isEmpty() || uploadPath.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "no server address or upload path";
        emit transferError();
        return;
    }

    QFile f(filePath, this);
    if (filePath.isEmpty() || !f.open(QIODevice::ReadOnly)) {
        qWarning() << Q_FUNC_INFO << "error opening file: " << filePath;
        emit transferError();
        return;
    }
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForData(&f);
    const QString mimeTypeName = mimeType.name();
    const QByteArray imageData = f.readAll();
    f.close();

    const QString fileName = QFileInfo(filePath).fileName();
    const QString remotePath = QStringLiteral("%1%2%3")
            .arg(uploadPath, fileName,
                 accessToken.isEmpty() ? QString() : QStringLiteral("?access_token=%1").arg(accessToken));

    QUrl url(serverAddress);
    url.setPath(remotePath);
    if (accessToken.isEmpty() && !username.isEmpty()) {
        url.setUserName(username);
        url.setPassword(password);
    }

    QNetworkRequest request(url);
    if (!mimeTypeName.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, mimeTypeName.toUtf8());
    }
    request.setHeader(QNetworkRequest::ContentLengthHeader, imageData.size());
    if (!accessToken.isEmpty()) {
        request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                             QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());
    }

    QNetworkReply *reply = m_qnam->put(request, imageData);
    if (!reply) {
        qWarning() << Q_FUNC_INFO << "Unable to post upload file request";
        emit transferError();
        return;
    }

    QTimer *timer = new QTimer(this);
    timer->setInterval(60000);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &NextcloudApi::timedOut);
    timer->start();
    m_timeouts.insert(timer, reply);
    reply->setProperty("timeoutTimer", QVariant::fromValue<QTimer*>(timer));

    m_replies.insert(reply, UPLOAD_FILE);
    connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
            this, &NextcloudApi::replyError);
    connect(reply, &QNetworkReply::uploadProgress, this, &NextcloudApi::uploadProgress);
    connect(reply, &QNetworkReply::finished, this, &NextcloudApi::finished);
    connect(reply, &QNetworkReply::sslErrors, [this, reply, ignoreSslErrors] (const QList<QSslError> &errors) {
        if (ignoreSslErrors) {
            reply->ignoreSslErrors(errors);
        }
    });
}

void NextcloudApi::cancelUpload()
{
    if (m_replies.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "can't cancel upload.";
        return;
    }

    QList<QNetworkReply*> replies = m_replies.keys();
    foreach (QNetworkReply *reply, replies) {
        reply->abort();
    }
    m_replies.clear();
}

void NextcloudApi::finished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_replies.contains(reply)) {
        return;
    }

    const int apiCall = m_replies.take(reply);
    const QByteArray data = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    clearPendingErrors();
    reply->deleteLater();

    QTimer *timer = reply->property("timeoutTimer").value<QTimer*>();
    if (timer) {
        m_timeouts.remove(timer);
        timer->stop();
        timer->deleteLater();
    }

    switch (apiCall) {
    case UPLOAD_FILE:
        // Uploaded, we're done.
        break;
    default:
        qWarning() << Q_FUNC_INFO << " unknown Nextcloud API call: " << apiCall;
        emit transferError();
        return;
    }

    finishTransfer(reply->error(), httpCode, data);
}

void NextcloudApi::finishTransfer(QNetworkReply::NetworkError error, int httpCode, const QByteArray &data)
{
    if (error != QNetworkReply::NoError) {
        if (error == QNetworkReply::OperationCanceledError) {
            emit transferCanceled();
            return;
        }

        qWarning() << Q_FUNC_INFO << error << "httpCode:" << httpCode << "data:" << data;
        emit transferError();
    } else {
        // Everything ok
        emit transferFinished();
    }
}

void NextcloudApi::clearPendingErrors()
{
    m_replyErrorTimer.stop();
    m_error = QNetworkReply::NoError;
}

void NextcloudApi::replyErrorTimerExpired()
{
    finishTransfer(m_error, 0, QByteArray());
    m_error = QNetworkReply::NoError;
}

void NextcloudApi::replyError(QNetworkReply::NetworkError error)
{
    // The error handling is done in QNetworkAccessmanager::finished(). However,
    // Qt documentation says that in some cases it is possible that finished()
    // isn't called after QNetworkReply::error() signal. If we haven't heard
    // anything in reasonable amount of time, we expect that happened and
    // handle the error independently.
    m_error = error;
    m_replyErrorTimer.start(API_ERROR_TIMER_TIMEOUT);
}

void NextcloudApi::networkError(QNetworkReply::NetworkError error)
{
    int httpStatus = qobject_cast<QNetworkReply*>(sender())->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qWarning() << Q_FUNC_INFO << " network error: " << error << "  http status code: " << httpStatus;
    emit transferError();
}

void NextcloudApi::uploadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        emit transferProgressUpdated(received / (qreal)total);
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            QTimer *timer = reply->property("timeoutTimer").value<QTimer*>();
            if (timer) {
                // restart the 60 second timer when there is progress
                timer->start();
            }
        }
    }
}

void NextcloudApi::timedOut()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    if (timer) {
        QNetworkReply *reply = m_timeouts.take(timer);
        if (reply) {
            reply->deleteLater();
            timer->deleteLater();
            m_replies.take(reply);
            qWarning() << Q_FUNC_INFO << "File transfer request timed out";
            emit transferError();
        }
    }
}
