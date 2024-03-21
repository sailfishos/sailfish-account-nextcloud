/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHEEVENTS_H
#define NEXTCLOUD_SYNCCACHEEVENTS_H

#include "synccachedatabase.h"

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkRequest>

namespace SyncCache {

class ProcessMutex;

struct Event {
    int accountId = 0;
    QString eventId;
    QString eventSubject;
    QString eventText;
    QUrl eventUrl;
    QUrl imageUrl;
    QUrl imagePath;
    bool deletedLocally = false;
    QDateTime timestamp;
};

class EventDatabase : public Database
{
    Q_OBJECT

public:
    EventDatabase(QObject *parent = nullptr);

    QVector<SyncCache::Event> events(int accountId, SyncCache::DatabaseError *error, bool includeLocallyDeleted = true) const;
    SyncCache::Event event(int accountId, const QString &eventId, SyncCache::DatabaseError *error) const;
    void storeEvent(const SyncCache::Event &event, SyncCache::DatabaseError *error);
    void deleteEvent(int accountId, const QString &eventId, SyncCache::DatabaseError *error);
    void flagEventForDeletion(int accountId, const QString &eventId, SyncCache::DatabaseError *error);

Q_SIGNALS:
    void eventsStored(const QVector<SyncCache::Event> &events);
    void eventsDeleted(const QVector<SyncCache::Event> &events);
    void eventsFlaggedForDeletion(const QVector<SyncCache::Event> &events);
};

class EventCachePrivate;
class EventCache : public QObject
{
    Q_OBJECT

public:
    EventCache(QObject *parent = nullptr);
    ~EventCache();

    static QString eventCacheDir(int accountId);
    static QString eventCacheRootDir();

public Q_SLOTS:
    virtual void openDatabase(const QString &accountType); // e.g. "nextcloud"

    virtual void requestEvents(int accountId, bool includeLocallyDeleted = true);
    virtual void deleteEvent(int accountId, const QString &eventId);
    virtual void flagEventForDeletion(int accountId, const QString &eventId);

    virtual void populateEventImage(int idempToken, int accountId, const QString &eventId, const QNetworkRequest &requestTemplate);

Q_SIGNALS:
    void openDatabaseFailed(const QString &errorMessage);
    void openDatabaseFinished();

    void requestEventsFailed(int accountId, const QString &errorMessage);
    void requestEventsFinished(int accountId, const QVector<SyncCache::Event> &events);

    void deleteEventFailed(int accountId, const QString &eventId, const QString &errorMessage);
    void deleteEventFinished(int accountId, const QString &eventId);

    void flagEventForDeletionFailed(int accountId, const QString &eventId, const QString &errorMessage);
    void flagEventForDeletionFinished(int accountId, const QString &eventId);

    void populateEventImageFailed(int idempToken, const QString &errorMessage);
    void populateEventImageFinished(int idempToken, const QString &path);

    void eventsStored(const QVector<SyncCache::Event> &photos);
    void eventsDeleted(const QVector<SyncCache::Event> &photos);
    void eventsFlaggedForDeletion(const QVector<SyncCache::Event> &events);

private:
    Q_DECLARE_PRIVATE(EventCache)
    QScopedPointer<EventCachePrivate> d_ptr;
};

} // namespace SyncCache

Q_DECLARE_METATYPE(SyncCache::Event)
Q_DECLARE_METATYPE(QVector<SyncCache::Event>)
Q_DECLARE_TYPEINFO(SyncCache::Event, Q_MOVABLE_TYPE);

#endif // NEXTCLOUD_SYNCCACHEEVENTS_H
