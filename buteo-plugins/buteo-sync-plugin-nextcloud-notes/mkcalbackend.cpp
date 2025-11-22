/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mkcalbackend.h"

#include "logging.h"

#include <extendedcalendar.h>

#include <KCalendarCore/Calendar>
#include <KCalendarCore/Journal>

#include <QDir>
#include <QStandardPaths>

namespace {
    const QString NOTEBOOK = QStringLiteral("nextcloud-notes");
    const QByteArray CTAG = QByteArrayLiteral("COLLECTION-SYNC-ETAG");
    const QByteArray APP = QByteArrayLiteral("VOLATILE");
    const QByteArray ETAG = QByteArrayLiteral("SYNC-ETAG");
    const QByteArray FAVORITE = QByteArrayLiteral("FAVORITE");

    NextCloud::Note noteFromJournal(const KCalendarCore::Journal::Ptr &journal)
    {
        return NextCloud::Note(journal->summary(), journal->description(),
                               journal->categories().value(0, QString()),
                               !journal->customProperty(APP, FAVORITE).isEmpty(),
                               journal->lastModified());
    }

    KCalendarCore::Journal::Ptr updateJournalFromNote(KCalendarCore::Journal::Ptr journal,
                                                      const NextCloud::Note &note)
    {
        if (note.id() > 0)
            journal->setUid(mKCalBackend::storeId(note.id()));
        journal->setSummary(note.title());
        journal->setDescription(note.content());
        if (!note.category().isEmpty())
            journal->setCategories(QStringList() << note.category());
        else
            journal->setCategories(QStringList());
        if (note.favorite())
            journal->setCustomProperty(APP, FAVORITE, QStringLiteral("true"));
        else
            journal->removeCustomProperty(APP, FAVORITE);
        if (note.modified().isValid())
            journal->setLastModified(note.modified());
        if (!note.etag().isEmpty())
            journal->setCustomProperty(APP, ETAG, QString::fromUtf8(note.etag()));
        else
            journal->removeCustomProperty(APP, ETAG);
        return journal;
    }

    KCalendarCore::Journal::Ptr journalFromNote(const NextCloud::Note &note)
    {
        return updateJournalFromNote(KCalendarCore::Journal::Ptr(new KCalendarCore::Journal), note);
    }
}

mKCalBackend::mKCalBackend(const QString &accountId, const QString &pluginName)
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::utc()));
    calendar->setUpdateLastModifiedOnChange(false);
    // Use a path that can be read from the note application, inside the jail.
    QString path = QString::fromLatin1("%1/com.jolla/notes").arg(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation));
    QDir dir(path);
    dir.mkpath(QStringLiteral("."));
    m_storage = mKCal::SqliteStorage::Ptr(new mKCal::SqliteStorage(calendar, dir.filePath(QString::fromLatin1("nextcloud-%1-mkcal.db").arg(accountId))));
    if (!m_storage || !m_storage->open()) {
        qCWarning(lcNextcloud) << "cannot open journal storage";
        m_storage.clear();
        return;
    }
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NOTEBOOK);
    if (!notebook) {
        notebook = mKCal::Notebook::Ptr(new mKCal::Notebook(QStringLiteral("Nextcloud notes"), QString()));
        notebook->setUid(NOTEBOOK);
        notebook->setAccount(accountId);
        notebook->setPluginName(pluginName);
        notebook->setEventsAllowed(false);
        notebook->setTodosAllowed(false);
        notebook->setJournalsAllowed(true);
        notebook->setModifiedDate(QDateTime());
        if (!m_storage->addNotebook(notebook)) {
            qCWarning(lcNextcloud) << "cannot add journal notebook";
            m_storage.clear();
            return;
        }
    } else if (!m_storage->loadNotebookIncidences(NOTEBOOK)) {
        qCWarning(lcNextcloud) << "cannot read journals from storage";
        m_storage.clear();
        return;
    }
}

mKCalBackend::~mKCalBackend()
{
}

bool mKCalBackend::removeStorage()
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NOTEBOOK);
    if (!notebook) {
        // Notebook for the journals has already been removed.
        return true;
    }
    bool success = m_storage->deleteNotebook(notebook);
    m_storage.clear();
    return success;
}

QString mKCalBackend::id() const
{
    return NOTEBOOK;
}

bool mKCalBackend::isReady() const
{
    return !m_storage.isNull();
}

QByteArray mKCalBackend::collectionETag() const
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NOTEBOOK);
    if (!notebook) {
        qCWarning(lcNextcloud) << "cannot find journal notebook";
        return QByteArray();
    }
    return notebook->customProperty(CTAG).toUtf8();
}

QDateTime mKCalBackend::lastSyncDate() const
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NOTEBOOK);
    if (!notebook) {
        qCWarning(lcNextcloud) << "cannot find journal notebook";
        return QDateTime();
    }
    return notebook->modifiedDate();
}

void mKCalBackend::setRemoteMetaData(const QByteArray &collectionEtag,
                                     const QDateTime &lastModified)
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NOTEBOOK);
    if (!notebook) {
        qCWarning(lcNextcloud) << "cannot find journal notebook";
        return;
    }
    notebook->setCustomProperty(CTAG, QString::fromUtf8(collectionEtag));
    notebook->setModifiedDate(lastModified);
    if (!m_storage->updateNotebook(notebook)) {
        qCWarning(lcNextcloud) << "cannot update collection etag for notebook.";
    }
}

QByteArray mKCalBackend::localEtag(NextCloud::Note::Id id)
{
    KCalendarCore::Journal::Ptr journal = m_storage->calendar()->journal(storeId(id));
    return journal ? journal->customProperty(APP, ETAG).toUtf8() : QByteArray();
}

QMap<QString, NextCloud::Note> mKCalBackend::localOnlyNotes()
{
    QMap<QString, NextCloud::Note> notes;
    for (const KCalendarCore::Journal::Ptr &journal : m_storage->calendar()->rawJournals()) {
        if (journal->customProperty(APP, ETAG).isEmpty()) {
            notes.insert(journal->uid(), noteFromJournal(journal));
        }
    }
    return notes;
}

QList<NextCloud::Note> mKCalBackend::locallyModifiedNotes()
{
    const QDateTime since = lastSyncDate();
    QList<NextCloud::Note> notes;
    for (const KCalendarCore::Journal::Ptr &journal : m_storage->calendar()->rawJournals()) {
        const QByteArray etag = journal->customProperty(APP, ETAG).toUtf8();
        if (!etag.isEmpty() && journal->lastModified() >= since) {
            qDebug() << journal->lastModified() << since;
            NextCloud::Note::Id id = noteId(journal);
            if (id) {
                notes << NextCloud::Note::withSyncData(id, etag, noteFromJournal(journal));
            } else {
                qCWarning(lcNextcloud) << "cannot convert to a note id from" << journal->uid();
            }
        }
    }
    return notes;
}

QList<QPair<NextCloud::Note::Id, QByteArray>> mKCalBackend::locallyDeletedNotes()
{
    KCalendarCore::Incidence::List deleted;
    if (!m_storage->deletedIncidences(&deleted, QDateTime(), NOTEBOOK)) {
        qCWarning(lcNextcloud) << "cannot list deleted journals";
        return QList<QPair<NextCloud::Note::Id, QByteArray>>();
    }

    QList<QPair<NextCloud::Note::Id, QByteArray>> ids;
    for (const KCalendarCore::Incidence::Ptr &incidence : deleted) {
        NextCloud::Note::Id id = noteId(incidence);
        const QByteArray etag = incidence->customProperty(APP, ETAG).toUtf8();
        if (id && !etag.isEmpty()) {
            ids << QPair<NextCloud::Note::Id, QByteArray>(id, etag);
        }
    }
    return ids;
}

QList<NextCloud::Note::Id> mKCalBackend::syncedNoteIds()
{
    QList<NextCloud::Note::Id> ids;
    for (const KCalendarCore::Journal::Ptr &journal : m_storage->calendar()->rawJournals()) {
        const QByteArray etag = journal->customProperty(APP, ETAG).toUtf8();
        if (!etag.isEmpty()) {
            NextCloud::Note::Id id = noteId(journal);
            if (id) {
                ids.append(id);
            } else {
                qCWarning(lcNextcloud) << "cannot convert to a note id from" << journal->uid();
            }
        }
    }
    return ids;
}

bool mKCalBackend::applyChanges(const QList<NextCloud::Note> &noteInsertions,
                                const QStringList &noteDeletions,
                                const QList<NextCloud::Note::Id> &notePurges)
{
    const QDateTime syncDate = lastSyncDate();
    for (const QString &uid : noteDeletions) {
        if (m_storage->load(uid)) {
            KCalendarCore::Journal::Ptr journal = m_storage->calendar()->journal(uid);
            if (journal
                && (journal->lastModified() > syncDate
                    || !m_storage->calendar()->deleteJournal(journal))) {
                qCWarning(lcNextcloud) << "cannot remove journal from calendar.";
            }
        }
    }
    for (const NextCloud::Note &note : noteInsertions) {
        const QString journalId(storeId(note.id()));
        if (!m_storage->load(journalId)) {
            qCWarning(lcNextcloud) << "cannot load journal from store" << journalId;
        } else {
            KCalendarCore::Journal::Ptr journal = m_storage->calendar()->journal(journalId);
            if (journal) {
                if (journal->lastModified() <= syncDate) {
                    journal->startUpdates();
                    updateJournalFromNote(journal, note);
                    journal->endUpdates();
                } else {
                    qCWarning(lcNextcloud) << "cannot update journal" << journalId
                                           << ", stored version has been modified.";
                }
            } else {
                KCalendarCore::Journal::Ptr journal = journalFromNote(note);
                // Journal requires a DTSTART, but the creation date of the note
                // is not exposed by the NextCloud API. So arbitrarily choose one.
                journal->setDtStart(QDateTime::currentDateTimeUtc());
                if (!m_storage->calendar()->addJournal(journal)
                    || !m_storage->calendar()->setNotebook(journal, NOTEBOOK)) {
                    qCWarning(lcNextcloud) << "cannot add journal to calendar.";
                }
            }
        }
    }
    bool success = true;
    if (!noteInsertions.isEmpty() || !noteDeletions.isEmpty()) {
        if (!m_storage->save(mKCal::ExtendedStorage::PurgeDeleted)) {
            qCWarning(lcNextcloud) << "cannot save local changes.";
            success = false;
        }
    }
    KCalendarCore::Incidence::List purge;
    for (NextCloud::Note::Id id : notePurges) {
        KCalendarCore::Journal::Ptr journal(new KCalendarCore::Journal);
        journal->setUid(storeId(id));
        purge.append(journal);
    }
    if (!purge.isEmpty() && !m_storage->purgeDeletedIncidences(purge, NOTEBOOK)) {
        qCWarning(lcNextcloud) << "cannot purge locally deleted journals.";
        success = false;
    }
    return success;
}

QString mKCalBackend::storeId(NextCloud::Note::Id id)
{
    return QString::number(id);
}

NextCloud::Note::Id mKCalBackend::noteId(const KCalendarCore::Incidence::Ptr &incidence)
{
    return NextCloud::Note::noteId(incidence->uid());
}
