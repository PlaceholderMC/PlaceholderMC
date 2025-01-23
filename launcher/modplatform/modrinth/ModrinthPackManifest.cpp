// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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
 *      Copyright 2022 kb1000
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

#include "ModrinthPackManifest.h"
#include <QFileInfo>
#include "Json.h"

#include "modplatform/modrinth/ModrinthAPI.h"

#include <QSet>

static ModrinthAPI api;

namespace Modrinth {

void loadIndexedPack(Modpack& pack, QJsonObject& obj)
{
    pack.id = Json::ensureString(obj, "project_id");

    pack.name = Json::ensureString(obj, "title");
    pack.description = Json::ensureString(obj, "description");
    auto temp_author_name = Json::ensureString(obj, "author");
    pack.author = std::make_tuple(temp_author_name, api.getAuthorURL(temp_author_name));
    pack.iconUrl = Json::ensureString(obj, "icon_url");
    pack.iconName = QString("modrinth_%1.%2").arg(Json::ensureString(obj, "slug"), QFileInfo(pack.iconUrl.fileName()).suffix());
}

void loadIndexedInfo(Modpack& pack, QJsonObject& obj)
{
    pack.extra.body = Json::ensureString(obj, "body");
    pack.extra.projectUrl = QString("https://modrinth.com/modpack/%1").arg(Json::ensureString(obj, "slug"));

    pack.extra.issuesUrl = Json::ensureString(obj, "issues_url");
    if (pack.extra.issuesUrl.endsWith('/'))
        pack.extra.issuesUrl.chop(1);

    pack.extra.sourceUrl = Json::ensureString(obj, "source_url");
    if (pack.extra.sourceUrl.endsWith('/'))
        pack.extra.sourceUrl.chop(1);

    pack.extra.wikiUrl = Json::ensureString(obj, "wiki_url");
    if (pack.extra.wikiUrl.endsWith('/'))
        pack.extra.wikiUrl.chop(1);

    pack.extra.discordUrl = Json::ensureString(obj, "discord_url");
    if (pack.extra.discordUrl.endsWith('/'))
        pack.extra.discordUrl.chop(1);

    auto donate_arr = Json::ensureArray(obj, "donation_urls");
    for (auto d : donate_arr) {
        auto d_obj = Json::requireObject(d);

        DonationData donate;

        donate.id = Json::ensureString(d_obj, "id");
        donate.platform = Json::ensureString(d_obj, "platform");
        donate.url = Json::ensureString(d_obj, "url");

        pack.extra.donate.append(donate);
    }

    pack.extra.status = Json::ensureString(obj, "status");

    pack.extraInfoLoaded = true;
}

void loadIndexedVersions(Modpack& pack, QJsonDocument& doc)
{
    QVector<ModpackVersion> unsortedVersions;

    auto arr = Json::requireArray(doc);

    for (auto versionIter : arr) {
        auto obj = Json::requireObject(versionIter);
        auto file = loadIndexedVersion(obj);

        if (!file.id.isEmpty())  // Heuristic to check if the returned value is valid
            unsortedVersions.append(file);
    }
    auto orderSortPredicate = [](const ModpackVersion& a, const ModpackVersion& b) -> bool {
        // dates are in RFC 3339 format
        return a.date > b.date;
    };

    std::sort(unsortedVersions.begin(), unsortedVersions.end(), orderSortPredicate);

    pack.versions.swap(unsortedVersions);

    pack.versionsLoaded = true;
}

auto loadIndexedVersion(QJsonObject& obj) -> ModpackVersion
{
    ModpackVersion file;

    file.name = Json::requireString(obj, "name");
    file.version = Json::requireString(obj, "version_number");
    auto gameVersions = Json::ensureArray(obj, "game_versions");
    if (!gameVersions.isEmpty()) {
        file.gameVersion = Json::ensureString(gameVersions[0]);
        file.gameVersion = ModrinthAPI::mapMCVersionFromModrinth(file.gameVersion);
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
    file.version_type = ModPlatform::IndexedVersionType(Json::requireString(obj, "version_type"));
    file.changelog = Json::ensureString(obj, "changelog");

    file.id = Json::requireString(obj, "id");
    file.project_id = Json::requireString(obj, "project_id");

    file.date = Json::requireString(obj, "date_published");

    auto files = Json::requireArray(obj, "files");

    for (auto file_iter : files) {
        File indexed_file;
        auto parent = Json::requireObject(file_iter);
        auto is_primary = Json::ensureBoolean(parent, (const QString)QStringLiteral("primary"), false);
        if (!is_primary) {
            auto filename = Json::ensureString(parent, "filename");
            // Checking suffix here is fine because it's the response from Modrinth,
            // so one would assume it will always be in English.
            if (!filename.endsWith("mrpack") && !filename.endsWith("zip"))
                continue;
        }

        auto url = Json::requireString(parent, "url");

        file.download_url = url;
        if (is_primary)
            break;
    }

    if (file.download_url.isEmpty())
        return {};

    return file;
}

}  // namespace Modrinth
