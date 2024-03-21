/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_EVENTSVIEW_EVENTIMAGEDOWNLOADER_H
#define NEXTCLOUD_EVENTSVIEW_EVENTIMAGEDOWNLOADER_H

#include "eventcache.h"

#include <QtQml/QQmlParserStatus>
#include <QtCore/QUrl>

class NextcloudEventImageDownloader : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(SyncCache::EventCache* eventCache READ eventCache WRITE setEventCache NOTIFY eventCacheChanged)
    Q_PROPERTY(int accountId READ accountId WRITE setAccountId NOTIFY accountIdChanged)
    Q_PROPERTY(QString eventId READ eventId WRITE setEventId NOTIFY eventIdChanged)
    Q_PROPERTY(QUrl imagePath READ imagePath NOTIFY imagePathChanged)

public:
    explicit NextcloudEventImageDownloader(QObject *parent = nullptr);

    // QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    SyncCache::EventCache *eventCache() const;
    void setEventCache(SyncCache::EventCache *cache);

    int accountId() const;
    void setAccountId(int id);

    QString eventId() const;
    void setEventId(const QString &eventId);

    QUrl imagePath() const;

Q_SIGNALS:
    void eventCacheChanged();
    void accountIdChanged();
    void eventIdChanged();
    void imagePathChanged();

private:
    void loadImage();

    bool m_deferLoad;
    SyncCache::EventCache *m_eventCache;
    int m_accountId;
    QString m_eventId;
    QUrl m_imagePath;
    int m_idempToken;
};

#endif // NEXTCLOUD_EVENTSVIEW_EVENTIMAGEDOWNLOADER_H
