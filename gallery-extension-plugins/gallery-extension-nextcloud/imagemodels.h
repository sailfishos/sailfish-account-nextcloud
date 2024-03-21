/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUD_GALLERY_IMAGEMODELS_H
#define NEXTCLOUD_GALLERY_IMAGEMODELS_H

#include "synccacheimages.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtQml/QQmlParserStatus>

// libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Account>

class NextcloudEnabledUsersListener : public QObject
{
    Q_OBJECT
public:
    explicit NextcloudEnabledUsersListener(QObject *parent = nullptr);

    SyncCache::ImageCache *imageCache() const;
    void setImageCache(SyncCache::ImageCache *cache);

    QVector<SyncCache::User> enabledUsers() const;

    void loadData();

Q_SIGNALS:
    void enabledUsersChanged();

private:
    void addAccount(Accounts::AccountId accountId);
    void addAllAccounts();
    void removeAccount(Accounts::AccountId accountId);
    void removeAllAccounts();
    void enabledChanged(const QString &serviceName, bool enabled);
    void accountDestroyed();
    void reload();

    class AccountInfo
    {
    public:
        Accounts::Account *account = nullptr;
        bool accountEnabled = false;
        bool imageServiceEnabled = false;
    };

    SyncCache::ImageCache *m_imageCache = nullptr;
    Accounts::Manager *m_accountManager = nullptr;
    QVector<SyncCache::User> m_data;
    QVector<SyncCache::User> m_filteredData;
    QMap<Accounts::AccountId, AccountInfo> m_accounts;
};

class NextcloudUserModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(SyncCache::ImageCache* imageCache READ imageCache WRITE setImageCache NOTIFY imageCacheChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY rowCountChanged)

public:
    explicit NextcloudUserModel(QObject *parent = nullptr);

    // QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    // QQmlAbstractListModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        UserIdRole,
        DisplayNameRole,
        ThumbnailUrlRole,
        ThumbnailPathRole
    };
    Q_ENUM(Roles)

    SyncCache::ImageCache *imageCache() const;
    void setImageCache(SyncCache::ImageCache *cache);

    Q_INVOKABLE QVariantMap at(int row) const;

Q_SIGNALS:
    void imageCacheChanged();
    void rowCountChanged();

private:
    void enabledUsersChanged();

    QVector<SyncCache::User> m_users;
    NextcloudEnabledUsersListener *m_enabledUsersListener = nullptr;
    bool m_deferLoad = false;
};

class NextcloudAlbumModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(SyncCache::ImageCache* imageCache READ imageCache WRITE setImageCache NOTIFY imageCacheChanged)
    Q_PROPERTY(int accountId READ accountId WRITE setAccountId NOTIFY accountIdChanged)
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QString userDisplayName READ userDisplayName NOTIFY userDisplayNameChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY rowCountChanged)

public:
    explicit NextcloudAlbumModel(QObject *parent = nullptr);

    // QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    // QQmlAbstractListModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        UserIdRole,
        AlbumIdRole,
        ParentAlbumIdRole,
        AlbumNameRole,
        PhotoCountRole,
        ThumbnailUrlRole,
        ThumbnailPathRole,
        ThumbnailFileNameRole,
    };
    Q_ENUM(Roles)

    SyncCache::ImageCache *imageCache() const;
    void setImageCache(SyncCache::ImageCache *cache);

    int accountId() const;
    void setAccountId(int id);

    QString userId() const;
    void setUserId(const QString &id);

    QString userDisplayName() const;

    Q_INVOKABLE QVariantMap at(int row) const;

Q_SIGNALS:
    void imageCacheChanged();
    void accountIdChanged();
    void userIdChanged();
    void userDisplayNameChanged();
    void rowCountChanged();

private:
    void loadData();

    bool m_deferLoad = false;
    SyncCache::ImageCache *m_imageCache = nullptr;
    int m_accountId = 0;
    QString m_userId;
    QString m_userDisplayName;
    QVector<SyncCache::Album> m_data;
};

class NextcloudPhotoModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(SyncCache::ImageCache* imageCache READ imageCache WRITE setImageCache NOTIFY imageCacheChanged)
    Q_PROPERTY(int accountId READ accountId WRITE setAccountId NOTIFY accountIdChanged)
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QString albumId READ albumId WRITE setAlbumId NOTIFY albumIdChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY rowCountChanged)

public:
    explicit NextcloudPhotoModel(QObject *parent = nullptr);

    // QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    // QQmlAbstractListModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        UserIdRole,
        AlbumIdRole,
        PhotoIdRole,
        CreatedTimestampRole,
        UpdatedTimestampRole,
        FileNameRole,
        AlbumPathRole,
        DescriptionRole,
        ThumbnailUrlRole,
        ThumbnailPathRole,
        ImageUrlRole,
        ImagePathRole,
        ImageWidthRole,
        ImageHeightRole,
        FileSizeRole,
        FileTypeRole
    };
    Q_ENUM(Roles)

    SyncCache::ImageCache *imageCache() const;
    void setImageCache(SyncCache::ImageCache *cache);

    int accountId() const;
    void setAccountId(int id);

    QString userId() const;
    void setUserId(const QString &id);

    QString albumId() const;
    void setAlbumId(const QString &albumId);

    Q_INVOKABLE QVariantMap at(int row) const;

Q_SIGNALS:
    void imageCacheChanged();
    void accountIdChanged();
    void userIdChanged();
    void albumIdChanged();
    void rowCountChanged();

private:
    void loadData();

    bool m_deferLoad = false;
    SyncCache::ImageCache *m_imageCache = nullptr;
    int m_accountId = 0;
    QString m_userId;
    QString m_albumId;
    QVector<SyncCache::Photo> m_data;
};

class NextcloudPhotoCounter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SyncCache::ImageCache* imageCache READ imageCache WRITE setImageCache NOTIFY imageCacheChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit NextcloudPhotoCounter(QObject *parent = nullptr);

    SyncCache::ImageCache *imageCache() const;
    void setImageCache(SyncCache::ImageCache *cache);

    int count();

Q_SIGNALS:
    void countChanged();
    void imageCacheChanged();

private:
    void enabledUsersChanged();
    void requestPhotoCount();
    void reload();

    QVector<SyncCache::User> m_users;
    NextcloudEnabledUsersListener *m_enabledUsersListener = nullptr;
    QMap<int, int> m_photoCounts;
    int m_count = 0;
};

#endif // NEXTCLOUD_GALLERY_IMAGEMODELS_H
