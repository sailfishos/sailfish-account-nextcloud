/****************************************************************************************
**
** Copyright (C) 2013-2019 Jolla Ltd
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "synccachedatabase.h"
#include "synccachedatabase_p.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QMutexLocker>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

#include <QtDebug>

using namespace SyncCache;

static const char *quickCheck =
        "\n PRAGMA quick_check;";
static const char *quickCheckOk = "ok";

static const char *userVersion =
        "\n PRAGMA user_version;";

static const char *setUserVersion =
        "\n PRAGMA user_version=%1;";

static const char *setupEncoding =
        "\n PRAGMA encoding = \"UTF-16\";";

static const char *setupTempStore =
        "\n PRAGMA temp_store = MEMORY;";

static const char *setupJournal =
        "\n PRAGMA journal_mode = WAL;";

static const char *setupSynchronous =
        "\n PRAGMA synchronous = FULL;";

static const char *setupForeignKeys =
        "\n PRAGMA foreign_keys = ON;";

static bool execute(QSqlDatabase &database, const QString &statement)
{
    QSqlQuery query(database);
    if (!query.exec(statement)) {
        qWarning() << QString::fromLatin1("Query failed: %1\n%2")
                                     .arg(query.lastError().text())
                                     .arg(statement);
        return false;
    } else {
        return true;
    }
}

static bool beginTransaction(QSqlDatabase &database)
{
    // Use immediate lock acquisition; we should already have an IPC lock, so
    // there will be no lock contention with other writing processes
    return execute(database, QString::fromLatin1("BEGIN IMMEDIATE TRANSACTION"));
}

static bool commitTransaction(QSqlDatabase &database)
{
    return execute(database, QString::fromLatin1("COMMIT TRANSACTION"));
}

static bool rollbackTransaction(QSqlDatabase &database)
{
    return execute(database, QString::fromLatin1("ROLLBACK TRANSACTION"));
}

static bool finalizeTransaction(QSqlDatabase &database, bool success)
{
    if (success) {
        return commitTransaction(database);
    }

    rollbackTransaction(database);
    return false;
}

static bool executeUpgradeStatements(QSqlDatabase &database, const int currentSchemaVersion, const QVector<UpgradeOperation> &upgradeVersions)
{
    // Check that the defined schema matches the array of upgrade scripts
    if (currentSchemaVersion != upgradeVersions.size()) {
        qWarning() << "Invalid schema version:" << currentSchemaVersion;
        return false;
    }

    QSqlQuery versionQuery(database);
    versionQuery.prepare("PRAGMA user_version");
    if (!versionQuery.exec() || !versionQuery.next()) {
        qWarning() << "User version query failed:" << versionQuery.lastError();
        return false;
    }

    quint16 schemaVersion = versionQuery.value(0).toInt();
    versionQuery.finish();

    while (schemaVersion < currentSchemaVersion) {
        qWarning() << "Upgrading database from schema version" << schemaVersion << ":" << database.connectionName();

        if (upgradeVersions[schemaVersion].fn) {
            if (!(*upgradeVersions[schemaVersion].fn)(database)) {
                qWarning() << "Unable to update data for schema version" << schemaVersion;
                return false;
            }
        }
        if (upgradeVersions[schemaVersion].statements) {
            for (unsigned i = 0; upgradeVersions[schemaVersion].statements[i]; i++) {
                if (!execute(database, QLatin1String(upgradeVersions[schemaVersion].statements[i])))
                    return false;
            }
        }

        if (!versionQuery.exec() || !versionQuery.next()) {
            qWarning() << "User version query failed:" << versionQuery.lastError();
            return false;
        }

        int version = versionQuery.value(0).toInt();
        versionQuery.finish();

        if (version <= schemaVersion) {
            qWarning() << "Nextcloud database schema upgrade cycle detected - aborting";
            return false;
        } else {
            schemaVersion = version;
            if (schemaVersion == currentSchemaVersion) {
                qWarning() << "Nextcloud database upgraded to version" << schemaVersion;
            }
        }
    }

    if (schemaVersion > currentSchemaVersion) {
        qWarning() << "Nextcloud database schema is newer than expected - this may result in failures or corruption";
    }

    return true;
}

static bool checkDatabase(QSqlDatabase &database)
{
    QSqlQuery query(database);
    if (query.exec(QLatin1String(quickCheck))) {
        while (query.next()) {
            const QString result(query.value(0).toString());
            if (result == QLatin1String(quickCheckOk)) {
                return true;
            }
            qWarning() << "Integrity problem:" << result;
        }
    }

    return false;
}

static bool upgradeDatabase(QSqlDatabase &database, const int currentSchemaVersion, const QVector<UpgradeOperation> &upgradeVersions)
{
    if (!beginTransaction(database))
        return false;

    bool success = executeUpgradeStatements(database, currentSchemaVersion, upgradeVersions);

    return finalizeTransaction(database, success);
}

static bool configureDatabase(QSqlDatabase &database)
{
    if (!execute(database, QLatin1String(setupEncoding))
            || !execute(database, QLatin1String(setupTempStore))
            || !execute(database, QLatin1String(setupJournal))
            || !execute(database, QLatin1String(setupSynchronous))
            || !execute(database, QLatin1String(setupForeignKeys))) {
        return false;
    }

    return true;
}

static bool executeCreationStatements(QSqlDatabase &database, const int currentSchemaVersion, const QVector<const char *> &createStatements)
{
    for (int i = 0; i < createStatements.size(); ++i) {
        QSqlQuery query(database);
        if (!query.exec(QLatin1String(createStatements[i]))) {
            qWarning() << QString::fromLatin1("Database creation failed: %1\n%2")
                                         .arg(query.lastError().text())
                                         .arg(createStatements[i]);
            return false;
        }
    }

    if (!execute(database, QString::fromLatin1(setUserVersion).arg(currentSchemaVersion))) {
        return false;
    }

    return true;
}

static bool prepareDatabase(QSqlDatabase &database, const int currentSchemaVersion, const QVector<const char *> &createStatements)
{
    if (!configureDatabase(database))
        return false;

    if (!beginTransaction(database))
        return false;

    bool success = executeCreationStatements(database, currentSchemaVersion, createStatements);

    return finalizeTransaction(database, success);
}

//-----------------------------------------------------------------------------

DatabasePrivate::~DatabasePrivate()
{
}

Database::Database(DatabasePrivate *dptr, QObject *parent)
    : QObject(parent), d_ptr(dptr)
{
}

Database::~Database()
{
}

void Database::setDatabaseError(DatabaseError *error, DatabaseError::ErrorCode code, const QString &message) {
    if (error) {
        error->errorCode = code;
        error->errorMessage = message;
    }
}

ProcessMutex *Database::processMutex() const
{
    Q_D(const Database);

    if (!d->m_processMutex) {
        Q_ASSERT(d->m_database.isOpen());
        d->m_processMutex.reset(new ProcessMutex(d->m_database.databaseName()));
    }
    return d->m_processMutex.data();
}

void Database::openDatabase(const QString &fileName, DatabaseError *error)
{
    Q_D(Database);

    if (d->m_database.isOpen()) {
        setDatabaseError(error, DatabaseError::AlreadyOpenError,
                         QString::fromLatin1("Unable to open database when already open: %1").arg(fileName));
        return;
    }

    QDir dir = QFileInfo(fileName).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            setDatabaseError(error, DatabaseError::CreateError,
                             QString::fromLatin1("Unable to create database directory: %1").arg(dir.path()));
            return;
        }
    }

    const bool databasePreexisting = QFile::exists(fileName);
    const QString connectionName = QUuid::createUuid().toString().mid(1, 36);
    d->m_database = QSqlDatabase::addDatabase(QString::fromLatin1("QSQLITE"), connectionName);
    d->m_database.setDatabaseName(fileName);

    if (!d->m_database.open()) {
        setDatabaseError(error,
                         databasePreexisting ? DatabaseError::OpenError
                                             : DatabaseError::CreateError,
                         databasePreexisting ? QString::fromLatin1("Unable to open database: %1: %2")
                                                              .arg(fileName, d->m_database.lastError().text())
                                             : QString::fromLatin1("Unable to create database: %1: %2")
                                                              .arg(fileName, d->m_database.lastError().text()));
        return;
    }

    if (!databasePreexisting && !prepareDatabase(d->m_database, d->currentSchemaVersion(), d->createStatements())) {
        setDatabaseError(error, DatabaseError::CreateError,
                         QString::fromLatin1("Failed to prepare database: %1")
                                        .arg(d->m_database.lastError().text()));
        d->m_database.close();
        QFile::remove(fileName);
        return;
    } else if (databasePreexisting && !configureDatabase(d->m_database)) {
        setDatabaseError(error, DatabaseError::ConfigurationError,
                         QString::fromLatin1("Failed to configure Nextcloud database: %1")
                                        .arg(d->m_database.lastError().text()));
        d->m_database.close();
        return;
    }

    // Get the process mutex for this database
    ProcessMutex *mutex(processMutex());

    // Only the first connection in the first process to concurrently open the DB is the owner
    const bool databaseOwner(mutex->isInitialProcess());

    if (databasePreexisting && databaseOwner) {
        // Try to upgrade, if necessary
        if (!mutex->lock()) {
            setDatabaseError(error, DatabaseError::ProcessMutexError,
                             QString::fromLatin1("Failed to lock mutex for database: %1")
                                            .arg(fileName));
            d->m_database.close();
            return;
        }

        // Perform an integrity check
        if (!checkDatabase(d->m_database)) {
            setDatabaseError(error, DatabaseError::IntegrityCheckError,
                             QString::fromLatin1("Database integrity check failed: %1")
                                            .arg(d->m_database.lastError().text()));
            d->m_database.close();
            mutex->unlock();
            return;
        }

        if (!upgradeDatabase(d->m_database, d->currentSchemaVersion(), d->upgradeVersions())) {
            setDatabaseError(error, DatabaseError::UpgradeError,
                             QString::fromLatin1("Failed to upgrade database: %1")
                                            .arg(d->m_database.lastError().text()));
            d->m_database.close();
            mutex->unlock();
            return;
        }

        mutex->unlock();
    } else if (databasePreexisting && !databaseOwner) {
        // check that the version is correct.  If not, it is probably because another process
        // with an open database connection is preventing upgrade of the database schema.
        QSqlQuery versionQuery(d->m_database);
        versionQuery.prepare(QLatin1String(userVersion));
        if (!versionQuery.exec() || !versionQuery.next()) {
            setDatabaseError(error, DatabaseError::VersionQueryError,
                             QString::fromLatin1("Failed to query existing database schema version: %1")
                                            .arg(versionQuery.lastError().text()));
            d->m_database.close();
            return;
        }

        const int schemaVersion = versionQuery.value(0).toInt();
        if (schemaVersion != d->currentSchemaVersion()) {
            setDatabaseError(error, DatabaseError::VersionMismatchError,
                             QString::fromLatin1("Existing database schema version is unexpected: %1 != %2. "
                                                 "Is a process preventing schema upgrade?")
                                            .arg(schemaVersion).arg(d->currentSchemaVersion()));
            d->m_database.close();
            return;
        }
    }
}

bool Database::inTransaction() const
{
    Q_D(const Database);
    return d->m_inTransaction;
}

bool Database::beginTransaction(DatabaseError *error)
{
    Q_D(Database);

    if (inTransaction()) {
        setDatabaseError(error, DatabaseError::TransactionError,
                         QStringLiteral("Transaction error: cannot begin transaction, already in a transaction"));
        return false;
    }

    ProcessMutex *mutex(processMutex());

    // We use a cross-process mutex to ensure only one process can write
    // to the DB at once.  Without external locking, SQLite will back off
    // on write contention, and the backed-off process may never get access
    // if other processes are performing regular writes.
    if (mutex->lock()) {
        if (::beginTransaction(d->m_database)) {
            d->m_inTransaction = true;
            return true;
        }

        setDatabaseError(error, DatabaseError::TransactionError,
                         QStringLiteral("Transaction error: unable to begin transaction: %1")
                                   .arg(d->m_database.lastError().text()));
        mutex->unlock();
    } else {
        setDatabaseError(error, DatabaseError::TransactionLockError,
                         QStringLiteral("Lock error: unable to lock for transaction"));
    }

    return false;
}

bool Database::commitTransaction(DatabaseError *error)
{
    Q_D(Database);

    ProcessMutex *mutex(processMutex());

    if (mutex->isLocked()) {
        if (d->preTransactionCommit()) {
            if (::commitTransaction(d->m_database)) {
                d->m_inTransaction = false;
                d->transactionCommittedPreUnlock();
                mutex->unlock(); // process mutex.
                d->transactionCommittedPostUnlock();

                return true;
            }

            setDatabaseError(error, DatabaseError::TransactionError,
                             QStringLiteral("Transaction error: unable to commit transaction: %1")
                                       .arg(d->m_database.lastError().text()));
        } else {
            setDatabaseError(error, DatabaseError::TransactionError,
                             QStringLiteral("Transaction error: pre-commit hook failed: %1")
                                       .arg(d->m_database.lastError().text()));
        }
    } else {
        setDatabaseError(error, DatabaseError::TransactionLockError,
                         QStringLiteral("Lock error: no lock held on commit"));
    }

    return false;
}

bool Database::rollbackTransaction(DatabaseError *error)
{
    Q_D(Database);

    ProcessMutex *mutex(processMutex());

    const bool rv = ::rollbackTransaction(d->m_database);
    d->transactionRolledBackPreUnlocked();
    d->m_inTransaction = false;

    if (mutex->isLocked()) {
        mutex->unlock();
    } else {
        setDatabaseError(error, DatabaseError::TransactionLockError,
                         QStringLiteral("Lock error: no lock held on rollback"));
    }

    return rv;
}
