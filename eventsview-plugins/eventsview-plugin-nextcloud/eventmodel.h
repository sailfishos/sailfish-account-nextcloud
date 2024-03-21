/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_EVENTSVIEW_EVENTMODEL_H
#define NEXTCLOUD_EVENTSVIEW_EVENTMODEL_H

#include "synccacheevents.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QPair>
#include <QtQml/QQmlParserStatus>

class QTimer;
class MGConfItem;
class QDBusInterface;

class NextcloudEventModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(SyncCache::EventCache* eventCache READ eventCache WRITE setEventCache NOTIFY eventCacheChanged)
    Q_PROPERTY(int accountId READ accountId WRITE setAccountId NOTIFY accountIdChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(Actions supportedActions READ supportedActions NOTIFY supportedActionsChanged)

public:
    enum Action {
        DeleteEvent = 0x01,
        DeleteAllEvents = 0x02
    };
    Q_ENUM(Action)
    Q_DECLARE_FLAGS(Actions, Action)
    Q_FLAG(Actions)

    explicit NextcloudEventModel(QObject *parent = nullptr);
    ~NextcloudEventModel();

    // QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    // QQmlAbstractListModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        EventIdRole,
        EventSubjectRole,
        EventTextRole,
        EventUrlRole,
        ImageUrlRole,
        ImagePathRole,
        TimestampRole,
        DeletedLocallyRole,
    };
    Q_ENUM(Roles)

    SyncCache::EventCache *eventCache() const;
    void setEventCache(SyncCache::EventCache *cache);

    int accountId() const;
    void setAccountId(int id);

    Actions supportedActions() const;

    Q_INVOKABLE QVariantMap at(int row) const;
    Q_INVOKABLE void refresh();

    Q_INVOKABLE void deleteEventAt(int row);
    Q_INVOKABLE void deleteAllEvents();

Q_SIGNALS:
    void eventCacheChanged();
    void accountIdChanged();
    void rowCountChanged();
    void supportedActionsChanged();

private:
    void loadData();
    void initRequests();
    void updateSupportedActions();

    void startNotificationDeleteTimer();
    void notificationDeleteTimeout();

    bool m_deferLoad = false;
    int m_accountId = 0;
    NextcloudEventModel::Actions m_supportedActions = 0;
    QVector<SyncCache::Event> m_data;
    SyncCache::EventCache *m_eventCache = nullptr;

    QTimer *m_notificationDeleteTimer = nullptr;
    QSet<QString> m_notificationsToDelete;

    MGConfItem *m_notifCapabilityConf = nullptr;
    QDBusInterface *m_buteoInterface = nullptr;
    QString m_buteoProfileId;
};

#endif // NEXTCLOUD_EVENTSVIEW_EVENTMODEL_H
