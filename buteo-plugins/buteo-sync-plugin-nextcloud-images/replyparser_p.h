/*
 * SPDX-FileCopyrightText: 2019 Open Mobile Platform LLC.
 * SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
 * SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NEXTCLOUD_IMAGES_REPLYPARSER_P_H
#define NEXTCLOUD_IMAGES_REPLYPARSER_P_H

#include <QObject>
#include <QList>

#include "synccacheimages.h"
#include "networkreplyparser_p.h"

class Syncer;
class ReplyParser
{
public:
    struct GalleryMetadata {
        SyncCache::Album album;
        QVector<SyncCache::Photo> photos;
        QVector<SyncCache::Album> subAlbums;
    };

    static GalleryMetadata galleryMetadataFromResources(Syncer *imageSyncer,
                                                        const QString &rootPath,
                                                        const QString &queriedAlbumPath,
                                                        const QList<NetworkReplyParser::Resource> &resources);
};

Q_DECLARE_METATYPE(ReplyParser::GalleryMetadata)

#endif // NEXTCLOUD_IMAGES_REPLYPARSER_P_H

