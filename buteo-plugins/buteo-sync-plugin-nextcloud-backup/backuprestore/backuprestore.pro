TARGET = nextcloud-backuprestore-client

include($$PWD/../backup-common.pri)

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/nextcloudbackuprestoreclient.cpp

HEADERS += \
    $$PWD/nextcloudbackuprestoreclient.h

OTHER_FILES += \
    $$PWD/nextcloud-backuprestore.xml \
    $$PWD/nextcloud.BackupRestore.xml

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = nextcloud.BackupRestore.xml

client.path = /etc/buteo/profiles/client
client.files = nextcloud-backuprestore.xml

INSTALLS += target sync client
