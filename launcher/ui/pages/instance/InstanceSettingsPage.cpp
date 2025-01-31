// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2022 TheKodeToad <TheKodeToad@proton.me>
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

#include "InstanceSettingsPage.h"
#include "DesktopServices.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/WorldList.h"
#include "settings/Setting.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/java/InstallJavaDialog.h"
#include "ui_InstanceSettingsPage.h"

#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>

#include <sys.h>

#include "ui/dialogs/VersionSelectDialog.h"
#include "ui/widgets/CustomCommands.h"

#include "Application.h"
#include "BuildConfig.h"
#include "JavaCommon.h"
#include "minecraft/auth/AccountList.h"

#include "FileSystem.h"
#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"

InstanceSettingsPage::InstanceSettingsPage(BaseInstance* inst, QWidget* parent)
    : QWidget(parent), ui(new Ui::InstanceSettingsPage), m_instance(inst)
{
    m_settings = inst->settings();
    ui->setupUi(this);

    ui->javaDownloadBtn->setHidden(!BuildConfig.JAVA_DOWNLOADER_ENABLED);

    connect(ui->openGlobalJavaSettingsButton, &QCommandLinkButton::clicked, this, &InstanceSettingsPage::globalSettingsButtonClicked);
    connect(APPLICATION, &Application::globalSettingsAboutToOpen, this, &InstanceSettingsPage::applySettings);
    connect(APPLICATION, &Application::globalSettingsClosed, this, &InstanceSettingsPage::loadSettings);
    connect(ui->instanceAccountSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &InstanceSettingsPage::changeInstanceAccount);

    connect(ui->useNativeGLFWCheck, &QAbstractButton::toggled, this, &InstanceSettingsPage::onUseNativeGLFWChanged);
    connect(ui->useNativeOpenALCheck, &QAbstractButton::toggled, this, &InstanceSettingsPage::onUseNativeOpenALChanged);

    auto mInst = dynamic_cast<MinecraftInstance*>(inst);
    m_world_quickplay_supported = mInst && mInst->traits().contains("feature:is_quick_play_singleplayer");
    if (m_world_quickplay_supported) {
        auto worlds = mInst->worldList();
        worlds->update();
        for (const auto& world : worlds->allWorlds()) {
            ui->worldsCb->addItem(world.folderName());
        }
    } else {
        ui->worldsCb->hide();
        ui->worldJoinButton->hide();
        ui->serverJoinAddressButton->setChecked(true);
        ui->serverJoinAddress->setEnabled(true);
        ui->serverJoinAddressButton->setStyleSheet("QRadioButton::indicator { width: 0px; height: 0px; }");
    }

    loadSettings();

    updateThresholds();
}

InstanceSettingsPage::~InstanceSettingsPage()
{
    delete ui;
}

void InstanceSettingsPage::globalSettingsButtonClicked(bool)
{
    switch (ui->settingsTabs->currentIndex()) {
        case 0:
            APPLICATION->ShowGlobalSettings(this, "java-settings");
            return;
        case 2:
            APPLICATION->ShowGlobalSettings(this, "custom-commands");
            return;
        case 3:
            APPLICATION->ShowGlobalSettings(this, "environment-variables");
            return;
        default:
            APPLICATION->ShowGlobalSettings(this, "minecraft-settings");
            return;
    }
}

bool InstanceSettingsPage::apply()
{
    applySettings();
    return true;
}

void InstanceSettingsPage::applySettings()
{
    SettingsObject::Lock lock(m_settings);

    // Miscellaneous
    bool miscellaneous = ui->miscellaneousSettingsBox->isChecked();
    m_settings->set("OverrideMiscellaneous", miscellaneous);
    if (miscellaneous) {
        m_settings->set("CloseAfterLaunch", ui->closeAfterLaunchCheck->isChecked());
        m_settings->set("QuitAfterGameStop", ui->quitAfterGameStopCheck->isChecked());
    } else {
        m_settings->reset("CloseAfterLaunch");
        m_settings->reset("QuitAfterGameStop");
    }

    // Console
    bool console = ui->consoleSettingsBox->isChecked();
    m_settings->set("OverrideConsole", console);
    if (console) {
        m_settings->set("ShowConsole", ui->showConsoleCheck->isChecked());
        m_settings->set("AutoCloseConsole", ui->autoCloseConsoleCheck->isChecked());
        m_settings->set("ShowConsoleOnError", ui->showConsoleErrorCheck->isChecked());
    } else {
        m_settings->reset("ShowConsole");
        m_settings->reset("AutoCloseConsole");
        m_settings->reset("ShowConsoleOnError");
    }

    // Window Size
    bool window = ui->windowSizeGroupBox->isChecked();
    m_settings->set("OverrideWindow", window);
    if (window) {
        m_settings->set("LaunchMaximized", ui->maximizedCheckBox->isChecked());
        m_settings->set("MinecraftWinWidth", ui->windowWidthSpinBox->value());
        m_settings->set("MinecraftWinHeight", ui->windowHeightSpinBox->value());
    } else {
        m_settings->reset("LaunchMaximized");
        m_settings->reset("MinecraftWinWidth");
        m_settings->reset("MinecraftWinHeight");
    }

    // Memory
    bool memory = ui->memoryGroupBox->isChecked();
    m_settings->set("OverrideMemory", memory);
    if (memory) {
        int min = ui->minMemSpinBox->value();
        int max = ui->maxMemSpinBox->value();
        if (min < max) {
            m_settings->set("MinMemAlloc", min);
            m_settings->set("MaxMemAlloc", max);
        } else {
            m_settings->set("MinMemAlloc", max);
            m_settings->set("MaxMemAlloc", min);
        }
        m_settings->set("PermGen", ui->permGenSpinBox->value());
    } else {
        m_settings->reset("MinMemAlloc");
        m_settings->reset("MaxMemAlloc");
        m_settings->reset("PermGen");
    }

    // Java Install Settings
    bool javaInstall = ui->javaSettingsGroupBox->isChecked();
    m_settings->set("OverrideJavaLocation", javaInstall);
    if (javaInstall) {
        m_settings->set("JavaPath", ui->javaPathTextBox->text());
        m_settings->set("IgnoreJavaCompatibility", ui->skipCompatibilityCheckbox->isChecked());
    } else {
        m_settings->reset("JavaPath");
        m_settings->reset("IgnoreJavaCompatibility");
    }

    // Java arguments
    bool javaArgs = ui->javaArgumentsGroupBox->isChecked();
    m_settings->set("OverrideJavaArgs", javaArgs);
    if (javaArgs) {
        m_settings->set("JvmArgs", ui->jvmArgsTextBox->toPlainText().replace("\n", " "));
    } else {
        m_settings->reset("JvmArgs");
    }

    // Custom Commands
    bool custcmd = ui->customCommands->checked();
    m_settings->set("OverrideCommands", custcmd);
    if (custcmd) {
        m_settings->set("PreLaunchCommand", ui->customCommands->prelaunchCommand());
        m_settings->set("WrapperCommand", ui->customCommands->wrapperCommand());
        m_settings->set("PostExitCommand", ui->customCommands->postexitCommand());
    } else {
        m_settings->reset("PreLaunchCommand");
        m_settings->reset("WrapperCommand");
        m_settings->reset("PostExitCommand");
    }

    // Environment Variables
    auto env = ui->environmentVariables->override();
    m_settings->set("OverrideEnv", env);
    if (env)
        m_settings->set("Env", ui->environmentVariables->value());
    else
        m_settings->reset("Env");

    // Workarounds
    bool workarounds = ui->nativeWorkaroundsGroupBox->isChecked();
    m_settings->set("OverrideNativeWorkarounds", workarounds);
    if (workarounds) {
        m_settings->set("UseNativeGLFW", ui->useNativeGLFWCheck->isChecked());
        m_settings->set("CustomGLFWPath", ui->lineEditGLFWPath->text());
        m_settings->set("UseNativeOpenAL", ui->useNativeOpenALCheck->isChecked());
        m_settings->set("CustomOpenALPath", ui->lineEditOpenALPath->text());
    } else {
        m_settings->reset("UseNativeGLFW");
        m_settings->reset("CustomGLFWPath");
        m_settings->reset("UseNativeOpenAL");
        m_settings->reset("CustomOpenALPath");
    }

    // Performance
    bool performance = ui->perfomanceGroupBox->isChecked();
    m_settings->set("OverridePerformance", performance);
    if (performance) {
        m_settings->set("EnableFeralGamemode", ui->enableFeralGamemodeCheck->isChecked());
        m_settings->set("EnableMangoHud", ui->enableMangoHud->isChecked());
        m_settings->set("UseDiscreteGpu", ui->useDiscreteGpuCheck->isChecked());
        m_settings->set("UseZink", ui->useZink->isChecked());

    } else {
        m_settings->reset("EnableFeralGamemode");
        m_settings->reset("EnableMangoHud");
        m_settings->reset("UseDiscreteGpu");
        m_settings->reset("UseZink");
    }

    // Game time
    bool gameTime = ui->gameTimeGroupBox->isChecked();
    m_settings->set("OverrideGameTime", gameTime);
    if (gameTime) {
        m_settings->set("ShowGameTime", ui->showGameTime->isChecked());
        m_settings->set("RecordGameTime", ui->recordGameTime->isChecked());
    } else {
        m_settings->reset("ShowGameTime");
        m_settings->reset("RecordGameTime");
    }

    // Join server on launch
    bool joinServerOnLaunch = ui->serverJoinGroupBox->isChecked();
    m_settings->set("JoinServerOnLaunch", joinServerOnLaunch);
    if (joinServerOnLaunch) {
        if (ui->serverJoinAddressButton->isChecked() || !m_world_quickplay_supported) {
            m_settings->set("JoinServerOnLaunchAddress", ui->serverJoinAddress->text());
            m_settings->reset("JoinWorldOnLaunch");
        } else {
            m_settings->set("JoinWorldOnLaunch", ui->worldsCb->currentText());
            m_settings->reset("JoinServerOnLaunchAddress");
        }
    } else {
        m_settings->reset("JoinServerOnLaunchAddress");
        m_settings->reset("JoinWorldOnLaunch");
    }

    // Use an account for this instance
    bool useAccountForInstance = ui->instanceAccountGroupBox->isChecked();
    m_settings->set("UseAccountForInstance", useAccountForInstance);
    if (!useAccountForInstance) {
        m_settings->reset("InstanceAccountId");
    }

    bool overrideLegacySettings = ui->legacySettingsGroupBox->isChecked();
    m_settings->set("OverrideLegacySettings", overrideLegacySettings);
    if (overrideLegacySettings) {
        m_settings->set("OnlineFixes", ui->onlineFixes->isChecked());
    } else {
        m_settings->reset("OnlineFixes");
    }

    // FIXME: This should probably be called by a signal instead
    m_instance->updateRuntimeContext();
}

void InstanceSettingsPage::loadSettings()
{
    // Miscellaneous
    ui->miscellaneousSettingsBox->setChecked(m_settings->get("OverrideMiscellaneous").toBool());
    ui->closeAfterLaunchCheck->setChecked(m_settings->get("CloseAfterLaunch").toBool());
    ui->quitAfterGameStopCheck->setChecked(m_settings->get("QuitAfterGameStop").toBool());

    // Console
    ui->consoleSettingsBox->setChecked(m_settings->get("OverrideConsole").toBool());
    ui->showConsoleCheck->setChecked(m_settings->get("ShowConsole").toBool());
    ui->autoCloseConsoleCheck->setChecked(m_settings->get("AutoCloseConsole").toBool());
    ui->showConsoleErrorCheck->setChecked(m_settings->get("ShowConsoleOnError").toBool());

    // Window Size
    ui->windowSizeGroupBox->setChecked(m_settings->get("OverrideWindow").toBool());
    ui->maximizedCheckBox->setChecked(m_settings->get("LaunchMaximized").toBool());
    ui->maximizedWarning->setVisible(m_settings->get("LaunchMaximized").toBool() && !m_instance->isLegacy());
    ui->windowWidthSpinBox->setValue(m_settings->get("MinecraftWinWidth").toInt());
    ui->windowHeightSpinBox->setValue(m_settings->get("MinecraftWinHeight").toInt());

    // Memory
    ui->memoryGroupBox->setChecked(m_settings->get("OverrideMemory").toBool());
    int min = m_settings->get("MinMemAlloc").toInt();
    int max = m_settings->get("MaxMemAlloc").toInt();
    if (min < max) {
        ui->minMemSpinBox->setValue(min);
        ui->maxMemSpinBox->setValue(max);
    } else {
        ui->minMemSpinBox->setValue(max);
        ui->maxMemSpinBox->setValue(min);
    }
    ui->permGenSpinBox->setValue(m_settings->get("PermGen").toInt());
    bool permGenVisible = m_settings->get("PermGenVisible").toBool();
    ui->permGenSpinBox->setVisible(permGenVisible);
    ui->labelPermGen->setVisible(permGenVisible);
    ui->labelPermgenNote->setVisible(permGenVisible);

    // Java Settings
    bool overrideLocation = m_settings->get("OverrideJavaLocation").toBool();
    bool overrideArgs = m_settings->get("OverrideJavaArgs").toBool();

    connect(m_settings->getSetting("OverrideJavaLocation").get(), &Setting::SettingChanged, ui->javaSettingsGroupBox,
            [this] { ui->javaSettingsGroupBox->setChecked(m_settings->get("OverrideJavaLocation").toBool()); });
    ui->javaSettingsGroupBox->setChecked(overrideLocation);
    ui->javaPathTextBox->setText(m_settings->get("JavaPath").toString());
    connect(m_settings->getSetting("JavaPath").get(), &Setting::SettingChanged, ui->javaSettingsGroupBox,
            [this] { ui->javaPathTextBox->setText(m_settings->get("JavaPath").toString()); });

    ui->skipCompatibilityCheckbox->setChecked(m_settings->get("IgnoreJavaCompatibility").toBool());

    ui->javaArgumentsGroupBox->setChecked(overrideArgs);
    ui->jvmArgsTextBox->setPlainText(m_settings->get("JvmArgs").toString());

    // Custom commands
    ui->customCommands->initialize(true, m_settings->get("OverrideCommands").toBool(), m_settings->get("PreLaunchCommand").toString(),
                                   m_settings->get("WrapperCommand").toString(), m_settings->get("PostExitCommand").toString());

    // Environment variables
    ui->environmentVariables->initialize(true, m_settings->get("OverrideEnv").toBool(), m_settings->get("Env").toMap());

    // Workarounds
    ui->nativeWorkaroundsGroupBox->setChecked(m_settings->get("OverrideNativeWorkarounds").toBool());
    ui->useNativeGLFWCheck->setChecked(m_settings->get("UseNativeGLFW").toBool());
    ui->lineEditGLFWPath->setText(m_settings->get("CustomGLFWPath").toString());
#ifdef Q_OS_LINUX
    ui->lineEditGLFWPath->setPlaceholderText(APPLICATION->m_detectedGLFWPath);
#else
    ui->lineEditGLFWPath->setPlaceholderText(tr("Path to %1 library file").arg(BuildConfig.GLFW_LIBRARY_NAME));
#endif
    ui->useNativeOpenALCheck->setChecked(m_settings->get("UseNativeOpenAL").toBool());
    ui->lineEditOpenALPath->setText(m_settings->get("CustomOpenALPath").toString());
#ifdef Q_OS_LINUX
    ui->lineEditOpenALPath->setPlaceholderText(APPLICATION->m_detectedOpenALPath);
#else
    ui->lineEditOpenALPath->setPlaceholderText(tr("Path to %1 library file").arg(BuildConfig.OPENAL_LIBRARY_NAME));
#endif

    // Performance
    ui->perfomanceGroupBox->setChecked(m_settings->get("OverridePerformance").toBool());
    ui->enableFeralGamemodeCheck->setChecked(m_settings->get("EnableFeralGamemode").toBool());
    ui->enableMangoHud->setChecked(m_settings->get("EnableMangoHud").toBool());
    ui->useDiscreteGpuCheck->setChecked(m_settings->get("UseDiscreteGpu").toBool());
    ui->useZink->setChecked(m_settings->get("UseZink").toBool());

#if !defined(Q_OS_LINUX)
    ui->settingsTabs->setTabVisible(ui->settingsTabs->indexOf(ui->performancePage), false);
#endif

    if (!(APPLICATION->capabilities() & Application::SupportsGameMode)) {
        ui->enableFeralGamemodeCheck->setDisabled(true);
        ui->enableFeralGamemodeCheck->setToolTip(tr("Feral Interactive's GameMode could not be found on your system."));
    }

    if (!(APPLICATION->capabilities() & Application::SupportsMangoHud)) {
        ui->enableMangoHud->setDisabled(true);
        ui->enableMangoHud->setToolTip(tr("MangoHud could not be found on your system."));
    }

    // Miscellanous
    ui->gameTimeGroupBox->setChecked(m_settings->get("OverrideGameTime").toBool());
    ui->showGameTime->setChecked(m_settings->get("ShowGameTime").toBool());
    ui->recordGameTime->setChecked(m_settings->get("RecordGameTime").toBool());

    ui->serverJoinGroupBox->setChecked(m_settings->get("JoinServerOnLaunch").toBool());

    if (auto server = m_settings->get("JoinServerOnLaunchAddress").toString(); !server.isEmpty()) {
        ui->serverJoinAddress->setText(server);
        ui->serverJoinAddressButton->setChecked(true);
        ui->worldJoinButton->setChecked(false);
        ui->serverJoinAddress->setEnabled(true);
        ui->worldsCb->setEnabled(false);
    } else if (auto world = m_settings->get("JoinWorldOnLaunch").toString(); !world.isEmpty() && m_world_quickplay_supported) {
        ui->worldsCb->setCurrentText(world);
        ui->serverJoinAddressButton->setChecked(false);
        ui->worldJoinButton->setChecked(true);
        ui->serverJoinAddress->setEnabled(false);
        ui->worldsCb->setEnabled(true);
    } else {
        ui->serverJoinAddressButton->setChecked(true);
        ui->worldJoinButton->setChecked(false);
        ui->serverJoinAddress->setEnabled(true);
        ui->worldsCb->setEnabled(false);
    }

    ui->instanceAccountGroupBox->setChecked(m_settings->get("UseAccountForInstance").toBool());
    updateAccountsMenu();

    ui->legacySettingsGroupBox->setChecked(m_settings->get("OverrideLegacySettings").toBool());
    ui->onlineFixes->setChecked(m_settings->get("OnlineFixes").toBool());
}

void InstanceSettingsPage::on_javaDownloadBtn_clicked()
{
    auto jdialog = new Java::InstallDialog({}, m_instance, this);
    jdialog->exec();
}

void InstanceSettingsPage::on_javaDetectBtn_clicked()
{
    if (JavaUtils::getJavaCheckPath().isEmpty()) {
        JavaCommon::javaCheckNotFound(this);
        return;
    }

    JavaInstallPtr java;

    VersionSelectDialog vselect(APPLICATION->javalist().get(), tr("Select a Java version"), this, true);
    vselect.setResizeOn(2);
    vselect.exec();

    if (vselect.result() == QDialog::Accepted && vselect.selectedVersion()) {
        java = std::dynamic_pointer_cast<JavaInstall>(vselect.selectedVersion());
        ui->javaPathTextBox->setText(java->path);
        bool visible = java->id.requiresPermGen() && m_settings->get("OverrideMemory").toBool();
        ui->permGenSpinBox->setVisible(visible);
        ui->labelPermGen->setVisible(visible);
        ui->labelPermgenNote->setVisible(visible);
        m_settings->set("PermGenVisible", visible);

        if (!java->is_64bit && m_settings->get("MaxMemAlloc").toInt() > 2048) {
            CustomMessageBox::selectable(this, tr("Confirm Selection"),
                                         tr("You selected a 32-bit version of Java.\n"
                                            "This installation does not support more than 2048MiB of RAM.\n"
                                            "Please make sure that the maximum memory value is lower."),
                                         QMessageBox::Warning, QMessageBox::Ok, QMessageBox::Ok)
                ->exec();
        }
    }
}

void InstanceSettingsPage::on_javaBrowseBtn_clicked()
{
    QString raw_path = QFileDialog::getOpenFileName(this, tr("Find Java executable"));

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (raw_path.isEmpty()) {
        return;
    }
    QString cooked_path = FS::NormalizePath(raw_path);

    QFileInfo javaInfo(cooked_path);
    if (!javaInfo.exists() || !javaInfo.isExecutable()) {
        return;
    }
    ui->javaPathTextBox->setText(cooked_path);

    // custom Java could be anything... enable perm gen option
    ui->permGenSpinBox->setVisible(true);
    ui->labelPermGen->setVisible(true);
    ui->labelPermgenNote->setVisible(true);
    m_settings->set("PermGenVisible", true);
}

void InstanceSettingsPage::on_javaTestBtn_clicked()
{
    if (checker) {
        return;
    }
    checker.reset(new JavaCommon::TestCheck(this, ui->javaPathTextBox->text(), ui->jvmArgsTextBox->toPlainText().replace("\n", " "),
                                            ui->minMemSpinBox->value(), ui->maxMemSpinBox->value(), ui->permGenSpinBox->value()));
    connect(checker.get(), SIGNAL(finished()), SLOT(checkerFinished()));
    checker->run();
}

void InstanceSettingsPage::onUseNativeGLFWChanged(bool checked)
{
    ui->lineEditGLFWPath->setEnabled(checked);
}

void InstanceSettingsPage::onUseNativeOpenALChanged(bool checked)
{
    ui->lineEditOpenALPath->setEnabled(checked);
}

void InstanceSettingsPage::updateAccountsMenu()
{
    ui->instanceAccountSelector->clear();
    auto accounts = APPLICATION->accounts();
    int accountIndex = accounts->findAccountByProfileId(m_settings->get("InstanceAccountId").toString());

    for (int i = 0; i < accounts->count(); i++) {
        MinecraftAccountPtr account = accounts->at(i);
        ui->instanceAccountSelector->addItem(getFaceForAccount(account), account->profileName(), i);
        if (i == accountIndex)
            ui->instanceAccountSelector->setCurrentIndex(i);
    }
}

QIcon InstanceSettingsPage::getFaceForAccount(MinecraftAccountPtr account)
{
    if (auto face = account->getFace(); !face.isNull()) {
        return face;
    }

    return APPLICATION->getThemedIcon("noaccount");
}

void InstanceSettingsPage::changeInstanceAccount(int index)
{
    auto accounts = APPLICATION->accounts();
    if (index != -1 && accounts->at(index) && ui->instanceAccountGroupBox->isChecked()) {
        auto account = accounts->at(index);
        m_settings->set("InstanceAccountId", account->profileId());
    }
}

void InstanceSettingsPage::on_maxMemSpinBox_valueChanged([[maybe_unused]] int i)
{
    updateThresholds();
}

void InstanceSettingsPage::checkerFinished()
{
    checker.reset();
}

void InstanceSettingsPage::retranslate()
{
    ui->retranslateUi(this);
    ui->customCommands->retranslate();  // TODO: why is this seperate from the others?
    ui->environmentVariables->retranslate();
}

void InstanceSettingsPage::updateThresholds()
{
    auto sysMiB = Sys::getSystemRam() / Sys::mebibyte;
    unsigned int maxMem = ui->maxMemSpinBox->value();
    unsigned int minMem = ui->minMemSpinBox->value();

    QString iconName;

    if (maxMem >= sysMiB) {
        iconName = "status-bad";
        ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation exceeds your system memory capacity."));
    } else if (maxMem > (sysMiB * 0.9)) {
        iconName = "status-yellow";
        ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation approaches your system memory capacity."));
    } else if (maxMem < minMem) {
        iconName = "status-yellow";
        ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation is smaller than the minimum value"));
    } else {
        iconName = "status-good";
        ui->labelMaxMemIcon->setToolTip("");
    }

    {
        auto height = ui->labelMaxMemIcon->fontInfo().pixelSize();
        QIcon icon = APPLICATION->getThemedIcon(iconName);
        QPixmap pix = icon.pixmap(height, height);
        ui->labelMaxMemIcon->setPixmap(pix);
    }
}

void InstanceSettingsPage::on_serverJoinAddressButton_toggled(bool checked)
{
    ui->serverJoinAddress->setEnabled(checked);
}

void InstanceSettingsPage::on_worldJoinButton_toggled(bool checked)
{
    ui->worldsCb->setEnabled(checked);
}
