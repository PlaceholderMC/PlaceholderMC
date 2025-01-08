// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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

#include "modplatform/ModIndex.h"

#include <QString>
#include <QUrl>
#include <QVariant>

class QDir;

// Mod from launcher/minecraft/mod/Mod.h
class Mod;

namespace Packwiz {

auto getRealIndexName(const QDir& index_dir, QString normalized_index_name, bool should_match = false) -> QString;

class V1 {
   public:
    enum class Side { ClientSide = 1 << 0, ServerSide = 1 << 1, UniversalSide = ClientSide | ServerSide };

    // can also represent other resources beside loader mods - but this is what packwiz calls it
    struct Mod {
        QString slug{};
        QString name{};
        QString filename{};
        Side side{ Side::UniversalSide };
        ModPlatform::ModLoaderTypes loaders;
        QStringList mcVersions;
        ModPlatform::IndexedVersionType releaseType;

        // [download]
        QString mode{};
        QUrl url{};
        QString hash_format{};
        QString hash{};

        // [update]
        ModPlatform::ResourceProvider provider{};
        QVariant file_id{};
        QVariant project_id{};
        QString version_number{};

        bool lockUpdate;

       public:
        // This is a totally heuristic, but should work for now.
        auto isValid() const -> bool { return !slug.isEmpty() && !project_id.isNull(); }

        // Different providers can use different names for the same thing
        // Modrinth-specific
        auto mod_id() -> QVariant& { return project_id; }
        auto version() -> QVariant& { return file_id; }
    };

    /* Generates the object representing the information in a mod.pw.toml file via
     * its common representation in the launcher, when downloading mods.
     * */
    static auto createModFormat(const QDir& index_dir, ModPlatform::IndexedPack& mod_pack, ModPlatform::IndexedVersion& mod_version) -> Mod;
    /* Generates the object representing the information in a mod.pw.toml file via
     * its common representation in the launcher, plus a necessary slug.
     * */
    static auto createModFormat(const QDir& index_dir, ::Mod& internal_mod, QString slug) -> Mod;

    /* Updates the mod index for the provided mod.
     * This creates a new index if one does not exist already
     * TODO: Ask the user if they want to override, and delete the old mod's files, or keep the old one.
     * */
    static void updateModIndex(const QDir& index_dir, Mod& mod);

    /* Deletes the metadata for the mod with the given slug. If the metadata doesn't exist, it does nothing. */
    static void deleteModIndex(const QDir& index_dir, QString& mod_slug);

    /* Deletes the metadata for the mod with the given id. If the metadata doesn't exist, it does nothing. */
    static void deleteModIndex(const QDir& index_dir, QVariant& mod_id);

    /* Gets the metadata for a mod with a particular file name.
     * If the mod doesn't have a metadata, it simply returns an empty Mod object.
     * */
    static auto getIndexForMod(const QDir& index_dir, QString slug) -> Mod;

    /* Gets the metadata for a mod with a particular id.
     * If the mod doesn't have a metadata, it simply returns an empty Mod object.
     * */
    static auto getIndexForMod(const QDir& index_dir, QVariant& mod_id) -> Mod;

    static auto sideToString(Side side) -> QString;
    static auto stringToSide(QString side) -> Side;
};

}  // namespace Packwiz
