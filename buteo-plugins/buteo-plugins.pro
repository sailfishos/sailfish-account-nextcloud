# SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE=subdirs
SUBDIRS += \
    buteo-common \
    buteo-sync-plugin-nextcloud-backup \
    buteo-sync-plugin-nextcloud-images \
    buteo-sync-plugin-nextcloud-posts

buteo-sync-plugin-nextcloud-backup.depends = buteo-common
buteo-sync-plugin-nextcloud-images.depends = buteo-common
buteo-sync-plugin-nextcloud-posts.depends = buteo-common
