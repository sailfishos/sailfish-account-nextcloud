// SPDX-FileCopyrightText: 2019 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0

MediaSourcePage {
    id: root

    SilicaListView {
        id: view

        anchors.fill: parent
        header: PageHeader {
            title: view.model.userDisplayName || view.model.userId
        }
        cacheBuffer: Screen.height
        model: root.model

        delegate: NextcloudAlbumDelegate {
            accountId: model.accountId
            userId: model.userId
            albumId: model.albumId
            albumName: model.albumName.length > 0
                       ? model.albumName
                       : "Photos" // not translated, this is the non-localized root Nextcloud photos directory
            albumThumbnailPath: model.thumbnailPath
            photoCount: model.photoCount
            usePlaceholderColor: model.albumName.length === 0

            onClicked: {
                var props = {
                    "accountId": accountId,
                    "userId": userId,
                    "albumId": albumId,
                    "albumName": albumName
                }
                pageStack.animatorPush(Qt.resolvedUrl("NextcloudPhotoListPage.qml"), props)
            }
        }

        VerticalScrollDecorator {}
    }
}
