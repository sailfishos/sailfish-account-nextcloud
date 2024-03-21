/*
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * License: Proprietary
 */
import QtQuick 2.6
import Sailfish.Silica 1.0

Item {
    id: root

    property alias title: titleLabel.text
    property alias titleLabel: titleLabel
    property alias countText: countLabel.text
    property alias icon: icon

    width: parent.width
    height: Theme.itemSizeExtraLarge

    Label {
        id: titleLabel

        anchors {
            left: parent.left
            leftMargin: Theme.paddingLarge
            right: iconContainer.left
            rightMargin: Theme.paddingLarge
            top: parent.top
            bottom: parent.bottom
        }
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WrapAnywhere
        font.pixelSize: Theme.fontSizeLarge
        fontSizeMode: Text.VerticalFit
    }

    Rectangle {
        id: iconContainer

        anchors.left: parent.horizontalCenter
        width: Theme.itemSizeExtraLarge
        height: Theme.itemSizeExtraLarge
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rgba(Theme.primaryColor, 0.1) }
            GradientStop { position: 1.0; color: "transparent" }
        }

        HighlightImage {
            id: icon

            anchors.centerIn: parent
            source: "image://theme/icon-m-file-folder"
        }
    }

    Label {
        id: countLabel

        anchors {
            right: parent.right
            leftMargin: Theme.horizontalPageMargin
            left: iconContainer.right
            verticalCenter: parent.verticalCenter
        }
        font.pixelSize: Theme.fontSizeLarge
    }
}
