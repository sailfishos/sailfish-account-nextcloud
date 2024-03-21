/*
 * Copyright (C) 2013-2019 Jolla Ltd.
 * Contact: Matthew Vogt <matthew.vogt@jollamobile.com>
 *
 * All rights reserved.
 * License: Proprietary.
 */

#include "processmutex_p.h"

#include <errno.h>
#include <unistd.h>

#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include <QMutexLocker>
#include <QtDebug>

namespace {

const int initialSemaphoreValues[] = { 1, 0, 1 };

const size_t databaseOwnershipIndex = 0;
const size_t databaseConnectionsIndex = 1;
const size_t writeAccessIndex = 2;

// Defined as required for ::semun
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
};

void semaphoreError(const char *msg, const char *id, int error)
{
    qWarning() << QString::fromLatin1("%1 %2: %3 (%4)").arg(msg).arg(id).arg(::strerror(error)).arg(error);
}

int semaphoreInit(const char *id, size_t count, const int *initialValues)
{
    int rv = -1;

    // It doesn't matter what proj_id we use, there are no other ftok uses on this ID
    key_t key = ::ftok(id, 1);

    rv = ::semget(key, count, 0);
    if (rv == -1) {
        if (errno != ENOENT) {
            semaphoreError("Unable to get semaphore", id, errno);
        } else {
            // The semaphore does not currently exist
            rv = ::semget(key, count, IPC_CREAT | IPC_EXCL | S_IRWXO | S_IRWXG | S_IRWXU);
            if (rv == -1) {
                if (errno == EEXIST) {
                    // Someone else won the race to create the semaphore - retry get
                    rv = ::semget(key, count, 0);
                }

                if (rv == -1) {
                    semaphoreError("Unable to create semaphore", id, errno);
                }
            } else {
                // Set the initial value
                for (size_t i = 0; i < count; ++i) {
                    union semun arg = { 0 };
                    arg.val = *initialValues++;

                    int status = ::semctl(rv, static_cast<int>(i), SETVAL, arg);
                    if (status == -1) {
                        rv = -1;
                        semaphoreError("Unable to initialize semaphore", id, errno);
                    }
                }
            }
        }
    }

    return rv;
}

bool semaphoreIncrement(int id, size_t index, bool wait, size_t ms, int value)
{
    if (id == -1) {
        errno = 0;
        return false;
    }

    struct sembuf op;
    op.sem_num = index;
    op.sem_op = value;
    op.sem_flg = SEM_UNDO;
    if (!wait) {
        op.sem_flg |= IPC_NOWAIT;
    }

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = ms * 1000;

    do {
        int rv = ::semtimedop(id, &op, 1, (wait && ms > 0 ? &timeout : 0));
        if (rv == 0)
            return true;
    } while (errno == EINTR);

    return false;
}

}

using namespace SyncCache;

Semaphore::Semaphore(const char *id, int initial)
    : m_identifier(id)
    , m_id(-1)
{
    m_id = semaphoreInit(m_identifier.toUtf8().constData(), 1, &initial);
}

Semaphore::Semaphore(const char *id, size_t count, const int *initialValues)
    : m_identifier(id)
    , m_id(-1)
{
    m_id = semaphoreInit(m_identifier.toUtf8().constData(), count, initialValues);
}

Semaphore::~Semaphore()
{
}

bool Semaphore::isValid() const
{
    return (m_id != -1);
}

bool Semaphore::decrement(size_t index, bool wait, size_t timeoutMs)
{
    if (!semaphoreIncrement(m_id, index, wait, timeoutMs, -1)) {
        if (errno != EAGAIN || wait) {
            error("Unable to decrement semaphore", errno);
        }
        return false;
    }
    return true;
}

bool Semaphore::increment(size_t index, bool wait, size_t timeoutMs)
{
    if (!semaphoreIncrement(m_id, index, wait, timeoutMs, 1)) {
        if (errno != EAGAIN || wait) {
            error("Unable to increment semaphore", errno);
        }
        return false;
    }
    return true;
}

int Semaphore::value(size_t index) const
{
    if (m_id == -1)
        return -1;

    return ::semctl(m_id, index, GETVAL, 0);
}

void Semaphore::error(const char *msg, int error)
{
    semaphoreError(msg, m_identifier.toUtf8().constData(), error);
}

ProcessMutex::ProcessMutex(const QString &path)
    : m_semaphore(path.toLatin1(), 3, initialSemaphoreValues)
    , m_initialProcess(false)
{
    if (!m_semaphore.isValid()) {
        qWarning() << QStringLiteral("Unable to create semaphore array!");
    } else {
        if (!m_semaphore.decrement(databaseOwnershipIndex)) {
            qWarning() << QStringLiteral("Unable to determine database ownership!");
        } else {
            // Only the first process to connect to the semaphore is the owner
            m_initialProcess = (m_semaphore.value(databaseConnectionsIndex) == 0);
            if (!m_semaphore.increment(databaseConnectionsIndex)) {
                qWarning() << QStringLiteral("Unable to increment database connections!");
            }
            m_semaphore.increment(databaseOwnershipIndex);
        }
    }
}

bool ProcessMutex::lock()
{
    m_mutex.lock();
    return m_semaphore.decrement(writeAccessIndex);
}

bool ProcessMutex::unlock()
{
    bool retn = m_semaphore.increment(writeAccessIndex);
    m_mutex.unlock();
    return retn;
}

bool ProcessMutex::isLocked() const
{
    return (m_semaphore.value(writeAccessIndex) == 0);
}

bool ProcessMutex::isInitialProcess() const
{
    return m_initialProcess;
}

