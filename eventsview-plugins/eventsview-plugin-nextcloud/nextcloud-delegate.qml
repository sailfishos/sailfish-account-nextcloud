/****************************************************************************************
**
** Copyright (c) 2019-2020 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

import QtQuick 2.0
import Sailfish.Accounts 1.0
import com.jolla.eventsview.nextcloud 1.0

Column {
    id: root

    // used by lipstick
    property var downloader
    property string providerName
    property var subviewModel
    property int animationDuration
    property bool collapsed: true
    property bool showingInActiveView
    property int eventsColumnMaxWidth
    property bool userRemovable
    property bool hasRemovableItems
    property real mainContentHeight

    signal expanded(int itemPosY)

    // used by lipstick
    function findMatchingRemovableItems(filterFunc, matchingResults) {
        for (var i = 0; i < accountFeedRepeater.count; ++i) {
            accountFeedRepeater.itemAt(i).findMatchingRemovableItems(filterFunc, matchingResults)
        }
    }

    // used by lipstick
    function removeAllNotifications() {
        if (userRemovable) {
            for (var i = 0; i < accountFeedRepeater.count; ++i) {
                accountFeedRepeater.itemAt(i).removeAllNotifications()
            }
        }
    }

    width: parent.width

    AccountManager {
        id: accountManager

        Component.onCompleted: {
            var accountIds = accountManager.providerAccountIdentifiers(root.providerName)
            var model = []
            for (var i = 0; i < accountIds.length; ++i) {
                model.push(accountIds[i])
            }
            accountFeedRepeater.model = model
        }
    }

    Repeater {
        id: accountFeedRepeater

        delegate: NextcloudFeed {
            width: root.width

            accountId: modelData
            collapsed: root.collapsed
            showingInActiveView: root.showingInActiveView

            onExpanded: {
                root.collapsed = false
                root.expanded(itemPosY)
            }

            onUserRemovableChanged: {
                var _userRemovable = true
                for (var i = 0; i < accountFeedRepeater.count; ++i) {
                    var item = accountFeedRepeater.itemAt(i)
                    if (!!item && !item.userRemovable) {
                        _userRemovable = false
                        break
                    }
                }
                root.userRemovable = _userRemovable
            }

            onHasRemovableItemsChanged: {
                var _hasRemovableItems = true
                for (var i = 0; i < accountFeedRepeater.count; ++i) {
                    var item = accountFeedRepeater.itemAt(i)
                    if (!!item && !item.hasRemovableItems) {
                        _hasRemovableItems = false
                        break
                    }
                }
                root.hasRemovableItems = _hasRemovableItems
            }

            onMainContentHeightChanged: {
                var _mainContentHeight = 0
                for (var i = 0; i < accountFeedRepeater.count; ++i) {
                    var item = accountFeedRepeater.itemAt(i)
                    if (!!item) {
                        _mainContentHeight += item.mainContentHeight
                    }
                }
                root.mainContentHeight = _mainContentHeight
            }
        }
    }
}
