# SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

TEMPLATE=aux
THEMENAME = sailfish-default
CONFIG += sailfish-svg2png
# to check: should all the icons really be non-monochrome. none apply to the old grayscale rules now.

OTHER_FILES+=$$PWD/svgs/*
