// SPDX-FileCopyrightText: 2019 Open Mobile Platform LLC
// SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
// SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Gallery 1.0

Page {
    property var modelData
    property var imageMetaData

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: details.height

        ImageDetailsItem {
            id: details

            // Current details do not include exposure etc. Image metadata is expensive to fetch,
            // and Nextcloud images aren't indexed by tracker so the cached metadata is not available.
            filePathDetail.value: modelData.imageUrl
            fileSizeDetail.value: Format.formatFileSize(modelData.fileSize)
            typeDetail.value: modelData.fileType
            sizeDetail.value: details.formatDimensions(imageMetaData.width, imageMetaData.height)
            dateTakenDetail.value: Format.formatDate(modelData.createdTimestamp, Format.Timepoint)
        }

        VerticalScrollDecorator { }
    }
}
