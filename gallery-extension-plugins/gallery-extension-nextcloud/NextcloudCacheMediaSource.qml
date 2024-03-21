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
import Sailfish.Accounts 1.0
import com.jolla.gallery 1.0
import com.jolla.gallery.nextcloud 1.0
import com.jolla.gallery.extensions 1.0
import org.nemomobile.socialcache 1.0

MediaSource {
    id: root

    //: Label of the Nextcloud album in Jolla Gallery application
    //% "Nextcloud"
    title: qsTrId("jolla_gallery_nextcloud-la-user_photos")
    icon: StandardPaths.resolveImport("com.jolla.gallery.nextcloud.NextcloudGalleryIcon")
    model: nextcloudUsers.count == 1
          ? nextcloudAlbums
          : nextcloudUsers
    count: photoCounter.count
    ready: nextcloudUsers.count > 0 && accountManager.cloudServiceReady
    page: nextcloudUsers.count == 1
          ? StandardPaths.resolveImport("com.jolla.gallery.nextcloud.NextcloudAlbumsPage")
          : StandardPaths.resolveImport("com.jolla.gallery.nextcloud.NextcloudUsersPage")

    property bool applicationActive: Qt.application.active

    property NextcloudPhotoCounter photoCounter: NextcloudPhotoCounter {
        imageCache: NextcloudImageCache
    }

    property NextcloudUserModel nextcloudUsers: NextcloudUserModel {
        imageCache: NextcloudImageCache
    }

    property NextcloudAlbumModel nextcloudAlbums: NextcloudAlbumModel {
        imageCache: NextcloudImageCache
        accountId: nextcloudUsers.count > 0 ? nextcloudUsers.at(0).accountId : 0
        userId: nextcloudUsers.count > 0 ? nextcloudUsers.at(0).userId : ""
    }

    property AccountManager accountManager: AccountManager {
        property bool cloudServiceReady

        Component.onCompleted: {
            cloudServiceReady = enabledAccounts("nextcloud", "nextcloud-images").length > 0
        }
    }
}
