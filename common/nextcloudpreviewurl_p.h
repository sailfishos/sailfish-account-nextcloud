// SPDX-FileCopyrightText: 2025 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef NEXTCLOUD_PREVIEWURL_P_H
#define NEXTCLOUD_PREVIEWURL_P_H

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

namespace NextcloudPreviewUrl {

inline QUrl build(const QUrl &serverUrl, const QString &fileId)
{
    if (serverUrl.isEmpty() || fileId.isEmpty()) {
        return QUrl();
    }

    QUrl url(serverUrl);
    QString basePath = url.path();
    if (basePath.isEmpty()) {
        basePath = QStringLiteral("/");
    } else if (!basePath.endsWith('/')) {
        basePath += '/';
    }
    url.setPath(basePath + QStringLiteral("index.php/core/preview"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fileId"), fileId);
    query.addQueryItem(QStringLiteral("x"), QString::number(320));
    query.addQueryItem(QStringLiteral("y"), QString::number(320));
    query.addQueryItem(QStringLiteral("forceIcon"), QString::number(0));
    query.addQueryItem(QStringLiteral("a"), QString::number(0));
    url.setQuery(query);

    return url;
}

inline QUrl build(const QString &serverUrl, const QString &fileId)
{
    return build(QUrl(serverUrl), fileId);
}

inline QUrl buildFromImageUrl(const QUrl &imageUrl, const QString &fileId)
{
    if (imageUrl.isEmpty() || fileId.isEmpty()) {
        return QUrl();
    }

    QUrl baseUrl(imageUrl);
    const QString imagePath = baseUrl.path();
    const int remotePhpIndex = imagePath.indexOf(QStringLiteral("/remote.php/"));
    if (remotePhpIndex >= 0) {
        baseUrl.setPath(imagePath.left(remotePhpIndex));
    } else {
        baseUrl.setPath(QString());
    }

    return build(baseUrl, fileId);
}

} // namespace NextcloudPreviewUrl

#endif // NEXTCLOUD_PREVIEWURL_P_H
