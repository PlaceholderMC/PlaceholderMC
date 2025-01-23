// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#pragma once

#include <QDebug>
#include <QList>
#include <QString>

#include <list>
#include <optional>

#include "../Version.h"

#include "modplatform/ModIndex.h"
#include "tasks/Task.h"

/* Simple class with a common interface for interacting with APIs */
class ResourceAPI {
   public:
    virtual ~ResourceAPI() = default;

    struct SortingMethod {
        // The index of the sorting method. Used to allow for arbitrary ordering in the list of methods.
        // Used by Flame in the API request.
        unsigned int index;
        // The real name of the sorting, as used in the respective API specification.
        // Used by Modrinth in the API request.
        QString name;
        // The human-readable name of the sorting, used for display in the UI.
        QString readable_name;
    };

    struct SearchArgs {
        ModPlatform::ResourceType type{};
        int offset = 0;

        std::optional<QString> search;
        std::optional<SortingMethod> sorting;
        std::optional<ModPlatform::ModLoaderTypes> loaders;
        std::optional<std::list<Version>> versions;
        std::optional<QString> side;
        std::optional<QStringList> categoryIds;
        bool openSource;
    };
    struct SearchCallbacks {
        std::function<void(QJsonDocument&)> on_succeed;
        std::function<void(QString const& reason, int network_error_code)> on_fail;
        std::function<void()> on_abort;
    };

    struct VersionSearchArgs {
        ModPlatform::IndexedPack pack;

        std::optional<std::list<Version>> mcVersions;
        std::optional<ModPlatform::ModLoaderTypes> loaders;

        VersionSearchArgs(VersionSearchArgs const&) = default;
        void operator=(VersionSearchArgs other)
        {
            pack = other.pack;
            mcVersions = other.mcVersions;
            loaders = other.loaders;
        }
    };
    struct VersionSearchCallbacks {
        std::function<void(QJsonDocument&, ModPlatform::IndexedPack)> on_succeed;
        std::function<void(QString const& reason, int network_error_code)> on_fail;
    };

    struct ProjectInfoArgs {
        ModPlatform::IndexedPack pack;

        ProjectInfoArgs(ProjectInfoArgs const&) = default;
        void operator=(ProjectInfoArgs other) { pack = other.pack; }
    };
    struct ProjectInfoCallbacks {
        std::function<void(QJsonDocument&, const ModPlatform::IndexedPack&)> on_succeed;
        std::function<void(QString const& reason)> on_fail;
        std::function<void()> on_abort;
    };

    struct DependencySearchArgs {
        ModPlatform::Dependency dependency;
        Version mcVersion;
        ModPlatform::ModLoaderTypes loader;
    };

    struct DependencySearchCallbacks {
        std::function<void(QJsonDocument&, const ModPlatform::Dependency&)> on_succeed;
        std::function<void(QString const& reason, int network_error_code)> on_fail;
    };

   public:
    /** Gets a list of available sorting methods for this API. */
    [[nodiscard]] virtual auto getSortingMethods() const -> QList<SortingMethod> = 0;

   public slots:
    [[nodiscard]] virtual Task::Ptr searchProjects(SearchArgs&&, SearchCallbacks&&) const
    {
        qWarning() << "TODO: ResourceAPI::searchProjects";
        return nullptr;
    }
    [[nodiscard]] virtual Task::Ptr getProject([[maybe_unused]] QString addonId,
                                               [[maybe_unused]] std::shared_ptr<QByteArray> response) const
    {
        qWarning() << "TODO: ResourceAPI::getProject";
        return nullptr;
    }
    [[nodiscard]] virtual Task::Ptr getProjects([[maybe_unused]] QStringList addonIds,
                                                [[maybe_unused]] std::shared_ptr<QByteArray> response) const
    {
        qWarning() << "TODO: ResourceAPI::getProjects";
        return nullptr;
    }

    [[nodiscard]] virtual Task::Ptr getProjectInfo(ProjectInfoArgs&&, ProjectInfoCallbacks&&) const
    {
        qWarning() << "TODO: ResourceAPI::getProjectInfo";
        return nullptr;
    }
    [[nodiscard]] virtual Task::Ptr getProjectVersions(VersionSearchArgs&&, VersionSearchCallbacks&&) const
    {
        qWarning() << "TODO: ResourceAPI::getProjectVersions";
        return nullptr;
    }

    [[nodiscard]] virtual Task::Ptr getDependencyVersion(DependencySearchArgs&&, DependencySearchCallbacks&&) const
    {
        qWarning() << "TODO";
        return nullptr;
    }

   protected:
    [[nodiscard]] inline QString debugName() const { return "External resource API"; }

    [[nodiscard]] inline QString mapMCVersionToModrinth(Version v) const
    {
        static const QString preString = " Pre-Release ";
        auto verStr = v.toString();

        if (verStr.contains(preString)) {
            verStr.replace(preString, "-pre");
        }
        verStr.replace(" ", "-");
        return verStr;
    }

    [[nodiscard]] inline QString getGameVersionsString(std::list<Version> mcVersions) const
    {
        QString s;
        for (auto& ver : mcVersions) {
            s += QString("\"%1\",").arg(mapMCVersionToModrinth(ver));
        }
        s.remove(s.length() - 1, 1);  // remove last comma
        return s;
    }
};
