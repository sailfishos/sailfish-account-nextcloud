// SPDX-FileCopyrightText: 2019 - 2020 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2020 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

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

    ImageGridView {
        id: grid

        anchors.fill: parent
        model: photoModel

        dateProperty: "createdTimestamp"

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

        delegate: GridItem {
            id: photoDelegate

            width: grid.cellWidth
            height: grid.cellHeight

            Image {
                id: thumbnail

                anchors.centerIn: parent
                width: parent.width
                height: parent.height

                sourceSize.width: width
                sourceSize.height: width

                fillMode: Image.PreserveAspectCrop
                clip: true
                source: Theme.iconForMimeType(model.fileType)
                opacity: Theme.opacityFaint
                Behavior on opacity { FadeAnimator {} }

                states: [
                    State {
                        name: "loadOk"
                        when: thumbDownloader.status === NextcloudImageDownloader.Ready
                        PropertyChanges {
                            target: thumbnail
                            cache: false
                            source: thumbDownloader.imagePath
                            opacity: 1.0
                        }
                    },
                    State {
                        name: "loadError"
                        when: thumbDownloader.status === NextcloudImageDownloader.Error
                        PropertyChanges {
                            target: errorLabel
                            visible: true
                        }
                        PropertyChanges {
                            target: thumbnail
                            fillMode: Image.Pad
                            clip: false
                            cache: true
                            source: "" //Theme.iconForMimeType(model.fileType) + "?" + Theme.errorColor
                            opacity: 1.0
                        }
                    }
                ]
                Label {
                    id: errorLabel

                    visible: false
                    //: Thumbnail Image loading failed
                    //% "Oops, can't display the thumbnail!"
                    text: qsTrId("jolla-gallery-ambience-la-image-thumbnail-loading-failed")
                    anchors.centerIn: parent
                    width: parent.width - 2 * Theme.paddingMedium
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            onClicked: {
                var props = {
                    "imageModel": photoModel,
                    "currentIndex": model.index
                }
                pageStack.push(Qt.resolvedUrl("NextcloudFullscreenPhotoPage.qml"), props)
                state = "loadOk"
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
