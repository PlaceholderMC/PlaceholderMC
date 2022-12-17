/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QtWidgets/QDialog>
#include <QtCore/QEventLoop>
#include <QTimer>

#include "minecraft/auth/MinecraftAccount.h"

namespace Ui
{
class MSALoginDialog;
}

class MSALoginDialog : public QDialog
{
    Q_OBJECT

public:
    ~MSALoginDialog();

    static MinecraftAccountPtr newAccount(QWidget *parent, QString message);
    int exec() override;

private:
    explicit MSALoginDialog(QWidget *parent = 0);

    void setUserInputsEnabled(bool enable);

protected
slots:
    void onTaskFailed(const QString &reason);
    void onTaskSucceeded();
    void onTaskStatus(const QString &status);
    void onTaskProgress(qint64 current, qint64 total);
    void showVerificationUriAndCode(const QUrl &uri, const QString &code, int expiresIn);
    void hideVerificationUriAndCode();

    void externalLoginTick();

private:
    Ui::MSALoginDialog *ui;
    MinecraftAccountPtr hello_developer_i_am_here_to_kindly_tell_you_that_the_following_variable_is_actually_a_member_account;
    shared_qobject_ptr<AccountTask> hello_developer_i_am_here_to_kindly_tell_you_that_the_following_variable_is_actually_a_member_loginTask;
    QTimer hello_developer_i_am_here_to_kindly_tell_you_that_the_following_variable_is_actually_a_member_externalLoginTimer;
    int hello_developer_i_am_here_to_kindly_tell_you_that_the_following_variable_is_actually_a_member_externalLoginElapsed = 0;
    int hello_developer_i_am_here_to_kindly_tell_you_that_the_following_variable_is_actually_a_member_externalLoginTimeout = 0;
};

