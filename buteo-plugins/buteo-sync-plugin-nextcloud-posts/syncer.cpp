/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "syncer_p.h"

#include "networkrequestgenerator_p.h"
#include "networkreplyparser_p.h"
#include "logging.h"

#include "synccacheevents.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QStandardPaths>

// mlite5
#include <MGConfItem>

// buteo
#include <SyncProfile.h>

namespace {
    const int HTTP_UNAUTHORIZED_ACCESS = 401;

    const QString NotificationsEndpointsKey = QStringLiteral("ocs-endpoints");
}

Syncer::Syncer(QObject *parent, Buteo::SyncProfile *syncProfile)
    : WebDavSyncer(parent, syncProfile, QStringLiteral("nextcloud-posts"))
{
}

Syncer::~Syncer()
{
}

void Syncer::beginSync()
{
    if (!performCapabilitiesRequest()) {
        finishWithError("Capabilities request failed");
        return;
    }
}

bool Syncer::performCapabilitiesRequest()
{
    QNetworkReply *reply = m_requestGenerator->capabilities(NetworkRequestGenerator::JsonContentType);
    if (reply) {
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleCapabilitiesReply);
        return true;
    }

    return false;
}

void Syncer::handleCapabilitiesReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const QByteArray replyData = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        finishWithHttpError("Capabilities request failed", httpCode);
        return;
    }

    const QVariantMap capabilityMap = JsonReplyParser::findCapability(QStringLiteral("notifications"), replyData);
    if (capabilityMap.isEmpty()) {
        finishWithError("Server does not support Notifications app!");
        return;
    }

    const QStringList ocsEndPointsList = capabilityMap.value(NotificationsEndpointsKey).toStringList();
    m_deleteAllNotifsSupported = ocsEndPointsList.contains(QStringLiteral("delete-all"));

    MGConfItem capabilityConf("/sailfish/sync/profiles/" + m_syncProfile->name() + "/" + NotificationsEndpointsKey);
    if (capabilityConf.value() != ocsEndPointsList) {
        capabilityConf.set(ocsEndPointsList);
    }

    if (!performNotificationListRequest()) {
        finishWithError("Notification list request failed");
        return;
    }
}

bool Syncer::performNotificationListRequest()
{
    QNetworkReply *reply = m_requestGenerator->notificationList(NetworkRequestGenerator::JsonContentType);
    if (reply) {
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleNotificationListReply);
        return true;
    }

    return false;
}

void Syncer::handleNotificationListReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();
    const QByteArray replyData = reply->readAll();
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        finishWithHttpError("Notifications request failed: " + reply->error(), httpCode);
        return;
    }

    SyncCache::EventDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Posts/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        emit finishWithError(QStringLiteral("Failed to open Posts database: %1: %2").arg(error.errorCode).arg(error.errorMessage));
        return;
    }

    if (!db.beginTransaction(&error)) {
        emit finishWithError(QStringLiteral("Failed to begin Posts transaction: %1: %2").arg(error.errorCode).arg(error.errorMessage));
        return;
    }

    bool transactionsSucceeded = true;

    // Find local events
    const QVector<SyncCache::Event> localEvents = db.events(m_accountId, &error);
    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        transactionsSucceeded = false;
        qCWarning(lcNextcloud) << "Unable to fetch stored events:" << error.errorCode << error.errorMessage;
    }

    QList<NetworkReplyParser::Notification> remoteEvents;
    QSet<QString> locallyDeletedEventIds;
    QSet<QString> remoteEventIds;

    if (transactionsSucceeded) {
        // Find remote events
        remoteEvents = JsonReplyParser::parseNotificationResponse(replyData);
        for (const NetworkReplyParser::Notification &notif : remoteEvents) {
            remoteEventIds.insert(notif.notificationId);
        }

        for (const SyncCache::Event &event : localEvents) {
            // Find events that are locally marked for deletion
            if (event.deletedLocally) {
                qCDebug(lcNextcloud) << "Event deleted locally:" << event.eventId << event.eventSubject << event.eventText;
                locallyDeletedEventIds.insert(event.eventId);
            }

            // Delete local events that were deleted remotely
            if (!remoteEventIds.contains(event.eventId)) {
                qCDebug(lcNextcloud) << "Event deleted remotely:" << event.eventId << event.eventSubject << event.eventText;
                db.deleteEvent(event.accountId, event.eventId, &error);
                if (error.errorCode != SyncCache::DatabaseError::NoError) {
                    transactionsSucceeded = false;
                    qCWarning(lcNextcloud) << "Failed to delete event" << event.eventId << ":"
                                << error.errorCode << error.errorMessage;
                    break;
                }
            }
        }
    }

    const bool deleteAllRemoteEvents = !locallyDeletedEventIds.isEmpty()
            && locallyDeletedEventIds == remoteEventIds;
    if (!deleteAllRemoteEvents && transactionsSucceeded) {
        // Store all events found remotely, except for those already deleted locally.
        for (const NetworkReplyParser::Notification &notif : remoteEvents) {
            if (locallyDeletedEventIds.contains(notif.notificationId)) {
                continue;
            }

            SyncCache::Event event;
            event.accountId = m_accountId;
            event.eventId = notif.notificationId;
            event.eventSubject = notif.subject;
            event.eventText = notif.message;
            event.eventUrl = notif.link;
            event.imageUrl = notif.icon;
            event.timestamp = notif.dateTime;

            db.storeEvent(event, &error);
            qCDebug(lcNextcloud) << "Adding event:" << event.eventId << event.eventSubject << event.eventText;

            if (error.errorCode != SyncCache::DatabaseError::NoError) {
                transactionsSucceeded = false;
                qCWarning(lcNextcloud) << "Failed to store event" << event.eventId << ":"
                            << error.errorCode << error.errorMessage;
                break;
            }
        }
    }

    bool commitSucceeded = transactionsSucceeded && db.commitTransaction(&error);
    if (!commitSucceeded) {
        SyncCache::DatabaseError rollbackError;
        db.rollbackTransaction(&rollbackError);
    }

    if (commitSucceeded) {
        // Request server to remove any notifications that have been deleted locally
        if (deleteAllRemoteEvents && m_deleteAllNotifsSupported) {
            if (!performNotificationDeleteAllRequest()) {
                finishWithError("Notifications delete request failed");
            }
        } else if (!locallyDeletedEventIds.isEmpty()) {
            if (!performNotificationDeleteRequest(locallyDeletedEventIds.toList())) {
                finishWithError("Notifications delete-all request failed");
            }
        } else {
            finishWithSuccess();
        }

    } else if (transactionsSucceeded) {
        emit finishWithError(QStringLiteral("Failed to commit Posts transaction: %1: %2").arg(error.errorCode).arg(error.errorMessage));
    } else {
        emit finishWithError(QStringLiteral("Failed to store Posts: %1: %2").arg(error.errorCode).arg(error.errorMessage));
    }
}

bool Syncer::performNotificationDeleteRequest(const QStringList &notificationIds)
{
    m_currentDeleteNotificationIds.clear();

    for (const QString &notificationId : notificationIds) {
        QNetworkReply *reply = m_requestGenerator->deleteNotification(notificationId);
        if (reply) {
            reply->setProperty("notificationId", notificationId);
            m_currentDeleteNotificationIds.insert(notificationId);
            connect(reply, &QNetworkReply::finished,
                    this, &Syncer::handleNotificationDeleteReply);
        } else {
            qCWarning(lcNextcloud) << "Failed to start request to delete notification:" << reply->error();
        }
    }

    return true;
}

void Syncer::handleNotificationDeleteReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finishWithHttpError("Notifications delete request failed: " + reply->error(), httpCode);
        return;
    }

    m_currentDeleteNotificationIds.remove(reply->property("notificationId").toString());
    if (m_currentDeleteNotificationIds.isEmpty()) {
        finishWithSuccess();
    }
}

bool Syncer::performNotificationDeleteAllRequest()
{
    QNetworkReply *reply = m_requestGenerator->deleteAllNotifications();
    if (reply) {
        connect(reply, &QNetworkReply::finished,
                this, &Syncer::handleNotificationDeleteAllReply);
    }

    return true;
}

void Syncer::handleNotificationDeleteAllReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finishWithHttpError("Notifications delete-all request failed: " + reply->error(), httpCode);
        return;
    }

    finishWithSuccess();
}

void Syncer::purgeAccount(int accountId)
{
    SyncCache::EventDatabase db;
    SyncCache::DatabaseError error;
    db.openDatabase(
            QStringLiteral("%1/system/privileged/Posts/nextcloud.db").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)),
            &error);

    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        emit finishWithError(QStringLiteral("Failed to open Posts database: %1: %2").arg(error.errorCode).arg(error.errorMessage));
        return;
    }

    if (!db.beginTransaction(&error)) {
        emit finishWithError(QStringLiteral("Failed to begin Posts transaction: %1: %2").arg(error.errorCode).arg(error.errorMessage));
        return;
    }

    bool deleteSucceeded = true;
    const QVector<SyncCache::Event> events = db.events(accountId, &error);
    if (error.errorCode != SyncCache::DatabaseError::NoError) {
        qCWarning(lcNextcloud) << "Failed to query Posts for purge:" << error.errorCode << error.errorMessage;
        db.rollbackTransaction(&error); // TODO: log the fact that we need to purge this one later.
        return;
    }

    Q_FOREACH (const SyncCache::Event &event, events) {
        db.deleteEvent(event.accountId, event.eventId, &error);
        if (error.errorCode != SyncCache::DatabaseError::NoError) {
            deleteSucceeded = false;
            break;
        }
    }

    const bool commitSucceeded = deleteSucceeded && db.commitTransaction(&error);
    if (!commitSucceeded) {
        SyncCache::DatabaseError rollbackError;
        db.rollbackTransaction(&rollbackError); // TODO: log the fact that we need to purge this one later.
    }

    if (commitSucceeded) {
        qCDebug(lcNextcloud) << "Successfully purged posts for deleted account" << accountId;
    } else if (deleteSucceeded) {
        qCWarning(lcNextcloud) << "Failed to commit purge Posts transaction:" << error.errorCode << error.errorMessage;
    } else {
        qCWarning(lcNextcloud) << "Failed to purge Posts:" << error.errorCode << error.errorMessage;
    }
}
