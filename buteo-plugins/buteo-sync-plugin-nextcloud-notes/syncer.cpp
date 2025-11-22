/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "syncer.h"

#include <logging.h>

namespace {
    Buteo::SyncResults::MajorCode toMajorCode(Buteo::SyncResults::MinorCode code)
    {
        if (code == Buteo::SyncResults::NO_ERROR
            || code == Buteo::SyncResults::ITEM_FAILURES) {
            return Buteo::SyncResults::SYNC_RESULT_SUCCESS;
        } else if (code == Buteo::SyncResults::ABORTED) {
            return Buteo::SyncResults::SYNC_RESULT_CANCELLED;
        } else {
            return Buteo::SyncResults::SYNC_RESULT_FAILED;
        }
    }
}

NotesSyncer::NotesSyncer(const QString &serverAddress,
                         const QString &accountId, const QString &pluginName,
                         QObject *parent)
    : QObject(parent)
    , m_notesClient(serverAddress)
    , m_mKCalBackend(accountId, pluginName)
    , m_results(m_mKCalBackend.id())
{
}

NotesSyncer::~NotesSyncer()
{
}

void NotesSyncer::setAuthLogin(const QString &username, const QString &password)
{
    m_notesClient.setAuthLogin(username, password);
}

void NotesSyncer::sync(Buteo::SyncProfile::SyncDirection direction,
                       Buteo::SyncProfile::ConflictResolutionPolicy policy)
{
    if (m_syncing) {
        qCWarning(lcNextcloud) << "sync already running";
        return;
    }

    connect(&m_notesClient, &NextCloud::NotesClient::errorOccurred,
            this, &NotesSyncer::onErrorOccurred);
    connect(&m_notesClient, &NextCloud::NotesClient::notesRetrieved, this,
            [this, direction, policy] (NextCloud::NotesClient::RequestId requestId,
                                       const QByteArray &etag,
                                       const QList<NextCloud::Note> &notes,
                                       const QDateTime &lastModified) {
                if (requestId == m_notesRetrievalRequestId) {
                    computeDeltaAndUpdate(etag, notes, lastModified, direction, policy);
                }
            });
    m_notesRetrievalRequestId
        = m_notesClient.requestNotes(QString(),
                                     m_mKCalBackend.collectionETag(),
                                     m_mKCalBackend.lastSyncDate());
}

void NotesSyncer::onErrorOccurred(NextCloud::NotesClient::RequestId id, const QString &message)
{
    qCWarning(lcNextcloud) << "error on request:" << message;
    if (id == m_notesRetrievalRequestId) {
        // Not listing server note ids and etags is a fatal error;
        disconnect(&m_notesClient, 0, this, 0);
        m_syncing = false;
        if (m_errorCode == Buteo::SyncResults::NO_ERROR)
            m_errorCode = Buteo::SyncResults::CONNECTION_ERROR;
        emit syncFinished(Buteo::SyncResults(QDateTime::currentDateTimeUtc(),
                                             toMajorCode(m_errorCode), m_errorCode));
        return;
    } else if (m_creationRequests.contains(id)) {
        const QString localId = m_creationRequests.take(id);
        m_results.addRemoteDetails(localId, Buteo::TargetResults::ITEM_ADDED,
                                   Buteo::TargetResults::ITEM_OPERATION_FAILED, message);
    } else if (m_updateRequests.contains(id)) {
        NextCloud::Note::Id noteId = m_updateRequests.take(id);
        m_results.addRemoteDetails(mKCalBackend::storeId(noteId), Buteo::TargetResults::ITEM_MODIFIED,
                                   Buteo::TargetResults::ITEM_OPERATION_FAILED, message);
    } else if (m_deletionRequests.contains(id)) {
        NextCloud::Note::Id noteId = m_deletionRequests.take(id);
        m_results.addRemoteDetails(mKCalBackend::storeId(noteId), Buteo::TargetResults::ITEM_DELETED,
                                   Buteo::TargetResults::ITEM_OPERATION_FAILED, message);
    }
    if (m_errorCode == Buteo::SyncResults::NO_ERROR)
        m_errorCode = Buteo::SyncResults::ITEM_FAILURES;
    requestFinished();
}

void NotesSyncer::computeDeltaAndUpdate(const QByteArray &etag,
                                        const QList<NextCloud::Note> &notes,
                                        const QDateTime &lastModified,
                                        Buteo::SyncProfile::SyncDirection direction,
                                        Buteo::SyncProfile::ConflictResolutionPolicy policy)
{
    qCDebug(lcNextcloud) << "got" << notes.count() << "notes from server:";
    QMap<NextCloud::Note::Id, QByteArray> serverEtags;
    for (const NextCloud::Note &note : notes) {
        qCDebug(lcNextcloud) << "- " << note.id() << note.etag();
        serverEtags.insert(note.id(), note.etag());
    }
    bool remoteHasChanged = (etag != m_mKCalBackend.collectionETag());
    qCDebug(lcNextcloud) << "collection etag makes remote changed" << remoteHasChanged;
    qCDebug(lcNextcloud) << "remote collection etag" << etag;
    qCDebug(lcNextcloud) << "cached collection etag" << m_mKCalBackend.collectionETag();

    m_errorCode = Buteo::SyncResults::NO_ERROR;
    m_creationRequests.clear();
    m_updateRequests.clear();
    m_deletionRequests.clear();
    m_localInsertions.clear();
    m_localDeletions.clear();
    m_localPurges.clear();
    if (direction != Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE) {
        connect(&m_notesClient, &NextCloud::NotesClient::noteCreated,
                this, &NotesSyncer::onNoteCreated);
        const QMap<QString, NextCloud::Note> localNotes = m_mKCalBackend.localOnlyNotes();
        qCDebug(lcNextcloud) << "got" << localNotes.count() << "local only notes.";
        QMap<QString, NextCloud::Note>::ConstIterator it;
        for (it = localNotes.constBegin(); it != localNotes.constEnd(); it++) {
            NextCloud::NotesClient::RequestId requestId = m_notesClient.requestPushNote(it.value());
            m_creationRequests.insert(requestId, it.key());
        }

        connect(&m_notesClient, &NextCloud::NotesClient::noteUpdated,
                this, &NotesSyncer::onNoteUpdated);
        for (const NextCloud::Note &note : m_mKCalBackend.locallyModifiedNotes()) {
            bool upload = true;
            if (remoteHasChanged) {
                QMap<NextCloud::Note::Id, QByteArray>::ConstIterator it = serverEtags.find(note.id());
                if (it == serverEtags.constEnd()) {
                    // Conflict: modified locally but erased from server.
                    upload = policy == Buteo::SyncProfile::CR_POLICY_PREFER_LOCAL_CHANGES;
                } else if (!it.value().isEmpty() && it.value() != note.etag()) {
                    // Conflict: modified locally but also on server.
                    upload = policy == Buteo::SyncProfile::CR_POLICY_PREFER_LOCAL_CHANGES;
                }
            }

            if (upload) {
                NextCloud::NotesClient::RequestId requestId = m_notesClient.requestPushNote(note);
                m_updateRequests.insert(requestId, note.id());
            }
            qCDebug(lcNextcloud) << "uploading a locally modified note" << note.id() << note.etag() << upload;
        }

        connect(&m_notesClient, &NextCloud::NotesClient::noteDeleted,
                this, &NotesSyncer::onNoteDeleted);
        for (QPair<NextCloud::Note::Id, QByteArray> deleted : m_mKCalBackend.locallyDeletedNotes()) {
            bool remove = true;
            if (remoteHasChanged) {
                QMap<NextCloud::Note::Id, QByteArray>::ConstIterator it = serverEtags.find(deleted.first);
                if (it == serverEtags.constEnd()) {
                    // Also deleted on server, nothing to do server-side.
                    remove = false;
                    m_localPurges.append(deleted.first);
                } else if (!it.value().isEmpty() && it.value() != deleted.second) {
                    // Conflict: deleted locally but modified on server.
                    remove = policy == Buteo::SyncProfile::CR_POLICY_PREFER_LOCAL_CHANGES;
                }
            }

            if (remove) {
                NextCloud::NotesClient::RequestId requestId = m_notesClient.requestDeleteNote(deleted.first);
                m_deletionRequests.insert(requestId, deleted.first);
            }
            qCDebug(lcNextcloud) << "requesting deletion of a locally deleted note" << deleted.first << deleted.second << remove;
        }
    }
    bool hasConflicts = false;
    if (direction != Buteo::SyncProfile::SYNC_DIRECTION_TO_REMOTE
        && remoteHasChanged) {
        QSet<NextCloud::Note::Id> locallyModifiedIds;
        for (const NextCloud::Note &note : m_mKCalBackend.locallyModifiedNotes()) {
            locallyModifiedIds.insert(note.id());
        }
        QMap<NextCloud::Note::Id, QByteArray> locallyDeletedIds;
        for (const QPair<NextCloud::Note::Id, QByteArray> &item : m_mKCalBackend.locallyDeletedNotes()) {
            locallyDeletedIds.insert(item.first, item.second);
        }
        
        for (const NextCloud::Note &note : notes) {
            const QByteArray localEtag = m_mKCalBackend.localEtag(note.id());
            if (localEtag.isEmpty()) {
                QMap<NextCloud::Note::Id, QByteArray>::ConstIterator it = locallyDeletedIds.find(note.id());
                bool add = it == locallyDeletedIds.constEnd();
                if (it != locallyDeletedIds.constEnd()
                    && it.value() != note.etag()) {
                    // Conflict: deleted locally but modified on server.
                    add = policy == Buteo::SyncProfile::CR_POLICY_PREFER_REMOTE_CHANGES;
                    if (add)
                        m_localPurges.append(note.id());
                }

                if (add) {
                    m_localInsertions.append(note);
                    m_results.addLocalDetails(mKCalBackend::storeId(note.id()),
                                              Buteo::TargetResults::ITEM_ADDED);
                    qCDebug(lcNextcloud) << "store a new note" << note.id();
                } else if (it != locallyDeletedIds.constEnd()
                           && it.value() != note.etag()
                           && policy == Buteo::SyncProfile::CR_POLICY_UNDEFINED) {
                    hasConflicts = true;
                    m_results.addLocalDetails(mKCalBackend::storeId(note.id()),
                                              Buteo::TargetResults::ITEM_ADDED,
                                              Buteo::TargetResults::ITEM_OPERATION_FAILED,
                                              QStringLiteral("unresolved conflict: deleted locally but modified on server."));
                }
            } else if (!note.etag().isEmpty() && localEtag != note.etag()) {
                bool update = true;
                if (locallyModifiedIds.contains(note.id())) {
                    // Conflict: modified both locally and on server.
                    update = policy == Buteo::SyncProfile::CR_POLICY_PREFER_REMOTE_CHANGES;
                }

                if (update) {
                    m_localInsertions.append(note);
                    m_results.addLocalDetails(mKCalBackend::storeId(note.id()),
                                              Buteo::TargetResults::ITEM_MODIFIED);
                    qCDebug(lcNextcloud) << "update a stored note" << note.id();
                } else if (policy == Buteo::SyncProfile::CR_POLICY_UNDEFINED) {
                    hasConflicts = true;
                    m_results.addLocalDetails(mKCalBackend::storeId(note.id()),
                                              Buteo::TargetResults::ITEM_MODIFIED,
                                              Buteo::TargetResults::ITEM_OPERATION_FAILED,
                                              QStringLiteral("unresolved conflict: modified both locally and on server."));
                }
            }
        }

        for (NextCloud::Note::Id id : m_mKCalBackend.syncedNoteIds()) {
            if (!serverEtags.contains(id)) {
                bool remove = true;
                if (locallyModifiedIds.contains(id)) {
                    // Conflict: modified locally but deleted from server.
                    remove = policy == Buteo::SyncProfile::CR_POLICY_PREFER_REMOTE_CHANGES;
                }

                if (remove) {
                    m_localDeletions.append(mKCalBackend::storeId(id));
                    m_results.addLocalDetails(mKCalBackend::storeId(id),
                                              Buteo::TargetResults::ITEM_DELETED);
                    qCDebug(lcNextcloud) << "remove a stored note" << id;
                } else if (policy == Buteo::SyncProfile::CR_POLICY_UNDEFINED) {
                    hasConflicts = true;
                    m_results.addLocalDetails(mKCalBackend::storeId(id),
                                              Buteo::TargetResults::ITEM_DELETED,
                                              Buteo::TargetResults::ITEM_OPERATION_FAILED,
                                              QStringLiteral("unresolved conflict: modified locally but deleted on server."));
                }
            }
        }
    }
    // If there are conflicts on importing changes from the server,
    // we should not save the collection etag since the local copies
    // don't reflect the server states.
    m_mKCalBackend.setRemoteMetaData(hasConflicts ? QByteArray() : etag, lastModified);

    requestFinished();
}

void NotesSyncer::onNoteCreated(NextCloud::NotesClient::RequestId requestId, const NextCloud::Note &note)
{
    const QString localId = m_creationRequests.take(requestId);
    // Note has been successfully added on server.
    m_localDeletions.append(localId);
    m_localInsertions.append(note);
    m_results.addRemoteDetails(mKCalBackend::storeId(note.id()), Buteo::TargetResults::ITEM_ADDED);
    qCDebug(lcNextcloud) << "new note created on server" << note.id();
    requestFinished();
}

void NotesSyncer::onNoteUpdated(NextCloud::NotesClient::RequestId requestId, const NextCloud::Note &note)
{
    m_updateRequests.remove(requestId);
    // Note has been successfully updated on server.
    m_localInsertions.append(note);
    m_results.addRemoteDetails(mKCalBackend::storeId(note.id()), Buteo::TargetResults::ITEM_MODIFIED);
    qCDebug(lcNextcloud) << "note updated on server" << note.id();
    requestFinished();
}

void NotesSyncer::onNoteDeleted(NextCloud::NotesClient::RequestId requestId, NextCloud::Note::Id id)
{
    m_deletionRequests.remove(requestId);
    // Note has been successfully deleted from the server.
    m_localPurges.append(id);
    m_results.addRemoteDetails(mKCalBackend::storeId(id), Buteo::TargetResults::ITEM_DELETED);
    qCDebug(lcNextcloud) << "note removed from server" << id;
    requestFinished();
}

void NotesSyncer::requestFinished()
{
    if (!m_creationRequests.isEmpty()
        || !m_updateRequests.isEmpty()
        || !m_deletionRequests.isEmpty()) {
        return;
    }

    qCDebug(lcNextcloud) << "commiting changes to local store";
    qCDebug(lcNextcloud) << m_localInsertions.count() << "notes to insert or update";
    qCDebug(lcNextcloud) << m_localDeletions.count() << "notes to delete";
    qCDebug(lcNextcloud) << m_localPurges.count() << "notes to purge";
    if (!m_mKCalBackend.applyChanges(m_localInsertions,
                                     m_localDeletions,
                                     m_localPurges)) {
        if (m_errorCode == Buteo::SyncResults::NO_ERROR)
            m_errorCode = Buteo::SyncResults::DATABASE_FAILURE;
        qCWarning(lcNextcloud) << "cannot commit all changes to local storage.";
    }
    Buteo::SyncResults result(m_mKCalBackend.lastSyncDate(),
                              toMajorCode(m_errorCode), m_errorCode);
    result.addTargetResults(m_results);
    emit syncFinished(result);
    disconnect(&m_notesClient, 0, this, 0);
    m_syncing = false;
}

void NotesSyncer::abortSync()
{
    if (m_syncing) {
        m_errorCode = Buteo::SyncResults::ABORTED;
        for (NextCloud::NotesClient::RequestId id : m_creationRequests.keys()
                 + m_updateRequests.keys() + m_deletionRequests.keys()) {
            onErrorOccurred(id, QStringLiteral("sync aborted."));
        }
    } else {
        emit syncFinished(Buteo::SyncResults
                          (QDateTime::currentDateTimeUtc(),
                           Buteo::SyncResults::SYNC_RESULT_CANCELLED,
                           Buteo::SyncResults::ABORTED));
    }
}

bool NotesSyncer::cleanup()
{
    if (!m_mKCalBackend.removeStorage()) {
        qCWarning(lcNextcloud) << "cannot remove local storage for notes.";
        return false;
    }
    return true;
}
