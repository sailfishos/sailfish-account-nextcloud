/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include "eventimagedownloader.h"
#include "synccacheevents.h"

#include <QtCore/QDebug>
#include <QtQml/QQmlInfo>

NextcloudEventImageDownloader::NextcloudEventImageDownloader(QObject *parent)
    : QObject(parent)
    , m_deferLoad(false)
    , m_eventCache(nullptr)
    , m_accountId(0)
    , m_idempToken(0)
{
}

void NextcloudEventImageDownloader::classBegin()
{
    m_deferLoad = true;
}

void NextcloudEventImageDownloader::componentComplete()
{
    m_deferLoad = false;
    if (m_eventCache) {
        loadImage();
    }
}

SyncCache::EventCache *NextcloudEventImageDownloader::eventCache() const
{
    return m_eventCache;
}

void NextcloudEventImageDownloader::setEventCache(SyncCache::EventCache *cache)
{
    if (m_eventCache == cache) {
        return;
    }

    if (m_eventCache) {
        disconnect(m_eventCache, 0, this, 0);
    }

    m_eventCache = cache;
    emit eventCacheChanged();

    if (!m_deferLoad) {
        loadImage();
    }
}

int NextcloudEventImageDownloader::accountId() const
{
    return m_accountId;
}

void NextcloudEventImageDownloader::setAccountId(int id)
{
    if (m_accountId == id) {
        return;
    }

    m_accountId = id;
    emit accountIdChanged();

    if (!m_deferLoad) {
        loadImage();
    }
}

QString NextcloudEventImageDownloader::eventId() const
{
    return m_eventId;
}
void NextcloudEventImageDownloader::setEventId(const QString &id)
{
    if (m_eventId == id) {
        return;
    }

    m_eventId = id;
    emit eventIdChanged();

    if (!m_deferLoad) {
        loadImage();
    }
}

QUrl NextcloudEventImageDownloader::imagePath() const
{
    return m_imagePath;
}

void NextcloudEventImageDownloader::loadImage()
{
    if (!m_eventCache) {
        qWarning() << "No event cache, cannot load image";
        return;
    }

    m_idempToken = qHash(QStringLiteral("%1|%2").arg(m_accountId).arg(m_eventId));
    QObject *contextObject = new QObject(this);
    connect(m_eventCache, &SyncCache::EventCache::populateEventImageFinished,
            contextObject, [this, contextObject] (int idempToken, const QString &path) {
        if (m_idempToken == idempToken) {
            contextObject->deleteLater();
            const QUrl imagePath = QUrl::fromLocalFile(path);
            if (m_imagePath != imagePath) {
                m_imagePath = imagePath;
                emit imagePathChanged();
            }
        }
    });
    connect(m_eventCache, &SyncCache::EventCache::populateEventImageFailed,
            contextObject, [this, contextObject] (int idempToken, const QString &errorMessage) {
        if (m_idempToken == idempToken) {
            contextObject->deleteLater();
            qWarning() << "NextcloudEventImageDownloader::loadImage: failed:" << errorMessage;
        }
    });
    m_eventCache->populateEventImage(m_idempToken, m_accountId, m_eventId, QNetworkRequest());
}
