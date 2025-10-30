# SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE = lib
TARGET = $$qtLibraryTarget(nextcloudshareplugin)
CONFIG += plugin
DEPENDPATH += .
INCLUDEPATH += ..

CONFIG += link_pkgconfig
PKGCONFIG += nemotransferengine-qt5 accounts-qt5 sailfishaccounts

HEADERS += nextcloudshareplugin.h \
           ../nextcloudshareservicestatus.h \
           nextcloudplugininfo.h

SOURCES += nextcloudshareplugin.cpp \
           nextcloudplugininfo.cpp \
           ../nextcloudshareservicestatus.cpp

target.path = $$[QT_INSTALL_LIBS]/nemo-transferengine/plugins/sharing

OTHER_FILES += *.qml

shareui.files = NextcloudShareFile.qml
shareui.path = /usr/share/nemo-transferengine/plugins/sharing

INSTALLS += target shareui
