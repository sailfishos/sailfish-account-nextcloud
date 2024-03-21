/****************************************************************************************
**
** Copyright (c) 2019 Open Mobile Platform LLC.
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QtQml>

#include <QGuiApplication>
#include <QTranslator>
#include <QLocale>

#include "eventcache.h"
#include "eventimagedownloader.h"
#include "eventmodel.h"

// using custom translator so it gets properly removed from qApp when engine is deleted
class AppTranslator: public QTranslator
{
    Q_OBJECT

public:
    AppTranslator(QObject *parent)
        : QTranslator(parent)
    {
        qApp->installTranslator(this);
    }

    virtual ~AppTranslator()
    {
        qApp->removeTranslator(this);
    }
};

class JollaEventsviewNextcloudPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.jolla.eventsview.nextcloud")

public:
    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri)
        Q_ASSERT(QLatin1String(uri) == QLatin1String("com.jolla.eventsview.nextcloud"));
        AppTranslator *engineeringEnglish = new AppTranslator(engine);
        engineeringEnglish->load("eventsview-nextcloud_eng_en", "/usr/share/translations");
        AppTranslator *translator = new AppTranslator(engine);
        translator->load(QLocale(), "eventsview-nextcloud", "-", "/usr/share/translations");
    }

    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("com.jolla.eventsview.nextcloud"));

        qmlRegisterType<NextcloudEventCache> (uri, 1, 0, "NextcloudEventCache");
        qmlRegisterType<NextcloudEventModel> (uri, 1, 0, "NextcloudEventModel");
        qmlRegisterType<NextcloudEventImageDownloader> (uri, 1, 0, "NextcloudEventImageDownloader");
    }
};

#include "plugin.moc"

