TARGET = nextcloud-backupquery-client

include($$PWD/../backup-common.pri)

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudbackupqueryclient.cpp

HEADERS += \
    $$PWD/nextcloudbackupqueryclient.h

OTHER_FILES += \
    $$PWD/nextcloud-backupquery.xml \
    $$PWD/nextcloud.BackupQuery.xml

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.BackupQuery.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-backupquery.xml

INSTALLS += target sync client
