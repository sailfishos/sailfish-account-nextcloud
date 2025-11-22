/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MKCALBACKEND_H
#define MKCALBACKEND_H

#include <notesclient.h>

#include <sqlitestorage.h>

#include <QDateTime>

class mKCalBackend
{
public:
    mKCalBackend(const QString &accountId, const QString &pluginName);
    ~mKCalBackend();

    QString id() const;

    bool isReady() const;

    QByteArray collectionETag() const;
    QDateTime lastSyncDate() const;
    void setRemoteMetaData(const QByteArray &collectionEtag,
                           const QDateTime &lastModified);

    QByteArray localEtag(NextCloud::Note::Id id);
    QMap<QString, NextCloud::Note> localOnlyNotes();
    QList<NextCloud::Note> locallyModifiedNotes();
    QList<QPair<NextCloud::Note::Id, QByteArray>> locallyDeletedNotes();
    QList<NextCloud::Note::Id> syncedNoteIds();

    bool applyChanges(const QList<NextCloud::Note> &noteInsertions,
                      const QStringList &noteDeletions,
                      const QList<NextCloud::Note::Id> &notePurges);

    bool removeStorage();

    static QString storeId(NextCloud::Note::Id id);
private:
    static NextCloud::Note::Id noteId(const KCalendarCore::Incidence::Ptr &incidence);

    mKCal::SqliteStorage::Ptr m_storage;
};

#endif
