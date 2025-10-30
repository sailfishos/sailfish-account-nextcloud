/*
 * SPDX-FileCopyrightText: 2020 Open Mobile Platform LLC
 * SPDX-FileCopyrightText: 2020 - 2023 Jolla Ltd.
 * SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// modified version of BSD-licensed code copied from qtcontacts-sqlite

#include "synccacheimages.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>

// This class exists to allow cross-process signalling of database changes.
// It will not be needed once we migrate to service (daemon-based) APIs.

namespace SyncCache {

class ImageChangeNotifier : public QObject
{
    Q_OBJECT

public:
    ImageChangeNotifier(ImageDatabase *db);

    bool connectNotification(const char *name, const char *signature, QObject *receiver, const char *slot);

    // we leak sensitive information if we duplicate the signals exactly
    // e.g. usersDeleted()/usersStored()
    //      albumsDeleted()/albumsStored()
    //      photosDeleted()/photosStored()
    // so instead, provide an opaque dataChanged() signal.
    void dataChanged();

public Q_SLOTS:
    void _q_dataChanged();

private:
    QPointer<ImageDatabase> m_db;
};

} // namespace SyncCache
