// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QList>
#include <memory>
#include "modplatform/ModIndex.h"
#include "modplatform/ResourceAPI.h"
#include "modplatform/helpers/NetworkResourceAPI.h"

class FlameAPI : public NetworkResourceAPI {
   public:
    QString getModFileChangelog(int modId, int fileId);
    QString getModDescription(int modId);

    std::optional<ModPlatform::IndexedVersion> getLatestVersion(QVector<ModPlatform::IndexedVersion> versions,
                                                                QList<ModPlatform::ModLoaderType> instanceLoaders,
                                                                ModPlatform::ModLoaderTypes fallback);

    Task::Ptr getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const override;
    Task::Ptr matchFingerprints(const QList<uint>& fingerprints, std::shared_ptr<QByteArray> response);
    Task::Ptr getFiles(const QStringList& fileIds, std::shared_ptr<QByteArray> response) const;
    Task::Ptr getFile(const QString& addonId, const QString& fileId, std::shared_ptr<QByteArray> response) const;

    static Task::Ptr getCategories(std::shared_ptr<QByteArray> response, ModPlatform::ResourceType type);
    static Task::Ptr getModCategories(std::shared_ptr<QByteArray> response);
    static QList<ModPlatform::Category> loadModCategories(std::shared_ptr<QByteArray> response);

    [[nodiscard]] QList<ResourceAPI::SortingMethod> getSortingMethods() const override;

    static inline bool validateModLoaders(ModPlatform::ModLoaderTypes loaders)
    {
        return loaders & (ModPlatform::NeoForge | ModPlatform::Forge | ModPlatform::Fabric | ModPlatform::Quilt);
    }

   private:
    static int getClassId(ModPlatform::ResourceType type)
    {
        switch (type) {
            default:
            case ModPlatform::ResourceType::MOD:
                return 6;
            case ModPlatform::ResourceType::RESOURCE_PACK:
                return 12;
            case ModPlatform::ResourceType::SHADER_PACK:
                return 6552;
            case ModPlatform::ResourceType::MODPACK:
                return 4471;
        }
    }

    static int getMappedModLoader(ModPlatform::ModLoaderType loaders)
    {
        // https://docs.curseforge.com/?http#tocS_ModLoaderType
        switch (loaders) {
            case ModPlatform::Forge:
                return 1;
            case ModPlatform::Cauldron:
                return 2;
            case ModPlatform::LiteLoader:
                return 3;
            case ModPlatform::Fabric:
                return 4;
            case ModPlatform::Quilt:
                return 5;
            case ModPlatform::NeoForge:
                return 6;
        }
        return 0;
    }

    static const QStringList getModLoaderStrings(const ModPlatform::ModLoaderTypes types)
    {
        QStringList l;
        for (auto loader : { ModPlatform::NeoForge, ModPlatform::Forge, ModPlatform::Fabric, ModPlatform::Quilt }) {
            if (types & loader) {
                l << QString::number(getMappedModLoader(loader));
            }
        }
        return l;
    }

    static const QString getModLoaderFilters(ModPlatform::ModLoaderTypes types) { return "[" + getModLoaderStrings(types).join(',') + "]"; }

   public:
    [[nodiscard]] std::optional<QString> getSearchURL(SearchArgs const& args) const override
    {
        QStringList get_arguments;
        get_arguments.append(QString("classId=%1").arg(getClassId(args.type)));
        get_arguments.append(QString("index=%1").arg(args.offset));
        get_arguments.append("pageSize=25");
        if (args.search.has_value())
            get_arguments.append(QString("searchFilter=%1").arg(args.search.value()));
        if (args.sorting.has_value())
            get_arguments.append(QString("sortField=%1").arg(args.sorting.value().index));
        get_arguments.append("sortOrder=desc");
        if (args.loaders.has_value() && args.loaders.value() != 0)
            get_arguments.append(QString("modLoaderTypes=%1").arg(getModLoaderFilters(args.loaders.value())));
        if (args.categoryIds.has_value() && !args.categoryIds->empty())
            get_arguments.append(QString("categoryIds=[%1]").arg(args.categoryIds->join(",")));

        if (args.versions.has_value() && !args.versions.value().empty())
            get_arguments.append(QString("gameVersion=%1").arg(args.versions.value().front().toString()));

        return "https://api.curseforge.com/v1/mods/search?gameId=432&" + get_arguments.join('&');
    }

    [[nodiscard]] std::optional<QString> getVersionsURL(VersionSearchArgs const& args) const override
    {
        auto addonId = args.pack.addonId.toString();
        QString url = QString("https://api.curseforge.com/v1/mods/%1/files?pageSize=10000").arg(addonId);

        if (args.mcVersions.has_value())
            url += QString("&gameVersion=%1").arg(args.mcVersions.value().front().toString());

        if (args.loaders.has_value() && ModPlatform::hasSingleModLoaderSelected(args.loaders.value())) {
            int mappedModLoader = getMappedModLoader(static_cast<ModPlatform::ModLoaderType>(static_cast<int>(args.loaders.value())));
            url += QString("&modLoaderType=%1").arg(mappedModLoader);
        }
        return url;
    }

   private:
    [[nodiscard]] std::optional<QString> getInfoURL(QString const& id) const override
    {
        return QString("https://api.curseforge.com/v1/mods/%1").arg(id);
    }
    [[nodiscard]] std::optional<QString> getDependencyURL(DependencySearchArgs const& args) const override
    {
        auto addonId = args.dependency.addonId.toString();
        auto url =
            QString("https://api.curseforge.com/v1/mods/%1/files?pageSize=10000&gameVersion=%2").arg(addonId, args.mcVersion.toString());
        if (args.loader && ModPlatform::hasSingleModLoaderSelected(args.loader)) {
            int mappedModLoader = getMappedModLoader(static_cast<ModPlatform::ModLoaderType>(static_cast<int>(args.loader)));
            url += QString("&modLoaderType=%1").arg(mappedModLoader);
        }
        return url;
    }
};
