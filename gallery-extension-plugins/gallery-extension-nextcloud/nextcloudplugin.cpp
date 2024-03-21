#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QtQml>

#include <QGuiApplication>
#include <QTranslator>
#include <QLocale>

#include "imagecache.h"
#include "imagedownloader.h"
#include "imagemodels.h"

static QObject *synccacheimages_api_factory(QQmlEngine *, QJSEngine *)
{
    return new NextcloudImageCache;
}

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

class JollaGalleryNextcloudPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.jolla.gallery.nextcloud")

public:
    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri)
        Q_ASSERT(QLatin1String(uri) == QLatin1String("com.jolla.gallery.nextcloud"));
        AppTranslator *engineeringEnglish = new AppTranslator(engine);
        engineeringEnglish->load("gallery-extension-nextcloud_eng_en", "/usr/share/translations");
        AppTranslator *translator = new AppTranslator(engine);
        translator->load(QLocale(), "gallery-extension-nextcloud", "-", "/usr/share/translations");
    }

    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("com.jolla.gallery.nextcloud"));

        qmlRegisterSingletonType<NextcloudImageCache>(uri, 1, 0, "NextcloudImageCache", synccacheimages_api_factory);
        qmlRegisterType<NextcloudUserModel> (uri, 1, 0, "NextcloudUserModel");
        qmlRegisterType<NextcloudAlbumModel>(uri, 1, 0, "NextcloudAlbumModel");
        qmlRegisterType<NextcloudPhotoModel>(uri, 1, 0, "NextcloudPhotoModel");
        qmlRegisterType<NextcloudPhotoCounter>(uri, 1, 0, "NextcloudPhotoCounter");
        qmlRegisterType<NextcloudImageDownloader>(uri, 1, 0, "NextcloudImageDownloader");
    }
};

#include "nextcloudplugin.moc"

