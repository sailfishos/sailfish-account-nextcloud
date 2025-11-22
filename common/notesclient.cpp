/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "notesclient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QMimeDatabase>
#include <QFile>
#include <QFileInfo>

NextCloud::Note::Note()
{
}

NextCloud::Note::Note(const QString &title, const QString &content,
                      const QString &category, bool favorite, const QDateTime &modified)
    : mContent(content)
    , mTitle(title)
    , mCategory(category)
    , mFavorite(favorite)
    , mModified(modified)
{
}

NextCloud::Note::~Note()
{
}

bool NextCloud::Note::isValid() const
{
    return !mTitle.isEmpty();
}

NextCloud::Note::Id NextCloud::Note::id() const
{
    return mId;
}

QByteArray NextCloud::Note::etag() const
{
    return mEtag;
}

bool NextCloud::Note::readOnly() const
{
    return mReadOnly;
}

QString NextCloud::Note::content() const
{
    return mContent;
}

QString NextCloud::Note::title() const
{
    return mTitle;
}

QString NextCloud::Note::category() const
{
    return mCategory;
}

bool NextCloud::Note::favorite() const
{
    return mFavorite;
}

QDateTime NextCloud::Note::modified() const
{
    return mModified;
}

void NextCloud::Note::setTitle(const QString &title)
{
    mTitle = title;
}

void NextCloud::Note::setContent(const QString &content)
{
    mContent = content;
}

void NextCloud::Note::setCategory(const QString &category)
{
    mCategory = category;
}

void NextCloud::Note::setFavorite(bool favorite)
{
    mFavorite = favorite;
}

NextCloud::Note::Id NextCloud::Note::noteId(const QString &str)
{
    bool ok = false;
    int id = str.toInt(&ok);
    return Id(ok ? id : 0);
}

static QByteArray toJsonValue(bool val)
{
    return val ? "true" : "false";
}

static QByteArray toJsonValue(const QDateTime &dt)
{
    return QByteArray::number((dt.isValid() ? dt.toMSecsSinceEpoch() : QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) / 1000);
}

static QByteArray toJsonValue(const QByteArray &data)
{
    return QByteArray(data).replace('\n', "\\n");
}

QByteArray NextCloud::Note::toJson() const
{
    QByteArray json = "{\n";
    json += "  \"title\": \"" + mTitle.toUtf8() + "\",\n";
    json += "  \"content\": \"" + toJsonValue(mContent.toUtf8()) + "\",\n";
    json += "  \"category\": \"" + mCategory.toUtf8() + "\",\n";
    json += "  \"favorite\": \"" + toJsonValue(mFavorite) + "\",\n";
    json += "  \"modified\": \"" + toJsonValue(mModified) + "\"";
    json += "\n}";
    return json;
}

NextCloud::Note NextCloud::Note::withSyncData(Id id, const QByteArray &etag, const Note &note)
{
    Note syncedNote(note);
    syncedNote.mId = id;
    syncedNote.mEtag = etag;
    return syncedNote;
}

NextCloud::Note NextCloud::Note::fromFilePath(const QString &filePath, const QString &category,
                                              bool favorite, QString *error)
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QString::fromLatin1("cannot open file '%1'.").arg(filePath);
        return Note();
    }

    QFileInfo info(filePath);

    return Note(info.completeBaseName(), file.readAll(), category, favorite, info.lastModified());
}

NextCloud::Note NextCloud::Note::fromJsonObject(const QJsonObject &obj, QString *error)
{
    Note note;
    QJsonObject::ConstIterator it;
    for (it = obj.constBegin(); it != obj.constEnd(); it++) {
        if (it.key() == QStringLiteral("id")) {
            if (!it.value().isDouble()) {
                if (error)
                    *error = QStringLiteral("unknown return data, id must be an int.");
                break;
            }
            note.mId = it.value().toInt(-1);
        } else if (it.key() == QStringLiteral("etag")) {
            if (!it.value().isString()) {
                if (error)
                    *error = QStringLiteral("unknown return data, etag must be a string.");
                break;
            }
            note.mEtag = "\"" + it.value().toString().toUtf8() + "\"";
        } else if (it.key() == QStringLiteral("readonly")) {
            if (!it.value().isBool()) {
                if (error)
                    *error = QStringLiteral("unknown return data, readonly must be a boolean.");
                break;
            }
            note.mReadOnly = it.value().toBool();
        } else if (it.key() == QStringLiteral("content")) {
            if (!it.value().isString()) {
                if (error)
                    *error = QStringLiteral("unknown return data, content must be a string.");
                break;
            }
            note.mContent = it.value().toString();
        } else if (it.key() == QStringLiteral("title")) {
            if (!it.value().isString()) {
                if (error)
                    *error = QStringLiteral("unknown return data, title must be a string.");
                break;
            }
            note.mTitle = it.value().toString();
        } else if (it.key() == QStringLiteral("category")) {
            if (!it.value().isString()) {
                if (error)
                    *error = QStringLiteral("unknown return data, category must be a string.");
                break;
            }
            note.mCategory = it.value().toString();
        } else if (it.key() == QStringLiteral("favorite")) {
            if (!it.value().isBool()) {
                if (error)
                    *error = QStringLiteral("unknown return data, favorite must be a boolean.");
                break;
            }
            note.mFavorite = it.value().toBool();
        } else if (it.key() == QStringLiteral("modified")) {
            if (!it.value().isDouble()) {
                if (error)
                    *error = QStringLiteral("unknown return data, modified must be an int.");
                break;
            }
            qint64 secs = it.value().toInt(0);
            note.mModified = QDateTime::fromMSecsSinceEpoch(secs * 1000, Qt::UTC);
        }
    }
    return note;
}

NextCloud::Note NextCloud::Note::fromJsonData(const QByteArray &data, QString *error)
{
    QJsonParseError parserError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parserError);

    if (doc.isNull()) {
        if (error)
            *error = parserError.errorString();
        return Note();
    }

    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("unknown return data, object awaited.");
        return Note();
    }

    return fromJsonObject(doc.object(), error);
}

QList<NextCloud::Note> NextCloud::Note::listFromJsonData(const QByteArray &data, QString *error)
{
    QJsonParseError parserError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parserError);

    if (doc.isNull()) {
        if (error)
            *error = parserError.errorString();
        return QList<Note>();
    }

    if (!doc.isArray()) {
        if (error)
            *error = QStringLiteral("unknown return data, array awaited.");
        return QList<Note>();
    }

    QList<Note> notes;
    for (const QJsonValue value : doc.array()) {
        if (!value.isObject()) {
            if (error)
                *error = QStringLiteral("unknown return data, object awaited.");
            continue;
        }
        const Note note = fromJsonObject(value.toObject(), error);
        if (note.mId > 0)
            notes.append(note);
    }
    return notes;
}

NextCloud::NotesClient::NotesClient(const QString &serverAddress, QObject *parent)
    : QObject(parent)
{
    if (serverAddress.startsWith(QStringLiteral("https://"))
        || serverAddress.startsWith(QStringLiteral("http://"))) {
        m_accessPoint = QUrl(serverAddress);
    } else {
        m_accessPoint.setScheme(QStringLiteral("https"));
        m_accessPoint.setHost(serverAddress);
        if (serverAddress.isEmpty())
            qWarning() << "invalid empty server address.";
    }
    m_accessPoint.setPath(QStringLiteral("/index.php/apps/notes/api/v1"));
}

NextCloud::NotesClient::~NotesClient()
{
}

void NextCloud::NotesClient::setAuthLogin(const QString &username, const QString &password)
{
    m_accessPoint.setUserName(username);
    m_accessPoint.setPassword(password);
}

void NextCloud::NotesClient::setRequestAuthentication(QNetworkRequest *request)
{
    request->setRawHeader(QString("Authorization").toLatin1(),
                          QByteArray("Basic ") +
                          QString::fromLatin1("%1:%2").arg(m_accessPoint.userName(),
                                                           m_accessPoint.password()).toLatin1().toBase64());
}

NextCloud::NotesClient::RequestId NextCloud::NotesClient::requestNotes(const QString &category,
                                                                       const QByteArray &etag,
                                                                       const QDateTime &pruneBefore)
{
    QUrl url(m_accessPoint);
    url.setPath(m_accessPoint.path() + QStringLiteral("/notes"));
    QUrlQuery query;
    if (!category.isEmpty())
        query.addQueryItem(QStringLiteral("category"), category);
    if (pruneBefore.isValid())
        query.addQueryItem(QStringLiteral("pruneBefore"), QString::number(pruneBefore.toMSecsSinceEpoch() / 1000));
    url.setQuery(query);
    QNetworkRequest request;
    request.setUrl(url);
    if (!etag.isEmpty()) {
        // request.setHeader(QNetworkRequest::IfNoneMatchHeader, etag);
        request.setRawHeader("If-None-Match", etag);
    }
    setRequestAuthentication(&request);

    RequestId requestId = ++m_requestId;
    QNetworkReply *reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId] () {
        reply->deleteLater();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QByteArray etag;
        QList<Note> notes;
        QDateTime lastModified;
        QString error;
        if (reply->error() == QNetworkReply::NoError) {
            // etag = reply->header(QNetworkRequest::ETagHeader).toByteArray();
            etag = reply->rawHeader("ETag");
            lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();

            if (httpStatus == 200) {
                const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().split(';').first();
                QMimeType mimeType = QMimeDatabase().mimeTypeForName(contentType);
                if (mimeType.inherits(QStringLiteral("application/json"))) {
                    notes = Note::listFromJsonData(reply->readAll(), &error);
                } else {
                    error = QString::fromLatin1("The GET /notes operation expect data in Json format, got: %1").arg(contentType);
                }
            }
        } else {
            qWarning() << "request error, reply content:" << reply->readAll();
            error = QString::fromLatin1("The GET /notes operation failed with HTTP code: %1").arg(httpStatus);
        }

        if (error.isEmpty()) {
            emit notesRetrieved(requestId, etag, notes, lastModified);
        } else {
            emit errorOccurred(requestId, error);
        }
    });
    return requestId;
}

NextCloud::NotesClient::RequestId NextCloud::NotesClient::requestNoteEtags(const QString &category,
                                                                           const QByteArray &etag)
{
    QUrl url(m_accessPoint);
    url.setPath(m_accessPoint.path() + QStringLiteral("/notes"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("exclude"), QStringLiteral("readonly,content,title,category,favorite,modified"));
    if (!category.isEmpty())
        query.addQueryItem(QStringLiteral("category"), category);
    url.setQuery(query);
    QNetworkRequest request;
    request.setUrl(url);
    if (!etag.isEmpty()) {
        // request.setHeader(QNetworkRequest::IfNoneMatchHeader, etag);
        request.setRawHeader("If-None-Match", etag);
    }
    setRequestAuthentication(&request);
    
    RequestId requestId = ++m_requestId;
    QNetworkReply *reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId] () {
        reply->deleteLater();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QByteArray etag;
        QList<Note> notes;
        QString error;
        if (reply->error() == QNetworkReply::NoError) {
            // etag = reply->header(QNetworkRequest::ETagHeader).toByteArray();
            etag = reply->rawHeader("ETag");

            if (httpStatus == 200) {
                const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().split(';').first();
                QMimeType mimeType = QMimeDatabase().mimeTypeForName(contentType);
                if (mimeType.inherits(QStringLiteral("application/json"))) {
                    notes = Note::listFromJsonData(reply->readAll(), &error);
                } else {
                    error = QString::fromLatin1("The GET /notes operation expect data in Json format, got: %1").arg(contentType);
                }
            }
        } else {
            qWarning() << "request error, reply content:" << reply->readAll();
            error = QString::fromLatin1("The GET /notes operation failed with HTTP code: %1").arg(httpStatus);
        }

        if (error.isEmpty()) {
            QMap<Note::Id, QByteArray> noteEtags;
            for (const Note &note : const_cast<const QList<Note>&>(notes)) {
                noteEtags.insert(note.id(), note.etag());
            }
            emit noteEtagsRetrieved(requestId, etag, noteEtags);
        } else {
            emit errorOccurred(requestId, error);
        }
    });
    return requestId;
}

NextCloud::NotesClient::RequestId NextCloud::NotesClient::requestNote(Note::Id id, const QByteArray &etag)
{
    QUrl url(m_accessPoint);
    url.setPath(m_accessPoint.path() + QString::fromLatin1("/notes/%1").arg(id));
    QNetworkRequest request;
    request.setUrl(url);
    if (!etag.isEmpty()) {
        // request.setHeader(QNetworkRequest::IfNoneMatchHeader, etag);
        request.setRawHeader("If-None-Match", etag);
    }
    setRequestAuthentication(&request);
    
    RequestId requestId = ++m_requestId;
    QNetworkReply *reply = m_networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, id, etag, requestId] () {
        reply->deleteLater();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        Note note;
        QString error;
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray newEtag = reply->rawHeader("ETag");
            // etag = reply->header(QNetworkRequest::ETagHeader).toByteArray();

            if (httpStatus == 200) {
                const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().split(';').first();
                QMimeType mimeType = QMimeDatabase().mimeTypeForName(contentType);
                if (mimeType.inherits(QStringLiteral("application/json"))) {
                    note = Note::fromJsonData(reply->readAll(), &error);
                } else {
                    error = QString::fromLatin1("The GET /notes/%id operation expect data in Json format, got: %1").arg(contentType);
                }
            } else if (etag == newEtag) {
                note = Note::withSyncData(id, etag, note);
            }
        } else {
            qWarning() << "request error, reply content:" << reply->readAll();
            error = QString::fromLatin1("The GET /notes/%id operation failed with HTTP code: %1").arg(httpStatus);
        }

        if (error.isEmpty()) {
            emit noteRetrieved(requestId, note);
        } else {
            emit errorOccurred(requestId, error);
        }
    });
    return requestId;
}

NextCloud::NotesClient::RequestId NextCloud::NotesClient::requestPushNote(const Note &note)
{
    QUrl url(m_accessPoint);
    if (!note.id()) {
        url.setPath(m_accessPoint.path() + QStringLiteral("/notes"));
        QNetworkRequest request;
        request.setUrl(url);
        setRequestAuthentication(&request);
        const QByteArray data = note.toJson();
        request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");

        RequestId requestId = ++m_requestId;
        QNetworkReply *reply = m_networkManager.post(request, data);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, requestId] () {
            reply->deleteLater();
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            QByteArray etag;
            Note note;
            QString error;
            if (reply->error() == QNetworkReply::NoError) {
                // etag = reply->header(QNetworkRequest::ETagHeader).toByteArray();
                etag = reply->rawHeader("ETag");

                if (httpStatus == 200) {
                    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().split(';').first();
                    QMimeType mimeType = QMimeDatabase().mimeTypeForName(contentType);
                    if (mimeType.inherits(QStringLiteral("application/json"))) {
                        note = Note::fromJsonData(reply->readAll(), &error);
                    } else {
                        error = QString::fromLatin1("The POST /notes operation expect data in Json format, got: %1").arg(contentType);
                    }
                }
            } else {
                qWarning() << "request error, reply content:" << reply->readAll();
                error = QString::fromLatin1("The POST /notes operation failed with HTTP code: %1").arg(httpStatus);
            }

            if (error.isEmpty()) {
                emit noteCreated(requestId, note);
            } else {
                emit errorOccurred(requestId, error);
            }
        });
        return requestId;
    } else {
        url.setPath(m_accessPoint.path() + QString::fromLatin1("/notes/%1").arg(note.id()));
        QNetworkRequest request;
        request.setUrl(url);
        if (!note.etag().isEmpty()) {
            // request.setHeader(QNetworkRequest::IfNoneMatchHeader, note.etag());
            request.setRawHeader("If-Match", note.etag());
        }
        setRequestAuthentication(&request);
        const QByteArray data = note.toJson();
        request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    
        RequestId requestId = ++m_requestId;
        QNetworkReply *reply = m_networkManager.put(request, data);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, requestId] () {
            reply->deleteLater();
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            QByteArray etag;
            Note note;
            QString error;
            if (reply->error() == QNetworkReply::NoError) {
                // etag = reply->header(QNetworkRequest::ETagHeader).toByteArray();
                etag = reply->rawHeader("ETag");

                if (httpStatus == 200) {
                    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().split(';').first();
                    QMimeType mimeType = QMimeDatabase().mimeTypeForName(contentType);
                    if (mimeType.inherits(QStringLiteral("application/json"))) {
                        note = Note::fromJsonData(reply->readAll(), &error);
                    } else {
                        error = QString::fromLatin1("The PUT /notes/%id operation expect data in Json format, got: %1").arg(contentType);
                    }
                }
            } else {
                qWarning() << "request error, reply content:" << reply->readAll();
                error = QString::fromLatin1("The PUT /notes/%id operation failed with HTTP code: %1").arg(httpStatus);
            }

            if (error.isEmpty()) {
                emit noteUpdated(requestId, note);
            } else {
                emit errorOccurred(requestId, error);
            }
        });
        return requestId;
    }
}

NextCloud::NotesClient::RequestId NextCloud::NotesClient::requestDeleteNote(Note::Id id)
{
    QUrl url(m_accessPoint);
    url.setPath(m_accessPoint.path() + QString::fromLatin1("/notes/%1").arg(id));
    QNetworkRequest request;
    request.setUrl(url);
    setRequestAuthentication(&request);
    
    RequestId requestId = ++m_requestId;
    QNetworkReply *reply = m_networkManager.deleteResource(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, id, requestId] () {
        reply->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            emit noteDeleted(requestId, id);
        } else {
            qWarning() << "request error, reply content:" << reply->readAll();
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit errorOccurred(requestId, QString::fromLatin1("The DELETE /notes/%id operation failed with HTTP code: %1").arg(httpStatus));
        }
    });
    return requestId;
}
