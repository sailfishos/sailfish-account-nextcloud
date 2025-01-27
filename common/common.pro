TEMPLATE = lib

QT -= gui
QT += network dbus sql

TARGET = nextcloudcommon
TARGET = $$qtLibraryTarget($$TARGET)

HEADERS += \
    $$PWD/processmutex_p.h \
    $$PWD/synccachedatabase.h \
    $$PWD/synccachedatabase_p.h \
    $$PWD/synccacheevents.h \
    $$PWD/synccacheevents_p.h \
    $$PWD/synccacheeventdownloads_p.h \
    $$PWD/synccacheimages.h \
    $$PWD/synccacheimages_p.h \
    $$PWD/synccacheimagechangenotifier_p.h \
    $$PWD/synccacheimagedownloads_p.h

SOURCES += \
    $$PWD/processmutex.cpp \
    $$PWD/synccachedatabase.cpp \
    $$PWD/synccacheevents.cpp \
    $$PWD/eventdatabase.cpp \
    $$PWD/synccacheeventdownloads.cpp \
    $$PWD/synccacheimages.cpp \
    $$PWD/synccacheimagechangenotifier.cpp \
    $$PWD/synccacheimagedownloads.cpp \
    $$PWD/imagedatabase.cpp

TARGETPATH = $$[QT_INSTALL_LIBS]
target.path = $$TARGETPATH

INSTALLS += target
