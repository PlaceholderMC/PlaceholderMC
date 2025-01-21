// SPDX-FileCopyrightText: 2022-2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "BuildConfig.h"
#include "modplatform/ModIndex.h"
#include "modplatform/helpers/NetworkResourceAPI.h"

#include <QDebug>

class ModrinthAPI : public NetworkResourceAPI {
   public:
    Task::Ptr currentVersion(QString hash, QString hash_format, std::shared_ptr<QByteArray> response);

    Task::Ptr currentVersions(const QStringList& hashes, QString hash_format, std::shared_ptr<QByteArray> response);

    Task::Ptr latestVersion(QString hash,
                            QString hash_format,
                            std::optional<std::list<Version>> mcVersions,
                            std::optional<ModPlatform::ModLoaderTypes> loaders,
                            std::shared_ptr<QByteArray> response);

    Task::Ptr latestVersions(const QStringList& hashes,
                             QString hash_format,
                             std::optional<std::list<Version>> mcVersions,
                             std::optional<ModPlatform::ModLoaderTypes> loaders,
                             std::shared_ptr<QByteArray> response);

    Task::Ptr getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const override;

    static Task::Ptr getModCategories(std::shared_ptr<QByteArray> response);
    static QList<ModPlatform::Category> loadCategories(std::shared_ptr<QByteArray> response, QString projectType);
    static QList<ModPlatform::Category> loadModCategories(std::shared_ptr<QByteArray> response);

   public:
    [[nodiscard]] auto getSortingMethods() const -> QList<ResourceAPI::SortingMethod> override;

    inline auto getAuthorURL(const QString& name) const -> QString { return "https://modrinth.com/user/" + name; };

    static auto getModLoaderStrings(const ModPlatform::ModLoaderTypes types) -> const QStringList
    {
        QStringList l;
        for (auto loader :
             { ModPlatform::NeoForge, ModPlatform::Forge, ModPlatform::Fabric, ModPlatform::Quilt, ModPlatform::LiteLoader }) {
            if (types & loader) {
                l << getModLoaderAsString(loader);
            }
        }
        return l;
    }

    static auto getModLoaderFilters(ModPlatform::ModLoaderTypes types) -> const QString
    {
        QStringList l;
        for (auto loader : getModLoaderStrings(types)) {
            l << QString("\"categories:%1\"").arg(loader);
        }
        return l.join(',');
    }

    static auto getCategoriesFilters(QStringList categories) -> const QString
    {
        QStringList l;
        for (auto cat : categories) {
            l << QString("\"categories:%1\"").arg(cat);
        }
        return l.join(',');
    }

    static auto getSideFilters(QString side) -> const QString
    {
        if (side.isEmpty() || side == "both") {
            return {};
        }
        if (side == "client")
            return QString("\"client_side:required\",\"client_side:optional\"");
        if (side == "server")
            return QString("\"server_side:required\",\"server_side:optional\"");
        return {};
    }

   private:
    [[nodiscard]] static QString resourceTypeParameter(ModPlatform::ResourceType type)
    {
        switch (type) {
            case ModPlatform::ResourceType::MOD:
                return "mod";
            case ModPlatform::ResourceType::RESOURCE_PACK:
                return "resourcepack";
            case ModPlatform::ResourceType::SHADER_PACK:
                return "shader";
            case ModPlatform::ResourceType::MODPACK:
                return "modpack";
            default:
                qWarning() << "Invalid resource type for Modrinth API!";
                break;
        }

        return "";
    }

    [[nodiscard]] QString createFacets(SearchArgs const& args) const
    {
        QStringList facets_list;

        if (args.loaders.has_value() && args.loaders.value() != 0)
            facets_list.append(QString("[%1]").arg(getModLoaderFilters(args.loaders.value())));
        if (args.versions.has_value() && !args.versions.value().empty())
            facets_list.append(QString("[%1]").arg(getGameVersionsArray(args.versions.value())));
        if (args.side.has_value()) {
            auto side = getSideFilters(args.side.value());
            if (!side.isEmpty())
                facets_list.append(QString("[%1]").arg(side));
        }
        if (args.categoryIds.has_value() && !args.categoryIds->empty())
            facets_list.append(QString("[%1]").arg(getCategoriesFilters(args.categoryIds.value())));
        if (args.openSource)
            facets_list.append("[\"open_source:true\"]");

        facets_list.append(QString("[\"project_type:%1\"]").arg(resourceTypeParameter(args.type)));

        return QString("[%1]").arg(facets_list.join(','));
    }

   public:
    [[nodiscard]] inline auto getSearchURL(SearchArgs const& args) const -> std::optional<QString> override
    {
        if (args.loaders.has_value() && args.loaders.value() != 0) {
            if (!validateModLoaders(args.loaders.value())) {
                qWarning() << "Modrinth - or our interface - does not support any the provided mod loaders!";
                return {};
            }
        }

        QStringList get_arguments;
        get_arguments.append(QString("offset=%1").arg(args.offset));
        get_arguments.append(QString("limit=25"));
        if (args.search.has_value())
            get_arguments.append(QString("query=%1").arg(args.search.value()));
        if (args.sorting.has_value())
            get_arguments.append(QString("index=%1").arg(args.sorting.value().name));
        get_arguments.append(QString("facets=%1").arg(createFacets(args)));

        return BuildConfig.MODRINTH_PROD_URL + "/search?" + get_arguments.join('&');
    };

    inline auto getInfoURL(QString const& id) const -> std::optional<QString> override
    {
        return BuildConfig.MODRINTH_PROD_URL + "/project/" + id;
    };

    inline auto getMultipleModInfoURL(QStringList ids) const -> QString
    {
        return BuildConfig.MODRINTH_PROD_URL + QString("/projects?ids=[\"%1\"]").arg(ids.join("\",\""));
    };

    inline auto getVersionsURL(VersionSearchArgs const& args) const -> std::optional<QString> override
    {
        QStringList get_arguments;
        if (args.mcVersions.has_value())
            get_arguments.append(QString("game_versions=[%1]").arg(getGameVersionsString(args.mcVersions.value())));
        if (args.loaders.has_value())
            get_arguments.append(QString("loaders=[\"%1\"]").arg(getModLoaderStrings(args.loaders.value()).join("\",\"")));

        return QString("%1/project/%2/version%3%4")
            .arg(BuildConfig.MODRINTH_PROD_URL, args.pack.addonId.toString(), get_arguments.isEmpty() ? "" : "?", get_arguments.join('&'));
    };

    QString getGameVersionsArray(std::list<Version> mcVersions) const
    {
        QString s;
        for (auto& ver : mcVersions) {
            s += QString("\"versions:%1\",").arg(ver.toString());
        }
        s.remove(s.length() - 1, 1);  // remove last comma
        return s.isEmpty() ? QString() : s;
    }

    static inline auto validateModLoaders(ModPlatform::ModLoaderTypes loaders) -> bool
    {
        return loaders & (ModPlatform::NeoForge | ModPlatform::Forge | ModPlatform::Fabric | ModPlatform::Quilt | ModPlatform::LiteLoader);
    }

    [[nodiscard]] std::optional<QString> getDependencyURL(DependencySearchArgs const& args) const override
    {
        return args.dependency.version.length() != 0 ? QString("%1/version/%2").arg(BuildConfig.MODRINTH_PROD_URL, args.dependency.version)
                                                     : QString("%1/project/%2/version?game_versions=[\"%3\"]&loaders=[\"%4\"]")
                                                           .arg(BuildConfig.MODRINTH_PROD_URL)
                                                           .arg(args.dependency.addonId.toString())
                                                           .arg(args.mcVersion.toString())
                                                           .arg(getModLoaderStrings(args.loader).join("\",\""));
    };
};
