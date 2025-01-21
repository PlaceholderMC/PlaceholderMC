// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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
 */

#include "GetModDependenciesTask.h"

#include <QDebug>
#include <algorithm>
#include <memory>
#include "Json.h"
#include "QObjectPtr.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/MetadataHandler.h"
#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "tasks/SequentialTask.h"
#include "ui/pages/modplatform/ModModel.h"
#include "ui/pages/modplatform/flame/FlameResourceModels.h"
#include "ui/pages/modplatform/modrinth/ModrinthResourceModels.h"

static Version mcVersion(BaseInstance* inst)
{
    return static_cast<MinecraftInstance*>(inst)->getPackProfile()->getComponent("net.minecraft")->getVersion();
}

static ModPlatform::ModLoaderTypes mcLoaders(BaseInstance* inst)
{
    return static_cast<MinecraftInstance*>(inst)->getPackProfile()->getSupportedModLoaders().value();
}

static bool checkDependencies(std::shared_ptr<GetModDependenciesTask::PackDependency> sel,
                              Version mcVersion,
                              ModPlatform::ModLoaderTypes loaders)
{
    return (sel->pack->versions.isEmpty() || sel->version.mcVersion.contains(mcVersion.toString())) &&
           (!loaders || !sel->version.loaders || sel->version.loaders & loaders);
}

GetModDependenciesTask::GetModDependenciesTask(BaseInstance* instance,
                                               ModFolderModel* folder,
                                               QList<std::shared_ptr<PackDependency>> selected)
    : SequentialTask(tr("Get dependencies"))
    , m_selected(selected)
    , m_flame_provider{ ModPlatform::ResourceProvider::FLAME, std::make_shared<ResourceDownload::FlameModModel>(*instance),
                        std::make_shared<FlameAPI>() }
    , m_modrinth_provider{ ModPlatform::ResourceProvider::MODRINTH, std::make_shared<ResourceDownload::ModrinthModModel>(*instance),
                           std::make_shared<ModrinthAPI>() }
    , m_version(mcVersion(instance))
    , m_loaderType(mcLoaders(instance))
{
    for (auto mod : folder->allMods()) {
        m_mods_file_names << mod->fileinfo().fileName();
        if (auto meta = mod->metadata(); meta)
            m_mods.append(meta);
    }
    prepare();
}

void GetModDependenciesTask::prepare()
{
    for (auto sel : m_selected) {
        if (checkDependencies(sel, m_version, m_loaderType))
            for (auto dep : getDependenciesForVersion(sel->version, sel->pack->provider)) {
                addTask(prepareDependencyTask(dep, sel->pack->provider, 20));
            }
    }
}

ModPlatform::Dependency GetModDependenciesTask::getOverride(const ModPlatform::Dependency& dep,
                                                            const ModPlatform::ResourceProvider providerName)
{
    if (auto isQuilt = m_loaderType & ModPlatform::Quilt; isQuilt || m_loaderType & ModPlatform::Fabric) {
        auto overide = ModPlatform::getOverrideDeps();
        auto over = std::find_if(overide.cbegin(), overide.cend(), [dep, providerName, isQuilt](auto o) {
            return o.provider == providerName && dep.addonId == (isQuilt ? o.fabric : o.quilt);
        });
        if (over != overide.cend()) {
            return { isQuilt ? over->quilt : over->fabric, dep.type };
        }
    }
    return dep;
}

QList<ModPlatform::Dependency> GetModDependenciesTask::getDependenciesForVersion(const ModPlatform::IndexedVersion& version,
                                                                                 const ModPlatform::ResourceProvider providerName)
{
    QList<ModPlatform::Dependency> c_dependencies;
    for (auto ver_dep : version.dependencies) {
        if (ver_dep.type != ModPlatform::DependencyType::REQUIRED)
            continue;
        ver_dep = getOverride(ver_dep, providerName);
        auto isOnlyVersion = providerName == ModPlatform::ResourceProvider::MODRINTH && ver_dep.addonId.toString().isEmpty();
        if (auto dep = std::find_if(c_dependencies.begin(), c_dependencies.end(),
                                    [&ver_dep, isOnlyVersion](const ModPlatform::Dependency& i) {
                                        return isOnlyVersion ? i.version == ver_dep.version : i.addonId == ver_dep.addonId;
                                    });
            dep != c_dependencies.end())
            continue;  // check the current dependency list

        if (auto dep = std::find_if(m_selected.begin(), m_selected.end(),
                                    [&ver_dep, providerName, isOnlyVersion](std::shared_ptr<PackDependency> i) {
                                        return i->pack->provider == providerName && (isOnlyVersion ? i->version.version == ver_dep.version
                                                                                                   : i->pack->addonId == ver_dep.addonId);
                                    });
            dep != m_selected.end())
            continue;  // check the selected versions

        if (auto dep = std::find_if(m_mods.begin(), m_mods.end(),
                                    [&ver_dep, providerName, isOnlyVersion](std::shared_ptr<Metadata::ModStruct> i) {
                                        return i->provider == providerName &&
                                               (isOnlyVersion ? i->file_id == ver_dep.version : i->project_id == ver_dep.addonId);
                                    });
            dep != m_mods.end())
            continue;  // check the existing mods

        if (auto dep = std::find_if(m_pack_dependencies.begin(), m_pack_dependencies.end(),
                                    [&ver_dep, providerName, isOnlyVersion](std::shared_ptr<PackDependency> i) {
                                        return i->pack->provider == providerName && (isOnlyVersion ? i->version.version == ver_dep.addonId
                                                                                                   : i->pack->addonId == ver_dep.addonId);
                                    });
            dep != m_pack_dependencies.end())  // check loaded dependencies
            continue;

        c_dependencies.append(ver_dep);
    }
    return c_dependencies;
}

Task::Ptr GetModDependenciesTask::getProjectInfoTask(std::shared_ptr<PackDependency> pDep)
{
    auto provider = pDep->pack->provider == m_flame_provider.name ? m_flame_provider : m_modrinth_provider;
    auto responseInfo = std::make_shared<QByteArray>();
    auto info = provider.api->getProject(pDep->pack->addonId.toString(), responseInfo);
    QObject::connect(info.get(), &NetJob::succeeded, [this, responseInfo, provider, pDep] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*responseInfo, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            removePack(pDep->pack->addonId);
            qWarning() << "Error while parsing JSON response for mod info at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qDebug() << *responseInfo;
            return;
        }
        try {
            auto obj = provider.name == ModPlatform::ResourceProvider::FLAME ? Json::requireObject(Json::requireObject(doc), "data")
                                                                             : Json::requireObject(doc);
            provider.mod->loadIndexedPack(*pDep->pack, obj);
        } catch (const JSONValidationError& e) {
            removePack(pDep->pack->addonId);
            qDebug() << doc;
            qWarning() << "Error while reading mod info: " << e.cause();
        }
    });
    QObject::connect(info.get(), &NetJob::failed, [this, info, pDep] {
        removePack(pDep->pack->addonId);
        m_failed.remove(info.get());
    });
    return info;
}

Task::Ptr GetModDependenciesTask::prepareDependencyTask(const ModPlatform::Dependency& dep,
                                                        const ModPlatform::ResourceProvider providerName,
                                                        int level)
{
    auto pDep = std::make_shared<PackDependency>();
    pDep->dependency = dep;
    pDep->pack = std::make_shared<ModPlatform::IndexedPack>();
    pDep->pack->addonId = dep.addonId;
    pDep->pack->provider = providerName;

    m_pack_dependencies.append(pDep);
    auto provider = providerName == m_flame_provider.name ? m_flame_provider : m_modrinth_provider;

    auto tasks = makeShared<SequentialTask>(
        QString("DependencyInfo: %1").arg(dep.addonId.toString().isEmpty() ? dep.version : dep.addonId.toString()));

    if (!dep.addonId.toString().isEmpty()) {
        tasks->addTask(getProjectInfoTask(pDep));
    }

    ResourceAPI::DependencySearchArgs args = { dep, m_version, m_loaderType };
    ResourceAPI::DependencySearchCallbacks callbacks;
    callbacks.on_fail = [](QString reason, int) {
        qCritical() << tr("A network error occurred. Could not load project dependencies:%1").arg(reason);
    };
    callbacks.on_succeed = [dep, provider, pDep, level, this](auto& doc, [[maybe_unused]] auto& pack) {
        try {
            QJsonArray arr;
            if (dep.version.length() != 0 && doc.isObject()) {
                arr.append(doc.object());
            } else {
                arr = doc.isObject() ? Json::ensureArray(doc.object(), "data") : doc.array();
            }
            pDep->version = provider.mod->loadDependencyVersions(dep, arr);
            if (!pDep->version.addonId.isValid()) {
                if (m_loaderType & ModPlatform::Quilt) {  // falback for quilt
                    auto overide = ModPlatform::getOverrideDeps();
                    auto over = std::find_if(overide.cbegin(), overide.cend(),
                                             [dep, provider](auto o) { return o.provider == provider.name && dep.addonId == o.quilt; });
                    if (over != overide.cend()) {
                        removePack(dep.addonId);
                        addTask(prepareDependencyTask({ over->fabric, dep.type }, provider.name, level));
                        return;
                    }
                }
                removePack(dep.addonId);
                qWarning() << "Error while reading mod version empty ";
                qDebug() << doc;
                return;
            }
            pDep->version.is_currently_selected = true;
            pDep->pack->versions = { pDep->version };
            pDep->pack->versionsLoaded = true;

        } catch (const JSONValidationError& e) {
            removePack(dep.addonId);
            qDebug() << doc;
            qWarning() << "Error while reading mod version: " << e.cause();
            return;
        }
        if (level == 0) {
            removePack(dep.addonId);
            qWarning() << "Dependency cycle exceeded";
            return;
        }
        if (dep.addonId.toString().isEmpty() && !pDep->version.addonId.toString().isEmpty()) {
            pDep->pack->addonId = pDep->version.addonId;
            auto dep_ = getOverride({ pDep->version.addonId, pDep->dependency.type }, provider.name);
            if (dep_.addonId != pDep->version.addonId) {
                removePack(pDep->version.addonId);
                addTask(prepareDependencyTask(dep_, provider.name, level));
            } else {
                addTask(getProjectInfoTask(pDep));
            }
        }
        if (isLocalyInstalled(pDep)) {
            removePack(pDep->version.addonId);
            return;
        }
        for (auto dep_ : getDependenciesForVersion(pDep->version, provider.name)) {
            addTask(prepareDependencyTask(dep_, provider.name, level - 1));
        }
    };

    auto version = provider.api->getDependencyVersion(std::move(args), std::move(callbacks));
    QObject::connect(version.get(), &NetJob::failed, [this, version, pDep] {
        removePack(pDep->pack->addonId);
        m_failed.remove(version.get());
    });
    tasks->addTask(version);
    return tasks;
}

void GetModDependenciesTask::removePack(const QVariant& addonId)
{
    auto pred = [addonId](const std::shared_ptr<PackDependency>& v) { return v->pack->addonId == addonId; };
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
    m_pack_dependencies.removeIf(pred);
#else
    for (auto it = m_pack_dependencies.begin(); it != m_pack_dependencies.end();)
        if (pred(*it))
            it = m_pack_dependencies.erase(it);
        else
            ++it;
#endif
}

auto GetModDependenciesTask::getExtraInfo() -> QHash<QString, PackDependencyExtraInfo>
{
    QHash<QString, PackDependencyExtraInfo> rby;
    auto fullList = m_selected + m_pack_dependencies;
    for (auto& mod : fullList) {
        auto addonId = mod->pack->addonId;
        auto provider = mod->pack->provider;
        auto version = mod->version.fileId;
        auto req = QStringList();
        for (auto& smod : fullList) {
            if (provider != smod->pack->provider)
                continue;
            auto deps = smod->version.dependencies;
            if (auto dep = std::find_if(deps.begin(), deps.end(),
                                        [addonId, provider, version](const ModPlatform::Dependency& d) {
                                            return d.type == ModPlatform::DependencyType::REQUIRED &&
                                                   (provider == ModPlatform::ResourceProvider::MODRINTH && d.addonId.toString().isEmpty()
                                                        ? version == d.version
                                                        : d.addonId == addonId);
                                        });
                dep != deps.end()) {
                req.append(smod->pack->name);
            }
        }
        rby[addonId.toString()] = { maybeInstalled(mod), req };
    }
    return rby;
}

// super lax compare (but not fuzzy)
// convert to lowercase
// convert all speratores to whitespace
// simplify sequence of internal whitespace to a single space
// efectivly compare two strings ignoring all separators and case
auto laxCompare = [](QString fsfilename, QString metadataFilename, bool excludeDigits = false) {
    // allowed character seperators
    QList<QChar> allowedSeperators = { '-', '+', '.', '_' };
    if (excludeDigits)
        allowedSeperators.append({ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' });

    // copy in lowercase
    auto fsName = fsfilename.toLower();
    auto metaName = metadataFilename.toLower();

    // replace all potential allowed seperatores with whitespace
    for (auto sep : allowedSeperators) {
        fsName = fsName.replace(sep, ' ');
        metaName = metaName.replace(sep, ' ');
    }

    // remove extraneous whitespace
    fsName = fsName.simplified();
    metaName = metaName.simplified();

    return fsName.compare(metaName) == 0;
};

bool GetModDependenciesTask::isLocalyInstalled(std::shared_ptr<PackDependency> pDep)
{
    return pDep->version.fileName.isEmpty() ||

           std::find_if(m_selected.begin(), m_selected.end(),
                        [pDep](std::shared_ptr<PackDependency> i) {
                            return !i->version.fileName.isEmpty() && laxCompare(i->version.fileName, pDep->version.fileName);
                        }) != m_selected.end() ||  // check the selected versions

           std::find_if(m_mods_file_names.begin(), m_mods_file_names.end(),
                        [pDep](QString i) { return !i.isEmpty() && laxCompare(i, pDep->version.fileName); }) !=
               m_mods_file_names.end() ||  // check the existing mods

           std::find_if(m_pack_dependencies.begin(), m_pack_dependencies.end(), [pDep](std::shared_ptr<PackDependency> i) {
               return pDep->pack->addonId != i->pack->addonId && !i->version.fileName.isEmpty() &&
                      laxCompare(pDep->version.fileName, i->version.fileName);
           }) != m_pack_dependencies.end();  // check loaded dependencies
}

bool GetModDependenciesTask::maybeInstalled(std::shared_ptr<PackDependency> pDep)
{
    return std::find_if(m_mods_file_names.begin(), m_mods_file_names.end(), [pDep](QString i) {
               return !i.isEmpty() && laxCompare(i, pDep->version.fileName, true);
           }) != m_mods_file_names.end();  // check the existing mods
}
