/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NOTECLIENT_H
#define NOTECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QMap>
#include <QUrl>

class QJsonObject;

namespace NextCloud {
class Note
{
public:
    typedef qint64 Id;
    
    Note();
    Note(const QString &title, const QString &content,
         const QString &category = QString(), bool favorite = false,
         const QDateTime &modified = QDateTime());
    ~Note();

    bool isValid() const;

    Id id() const;
    QByteArray etag() const;
    bool readOnly() const;
    QString content() const;
    QString title() const;
    QString category() const;
    bool favorite() const;
    QDateTime modified() const;

    void setTitle(const QString &title);
    void setContent(const QString &content);
    void setCategory(const QString &category);
    void setFavorite(bool favorite);

    QByteArray toJson() const;

    static Id noteId(const QString &str);
    static Note withSyncData(Id id, const QByteArray &etag, const Note &note);
    static Note fromFilePath(const QString &filePath, const QString &category = QString(),
                             bool favorite = false, QString *error = nullptr);
    static Note fromJsonObject(const QJsonObject &obj, QString *error = nullptr);
    static Note fromJsonData(const QByteArray &data, QString *error = nullptr);
    static QList<Note> listFromJsonData(const QByteArray &data, QString *error = nullptr);

private:
    Id mId = 0;
    QByteArray mEtag;
    bool mReadOnly = false;
    QString mContent;
    QString mTitle;
    QString mCategory;
    bool mFavorite = false;
    QDateTime mModified;
};

class NotesClient : public QObject
{
    Q_OBJECT
public:
    typedef int RequestId;

    NotesClient(const QString &serverAddress, QObject *parent = nullptr);
    ~NotesClient();

    void setAuthLogin(const QString &username, const QString &password);

    RequestId requestNotes(const QString &category = QString(),
                           const QByteArray &etag = QByteArray(),
                           const QDateTime &pruneBefore = QDateTime());
    RequestId requestNoteEtags(const QString &category = QString(),
                               const QByteArray &etag = QByteArray());
    RequestId requestNote(Note::Id id, const QByteArray &etag = QByteArray());

    RequestId requestPushNote(const Note &note);
    RequestId requestDeleteNote(Note::Id id);

signals:
    void errorOccurred(RequestId requestId, const QString &message);
    void notesRetrieved(RequestId requestId, const QByteArray &etag,
                        const QList<Note> &notes, const QDateTime &lastModified);
    void noteEtagsRetrieved(RequestId requestId, const QByteArray &etag,
                            const QMap<Note::Id, QByteArray> &noteEtags);
    void noteRetrieved(RequestId requestId, const Note &note);
    void noteCreated(RequestId requestId, const Note &note);
    void noteUpdated(RequestId requestId, const Note &note);
    void noteDeleted(RequestId requestId, Note::Id id);

private:
    void setRequestAuthentication(QNetworkRequest *request);
    
    QUrl m_accessPoint;

    QNetworkAccessManager m_networkManager;
    RequestId m_requestId = 0;
};
}

#endif
