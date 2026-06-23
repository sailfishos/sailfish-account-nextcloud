# SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
#
# SPDX-License-Identifier: BSD-3-Clause

TARGET = nextcloud-notes-client

QT -= gui
QT += network

include($$PWD/../../common/common.pri)
include($$PWD/../buteo-common/buteo-common.pri)

CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt5 buteosyncfw5 libsignon-qt5 signon-oauth2plugin libmkcal-qt5 KF5CalendarCore

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/authenticator.cpp \
    $$PWD/syncer.cpp \
    $$PWD/mkcalbackend.cpp \
    $$PWD/nextcloud-notes-client.cpp

HEADERS += \
    $$PWD/authenticator.h \
    $$PWD/syncer.h \
    $$PWD/mkcalbackend.h

TEMPLATE = app

target.path = $$INSTALL_ROOT/usr/bin
INSTALLS += target
