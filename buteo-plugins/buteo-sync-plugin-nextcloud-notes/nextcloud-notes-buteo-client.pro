# SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
#
# SPDX-License-Identifier: BSD-3-Clause

TARGET = nextcloud-notes-client

include($$PWD/../buteo-common/buteo-common.pri)
include($$PWD/../../common/common.pri)

CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt5 buteosyncfw5 libsignon-qt5 signon-oauth2plugin libmkcal-qt5 KF5CalendarCore

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/plugin.cpp \
    $$PWD/authenticator.cpp \
    $$PWD/syncer.cpp \
    $$PWD/mkcalbackend.cpp

HEADERS += \
    $$PWD/plugin.h \
    $$PWD/authenticator.h \
    $$PWD/syncer.h \
    $$PWD/mkcalbackend.h

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.Notes.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-notes.xml

INSTALLS += target sync client
