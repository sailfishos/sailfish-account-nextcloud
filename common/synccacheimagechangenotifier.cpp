// SPDX-FileCopyrightText: 2020 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2020 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

// modified version of BSD-licensed code copied from qtcontacts-sqlite

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

bool SyncCache::ImageChangeNotifier::connectNotification(const char *name, const char *signature,
                                                         QObject *receiver, const char *slot)
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
