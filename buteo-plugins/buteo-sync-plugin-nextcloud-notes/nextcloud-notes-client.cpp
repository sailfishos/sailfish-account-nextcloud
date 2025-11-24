/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>

#include <notesclient.h>

#include "authenticator.h"
#include "syncer.h"

namespace
{
    Buteo::SyncProfile::SyncDirection direction(const QString &value)
    {
        if (value == QStringLiteral("two_ways")) {
            return Buteo::SyncProfile::SYNC_DIRECTION_TWO_WAY;
        } else if (value == QStringLiteral("up_only")) {
            return Buteo::SyncProfile::SYNC_DIRECTION_TO_REMOTE;
        } else if (value == QStringLiteral("down_only")) {
            return Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE;
        }
        return Buteo::SyncProfile::SYNC_DIRECTION_UNDEFINED;
    }

    Buteo::SyncProfile::ConflictResolutionPolicy policy(const QString &value)
    {
        if (value == QStringLiteral("prefer_local")) {
            return Buteo::SyncProfile::CR_POLICY_PREFER_LOCAL_CHANGES;
        } else if (value == QStringLiteral("prefer_server")) {
            return Buteo::SyncProfile::CR_POLICY_PREFER_REMOTE_CHANGES;
        }
        return Buteo::SyncProfile::CR_POLICY_UNDEFINED;
    }

    void printNote(const NextCloud::Note &note)
    {
        qInfo() << "  - id:" << note.id();
        qInfo() << "    etag:" << note.etag();
        qInfo() << "    readonly:" << (note.readOnly() ? "yes" : "no");
        qInfo() << "    content:" << note.content();
        qInfo() << "    title:" << note.title();
        qInfo() << "    category:" << note.category();
        qInfo() << "    favorite:" << (note.favorite() ? "yes" : "no");
        qInfo() << "    modified:" << note.modified();
    }
}

class NextCloudNotesClient : public QCoreApplication
{
public:
    NextCloudNotesClient(int argc, char *argv[])
        : QCoreApplication(argc, argv)
    {
        setApplicationName("nextcloud-notes-client");

        mParser.setApplicationDescription("Command line tool to sync with NextCloud notes.");
        mParser.addHelpOption();

        mParser.addOption(QCommandLineOption(QStringList() << "s" << "server",
                                             "server address.", "server"));
        mParser.addOption(QCommandLineOption(QStringList() << "u" << "user",
                                             "authenticate by username.", "login"));
        mParser.addOption(QCommandLineOption(QStringList() << "P" << "password",
                                             "authenticate with a password.", "passwd"));
        mParser.addOption(QCommandLineOption(QStringList() << "a" << "account-id",
                                             "account id.", "id"));

        mParser.addOption(QCommandLineOption(QStringList() << "sync",
                                             "sync local notes with the server, using account id."));
        mParser.addOption(QCommandLineOption(QStringList() << "d" << "direction",
                                             "sync direction.", "{two_ways, up_only, down_only}"));
        mParser.addOption(QCommandLineOption(QStringList() << "p" << "policy",
                                             "conflict resolution policy.", "{prefer_server, prefer_local, pass}"));

        mParser.addOption(QCommandLineOption(QStringList() << "list-notes", "list notes."));
        mParser.addOption(QCommandLineOption(QStringList() << "show-note", "download note by id.", "id"));
        mParser.addOption(QCommandLineOption(QStringList() << "delete-note", "delete note by id.", "id"));
        mParser.addOption(QCommandLineOption(QStringList() << "push-note", "send note on server from file.", "file"));
        mParser.addOption(QCommandLineOption(QStringList() << "c" << "category",
                                             "a note category.", "category"));
        mParser.addOption(QCommandLineOption(QStringList() << "i" << "id",
                                             "a note id.", "id"));
        mParser.addOption(QCommandLineOption(QStringList() << "e" << "etag",
                                             "a note etag.", "etag"));

        mParser.process(*this);

        QTimer::singleShot(0, this, [this] () {
                                        start();
                                    });
    }

    void start()
    {
        if (mParser.isSet("sync")) {
            if (!mParser.isSet("a")) {
                qWarning() << "option -a, --account-id is mandatory when using the --sync action.";
                mParser.showHelp(1);
            }
            Authenticator *auth = new Authenticator(mParser.value("a"), this);
            if (!auth->isValid()) {
                exit(1);
                return;
            }
            NotesSyncer *syncer = new NotesSyncer(auth->serverAddress(), mParser.value("a"),
                                                  QStringLiteral("nextcloud-notes"), this);
            connect(auth, &Authenticator::authLoginAvailable,
                    syncer, &NotesSyncer::setAuthLogin);
            connect(auth, &Authenticator::errorOccurred, this,
                    [this] (const QString &message) {
                        qWarning() << message;
                        exit(1);
                        return;
                    });
            connect(auth, &Authenticator::finished,
                    this, [this, auth, syncer] () {
                              auth->deleteLater();
                              syncer->sync(direction(mParser.value("d")),
                                           policy(mParser.value("p")));
                          });
            connect(syncer, &NotesSyncer::syncFinished, this,
                    [this, syncer] (Buteo::SyncResults results) {
                        syncer->deleteLater();
                        qInfo() << "sync results:";
                        qInfo() << "  major code:" << results.majorCode();
                        qInfo() << "  minor code:" << results.minorCode();
                        qInfo() << "  sync date:" << results.syncTime();
                        qInfo() << "  targets:";
                        for (const Buteo::TargetResults &result : results.targetResults()) {
                            qInfo().noquote() << QString::fromLatin1("  - %1:").arg(result.targetName());
                            qInfo().noquote() << QString::fromLatin1("    local changes:"
                                                                     " {additions: %1"
                                                                     ", modifications: %2"
                                                                     ", removals: %3}").arg
                                (QString::number(result.localItems().added),
                                 QString::number(result.localItems().modified),
                                 QString::number(result.localItems().deleted));
                            qInfo().noquote() << QString::fromLatin1("    server changes:"
                                                                     " {additions: %1"
                                                                     ", modifications: %2"
                                                                     ", removals: %3}").arg
                                (QString::number(result.remoteItems().added),
                                 QString::number(result.remoteItems().modified),
                                 QString::number(result.remoteItems().deleted));
                        }
                        exit(results.majorCode());
                    });
            if (!auth->start()) {
                qWarning() << "cannot start authentication session.";
                exit(1);
                return;
            }
        } else {
            NextCloud::NotesClient *client = new NextCloud::NotesClient(mParser.value("s"), this);
            client->setAuthLogin(mParser.value("u"), mParser.value("P"));
            connect(client, &NextCloud::NotesClient::errorOccurred,
                    this, &NextCloudNotesClient::onErrorOccurred);

            if (mParser.isSet("list-notes")) {
                connect(client, &NextCloud::NotesClient::notesRetrieved,
                        this, &NextCloudNotesClient::onNotesRetrieved);
                client->requestNotes(mParser.value("c"));
            } else if (mParser.isSet("show-note")) {
                connect(client, &NextCloud::NotesClient::noteRetrieved,
                        this, &NextCloudNotesClient::onNote);
                client->requestNote(NextCloud::Note::noteId(mParser.value("show-note")),
                                    mParser.value("e").toUtf8());
            } else if (mParser.isSet("delete-note")) {
                connect(client, &NextCloud::NotesClient::noteDeleted,
                        this, &NextCloudNotesClient::onNoteDeleted);
                client->requestNote(NextCloud::Note::noteId(mParser.value("delete-note")));
            } else if (mParser.isSet("push-note")) {
                connect(client, &NextCloud::NotesClient::noteCreated,
                        this, &NextCloudNotesClient::onNotePushed);
                connect(client, &NextCloud::NotesClient::noteUpdated,
                        this, &NextCloudNotesClient::onNotePushed);
                QString error;
                NextCloud::Note note = NextCloud::Note::fromFilePath(mParser.value("push-note"),
                                                                     mParser.value("c"), false, &error);
                if (error.isEmpty()) {
                    if (mParser.isSet("i")) {
                        client->requestPushNote(note.withSyncData(NextCloud::Note::noteId(mParser.value("i")),
                                                                  mParser.value("e").toUtf8(), note));
                    } else {
                        client->requestPushNote(note);
                    }
                } else {
                    onErrorOccurred(0, error);
                }
            } else {
                mParser.showHelp();
            }
        }
    }

    void onErrorOccurred(NextCloud::NotesClient::RequestId id, const QString &error)
    {
        qWarning() << "Request failed!";
        qInfo() << "request failure:";
        qInfo() << "  id:" << id;
        qInfo() << "  error:" << error;
        exit(1);
    }

    void onNotesRetrieved(NextCloud::NotesClient::RequestId id,
                          const QByteArray &etag,
                          const QList<NextCloud::Note> &notes,
                          const QDateTime &lastModified)
    {
        qInfo() << "list-notes:";
        qInfo() << "  request id:" << id;
        qInfo() << "  etag:" << etag;
        qInfo() << "  last modified date:" << lastModified;
        for (const NextCloud::Note &note : notes) {
            printNote(note);
        }
        exit(0);
    }

    void onNote(NextCloud::NotesClient::RequestId id,
                const NextCloud::Note &note)
    {
        qInfo() << "show-note:";
        qInfo() << "  request id:" << id;
        if (note.etag() == mParser.value("e")) {
            // Note unchanged.
            qInfo() << "  - id:" << note.id();
            qInfo() << "    etag:" << note.etag();
        } else {
            printNote(note);
        }
        exit(0);
    }

    void onNotePushed(NextCloud::NotesClient::RequestId id,
                      const NextCloud::Note &note)
    {
        qInfo() << "push-note:";
        qInfo() << "  request id:" << id;
        printNote(note);
        exit(0);
    }

    void onNoteDeleted(NextCloud::NotesClient::RequestId id,
                       NextCloud::Note::Id noteId)
    {
        qInfo() << "delete-note:";
        qInfo() << "  request id:" << id;
        qInfo() << "  note id:" << noteId;
        exit(0);
    }

    QCommandLineParser mParser;
    NextCloud::NotesClient *mNotesClient;
};

int main(int argc, char *argv[])
{
    return NextCloudNotesClient(argc, argv).exec();
}
