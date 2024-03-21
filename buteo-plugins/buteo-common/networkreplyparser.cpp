/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "networkreplyparser_p.h"

#include <QDebug>
#include <QList>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

const QString XmlReplyParser::XmlElementTextKey = QStringLiteral("@text");

namespace {

QVariantMap elementToVariantMap(QXmlStreamReader &reader)
{
    QVariantMap element;

    // store the attributes of the element
    QXmlStreamAttributes attrs = reader.attributes();
    while (attrs.size()) {
        QXmlStreamAttribute attr = attrs.takeFirst();
        element.insert(attr.name().toString(), attr.value().toString());
    }

    while (reader.readNext() != QXmlStreamReader::EndElement) {
        if (reader.isCharacters()) {
            // store the text of the element, if any
            QString elementText = reader.text().toString();
            if (!elementText.trimmed().isEmpty()) {
                element.insert(XmlReplyParser::XmlElementTextKey, elementText);
            }
        } else if (reader.isStartElement()) {
            // recurse if necessary.
            QString subElementName = reader.name().toString();
            QVariantMap subElement = elementToVariantMap(reader);
            if (element.contains(subElementName)) {
                // already have an element with this name.
                // create a variantlist and append.
                QVariant existing = element.value(subElementName);
                QVariantList subElementList;
                if (existing.type() == QVariant::Map) {
                    // we need to convert the value into a QVariantList
                    subElementList << existing.toMap();
                } else if (existing.type() == QVariant::List) {
                    subElementList = existing.toList();
                }
                subElementList << subElement;
                element.insert(subElementName, subElementList);
            } else {
                // first element with this name.  insert as a map.
                element.insert(subElementName, subElement);
            }
        }
    }

    return element;
}

bool ocsMetaStatusCodeSucceeded(const QJsonObject &ocs, int *errorCode, QString *errorMessage)
{
    const QJsonObject meta = ocs.value(QLatin1String("meta")).toObject();
    const int code = meta.value(QLatin1String("statuscode")).toInt();
    if (errorCode) {
        *errorCode = code;
    }
    if (code != 200) {
        const QString message = meta.value(QLatin1String("message")).toString();
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }
    return true;
}

}

bool NetworkReplyParser::debugEnabled = false;

void NetworkReplyParser::debugDumpData(const QString &data)
{
    if (!debugEnabled) {
        return;
    }

    QString dbgout;
    Q_FOREACH (const QChar &c, data) {
        if (c == '\r' || c == '\n') {
            if (!dbgout.isEmpty()) {
                qDebug() << dbgout;
                dbgout.clear();
            }
        } else {
            dbgout += c;
        }
    }
    if (!dbgout.isEmpty()) {
        qDebug() << dbgout;
    }
}

//--- JsonReplyParser:

QVariantMap JsonReplyParser::findCapability(const QString &capabilityName, const QByteArray &capabilityResponse)
{
    /* We expect a response of the form:

    {
      "ocs": {
        ...
        "data": {
          ...
          "capabilities": {
            ...
            "notifications": {
              "ocs-endpoints": [
                "list",
                "get",
                "delete",
                "delete-all",
                "icons",
                "rich-strings",
                "action-web"
              ]
            }
          }
        }
      }
    }
    */

    NetworkReplyParser::debugDumpData(QString::fromUtf8(capabilityResponse));

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(capabilityResponse, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parsing failed:" << err.errorString();
        return QVariantMap();
    }

    const QJsonObject ocs = doc.object().value(QLatin1String("ocs")).toObject();
    int errorCode = 0;
    QString errorMessage;
    if (!ocsMetaStatusCodeSucceeded(ocs, &errorCode, &errorMessage)) {
        qWarning() << "Capabilities response has non-success status code:" << errorCode << errorMessage;
        return QVariantMap();
    }

    const QJsonObject data = ocs.value(QLatin1String("data")).toObject();
    if (data.isEmpty()) {
        qWarning() << "No 'data' entry found in capabilities JSON response!";
        return QVariantMap();
    }

    const QJsonObject capabilities = data.value(QLatin1String("capabilities")).toObject();
    if (capabilities.isEmpty()) {
        qWarning() << "No 'capabilities' entry found in capabilities JSON response!";
        return QVariantMap();
    }

    return capabilities.value(capabilityName).toObject().toVariantMap();
}

NetworkReplyParser::User JsonReplyParser::parseUserResponse(const QByteArray &userInfoResponse)
{
    /* Expected result:
    {
        "ocs": {
            "meta": {
                "status": "ok",
                "statuscode": 200,
                "message": "OK"
            },
            "data": {
                "enabled": true,
                "storageLocation": "\\/var\\/www\\/html\\/data\\/MyUserName",
                "id": "MyUserName",
                "lastLogin": 1574123564000,
                "backend": "Database",
                "subadmin": [
                    "group 3"
                ],
                "quota": {
                    "free": 109500956672,
                    "used": 10233453,
                    "total": 109511190125,
                    "relative": 0.01,
                    "quota": -3
                },
                "email": "me@myemail.com",
                "phone": "",
                "address": "",
                "website": "",
                "twitter": "",
                "groups": [
                    "group 1",
                    "group 2",
                    "group 3"
                ],
                "language": "en",
                "locale": "",
                "backendCapabilities": {
                    "setDisplayName": true,
                    "setPassword": true
                },
                "display-name": "My Display Name"
            }
        }
    }
    */

    NetworkReplyParser::debugDumpData(QString::fromUtf8(userInfoResponse));

    QJsonParseError err;
    NetworkReplyParser::User user;

    const QJsonDocument doc = QJsonDocument::fromJson(userInfoResponse, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parsing failed:" << err.errorString();
        return user;
    }

    const QJsonObject ocs = doc.object().value(QLatin1String("ocs")).toObject();
    int errorCode = 0;
    QString errorMessage;
    if (!ocsMetaStatusCodeSucceeded(ocs, &errorCode, &errorMessage)) {
        qWarning() << "user metadata response has non-success status code:" << errorCode << errorMessage;
        return user;
    }

    const QJsonObject data = ocs.value(QLatin1String("data")).toObject();
    user.userId = data.value(QLatin1String("id")).toString();
    user.displayName = data.value(QLatin1String("display-name")).toString();

    // There's an API inconsistency where ocs/v2.php/cloud/user has a "display-name" key while
    // ocs/v2.php/cloud/users/<userid> has "displayname". Check for both just in case.
    if (user.displayName.isEmpty()) {
        user.displayName = data.value(QLatin1String("displayname")).toString();
    }

    return user;
}

QList<NetworkReplyParser::Notification> JsonReplyParser::parseNotificationResponse(const QByteArray &replyData)
{
    /* Expected result:
    {
      "ocs": {
        "meta": {
          "status": "ok",
          "statuscode": 200,
          "message": null
        },
        "data": [
          {
            "notification_id": 61,
            "app": "files_sharing",
            "user": "admin",
            "datetime": "2004-02-12T15:19:21+00:00",
            "object_type": "remote_share",
            "object_id": "13",
            "subject": "You received admin@localhost as a remote share from test",
            "subjectRich": "You received {share} as a remote share from {user}",
            "subjectRichParameters": {
              "share": {
                "type": "pending-federated-share",
                "id": "1",
                "name": "test"
              },
              "user": {
                "type": "user",
                "id": "test1",
                "name": "User One",
                "server": "http:\/\/nextcloud11.local"
              }
            },
            "message": "",
            "messageRich": "",
            "messageRichParameters": [],
            "link": "http://localhost/index.php/apps/files_sharing/pending",
            "icon": "http://localhost/img/icon.svg",
            "actions": [
              {
                "label": "Accept",
                "link": "http:\/\/localhost\/ocs\/v1.php\/apps\/files_sharing\/api\/v1\/remote_shares\/13",
                "type": "POST",
                "primary": true
              },
              {
                "label": "Decline",
                "link": "http:\/\/localhost\/ocs\/v1.php\/apps\/files_sharing\/api\/v1\/remote_shares\/13",
                "type": "DELETE",
                "primary": false
              }
            ]
          }
        ]
      }
    }
    */

    NetworkReplyParser::debugDumpData(QString::fromUtf8(replyData));

    QList<NetworkReplyParser::Notification> notifs;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(replyData, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parsing failed:" << err.errorString();
        return notifs;
    }

    const QJsonObject ocs = doc.object().value(QLatin1String("ocs")).toObject();
    int errorCode = 0;
    QString errorMessage;
    if (!ocsMetaStatusCodeSucceeded(ocs, &errorCode, &errorMessage)) {
        qWarning() << "Notifications response has non-success status code:" << errorCode << errorMessage;
        return notifs;
    }

    const QJsonArray notificationArray = ocs.value(QLatin1String("data")).toArray();
    if (notificationArray.isEmpty()) {
        qWarning() << "No 'data' entry found in notifications JSON response!";
        return notifs;
    }

    for (const QJsonValue &jsonValue : notificationArray) {
        NetworkReplyParser::Notification notif;
        const QJsonObject notifObject = jsonValue.toObject();

        notif.notificationId = QString::number(notifObject.value(QLatin1String("notification_id")).toDouble());
        notif.app = notifObject.value(QLatin1String("app")).toString();
        notif.userId = notifObject.value(QLatin1String("user")).toString();
        notif.dateTime = QDateTime::fromString(notifObject.value(QLatin1String("datetime")).toString(), Qt::ISODate);
        notif.icon = notifObject.value(QLatin1String("icon")).toString();
        notif.link = notifObject.value(QLatin1String("link")).toString();
        notif.actions = notifObject.value(QLatin1String("actions")).toArray().toVariantList();

        notif.objectType = notifObject.value(QLatin1String("object_type")).toString();
        notif.objectId = notifObject.value(QLatin1String("object_id")).toString();

        notif.subject = notifObject.value(QLatin1String("subject")).toString();
        notif.subjectRich = notifObject.value(QLatin1String("subjectRich")).toString();
        notif.subjectRichParameters = notifObject.value(QLatin1String("subjectRichParameters"));

        notif.message = notifObject.value(QLatin1String("message")).toString();
        notif.messageRich = notifObject.value(QLatin1String("messageRich")).toString();
        notif.messageRichParameters = notifObject.value(QLatin1String("messageRichParameters"));

        notifs.append(notif);
    }

    return notifs;
}

//--- XmlReplyParser:

QVariantMap XmlReplyParser::xmlToVariantMap(QXmlStreamReader &reader)
{
    QVariantMap retn;
    while (!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
        QString elementName = reader.name().toString();
        QVariantMap element = elementToVariantMap(reader);
        retn.insert(elementName, element);
    }
    return retn;
}

QList<NetworkReplyParser::Resource> XmlReplyParser::parsePropFindResponse(const QByteArray &propFindResponse)
{
    /* We expect a response of the form:
        <?xml version="1.0" encoding="UTF-8"?>
        <d:multistatus xmlns:d="DAV:" xmlns:nc="http://nextcloud.org/ns" xmlns:oc="http://owncloud.org/ns" xmlns:s="http://sabredav.org/ns">
           <d:response>
              <d:href>/remote.php/dav/files/Username/</d:href>
              <d:propstat>
                 <d:prop>
                    <d:getlastmodified>Tue, 17 Sep 2019 06:39:33 GMT</d:getlastmodified>
                    <d:resourcetype>
                       <d:collection />
                    </d:resourcetype>
                    <d:quota-used-bytes>2505506</d:quota-used-bytes>
                    <d:quota-available-bytes>-3</d:quota-available-bytes>
                    <d:getetag>"5d807fa5bea23"</d:getetag>
                 </d:prop>
                 <d:status>HTTP/1.1 200 OK</d:status>
              </d:propstat>
           </d:response>
           <d:response>
              <d:href>/remote.php/dav/files/Username/Documents/</d:href>
              <d:propstat>
                 <d:prop>
                    <d:getlastmodified>Tue, 17 Sep 2019 06:39:33 GMT</d:getlastmodified>
                    <d:resourcetype>
                       <d:collection />
                    </d:resourcetype>
                    <d:quota-used-bytes>119824</d:quota-used-bytes>
                    <d:quota-available-bytes>-3</d:quota-available-bytes>
                    <d:getetag>"5d807fa5bea23"</d:getetag>
                 </d:prop>
                 <d:status>HTTP/1.1 200 OK</d:status>
              </d:propstat>
           </d:response>
           <d:response>
              <d:href>/remote.php/dav/files/Username/Storage-Share.png</d:href>
              <d:propstat>
                 <d:prop>
                    <d:getlastmodified>Tue, 17 Sep 2019 06:39:33 GMT</d:getlastmodified>
                    <d:getcontentlength>25671</d:getcontentlength>
                    <d:resourcetype />
                    <d:getetag>"2d391d9c53795329f51011d1af60158b"</d:getetag>
                    <d:getcontenttype>image/png</d:getcontenttype>
                 </d:prop>
                 <d:status>HTTP/1.1 200 OK</d:status>
              </d:propstat>
           </d:response>
        </d:multistatus>
    */

    NetworkReplyParser::debugDumpData(QString::fromUtf8(propFindResponse));

    QXmlStreamReader reader(propFindResponse);
    QVariantMap variantMap = XmlReplyParser::xmlToVariantMap(reader);

    QVariantMap multistatusMap = variantMap.value(QStringLiteral("multistatus")).toMap();
    QVariantList responses;

    QVariant response = multistatusMap.value(QStringLiteral("response"));
    if (response.type() == QVariant::List) {
        // multiple directory entries.
        responses = response.toList();
    } else {
        // only one directory entry.
        responses << response.toMap();
    }

    // parse the information about each entry (response element)
    QList<NetworkReplyParser::Resource> result;
    for (const QVariant &rv : responses) {
        const QVariantMap rmap = rv.toMap();
        const QString entryHref = QString::fromUtf8(
                QByteArray::fromPercentEncoding(
                        rmap.value(QStringLiteral("href")).toMap().value(
                                XmlElementTextKey).toString().toUtf8()));

        QVariantList propstats;
        if (rmap.value(QStringLiteral("propstat")).type() == QVariant::List) {
            propstats = rmap.value(QStringLiteral("propstat")).toList();
        } else {
            QVariantMap propstat = rmap.value(QStringLiteral("propstat")).toMap();
            propstats << propstat;
        }

        NetworkReplyParser::Resource resource;
        resource.href = entryHref;

        for (const QVariant &vpropstat : propstats) {
            const QVariantMap propstat = vpropstat.toMap();
            const QVariantMap &prop(propstat.value(QStringLiteral("prop")).toMap());
            for (QVariantMap::ConstIterator it = prop.constBegin(); it != prop.constEnd(); ++it) {
                const QString &key = it.key();

                if (key == QStringLiteral("getlastmodified")) {
                    resource.lastModified = QDateTime::fromString(it.value().toMap().value(XmlElementTextKey).toString(), Qt::RFC2822Date);
                } else if (key == QStringLiteral("getcontenttype")) {
                    resource.contentType = it.value().toMap().value(XmlElementTextKey).toString();
                } else if (key == QStringLiteral("owner-id")) {
                    resource.ownerId = it.value().toMap().value(XmlElementTextKey).toString();
                } else if (key == QStringLiteral("fileid")) {
                    resource.fileId = it.value().toMap().value(XmlElementTextKey).toString();
                } else if (key == QStringLiteral("getetag")) {
                    resource.etag = it.value().toMap().value(XmlElementTextKey).toString();
                } else if (key == QStringLiteral("size")) {
                    resource.size = it.value().toMap().value(XmlElementTextKey).toInt();
                } else if (key == QStringLiteral("resourcetype")) {
                    const QStringList resourceTypeKeys = it.value().toMap().keys();
                    resource.isCollection = resourceTypeKeys.contains(QStringLiteral("collection"), Qt::CaseInsensitive);
                }
            }
        }
        result.append(resource);
    }

    return result;
}
