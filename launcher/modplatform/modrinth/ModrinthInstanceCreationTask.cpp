#include "ModrinthInstanceCreationTask.h"

#include "Application.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "Json.h"

#include "QObjectPtr.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

#include "minecraft/mod/Mod.h"
#include "modplatform/EnsureMetadataTask.h"
#include "modplatform/helpers/OverrideUtils.h"

#include "modplatform/modrinth/ModrinthPackManifest.h"
#include "net/ChecksumValidator.h"

#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "settings/INISettingsObject.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/pages/modplatform/OptionalModDialog.h"

#include <QAbstractButton>
#include <QFileInfo>
#include <QHash>
#include <vector>

bool ModrinthCreationTask::abort()
{
    if (!canAbort())
        return false;

    m_abort = true;
    if (m_task)
        m_task->abort();
    return Task::abort();
}

bool ModrinthCreationTask::updateInstance()
{
    auto instance_list = APPLICATION->instances();

    // FIXME: How to handle situations when there's more than one install already for a given modpack?
    InstancePtr inst;
    if (auto original_id = originalInstanceID(); !original_id.isEmpty()) {
        inst = instance_list->getInstanceById(original_id);
        Q_ASSERT(inst);
    } else {
        inst = instance_list->getInstanceByManagedName(originalName());

        if (!inst) {
            inst = instance_list->getInstanceById(originalName());

            if (!inst)
                return false;
        }
    }

    QString index_path = FS::PathCombine(m_stagingPath, "modrinth.index.json");
    if (!parseManifest(index_path, m_files, true, false))
        return false;

    auto version_name = inst->getManagedPackVersionName();
    m_root_path = QFileInfo(inst->gameRoot()).fileName();
    auto version_str = !version_name.isEmpty() ? tr(" (version %1)").arg(version_name) : "";

    if (shouldConfirmUpdate()) {
        auto should_update = askIfShouldUpdate(m_parent, version_str);
        if (should_update == ShouldUpdate::SkipUpdating)
            return false;
        if (should_update == ShouldUpdate::Cancel) {
            m_abort = true;
            return false;
        }
    }

    // Remove repeated files, we don't need to download them!
    QDir old_inst_dir(inst->instanceRoot());

    QString old_index_folder(FS::PathCombine(old_inst_dir.absolutePath(), "mrpack"));

    QString old_index_path(FS::PathCombine(old_index_folder, "modrinth.index.json"));
    QFileInfo old_index_file(old_index_path);
    if (old_index_file.exists()) {
        std::vector<Modrinth::File> old_files;
        parseManifest(old_index_path, old_files, false, false);

        // Let's remove all duplicated, identical resources!
        auto files_iterator = m_files.begin();
    begin:
        while (files_iterator != m_files.end()) {
            auto const& file = *files_iterator;

            auto old_files_iterator = old_files.begin();
            while (old_files_iterator != old_files.end()) {
                auto const& old_file = *old_files_iterator;

                if (old_file.hash == file.hash) {
                    qDebug() << "Removed file at" << file.path << "from list of downloads";
                    files_iterator = m_files.erase(files_iterator);
                    old_files_iterator = old_files.erase(old_files_iterator);
                    goto begin;  // Sorry :c
                }

                old_files_iterator++;
            }

            files_iterator++;
        }

        QDir old_minecraft_dir(inst->gameRoot());

        // Some files were removed from the old version, and some will be downloaded in an updated version,
        // so we're fine removing them!
        if (!old_files.empty()) {
            for (auto const& file : old_files) {
                if (file.path.isEmpty())
                    continue;
                qDebug() << "Scheduling" << file.path << "for removal";
                m_files_to_remove.append(old_minecraft_dir.absoluteFilePath(file.path));
            }
        }

        // We will remove all the previous overrides, to prevent duplicate files!
        // TODO: Currently 'overrides' will always override the stuff on update. How do we preserve unchanged overrides?
        // FIXME: We may want to do something about disabled mods.
        auto old_overrides = Override::readOverrides("overrides", old_index_folder);
        for (const auto& entry : old_overrides) {
            if (entry.isEmpty())
                continue;
            qDebug() << "Scheduling" << entry << "for removal";
            m_files_to_remove.append(old_minecraft_dir.absoluteFilePath(entry));
        }

        auto old_client_overrides = Override::readOverrides("client-overrides", old_index_folder);
        for (const auto& entry : old_client_overrides) {
            if (entry.isEmpty())
                continue;
            qDebug() << "Scheduling" << entry << "for removal";
            m_files_to_remove.append(old_minecraft_dir.absoluteFilePath(entry));
        }
    } else {
        // We don't have an old index file, so we may duplicate stuff!
        auto dialog = CustomMessageBox::selectable(m_parent, tr("No index file."),
                                                   tr("We couldn't find a suitable index file for the older version. This may cause some "
                                                      "of the files to be duplicated. Do you want to continue?"),
                                                   QMessageBox::Warning, QMessageBox::Ok | QMessageBox::Cancel);

        if (dialog->exec() == QDialog::DialogCode::Rejected) {
            m_abort = true;
            return false;
        }
    }

    setOverride(true, inst->id());
    qDebug() << "Will override instance!";

    m_instance = inst;

    // We let it go through the createInstance() stage, just with a couple modifications for updating
    return false;
}

// https://docs.modrinth.com/docs/modpacks/format_definition/
bool ModrinthCreationTask::createInstance()
{
    QEventLoop loop;

    QString parent_folder(FS::PathCombine(m_stagingPath, "mrpack"));

    QString index_path = FS::PathCombine(m_stagingPath, "modrinth.index.json");
    if (m_files.empty() && !parseManifest(index_path, m_files, true, true))
        return false;

    // Keep index file in case we need it some other time (like when changing versions)
    QString new_index_place(FS::PathCombine(parent_folder, "modrinth.index.json"));
    FS::ensureFilePathExists(new_index_place);
    FS::move(index_path, new_index_place);

    auto mcPath = FS::PathCombine(m_stagingPath, m_root_path);

    auto override_path = FS::PathCombine(m_stagingPath, "overrides");
    if (QFile::exists(override_path)) {
        // Create a list of overrides in "overrides.txt" inside mrpack/
        Override::createOverrides("overrides", parent_folder, override_path);

        // Apply the overrides
        if (!FS::move(override_path, mcPath)) {
            setError(tr("Could not rename the overrides folder:\n") + "overrides");
            return false;
        }
    }

    // Do client overrides
    auto client_override_path = FS::PathCombine(m_stagingPath, "client-overrides");
    if (QFile::exists(client_override_path)) {
        // Create a list of overrides in "client-overrides.txt" inside mrpack/
        Override::createOverrides("client-overrides", parent_folder, client_override_path);

        // Apply the overrides
        if (!FS::overrideFolder(mcPath, client_override_path)) {
            setError(tr("Could not rename the client overrides folder:\n") + "client overrides");
            return false;
        }
    }

    QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
    auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
    MinecraftInstance instance(m_globalSettings, instanceSettings, m_stagingPath);

    auto components = instance.getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion("net.minecraft", m_minecraft_version, true);

    if (!m_fabric_version.isEmpty())
        components->setComponentVersion("net.fabricmc.fabric-loader", m_fabric_version);
    if (!m_quilt_version.isEmpty())
        components->setComponentVersion("org.quiltmc.quilt-loader", m_quilt_version);
    if (!m_forge_version.isEmpty())
        components->setComponentVersion("net.minecraftforge", m_forge_version);
    if (!m_neoForge_version.isEmpty())
        components->setComponentVersion("net.neoforged", m_neoForge_version);

    if (m_instIcon != "default") {
        instance.setIconKey(m_instIcon);
    } else if (!m_managed_id.isEmpty()) {
        instance.setIconKey("modrinth");
    }

    // Don't add managed info to packs without an ID (most likely imported from ZIP)
    if (!m_managed_id.isEmpty())
        instance.setManagedPack("modrinth", m_managed_id, m_managed_name, m_managed_version_id, version());
    else
        instance.setManagedPack("modrinth", "", name(), "", "");

    instance.setName(name());
    instance.saveNow();

    auto downloadMods = makeShared<NetJob>(tr("Mod Download Modrinth"), APPLICATION->network());

    auto root_modpack_path = FS::PathCombine(m_stagingPath, m_root_path);
    auto root_modpack_url = QUrl::fromLocalFile(root_modpack_path);
    // TODO make this work with other sorts of resource
    QHash<QString, Resource*> resources;
    for (auto file : m_files) {
        auto fileName = file.path;
        fileName = FS::RemoveInvalidPathChars(fileName);
        auto file_path = FS::PathCombine(root_modpack_path, fileName);
        if (!root_modpack_url.isParentOf(QUrl::fromLocalFile(file_path))) {
            // This means we somehow got out of the root folder, so abort here to prevent exploits
            setError(tr("One of the files has a path that leads to an arbitrary location (%1). This is a security risk and isn't allowed.")
                         .arg(fileName));
            return false;
        }
        if (fileName.startsWith("mods/")) {
            auto mod = new Mod(file_path);
            ModDetails d;
            d.mod_id = file_path;
            mod->setDetails(d);
            resources[file.hash.toHex()] = mod;
        }
        if (file.downloads.empty()) {
            setError(tr("The file '%1' is missing a download link. This is invalid in the pack format.").arg(fileName));
            return false;
        }
        qDebug() << "Will try to download" << file.downloads.front() << "to" << file_path;
        auto dl = Net::ApiDownload::makeFile(file.downloads.dequeue(), file_path);
        dl->addValidator(new Net::ChecksumValidator(file.hashAlgorithm, file.hash));
        downloadMods->addNetAction(dl);
        if (!file.downloads.empty()) {
            // FIXME: This really needs to be put into a ConcurrentTask of
            // MultipleOptionsTask's , once those exist :)
            auto param = dl.toWeakRef();
            connect(dl.get(), &Task::failed, [&file, file_path, param, downloadMods] {
                auto ndl = Net::ApiDownload::makeFile(file.downloads.dequeue(), file_path);
                ndl->addValidator(new Net::ChecksumValidator(file.hashAlgorithm, file.hash));
                downloadMods->addNetAction(ndl);
                if (auto shared = param.lock())
                    shared->succeeded();
            });
        }
    }

    bool ended_well = false;

    connect(downloadMods.get(), &NetJob::succeeded, this, [&ended_well]() { ended_well = true; });
    connect(downloadMods.get(), &NetJob::failed, [this, &ended_well](const QString& reason) {
        ended_well = false;
        setError(reason);
    });
    connect(downloadMods.get(), &NetJob::finished, &loop, &QEventLoop::quit);
    connect(downloadMods.get(), &NetJob::progress, [this](qint64 current, qint64 total) {
        setDetails(tr("%1 out of %2 complete").arg(current).arg(total));
        setProgress(current, total);
    });
    connect(downloadMods.get(), &NetJob::stepProgress, this, &ModrinthCreationTask::propagateStepProgress);

    setStatus(tr("Downloading mods..."));
    downloadMods->start();
    m_task = downloadMods;

    loop.exec();

    if (!ended_well) {
        for (auto resource : resources) {
            delete resource;
        }
        return ended_well;
    }

    QEventLoop ensureMetaLoop;
    QDir folder = FS::PathCombine(instance.modsRoot(), ".index");
    auto ensureMetadataTask = makeShared<EnsureMetadataTask>(resources, folder, ModPlatform::ResourceProvider::MODRINTH);
    ensureMetadataTask->setLockUpdate(true);
    connect(ensureMetadataTask.get(), &Task::succeeded, this, [&ended_well]() { ended_well = true; });
    connect(ensureMetadataTask.get(), &Task::finished, &ensureMetaLoop, &QEventLoop::quit);
    connect(ensureMetadataTask.get(), &Task::progress, [this](qint64 current, qint64 total) {
        setDetails(tr("%1 out of %2 complete").arg(current).arg(total));
        setProgress(current, total);
    });
    connect(ensureMetadataTask.get(), &Task::stepProgress, this, &ModrinthCreationTask::propagateStepProgress);

    ensureMetadataTask->start();
    m_task = ensureMetadataTask;

    ensureMetaLoop.exec();
    for (auto resource : resources) {
        delete resource;
    }
    resources.clear();

    // Update information of the already installed instance, if any.
    if (m_instance && ended_well) {
        setAbortable(false);
        auto inst = m_instance.value();

        // Only change the name if it didn't use a custom name, so that the previous custom name
        // is preserved, but if we're using the original one, we update the version string.
        // NOTE: This needs to come before the copyManagedPack call!
        if (inst->name().contains(inst->getManagedPackVersionName()) && inst->name() != instance.name()) {
            if (askForChangingInstanceName(m_parent, inst->name(), instance.name()) == InstanceNameChange::ShouldChange)
                inst->setName(instance.name());
        }

        inst->copyManagedPack(instance);
    }

    return ended_well;
}

bool ModrinthCreationTask::parseManifest(const QString& index_path,
                                         std::vector<Modrinth::File>& files,
                                         bool set_internal_data,
                                         bool show_optional_dialog)
{
    try {
        auto doc = Json::requireDocument(index_path);
        auto obj = Json::requireObject(doc, "modrinth.index.json");
        int formatVersion = Json::requireInteger(obj, "formatVersion", "modrinth.index.json");
        if (formatVersion == 1) {
            auto game = Json::requireString(obj, "game", "modrinth.index.json");
            if (game != "minecraft") {
                throw JSONValidationError("Unknown game: " + game);
            }

            if (set_internal_data) {
                if (m_managed_version_id.isEmpty())
                    m_managed_version_id = Json::ensureString(obj, "versionId", {}, "Managed ID");
                m_managed_name = Json::ensureString(obj, "name", {}, "Managed Name");
            }

            auto jsonFiles = Json::requireIsArrayOf<QJsonObject>(obj, "files", "modrinth.index.json");
            std::vector<Modrinth::File> optionalFiles;
            for (const auto& modInfo : jsonFiles) {
                Modrinth::File file;
                file.path = Json::requireString(modInfo, "path").replace("\\", "/");

                auto env = Json::ensureObject(modInfo, "env");
                // 'env' field is optional
                if (!env.isEmpty()) {
                    QString support = Json::ensureString(env, "client", "unsupported");
                    if (support == "unsupported") {
                        continue;
                    } else if (support == "optional") {
                        file.required = false;
                    }
                }

                QJsonObject hashes = Json::requireObject(modInfo, "hashes");
                file.hash = QByteArray::fromHex(Json::requireString(hashes, "sha512").toLatin1());
                file.hashAlgorithm = QCryptographicHash::Sha512;

                // Do not use requireUrl, which uses StrictMode, instead use QUrl's default TolerantMode
                // (as Modrinth seems to incorrectly handle spaces)

                auto download_arr = Json::ensureArray(modInfo, "downloads");
                for (auto download : download_arr) {
                    qWarning() << download.toString();
                    bool is_last = download.toString() == download_arr.last().toString();

                    auto download_url = QUrl(download.toString());

                    if (!download_url.isValid()) {
                        qDebug()
                            << QString("Download URL (%1) for %2 is not a correctly formatted URL").arg(download_url.toString(), file.path);
                        if (is_last && file.downloads.isEmpty())
                            throw JSONValidationError(tr("Download URL for %1 is not a correctly formatted URL").arg(file.path));
                    } else {
                        file.downloads.push_back(download_url);
                    }
                }

                (file.required ? files : optionalFiles).push_back(file);
            }

            if (!optionalFiles.empty()) {
                QStringList oFiles;
                for (auto file : optionalFiles)
                    oFiles.push_back(file.path);
                OptionalModDialog optionalModDialog(m_parent, oFiles);
                if (optionalModDialog.exec() == QDialog::Rejected) {
                    emitAborted();
                    return false;
                }

                auto selectedMods = optionalModDialog.getResult();
                for (auto file : optionalFiles) {
                    if (selectedMods.contains(file.path)) {
                        file.required = true;
                    } else {
                        file.path += ".disabled";
                    }
                    files.push_back(file);
                }
            }
            if (set_internal_data) {
                auto dependencies = Json::requireObject(obj, "dependencies", "modrinth.index.json");
                for (auto it = dependencies.begin(), end = dependencies.end(); it != end; ++it) {
                    QString name = it.key();
                    if (name == "minecraft") {
                        m_minecraft_version = Json::requireString(*it, "Minecraft version");
                    } else if (name == "fabric-loader") {
                        m_fabric_version = Json::requireString(*it, "Fabric Loader version");
                    } else if (name == "quilt-loader") {
                        m_quilt_version = Json::requireString(*it, "Quilt Loader version");
                    } else if (name == "forge") {
                        m_forge_version = Json::requireString(*it, "Forge version");
                    } else if (name == "neoforge") {
                        m_neoForge_version = Json::requireString(*it, "NeoForge version");
                    } else {
                        throw JSONValidationError("Unknown dependency type: " + name);
                    }
                }
            }
        } else {
            throw JSONValidationError(QStringLiteral("Unknown format version: %s").arg(formatVersion));
        }

    } catch (const JSONValidationError& e) {
        setError(tr("Could not understand pack index:\n") + e.cause());
        return false;
    }

    return true;
}
