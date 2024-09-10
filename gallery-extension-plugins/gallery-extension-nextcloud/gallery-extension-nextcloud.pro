TEMPLATE = lib
TARGET = jollagallerynextcloudplugin
TARGET = $$qtLibraryTarget($$TARGET)

MODULENAME = com/jolla/gallery/nextcloud
TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

QT += qml
CONFIG += plugin link_pkgconfig
PKGCONFIG += accounts-qt5 libsailfishkeyprovider sailfishaccounts

include($$PWD/../../common/common.pri)

TS_FILE = $$OUT_PWD/gallery-extension-nextcloud.ts
EE_QM = $$OUT_PWD/gallery-extension-nextcloud_eng_en.qm

ts.commands += lupdate $$PWD -ts $$TS_FILE
ts.CONFIG += no_check_exist no_link
ts.output = $$TS_FILE
ts.input = .

ts_install.files = $$TS_FILE
ts_install.path = /usr/share/translations/source
ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
engineering_english.commands += lrelease -idbased $$TS_FILE -qm $$EE_QM
engineering_english.CONFIG += no_check_exist no_link
engineering_english.depends = ts
engineering_english.input = $$TS_FILE
engineering_english.output = $$EE_QM

engineering_english_install.path = /usr/share/translations
engineering_english_install.files = $$EE_QM
engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += ts engineering_english

PRE_TARGETDEPS += ts engineering_english

INSTALLS += ts_install engineering_english_install


import.files = \
    NextcloudGalleryIcon.qml \
    NextcloudUsersPage.qml \
    NextcloudAlbumsPage.qml \
    NextcloudAlbumDelegate.qml \
    NextcloudDirectoryItem.qml \
    NextcloudPhotoListPage.qml \
    NextcloudFullscreenPhotoPage.qml \
    NextcloudImageDetailsPage.qml \
    qmldir

import.path = $$TARGETPATH
target.path = $$TARGETPATH

qml.files = NextcloudCacheMediaSource.qml
qml.path = /usr/share/jolla-gallery/mediasources/

HEADERS += \
    imagemodels.h \
    imagecache.h \
    imagedownloader.h

SOURCES += \
    imagemodels.cpp \
    imagecache.cpp \
    imagedownloader.cpp \
    nextcloudplugin.cpp

OTHER_FILES += $$import.files $$qml.files

INSTALLS += target import qml
