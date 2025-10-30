# SPDX-FileCopyrightText: 2021 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

INCLUDEPATH += $$PWD
DEPENDPATH += .

QT += dbus

CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt5 buteosyncfw5 sailfishaccounts

LIBS += -L$$PWD -lnextcloudbuteocommon
