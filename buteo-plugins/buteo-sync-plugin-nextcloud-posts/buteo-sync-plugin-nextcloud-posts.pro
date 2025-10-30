# SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TARGET = nextcloud-posts-client

QT -= gui

include($$PWD/../buteo-common/buteo-common.pri)
include($$PWD/../../common/common.pri)

CONFIG += link_pkgconfig
PKGCONFIG += mlite5

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudpostsclient.cpp \
    $$PWD/syncer.cpp

HEADERS += \
    $$PWD/nextcloudpostsclient.h \
    $$PWD/syncer_p.h

OTHER_FILES += \
    $$PWD/nextcloud-posts.xml \
    $$PWD/nextcloud.Posts.xml

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.Posts.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-posts.xml

INSTALLS += target sync client
