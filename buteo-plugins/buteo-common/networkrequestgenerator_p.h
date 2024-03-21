/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_NETWORKREQUESTGENERATOR_P_H
#define NEXTCLOUD_NETWORKREQUESTGENERATOR_P_H

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QUrlQuery>

class NetworkRequestGenerator
{
public:
    NetworkRequestGenerator(QNetworkAccessManager *networkAccessManager, const QString &serverUrl, const QString &username, const QString &password);
    NetworkRequestGenerator(QNetworkAccessManager *networkAccessManager, const QString &serverUrl, const QString &accessToken);

    QNetworkReply *userInfo(const QByteArray &acceptContentType);
    QNetworkReply *capabilities(const QByteArray &acceptContentType);

    QNetworkReply *notificationList(const QByteArray &acceptContentType);
    QNetworkReply *deleteNotification(const QString &notificationId);
    QNetworkReply *deleteAllNotifications();

    QNetworkReply *dirListing(const QString &remoteDirPath);
    QNetworkReply *dirCreation(const QString &remoteDirPath);
    QNetworkReply *upload(const QString &dataContentType, const QByteArray &data, const QString &remoteDirPath);
    QNetworkReply *download(const QString &remoteFilePath);

    static bool debugEnabled;

    static const QByteArray XmlContentType;
    static const QByteArray JsonContentType;

private:
    QNetworkRequest networkRequest(const QString &path,
                                   const QString &contentType = QString(),
                                   const QByteArray &requestData = QByteArray()) const;
    QUrl networkRequestUrl(const QString &path);
    QNetworkReply *sendRequest(const QNetworkRequest &request, const QByteArray &requestType, const QByteArray &requestData = QByteArray()) const;

    QString m_username;
    QString m_password;
    QString m_accessToken;
    QUrl m_serverUrl;
    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

#endif // NEXTCLOUD_NETWORKREQUESTGENERATOR_P_H
