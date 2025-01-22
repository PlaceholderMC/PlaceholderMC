// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2022 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ModFolderPage.h"
#include "ui/dialogs/ExportToModListDialog.h"
#include "ui_ExternalResourcesPage.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <algorithm>

#include "Application.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"

#include "minecraft/PackProfile.h"
#include "minecraft/VersionFilterData.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"

#include "tasks/ConcurrentTask.h"
#include "tasks/Task.h"
#include "ui/dialogs/ProgressDialog.h"

ModFolderPage::ModFolderPage(BaseInstance* inst, std::shared_ptr<ModFolderModel> model, QWidget* parent)
    : ExternalResourcesPage(inst, model, parent), m_model(model)
{
    ui->actionDownloadItem->setText(tr("Download Mods"));
    ui->actionDownloadItem->setToolTip(tr("Download mods from online mod platforms"));
    ui->actionDownloadItem->setEnabled(true);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionDownloadItem);

    connect(ui->actionDownloadItem, &QAction::triggered, this, &ModFolderPage::downloadMods);

    ui->actionUpdateItem->setToolTip(tr("Try to check or update all selected mods (all mods if none are selected)"));
    connect(ui->actionUpdateItem, &QAction::triggered, this, &ModFolderPage::updateMods);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionUpdateItem);

    auto updateMenu = new QMenu(this);

    auto update = updateMenu->addAction(tr("Check for Updates"));
    connect(update, &QAction::triggered, this, &ModFolderPage::updateMods);

    updateMenu->addAction(ui->actionVerifyItemDependencies);
    connect(ui->actionVerifyItemDependencies, &QAction::triggered, this, [this] { updateMods(true); });

    auto depsDisabled = APPLICATION->settings()->getSetting("ModDependenciesDisabled");
    ui->actionVerifyItemDependencies->setVisible(!depsDisabled->get().toBool());
    connect(depsDisabled.get(), &Setting::SettingChanged, this,
            [this](const Setting& setting, const QVariant& value) { ui->actionVerifyItemDependencies->setVisible(!value.toBool()); });

    updateMenu->addAction(ui->actionResetItemMetadata);
    connect(ui->actionResetItemMetadata, &QAction::triggered, this, &ModFolderPage::deleteModMetadata);

    ui->actionUpdateItem->setMenu(updateMenu);

    ui->actionChangeVersion->setToolTip(tr("Change a mod's version."));
    connect(ui->actionChangeVersion, &QAction::triggered, this, &ModFolderPage::changeModVersion);
    ui->actionsToolbar->insertActionAfter(ui->actionUpdateItem, ui->actionChangeVersion);

    ui->actionViewHomepage->setToolTip(tr("View the homepages of all selected mods."));

    ui->actionExportMetadata->setToolTip(tr("Export mod's metadata to text."));
    connect(ui->actionExportMetadata, &QAction::triggered, this, &ModFolderPage::exportModMetadata);
    ui->actionsToolbar->insertActionAfter(ui->actionViewHomepage, ui->actionExportMetadata);

    ui->actionsToolbar->insertActionAfter(ui->actionUpdateItem, ui->actionEnableUpdates);
    ui->actionsToolbar->insertActionAfter(ui->actionEnableUpdates, ui->actionDisableUpdates);
}

bool ModFolderPage::shouldDisplay() const
{
    return true;
}

void ModFolderPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    auto sourceCurrent = m_filterModel->mapToSource(current);
    int row = sourceCurrent.row();
    const Mod& mod = m_model->at(row);
    ui->frame->updateWithMod(mod);
}

void ModFolderPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove mods while the game is running it may crash your game.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    m_model->deleteResources(selection.indexes());
}

void ModFolderPage::downloadMods()
{
    if (m_instance->typeName() != "Minecraft")
        return;  // this is a null instance or a legacy instance

    auto profile = static_cast<MinecraftInstance*>(m_instance)->getPackProfile();
    if (!profile->getModLoaders().has_value()) {
        QMessageBox::critical(this, tr("Error"), tr("Please install a mod loader first!"));
        return;
    }

    ResourceDownload::ModDownloadDialog mdownload(this, m_model, m_instance);
    if (mdownload.exec()) {
        auto tasks = new ConcurrentTask(tr("Download Mods"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](QString reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count())
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();

            tasks->deleteLater();
        });

        for (auto& task : mdownload.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

void ModFolderPage::updateMods(bool includeDeps)
{
    if (m_instance->typeName() != "Minecraft")
        return;  // this is a null instance or a legacy instance

    auto profile = static_cast<MinecraftInstance*>(m_instance)->getPackProfile();
    if (!profile->getModLoaders().has_value()) {
        QMessageBox::critical(this, tr("Error"), tr("Please install a mod loader first!"));
        return;
    }
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Mod updates are unavailable when metadata is disabled!"));
        return;
    }
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response =
            CustomMessageBox::selectable(this, tr("Confirm Update"),
                                         tr("Updating mods while the game is running may cause mod duplication and game crashes.\n"
                                            "The old files may not be deleted as they are in use.\n"
                                            "Are you sure you want to do this?"),
                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();

    auto mods_list = m_model->selectedResources(selection);
    bool use_all = mods_list.empty();
    if (use_all)
        mods_list = m_model->allResources();

    ResourceUpdateDialog update_dialog(this, m_instance, m_model, mods_list, includeDeps, true);
    update_dialog.checkCandidates();

    if (update_dialog.aborted()) {
        CustomMessageBox::selectable(this, tr("Aborted"), tr("The mod updater was aborted!"), QMessageBox::Warning)->show();
        return;
    }
    if (update_dialog.noUpdates()) {
        QString message{ tr("'%1' is up-to-date! :)").arg(mods_list.front()->name()) };
        if (mods_list.size() > 1) {
            if (use_all) {
                message = tr("All mods are up-to-date! :)");
            } else {
                message = tr("All selected mods are up-to-date! :)");
            }
        }
        CustomMessageBox::selectable(this, tr("Update checker"), message)->exec();
        return;
    }

    if (update_dialog.exec()) {
        auto tasks = new ConcurrentTask("Download Mods", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](QString reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }
            tasks->deleteLater();
        });

        for (auto task : update_dialog.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

void ModFolderPage::deleteModMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto selectionCount = m_model->selectedMods(selection).length();
    if (selectionCount == 0)
        return;
    if (selectionCount > 1) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove the metadata for %1 mods.\n"
                                                        "Are you sure?")
                                                         .arg(selectionCount),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }

    m_model->deleteMetadata(selection);
}

void ModFolderPage::changeModVersion()
{
    if (m_instance->typeName() != "Minecraft")
        return;  // this is a null instance or a legacy instance

    auto profile = static_cast<MinecraftInstance*>(m_instance)->getPackProfile();
    if (!profile->getModLoaders().has_value()) {
        QMessageBox::critical(this, tr("Error"), tr("Please install a mod loader first!"));
        return;
    }
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Mod updates are unavailable when metadata is disabled!"));
        return;
    }
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto mods_list = m_model->selectedMods(selection);
    if (mods_list.length() != 1 || mods_list[0]->metadata() == nullptr)
        return;

    ResourceDownload::ModDownloadDialog mdownload(this, m_model, m_instance);
    mdownload.setResourceMetadata((*mods_list.begin())->metadata());
    if (mdownload.exec()) {
        auto tasks = new ConcurrentTask("Download Mods", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](QString reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count())
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();

            tasks->deleteLater();
        });

        for (auto& task : mdownload.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

void ModFolderPage::exportModMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto selectedMods = m_model->selectedMods(selection);
    if (selectedMods.length() == 0)
        selectedMods = m_model->allMods();

    std::sort(selectedMods.begin(), selectedMods.end(), [](const Mod* a, const Mod* b) { return a->name() < b->name(); });
    ExportToModListDialog dlg(m_instance->name(), selectedMods, this);
    dlg.exec();
}

CoreModFolderPage::CoreModFolderPage(BaseInstance* inst, std::shared_ptr<ModFolderModel> mods, QWidget* parent)
    : ModFolderPage(inst, mods, parent)
{
    auto mcInst = dynamic_cast<MinecraftInstance*>(m_instance);
    if (mcInst) {
        auto version = mcInst->getPackProfile();
        if (version && version->getComponent("net.minecraftforge") && version->getComponent("net.minecraft")) {
            auto minecraftCmp = version->getComponent("net.minecraft");
            if (!minecraftCmp->m_loaded) {
                version->reload(Net::Mode::Offline);
                auto update = version->getCurrentTask();
                if (update) {
                    connect(update.get(), &Task::finished, this, [this] {
                        if (m_container) {
                            m_container->refreshContainer();
                        }
                    });
                    update->start();
                }
            }
        }
    }
}

bool CoreModFolderPage::shouldDisplay() const
{
    if (ModFolderPage::shouldDisplay()) {
        auto inst = dynamic_cast<MinecraftInstance*>(m_instance);
        if (!inst)
            return true;

        auto version = inst->getPackProfile();
        if (!version || !version->getComponent("net.minecraftforge") || !version->getComponent("net.minecraft"))
            return false;
        auto minecraftCmp = version->getComponent("net.minecraft");
        return minecraftCmp->m_loaded && minecraftCmp->getReleaseDateTime() < g_VersionFilterData.legacyCutoffDate;
    }
    return false;
}

NilModFolderPage::NilModFolderPage(BaseInstance* inst, std::shared_ptr<ModFolderModel> mods, QWidget* parent)
    : ModFolderPage(inst, mods, parent)
{}

bool NilModFolderPage::shouldDisplay() const
{
    return m_model->dir().exists();
}
