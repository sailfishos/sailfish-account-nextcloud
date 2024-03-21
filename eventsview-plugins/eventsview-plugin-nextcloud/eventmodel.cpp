/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "eventmodel.h"
#include "synccacheevents.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtQml/QQmlInfo>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

// mlite5
#include <MGConfItem>

NextcloudEventModel::NextcloudEventModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<SyncCache::Event>();
    qRegisterMetaType<QVector<SyncCache::Event> >();
}

NextcloudEventModel::~NextcloudEventModel()
{
}

void NextcloudEventModel::classBegin()
{
    m_deferLoad = true;
}

void NextcloudEventModel::componentComplete()
{
    m_deferLoad = false;
    if (m_eventCache) {
        loadData();
    }
}

QModelIndex NextcloudEventModel::index(int row, int column, const QModelIndex &parent) const
{
    return !parent.isValid() && column == 0 && row >= 0 && row < m_data.size()
            ? createIndex(row, column)
            : QModelIndex();
}

QVariant NextcloudEventModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= m_data.size()) {
        return QVariant();
    }

    // TODO: if image path is requested but empty,
    //       call m_cache->populateEventImagePath(),
    //       and when it succeeds, emit dataChanged(row).
    switch (role) {
        case AccountIdRole:     return m_data[row].accountId;
        case EventIdRole:       return m_data[row].eventId;
        case EventSubjectRole:  return m_data[row].eventSubject;
        case EventTextRole:     return m_data[row].eventText;
        case EventUrlRole:      return m_data[row].eventUrl;
        case ImageUrlRole:      return m_data[row].imageUrl;
        case ImagePathRole:     return m_data[row].imagePath;
        case TimestampRole:     return m_data[row].timestamp;
        case DeletedLocallyRole: return m_data[row].deletedLocally;
        default:                return QVariant();
    }
}

int NextcloudEventModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QHash<int, QByteArray> NextcloudEventModel::roleNames() const
{
    static QHash<int, QByteArray> retn {
        { AccountIdRole,        "accountId" },
        { EventIdRole,          "eventId" },
        { EventSubjectRole,     "eventSubject" },
        { EventTextRole,        "eventText" },
        { EventUrlRole,         "eventUrl" },
        { ImageUrlRole,         "imageUrl" },
        { ImagePathRole,        "imagePath" },
        { TimestampRole,        "timestamp" },
        { DeletedLocallyRole,   "deletedLocally" },
    };

    return retn;
}

SyncCache::EventCache *NextcloudEventModel::eventCache() const
{
    return m_eventCache;
}

void NextcloudEventModel::setEventCache(SyncCache::EventCache *cache)
{
    if (m_eventCache == cache) {
        return;
    }

    if (m_eventCache) {
        disconnect(m_eventCache, 0, this, 0);
    }

    m_eventCache = cache;
    emit eventCacheChanged();

    if (!m_deferLoad) {
        loadData();
    }

    connect(m_eventCache, &SyncCache::EventCache::flagEventForDeletionFailed,
            this, [this] (int accountId, const QString &eventId, const QString &errorMessage) {
        if (accountId == this->accountId()) {
            qmlInfo(this) << "Failed to locally delete event:" << eventId
                          << "for account:" << this->accountId()
                          << ":" << errorMessage;
        }
    });

    connect(m_eventCache, &SyncCache::EventCache::eventsStored,
            this, [this] (const QVector<SyncCache::Event> &events) {
        for (const SyncCache::Event &event : events) {
            bool foundEvent = false;
            for (int row = 0; row < m_data.size(); ++row) {
                const SyncCache::Event &existing(m_data[row]);
                if (event.accountId == existing.accountId
                        && event.eventId == existing.eventId) {
                    m_data.replace(row, event);
                    emit dataChanged(index(row, 0, QModelIndex()), index(row, 0, QModelIndex()));
                    foundEvent = true;
                }
            }

            if (!foundEvent
                    && (event.accountId == accountId() || accountId() == 0)) {
                emit beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
                m_data.append(event);
                emit endInsertRows();
                emit rowCountChanged();
            }
        }
    });

    connect(m_eventCache, &SyncCache::EventCache::eventsDeleted,
            this, [this] (const QVector<SyncCache::Event> &events) {
        for (const SyncCache::Event &event : events) {
            for (int row = 0; row < m_data.size(); ++row) {
                const SyncCache::Event &existing(m_data[row]);
                if (event.accountId == existing.accountId
                        && event.eventId == existing.eventId) {
                    emit beginRemoveRows(QModelIndex(), row, row);
                    m_data.remove(row);
                    emit endRemoveRows();
                    emit rowCountChanged();
                }
            }
        }
    });
}

int NextcloudEventModel::accountId() const
{
    return m_accountId;
}

void NextcloudEventModel::setAccountId(int id)
{
    if (m_accountId == id) {
        return;
    }

    m_accountId = id;
    m_buteoProfileId = QString("nextcloud.Posts-%1").arg(id);
    emit accountIdChanged();

    if (!m_deferLoad) {
        loadData();
    }
}

QVariantMap NextcloudEventModel::at(int row) const
{
    QVariantMap retn;
    if (row < 0 || row >= rowCount()) {
        return retn;
    }

    const QHash<int, QByteArray> roles = roleNames();
    QHash<int, QByteArray>::const_iterator it = roles.constBegin();
    QHash<int, QByteArray>::const_iterator end = roles.constEnd();
    for ( ; it != end; it++) {
        retn.insert(QString::fromLatin1(it.value()), data(index(row, 0, QModelIndex()), it.key()));
    }
    return retn;
}

void NextcloudEventModel::loadData()
{
    if (!m_eventCache) {
        return;
    }

    if (m_accountId <= 0) {
        return;
    }

    if (!m_notifCapabilityConf) {
        m_notifCapabilityConf = new MGConfItem("/sailfish/sync/profiles/" + m_buteoProfileId + "/ocs-endpoints", this);
        connect(m_notifCapabilityConf, &MGConfItem::valueChanged,
                this, &NextcloudEventModel::updateSupportedActions);
    }
    updateSupportedActions();

    QObject *contextObject = new QObject(this);
    connect(m_eventCache, &SyncCache::EventCache::requestEventsFinished,
            contextObject, [this, contextObject] (int accountId,
                                                  const QVector<SyncCache::Event> &events) {
        if (accountId != this->accountId()) {
            return;
        }
        contextObject->deleteLater();
        const int oldSize = m_data.size();
        if (m_data.size()) {
            emit beginRemoveRows(QModelIndex(), 0, m_data.size() - 1);
            m_data.clear();
            emit endRemoveRows();
        }
        if (events.size()) {
            emit beginInsertRows(QModelIndex(), 0, events.size() - 1);
            m_data = events;
            emit endInsertRows();
        }
        if (m_data.size() != oldSize) {
            emit rowCountChanged();
        }
    });
    connect(m_eventCache, &SyncCache::EventCache::requestEventsFailed,
            contextObject, [this, contextObject] (int accountId,
                                                  const QString &errorMessage) {
        if (accountId != this->accountId()) {
            return;
        }
        contextObject->deleteLater();
        qmlInfo(this) << "NextcloudEventModel::loadData: failed:" << errorMessage;
    });
    m_eventCache->requestEvents(m_accountId, false);
}

void NextcloudEventModel::updateSupportedActions()
{
    if (m_accountId <= 0 || !m_notifCapabilityConf) {
        return;
    }

    Actions supportedActions = 0;
    QStringList capabilities = m_notifCapabilityConf->value().toStringList();

    if (capabilities.contains(QStringLiteral("delete"))) {
        supportedActions |= DeleteEvent;
    }
    if (capabilities.contains(QStringLiteral("delete-all"))) {
        supportedActions |= DeleteAllEvents;
    }

    if (supportedActions != m_supportedActions) {
        m_supportedActions = supportedActions;
        emit supportedActionsChanged();
    }
}

// call this periodically to detect changes made to the database by another process.
// this would go away if we had daemonized storage rather than direct-file database access...
void NextcloudEventModel::refresh()
{
    if (!m_eventCache) {
        return;
    }

    if (m_accountId <= 0) {
        return;
    }

    if (m_deferLoad) {
        return;
    }

    QObject *contextObject = new QObject(this);
    connect(m_eventCache, &SyncCache::EventCache::requestEventsFinished,
            contextObject, [this, contextObject] (int accountId,
                                                  const QVector<SyncCache::Event> &events) {
        if (accountId != this->accountId()) {
            return;
        }
        contextObject->deleteLater();

        int row = 0;
        const QVector<SyncCache::Event> oldEvents = m_data;
        QVector<SyncCache::Event>::const_iterator newValuesIter(events.constBegin());
        QVector<SyncCache::Event>::const_iterator oldValuesIter(oldEvents.constBegin());
        while (newValuesIter != events.constEnd() || oldValuesIter != oldEvents.constEnd()) {
            bool bothNeedIncrement = newValuesIter != events.constEnd()
                    && oldValuesIter != oldEvents.constEnd()
                    && newValuesIter->eventId == oldValuesIter->eventId;
            bool newNeedsIncrement =
                    // if they both need incrementing, then newIter needs incrementing
                    bothNeedIncrement ? true
                    // if they are both not at end, and newIter.timestamp > oldIter.timestamp, then newIter needs incrementing
                    : ((newValuesIter != events.constEnd() &&
                        oldValuesIter != oldEvents.constEnd() &&
                        newValuesIter->timestamp > oldValuesIter->timestamp)
                    // if only newIter is not at end, then newIter needs incrementing
                    || (newValuesIter != events.constEnd() &&
                        oldValuesIter == oldEvents.constEnd()));

            if (bothNeedIncrement) {
                // the current event exists in both old + new.
                // this is either a row change, or an unchanged row (identical data)
                QVector<int> changedRoles;
                if (newValuesIter->eventSubject != oldValuesIter->eventSubject) {
                    changedRoles.append(static_cast<int>(EventSubjectRole));
                }
                if (newValuesIter->eventText != oldValuesIter->eventText) {
                    changedRoles.append(static_cast<int>(EventTextRole));
                }
                if (newValuesIter->eventUrl != oldValuesIter->eventUrl) {
                    changedRoles.append(static_cast<int>(EventUrlRole));
                }
                if (!changedRoles.isEmpty()) {
                    m_data.replace(row, *newValuesIter);
                    emit dataChanged(createIndex(row, 0), createIndex(row, 0), changedRoles);
                }
            } else if (newNeedsIncrement) {
                // the current key exists only in new.
                // this is a row addition.
                emit beginInsertRows(QModelIndex(), row, row);
                m_data.insert(row, *newValuesIter);
                emit endInsertRows();
            } else {
                // the current key exists only in old.
                // this is a row removal.
                emit beginRemoveRows(QModelIndex(), row, row);
                m_data.removeAt(row);
                emit endRemoveRows();
            }

            if (bothNeedIncrement) {
                newValuesIter++;
                oldValuesIter++;
                row++; // we changed (or ignored) a value at the previous row.
            } else if (newNeedsIncrement) {
                newValuesIter++;
                row++; // we added a new value at the previous row.
            } else {
                oldValuesIter++;
                // no need to increment or decrement row, as we removed the old value associated with that row number.
            }
        }

        if (oldEvents.size() != m_data.size()) {
            emit rowCountChanged();
        }
    });
    connect(m_eventCache, &SyncCache::EventCache::requestEventsFailed,
            contextObject, [this, contextObject] (int accountId,
                                                  const QString &errorMessage) {
        if (accountId != this->accountId()) {
            return;
        }
        contextObject->deleteLater();
        qmlInfo(this) << "NextcloudEventModel::refresh: failed:" << errorMessage;
    });
    m_eventCache->requestEvents(m_accountId, false);
}

NextcloudEventModel::Actions NextcloudEventModel::supportedActions() const
{
    return m_supportedActions;
}

void NextcloudEventModel::deleteEventAt(int row)
{
    if (row < 0 || row >= m_data.count()) {
        return;
    }
    if (!(m_supportedActions & DeleteEvent)) {
        qmlInfo(this) << "Notification delete action is not supported!";
        return;
    }

    m_notificationsToDelete.insert(m_data[row].eventId);
    startNotificationDeleteTimer();
}

void NextcloudEventModel::deleteAllEvents()
{
    if (!(m_supportedActions & DeleteAllEvents)) {
        qmlInfo(this) << "Notification delete-all action is not supported!";
        return;
    }

    if (m_data.count() > 0) {
        for (int i = 0; i < m_data.count(); ++i) {
            m_notificationsToDelete.insert(m_data[i].eventId);
        }
        startNotificationDeleteTimer();
    }
}

void NextcloudEventModel::startNotificationDeleteTimer()
{
    // Batch notification deletions to avoid unnecessary sync requests.
    if (!m_notificationDeleteTimer) {
        m_notificationDeleteTimer = new QTimer(this);
        m_notificationDeleteTimer->setSingleShot(true);
        connect(m_notificationDeleteTimer, &QTimer::timeout,
                this, &NextcloudEventModel::notificationDeleteTimeout);
        m_notificationDeleteTimer->setInterval(10 * 1000);
    }
    m_notificationDeleteTimer->start();
}

void NextcloudEventModel::notificationDeleteTimeout()
{
    if (m_notificationsToDelete.isEmpty()) {
        return;
    }

    const QStringList notificationIds = m_notificationsToDelete.toList();
    for (const QString &notificationId : notificationIds) {
        m_eventCache->flagEventForDeletion(m_accountId, notificationId);
    }

    if (!m_buteoInterface) {
        m_buteoInterface = new QDBusInterface("com.meego.msyncd",
                                              "/synchronizer",
                                              "com.meego.msyncd",
                                              QDBusConnection::sessionBus(),
                                              this);
    }

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
                m_buteoInterface->asyncCall(QStringLiteral("startSync"), m_buteoProfileId), this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this] (QDBusPendingCallWatcher *call) {
        QDBusPendingReply<bool> reply = *call;
        if (reply.isError()) {
            qWarning() << "Failed to request buteo to delete notifications for profile:"
                       << this->m_buteoProfileId
                       << reply.error().name()
                       << reply.error().message();
        }
       call->deleteLater();
    });

    m_notificationsToDelete.clear();
}
