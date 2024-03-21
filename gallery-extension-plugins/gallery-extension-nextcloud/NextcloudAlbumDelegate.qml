/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

import QtQuick 2.0
import Sailfish.Silica 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0

BackgroundItem {
    id: root

    property alias accountId: imageDownloader.accountId
    property alias userId: imageDownloader.userId
    property alias albumId: imageDownloader.albumId
    property alias albumName: dirItem.title
    property string albumThumbnailPath
    property int photoCount
    property bool usePlaceholderColor

    width: parent.width
    height: dirItem.height

    NextcloudDirectoryItem {
        id: dirItem

        titleLabel.color: root.usePlaceholderColor
                          ? (highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor)
                          : (highlighted ? Theme.highlightColor : Theme.primaryColor)

        icon {
            source: albumThumbnailPath.length ? albumThumbnailPath : "image://theme/icon-m-file-folder-nextcloud"
            width: icon.sourceSize.width
            height: icon.sourceSize.height
            sourceSize.width: albumThumbnailPath.length ? dirItem.height : icon.paintedWidth
            sourceSize.height: albumThumbnailPath.length ? dirItem.height : icon.paintedHeight
            fillMode: Image.PreserveAspectCrop
            clip: albumThumbnailPath.length > 0
            highlighted: albumThumbnailPath.length === 0 && root.highlighted
            opacity: albumThumbnailPath.length > 0 && highlighted ? Theme.opacityHigh : 1
        }

        countText: root.photoCount
    }

    NextcloudImageDownloader {
        id: imageDownloader

        imageCache: albumThumbnailPath.length === 0
                    ? NextcloudImageCache
                    : null
        downloadThumbnail: true
    }
}
