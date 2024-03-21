/*
 * Copyright (C) 2013-2019 Jolla Ltd.
 * Contact: Matthew Vogt <matthew.vogt@jollamobile.com>
 *
 * All rights reserved.
 * License: Proprietary.
 */

#ifndef NEXTCLOUD_PROCESSMUTEX_P_H
#define NEXTCLOUD_PROCESSMUTEX_P_H

#include <QString>
#include <QMutex>

namespace SyncCache {

class Semaphore
{
public:
    Semaphore(const char *identifier, int initial);
    Semaphore(const char *identifier, size_t count, const int *initialValues);
    ~Semaphore();

    bool isValid() const;

    bool decrement(size_t index = 0, bool wait = true, size_t timeoutMs = 0);
    bool increment(size_t index = 0, bool wait = true, size_t timeoutMs = 0);

    int value(size_t index = 0) const;

private:
    void error(const char *msg, int error);

    QString m_identifier;
    int m_id;
};

class ProcessMutex
{
    Semaphore m_semaphore;
    QMutex m_mutex;
    bool m_initialProcess;

public:
    ProcessMutex(const QString &path);
    bool lock();
    bool unlock();
    bool isLocked() const;
    bool isInitialProcess() const;
};

} // namespace SyncCache

#endif // NEXTCLOUD_PROCESSMUTEX_P_H
