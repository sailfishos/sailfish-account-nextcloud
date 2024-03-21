/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_NETWORKREPLYPARSER_P_H
#define NEXTCLOUD_NETWORKREPLYPARSER_P_H

#include <QVariantMap>
#include <QDateTime>
#include <QUrl>

class QXmlStreamReader;

class NetworkReplyParser
{
public:
    class User
    {
    public:
        QString userId;
        QString displayName;
    };

    class Resource
    {
    public:
        QDateTime lastModified;
        QString href;
        QString contentType;
        QString ownerId;
        QString fileId;
        QString etag;
        int size = 0;
        bool isCollection = false;
    };

    class Notification
    {
    public:
        QString notificationId;
        QString app;
        QString userId;
        QDateTime dateTime;
        QString icon;
        QString link;
        QVariantList actions;

        QString objectType;
        QString objectId;

        QString subject;
        QString subjectRich;
        QVariant subjectRichParameters;

        QString message;
        QString messageRich;
        QVariant messageRichParameters;
    };

    static void debugDumpData(const QString &data);

    static bool debugEnabled;
};

class XmlReplyParser
{
public:
    static QVariantMap xmlToVariantMap(QXmlStreamReader &reader);
    static QList<NetworkReplyParser::Resource> parsePropFindResponse(const QByteArray &propFindResponse);

    static const QString XmlElementTextKey;
};

class JsonReplyParser
{
public:
    static QVariantMap findCapability(const QString &capabilityName, const QByteArray &capabilityResponse);
    static NetworkReplyParser::User parseUserResponse(const QByteArray &userInfoResponse);
    static QList<NetworkReplyParser::Notification> parseNotificationResponse(const QByteArray &replyData);
};

#endif // NEXTCLOUD_NETWORKREPLYPARSER_P_H


