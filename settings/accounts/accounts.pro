# SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE=aux

OTHER_FILES += \
    $$PWD/providers/nextcloud.provider \
    $$PWD/services/nextcloud-backup.service \
    $$PWD/services/nextcloud-caldav.service \
    $$PWD/services/nextcloud-carddav.service \
    $$PWD/services/nextcloud-images.service \
    $$PWD/services/nextcloud-posts.service \
    $$PWD/services/nextcloud-sharing.service \
    $$PWD/services/nextcloud-notes.service \
    $$PWD/ui/nextcloud.qml \
    $$PWD/ui/nextcloud-settings.qml \
    $$PWD/ui/nextcloud-update.qml

provider.files += $$PWD/providers/nextcloud.provider
provider.path = /usr/share/accounts/providers/

services.files += \
    $$PWD/services/nextcloud-backup.service \
    $$PWD/services/nextcloud-caldav.service \
    $$PWD/services/nextcloud-carddav.service \
    $$PWD/services/nextcloud-images.service \
    $$PWD/services/nextcloud-posts.service \
    $$PWD/services/nextcloud-sharing.service \
    $$PWD/services/nextcloud-notes.service
services.path = /usr/share/accounts/services/

ui.files += \
    $$PWD/ui/nextcloud.qml \
    $$PWD/ui/nextcloud-settings.qml \
    $$PWD/ui/nextcloud-update.qml
ui.path = /usr/share/accounts/ui/

INSTALLS += provider services ui
