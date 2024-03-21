// modified version of BSD-licensed code copied from qtcontacts-sqlite
/*
 * Copyright (C) 2013 Jolla Ltd
 * Copyright (C) 2020 Open Mobile Platform LLC
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "synccacheimagechangenotifier_p.h"

#include "synccacheimages.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QVector>
#include <QTimer>

#include <QDebug>

#define NOTIFIER_PATH "/org/sailfishos/nextcloud/gallery"
#define NOTIFIER_INTERFACE "org.sailfishos.nextcloud.gallery"

namespace {

QString pathName()
{
    return QString::fromLatin1(NOTIFIER_PATH);
}

QString interfaceName()
{
    return QString::fromLatin1(NOTIFIER_INTERFACE);
}

QDBusMessage createSignal(const char *name)
{
    return QDBusMessage::createSignal(pathName(), interfaceName(), QString::fromLatin1(name));
}

} // namespace

SyncCache::ImageChangeNotifier::ImageChangeNotifier(ImageDatabase *db)
    : QObject(nullptr)
{
    QTimer::singleShot(1, Qt::CoarseTimer, this, [this, db] {
        m_db = db;
        this->connectNotification("dataChanged", "", this, SLOT(_q_dataChanged()));
    });
}

void SyncCache::ImageChangeNotifier::dataChanged()
{
    QDBusMessage message = createSignal("dataChanged");
    QDBusConnection::sessionBus().send(message);
}

bool SyncCache::ImageChangeNotifier::connectNotification(const char *name, const char *signature, QObject *receiver, const char *slot)
{
    static QDBusConnection connection(QDBusConnection::sessionBus());

    if (!connection.isConnected()) {
        qWarning() << "Session Bus is not connected";
        return false;
    }

    if (!connection.connect(QString(),
                            pathName(),
                            interfaceName(),
                            QLatin1String(name),
                            QLatin1String(signature),
                            receiver,
                            slot)) {
        qWarning() << "Unable to connect DBUS signal:" << name;
        return false;
    }

    return true;
}

void SyncCache::ImageChangeNotifier::_q_dataChanged()
{
    if (m_db) {
        m_db->dataChanged();
    }
}
