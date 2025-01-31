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

#include "NewInstanceDialog.h"
#include "Application.h"
#include "ui/pages/modplatform/ModpackProviderBasePage.h"
#include "ui/pages/modplatform/import_ftb/ImportFTBPage.h"
#include "ui_NewInstanceDialog.h"

#include <BaseVersion.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include <tasks/Task.h>

#include "DesktopServices.h"
#include "IconPickerDialog.h"
#include "ProgressDialog.h"
#include "VersionSelectDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLayout>
#include <QPushButton>
#include <QScreen>
#include <QValidator>
#include <utility>

#include "ui/pages/modplatform/CustomPage.h"
#include "ui/pages/modplatform/ImportPage.h"
#include "ui/pages/modplatform/atlauncher/AtlPage.h"
#include "ui/pages/modplatform/flame/FlamePage.h"
#include "ui/pages/modplatform/legacy_ftb/Page.h"
#include "ui/pages/modplatform/modrinth/ModrinthPage.h"
#include "ui/pages/modplatform/technic/TechnicPage.h"
#include "ui/widgets/PageContainer.h"

NewInstanceDialog::NewInstanceDialog(const QString& initialGroup,
                                     const QString& url,
                                     const QMap<QString, QString>& extra_info,
                                     QWidget* parent)
    : QDialog(parent), ui(new Ui::NewInstanceDialog)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->getThemedIcon("new"));

    InstIconKey = "default";
    ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(initialGroup);
    if (index == -1) {
        index = 1;
        groups.insert(index, initialGroup);
    }
    ui->groupBox->addItems(groups);
    ui->groupBox->setCurrentIndex(index);
    ui->groupBox->lineEdit()->setPlaceholderText(tr("No group"));

    // NOTE: m_buttons must be initialized before PageContainer, because it indirectly accesses m_buttons through setSuggestedPack! Do not
    // move this below.
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    m_container = new PageContainer(this, {}, this);
    m_container->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    m_container->layout()->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout->insertWidget(2, m_container);

    m_container->addButtons(m_buttons);
    connect(m_container, &PageContainer::selectedPageChanged, this, [this](BasePage* previous, BasePage* selected) {
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(creationTask && !instName().isEmpty());
    });

    // Bonk Qt over its stupid head and make sure it understands which button is the default one...
    // See: https://stackoverflow.com/questions/24556831/qbuttonbox-set-default-button
    auto OkButton = m_buttons->button(QDialogButtonBox::Ok);
    OkButton->setDefault(true);
    OkButton->setAutoDefault(true);
    OkButton->setText(tr("OK"));
    connect(OkButton, &QPushButton::clicked, this, &NewInstanceDialog::accept);

    auto CancelButton = m_buttons->button(QDialogButtonBox::Cancel);
    CancelButton->setDefault(false);
    CancelButton->setAutoDefault(false);
    CancelButton->setText(tr("Cancel"));
    connect(CancelButton, &QPushButton::clicked, this, &NewInstanceDialog::reject);

    auto HelpButton = m_buttons->button(QDialogButtonBox::Help);
    HelpButton->setDefault(false);
    HelpButton->setAutoDefault(false);
    HelpButton->setText(tr("Help"));
    connect(HelpButton, &QPushButton::clicked, m_container, &PageContainer::help);

    if (!url.isEmpty()) {
        QUrl actualUrl(url);
        m_container->selectPage("import");
        importPage->setUrl(url);
        importPage->setExtraInfo(extra_info);
    }

    updateDialogState();

    connect(m_container, &PageContainer::selectedPageChanged, this, &NewInstanceDialog::selectedPageChanged);

    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    } else {
        if (APPLICATION->settings()->get("NewInstanceGeometry").isValid()) {
            restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("NewInstanceGeometry").toByteArray()));
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            auto screen = parent->screen();
#else
            auto screen = QGuiApplication::primaryScreen();
#endif
            auto geometry = screen->availableSize();
            resize(width(), qMin(geometry.height() - 50, 710));
        }
    }
}

void NewInstanceDialog::reject()
{
    APPLICATION->settings()->set("NewInstanceGeometry", saveGeometry().toBase64());

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::reject();
}

void NewInstanceDialog::accept()
{
    APPLICATION->settings()->set("NewInstanceGeometry", saveGeometry().toBase64());
    importIconNow();

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::accept();
}

QList<BasePage*> NewInstanceDialog::getPages()
{
    QList<BasePage*> pages;

    importPage = new ImportPage(this);

    pages.append(new CustomPage(this));
    pages.append(importPage);
    pages.append(new AtlPage(this));
    if (APPLICATION->capabilities() & Application::SupportsFlame)
        pages.append(new FlamePage(this));
    pages.append(new LegacyFTB::Page(this));
    pages.append(new FTBImportAPP::ImportFTBPage(this));
    pages.append(new ModrinthPage(this));
    pages.append(new TechnicPage(this));

    return pages;
}

QString NewInstanceDialog::dialogTitle()
{
    return tr("New Instance");
}

NewInstanceDialog::~NewInstanceDialog()
{
    delete ui;
}

void NewInstanceDialog::setSuggestedPack(const QString& name, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion.clear();

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    auto allowOK = task && !instName().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(allowOK);
}

void NewInstanceDialog::setSuggestedPack(const QString& name, QString version, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion = std::move(version);

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    auto allowOK = task && !instName().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(allowOK);
}

void NewInstanceDialog::setSuggestedIconFromFile(const QString& path, const QString& name)
{
    importIcon = true;
    importIconPath = path;
    importIconName = name;

    // Hmm, for some reason they can be to small
    ui->iconButton->setIcon(QIcon(path));
}

void NewInstanceDialog::setSuggestedIcon(const QString& key)
{
    if (key == "default")
        return;

    auto icon = APPLICATION->icons()->getIcon(key);
    importIcon = false;

    ui->iconButton->setIcon(icon);
}

InstanceTask* NewInstanceDialog::extractTask()
{
    InstanceTask* extracted = creationTask.release();

    InstanceName inst_name(ui->instNameTextBox->placeholderText().trimmed(), importVersion);
    inst_name.setName(ui->instNameTextBox->text().trimmed());
    extracted->setName(inst_name);

    extracted->setGroup(instGroup());
    extracted->setIcon(iconKey());
    return extracted;
}

void NewInstanceDialog::updateDialogState()
{
    auto allowOK = creationTask && !instName().isEmpty();
    auto OkButton = m_buttons->button(QDialogButtonBox::Ok);
    if (OkButton->isEnabled() != allowOK) {
        OkButton->setEnabled(allowOK);
    }
}

QString NewInstanceDialog::instName() const
{
    auto result = ui->instNameTextBox->text().trimmed();
    if (result.size()) {
        return result;
    }
    result = ui->instNameTextBox->placeholderText().trimmed();
    if (result.size()) {
        return result;
    }
    return QString();
}

QString NewInstanceDialog::instGroup() const
{
    return ui->groupBox->currentText();
}
QString NewInstanceDialog::iconKey() const
{
    return InstIconKey;
}

void NewInstanceDialog::on_iconButton_clicked()
{
    importIconNow();  // so the user can switch back
    IconPickerDialog dlg(this);
    dlg.execWithSelection(InstIconKey);

    if (dlg.result() == QDialog::Accepted) {
        InstIconKey = dlg.selectedIconKey;
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }
}

void NewInstanceDialog::on_instNameTextBox_textChanged([[maybe_unused]] const QString& arg1)
{
    updateDialogState();
}

void NewInstanceDialog::importIconNow()
{
    if (importIcon) {
        APPLICATION->icons()->installIcon(importIconPath, importIconName);
        InstIconKey = importIconName.mid(0, importIconName.lastIndexOf('.'));
        importIcon = false;
    }
    APPLICATION->settings()->set("NewInstanceGeometry", saveGeometry().toBase64());
}

void NewInstanceDialog::selectedPageChanged(BasePage* previous, BasePage* selected)
{
    auto prevPage = dynamic_cast<ModpackProviderBasePage*>(previous);
    if (prevPage) {
        m_searchTerm = prevPage->getSerachTerm();
    }

    auto nextPage = dynamic_cast<ModpackProviderBasePage*>(selected);
    if (nextPage) {
        nextPage->setSearchTerm(m_searchTerm);
    }
}
