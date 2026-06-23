/*
 * SPDX-FileCopyrightText: 2025 Damien Caliste <dcaliste@free.fr>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <QObject>

#include <SignOn/Identity>
#include <Accounts/AccountService>

class Authenticator : public QObject
{
    Q_OBJECT
public:
    Authenticator(const QString &accountId, QObject *parent = nullptr);
    ~Authenticator();

    bool isValid() const;
    bool isEnabled() const;

    QString serverAddress() const;

    bool start();

signals:
    void authLoginAvailable(const QString &username, const QString &password);
    void authTokenAvailable(const QString &token);
    void finished();
    void errorOccurred(const QString &message);

private:
    Accounts::AccountService *m_accountService = nullptr;
    SignOn::Identity *m_identity = nullptr;
};

#endif
