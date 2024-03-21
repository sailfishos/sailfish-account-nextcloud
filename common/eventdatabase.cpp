/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "synccacheevents.h"
#include "synccacheevents_p.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUuid>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

#include <QtDebug>

using namespace SyncCache;

static bool upgradeVersion1to2Fn(QSqlDatabase &database)
{
    QSqlQuery alterTableQuery(QStringLiteral("ALTER TABLE Events ADD eventSubject TEXT;"), database);
    if (alterTableQuery.lastError().isValid()) {
        qWarning() << "Failed to update events database schema for version 2:" << alterTableQuery.lastError().text();
        return false;
    }
    alterTableQuery.finish();

    QSqlQuery updateSubjectQuery(QStringLiteral("UPDATE Events SET eventSubject = eventText, eventText = '';"), database);
    if (updateSubjectQuery.lastError().isValid()) {
        qWarning() << "Failed to update events database values for version 2:" << updateSubjectQuery.lastError().text();
        return false;
    }
    updateSubjectQuery.finish();

    return true;
}

static bool upgradeVersion2to3Fn(QSqlDatabase &database)
{
    QSqlQuery alterTableQuery(QStringLiteral("ALTER TABLE Events ADD deletedLocally BOOL;"), database);
    if (alterTableQuery.lastError().isValid()) {
        qWarning() << "Failed to update events database schema for version 3:" << alterTableQuery.lastError().text();
        return false;
    }
    alterTableQuery.finish();

    QSqlQuery updateDeleteLocallyQuery(QStringLiteral("UPDATE Events SET deletedLocally = 0;"), database);
    if (updateDeleteLocallyQuery.lastError().isValid()) {
        qWarning() << "Failed to update events database values for version 3:" << updateDeleteLocallyQuery.lastError().text();
        return false;
    }
    updateDeleteLocallyQuery.finish();

    return true;
}

EventDatabasePrivate::EventDatabasePrivate(EventDatabase *parent)
    : DatabasePrivate(parent), m_eventDbParent(parent)
{
}

int EventDatabasePrivate::currentSchemaVersion() const
{
    return 3;
}

QVector<const char *> EventDatabasePrivate::createStatements() const
{
    static const char *createEventsTable =
            "\n CREATE TABLE Events ("
            "\n accountId INTEGER,"
            "\n eventId TEXT,"
            "\n eventSubject TEXT,"
            "\n eventText TEXT,"
            "\n eventUrl TEXT,"
            "\n imageUrl TEXT,"
            "\n imagePath TEXT,"
            "\n timestamp TEXT,"
            "\n deletedLocally BOOL,"
            "\n PRIMARY KEY (accountId, eventId));";
    static QVector<const char *> retn { createEventsTable };
    return retn;
}

QVector<UpgradeOperation> EventDatabasePrivate::upgradeVersions() const
{
    static const char *upgradeVersion0to1[] = {
        "PRAGMA user_version=1",
        0 // NULL-terminated
    };

    static const char *upgradeVersion1to2[] = {
        "PRAGMA user_version=2",
        0 // NULL-terminated
    };

    static const char *upgradeVersion2to3[] = {
        "PRAGMA user_version=3",
        0 // NULL-terminated
    };
    static QVector<UpgradeOperation> retn {
        { 0, upgradeVersion0to1 },
        { upgradeVersion1to2Fn, upgradeVersion1to2 },
        { upgradeVersion2to3Fn, upgradeVersion2to3 },
    };

    return retn;
}

bool EventDatabasePrivate::preTransactionCommit()
{
    // nothing to do.
    return true;
}

void EventDatabasePrivate::transactionCommittedPreUnlock()
{
    m_tempFilesToDelete = m_filesToDelete;
    m_tempDeletedEvents = m_deletedEvents;
    m_tempLocallyDeletedEvents = m_locallyDeletedEvents;
    m_tempStoredEvents = m_storedEvents;
    m_filesToDelete.clear();
    m_deletedEvents.clear();
    m_locallyDeletedEvents.clear();
    m_storedEvents.clear();
}

void EventDatabasePrivate::transactionCommittedPostUnlock()
{
    for (const QString &doomed : m_tempFilesToDelete) {
        if (QFile::exists(doomed)) {
            QFile::remove(doomed);
        }
    }
    if (!m_tempDeletedEvents.isEmpty()) {
        emit m_eventDbParent->eventsDeleted(m_tempDeletedEvents);
    }
    if (!m_tempLocallyDeletedEvents.isEmpty()) {
        emit m_eventDbParent->eventsDeleted(m_tempLocallyDeletedEvents);
    }
    if (!m_tempStoredEvents.isEmpty()) {
        emit m_eventDbParent->eventsStored(m_tempStoredEvents);
    }
}

void EventDatabasePrivate::transactionRolledBackPreUnlocked()
{
    m_filesToDelete.clear();
    m_deletedEvents.clear();
    m_locallyDeletedEvents.clear();
    m_storedEvents.clear();
}

//-----------------------------------------------------------------------------

EventDatabase::EventDatabase(QObject *parent)
    : Database(new EventDatabasePrivate(this), parent)
{
    qRegisterMetaType<SyncCache::Event>();
    qRegisterMetaType<QVector<SyncCache::Event> >();
}

QVector<SyncCache::Event> EventDatabase::events(int accountId, DatabaseError *error, bool includeLocallyDeleted) const
{
    SYNCCACHE_DB_D(const EventDatabase);

    QString queryString = QStringLiteral("SELECT eventId, eventSubject, eventText, eventUrl, imageUrl, imagePath, timestamp, deletedLocally FROM Events"
                                         " WHERE accountId = :accountId");
    if (!includeLocallyDeleted) {
        queryString += QStringLiteral(" AND deletedLocally = 0");
    }
    queryString += QStringLiteral(" ORDER BY timestamp DESC, eventId ASC");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId)
    };

    auto resultHandler = [accountId](DatabaseQuery &selectQuery) -> SyncCache::Event {
        int whichValue = 0;
        Event currEvent;
        currEvent.accountId = accountId;
        currEvent.eventId = selectQuery.value(whichValue++).toString();
        currEvent.eventSubject = selectQuery.value(whichValue++).toString();
        currEvent.eventText = selectQuery.value(whichValue++).toString();
        currEvent.eventUrl = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.imageUrl = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.imagePath = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.timestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currEvent.deletedLocally = selectQuery.value(whichValue++).toBool();
        return currEvent;
    };

    return DatabaseImpl::fetchMultiple<SyncCache::Event>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("events"),
            error);
}

Event EventDatabase::event(int accountId, const QString &eventId, DatabaseError *error) const
{
    SYNCCACHE_DB_D(const EventDatabase);

    const QString queryString = QStringLiteral("SELECT eventSubject, eventText, eventUrl, imageUrl, imagePath, timestamp, deletedLocally FROM Events"
                                               " WHERE accountId = :accountId AND eventId = :eventId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":eventId"), eventId)
    };

    auto resultHandler = [accountId, eventId](DatabaseQuery &selectQuery) -> SyncCache::Event {
        int whichValue = 0;
        Event currEvent;
        currEvent.accountId = accountId;
        currEvent.eventId = eventId;
        currEvent.eventSubject = selectQuery.value(whichValue++).toString();
        currEvent.eventText = selectQuery.value(whichValue++).toString();
        currEvent.eventUrl = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.imageUrl = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.imagePath = QUrl(selectQuery.value(whichValue++).toString());
        currEvent.timestamp = QDateTime::fromString(selectQuery.value(whichValue++).toString(), Qt::ISODate);
        currEvent.deletedLocally = selectQuery.value(whichValue++).toBool();
        return currEvent;
    };

    return DatabaseImpl::fetch<SyncCache::Event>(
            d,
            queryString,
            bindValues,
            resultHandler,
            QStringLiteral("event"),
            error);
}

void EventDatabase::storeEvent(const Event &event, DatabaseError *error)
{
    SYNCCACHE_DB_D(EventDatabase);

    DatabaseError err;
    const Event existingEvent = this->event(event.accountId, event.eventId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing event %1 for store: %2")
                                   .arg(event.eventId, err.errorMessage));
        return;
    }

    const QString insertString = QStringLiteral("INSERT INTO Events (accountId, eventId, eventSubject, eventText, eventUrl, imageUrl, imagePath, timestamp, deletedLocally)"
                                                " VALUES(:accountId, :eventId, :eventSubject, :eventText, :eventUrl, :imageUrl, :imagePath, :timestamp, :deletedLocally)");
    const QString updateString = QStringLiteral("UPDATE Events SET eventSubject = :eventSubject, eventText = :eventText, eventUrl = :eventUrl, imageUrl = :imageUrl, imagePath = :imagePath, timestamp = :timestamp, deletedLocally = :deletedLocally"
                                                " WHERE accountId = :accountId AND eventId = :eventId");

    const bool insert = existingEvent.eventId.isEmpty();
    const QString queryString = insert ? insertString : updateString;

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), event.accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":eventId"), event.eventId),
        qMakePair<QString, QVariant>(QStringLiteral(":eventSubject"), event.eventSubject),
        qMakePair<QString, QVariant>(QStringLiteral(":eventText"), event.eventText),
        qMakePair<QString, QVariant>(QStringLiteral(":eventUrl"), event.eventUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":imageUrl"), event.imageUrl),
        qMakePair<QString, QVariant>(QStringLiteral(":imagePath"), event.imagePath),
        qMakePair<QString, QVariant>(QStringLiteral(":timestamp"), event.timestamp.toString(Qt::ISODate)),
        qMakePair<QString, QVariant>(QStringLiteral(":deletedLocally"), event.deletedLocally),
    };

    auto storeResultHandler = [d, event]() -> void {
        d->m_storedEvents.append(event);
    };

    DatabaseImpl::store<SyncCache::Event>(
            d,
            queryString,
            bindValues,
            storeResultHandler,
            QStringLiteral("event"),
            error);
}

void EventDatabase::deleteEvent(int accountId, const QString &eventId, DatabaseError *error)
{
    SYNCCACHE_DB_D(EventDatabase);

    DatabaseError err;
    const Event existingEvent = this->event(accountId, eventId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing event %1 for delete: %2")
                                   .arg(eventId, err.errorMessage));
        return;
    }

    if (existingEvent.eventId.isEmpty()) {
        // does not exist in database, already deleted.
        return;
    }

    auto deleteRelatedValues = [] (DatabaseError *) -> void { };

    const QString queryString = QStringLiteral("DELETE FROM Events"
                                               " WHERE accountId = :accountId AND eventId = :eventId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":eventId"), eventId)
    };

    auto deleteResultHandler = [d, existingEvent]() -> void {
        d->m_deletedEvents.append(existingEvent);
        if (!existingEvent.imagePath.isEmpty()) {
            d->m_filesToDelete.append(existingEvent.imagePath.toString());
        }
    };

    DatabaseImpl::deleteValue<SyncCache::Event>(
            d,
            deleteRelatedValues,
            queryString,
            bindValues,
            deleteResultHandler,
            QStringLiteral("event"),
            error);
}

void EventDatabase::flagEventForDeletion(int accountId, const QString &eventId, SyncCache::DatabaseError *error)
{
    SYNCCACHE_DB_D(EventDatabase);

    DatabaseError err;
    const Event existingEvent = this->event(accountId, eventId, &err);
    if (err.errorCode != DatabaseError::NoError) {
        setDatabaseError(error, err.errorCode,
                         QStringLiteral("Error while querying existing event %1 for flagging: %2")
                                   .arg(eventId, err.errorMessage));
        return;
    }

    if (existingEvent.eventId.isEmpty()) {
        // does not exist in database
        return;
    }

    const QString queryString = QStringLiteral("UPDATE Events SET deletedLocally = :deletedLocally"
                                               " WHERE accountId = :accountId AND eventId = :eventId");

    const QList<QPair<QString, QVariant> > bindValues {
        qMakePair<QString, QVariant>(QStringLiteral(":accountId"), accountId),
        qMakePair<QString, QVariant>(QStringLiteral(":eventId"), eventId),
        qMakePair<QString, QVariant>(QStringLiteral(":deletedLocally"), true),
    };

    auto locallyDeleteResultHandler = [d, existingEvent]() -> void {
        d->m_locallyDeletedEvents.append(existingEvent);
    };

    DatabaseImpl::store<SyncCache::Event>(
            d,
            queryString,
            bindValues,
            locallyDeleteResultHandler,
            QStringLiteral("event"),
            error);
}
