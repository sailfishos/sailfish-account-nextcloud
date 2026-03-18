// SPDX-FileCopyrightText: 2019 - 2020 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Lipstick 1.0
import com.jolla.eventsview.nextcloud 1.0

NotificationGroupMember {
    id: root

    property alias icon: image
    property alias subject: subjectLabel.text
    property alias message: messageLabel.text
    property var timestamp
    property string eventUrl

    width: parent.width
    contentHeight: Math.max(image.y + image.height,
                            content.y + content.height)
                   + Theme.paddingLarge

    onTriggered: {
        if (eventUrl.length > 0) {
            Qt.openUrlExternally(eventUrl)
        }
    }

    Rectangle {
        id: imageContainer

        y: Theme.paddingLarge
        width: Theme.iconSizeMedium
        height: Theme.iconSizeMedium
        radius: Theme.paddingSmall
        color: "#FAF9F6" // whitish

        Image {
            id: image

            anchors.centerIn: parent
            width: Theme.iconSizeMedium - 2*Theme.paddingSmall
            height: width

            // might be svg, ensure it gets rendered at the end result resolution
            sourceSize.width: width
            sourceSize.height: height
        }
    }

    Column {
        id: content

        anchors {
            left: imageContainer.right
            leftMargin: Theme.paddingMedium
            top: imageContainer.top
            topMargin: -Theme.paddingSmall
        }

        width: root.width - x - root.contentLeftMargin - Theme.paddingMedium
        spacing: Theme.paddingSmall

        Label {
            id: subjectLabel

            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.bold: true
        }

        Label {
            id: messageLabel

            visible: text.length !== 0
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
        }

        Label {
            text: Format.formatDate(root.timestamp, Format.TimeElapsed)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
