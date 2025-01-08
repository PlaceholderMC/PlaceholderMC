// SPDX-FileCopyrightText: 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "Application.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QString>
#include <QUrl>
#include <algorithm>

#include "QObjectPtr.h"
#include "VersionPage.h"
#include "meta/JsonFormat.h"
#include "tasks/SequentialTask.h"
#include "ui/dialogs/InstallLoaderDialog.h"
#include "ui_VersionPage.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/NewComponentDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/VersionSelectDialog.h"

#include "ui/GuiUtil.h"

#include "DesktopServices.h"
#include "Exception.h"
#include "icons/IconList.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"

#include "meta/Index.h"
#include "meta/VersionList.h"

class IconProxy : public QIdentityProxyModel {
    Q_OBJECT
   public:
    IconProxy(QWidget* parentWidget) : QIdentityProxyModel(parentWidget)
    {
        connect(parentWidget, &QObject::destroyed, this, &IconProxy::widgetGone);
        m_parentWidget = parentWidget;
    }

    virtual QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override
    {
        QVariant var = QIdentityProxyModel::data(proxyIndex, role);
        int column = proxyIndex.column();
        if (column == 0 && role == Qt::DecorationRole && m_parentWidget) {
            if (!var.isNull()) {
                auto string = var.toString();
                if (string == "warning") {
                    return APPLICATION->getThemedIcon("status-yellow");
                } else if (string == "error") {
                    return APPLICATION->getThemedIcon("status-bad");
                }
            }
            return APPLICATION->getThemedIcon("status-good");
        }
        return var;
    }
   private slots:
    void widgetGone() { m_parentWidget = nullptr; }

   private:
    QWidget* m_parentWidget = nullptr;
};

QIcon VersionPage::icon() const
{
    return APPLICATION->icons()->getIcon(m_inst->iconKey());
}
bool VersionPage::shouldDisplay() const
{
    return true;
}

void VersionPage::retranslate()
{
    ui->retranslateUi(this);
}

void VersionPage::openedImpl()
{
    auto const setting_name = QString("WideBarVisibility_%1").arg(id());
    if (!APPLICATION->settings()->contains(setting_name))
        m_wide_bar_setting = APPLICATION->settings()->registerSetting(setting_name);
    else
        m_wide_bar_setting = APPLICATION->settings()->getSetting(setting_name);

    ui->toolBar->setVisibilityState(m_wide_bar_setting->get().toByteArray());
}
void VersionPage::closedImpl()
{
    m_wide_bar_setting->set(ui->toolBar->getVisibilityState());
}

QMenu* VersionPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->toolBar->toggleViewAction());
    return filteredMenu;
}

VersionPage::VersionPage(MinecraftInstance* inst, QWidget* parent) : QMainWindow(parent), ui(new Ui::VersionPage), m_inst(inst)
{
    ui->setupUi(this);

    ui->toolBar->insertSpacer(ui->actionReload);

    m_profile = m_inst->getPackProfile();

    reloadPackProfile();

    auto proxy = new IconProxy(ui->packageView);
    proxy->setSourceModel(m_profile.get());

    m_filterModel = new QSortFilterProxyModel(this);
    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(proxy);
    m_filterModel->setFilterKeyColumn(-1);

    ui->packageView->setModel(m_filterModel);
    ui->packageView->installEventFilter(this);
    ui->packageView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->packageView->setContextMenuPolicy(Qt::CustomContextMenu);

    auto smodel = ui->packageView->selectionModel();
    connect(smodel, &QItemSelectionModel::currentChanged, this, &VersionPage::versionCurrent);
    connect(smodel, &QItemSelectionModel::currentChanged, this, &VersionPage::packageCurrent);
    connect(m_profile.get(), &PackProfile::minecraftChanged, this, &VersionPage::updateVersionControls);
    updateVersionControls();
    preselect(0);
    connect(ui->packageView, &ModListView::customContextMenuRequested, this, &VersionPage::showContextMenu);
    connect(ui->packageView, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        auto component = m_profile->getComponent(index.row());
        component->setEnabled(!component->isEnabled());
    });
    connect(ui->filterEdit, &QLineEdit::textChanged, this, &VersionPage::onFilterTextChanged);
}

VersionPage::~VersionPage()
{
    delete ui;
}

void VersionPage::showContextMenu(const QPoint& pos)
{
    auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
    menu->exec(ui->packageView->mapToGlobal(pos));
    delete menu;
}

void VersionPage::packageCurrent(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        ui->frame->clear();
        return;
    }
    int row = current.row();
    auto patch = m_profile->getComponent(row);
    auto severity = patch->getProblemSeverity();
    switch (severity) {
        case ProblemSeverity::Warning:
            ui->frame->setName(tr("%1 possibly has issues.").arg(patch->getName()));
            break;
        case ProblemSeverity::Error:
            ui->frame->setName(tr("%1 has issues!").arg(patch->getName()));
            break;
        default:
        case ProblemSeverity::None:
            ui->frame->clear();
            return;
    }

    auto& problems = patch->getProblems();
    QString problemOut;
    for (auto& problem : problems) {
        if (problem.m_severity == ProblemSeverity::Error) {
            problemOut += tr("Error: ");
        } else if (problem.m_severity == ProblemSeverity::Warning) {
            problemOut += tr("Warning: ");
        }
        problemOut += problem.m_description;
        problemOut += "\n";
    }
    ui->frame->setDescription(problemOut);
}

void VersionPage::updateVersionControls()
{
    updateButtons();
}

void VersionPage::updateButtons(int row)
{
    if (row == -1)
        row = currentRow();
    auto patch = m_profile->getComponent(row);
    ui->actionRemove->setEnabled(patch && patch->isRemovable());
    ui->actionMove_down->setEnabled(patch && patch->isMoveable());
    ui->actionMove_up->setEnabled(patch && patch->isMoveable());
    ui->actionChange_version->setEnabled(patch && patch->isVersionChangeable(false));
    ui->actionEdit->setEnabled(patch && patch->isCustom());
    ui->actionCustomize->setEnabled(patch && patch->isCustomizable());
    ui->actionRevert->setEnabled(patch && patch->isRevertible());
}

bool VersionPage::reloadPackProfile()
{
    try {
        auto result = m_profile->reload(Net::Mode::Online);
        if (!result) {
            QMessageBox::critical(this, tr("Error"), result.error);
        }
        return result;
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
        return false;
    } catch (...) {
        QMessageBox::critical(this, tr("Error"), tr("Couldn't load the instance profile."));
        return false;
    }
}

void VersionPage::on_actionReload_triggered()
{
    reloadPackProfile();
    m_container->refreshContainer();
}

void VersionPage::on_actionRemove_triggered()
{
    if (!ui->packageView->currentIndex().isValid()) {
        return;
    }
    int index = ui->packageView->currentIndex().row();
    auto component = m_profile->getComponent(index);
    if (component->isCustom()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove \"%1\".\n"
                                                        "This is permanent and will completely remove the custom component.\n\n"
                                                        "Are you sure?")
                                                         .arg(component->getName()),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    // FIXME: use actual model, not reloading.
    if (!m_profile->remove(index)) {
        QMessageBox::critical(this, tr("Error"), tr("Couldn't remove file"));
    }
    updateButtons();
    reloadPackProfile();
    m_container->refreshContainer();
}

void VersionPage::on_actionAdd_to_Minecraft_jar_triggered()
{
    auto list = GuiUtil::BrowseForFiles("jarmod", tr("Select jar mods"), tr("Minecraft.jar mods") + " (*.zip *.jar)",
                                        APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());
    if (!list.empty()) {
        m_profile->installJarMods(list);
    }
    updateButtons();
}

void VersionPage::on_actionReplace_Minecraft_jar_triggered()
{
    auto jarPath = GuiUtil::BrowseForFile("jar", tr("Select jar"), tr("Minecraft.jar replacement") + " (*.jar)",
                                          APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());
    if (!jarPath.isEmpty()) {
        m_profile->installCustomJar(jarPath);
    }
    updateButtons();
}

void VersionPage::on_actionImport_Components_triggered()
{
    QStringList list = GuiUtil::BrowseForFiles("component", tr("Select components"), tr("Components") + " (*.json)",
                                               APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        if (!m_profile->installComponents(list)) {
            QMessageBox::warning(this, tr("Failed to import components"),
                                 tr("Some components could not be imported. Check logs for details"));
        }
    }

    updateButtons();
}

void VersionPage::on_actionAdd_Agents_triggered()
{
    QStringList list = GuiUtil::BrowseForFiles("agent", tr("Select agents"), tr("Java agents") + " (*.jar)",
                                               APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty())
        m_profile->installAgents(list);

    updateButtons();
}

void VersionPage::on_actionMove_up_triggered()
{
    try {
        m_profile->move(currentRow(), PackProfile::MoveUp);
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
    }
    updateButtons();
}

void VersionPage::on_actionMove_down_triggered()
{
    try {
        m_profile->move(currentRow(), PackProfile::MoveDown);
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
    }
    updateButtons();
}

void VersionPage::on_actionChange_version_triggered()
{
    auto versionRow = currentRow();
    if (versionRow == -1) {
        return;
    }
    auto patch = m_profile->getComponent(versionRow);
    auto name = patch->getName();
    auto list = patch->getVersionList();
    list->clearExternalRecommends();
    if (!list) {
        return;
    }
    auto uid = list->uid();

    // recommend the correct lwjgl version for the current minecraft version
    if (uid == "org.lwjgl" || uid == "org.lwjgl3") {
        auto minecraft = m_profile->getComponent("net.minecraft");
        auto lwjglReq = std::find_if(minecraft->m_cachedRequires.cbegin(), minecraft->m_cachedRequires.cend(),
                                     [uid](const Meta::Require& req) -> bool { return req.uid == uid; });
        if (lwjglReq != minecraft->m_cachedRequires.cend()) {
            auto lwjglVersion = !lwjglReq->equalsVersion.isEmpty() ? lwjglReq->equalsVersion : lwjglReq->suggests;
            if (!lwjglVersion.isEmpty()) {
                list->addExternalRecommends({ lwjglVersion });
            }
        }
    }

    VersionSelectDialog vselect(list.get(), tr("Change %1 version").arg(name), this);
    if (uid == "net.fabricmc.intermediary" || uid == "org.quiltmc.hashed") {
        vselect.setEmptyString(tr("No intermediary mappings versions are currently available."));
        vselect.setEmptyErrorString(tr("Couldn't load or download the intermediary mappings version lists!"));
    }
    vselect.setExactIfPresentFilter(BaseVersionList::ParentVersionRole, m_profile->getComponentVersion("net.minecraft"));

    auto currentVersion = patch->getVersion();
    if (!currentVersion.isEmpty()) {
        vselect.setCurrentVersion(currentVersion);
    }
    if (!vselect.exec() || !vselect.selectedVersion())
        return;

    qDebug() << "Change" << uid << "to" << vselect.selectedVersion()->descriptor();
    bool important = false;
    if (uid == "net.minecraft") {
        important = true;
        if (APPLICATION->settings()->get("AutomaticJavaSwitch").toBool() && m_inst->settings()->get("AutomaticJava").toBool() &&
            m_inst->settings()->get("OverrideJavaLocation").toBool()) {
            m_inst->settings()->set("OverrideJavaLocation", false);
            m_inst->settings()->set("JavaPath", "");
        }
    }
    m_profile->setComponentVersion(uid, vselect.selectedVersion()->descriptor(), important);
    m_profile->resolve(Net::Mode::Online);
    m_container->refreshContainer();
}

void VersionPage::on_actionDownload_All_triggered()
{
    if (!APPLICATION->accounts()->anyAccountIsValid()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Cannot download Minecraft or update instances unless you have at least "
                                        "one account added.\nPlease add a Microsoft account."),
                                     QMessageBox::Warning)
            ->show();
        return;
    }

    auto updateTasks = m_inst->createUpdateTask();
    if (updateTasks.isEmpty()) {
        return;
    }
    auto task = makeShared<SequentialTask>();
    for (auto t : updateTasks) {
        task->addTask(t);
    }
    ProgressDialog tDialog(this);
    connect(task.get(), &Task::failed, this, &VersionPage::onGameUpdateError);
    // FIXME: unused return value
    tDialog.execWithTask(task.get());
    updateButtons();
    m_container->refreshContainer();
}

void VersionPage::on_actionInstall_Loader_triggered()
{
    InstallLoaderDialog dialog(m_inst->getPackProfile(), QString(), this);
    dialog.exec();
    m_container->refreshContainer();
}

void VersionPage::on_actionAdd_Empty_triggered()
{
    NewComponentDialog compdialog(QString(), QString(), this);
    QStringList blacklist;
    for (int i = 0; i < m_profile->rowCount(); i++) {
        auto comp = m_profile->getComponent(i);
        blacklist.push_back(comp->getID());
    }
    compdialog.setBlacklist(blacklist);
    if (compdialog.exec()) {
        qDebug() << "name:" << compdialog.name();
        qDebug() << "uid:" << compdialog.uid();
        m_profile->installEmpty(compdialog.uid(), compdialog.name());
    }
}

void VersionPage::on_actionLibrariesFolder_triggered()
{
    DesktopServices::openPath(m_inst->getLocalLibraryPath(), true);
}

void VersionPage::on_actionMinecraftFolder_triggered()
{
    DesktopServices::openPath(m_inst->gameRoot(), true);
}

void VersionPage::versionCurrent(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    currentIdx = current.row();
    updateButtons(currentIdx);
}

void VersionPage::preselect(int row)
{
    if (row < 0) {
        row = 0;
    }
    if (row >= m_profile->rowCount(QModelIndex())) {
        row = m_profile->rowCount(QModelIndex()) - 1;
    }
    if (row < 0) {
        return;
    }
    auto model_index = m_profile->index(row);
    ui->packageView->selectionModel()->select(model_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    updateButtons(row);
}

void VersionPage::onGameUpdateError(QString error)
{
    CustomMessageBox::selectable(this, tr("Error updating instance"), error, QMessageBox::Warning)->show();
}

ComponentPtr VersionPage::current()
{
    auto row = currentRow();
    if (row < 0) {
        return nullptr;
    }
    return m_profile->getComponent(row);
}

int VersionPage::currentRow()
{
    if (ui->packageView->selectionModel()->selectedRows().isEmpty()) {
        return -1;
    }
    return ui->packageView->selectionModel()->selectedRows().first().row();
}

void VersionPage::on_actionCustomize_triggered()
{
    auto version = currentRow();
    if (version == -1) {
        return;
    }
    auto patch = m_profile->getComponent(version);
    if (!patch->getVersionFile()) {
        // TODO: wait for the update task to finish here...
        return;
    }
    if (!m_profile->customize(version)) {
        // TODO: some error box here
    }
    updateButtons();
    preselect(currentIdx);
}

void VersionPage::on_actionEdit_triggered()
{
    auto version = current();
    if (!version) {
        return;
    }
    auto filename = version->getFilename();
    if (!QFileInfo::exists(filename)) {
        qWarning() << "file" << filename << "can't be opened for editing, doesn't exist!";
        return;
    }
    APPLICATION->openJsonEditor(filename);
}

void VersionPage::on_actionRevert_triggered()
{
    auto version = currentRow();
    if (version == -1) {
        return;
    }
    auto component = m_profile->getComponent(version);

    auto response = CustomMessageBox::selectable(this, tr("Confirm Reversion"),
                                                 tr("You are about to revert \"%1\".\n"
                                                    "This is permanent and will completely revert your customizations.\n\n"
                                                    "Are you sure?")
                                                     .arg(component->getName()),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!m_profile->revertToBase(version)) {
        // TODO: some error box here
    }
    updateButtons();
    preselect(currentIdx);
    m_container->refreshContainer();
}

void VersionPage::onFilterTextChanged(const QString& newContents)
{
    m_filterModel->setFilterFixedString(newContents);
}

#include "VersionPage.moc"
