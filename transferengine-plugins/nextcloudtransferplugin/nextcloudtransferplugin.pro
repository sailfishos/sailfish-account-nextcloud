# SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE = lib
TARGET = $$qtLibraryTarget(nextcloudtransferplugin)
CONFIG += plugin
DEPENDPATH += .
INCLUDEPATH += ..

QT += network

CONFIG += link_pkgconfig
PKGCONFIG += nemotransferengine-qt5 accounts-qt5 sailfishaccounts

HEADERS += nextcloudtransferplugin.h \
           nextclouduploader.h \
           ../nextcloudshareservicestatus.h \
           nextcloudapi.h

SOURCES += nextcloudtransferplugin.cpp \
           nextclouduploader.cpp \
           ../nextcloudshareservicestatus.cpp \
           nextcloudapi.cpp

target.path = $$[QT_INSTALL_LIBS]/nemo-transferengine/plugins/transfer

OTHER_FILES += *.qml

INSTALLS += target
