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

#include "Page.h"
#include "StringUtils.h"
#include "ui/widgets/ProjectItem.h"
#include "ui_Page.h"

#include <QInputDialog>

#include "Application.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/NewInstanceDialog.h"

#include "ListModel.h"
#include "modplatform/legacy_ftb/PackFetchTask.h"
#include "modplatform/legacy_ftb/PackInstallTask.h"
#include "modplatform/legacy_ftb/PrivatePackManager.h"

namespace LegacyFTB {

Page::Page(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), dialog(dialog), ui(new Ui::Page)
{
    ftbFetchTask.reset(new PackFetchTask(APPLICATION->network()));
    ftbPrivatePacks.reset(new PrivatePackManager());

    ui->setupUi(this);

    {
        publicFilterModel = new FilterModel(this);
        publicListModel = new ListModel(this);
        publicFilterModel->setSourceModel(publicListModel);

        ui->publicPackList->setModel(publicFilterModel);
        ui->publicPackList->setSortingEnabled(true);
        ui->publicPackList->header()->hide();
        ui->publicPackList->setIndentation(0);
        ui->publicPackList->setIconSize(QSize(42, 42));

        for (int i = 0; i < publicFilterModel->getAvailableSortings().size(); i++) {
            ui->sortByBox->addItem(publicFilterModel->getAvailableSortings().keys().at(i));
        }

        ui->sortByBox->setCurrentText(publicFilterModel->translateCurrentSorting());
    }

    {
        thirdPartyFilterModel = new FilterModel(this);
        thirdPartyModel = new ListModel(this);
        thirdPartyFilterModel->setSourceModel(thirdPartyModel);

        ui->thirdPartyPackList->setModel(thirdPartyFilterModel);
        ui->thirdPartyPackList->setSortingEnabled(true);
        ui->thirdPartyPackList->header()->hide();
        ui->thirdPartyPackList->setIndentation(0);
        ui->thirdPartyPackList->setIconSize(QSize(42, 42));

        thirdPartyFilterModel->setSorting(publicFilterModel->getCurrentSorting());
    }

    {
        privateFilterModel = new FilterModel(this);
        privateListModel = new ListModel(this);
        privateFilterModel->setSourceModel(privateListModel);

        ui->privatePackList->setModel(privateFilterModel);
        ui->privatePackList->setSortingEnabled(true);
        ui->privatePackList->header()->hide();
        ui->privatePackList->setIndentation(0);
        ui->privatePackList->setIconSize(QSize(42, 42));

        privateFilterModel->setSorting(publicFilterModel->getCurrentSorting());
    }

    ui->versionSelectionBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->versionSelectionBox->view()->parentWidget()->setMaximumHeight(300);

    connect(ui->sortByBox, &QComboBox::currentTextChanged, this, &Page::onSortingSelectionChanged);
    connect(ui->versionSelectionBox, &QComboBox::currentTextChanged, this, &Page::onVersionSelectionItemChanged);

    connect(ui->searchEdit, &QLineEdit::textChanged, this, &Page::triggerSearch);

    connect(ui->publicPackList->selectionModel(), &QItemSelectionModel::currentChanged, this, &Page::onPublicPackSelectionChanged);
    connect(ui->thirdPartyPackList->selectionModel(), &QItemSelectionModel::currentChanged, this, &Page::onThirdPartyPackSelectionChanged);
    connect(ui->privatePackList->selectionModel(), &QItemSelectionModel::currentChanged, this, &Page::onPrivatePackSelectionChanged);

    connect(ui->addPackBtn, &QPushButton::clicked, this, &Page::onAddPackClicked);
    connect(ui->removePackBtn, &QPushButton::clicked, this, &Page::onRemovePackClicked);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &Page::onTabChanged);

    // ui->modpackInfo->setOpenExternalLinks(true);

    ui->publicPackList->selectionModel()->reset();
    ui->thirdPartyPackList->selectionModel()->reset();
    ui->privatePackList->selectionModel()->reset();

    ui->publicPackList->setItemDelegate(new ProjectItemDelegate(this));
    ui->thirdPartyPackList->setItemDelegate(new ProjectItemDelegate(this));
    ui->privatePackList->setItemDelegate(new ProjectItemDelegate(this));
    onTabChanged(ui->tabWidget->currentIndex());
}

Page::~Page()
{
    delete ui;
}

bool Page::shouldDisplay() const
{
    return true;
}

void Page::openedImpl()
{
    if (!initialized) {
        connect(ftbFetchTask.get(), &PackFetchTask::finished, this, &Page::ftbPackDataDownloadSuccessfully);
        connect(ftbFetchTask.get(), &PackFetchTask::failed, this, &Page::ftbPackDataDownloadFailed);
        connect(ftbFetchTask.get(), &PackFetchTask::aborted, this, &Page::ftbPackDataDownloadAborted);

        connect(ftbFetchTask.get(), &PackFetchTask::privateFileDownloadFinished, this, &Page::ftbPrivatePackDataDownloadSuccessfully);
        connect(ftbFetchTask.get(), &PackFetchTask::privateFileDownloadFailed, this, &Page::ftbPrivatePackDataDownloadFailed);

        ftbFetchTask->fetch();
        ftbPrivatePacks->load();
        ftbFetchTask->fetchPrivate(ftbPrivatePacks->getCurrentPackCodes().values());
        initialized = true;
    }
    suggestCurrent();
}

void Page::retranslate()
{
    ui->retranslateUi(this);
}

void Page::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (selected.broken || selectedVersion.isEmpty()) {
        dialog->setSuggestedPack();
        return;
    }

    dialog->setSuggestedPack(selected.name, selectedVersion, new PackInstallTask(APPLICATION->network(), selected, selectedVersion));
    QString editedLogoName = selected.logo;
    if (!selected.logo.toLower().startsWith("ftb")) {
        editedLogoName = "ftb_" + editedLogoName;
    }

    if (selected.type == PackType::Public) {
        publicListModel->getLogo(selected.logo,
                                 [this, editedLogoName](QString logo) { dialog->setSuggestedIconFromFile(logo, editedLogoName); });
    } else if (selected.type == PackType::ThirdParty) {
        thirdPartyModel->getLogo(selected.logo,
                                 [this, editedLogoName](QString logo) { dialog->setSuggestedIconFromFile(logo, editedLogoName); });
    } else if (selected.type == PackType::Private) {
        privateListModel->getLogo(selected.logo,
                                  [this, editedLogoName](QString logo) { dialog->setSuggestedIconFromFile(logo, editedLogoName); });
    }
}

void Page::ftbPackDataDownloadSuccessfully(ModpackList publicPacks, ModpackList thirdPartyPacks)
{
    publicListModel->fill(publicPacks);
    thirdPartyModel->fill(thirdPartyPacks);
}

void Page::ftbPackDataDownloadFailed(QString reason)
{
    CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
}

void Page::ftbPackDataDownloadAborted()
{
    CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)->show();
}

void Page::ftbPrivatePackDataDownloadSuccessfully(const Modpack& pack)
{
    privateListModel->addPack(pack);
}

void Page::ftbPrivatePackDataDownloadFailed([[maybe_unused]] QString reason, QString packCode)
{
    auto reply = QMessageBox::question(this, tr("FTB private packs"),
                                       tr("Failed to download pack information for code %1.\nShould it be removed now?").arg(packCode));
    if (reply == QMessageBox::Yes) {
        ftbPrivatePacks->remove(packCode);
    }
}

void Page::onPublicPackSelectionChanged(QModelIndex now, [[maybe_unused]] QModelIndex prev)
{
    if (!now.isValid()) {
        onPackSelectionChanged();
        return;
    }
    Modpack selectedPack = publicFilterModel->data(now, Qt::UserRole).value<Modpack>();
    onPackSelectionChanged(&selectedPack);
}

void Page::onThirdPartyPackSelectionChanged(QModelIndex now, [[maybe_unused]] QModelIndex prev)
{
    if (!now.isValid()) {
        onPackSelectionChanged();
        return;
    }
    Modpack selectedPack = thirdPartyFilterModel->data(now, Qt::UserRole).value<Modpack>();
    onPackSelectionChanged(&selectedPack);
}

void Page::onPrivatePackSelectionChanged(QModelIndex now, [[maybe_unused]] QModelIndex prev)
{
    if (!now.isValid()) {
        onPackSelectionChanged();
        return;
    }
    Modpack selectedPack = privateFilterModel->data(now, Qt::UserRole).value<Modpack>();
    onPackSelectionChanged(&selectedPack);
}

void Page::onPackSelectionChanged(Modpack* pack)
{
    ui->versionSelectionBox->clear();
    if (pack) {
        currentModpackInfo->setHtml(StringUtils::htmlListPatch("Pack by <b>" + pack->author + "</b>" + "<br>Minecraft " + pack->mcVersion +
                                                               "<br>" + "<br>" + pack->description + "<ul><li>" +
                                                               pack->mods.replace(";", "</li><li>") + "</li></ul>"));
        bool currentAdded = false;

        for (int i = 0; i < pack->oldVersions.size(); i++) {
            if (pack->currentVersion == pack->oldVersions.at(i)) {
                currentAdded = true;
            }
            ui->versionSelectionBox->addItem(pack->oldVersions.at(i));
        }

        if (!currentAdded) {
            ui->versionSelectionBox->addItem(pack->currentVersion);
        }
        selected = *pack;
    } else {
        currentModpackInfo->setHtml("");
        ui->versionSelectionBox->clear();
        if (isOpened) {
            dialog->setSuggestedPack();
        }
        return;
    }
    suggestCurrent();
}

void Page::onVersionSelectionItemChanged(QString version)
{
    if (version.isNull() || version.isEmpty()) {
        selectedVersion = "";
        return;
    }

    selectedVersion = version;
    suggestCurrent();
}

void Page::onSortingSelectionChanged(QString sort)
{
    FilterModel::Sorting toSet = publicFilterModel->getAvailableSortings().value(sort);
    publicFilterModel->setSorting(toSet);
    thirdPartyFilterModel->setSorting(toSet);
    privateFilterModel->setSorting(toSet);
}

void Page::onTabChanged(int tab)
{
    if (tab == 1) {
        currentModel = thirdPartyFilterModel;
        currentList = ui->thirdPartyPackList;
        currentModpackInfo = ui->thirdPartyPackDescription;
    } else if (tab == 2) {
        currentModel = privateFilterModel;
        currentList = ui->privatePackList;
        currentModpackInfo = ui->privatePackDescription;
    } else {
        currentModel = publicFilterModel;
        currentList = ui->publicPackList;
        currentModpackInfo = ui->publicPackDescription;
    }

    triggerSearch();

    currentList->selectionModel()->reset();
    QModelIndex idx = currentList->currentIndex();
    if (idx.isValid()) {
        auto pack = currentModel->data(idx, Qt::UserRole).value<Modpack>();
        onPackSelectionChanged(&pack);
    } else {
        onPackSelectionChanged();
    }
}

void Page::onAddPackClicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Add FTB pack"), tr("Enter pack code:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !text.isEmpty()) {
        ftbPrivatePacks->add(text);
        ftbFetchTask->fetchPrivate({ text });
    }
}

void Page::onRemovePackClicked()
{
    auto index = ui->privatePackList->currentIndex();
    if (!index.isValid()) {
        return;
    }
    auto row = index.row();
    Modpack pack = privateListModel->at(row);
    auto answer = QMessageBox::question(this, tr("Remove pack"), tr("Are you sure you want to remove pack %1?").arg(pack.name),
                                        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    ftbPrivatePacks->remove(pack.packCode);
    privateListModel->remove(row);
    onPackSelectionChanged();
}

void Page::triggerSearch()
{
    currentModel->setSearchTerm(ui->searchEdit->text());
}

void Page::setSearchTerm(QString term)
{
    ui->searchEdit->setText(term);
}

QString Page::getSerachTerm() const
{
    return ui->searchEdit->text();
}
}  // namespace LegacyFTB
