// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "ModrinthResourceModels.h"

#include "modplatform/modrinth/ModrinthAPI.h"
#include "modplatform/modrinth/ModrinthPackIndex.h"

namespace ResourceDownload {

ModrinthModModel::ModrinthModModel(BaseInstance& base) : ModModel(base, new ModrinthAPI) {}

void ModrinthModModel::loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadIndexedPack(m, obj);
}

void ModrinthModModel::loadExtraPackInfo(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadExtraPackData(m, obj);
}

void ModrinthModModel::loadIndexedPackVersions(ModPlatform::IndexedPack& m, QJsonArray& arr)
{
    ::Modrinth::loadIndexedPackVersions(m, arr);
}

auto ModrinthModModel::loadDependencyVersions(const ModPlatform::Dependency& m, QJsonArray& arr) -> ModPlatform::IndexedVersion
{
    return ::Modrinth::loadDependencyVersions(m, arr, &m_base_instance);
}

auto ModrinthModModel::documentToArray(QJsonDocument& obj) const -> QJsonArray
{
    return obj.object().value("hits").toArray();
}

ModrinthResourcePackModel::ModrinthResourcePackModel(const BaseInstance& base) : ResourcePackResourceModel(base, new ModrinthAPI) {}

void ModrinthResourcePackModel::loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadIndexedPack(m, obj);
}

void ModrinthResourcePackModel::loadExtraPackInfo(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadExtraPackData(m, obj);
}

void ModrinthResourcePackModel::loadIndexedPackVersions(ModPlatform::IndexedPack& m, QJsonArray& arr)
{
    ::Modrinth::loadIndexedPackVersions(m, arr);
}

auto ModrinthResourcePackModel::documentToArray(QJsonDocument& obj) const -> QJsonArray
{
    return obj.object().value("hits").toArray();
}

ModrinthTexturePackModel::ModrinthTexturePackModel(const BaseInstance& base) : TexturePackResourceModel(base, new ModrinthAPI) {}

void ModrinthTexturePackModel::loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadIndexedPack(m, obj);
}

void ModrinthTexturePackModel::loadExtraPackInfo(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadExtraPackData(m, obj);
}

void ModrinthTexturePackModel::loadIndexedPackVersions(ModPlatform::IndexedPack& m, QJsonArray& arr)
{
    ::Modrinth::loadIndexedPackVersions(m, arr);
}

auto ModrinthTexturePackModel::documentToArray(QJsonDocument& obj) const -> QJsonArray
{
    return obj.object().value("hits").toArray();
}

ModrinthShaderPackModel::ModrinthShaderPackModel(const BaseInstance& base) : ShaderPackResourceModel(base, new ModrinthAPI) {}

void ModrinthShaderPackModel::loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadIndexedPack(m, obj);
}

void ModrinthShaderPackModel::loadExtraPackInfo(ModPlatform::IndexedPack& m, QJsonObject& obj)
{
    ::Modrinth::loadExtraPackData(m, obj);
}

void ModrinthShaderPackModel::loadIndexedPackVersions(ModPlatform::IndexedPack& m, QJsonArray& arr)
{
    ::Modrinth::loadIndexedPackVersions(m, arr);
}

auto ModrinthShaderPackModel::documentToArray(QJsonDocument& obj) const -> QJsonArray
{
    return obj.object().value("hits").toArray();
}

}  // namespace ResourceDownload
