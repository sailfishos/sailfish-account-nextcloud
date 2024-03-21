/****************************************************************************************
**
** Copyright (c) 2019 - 2020 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Gallery 1.0
import Sailfish.FileManager 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0

Page {
    id: root

    property alias accountId: photoModel.accountId
    property alias userId: photoModel.userId
    property alias albumId: photoModel.albumId
    property string albumName

    NextcloudPhotoModel {
        id: photoModel

        imageCache: NextcloudImageCache
    }

    SilicaListView {
        anchors.fill: parent
        model: photoModel

        header: PageHeader {
            title: {
                if (albumName.length > 0) {
                    var lastSlash = albumName.lastIndexOf('/')
                    return lastSlash >= 0 ? albumName.substring(lastSlash + 1) : albumName
                }
                //: Heading for Nextcloud photos
                //% "Nextcloud"
                return qsTrId("jolla_gallery_nextcloud-la-nextcloud")
            }
        }

        delegate: BackgroundItem {
            id: photoDelegate

            width: parent.width
            height: fileItem.height

            onClicked: {
                var props = {
                    "imageModel": photoModel,
                    "currentIndex": model.index
                }
                pageStack.push(Qt.resolvedUrl("NextcloudFullscreenPhotoPage.qml"), props)
            }

            FileItem {
                id: fileItem

                fileName: model.fileName
                mimeType: model.fileType
                size: model.fileSize
                isDir: false
                created: model.createdTimestamp
                modified: model.modifiedTimestamp

                icon {
                    source: thumbDownloader.status === NextcloudImageDownloader.Ready
                            ? thumbDownloader.imagePath
                            : Theme.iconForMimeType(model.fileType)
                    width: thumbDownloader.status === NextcloudImageDownloader.Ready
                           ? Theme.itemSizeMedium
                           : undefined
                    height: icon.width
                    sourceSize.width: icon.width
                    sourceSize.height: icon.width
                    clip: thumbDownloader.status === NextcloudImageDownloader.Ready
                    fillMode: Image.PreserveAspectCrop
                    highlighted: thumbDownloader.status !== NextcloudImageDownloader.Ready && photoDelegate.highlighted
                    opacity: thumbDownloader.status === NextcloudImageDownloader.Ready && photoDelegate.highlighted ? Theme.opacityHigh : 1
                }
            }

            NextcloudImageDownloader {
                id: thumbDownloader

                imageCache: NextcloudImageCache
                downloadThumbnail: true
                accountId: model.accountId
                userId: model.userId
                albumId: model.albumId
                photoId: model.photoId
            }
        }
    }
}
