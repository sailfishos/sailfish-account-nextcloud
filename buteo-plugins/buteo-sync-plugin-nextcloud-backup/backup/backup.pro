TARGET = nextcloud-backup-client

include($$PWD/../backup-common.pri)

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudbackupclient.cpp

HEADERS += \
    $$PWD/nextcloudbackupclient.h

OTHER_FILES += \
    $$PWD/nextcloud-backup.xml \
    $$PWD/nextcloud.Backup.xml

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.Backup.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-backup.xml

INSTALLS += target sync client
