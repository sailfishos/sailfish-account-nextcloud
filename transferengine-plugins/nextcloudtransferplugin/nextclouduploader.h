/****************************************************************************************
**
** Copyright (C) 2019 Open Mobile Platform LLC
** All rights reserved.
**
** License: Proprietary.
**
****************************************************************************************/

#ifndef NEXTCLOUDUPLOADER_H
#define NEXTCLOUDUPLOADER_H

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

// transferengine-qt5
#include <mediatransferinterface.h>

#include "nextcloudshareservicestatus.h"

class NextcloudApi;
class AccountSettings;
class QFile;

class NextcloudUploader : public MediaTransferInterface
{
    Q_OBJECT

public:
    NextcloudUploader(QNetworkAccessManager *qnam, QObject *parent = 0);
    ~NextcloudUploader();

    QString displayName() const;
    QUrl serviceIcon() const;
    bool cancelEnabled() const;
    bool restartEnabled() const;

public Q_SLOTS:
    void start();
    void cancel();

private Q_SLOTS:
    void startUploading();
    void transferFinished();
    void transferProgress(qreal progress);
    void transferError();
    void transferCanceled();
    void credentialsExpired();

private:
    void postFile();
    void cleanUp();

    NextcloudApi *m_api = nullptr;
    NextcloudShareServiceStatus *m_nextcloudShareServiceStatus = nullptr;
    QNetworkAccessManager *m_qnam = nullptr;
    QFile *m_contentFile = nullptr;
    NextcloudShareServiceStatus::AccountDetails m_accountDetails;
    QString m_filePath;
    QString m_token;

    friend class ut_nextclouduploader;
};

#endif // NEXTCLOUDUPLOADER_H
