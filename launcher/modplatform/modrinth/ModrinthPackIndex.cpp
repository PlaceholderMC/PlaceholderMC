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

#include "ModrinthPackIndex.h"
#include "FileSystem.h"
#include "ModrinthAPI.h"

#include "Json.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "modplatform/ModIndex.h"

static ModrinthAPI api;

bool shouldDownloadOnSide(QString side)
{
    return side == "required" || side == "optional";
}

// https://docs.modrinth.com/api-spec/#tag/projects/operation/getProject
void Modrinth::loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj)
{
    pack.addonId = Json::ensureString(obj, "project_id");
    if (pack.addonId.toString().isEmpty())
        pack.addonId = Json::requireString(obj, "id");

    pack.provider = ModPlatform::ResourceProvider::MODRINTH;
    pack.name = Json::requireString(obj, "title");

    pack.slug = Json::ensureString(obj, "slug", "");
    if (!pack.slug.isEmpty())
        pack.websiteUrl = "https://modrinth.com/mod/" + pack.slug;
    else
        pack.websiteUrl = "";

    pack.description = Json::ensureString(obj, "description", "");

    pack.logoUrl = Json::ensureString(obj, "icon_url", "");
    pack.logoName = pack.addonId.toString();

    ModPlatform::ModpackAuthor modAuthor;
    modAuthor.name = Json::ensureString(obj, "author", QObject::tr("No author(s)"));
    modAuthor.url = api.getAuthorURL(modAuthor.name);
    pack.authors.append(modAuthor);

    auto client = shouldDownloadOnSide(Json::ensureString(obj, "client_side"));
    auto server = shouldDownloadOnSide(Json::ensureString(obj, "server_side"));

    if (server && client) {
        pack.side = "both";
    } else if (server) {
        pack.side = "server";
    } else if (client) {
        pack.side = "client";
    }

    // Modrinth can have more data than what's provided by the basic search :)
    pack.extraDataLoaded = false;
}

void Modrinth::loadExtraPackData(ModPlatform::IndexedPack& pack, QJsonObject& obj)
{
    pack.extraData.issuesUrl = Json::ensureString(obj, "issues_url");
    if (pack.extraData.issuesUrl.endsWith('/'))
        pack.extraData.issuesUrl.chop(1);

    pack.extraData.sourceUrl = Json::ensureString(obj, "source_url");
    if (pack.extraData.sourceUrl.endsWith('/'))
        pack.extraData.sourceUrl.chop(1);

    pack.extraData.wikiUrl = Json::ensureString(obj, "wiki_url");
    if (pack.extraData.wikiUrl.endsWith('/'))
        pack.extraData.wikiUrl.chop(1);

    pack.extraData.discordUrl = Json::ensureString(obj, "discord_url");
    if (pack.extraData.discordUrl.endsWith('/'))
        pack.extraData.discordUrl.chop(1);

    auto donate_arr = Json::ensureArray(obj, "donation_urls");
    for (auto d : donate_arr) {
        auto d_obj = Json::requireObject(d);

        ModPlatform::DonationData donate;

        donate.id = Json::ensureString(d_obj, "id");
        donate.platform = Json::ensureString(d_obj, "platform");
        donate.url = Json::ensureString(d_obj, "url");

        pack.extraData.donate.append(donate);
    }

    pack.extraData.status = Json::ensureString(obj, "status");

    pack.extraData.body = Json::ensureString(obj, "body").remove("<br>");

    pack.extraDataLoaded = true;
}

void Modrinth::loadIndexedPackVersions(ModPlatform::IndexedPack& pack, QJsonArray& arr)
{
    QVector<ModPlatform::IndexedVersion> unsortedVersions;
    for (auto versionIter : arr) {
        auto obj = versionIter.toObject();
        auto file = loadIndexedPackVersion(obj);

        if (file.fileId.isValid())  // Heuristic to check if the returned value is valid
            unsortedVersions.append(file);
    }
    auto orderSortPredicate = [](const ModPlatform::IndexedVersion& a, const ModPlatform::IndexedVersion& b) -> bool {
        // dates are in RFC 3339 format
        return a.date > b.date;
    };
    std::sort(unsortedVersions.begin(), unsortedVersions.end(), orderSortPredicate);
    pack.versions = unsortedVersions;
    pack.versionsLoaded = true;
}

auto Modrinth::loadIndexedPackVersion(QJsonObject& obj,
                                      QString preferred_hash_type,
                                      QString preferred_file_name) -> ModPlatform::IndexedVersion
{
    ModPlatform::IndexedVersion file;

    file.addonId = Json::requireString(obj, "project_id");
    file.fileId = Json::requireString(obj, "id");
    file.date = Json::requireString(obj, "date_published");
    auto versionArray = Json::requireArray(obj, "game_versions");
    if (versionArray.empty()) {
        return {};
    }
    for (auto mcVer : versionArray) {
        file.mcVersion.append(mcVer.toString());
    }
    auto loaders = Json::requireArray(obj, "loaders");
    for (auto loader : loaders) {
        if (loader == "neoforge")
            file.loaders |= ModPlatform::NeoForge;
        else if (loader == "forge")
            file.loaders |= ModPlatform::Forge;
        else if (loader == "cauldron")
            file.loaders |= ModPlatform::Cauldron;
        else if (loader == "liteloader")
            file.loaders |= ModPlatform::LiteLoader;
        else if (loader == "fabric")
            file.loaders |= ModPlatform::Fabric;
        else if (loader == "quilt")
            file.loaders |= ModPlatform::Quilt;
    }
    file.version = Json::requireString(obj, "name");
    file.version_number = Json::requireString(obj, "version_number");
    file.version_type = ModPlatform::IndexedVersionType(Json::requireString(obj, "version_type"));

    file.changelog = Json::requireString(obj, "changelog");

    auto dependencies = Json::ensureArray(obj, "dependencies");
    for (auto d : dependencies) {
        auto dep = Json::ensureObject(d);
        ModPlatform::Dependency dependency;
        dependency.addonId = Json::ensureString(dep, "project_id");
        dependency.version = Json::ensureString(dep, "version_id");
        auto depType = Json::requireString(dep, "dependency_type");

        if (depType == "required")
            dependency.type = ModPlatform::DependencyType::REQUIRED;
        else if (depType == "optional")
            dependency.type = ModPlatform::DependencyType::OPTIONAL;
        else if (depType == "incompatible")
            dependency.type = ModPlatform::DependencyType::INCOMPATIBLE;
        else if (depType == "embedded")
            dependency.type = ModPlatform::DependencyType::EMBEDDED;
        else
            dependency.type = ModPlatform::DependencyType::UNKNOWN;

        file.dependencies.append(dependency);
    }

    auto files = Json::requireArray(obj, "files");
    int i = 0;

    if (files.empty()) {
        // This should not happen normally, but check just in case
        qWarning() << "Modrinth returned an unexpected empty list of files:" << obj;
        return {};
    }

    // Find correct file (needed in cases where one version may have multiple files)
    // Will default to the last one if there's no primary (though I think Modrinth requires that
    // at least one file is primary, idk)
    // NOTE: files.count() is 1-indexed, so we need to subtract 1 to become 0-indexed
    while (i < files.count() - 1) {
        auto parent = files[i].toObject();
        auto fileName = Json::requireString(parent, "filename");

        if (!preferred_file_name.isEmpty() && fileName.contains(preferred_file_name)) {
            file.is_preferred = true;
            break;
        }

        // Grab the primary file, if available
        if (Json::requireBoolean(parent, "primary"))
            break;

        i++;
    }

    auto parent = files[i].toObject();
    if (parent.contains("url")) {
        file.downloadUrl = Json::requireString(parent, "url");
        file.fileName = Json::requireString(parent, "filename");
        file.fileName = FS::RemoveInvalidPathChars(file.fileName);
        file.is_preferred = Json::requireBoolean(parent, "primary") || (files.count() == 1);
        auto hash_list = Json::requireObject(parent, "hashes");

        if (hash_list.contains(preferred_hash_type)) {
            file.hash = Json::requireString(hash_list, preferred_hash_type);
            file.hash_type = preferred_hash_type;
        } else {
            auto hash_types = ModPlatform::ProviderCapabilities::hashType(ModPlatform::ResourceProvider::MODRINTH);
            for (auto& hash_type : hash_types) {
                if (hash_list.contains(hash_type)) {
                    file.hash = Json::requireString(hash_list, hash_type);
                    file.hash_type = hash_type;
                    break;
                }
            }
        }

        return file;
    }

    return {};
}

auto Modrinth::loadDependencyVersions([[maybe_unused]] const ModPlatform::Dependency& m,
                                      QJsonArray& arr,
                                      const BaseInstance* inst) -> ModPlatform::IndexedVersion
{
    auto profile = (dynamic_cast<const MinecraftInstance*>(inst))->getPackProfile();
    QString mcVersion = profile->getComponentVersion("net.minecraft");
    auto loaders = profile->getSupportedModLoaders();

    QVector<ModPlatform::IndexedVersion> versions;
    for (auto versionIter : arr) {
        auto obj = versionIter.toObject();
        auto file = loadIndexedPackVersion(obj);

        if (file.fileId.isValid() &&
            (!loaders.has_value() || !file.loaders || loaders.value() & file.loaders))  // Heuristic to check if the returned value is valid
            versions.append(file);
    }
    auto orderSortPredicate = [](const ModPlatform::IndexedVersion& a, const ModPlatform::IndexedVersion& b) -> bool {
        // dates are in RFC 3339 format
        return a.date > b.date;
    };
    std::sort(versions.begin(), versions.end(), orderSortPredicate);
    return versions.length() != 0 ? versions.front() : ModPlatform::IndexedVersion();
}
