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

#pragma once

#include <QDir>
#include <QList>
#include <QVariant>
#include <functional>
#include <memory>

#include "minecraft/mod/MetadataHandler.h"
#include "minecraft/mod/ModFolderModel.h"
#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"
#include "tasks/SequentialTask.h"
#include "tasks/Task.h"
#include "ui/pages/modplatform/ModModel.h"

class GetModDependenciesTask : public SequentialTask {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<GetModDependenciesTask>;

    struct PackDependency {
        ModPlatform::Dependency dependency;
        ModPlatform::IndexedPack::Ptr pack;
        ModPlatform::IndexedVersion version;
        PackDependency() = default;
        PackDependency(const ModPlatform::IndexedPack::Ptr p, const ModPlatform::IndexedVersion& v)
        {
            pack = p;
            version = v;
        }
    };

    struct PackDependencyExtraInfo {
        bool maybe_installed;
        QStringList required_by;
    };

    struct Provider {
        ModPlatform::ResourceProvider name;
        std::shared_ptr<ResourceDownload::ModModel> mod;
        std::shared_ptr<ResourceAPI> api;
    };

    explicit GetModDependenciesTask(BaseInstance* instance, ModFolderModel* folder, QList<std::shared_ptr<PackDependency>> selected);

    auto getDependecies() const -> QList<std::shared_ptr<PackDependency>> { return m_pack_dependencies; }
    QHash<QString, PackDependencyExtraInfo> getExtraInfo();

   protected slots:
    Task::Ptr prepareDependencyTask(const ModPlatform::Dependency&, ModPlatform::ResourceProvider, int);
    QList<ModPlatform::Dependency> getDependenciesForVersion(const ModPlatform::IndexedVersion&,
                                                             ModPlatform::ResourceProvider providerName);
    void prepare();
    Task::Ptr getProjectInfoTask(std::shared_ptr<PackDependency> pDep);
    ModPlatform::Dependency getOverride(const ModPlatform::Dependency&, ModPlatform::ResourceProvider providerName);
    void removePack(const QVariant& addonId);

    bool isLocalyInstalled(std::shared_ptr<PackDependency> pDep);
    bool maybeInstalled(std::shared_ptr<PackDependency> pDep);

   private:
    QList<std::shared_ptr<PackDependency>> m_pack_dependencies;
    QList<std::shared_ptr<Metadata::ModStruct>> m_mods;
    QList<std::shared_ptr<PackDependency>> m_selected;
    QStringList m_mods_file_names;
    Provider m_flame_provider;
    Provider m_modrinth_provider;

    Version m_version;
    ModPlatform::ModLoaderTypes m_loaderType;
};
