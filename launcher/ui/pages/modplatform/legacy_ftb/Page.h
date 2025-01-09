// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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

#pragma once

#include <QTextBrowser>
#include <QTreeView>
#include <QWidget>

#include <Application.h>
#include "QObjectPtr.h"
#include "modplatform/legacy_ftb/PackFetchTask.h"
#include "modplatform/legacy_ftb/PackHelpers.h"
#include "ui/pages/modplatform/ModpackProviderBasePage.h"

class NewInstanceDialog;

namespace LegacyFTB {

namespace Ui {
class Page;
}

class ListModel;
class FilterModel;
class PrivatePackManager;

class Page : public QWidget, public ModpackProviderBasePage {
    Q_OBJECT

   public:
    explicit Page(NewInstanceDialog* dialog, QWidget* parent = 0);
    virtual ~Page();
    QString displayName() const override { return "FTB Legacy"; }
    QIcon icon() const override { return APPLICATION->getThemedIcon("ftb_logo"); }
    QString id() const override { return "legacy_ftb"; }
    QString helpPage() const override { return "FTB-legacy"; }
    bool shouldDisplay() const override;
    void openedImpl() override;
    void retranslate() override;

    /** Programatically set the term in the search bar. */
    virtual void setSearchTerm(QString) override;
    /** Get the current term in the search bar. */
    [[nodiscard]] virtual QString getSerachTerm() const override;

   private:
    void suggestCurrent();
    void onPackSelectionChanged(Modpack* pack = nullptr);

   private slots:
    void ftbPackDataDownloadSuccessfully(ModpackList publicPacks, ModpackList thirdPartyPacks);
    void ftbPackDataDownloadFailed(QString reason);
    void ftbPackDataDownloadAborted();

    void ftbPrivatePackDataDownloadSuccessfully(const Modpack& pack);
    void ftbPrivatePackDataDownloadFailed(QString reason, QString packCode);

    void onSortingSelectionChanged(QString data);
    void onVersionSelectionItemChanged(QString data);

    void onPublicPackSelectionChanged(QModelIndex first, QModelIndex second);
    void onThirdPartyPackSelectionChanged(QModelIndex first, QModelIndex second);
    void onPrivatePackSelectionChanged(QModelIndex first, QModelIndex second);

    void onTabChanged(int tab);

    void onAddPackClicked();
    void onRemovePackClicked();

    void triggerSearch();

   private:
    FilterModel* currentModel = nullptr;
    QTreeView* currentList = nullptr;
    QTextBrowser* currentModpackInfo = nullptr;

    bool initialized = false;
    Modpack selected;
    QString selectedVersion;

    ListModel* publicListModel = nullptr;
    FilterModel* publicFilterModel = nullptr;

    ListModel* thirdPartyModel = nullptr;
    FilterModel* thirdPartyFilterModel = nullptr;

    ListModel* privateListModel = nullptr;
    FilterModel* privateFilterModel = nullptr;

    unique_qobject_ptr<PackFetchTask> ftbFetchTask;
    std::unique_ptr<PrivatePackManager> ftbPrivatePacks;

    NewInstanceDialog* dialog = nullptr;

    Ui::Page* ui = nullptr;
};

}  // namespace LegacyFTB
