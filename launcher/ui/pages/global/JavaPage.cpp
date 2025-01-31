// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "JavaPage.h"
#include "BuildConfig.h"
#include "JavaCommon.h"
#include "java/JavaInstall.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/java/InstallJavaDialog.h"
#include "ui_JavaPage.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringListModel>
#include <QTabBar>

#include "ui/dialogs/VersionSelectDialog.h"

#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"

#include <FileSystem.h>
#include <sys.h>
#include "Application.h"
#include "settings/SettingsObject.h"

JavaPage::JavaPage(QWidget* parent) : QWidget(parent), ui(new Ui::JavaPage)
{
    ui->setupUi(this);
    
    m_javaSettings = new JavaSettingsWidget(nullptr, this);
    ui->generalScrollArea->setWidget(m_javaSettings);

    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        ui->managedJavaList->initialize(new JavaInstallList(this, true));
        ui->managedJavaList->setResizeOn(2);
        ui->managedJavaList->selectCurrent();
        ui->managedJavaList->setEmptyString(tr("No managed Java versions are installed"));
        ui->managedJavaList->setEmptyErrorString(tr("Couldn't load the managed Java list!"));
    } else
        ui->tabWidget->tabBar()->hide();
}

JavaPage::~JavaPage()
{
    delete ui;
}

void JavaPage::retranslate()
{
    ui->retranslateUi(this);
}

bool JavaPage::apply()
{
    m_javaSettings->saveSettings();
    JavaCommon::checkJVMArgs(APPLICATION->settings()->get("JvmArgs").toString(), this);
    return true;
}

void JavaPage::on_downloadJavaButton_clicked()
{
    auto jdialog = new Java::InstallDialog({}, nullptr, this);
    jdialog->exec();
    ui->managedJavaList->loadList();
}

void JavaPage::on_removeJavaButton_clicked()
{
    auto version = ui->managedJavaList->selectedVersion();
    auto dcast = std::dynamic_pointer_cast<JavaInstall>(version);
    if (!dcast) {
        return;
    }
    QDir dir(APPLICATION->javaPath());

    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (auto& entry : entries) {
        if (dcast->path.startsWith(entry.canonicalFilePath())) {
            auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                         tr("You are about to remove  the Java installation named \"%1\".\n"
                                                            "Are you sure?")
                                                             .arg(entry.fileName()),
                                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                                ->exec();

            if (response == QMessageBox::Yes) {
                FS::deletePath(entry.canonicalFilePath());
                ui->managedJavaList->loadList();
            }
            break;
        }
    }
}
void JavaPage::on_refreshJavaButton_clicked()
{
    ui->managedJavaList->loadList();
}
