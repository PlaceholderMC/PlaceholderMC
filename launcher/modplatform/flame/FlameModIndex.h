//
// Created by timoreo on 16/01/2022.
//

#pragma once

#include "modplatform/ModIndex.h"

#include "BaseInstance.h"

namespace FlameMod {

void loadIndexedPack(ModPlatform::IndexedPack& m, QJsonObject& obj);
void loadURLs(ModPlatform::IndexedPack& m, QJsonObject& obj);
void loadBody(ModPlatform::IndexedPack& m, QJsonObject& obj);
void loadIndexedPackVersions(ModPlatform::IndexedPack& pack, QJsonArray& arr);
ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj, bool load_changelog = false);
ModPlatform::IndexedVersion loadDependencyVersions(const ModPlatform::Dependency& m, QJsonArray& arr, const BaseInstance* inst);
}  // namespace FlameMod