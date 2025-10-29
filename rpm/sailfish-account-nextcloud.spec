# SPDX-FileCopyrightText: 2019 - 2023 Jolla Ltd.
# SPDX-FileCopyrightText: 2024 - 2025 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

Name: sailfish-account-nextcloud
License: BSD-3-Clause
Version: 0.1.13
Release: 1
Source0: %{name}-%{version}.tar.bz2
Summary: Account plugin for Nextcloud
BuildRequires: qt5-qmake
BuildRequires: sailfish-svg2png
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Sql)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(mlite5)
BuildRequires: pkgconfig(buteosyncfw5) >= 0.10.0
BuildRequires: pkgconfig(accounts-qt5)
BuildRequires: pkgconfig(socialcache)
BuildRequires: pkgconfig(libsailfishkeyprovider)
BuildRequires: pkgconfig(sailfishaccounts)
BuildRequires: pkgconfig(nemotransferengine-qt5) >= 2.0.0
BuildRequires: qt5-qttools
BuildRequires: qt5-qttools-linguist
Requires:      jolla-settings-accounts-extensions-onlinesync
Requires:      jolla-vault
Requires(post): %{_libexecdir}/manage-groups
Requires(postun): %{_libexecdir}/manage-groups

%description
%{summary}.

%package -n buteo-sync-plugin-nextcloud-posts
Summary:   Provides synchronisation of posts blobs with Nextcloud
Requires: %{name} = %{version}-%{release}
Requires: buteo-syncfw-qt5-msyncd
Requires: systemd
Requires(post): systemd

%description -n buteo-sync-plugin-nextcloud-posts
Provides synchronisation of posts blobs with Nextcloud.

%package -n eventsview-extensions-nextcloud
Summary:   Provides integration of Nextcloud notifications into Events view
Requires: lipstick-jolla-home-qt5-components >= 1.2.50

%description -n eventsview-extensions-nextcloud
Provides integration of Nextcloud notifications into Events view

%package -n eventsview-extensions-nextcloud-ts-devel
Summary:  Translation source for Events Nextcloud plugin
Requires: eventsview-extensions-nextcloud

%description -n eventsview-extensions-nextcloud-ts-devel
Translation source for Events Nextcloud plugin.

%package -n buteo-sync-plugin-nextcloud-backup
Summary:   Provides synchronisation of backup/restore blobs with Nextcloud
Requires: %{name} = %{version}-%{release}
Requires: buteo-syncfw-qt5-msyncd
Requires: systemd
Requires(post): systemd

%description -n buteo-sync-plugin-nextcloud-backup
Provides synchronisation of backup/restore blobs with Nextcloud.

%package -n buteo-sync-plugin-nextcloud-images
Summary:   Provides synchronisation of gallery images with Nextcloud
Requires: %{name} = %{version}-%{release}
Requires: buteo-syncfw-qt5-msyncd
Requires: systemd
Requires(post): systemd

%description -n buteo-sync-plugin-nextcloud-images
Provides synchronisation of gallery images with Nextcloud.

%package -n jolla-gallery-extension-nextcloud
Summary:   Provides integration of Nextcloud images into Gallery application
Requires: sailfish-components-gallery-qt5 >= 1.1.9
Requires: sailfish-components-filemanager >= 0.2.17

%description -n jolla-gallery-extension-nextcloud
Provides integration of Nextcloud images into Gallery application.

%package -n jolla-gallery-extension-nextcloud-ts-devel
Summary:  Translation source for Gallery Nextcloud plugin
Requires: jolla-gallery-extension-nextcloud

%description -n jolla-gallery-extension-nextcloud-ts-devel
Translation source for Gallery Nextcloud plugin.

%package -n transferengine-plugin-nextcloud
Summary: Nextcloud file sharing plugin for Transfer Engine
Requires: sailfishsilica-qt5 >= 1.1.108
Requires: declarative-transferengine-qt5 >= 0.3.13
Requires: nemo-transferengine-qt5 >= 2.0.0
Requires: %{name} = %{version}-%{release}

%description -n transferengine-plugin-nextcloud
Nextcloud file sharing plugin for Transfer Engine.

%package features-all
Summary:   Meta package to include all Nextcloud account features
Requires: %{name} = %{version}-%{release}
Requires: transferengine-plugin-nextcloud
Requires: jolla-gallery-extension-nextcloud
Requires: eventsview-extensions-nextcloud
Requires: buteo-sync-plugin-nextcloud-images
Requires: buteo-sync-plugin-nextcloud-backup
Requires: buteo-sync-plugin-nextcloud-posts

%description features-all
This package is here to include all Nextcloud account
features to image (e.g. sharing, image sync, backups, etc).


%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 "VERSION=%{version}"
%make_build

%install
%qmake5_install
cd icons
make INSTALL_ROOT=%{buildroot} install

%post
/sbin/ldconfig
%{_libexecdir}/manage-groups add account-nextcloud || :

%postun
/sbin/ldconfig
if [ "$1" -eq 0 ]; then
    %{_libexecdir}/manage-groups remove account-nextcloud || :
fi


%files
%license LICENSES/BSD-3-Clause.txt
%{_libdir}/libnextcloudcommon.so.*
%exclude %{_libdir}/libnextcloudcommon.so
%{_libdir}/libnextcloudbuteocommon.so.*
%exclude %{_libdir}/libnextcloudbuteocommon.so
%{_datadir}/accounts/providers/nextcloud.provider
%{_datadir}/accounts/services/nextcloud-backup.service
%{_datadir}/accounts/services/nextcloud-caldav.service
%{_datadir}/accounts/services/nextcloud-carddav.service
%{_datadir}/accounts/services/nextcloud-images.service
%{_datadir}/accounts/services/nextcloud-posts.service
%{_datadir}/accounts/services/nextcloud-sharing.service
%{_datadir}/accounts/ui/nextcloud.qml
%{_datadir}/accounts/ui/nextcloud-settings.qml
%{_datadir}/accounts/ui/nextcloud-update.qml
%{_datadir}/themes/sailfish-default/silica/*/icons/graphic-service-nextcloud.png
%{_datadir}/themes/sailfish-default/silica/*/icons/graphic-m-service-nextcloud.png
%{_datadir}/themes/sailfish-default/silica/*/icons/graphic-s-service-nextcloud.png
%{_datadir}/themes/sailfish-default/silica/*/icons/icon-l-nextcloud.png
%{_datadir}/themes/sailfish-default/silica/*/icons/icon-m-file-folder-nextcloud.png

%files -n buteo-sync-plugin-nextcloud-posts
%{_libdir}/buteo-plugins-qt5/oopp/libnextcloud-posts-client.so
%config %{_sysconfdir}/buteo/profiles/client/nextcloud-posts.xml
%config %{_sysconfdir}/buteo/profiles/sync/nextcloud.Posts.xml

%files -n eventsview-extensions-nextcloud
%{_datadir}/translations/eventsview-nextcloud_eng_en.qm
%{_libdir}/qt5/qml/com/jolla/eventsview/nextcloud/*
%{_datadir}/lipstick/eventfeed/*

%files -n eventsview-extensions-nextcloud-ts-devel
%{_datadir}/translations/source/eventsview-nextcloud.ts

%files -n buteo-sync-plugin-nextcloud-backup
%{_libdir}/buteo-plugins-qt5/oopp/libnextcloud-backup-client.so
%{_libdir}/buteo-plugins-qt5/oopp/libnextcloud-backupquery-client.so
%{_libdir}/buteo-plugins-qt5/oopp/libnextcloud-backuprestore-client.so
%config %{_sysconfdir}/buteo/profiles/client/nextcloud-backup.xml
%config %{_sysconfdir}/buteo/profiles/client/nextcloud-backupquery.xml
%config %{_sysconfdir}/buteo/profiles/client/nextcloud-backuprestore.xml
%config %{_sysconfdir}/buteo/profiles/sync/nextcloud.Backup.xml
%config %{_sysconfdir}/buteo/profiles/sync/nextcloud.BackupQuery.xml
%config %{_sysconfdir}/buteo/profiles/sync/nextcloud.BackupRestore.xml

%files -n buteo-sync-plugin-nextcloud-images
%{_libdir}/buteo-plugins-qt5/oopp/libnextcloud-images-client.so
%config %{_sysconfdir}/buteo/profiles/client/nextcloud-images.xml
%config %{_sysconfdir}/buteo/profiles/sync/nextcloud.Images.xml

%files -n jolla-gallery-extension-nextcloud
%{_datadir}/translations/gallery-extension-nextcloud_eng_en.qm
%{_datadir}/jolla-gallery/mediasources/NextcloudCacheMediaSource.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudGalleryIcon.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudUsersPage.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudAlbumsPage.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudAlbumDelegate.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudDirectoryItem.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudPhotoListPage.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudFullscreenPhotoPage.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/NextcloudImageDetailsPage.qml
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/qmldir
%{_libdir}/qt5/qml/com/jolla/gallery/nextcloud/libjollagallerynextcloudplugin.so
%dir %{_libdir}/qt5/qml/com/jolla/gallery/nextcloud

%files -n jolla-gallery-extension-nextcloud-ts-devel
%{_datadir}/translations/source/gallery-extension-nextcloud.ts

%files -n transferengine-plugin-nextcloud
%{_libdir}/nemo-transferengine/plugins/sharing/libnextcloudshareplugin.so
%{_libdir}/nemo-transferengine/plugins/transfer/libnextcloudtransferplugin.so
%{_datadir}/nemo-transferengine/plugins/sharing/NextcloudShareFile.qml

%files features-all
# Empty as this is meta package.
