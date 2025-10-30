# SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE = lib

TARGET = nextcloudbuteocommon
TARGET = $$qtLibraryTarget($$TARGET)

QT -= gui
QT += network dbus
CONFIG += link_pkgconfig
PKGCONFIG += buteosyncfw5 sailfishaccounts

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/webdavsyncer_p.h \
    $$PWD/networkrequestgenerator_p.h \
    $$PWD/networkreplyparser_p.h \
    $$PWD/logging.h

SOURCES += \
    $$PWD/webdavsyncer.cpp \
    $$PWD/networkrequestgenerator.cpp \
    $$PWD/networkreplyparser.cpp \
    $$PWD/logging.cpp

TARGETPATH = $$[QT_INSTALL_LIBS]
target.path = $$TARGETPATH

INSTALLS += target
