// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ModrinthAPI.h"

#include "Application.h"
#include "Json.h"
#include "modplatform/modrinth/ModrinthPackIndex.h"
#include "net/ApiDownload.h"
#include "net/ApiUpload.h"
#include "net/NetJob.h"
#include "net/Upload.h"

Task::Ptr ModrinthAPI::currentVersion(QString hash, QString hash_format, std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCurrentVersion"), APPLICATION->network());

    netJob->addNetAction(Net::ApiDownload::makeByteArray(
        QString(BuildConfig.MODRINTH_PROD_URL + "/version_file/%1?algorithm=%2").arg(hash, hash_format), response));

    return netJob;
}

Task::Ptr ModrinthAPI::currentVersions(const QStringList& hashes, QString hash_format, std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCurrentVersions"), APPLICATION->network());

    QJsonObject body_obj;

    Json::writeStringList(body_obj, "hashes", hashes);
    Json::writeString(body_obj, "algorithm", hash_format);

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(Net::ApiUpload::makeByteArray(QString(BuildConfig.MODRINTH_PROD_URL + "/version_files"), response, body_raw));
    netJob->setAskRetry(false);
    return netJob;
}

Task::Ptr ModrinthAPI::latestVersion(QString hash,
                                     QString hash_format,
                                     std::optional<std::list<Version>> mcVersions,
                                     std::optional<ModPlatform::ModLoaderTypes> loaders,
                                     std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetLatestVersion"), APPLICATION->network());

    QJsonObject body_obj;

    if (loaders.has_value())
        Json::writeStringList(body_obj, "loaders", getModLoaderStrings(loaders.value()));

    if (mcVersions.has_value()) {
        QStringList game_versions;
        for (auto& ver : mcVersions.value()) {
            game_versions.append(ver.toString());
        }
        Json::writeStringList(body_obj, "game_versions", game_versions);
    }

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(Net::ApiUpload::makeByteArray(
        QString(BuildConfig.MODRINTH_PROD_URL + "/version_file/%1/update?algorithm=%2").arg(hash, hash_format), response, body_raw));

    return netJob;
}

Task::Ptr ModrinthAPI::latestVersions(const QStringList& hashes,
                                      QString hash_format,
                                      std::optional<std::list<Version>> mcVersions,
                                      std::optional<ModPlatform::ModLoaderTypes> loaders,
                                      std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetLatestVersions"), APPLICATION->network());

    QJsonObject body_obj;

    Json::writeStringList(body_obj, "hashes", hashes);
    Json::writeString(body_obj, "algorithm", hash_format);

    if (loaders.has_value())
        Json::writeStringList(body_obj, "loaders", getModLoaderStrings(loaders.value()));

    if (mcVersions.has_value()) {
        QStringList game_versions;
        for (auto& ver : mcVersions.value()) {
            game_versions.append(ver.toString());
        }
        Json::writeStringList(body_obj, "game_versions", game_versions);
    }

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(
        Net::ApiUpload::makeByteArray(QString(BuildConfig.MODRINTH_PROD_URL + "/version_files/update"), response, body_raw));

    return netJob;
}

Task::Ptr ModrinthAPI::getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetProjects"), APPLICATION->network());
    auto searchUrl = getMultipleModInfoURL(addonIds);

    netJob->addNetAction(Net::ApiDownload::makeByteArray(QUrl(searchUrl), response));

    return netJob;
}

QList<ResourceAPI::SortingMethod> ModrinthAPI::getSortingMethods() const
{
    // https://docs.modrinth.com/api-spec/#tag/projects/operation/searchProjects
    return { { 1, "relevance", QObject::tr("Sort by Relevance") },
             { 2, "downloads", QObject::tr("Sort by Downloads") },
             { 3, "follows", QObject::tr("Sort by Follows") },
             { 4, "newest", QObject::tr("Sort by Newest") },
             { 5, "updated", QObject::tr("Sort by Last Updated") } };
}

Task::Ptr ModrinthAPI::getModCategories(std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCategories"), APPLICATION->network());
    netJob->addNetAction(Net::ApiDownload::makeByteArray(QUrl(BuildConfig.MODRINTH_PROD_URL + "/tag/category"), response));
    QObject::connect(netJob.get(), &Task::failed, [](QString msg) { qDebug() << "Modrinth failed to get categories:" << msg; });
    return netJob;
}

QList<ModPlatform::Category> ModrinthAPI::loadCategories(std::shared_ptr<QByteArray> response, QString projectType)
{
    QList<ModPlatform::Category> categories;
    QJsonParseError parse_error{};
    QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from categories at " << parse_error.offset
                   << " reason: " << parse_error.errorString();
        qWarning() << *response;
        return categories;
    }

    try {
        auto arr = Json::requireArray(doc);

        for (auto val : arr) {
            auto cat = Json::requireObject(val);
            auto name = Json::requireString(cat, "name");
            if (Json::ensureString(cat, "project_type", "") == projectType)
                categories.push_back({ name, name });
        }

    } catch (Json::JsonException& e) {
        qCritical() << "Failed to parse response from a version request.";
        qCritical() << e.what();
        qDebug() << doc;
    }
    return categories;
}

QList<ModPlatform::Category> ModrinthAPI::loadModCategories(std::shared_ptr<QByteArray> response)
{
    return loadCategories(response, "mod");
}
Task::Ptr ModrinthAPI::getVersionFromHash(QString hash, ModPlatform::IndexedVersion& output)
{
    auto hash_type = ModPlatform::ProviderCapabilities::hashType(ModPlatform::ResourceProvider::MODRINTH).first();
    auto response = std::make_shared<QByteArray>();
    auto ver_task = currentVersion(hash, hash_type, response);
    QObject::connect(ver_task.get(), &Task::succeeded, [response, &output] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth::CurrentVersions at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;
            return;
        }

        try {
            auto entry = Json::requireObject(doc);
            output = Modrinth::loadIndexedPackVersion(entry);
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
    });
    return ver_task;
}
