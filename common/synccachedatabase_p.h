/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHE_DATABASE_P_H
#define NEXTCLOUD_SYNCCACHE_DATABASE_P_H

#include "synccachedatabase.h"
#include "processmutex_p.h"

#include <QtCore/QScopedPointer>
#include <QtCore/QHash>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QMutex>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <functional>

#define SYNCCACHE_DB_D(Class) Class##Private * const d = static_cast<Class##Private *>(d_func())

namespace SyncCache {

class DatabasePrivate;
class DatabaseQuery {
public:
    ~DatabaseQuery() { finish(); }

    QSqlError lastError() const { return m_error.isValid() ? m_error : m_query.lastError(); }

    void bindValue(const QString &id, const QVariant &value) { m_query.bindValue(id, value); }

    bool exec() { return m_query.exec(); }
    bool next() { return m_query.next(); }
    bool isValid() { return m_query.isValid(); }
    void finish() { return m_query.finish(); }
    QString executedQuery() const { return m_query.executedQuery(); }
    void setForwardOnly(bool forwardOnly) { m_query.setForwardOnly(forwardOnly); }

    QVariant lastInsertId() const { return m_query.lastInsertId(); }
    QVariant value(int index) { return m_query.value(index); }
    template<typename T> T value(int index) { return m_query.value(index).value<T>(); }

    operator QSqlQuery &() { return m_query; }
    operator QSqlQuery const &() const { return m_query; }

private:
    friend class DatabasePrivate;
    explicit DatabaseQuery(const QSqlQuery &query) : m_query(query) {}
    explicit DatabaseQuery(const QSqlError &error) : m_error(error) {}
    QSqlQuery m_query;
    QSqlError m_error;
};

typedef bool (*UpgradeFunction)(QSqlDatabase &database);
struct UpgradeOperation {
    UpgradeFunction fn;
    const char **statements;
};

class Database;
class DatabasePrivate
{
public:
    DatabasePrivate(Database *parent) : m_parent(parent) {}
    virtual ~DatabasePrivate();

    virtual int currentSchemaVersion() const = 0;
    virtual QVector<const char *> createStatements() const = 0;
    virtual QVector<UpgradeOperation> upgradeVersions() const = 0;

    virtual bool preTransactionCommit() = 0;
    virtual void transactionCommittedPreUnlock() = 0;
    virtual void transactionCommittedPostUnlock() = 0;
    virtual void transactionRolledBackPreUnlocked() = 0;

    DatabaseQuery prepare(const char *statement) const {
        return prepare(QLatin1String(statement));
    }
    DatabaseQuery prepare(const QString &statement) const {
        QMutexLocker lock(&m_preparedQueriesMutex);

        QHash<QString, QSqlQuery>::const_iterator it = m_preparedQueries.constFind(statement);
        if (it == m_preparedQueries.constEnd()) {
            QSqlQuery query(m_database);
            query.setForwardOnly(true);
            if (!query.prepare(statement)) {
                return DatabaseQuery(query.lastError());
            }
            it = m_preparedQueries.insert(statement, query);
        }
        return DatabaseQuery(*it);
    }

    QSqlDatabase m_database;
    Database *m_parent;

private:
    friend class SyncCache::Database;
    mutable QScopedPointer<ProcessMutex> m_processMutex;
    bool m_inTransaction = false;

    mutable QMutex m_preparedQueriesMutex;
    mutable QHash<QString, QSqlQuery> m_preparedQueries;
};

namespace DatabaseImpl {

template <typename T>
QVector<T> fetchMultiple(
        const DatabasePrivate *d,
        const QString &queryString,
        const QList<QPair<QString, QVariant> > &bindValues,
        std::function<T(DatabaseQuery&)> resultHandler,
        const QString &queryName,
        DatabaseError *error)
{
    QVector<T> retn;
    if (!d->m_database.isOpen()) {
        Database::setDatabaseError(error, DatabaseError::NotOpenError,
                                   QStringLiteral("Database is not open, cannot query %1")
                                             .arg(queryName));
        return retn;
    }

    DatabaseQuery selectQuery(d->prepare(queryString));
    if (selectQuery.lastError().isValid()) {
        Database::setDatabaseError(error, DatabaseError::PrepareQueryError,
                                   QStringLiteral("Failed to prepare %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(selectQuery.lastError().text())
                                             .arg(queryString));
        return retn;
    }

    for (const QPair<QString, QVariant> &bindValue : bindValues) {
        selectQuery.bindValue(bindValue.first, bindValue.second);
    }

    if (!selectQuery.exec()) {
        Database::setDatabaseError(error, DatabaseError::QueryError,
                                   QStringLiteral("Failed to execute %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(selectQuery.lastError().text())
                                             .arg(selectQuery.executedQuery()));
        return retn;
    }

    //retn.reserve(selectQuery.size()); // todo
    while (selectQuery.next()) {
        retn.append(resultHandler(selectQuery));
    }

    return retn;
}

template<typename T>
T fetch(const DatabasePrivate *d,
        const QString &queryString,
        const QList<QPair<QString, QVariant> > &bindValues,
        std::function<T(DatabaseQuery&)> resultHandler,
        const QString &queryName,
        DatabaseError *error)
{
    T retn;
    if (!d->m_database.isOpen()) {
        Database::setDatabaseError(error, DatabaseError::NotOpenError,
                                   QStringLiteral("Database is not open, cannot query %1")
                                             .arg(queryName));
        return retn;
    }

    DatabaseQuery selectQuery(d->prepare(queryString));
    if (selectQuery.lastError().isValid()) {
        Database::setDatabaseError(error, DatabaseError::PrepareQueryError,
                                   QStringLiteral("Failed to prepare %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(selectQuery.lastError().text())
                                             .arg(queryString));
        return retn;
    }

    for (const QPair<QString, QVariant> &bindValue : bindValues) {
        selectQuery.bindValue(bindValue.first, bindValue.second);
    }

    if (!selectQuery.exec()) {
        Database::setDatabaseError(error, DatabaseError::QueryError,
                                   QStringLiteral("Failed to execute %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(selectQuery.lastError().text())
                                             .arg(selectQuery.executedQuery()));
        return retn;
    }

    if (selectQuery.next()) {
        return resultHandler(selectQuery);
    }

    return retn;
}

template<typename T>
void store(
        DatabasePrivate *d,
        const QString &queryString,
        const QList<QPair<QString, QVariant> > &bindValues,
        std::function<void()> storeResultHandler,
        const QString &queryName,
        DatabaseError *error)
{
    if (!d->m_database.isOpen()) {
        Database::setDatabaseError(error, DatabaseError::NotOpenError,
                                   QStringLiteral("Database is not open, cannot store %1")
                                             .arg(queryName));
        return;
    }

    DatabaseQuery storeQuery(d->prepare(queryString));
    if (storeQuery.lastError().isValid()) {
        Database::setDatabaseError(error, DatabaseError::PrepareQueryError,
                                   QStringLiteral("Failed to prepare store %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(storeQuery.lastError().text())
                                             .arg(queryString));
        return;
    }

    for (const QPair<QString, QVariant> &bindValue : bindValues) {
        storeQuery.bindValue(bindValue.first, bindValue.second);
    }

    const bool wasInTransaction = d->m_parent->inTransaction();
    if (!wasInTransaction && !d->m_parent->beginTransaction(error)) {
        return;
    }

    if (!storeQuery.exec()) {
        Database::setDatabaseError(error, DatabaseError::QueryError,
                                   QStringLiteral("Failed to execute store %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(storeQuery.lastError().text())
                                             .arg(storeQuery.executedQuery()));
        if (!wasInTransaction) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
    } else {
        storeResultHandler();
        if (!wasInTransaction && !d->m_parent->commitTransaction(error)) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
    }

    return;
}

template<typename T>
void deleteValue(
        DatabasePrivate *d,
        std::function<void(DatabaseError*)> deleteRelatedValues,
        const QString &queryString,
        const QList<QPair<QString, QVariant> > &bindValues,
        std::function<void()> deleteResultHandler,
        const QString &queryName,
        DatabaseError *error)
{
    if (!d->m_database.isOpen()) {
        Database::setDatabaseError(error, DatabaseError::NotOpenError,
                                   QStringLiteral("Database is not open, cannot delete %1")
                                             .arg(queryName));
        return;
    }

    const bool wasInTransaction = d->m_parent->inTransaction();
    if (!wasInTransaction && !d->m_parent->beginTransaction(error)) {
        return;
    }

    deleteRelatedValues(error);
    if (error->errorCode != DatabaseError::NoError) {
        if (!wasInTransaction) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
        return;
    }

    DatabaseQuery deleteQuery(d->prepare(queryString));
    if (deleteQuery.lastError().isValid()) {
        Database::setDatabaseError(error, DatabaseError::PrepareQueryError,
                                   QStringLiteral("Failed to prepare delete %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(deleteQuery.lastError().text())
                                             .arg(queryString));
        if (!wasInTransaction) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
        return;
    }

    for (const QPair<QString, QVariant> &bindValue : bindValues) {
        deleteQuery.bindValue(bindValue.first, bindValue.second);
    }

    if (!deleteQuery.exec()) {
        Database::setDatabaseError(error, DatabaseError::QueryError,
                                   QStringLiteral("Failed to execute delete %1 query: %2\n%3")
                                             .arg(queryName)
                                             .arg(deleteQuery.lastError().text())
                                             .arg(deleteQuery.executedQuery()));
        if (!wasInTransaction) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
    } else {
        deleteResultHandler();
        if (!wasInTransaction && !d->m_parent->commitTransaction(error)) {
            DatabaseError rollbackError;
            d->m_parent->rollbackTransaction(&rollbackError);
        }
    }
}

} // namespace DatabaseImpl

} // namespace SyncCache

#endif // NEXTCLOUD_SYNCCACHE_DATABASE_P_H
