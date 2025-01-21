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
 */

#include "ResourceDownloadDialog.h"
#include <QList>

#include <QPushButton>
#include <algorithm>

#include "Application.h"
#include "ResourceDownloadTask.h"

#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"

#include "minecraft/mod/tasks/GetModDependenciesTask.h"
#include "modplatform/ModIndex.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ReviewMessageBox.h"

#include "ui/pages/modplatform/ResourcePage.h"

#include "ui/pages/modplatform/flame/FlameResourcePages.h"
#include "ui/pages/modplatform/modrinth/ModrinthResourcePages.h"

#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "ui/widgets/PageContainer.h"

namespace ResourceDownload {

ResourceDownloadDialog::ResourceDownloadDialog(QWidget* parent, const std::shared_ptr<ResourceFolderModel> base_model)
    : QDialog(parent)
    , m_base_model(base_model)
    , m_buttons(QDialogButtonBox::Help | QDialogButtonBox::Ok | QDialogButtonBox::Cancel)
    , m_vertical_layout(this)
{
    setObjectName(QStringLiteral("ResourceDownloadDialog"));

    resize(std::max(0.5 * parent->width(), 400.0), std::max(0.75 * parent->height(), 400.0));

    setWindowIcon(APPLICATION->getThemedIcon("new"));

    // Bonk Qt over its stupid head and make sure it understands which button is the default one...
    // See: https://stackoverflow.com/questions/24556831/qbuttonbox-set-default-button
    auto OkButton = m_buttons.button(QDialogButtonBox::Ok);
    OkButton->setEnabled(false);
    OkButton->setDefault(true);
    OkButton->setAutoDefault(true);
    OkButton->setText(tr("Review and confirm"));
    OkButton->setShortcut(tr("Ctrl+Return"));

    auto CancelButton = m_buttons.button(QDialogButtonBox::Cancel);
    CancelButton->setDefault(false);
    CancelButton->setAutoDefault(false);

    auto HelpButton = m_buttons.button(QDialogButtonBox::Help);
    HelpButton->setDefault(false);
    HelpButton->setAutoDefault(false);

    setWindowModality(Qt::WindowModal);
}

void ResourceDownloadDialog::accept()
{
    if (!geometrySaveKey().isEmpty())
        APPLICATION->settings()->set(geometrySaveKey(), saveGeometry().toBase64());

    QDialog::accept();
}

void ResourceDownloadDialog::reject()
{
    auto selected = getTasks();
    if (selected.count() > 0) {
        auto reply = CustomMessageBox::selectable(this, tr("Confirmation Needed"),
                                                  tr("You have %1 selected resources.\n"
                                                     "Are you sure you want to close this dialog?")
                                                      .arg(selected.count()),
                                                  QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                         ->exec();
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    if (!geometrySaveKey().isEmpty())
        APPLICATION->settings()->set(geometrySaveKey(), saveGeometry().toBase64());

    QDialog::reject();
}

// NOTE: We can't have this in the ctor because PageContainer calls a virtual function, and so
// won't work with subclasses if we put it in this ctor.
void ResourceDownloadDialog::initializeContainer()
{
    m_container = new PageContainer(this, {}, this);
    m_container->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    m_container->layout()->setContentsMargins(0, 0, 0, 0);
    m_vertical_layout.addWidget(m_container);

    m_container->addButtons(&m_buttons);

    connect(m_container, &PageContainer::selectedPageChanged, this, &ResourceDownloadDialog::selectedPageChanged);
}

void ResourceDownloadDialog::connectButtons()
{
    auto OkButton = m_buttons.button(QDialogButtonBox::Ok);
    OkButton->setToolTip(
        tr("Opens a new popup to review your selected %1 and confirm your selection. Shortcut: Ctrl+Return").arg(resourcesString()));
    connect(OkButton, &QPushButton::clicked, this, &ResourceDownloadDialog::confirm);

    auto CancelButton = m_buttons.button(QDialogButtonBox::Cancel);
    connect(CancelButton, &QPushButton::clicked, this, &ResourceDownloadDialog::reject);

    auto HelpButton = m_buttons.button(QDialogButtonBox::Help);
    connect(HelpButton, &QPushButton::clicked, m_container, &PageContainer::help);
}

void ResourceDownloadDialog::confirm()
{
    auto confirm_dialog = ReviewMessageBox::create(this, tr("Confirm %1 to download").arg(resourcesString()));
    confirm_dialog->retranslateUi(resourcesString());

    QHash<QString, GetModDependenciesTask::PackDependencyExtraInfo> dependencyExtraInfo;
    QStringList depNames;
    if (auto task = getModDependenciesTask(); task) {
        connect(task.get(), &Task::failed, this,
                [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });

        auto weak = task.toWeakRef();
        connect(task.get(), &Task::succeeded, this, [this, weak]() {
            QStringList warnings;
            if (auto task = weak.lock()) {
                warnings = task->warnings();
            }
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->exec();
            }
        });

        // Check for updates
        ProgressDialog progress_dialog(this);
        progress_dialog.setSkipButton(true, tr("Abort"));
        progress_dialog.setWindowTitle(tr("Checking for dependencies..."));
        auto ret = progress_dialog.execWithTask(task.get());

        // If the dialog was skipped / some download error happened
        if (ret == QDialog::DialogCode::Rejected) {
            QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
            return;
        } else {
            for (auto dep : task->getDependecies()) {
                addResource(dep->pack, dep->version);
                depNames << dep->pack->name;
            }
            dependencyExtraInfo = task->getExtraInfo();
        }
    }

    auto selected = getTasks();
    std::sort(selected.begin(), selected.end(), [](const DownloadTaskPtr& a, const DownloadTaskPtr& b) {
        return QString::compare(a->getName(), b->getName(), Qt::CaseInsensitive) < 0;
    });
    for (auto& task : selected) {
        auto extraInfo = dependencyExtraInfo.value(task->getPack()->addonId.toString());
        confirm_dialog->appendResource({ task->getName(), task->getFilename(), task->getCustomPath(),
                                         ModPlatform::ProviderCapabilities::name(task->getProvider()), extraInfo.required_by,
                                         task->getVersion().version_type.toString(), !extraInfo.maybe_installed });
    }

    if (confirm_dialog->exec()) {
        auto deselected = confirm_dialog->deselectedResources();
        for (auto page : m_container->getPages()) {
            auto res = static_cast<ResourcePage*>(page);
            for (auto name : deselected)
                res->removeResourceFromPage(name);
        }

        this->accept();
    } else {
        for (auto name : depNames)
            removeResource(name);
    }
}

bool ResourceDownloadDialog::selectPage(QString pageId)
{
    return m_container->selectPage(pageId);
}

ResourcePage* ResourceDownloadDialog::selectedPage()
{
    ResourcePage* result = dynamic_cast<ResourcePage*>(m_container->selectedPage());
    Q_ASSERT(result != nullptr);
    return result;
}

void ResourceDownloadDialog::addResource(ModPlatform::IndexedPack::Ptr pack, ModPlatform::IndexedVersion& ver)
{
    removeResource(pack->name);
    selectedPage()->addResourceToPage(pack, ver, getBaseModel());
    setButtonStatus();
}

void ResourceDownloadDialog::removeResource(const QString& pack_name)
{
    for (auto page : m_container->getPages()) {
        static_cast<ResourcePage*>(page)->removeResourceFromPage(pack_name);
    }
    setButtonStatus();
}

void ResourceDownloadDialog::setButtonStatus()
{
    auto selected = false;
    for (auto page : m_container->getPages()) {
        auto res = static_cast<ResourcePage*>(page);
        selected = selected || res->hasSelectedPacks();
    }
    m_buttons.button(QDialogButtonBox::Ok)->setEnabled(selected);
}

const QList<ResourceDownloadDialog::DownloadTaskPtr> ResourceDownloadDialog::getTasks()
{
    QList<DownloadTaskPtr> selected;
    for (auto page : m_container->getPages()) {
        auto res = static_cast<ResourcePage*>(page);
        selected.append(res->selectedPacks());
    }
    return selected;
}

void ResourceDownloadDialog::selectedPageChanged(BasePage* previous, BasePage* selected)
{
    auto* prev_page = dynamic_cast<ResourcePage*>(previous);
    if (!prev_page) {
        qCritical() << "Page '" << previous->displayName() << "' in ResourceDownloadDialog is not a ResourcePage!";
        return;
    }

    // Same effect as having a global search bar
    ResourcePage* result = dynamic_cast<ResourcePage*>(selected);
    Q_ASSERT(result != nullptr);
    result->setSearchTerm(prev_page->getSearchTerm());
}

ModDownloadDialog::ModDownloadDialog(QWidget* parent, const std::shared_ptr<ModFolderModel>& mods, BaseInstance* instance)
    : ResourceDownloadDialog(parent, mods), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty())
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toByteArray()));
}

QList<BasePage*> ModDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    auto loaders = static_cast<MinecraftInstance*>(m_instance)->getPackProfile()->getSupportedModLoaders().value();

    if (ModrinthAPI::validateModLoaders(loaders))
        pages.append(ModrinthModPage::create(this, *m_instance));
    if (APPLICATION->capabilities() & Application::SupportsFlame && FlameAPI::validateModLoaders(loaders))
        pages.append(FlameModPage::create(this, *m_instance));

    return pages;
}

GetModDependenciesTask::Ptr ModDownloadDialog::getModDependenciesTask()
{
    if (!APPLICATION->settings()->get("ModDependenciesDisabled").toBool()) {  // dependencies
        if (auto model = dynamic_cast<ModFolderModel*>(getBaseModel().get()); model) {
            QList<std::shared_ptr<GetModDependenciesTask::PackDependency>> selectedVers;
            for (auto& selected : getTasks()) {
                selectedVers.append(std::make_shared<GetModDependenciesTask::PackDependency>(selected->getPack(), selected->getVersion()));
            }

            return makeShared<GetModDependenciesTask>(m_instance, model, selectedVers);
        }
    }
    return nullptr;
}

ResourcePackDownloadDialog::ResourcePackDownloadDialog(QWidget* parent,
                                                       const std::shared_ptr<ResourcePackFolderModel>& resource_packs,
                                                       BaseInstance* instance)
    : ResourceDownloadDialog(parent, resource_packs), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty())
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toByteArray()));
}

QList<BasePage*> ResourcePackDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    pages.append(ModrinthResourcePackPage::create(this, *m_instance));
    if (APPLICATION->capabilities() & Application::SupportsFlame)
        pages.append(FlameResourcePackPage::create(this, *m_instance));

    return pages;
}

TexturePackDownloadDialog::TexturePackDownloadDialog(QWidget* parent,
                                                     const std::shared_ptr<TexturePackFolderModel>& resource_packs,
                                                     BaseInstance* instance)
    : ResourceDownloadDialog(parent, resource_packs), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty())
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toByteArray()));
}

QList<BasePage*> TexturePackDownloadDialog::getPages()
{
    QList<BasePage*> pages;

    pages.append(ModrinthTexturePackPage::create(this, *m_instance));
    if (APPLICATION->capabilities() & Application::SupportsFlame)
        pages.append(FlameTexturePackPage::create(this, *m_instance));

    return pages;
}

ShaderPackDownloadDialog::ShaderPackDownloadDialog(QWidget* parent,
                                                   const std::shared_ptr<ShaderPackFolderModel>& shaders,
                                                   BaseInstance* instance)
    : ResourceDownloadDialog(parent, shaders), m_instance(instance)
{
    setWindowTitle(dialogTitle());

    initializeContainer();
    connectButtons();

    if (!geometrySaveKey().isEmpty())
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get(geometrySaveKey()).toByteArray()));
}

QList<BasePage*> ShaderPackDownloadDialog::getPages()
{
    QList<BasePage*> pages;
    pages.append(ModrinthShaderPackPage::create(this, *m_instance));
    if (APPLICATION->capabilities() & Application::SupportsFlame)
        pages.append(FlameShaderPackPage::create(this, *m_instance));
    return pages;
}

void ResourceDownloadDialog::setResourceMetadata(const std::shared_ptr<Metadata::ModStruct>& meta)
{
    switch (meta->provider) {
        case ModPlatform::ResourceProvider::MODRINTH:
            selectPage(Modrinth::id());
            break;
        case ModPlatform::ResourceProvider::FLAME:
            selectPage(Flame::id());
            break;
    }
    setWindowTitle(tr("Change %1 version").arg(meta->name));
    m_container->hidePageList();
    m_buttons.hide();
    auto page = selectedPage();
    page->openProject(meta->project_id);
}
}  // namespace ResourceDownload
