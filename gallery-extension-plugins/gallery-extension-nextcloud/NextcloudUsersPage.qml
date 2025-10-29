// SPDX-FileCopyrightText: 2019 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0

Page {
    id: root

    allowedOrientations: window.allowedOrientations

    property alias model: view.model
    property string title

    SilicaListView {
        id: view

        anchors.fill: parent
        header: PageHeader { title: root.title }

        delegate: BackgroundItem {
            width: parent.width
            height: dirItem.height

            onClicked: {
                pageStack.animatorPush(Qt.resolvedUrl("NextcloudAlbumsPage.qml"),
                                       { "model": nextcloudAlbums })
            }

            NextcloudDirectoryItem {
                id: dirItem

                title: model.displayName
                countText: photoModel.count
                icon.source: "image://theme/icon-m-file-folder-nextcloud"
            }

            NextcloudAlbumModel {
                id: nextcloudAlbums

                imageCache: NextcloudImageCache
                accountId: model.accountId
                userId: model.userId
            }

            NextcloudPhotoModel {
                id: photoModel

                imageCache: NextcloudImageCache
                accountId: model.accountId
                userId: model.userId
            }
        }
    }
}
