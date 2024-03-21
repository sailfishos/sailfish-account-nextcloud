/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_SYNCCACHE_DATABASE_H
#define NEXTCLOUD_SYNCCACHE_DATABASE_H

#include <QtCore/QObject>
#include <QtCore/QString>

namespace SyncCache {

struct DatabaseError {
    enum ErrorCode {
        NoError = 0,
        CreateError,
        OpenError,
        AlreadyOpenError,
        NotOpenError,
        ConfigurationError,
        IntegrityCheckError,
        UpgradeError,
        ProcessMutexError,
        VersionQueryError,
        VersionMismatchError,
        TransactionError,
        TransactionLockError,
        PrepareQueryError,
        QueryError,
        InvalidArgumentError,
        UnknownError = 1024
    };
    ErrorCode errorCode = NoError;
    QString errorMessage;
};

class ProcessMutex;
class DatabasePrivate;
class Database : public QObject
{
    Q_OBJECT

public:
    virtual ~Database();
    ProcessMutex *processMutex() const;
    void openDatabase(const QString &fileName, SyncCache::DatabaseError *error);

    bool inTransaction() const;
    bool beginTransaction(SyncCache::DatabaseError *error);
    bool commitTransaction(SyncCache::DatabaseError *error);
    bool rollbackTransaction(SyncCache::DatabaseError *error);

    static void setDatabaseError(DatabaseError *error, DatabaseError::ErrorCode code, const QString &message);

protected:
    explicit Database(DatabasePrivate *dptr, QObject *parent = nullptr);
    Q_DECLARE_PRIVATE(Database)
    Q_DISABLE_COPY(Database)
    QScopedPointer<DatabasePrivate> d_ptr;
};

} // namespace SyncCache

#endif // NEXTCLOUD_SYNCCACHE_DATABASE_H
