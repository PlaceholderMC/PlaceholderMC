// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "LaunchController.h"
#include "Application.h"
#include "launch/steps/PrintServers.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"

#include "ui/InstanceWindow.h"
#include "ui/MainWindow.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/ProfileSelectDialog.h"
#include "ui/dialogs/ProfileSetupDialog.h"
#include "ui/dialogs/ProgressDialog.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>

#include "BuildConfig.h"
#include "JavaCommon.h"
#include "launch/steps/TextPrint.h"
#include "tasks/Task.h"

LaunchController::LaunchController() : Task() {}

void LaunchController::executeTask()
{
    if (!m_instance) {
        emitFailed(tr("No instance specified!"));
        return;
    }

    if (!JavaCommon::checkJVMArgs(m_instance->settings()->get("JvmArgs").toString(), m_parentWidget)) {
        emitFailed(tr("Invalid Java arguments specified. Please fix this first."));
        return;
    }

    login();
}

void LaunchController::decideAccount()
{
    if (m_accountToUse) {
        return;
    }

    // Find an account to use.
    auto accounts = APPLICATION->accounts();
    if (accounts->count() <= 0 || !accounts->anyAccountIsValid()) {
        // Tell the user they need to log in at least one account in order to play.
        auto reply = CustomMessageBox::selectable(m_parentWidget, tr("No Accounts"),
                                                  tr("In order to play Minecraft, you must have at least one Microsoft "
                                                     "account which owns Minecraft logged in. "
                                                     "Would you like to open the account manager to add an account now?"),
                                                  QMessageBox::Information, QMessageBox::Yes | QMessageBox::No)
                         ->exec();

        if (reply == QMessageBox::Yes) {
            // Open the account manager.
            APPLICATION->ShowGlobalSettings(m_parentWidget, "accounts");
        } else if (reply == QMessageBox::No) {
            // Do not open "profile select" dialog.
            return;
        }
    }

    // Select the account to use. If the instance has a specific account set, that will be used. Otherwise, the default account will be used
    auto instanceAccountId = m_instance->settings()->get("InstanceAccountId").toString();
    auto instanceAccountIndex = accounts->findAccountByProfileId(instanceAccountId);
    if (instanceAccountIndex == -1 || instanceAccountId.isEmpty()) {
        m_accountToUse = accounts->defaultAccount();
    } else {
        m_accountToUse = accounts->at(instanceAccountIndex);
    }

    if (!m_accountToUse) {
        // If no default account is set, ask the user which one to use.
        ProfileSelectDialog selectDialog(tr("Which account would you like to use?"), ProfileSelectDialog::GlobalDefaultCheckbox,
                                         m_parentWidget);

        selectDialog.exec();

        // Launch the instance with the selected account.
        m_accountToUse = selectDialog.selectedAccount();

        // If the user said to use the account as default, do that.
        if (selectDialog.useAsGlobalDefault() && m_accountToUse) {
            accounts->setDefaultAccount(m_accountToUse);
        }
    }
}

bool LaunchController::askPlayDemo()
{
    QMessageBox box(m_parentWidget);
    box.setWindowTitle(tr("Play demo?"));
    box.setText(
        tr("This account does not own Minecraft.\nYou need to purchase the game first to play it.\n\nDo you want to play "
           "the demo?"));
    box.setIcon(QMessageBox::Warning);
    auto demoButton = box.addButton(tr("Play Demo"), QMessageBox::ButtonRole::YesRole);
    auto cancelButton = box.addButton(tr("Cancel"), QMessageBox::ButtonRole::NoRole);
    box.setDefaultButton(cancelButton);

    box.exec();
    return box.clickedButton() == demoButton;
}

QString LaunchController::askOfflineName(QString playerName, bool demo, bool& ok)
{
    // we ask the user for a player name
    QString message = tr("Choose your offline mode player name.");
    if (demo) {
        message = tr("Choose your demo mode player name.");
    }

    QString lastOfflinePlayerName = APPLICATION->settings()->get("LastOfflinePlayerName").toString();
    QString usedname = lastOfflinePlayerName.isEmpty() ? playerName : lastOfflinePlayerName;
    QString name = QInputDialog::getText(m_parentWidget, tr("Player name"), message, QLineEdit::Normal, usedname, &ok);
    if (!ok)
        return {};
    if (name.length()) {
        usedname = name;
        APPLICATION->settings()->set("LastOfflinePlayerName", usedname);
    }
    return usedname;
}

void LaunchController::login()
{
    decideAccount();

    if (!m_accountToUse) {
        // if no account is selected, ask about demo
        if (!m_demo) {
            m_demo = askPlayDemo();
        }
        if (m_demo) {
            // we ask the user for a player name
            bool ok = false;
            auto name = askOfflineName("Player", m_demo, ok);
            if (ok) {
                m_session = std::make_shared<AuthSession>();
                m_session->MakeDemo(name, MinecraftAccount::uuidFromUsername(name).toString().remove(QRegularExpression("[{}-]")));
                launchInstance();
                return;
            }
        }
        // if no account is selected, we bail
        emitFailed(tr("No account selected for launch."));
        return;
    }

    // we loop until the user succeeds in logging in or gives up
    bool tryagain = true;
    unsigned int tries = 0;

    if ((m_accountToUse->accountType() != AccountType::Offline && m_accountToUse->accountState() == AccountState::Offline) ||
        m_accountToUse->shouldRefresh()) {
        // Force account refresh on the account used to launch the instance updating the AccountState
        // only on first try and if it is not meant to be offline
        auto accounts = APPLICATION->accounts();
        accounts->requestRefresh(m_accountToUse->internalId());
    }
    while (tryagain) {
        if (tries > 0 && tries % 3 == 0) {
            auto result =
                QMessageBox::question(m_parentWidget, tr("Continue launch?"),
                                      tr("It looks like we couldn't launch after %1 tries. Do you want to continue trying?").arg(tries));

            if (result == QMessageBox::No) {
                emitAborted();
                return;
            }
        }
        tries++;
        m_session = std::make_shared<AuthSession>();
        m_session->wants_online = m_online;
        m_session->demo = m_demo;
        m_accountToUse->fillSession(m_session);

        // Launch immediately in true offline mode
        if (m_accountToUse->accountType() == AccountType::Offline) {
            launchInstance();
            return;
        }

        switch (m_accountToUse->accountState()) {
            case AccountState::Offline: {
                m_session->wants_online = false;
            }
            /* fallthrough */
            case AccountState::Online: {
                if (!m_session->wants_online) {
                    // we ask the user for a player name
                    bool ok = false;
                    QString name;
                    if (m_offlineName.isEmpty()) {
                        name = askOfflineName(m_session->player_name, m_session->demo, ok);
                        if (!ok) {
                            tryagain = false;
                            break;
                        }
                    } else {
                        name = m_offlineName;
                    }
                    m_session->MakeOffline(name);
                    // offline flavored game from here :3
                }
                if (m_accountToUse->ownsMinecraft()) {
                    if (!m_accountToUse->hasProfile()) {
                        // Now handle setting up a profile name here...
                        ProfileSetupDialog dialog(m_accountToUse, m_parentWidget);
                        if (dialog.exec() == QDialog::Accepted) {
                            tryagain = true;
                            continue;
                        } else {
                            emitFailed(tr("Received undetermined session status during login."));
                            return;
                        }
                    }
                    // we own Minecraft, there is a profile, it's all ready to go!
                    launchInstance();
                    return;
                } else {
                    // play demo ?
                    if (!m_session->demo) {
                        m_session->demo = askPlayDemo();
                    }
                    if (m_session->demo) {  // play demo here
                        launchInstance();
                    } else {
                        emitFailed(tr("Launch cancelled - account does not own Minecraft."));
                    }
                }
                return;
            }
            case AccountState::Errored:
                // This means some sort of soft error that we can fix with a refresh ... so let's refresh.
            case AccountState::Unchecked: {
                m_accountToUse->refresh();
            }
            /* fallthrough */
            case AccountState::Working: {
                // refresh is in progress, we need to wait for it to finish to proceed.
                ProgressDialog progDialog(m_parentWidget);
                if (m_online) {
                    progDialog.setSkipButton(true, tr("Play Offline"));
                }
                auto task = m_accountToUse->currentTask();
                progDialog.execWithTask(task.get());
                continue;
            }
            case AccountState::Expired: {
                if (reauthenticateCurrentAccount())
                    continue;
                return;
            }
            case AccountState::Disabled: {
                auto errorString = tr("The launcher's client identification has changed. Please remove this account and add it again.");
                QMessageBox::warning(m_parentWidget, tr("Client identification changed"), errorString, QMessageBox::StandardButton::Ok,
                                     QMessageBox::StandardButton::Ok);
                emitFailed(errorString);
                return;
            }
            case AccountState::Gone: {
                auto errorString =
                    tr("The account no longer exists on the servers. It may have been migrated, in which case please add the new account "
                       "you migrated this one to.");
                QMessageBox::warning(m_parentWidget, tr("Account gone"), errorString, QMessageBox::StandardButton::Ok,
                                     QMessageBox::StandardButton::Ok);
                emitFailed(errorString);
                return;
            }
        }
    }
    emitFailed(tr("Failed to launch."));
}

bool LaunchController::reauthenticateCurrentAccount()
{
    auto button =
        QMessageBox::warning(m_parentWidget, tr("Account refresh failed"),
                             tr("The account has expired and needs to be reauthenticated. Do you want to reauthenticate this account?"),
                             QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes);
    if (button == QMessageBox::StandardButton::Yes) {
        auto accounts = APPLICATION->accounts();
        bool isDefault = accounts->defaultAccount() == m_accountToUse;
        accounts->removeAccount(accounts->index(accounts->findAccountByProfileId(m_accountToUse->profileId())));
        if (m_accountToUse->accountType() == AccountType::MSA) {
            auto newAccount = MSALoginDialog::newAccount(m_parentWidget);
            accounts->addAccount(newAccount);
            if (isDefault) {
                accounts->setDefaultAccount(newAccount);
            }
            m_accountToUse = nullptr;
            decideAccount();
            return true;
        }
        emitFailed(tr("Account expired and re-login attempt failed"));
    } else {
        emitFailed(tr("The account has expired and needs to be reauthenticated"));
    }
    return false;
}

void LaunchController::launchInstance()
{
    Q_ASSERT_X(m_instance != NULL, "launchInstance", "instance is NULL");
    Q_ASSERT_X(m_session.get() != nullptr, "launchInstance", "session is NULL");

    if (!m_instance->reloadSettings()) {
        QMessageBox::critical(m_parentWidget, tr("Error!"), tr("Couldn't load the instance profile."));
        emitFailed(tr("Couldn't load the instance profile."));
        return;
    }

    m_launcher = m_instance->createLaunchTask(m_session, m_targetToJoin);
    if (!m_launcher) {
        emitFailed(tr("Couldn't instantiate a launcher."));
        return;
    }

    auto console = qobject_cast<InstanceWindow*>(m_parentWidget);
    auto showConsole = m_instance->settings()->get("ShowConsole").toBool();
    if (!console && showConsole) {
        APPLICATION->showInstanceWindow(m_instance);
    }
    connect(m_launcher.get(), &LaunchTask::readyForLaunch, this, &LaunchController::readyForLaunch);
    connect(m_launcher.get(), &LaunchTask::succeeded, this, &LaunchController::onSucceeded);
    connect(m_launcher.get(), &LaunchTask::failed, this, &LaunchController::onFailed);
    connect(m_launcher.get(), &LaunchTask::requestProgress, this, &LaunchController::onProgressRequested);

    // Prepend Online and Auth Status
    QString online_mode;
    if (m_session->wants_online) {
        online_mode = "online";

        // Prepend Server Status
        QStringList servers = { "login.microsoftonline.com", "session.minecraft.net", "textures.minecraft.net", "api.mojang.com" };

        m_launcher->prependStep(makeShared<PrintServers>(m_launcher.get(), servers));
    } else {
        online_mode = m_demo ? "demo" : "offline";
    }

    m_launcher->prependStep(
        makeShared<TextPrint>(m_launcher.get(), "Launched instance in " + online_mode + " mode\n", MessageLevel::Launcher));

    // Prepend Version
    {
        auto versionString = QString("%1 version: %2 (%3)")
                                 .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.printableVersionString(), BuildConfig.BUILD_PLATFORM);
        m_launcher->prependStep(makeShared<TextPrint>(m_launcher.get(), versionString + "\n\n", MessageLevel::Launcher));
    }
    m_launcher->start();
}

void LaunchController::readyForLaunch()
{
    if (!m_profiler) {
        m_launcher->proceed();
        return;
    }

    QString error;
    if (!m_profiler->check(&error)) {
        m_launcher->abort();
        emitFailed("Profiler startup failed!");
        QMessageBox::critical(m_parentWidget, tr("Error!"), tr("Profiler check for %1 failed: %2").arg(m_profiler->name(), error));
        return;
    }
    BaseProfiler* profilerInstance = m_profiler->createProfiler(m_launcher->instance(), this);

    connect(profilerInstance, &BaseProfiler::readyToLaunch, [this](const QString& message) {
        QMessageBox msg(m_parentWidget);
        msg.setText(tr("The game launch is delayed until you press the "
                       "button. This is the right time to setup the profiler, as the "
                       "profiler server is running now.\n\n%1")
                        .arg(message));
        msg.setWindowTitle(tr("Waiting."));
        msg.setIcon(QMessageBox::Information);
        msg.addButton(tr("&Launch"), QMessageBox::AcceptRole);
        msg.exec();
        m_launcher->proceed();
    });
    connect(profilerInstance, &BaseProfiler::abortLaunch, [this](const QString& message) {
        QMessageBox msg;
        msg.setText(tr("Couldn't start the profiler: %1").arg(message));
        msg.setWindowTitle(tr("Error"));
        msg.setIcon(QMessageBox::Critical);
        msg.addButton(QMessageBox::Ok);
        msg.setModal(true);
        msg.exec();
        m_launcher->abort();
        emitFailed("Profiler startup failed!");
    });
    profilerInstance->beginProfiling(m_launcher);
}

void LaunchController::onSucceeded()
{
    emitSucceeded();
}

void LaunchController::onFailed(QString reason)
{
    if (m_instance->settings()->get("ShowConsoleOnError").toBool()) {
        APPLICATION->showInstanceWindow(m_instance, "console");
    }
    emitFailed(reason);
}

void LaunchController::onProgressRequested(Task* task)
{
    ProgressDialog progDialog(m_parentWidget);
    progDialog.setSkipButton(true, tr("Abort"));
    m_launcher->proceed();
    progDialog.execWithTask(task);
}

bool LaunchController::abort()
{
    if (!m_launcher) {
        return true;
    }
    if (!m_launcher->canAbort()) {
        return false;
    }
    auto response = CustomMessageBox::selectable(m_parentWidget, tr("Kill Minecraft?"),
                                                 tr("This can cause the instance to get corrupted and should only be used if Minecraft "
                                                    "is frozen for some reason"),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                        ->exec();
    if (response == QMessageBox::Yes) {
        return m_launcher->abort();
    }
    return false;
}
