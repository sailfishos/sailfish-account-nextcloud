# SPDX-FileCopyrightText: 2020 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

QT -= gui

include($$PWD/../buteo-common/buteo-common.pri)

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudbackupoperationclient.cpp \
    $$PWD/syncer.cpp

HEADERS += \
    $$PWD/nextcloudbackupoperationclient.h \
    $$PWD/syncer_p.h

