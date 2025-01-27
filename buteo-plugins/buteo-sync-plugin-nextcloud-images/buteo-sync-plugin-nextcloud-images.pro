TARGET = nextcloud-images-client

include($$PWD/../buteo-common/buteo-common.pri)
include($$PWD/../../common/common.pri)

CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt5 buteosyncfw5

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudimagesclient.cpp \
    $$PWD/replyparser.cpp \
    $$PWD/syncer.cpp

HEADERS += \
    $$PWD/nextcloudimagesclient.h \
    $$PWD/replyparser_p.h \
    $$PWD/syncer_p.h

OTHER_FILES += \
    $$PWD/nextcloud-images.xml \
    $$PWD/nextcloud.Images.xml

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.Images.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-images.xml

INSTALLS += target sync client
