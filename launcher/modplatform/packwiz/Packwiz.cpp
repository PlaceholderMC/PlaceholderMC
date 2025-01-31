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

#include "Packwiz.h"

#include <QDebug>
#include <QDir>
#include <QObject>
#include <sstream>
#include <string>

#include "FileSystem.h"
#include "StringUtils.h"

#include "minecraft/mod/Mod.h"
#include "modplatform/ModIndex.h"

#include <toml++/toml.h>

namespace Packwiz {

auto getRealIndexName(const QDir& index_dir, QString normalized_fname, bool should_find_match) -> QString
{
    QFile index_file(index_dir.absoluteFilePath(normalized_fname));

    QString real_fname = normalized_fname;
    if (!index_file.exists()) {
        // Tries to get similar entries
        for (auto& file_name : index_dir.entryList(QDir::Filter::Files)) {
            if (!QString::compare(normalized_fname, file_name, Qt::CaseInsensitive)) {
                real_fname = file_name;
                break;
            }
        }

        if (should_find_match && !QString::compare(normalized_fname, real_fname, Qt::CaseSensitive)) {
            qCritical() << "Could not find a match for a valid metadata file!";
            qCritical() << "File: " << normalized_fname;
            return {};
        }
    }

    return real_fname;
}

// Helpers
static inline auto indexFileName(QString const& mod_slug) -> QString
{
    if (mod_slug.endsWith(".pw.toml"))
        return mod_slug;
    return QString("%1.pw.toml").arg(mod_slug);
}

// Helper functions for extracting data from the TOML file
auto stringEntry(toml::table table, QString entry_name) -> QString
{
    auto node = table[StringUtils::toStdString(entry_name)];
    if (!node) {
        qWarning() << "Failed to read str property '" + entry_name + "' in mod metadata.";
        return {};
    }

    return node.value_or("");
}

auto intEntry(toml::table table, QString entry_name) -> int
{
    auto node = table[StringUtils::toStdString(entry_name)];
    if (!node) {
        qWarning() << "Failed to read int property '" + entry_name + "' in mod metadata.";
        return {};
    }

    return node.value_or(0);
}

auto V1::createModFormat([[maybe_unused]] const QDir& index_dir,
                         ModPlatform::IndexedPack& mod_pack,
                         ModPlatform::IndexedVersion& mod_version) -> Mod
{
    Mod mod;

    mod.slug = mod_pack.slug;
    mod.name = mod_pack.name;
    mod.filename = mod_version.fileName;

    if (mod_pack.provider == ModPlatform::ResourceProvider::FLAME) {
        mod.mode = "metadata:curseforge";
    } else {
        mod.mode = "url";
        mod.url = mod_version.downloadUrl;
    }

    mod.hash_format = mod_version.hash_type;
    mod.hash = mod_version.hash;

    mod.provider = mod_pack.provider;
    mod.file_id = mod_version.fileId;
    mod.project_id = mod_pack.addonId;
    mod.side = stringToSide(mod_version.side.isEmpty() ? mod_pack.side : mod_version.side);
    mod.loaders = mod_version.loaders;
    mod.mcVersions = mod_version.mcVersion;
    mod.mcVersions.sort();
    mod.releaseType = mod_version.version_type;

    mod.version_number = mod_version.version_number;
    if (mod.version_number.isNull())  // on CurseForge, there is only a version name - not a version number
        mod.version_number = mod_version.version;

    return mod;
}

auto V1::createModFormat(const QDir& index_dir, [[maybe_unused]] ::Mod& internal_mod, QString slug) -> Mod
{
    // Try getting metadata if it exists
    Mod mod{ getIndexForMod(index_dir, slug) };
    if (mod.isValid())
        return mod;

    qWarning() << QString("Tried to create mod metadata with a Mod without metadata!");

    return {};
}

void V1::updateModIndex(const QDir& index_dir, Mod& mod)
{
    if (!mod.isValid()) {
        qCritical() << QString("Tried to update metadata of an invalid mod!");
        return;
    }

    // Ensure the corresponding mod's info exists, and create it if not

    auto normalized_fname = indexFileName(mod.slug);
    auto real_fname = getRealIndexName(index_dir, normalized_fname);

    QFile index_file(index_dir.absoluteFilePath(real_fname));

    if (real_fname != normalized_fname)
        index_file.rename(normalized_fname);

    // There's already data on there!
    // TODO: We should do more stuff here, as the user is likely trying to
    // override a file. In this case, check versions and ask the user what
    // they want to do!
    if (index_file.exists()) {
        index_file.remove();
    } else {
        FS::ensureFilePathExists(index_file.fileName());
    }

    toml::table update;
    switch (mod.provider) {
        case (ModPlatform::ResourceProvider::FLAME):
            if (mod.file_id.toInt() == 0 || mod.project_id.toInt() == 0) {
                qCritical() << QString("Did not write file %1 because missing information!").arg(normalized_fname);
                return;
            }
            update = toml::table{
                { "file-id", mod.file_id.toInt() },
                { "project-id", mod.project_id.toInt() },
            };
            break;
        case (ModPlatform::ResourceProvider::MODRINTH):
            if (mod.mod_id().toString().isEmpty() || mod.version().toString().isEmpty()) {
                qCritical() << QString("Did not write file %1 because missing information!").arg(normalized_fname);
                return;
            }
            update = toml::table{
                { "mod-id", mod.mod_id().toString().toStdString() },
                { "version", mod.version().toString().toStdString() },
            };
            break;
    }

    toml::array loaders;
    for (auto loader : ModPlatform::modLoaderTypesToList(mod.loaders)) {
        loaders.push_back(getModLoaderAsString(loader).toStdString());
    }
    toml::array mcVersions;
    for (auto version : mod.mcVersions) {
        mcVersions.push_back(version.toStdString());
    }

    toml::array categories;
    for (auto version : mod.categories) {
        categories.push_back(version.toStdString());
    }

    if (!index_file.open(QIODevice::ReadWrite)) {
        qCritical() << QString("Could not open file %1!").arg(normalized_fname);
        return;
    }

    // Put TOML data into the file
    QTextStream in_stream(&index_file);
    {
        auto tbl = toml::table{ { "name", mod.name.toStdString() },
                                { "filename", mod.filename.toStdString() },
                                { "side", sideToString(mod.side).toStdString() },
                                { "x-prismlauncher-loaders", loaders },
                                { "x-prismlauncher-mc-versions", mcVersions },
                                { "x-prismlauncher-release-type", mod.releaseType.toString().toStdString() },
                                { "x-prismlauncher-version-number", mod.version_number.toStdString() },
                                { "x-prismlauncher-lock-update", mod.lockUpdate },
                                { "x-prismlauncher-categories", categories },
                                { "download",
                                  toml::table{
                                      { "mode", mod.mode.toStdString() },
                                      { "url", mod.url.toString().toStdString() },
                                      { "hash-format", mod.hash_format.toStdString() },
                                      { "hash", mod.hash.toStdString() },
                                  } },
                                { "update", toml::table{ { ModPlatform::ProviderCapabilities::name(mod.provider), update } } } };
        std::stringstream ss;
        ss << tbl;
        in_stream << QString::fromStdString(ss.str());
    }

    index_file.flush();
    index_file.close();
}

void V1::deleteModIndex(const QDir& index_dir, QString& mod_slug)
{
    auto normalized_fname = indexFileName(mod_slug);
    auto real_fname = getRealIndexName(index_dir, normalized_fname);
    if (real_fname.isEmpty())
        return;

    QFile index_file(index_dir.absoluteFilePath(real_fname));

    if (!index_file.exists()) {
        qWarning() << QString("Tried to delete non-existent mod metadata for %1!").arg(mod_slug);
        return;
    }

    if (!index_file.remove()) {
        qWarning() << QString("Failed to remove metadata for mod %1!").arg(mod_slug);
    }
}

void V1::deleteModIndex(const QDir& index_dir, QVariant& mod_id)
{
    for (auto& file_name : index_dir.entryList(QDir::Filter::Files)) {
        auto mod = getIndexForMod(index_dir, file_name);

        if (mod.mod_id() == mod_id) {
            deleteModIndex(index_dir, mod.name);
            break;
        }
    }
}

auto V1::getIndexForMod(const QDir& index_dir, QString slug) -> Mod
{
    Mod mod;

    auto normalized_fname = indexFileName(slug);
    auto real_fname = getRealIndexName(index_dir, normalized_fname, true);
    if (real_fname.isEmpty())
        return {};

    toml::table table;
#if TOML_EXCEPTIONS
    try {
        table = toml::parse_file(StringUtils::toStdString(index_dir.absoluteFilePath(real_fname)));
    } catch (const toml::parse_error& err) {
        qWarning() << QString("Could not open file %1!").arg(normalized_fname);
        qWarning() << "Reason: " << QString(err.what());
        return {};
    }
#else
    toml::parse_result result = toml::parse_file(StringUtils::toStdString(index_dir.absoluteFilePath(real_fname)));
    if (!result) {
        qWarning() << QString("Could not open file %1!").arg(normalized_fname);
        qWarning() << "Reason: " << result.error().description();
        return {};
    }
    table = result.table();
#endif

    // index_file.close();

    mod.slug = slug;

    {  // Basic info
        mod.name = stringEntry(table, "name");
        mod.filename = stringEntry(table, "filename");
        mod.side = stringToSide(stringEntry(table, "side"));
        mod.releaseType = ModPlatform::IndexedVersionType(table["x-prismlauncher-release-type"].value_or(""));
        mod.lockUpdate = table["x-prismlauncher-lock-update"].value_or(false);
        if (auto loaders = table["x-prismlauncher-loaders"]; loaders && loaders.is_array()) {
            for (auto&& loader : *loaders.as_array()) {
                if (loader.is_string()) {
                    mod.loaders |= ModPlatform::getModLoaderFromString(QString::fromStdString(loader.as_string()->value_or("")));
                }
            }
        }
        if (auto versions = table["x-prismlauncher-mc-versions"]; versions && versions.is_array()) {
            for (auto&& version : *versions.as_array()) {
                if (version.is_string()) {
                    auto ver = QString::fromStdString(version.as_string()->value_or(""));
                    if (!ver.isEmpty()) {
                        mod.mcVersions << ver;
                    }
                }
            }
            mod.mcVersions.sort();
        }
        if (auto categories = table["x-prismlauncher-categories"]; categories && categories.is_array()) {
            for (auto&& category : *categories.as_array()) {
                if (category.is_string()) {
                    auto ver = QString::fromStdString(category.as_string()->value_or(""));
                    if (!ver.isEmpty()) {
                        mod.categories << ver;
                    }
                }
            }
            mod.categories.sort();
        }
    }
    mod.version_number = table["x-prismlauncher-version-number"].value_or("");

    {  // [download] info
        auto download_table = table["download"].as_table();
        if (!download_table) {
            qCritical() << QString("No [download] section found on mod metadata!");
            return {};
        }

        mod.mode = stringEntry(*download_table, "mode");
        mod.url = stringEntry(*download_table, "url");
        mod.hash_format = stringEntry(*download_table, "hash-format");
        mod.hash = stringEntry(*download_table, "hash");
    }

    {  // [update] info
        using Provider = ModPlatform::ResourceProvider;

        auto update_table = table["update"];
        if (!update_table || !update_table.is_table()) {
            qCritical() << QString("No [update] section found on mod metadata!");
            return {};
        }

        toml::table* mod_provider_table = nullptr;
        if ((mod_provider_table = update_table[ModPlatform::ProviderCapabilities::name(Provider::FLAME)].as_table())) {
            mod.provider = Provider::FLAME;
            mod.file_id = intEntry(*mod_provider_table, "file-id");
            mod.project_id = intEntry(*mod_provider_table, "project-id");
        } else if ((mod_provider_table = update_table[ModPlatform::ProviderCapabilities::name(Provider::MODRINTH)].as_table())) {
            mod.provider = Provider::MODRINTH;
            mod.mod_id() = stringEntry(*mod_provider_table, "mod-id");
            mod.version() = stringEntry(*mod_provider_table, "version");
        } else {
            qCritical() << QString("No mod provider on mod metadata!");
            return {};
        }
    }

    return mod;
}

auto V1::getIndexForMod(const QDir& index_dir, QVariant& mod_id) -> Mod
{
    for (auto& file_name : index_dir.entryList(QDir::Filter::Files)) {
        auto mod = getIndexForMod(index_dir, file_name);

        if (mod.mod_id() == mod_id)
            return mod;
    }

    return {};
}

auto V1::sideToString(Side side) -> QString
{
    switch (side) {
        case Side::ClientSide:
            return "client";
        case Side::ServerSide:
            return "server";
        case Side::UniversalSide:
            return "both";
    }
    return {};
}

auto V1::stringToSide(QString side) -> Side
{
    if (side == "client")
        return Side::ClientSide;
    if (side == "server")
        return Side::ServerSide;
    if (side == "both")
        return Side::UniversalSide;
    return Side::UniversalSide;
}

}  // namespace Packwiz
