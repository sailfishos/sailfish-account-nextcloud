/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SYNCER_H
#define SYNCER_H

#include <QObject>

#include <SyncProfile.h>
#include <SyncResults.h>
#include <TargetResults.h>

#include <notesclient.h>
#include "mkcalbackend.h"

class NotesSyncer : public QObject
{
    Q_OBJECT
public:
    NotesSyncer(const QString &serverAddress,
                const QString &accountId, const QString &pluginName,
                QObject *parent = nullptr);
    ~NotesSyncer();

    void setAuthLogin(const QString &username, const QString &password);

    void sync(Buteo::SyncProfile::SyncDirection direction,
              Buteo::SyncProfile::ConflictResolutionPolicy policy);

    void abortSync();

    bool cleanup();

signals:
    void syncFinished(Buteo::SyncResults results);

private:
    void onErrorOccurred(NextCloud::NotesClient::RequestId id,
                         const QString &message);
    void onNoteCreated(NextCloud::NotesClient::RequestId requestId,
                       const NextCloud::Note &note);
    void onNoteUpdated(NextCloud::NotesClient::RequestId requestId,
                       const NextCloud::Note &note);
    void onNoteDeleted(NextCloud::NotesClient::RequestId requestId,
                       NextCloud::Note::Id id);
    void requestFinished();

    void computeDeltaAndUpdate(const QByteArray &etag,
                               const QList<NextCloud::Note> &notes,
                               const QDateTime &lastModified,
                               Buteo::SyncProfile::SyncDirection direction,
                               Buteo::SyncProfile::ConflictResolutionPolicy policy);

    NextCloud::NotesClient m_notesClient;
    mKCalBackend m_mKCalBackend;
    bool m_syncing = false;
    NextCloud::NotesClient::RequestId m_notesRetrievalRequestId = 0;
    Buteo::TargetResults m_results;
    Buteo::SyncResults::MinorCode m_errorCode = Buteo::SyncResults::NO_ERROR;

    QMap<NextCloud::NotesClient::RequestId, QString> m_creationRequests;
    QMap<NextCloud::NotesClient::RequestId, NextCloud::Note::Id> m_updateRequests;
    QMap<NextCloud::NotesClient::RequestId, NextCloud::Note::Id> m_deletionRequests;

    QList<NextCloud::Note> m_localInsertions;
    QStringList m_localDeletions;
    QList<NextCloud::Note::Id> m_localPurges;
};

#endif
