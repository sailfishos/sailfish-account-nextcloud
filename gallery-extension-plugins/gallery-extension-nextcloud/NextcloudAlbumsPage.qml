// SPDX-FileCopyrightText: 2019 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0
import org.nemomobile.models 1.0

MediaSourcePage {
    id: root

    property alias searchModel: searchModel
    SearchModel {
        id: searchModel
        sourceModel: root.model
        searchRoles: [ "albumName" ]
        matchType: SearchModel.MatchAnywhere
        caseSensitivity: Qt.CaseInsensitive
    }

    SilicaListView {
        id: view

        anchors.fill: parent

        header: FocusScope {
            id: pageHeader
            property var model: root.searchModel
            readonly property bool active: searchField.text.length > 0

            property string title: root.model.userDisplayName || root.model.userId

            width: view.width
            visible: active || model.count > 0
            implicitHeight: col.height

            Column {
                id: col
                width: parent.width
                PageHeader {
                    id: header
                    //: Nextcloud Albums header text
                    //% "Albums"
                    title: qsTrId("jolla_gallery_nextcloud-la-user_albums")
                    description: pageHeader.title
                }
                SearchField {
                    id: searchField
                    width: parent.width
                    //: Nextcloud Albums search field placeholder text
                    //% "Search albums"
                    placeholderText: qsTrId("jolla_gallery_nextcloud-ph-search_albums")
                    EnterKey.onClicked: model.pattern = text.trim()
                    EnterKey.iconSource: "image://theme/icon-m-search"
                    onTextChanged: if (text.length == 0) model.pattern = ""
                }
            }
        }
        cacheBuffer: Screen.height
        model: root.searchModel

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
