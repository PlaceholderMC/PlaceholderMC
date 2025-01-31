// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
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

#include "JavaSettingsWidget.h"
#include <Application.h>
#include <BuildConfig.h>
#include <JavaCommon.h>
#include <settings/Setting.h>
#include <ui/java/InstallJavaDialog.h>

#include <FileSystem.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <sys.h>
#include <ui/dialogs/CustomMessageBox.h>
#include <ui/dialogs/VersionSelectDialog.h>
#include <QFileDialog>
#include <QFileInfo>
#include <memory>

#include "ui_JavaSettingsWidget.h"

JavaSettingsWidget::JavaSettingsWidget(InstancePtr instance, QWidget* parent)
    : QWidget(parent), m_instance(std::move(instance)), m_ui(new Ui::JavaSettingsWidget)
{
    m_ui->setupUi(this);

    if (m_instance == nullptr) {
        m_ui->javaDownloadBtn->hide();
        if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
            connect(m_ui->autodetectJavaCheckBox, &QCheckBox::stateChanged, this, [this](bool state) {
                m_ui->autodownloadJavaCheckBox->setEnabled(state);
                if (!state)
                    m_ui->autodownloadJavaCheckBox->setChecked(false);
            });
        } else {
            m_ui->autodownloadJavaCheckBox->hide();
        }
    } else {
        m_ui->javaDownloadBtn->setVisible(BuildConfig.JAVA_DOWNLOADER_ENABLED);
        m_ui->skipWizardCheckBox->hide();
        m_ui->autodetectJavaCheckBox->hide();
        m_ui->autodownloadJavaCheckBox->hide();

        m_ui->javaInstallationGroupBox->setCheckable(true);
        m_ui->memoryGroupBox->setCheckable(true);
        m_ui->javaArgumentsGroupBox->setCheckable(true);

        SettingsObjectPtr settings = m_instance->settings();

        connect(settings->getSetting("OverrideJavaLocation").get(), &Setting::SettingChanged, m_ui->javaInstallationGroupBox,
                [this, settings] { m_ui->javaInstallationGroupBox->setChecked(settings->get("OverrideJavaLocation").toBool()); });
        connect(settings->getSetting("JavaPath").get(), &Setting::SettingChanged, m_ui->javaInstallationGroupBox,
                [this, settings] { m_ui->javaPathTextBox->setText(settings->get("JavaPath").toString()); });

        connect(m_ui->javaDownloadBtn, &QPushButton::clicked, this, [this] {
            auto javaDialog = new Java::InstallDialog({}, m_instance.get(), this);
            javaDialog->exec();
        });
    }

    connect(m_ui->javaTestBtn, &QPushButton::clicked, this, &JavaSettingsWidget::onJavaTest);
    connect(m_ui->javaDetectBtn, &QPushButton::clicked, this, &JavaSettingsWidget::onJavaAutodetect);
    connect(m_ui->javaBrowseBtn, &QPushButton::clicked, this, &JavaSettingsWidget::onJavaBrowse);

    connect(m_ui->maxMemSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &JavaSettingsWidget::updateThresholds);
    connect(m_ui->minMemSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &JavaSettingsWidget::updateThresholds);

    loadSettings();
    updateThresholds();
}

JavaSettingsWidget::~JavaSettingsWidget()
{
    delete m_ui;
}

void JavaSettingsWidget::loadSettings()
{
    SettingsObjectPtr settings;

    if (m_instance != nullptr)
        settings = m_instance->settings();
    else
        settings = APPLICATION->settings();

    // Java Settings
    m_ui->javaInstallationGroupBox->setChecked(settings->get("OverrideJavaLocation").toBool());
    m_ui->javaPathTextBox->setText(settings->get("JavaPath").toString());

    m_ui->skipCompatibilityCheckBox->setChecked(settings->get("IgnoreJavaCompatibility").toBool());

    m_ui->javaArgumentsGroupBox->setChecked(m_instance == nullptr || settings->get("OverrideJavaArgs").toBool());
    m_ui->jvmArgsTextBox->setPlainText(settings->get("JvmArgs").toString());

    if (m_instance == nullptr) {
        m_ui->skipWizardCheckBox->setChecked(settings->get("IgnoreJavaWizard").toBool());
        m_ui->autodetectJavaCheckBox->setChecked(settings->get("AutomaticJavaSwitch").toBool());
        m_ui->autodetectJavaCheckBox->stateChanged(m_ui->autodetectJavaCheckBox->isChecked());
        m_ui->autodownloadJavaCheckBox->setChecked(settings->get("AutomaticJavaDownload").toBool());
    }

    // Memory
    m_ui->memoryGroupBox->setChecked(m_instance == nullptr || settings->get("OverrideMemory").toBool());
    int min = settings->get("MinMemAlloc").toInt();
    int max = settings->get("MaxMemAlloc").toInt();
    if (min < max) {
        m_ui->minMemSpinBox->setValue(min);
        m_ui->maxMemSpinBox->setValue(max);
    } else {
        m_ui->minMemSpinBox->setValue(max);
        m_ui->maxMemSpinBox->setValue(min);
    }
    m_ui->permGenSpinBox->setValue(settings->get("PermGen").toInt());

    // Java arguments
    m_ui->javaArgumentsGroupBox->setChecked(m_instance == nullptr || settings->get("OverrideJavaArgs").toBool());
    m_ui->jvmArgsTextBox->setPlainText(settings->get("JvmArgs").toString());
}

void JavaSettingsWidget::saveSettings()
{
    SettingsObjectPtr settings;

    if (m_instance != nullptr)
        settings = m_instance->settings();
    else
        settings = APPLICATION->settings();

    SettingsObject::Lock lock(settings);

    // Java Install Settings
    bool javaInstall = m_instance == nullptr || m_ui->javaInstallationGroupBox->isChecked();

    if (m_instance != nullptr)
        settings->set("OverrideJavaLocation", javaInstall);

    if (javaInstall) {
        settings->set("JavaPath", m_ui->javaPathTextBox->text());
        settings->set("IgnoreJavaCompatibility", m_ui->skipCompatibilityCheckBox->isChecked());
    } else {
        settings->reset("JavaPath");
        settings->reset("IgnoreJavaCompatibility");
    }

    if (m_instance == nullptr) {
        settings->set("IgnoreJavaWizard", m_ui->skipWizardCheckBox->isChecked());
        settings->set("AutomaticJavaSwitch", m_ui->autodetectJavaCheckBox->isChecked());
        settings->set("AutomaticJavaDownload", m_ui->autodownloadJavaCheckBox->isChecked());
    }

    // Memory
    bool memory = m_instance == nullptr || m_ui->memoryGroupBox->isChecked();

    if (m_instance != nullptr)
        settings->set("OverrideMemory", memory);

    if (memory) {
        int min = m_ui->minMemSpinBox->value();
        int max = m_ui->maxMemSpinBox->value();
        if (min < max) {
            settings->set("MinMemAlloc", min);
            settings->set("MaxMemAlloc", max);
        } else {
            settings->set("MinMemAlloc", max);
            settings->set("MaxMemAlloc", min);
        }
        settings->set("PermGen", m_ui->permGenSpinBox->value());
    } else {
        settings->reset("MinMemAlloc");
        settings->reset("MaxMemAlloc");
        settings->reset("PermGen");
    }

    // Java arguments
    bool javaArgs = m_instance == nullptr || m_ui->javaArgumentsGroupBox->isChecked();

    if (m_instance != nullptr)
        settings->set("OverrideJavaArgs", javaArgs);

    if (javaArgs) {
        settings->set("JvmArgs", m_ui->jvmArgsTextBox->toPlainText().replace("\n", " "));
    } else {
        settings->reset("JvmArgs");
    }
}

void JavaSettingsWidget::onJavaBrowse()
{
    QString rawPath = QFileDialog::getOpenFileName(this, tr("Find Java executable"));

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (rawPath.isEmpty()) {
        return;
    }

    QString cookedPath = FS::NormalizePath(rawPath);
    QFileInfo javaInfo(cookedPath);
    if (!javaInfo.exists() || !javaInfo.isExecutable()) {
        return;
    }
    m_ui->javaPathTextBox->setText(cookedPath);
}

void JavaSettingsWidget::onJavaTest()
{
    if (m_checker != nullptr)
        return;

    QString jvmArgs;

    if (m_instance == nullptr || m_ui->javaArgumentsGroupBox->isChecked())
        jvmArgs = m_ui->jvmArgsTextBox->toPlainText().replace("\n", " ");
    else
        jvmArgs = APPLICATION->settings()->get("JvmArgs").toString();

    m_checker.reset(new JavaCommon::TestCheck(this, m_ui->javaPathTextBox->text(), jvmArgs, m_ui->minMemSpinBox->value(),
                                              m_ui->maxMemSpinBox->value(), m_ui->permGenSpinBox->value()));
    connect(m_checker.get(), &JavaCommon::TestCheck::finished, this, [this] { m_checker.reset(); });
    m_checker->run();
}

void JavaSettingsWidget::onJavaAutodetect()
{
    if (JavaUtils::getJavaCheckPath().isEmpty()) {
        JavaCommon::javaCheckNotFound(this);
        return;
    }

    VersionSelectDialog versionDialog(APPLICATION->javalist().get(), tr("Select a Java version"), this, true);
    versionDialog.setResizeOn(2);
    versionDialog.exec();

    if (versionDialog.result() == QDialog::Accepted && versionDialog.selectedVersion()) {
        JavaInstallPtr java = std::dynamic_pointer_cast<JavaInstall>(versionDialog.selectedVersion());
        m_ui->javaPathTextBox->setText(java->path);

        if (!java->is_64bit && m_ui->maxMemSpinBox->value() > 2048) {
            CustomMessageBox::selectable(this, tr("Confirm Selection"),
                                         tr("You selected a 32-bit version of Java.\n"
                                            "This installation does not support more than 2048MiB of RAM.\n"
                                            "Please make sure that the maximum memory value is lower."),
                                         QMessageBox::Warning, QMessageBox::Ok, QMessageBox::Ok)
                ->exec();
        }
    }
}
void JavaSettingsWidget::updateThresholds()
{
    auto sysMiB = Sys::getSystemRam() / Sys::mebibyte;
    unsigned int maxMem = m_ui->maxMemSpinBox->value();
    unsigned int minMem = m_ui->minMemSpinBox->value();

    QString iconName;

    if (maxMem >= sysMiB) {
        iconName = "status-bad";
        m_ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation exceeds your system memory capacity."));
    } else if (maxMem > (sysMiB * 0.9)) {
        iconName = "status-yellow";
        m_ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation approaches your system memory capacity."));
    } else if (maxMem < minMem) {
        iconName = "status-yellow";
        m_ui->labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation is smaller than the minimum value"));
    } else {
        iconName = "status-good";
        m_ui->labelMaxMemIcon->setToolTip("");
    }

    {
        auto height = m_ui->labelMaxMemIcon->fontInfo().pixelSize();
        QIcon icon = APPLICATION->getThemedIcon(iconName);
        QPixmap pix = icon.pixmap(height, height);
        m_ui->labelMaxMemIcon->setPixmap(pix);
    }
}
