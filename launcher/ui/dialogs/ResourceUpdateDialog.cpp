#include "ResourceUpdateDialog.h"
#include "Application.h"
#include "ChooseProviderDialog.h"
#include "CustomMessageBox.h"
#include "ProgressDialog.h"
#include "ScrollMessageBox.h"
#include "StringUtils.h"
#include "minecraft/mod/tasks/GetModDependenciesTask.h"
#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "tasks/SequentialTask.h"
#include "ui_ReviewMessageBox.h"

#include "Markdown.h"

#include "tasks/ConcurrentTask.h"

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

#include "modplatform/EnsureMetadataTask.h"
#include "modplatform/flame/FlameCheckUpdate.h"
#include "modplatform/modrinth/ModrinthCheckUpdate.h"

#include <QTextBrowser>
#include <QTreeWidgetItem>

#include <optional>

static std::list<Version> mcVersions(BaseInstance* inst)
{
    return { static_cast<MinecraftInstance*>(inst)->getPackProfile()->getComponent("net.minecraft")->getVersion() };
}

static QList<ModPlatform::ModLoaderType> mcLoadersList(BaseInstance* inst)
{
    return static_cast<MinecraftInstance*>(inst)->getPackProfile()->getModLoadersList();
}

ResourceUpdateDialog::ResourceUpdateDialog(QWidget* parent,
                                           BaseInstance* instance,
                                           const std::shared_ptr<ResourceFolderModel> resource_model,
                                           QList<Resource*>& search_for,
                                           bool include_deps,
                                           bool filter_loaders)
    : ReviewMessageBox(parent, tr("Confirm resources to update"), "")
    , m_parent(parent)
    , m_resource_model(resource_model)
    , m_candidates(search_for)
    , m_second_try_metadata(new ConcurrentTask("Second Metadata Search", APPLICATION->settings()->get("NumberOfConcurrentTasks").toInt()))
    , m_instance(instance)
    , m_include_deps(include_deps)
    , m_filter_loaders(filter_loaders)
{
    ReviewMessageBox::setGeometry(0, 0, 800, 600);

    ui->explainLabel->setText(tr("You're about to update the following resources:"));
    ui->onlyCheckedLabel->setText(tr("Only resources with a check will be updated!"));
}

void ResourceUpdateDialog::checkCandidates()
{
    // Ensure mods have valid metadata
    auto went_well = ensureMetadata();
    if (!went_well) {
        m_aborted = true;
        return;
    }

    // Report failed metadata generation
    if (!m_failed_metadata.empty()) {
        QString text;
        for (const auto& failed : m_failed_metadata) {
            const auto& mod = std::get<0>(failed);
            const auto& reason = std::get<1>(failed);
            text += tr("Mod name: %1<br>File name: %2<br>Reason: %3<br><br>").arg(mod->name(), mod->fileinfo().fileName(), reason);
        }

        ScrollMessageBox message_dialog(m_parent, tr("Metadata generation failed"),
                                        tr("Could not generate metadata for the following resources:<br>"
                                           "Do you wish to proceed without those resources?"),
                                        text);
        message_dialog.setModal(true);
        if (message_dialog.exec() == QDialog::Rejected) {
            m_aborted = true;
            QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
            return;
        }
    }

    auto versions = mcVersions(m_instance);
    auto loadersList = m_filter_loaders ? mcLoadersList(m_instance) : QList<ModPlatform::ModLoaderType>();

    SequentialTask check_task(tr("Checking for updates"));

    if (!m_modrinth_to_update.empty()) {
        m_modrinth_check_task.reset(new ModrinthCheckUpdate(m_modrinth_to_update, versions, loadersList, m_resource_model));
        connect(m_modrinth_check_task.get(), &CheckUpdateTask::checkFailed, this,
                [this](Resource* resource, QString reason, QUrl recover_url) {
                    m_failed_check_update.append({ resource, reason, recover_url });
                });
        check_task.addTask(m_modrinth_check_task);
    }

    if (!m_flame_to_update.empty()) {
        m_flame_check_task.reset(new FlameCheckUpdate(m_flame_to_update, versions, loadersList, m_resource_model));
        connect(m_flame_check_task.get(), &CheckUpdateTask::checkFailed, this,
                [this](Resource* resource, QString reason, QUrl recover_url) {
                    m_failed_check_update.append({ resource, reason, recover_url });
                });
        check_task.addTask(m_flame_check_task);
    }

    connect(&check_task, &Task::failed, this,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });

    connect(&check_task, &Task::succeeded, this, [this, &check_task]() {
        QStringList warnings = check_task.warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->exec();
        }
    });

    // Check for updates
    ProgressDialog progress_dialog(m_parent);
    progress_dialog.setSkipButton(true, tr("Abort"));
    progress_dialog.setWindowTitle(tr("Checking for updates..."));
    auto ret = progress_dialog.execWithTask(&check_task);

    // If the dialog was skipped / some download error happened
    if (ret == QDialog::DialogCode::Rejected) {
        m_aborted = true;
        QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
        return;
    }

    QList<std::shared_ptr<GetModDependenciesTask::PackDependency>> selectedVers;

    // Add found updates for Modrinth
    if (m_modrinth_check_task) {
        auto modrinth_updates = m_modrinth_check_task->getUpdates();
        for (auto& updatable : modrinth_updates) {
            qDebug() << QString("Mod %1 has an update available!").arg(updatable.name);

            appendResource(updatable);
            m_tasks.insert(updatable.name, updatable.download);
        }
        selectedVers.append(m_modrinth_check_task->getDependencies());
    }

    // Add found updated for Flame
    if (m_flame_check_task) {
        auto flame_updates = m_flame_check_task->getUpdates();
        for (auto& updatable : flame_updates) {
            qDebug() << QString("Mod %1 has an update available!").arg(updatable.name);

            appendResource(updatable);
            m_tasks.insert(updatable.name, updatable.download);
        }
        selectedVers.append(m_flame_check_task->getDependencies());
    }

    // Report failed update checking
    if (!m_failed_check_update.empty()) {
        QString text;
        for (const auto& failed : m_failed_check_update) {
            const auto& mod = std::get<0>(failed);
            const auto& reason = std::get<1>(failed);
            const auto& recover_url = std::get<2>(failed);

            qDebug() << mod->name() << " failed to check for updates!";

            text += tr("Mod name: %1").arg(mod->name()) + "<br>";
            if (!reason.isEmpty())
                text += tr("Reason: %1").arg(reason) + "<br>";
            if (!recover_url.isEmpty())
                //: %1 is the link to download it manually
                text += tr("Possible solution: Getting the latest version manually:<br>%1<br>")
                            .arg(QString("<a href='%1'>%1</a>").arg(recover_url.toString()));
            text += "<br>";
        }

        ScrollMessageBox message_dialog(m_parent, tr("Failed to check for updates"),
                                        tr("Could not check or get the following resources for updates:<br>"
                                           "Do you wish to proceed without those resources?"),
                                        text);
        message_dialog.setModal(true);
        if (message_dialog.exec() == QDialog::Rejected) {
            m_aborted = true;
            QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
            return;
        }
    }

    if (m_include_deps && !APPLICATION->settings()->get("ModDependenciesDisabled").toBool()) {  // dependencies
        auto* mod_model = dynamic_cast<ModFolderModel*>(m_resource_model.get());

        if (mod_model != nullptr) {
            auto depTask = makeShared<GetModDependenciesTask>(m_instance, mod_model, selectedVers);

            connect(depTask.get(), &Task::failed, this, [this](const QString& reason) {
                CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec();
            });
            auto weak = depTask.toWeakRef();
            connect(depTask.get(), &Task::succeeded, this, [this, weak]() {
                QStringList warnings;
                if (auto depTask = weak.lock()) {
                    warnings = depTask->warnings();
                }
                if (warnings.count()) {
                    CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->exec();
                }
            });

            ProgressDialog progress_dialog_deps(m_parent);
            progress_dialog_deps.setSkipButton(true, tr("Abort"));
            progress_dialog_deps.setWindowTitle(tr("Checking for dependencies..."));
            auto dret = progress_dialog_deps.execWithTask(depTask.get());

            // If the dialog was skipped / some download error happened
            if (dret == QDialog::DialogCode::Rejected) {
                m_aborted = true;
                QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
                return;
            }
            static FlameAPI api;

            auto dependencyExtraInfo = depTask->getExtraInfo();

            for (const auto& dep : depTask->getDependecies()) {
                auto changelog = dep->version.changelog;
                if (dep->pack->provider == ModPlatform::ResourceProvider::FLAME)
                    changelog = api.getModFileChangelog(dep->version.addonId.toInt(), dep->version.fileId.toInt());
                auto download_task = makeShared<ResourceDownloadTask>(dep->pack, dep->version, m_resource_model);
                auto extraInfo = dependencyExtraInfo.value(dep->version.addonId.toString());
                CheckUpdateTask::Update updatable = {
                    dep->pack->name, dep->version.hash,   tr("Not installed"), dep->version.version,      dep->version.version_type,
                    changelog,       dep->pack->provider, download_task,       !extraInfo.maybe_installed
                };

                appendResource(updatable, extraInfo.required_by);
                m_tasks.insert(updatable.name, updatable.download);
            }
        }
    }

    // If there's no resource to be updated
    if (ui->modTreeWidget->topLevelItemCount() == 0) {
        m_no_updates = true;
    } else {
        // FIXME: Find a more efficient way of doing this!

        // Sort major items in alphabetical order (also sorts the children unfortunately)
        ui->modTreeWidget->sortItems(0, Qt::SortOrder::AscendingOrder);

        // Re-sort the children
        auto* item = ui->modTreeWidget->topLevelItem(0);
        for (int i = 1; item != nullptr; ++i) {
            item->sortChildren(0, Qt::SortOrder::DescendingOrder);
            item = ui->modTreeWidget->topLevelItem(i);
        }
    }

    if (m_aborted || m_no_updates)
        QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
}

// Part 1: Ensure we have a valid metadata
auto ResourceUpdateDialog::ensureMetadata() -> bool
{
    auto index_dir = indexDir();

    SequentialTask seq(tr("Looking for metadata"));

    // A better use of data structures here could remove the need for this QHash
    QHash<QString, bool> should_try_others;
    QList<Resource*> modrinth_tmp;
    QList<Resource*> flame_tmp;

    bool confirm_rest = false;
    bool try_others_rest = false;
    bool skip_rest = false;
    ModPlatform::ResourceProvider provider_rest = ModPlatform::ResourceProvider::MODRINTH;

    // adds resource to list based on provider
    auto addToTmp = [&modrinth_tmp, &flame_tmp](Resource* resource, ModPlatform::ResourceProvider p) {
        switch (p) {
            case ModPlatform::ResourceProvider::MODRINTH:
                modrinth_tmp.push_back(resource);
                break;
            case ModPlatform::ResourceProvider::FLAME:
                flame_tmp.push_back(resource);
                break;
        }
    };

    // ask the user on what provider to seach for the mod first
    for (auto candidate : m_candidates) {
        if (candidate->status() != ResourceStatus::NO_METADATA) {
            onMetadataEnsured(candidate);
            continue;
        }

        if (skip_rest)
            continue;

        if (candidate->type() == ResourceType::FOLDER) {
            continue;
        }

        if (confirm_rest) {
            addToTmp(candidate, provider_rest);
            should_try_others.insert(candidate->internal_id(), try_others_rest);
            continue;
        }

        ChooseProviderDialog chooser(this);
        chooser.setDescription(tr("The resource '%1' does not have a metadata yet. We need to generate it in order to track relevant "
                                  "information on how to update this mod. "
                                  "To do this, please select a mod provider which we can use to check for updates for this mod.")
                                   .arg(candidate->name()));
        auto confirmed = chooser.exec() == QDialog::DialogCode::Accepted;

        auto response = chooser.getResponse();

        if (response.skip_all)
            skip_rest = true;
        if (response.confirm_all) {
            confirm_rest = true;
            provider_rest = response.chosen;
            try_others_rest = response.try_others;
        }

        should_try_others.insert(candidate->internal_id(), response.try_others);

        if (confirmed)
            addToTmp(candidate, response.chosen);
    }

    // prepare task for the modrinth mods
    if (!modrinth_tmp.empty()) {
        auto modrinth_task = makeShared<EnsureMetadataTask>(modrinth_tmp, index_dir, ModPlatform::ResourceProvider::MODRINTH);
        connect(modrinth_task.get(), &EnsureMetadataTask::metadataReady, [this](Resource* candidate) { onMetadataEnsured(candidate); });
        connect(modrinth_task.get(), &EnsureMetadataTask::metadataFailed, [this, &should_try_others](Resource* candidate) {
            onMetadataFailed(candidate, should_try_others.find(candidate->internal_id()).value(), ModPlatform::ResourceProvider::MODRINTH);
        });
        connect(modrinth_task.get(), &EnsureMetadataTask::failed,
                [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });

        if (modrinth_task->getHashingTask())
            seq.addTask(modrinth_task->getHashingTask());

        seq.addTask(modrinth_task);
    }

    // prepare task for the flame mods
    if (!flame_tmp.empty()) {
        auto flame_task = makeShared<EnsureMetadataTask>(flame_tmp, index_dir, ModPlatform::ResourceProvider::FLAME);
        connect(flame_task.get(), &EnsureMetadataTask::metadataReady, [this](Resource* candidate) { onMetadataEnsured(candidate); });
        connect(flame_task.get(), &EnsureMetadataTask::metadataFailed, [this, &should_try_others](Resource* candidate) {
            onMetadataFailed(candidate, should_try_others.find(candidate->internal_id()).value(), ModPlatform::ResourceProvider::FLAME);
        });
        connect(flame_task.get(), &EnsureMetadataTask::failed,
                [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });

        if (flame_task->getHashingTask())
            seq.addTask(flame_task->getHashingTask());

        seq.addTask(flame_task);
    }

    seq.addTask(m_second_try_metadata);

    // execute all the tasks
    ProgressDialog checking_dialog(m_parent);
    checking_dialog.setSkipButton(true, tr("Abort"));
    checking_dialog.setWindowTitle(tr("Generating metadata..."));
    auto ret_metadata = checking_dialog.execWithTask(&seq);

    return (ret_metadata != QDialog::DialogCode::Rejected);
}

void ResourceUpdateDialog::onMetadataEnsured(Resource* resource)
{
    // When the mod is a folder, for instance
    if (!resource->metadata())
        return;

    switch (resource->metadata()->provider) {
        case ModPlatform::ResourceProvider::MODRINTH:
            m_modrinth_to_update.push_back(resource);
            break;
        case ModPlatform::ResourceProvider::FLAME:
            m_flame_to_update.push_back(resource);
            break;
    }
}

ModPlatform::ResourceProvider next(ModPlatform::ResourceProvider p)
{
    switch (p) {
        case ModPlatform::ResourceProvider::MODRINTH:
            return ModPlatform::ResourceProvider::FLAME;
        case ModPlatform::ResourceProvider::FLAME:
            return ModPlatform::ResourceProvider::MODRINTH;
    }

    return ModPlatform::ResourceProvider::FLAME;
}

void ResourceUpdateDialog::onMetadataFailed(Resource* resource, bool try_others, ModPlatform::ResourceProvider first_choice)
{
    if (try_others) {
        auto index_dir = indexDir();

        auto task = makeShared<EnsureMetadataTask>(resource, index_dir, next(first_choice));
        connect(task.get(), &EnsureMetadataTask::metadataReady, [this](Resource* candidate) { onMetadataEnsured(candidate); });
        connect(task.get(), &EnsureMetadataTask::metadataFailed, [this](Resource* candidate) { onMetadataFailed(candidate, false); });
        connect(task.get(), &EnsureMetadataTask::failed,
                [this](const QString& reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });
        if (task->getHashingTask()) {
            auto seq = makeShared<SequentialTask>();
            seq->addTask(task->getHashingTask());
            seq->addTask(task);
            m_second_try_metadata->addTask(seq);
        } else {
            m_second_try_metadata->addTask(task);
        }
    } else {
        QString reason{ tr("Couldn't find a valid version on the selected mod provider(s)") };

        m_failed_metadata.append({ resource, reason });
    }
}

void ResourceUpdateDialog::appendResource(CheckUpdateTask::Update const& info, QStringList requiredBy)
{
    auto item_top = new QTreeWidgetItem(ui->modTreeWidget);
    item_top->setCheckState(0, info.enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    if (!info.enabled) {
        item_top->setToolTip(0, tr("Mod was disabled as it may be already installed."));
    }
    item_top->setText(0, info.name);
    item_top->setExpanded(true);

    auto provider_item = new QTreeWidgetItem(item_top);
    provider_item->setText(0, tr("Provider: %1").arg(ModPlatform::ProviderCapabilities::readableName(info.provider)));

    auto old_version_item = new QTreeWidgetItem(item_top);
    old_version_item->setText(0, tr("Old version: %1").arg(info.old_version));

    auto new_version_item = new QTreeWidgetItem(item_top);
    new_version_item->setText(0, tr("New version: %1").arg(info.new_version));

    if (info.new_version_type.has_value()) {
        auto new_version_type_itme = new QTreeWidgetItem(item_top);
        new_version_type_itme->setText(0, tr("New Version Type: %1").arg(info.new_version_type.value().toString()));
    }

    if (!requiredBy.isEmpty()) {
        auto requiredByItem = new QTreeWidgetItem(item_top);
        if (requiredBy.length() == 1) {
            requiredByItem->setText(0, tr("Required by: %1").arg(requiredBy.back()));
        } else {
            requiredByItem->setText(0, tr("Required by:"));
            auto i = 0;
            for (auto req : requiredBy) {
                auto reqItem = new QTreeWidgetItem(requiredByItem);
                reqItem->setText(0, req);
                reqItem->insertChildren(i++, { reqItem });
            }
        }

        ui->toggleDepsButton->show();
        m_deps << item_top;
    }

    auto changelog_item = new QTreeWidgetItem(item_top);
    changelog_item->setText(0, tr("Changelog of the latest version"));

    auto changelog = new QTreeWidgetItem(changelog_item);
    auto changelog_area = new QTextBrowser();

    QString text = info.changelog;
    if (info.provider == ModPlatform::ResourceProvider::MODRINTH) {
        text = markdownToHTML(info.changelog.toUtf8());
    }

    changelog_area->setHtml(StringUtils::htmlListPatch(text));
    changelog_area->setOpenExternalLinks(true);
    changelog_area->setLineWrapMode(QTextBrowser::LineWrapMode::WidgetWidth);
    changelog_area->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);

    ui->modTreeWidget->setItemWidget(changelog, 0, changelog_area);

    ui->modTreeWidget->addTopLevelItem(item_top);
}

auto ResourceUpdateDialog::getTasks() -> const QList<ResourceDownloadTask::Ptr>
{
    QList<ResourceDownloadTask::Ptr> list;

    auto* item = ui->modTreeWidget->topLevelItem(0);

    for (int i = 1; item != nullptr; ++i) {
        if (item->checkState(0) == Qt::CheckState::Checked) {
            list.push_back(m_tasks.find(item->text(0)).value());
        }

        item = ui->modTreeWidget->topLevelItem(i);
    }

    return list;
}
