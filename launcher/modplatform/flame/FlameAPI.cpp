// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "FlameAPI.h"
#include <memory>
#include <optional>
#include "FlameModIndex.h"

#include "Application.h"
#include "Json.h"
#include "modplatform/ModIndex.h"
#include "net/ApiDownload.h"
#include "net/ApiUpload.h"
#include "net/NetJob.h"

Task::Ptr FlameAPI::matchFingerprints(const QList<uint>& fingerprints, std::shared_ptr<QByteArray> response)
{
    auto netJob = makeShared<NetJob>(QString("Flame::MatchFingerprints"), APPLICATION->network());

    QJsonObject body_obj;
    QJsonArray fingerprints_arr;
    for (auto& fp : fingerprints) {
        fingerprints_arr.append(QString("%1").arg(fp));
    }

    body_obj["fingerprints"] = fingerprints_arr;

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(Net::ApiUpload::makeByteArray(QString("https://api.curseforge.com/v1/fingerprints"), response, body_raw));

    return netJob;
}

QString FlameAPI::getModFileChangelog(int modId, int fileId)
{
    QEventLoop lock;
    QString changelog;

    auto netJob = makeShared<NetJob>(QString("Flame::FileChangelog"), APPLICATION->network());
    auto response = std::make_shared<QByteArray>();
    netJob->addNetAction(Net::ApiDownload::makeByteArray(
        QString("https://api.curseforge.com/v1/mods/%1/files/%2/changelog")
            .arg(QString::fromStdString(std::to_string(modId)), QString::fromStdString(std::to_string(fileId))),
        response));

    QObject::connect(netJob.get(), &NetJob::succeeded, [&netJob, response, &changelog] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Flame::FileChangelog at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;

            netJob->failed(parse_error.errorString());
            return;
        }

        changelog = Json::ensureString(doc.object(), "data");
    });

    QObject::connect(netJob.get(), &NetJob::finished, [&lock] { lock.quit(); });

    netJob->start();
    lock.exec();

    return changelog;
}

QString FlameAPI::getModDescription(int modId)
{
    QEventLoop lock;
    QString description;

    auto netJob = makeShared<NetJob>(QString("Flame::ModDescription"), APPLICATION->network());
    auto response = std::make_shared<QByteArray>();
    netJob->addNetAction(Net::ApiDownload::makeByteArray(
        QString("https://api.curseforge.com/v1/mods/%1/description").arg(QString::number(modId)), response));

    QObject::connect(netJob.get(), &NetJob::succeeded, [&netJob, response, &description] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Flame::ModDescription at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;

            netJob->failed(parse_error.errorString());
            return;
        }

        description = Json::ensureString(doc.object(), "data");
    });

    QObject::connect(netJob.get(), &NetJob::finished, [&lock] { lock.quit(); });

    netJob->start();
    lock.exec();

    return description;
}

Task::Ptr FlameAPI::getProjects(QStringList addonIds, std::shared_ptr<QByteArray> response) const
{
    auto netJob = makeShared<NetJob>(QString("Flame::GetProjects"), APPLICATION->network());

    QJsonObject body_obj;
    QJsonArray addons_arr;
    for (auto& addonId : addonIds) {
        addons_arr.append(addonId);
    }

    body_obj["modIds"] = addons_arr;

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(Net::ApiUpload::makeByteArray(QString("https://api.curseforge.com/v1/mods"), response, body_raw));

    QObject::connect(netJob.get(), &NetJob::failed, [body_raw] { qDebug() << body_raw; });

    return netJob;
}

Task::Ptr FlameAPI::getFiles(const QStringList& fileIds, std::shared_ptr<QByteArray> response) const
{
    auto netJob = makeShared<NetJob>(QString("Flame::GetFiles"), APPLICATION->network());

    QJsonObject body_obj;
    QJsonArray files_arr;
    for (auto& fileId : fileIds) {
        files_arr.append(fileId);
    }

    body_obj["fileIds"] = files_arr;

    QJsonDocument body(body_obj);
    auto body_raw = body.toJson();

    netJob->addNetAction(Net::ApiUpload::makeByteArray(QString("https://api.curseforge.com/v1/mods/files"), response, body_raw));

    QObject::connect(netJob.get(), &NetJob::failed, [body_raw] { qDebug() << body_raw; });

    return netJob;
}

Task::Ptr FlameAPI::getFile(const QString& addonId, const QString& fileId, std::shared_ptr<QByteArray> response) const
{
    auto netJob = makeShared<NetJob>(QString("Flame::GetFile"), APPLICATION->network());
    netJob->addNetAction(
        Net::ApiDownload::makeByteArray(QUrl(QString("https://api.curseforge.com/v1/mods/%1/files/%2").arg(addonId, fileId)), response));

    QObject::connect(netJob.get(), &NetJob::failed, [addonId, fileId] { qDebug() << "Flame API file failure" << addonId << fileId; });

    return netJob;
}

QList<ResourceAPI::SortingMethod> FlameAPI::getSortingMethods() const
{
    // https://docs.curseforge.com/?python#tocS_ModsSearchSortField
    return { { 1, "Featured", QObject::tr("Sort by Featured") },
             { 2, "Popularity", QObject::tr("Sort by Popularity") },
             { 3, "LastUpdated", QObject::tr("Sort by Last Updated") },
             { 4, "Name", QObject::tr("Sort by Name") },
             { 5, "Author", QObject::tr("Sort by Author") },
             { 6, "TotalDownloads", QObject::tr("Sort by Downloads") },
             { 7, "Category", QObject::tr("Sort by Category") },
             { 8, "GameVersion", QObject::tr("Sort by Game Version") } };
}

Task::Ptr FlameAPI::getCategories(std::shared_ptr<QByteArray> response, ModPlatform::ResourceType type)
{
    auto netJob = makeShared<NetJob>(QString("Flame::GetCategories"), APPLICATION->network());
    netJob->addNetAction(Net::ApiDownload::makeByteArray(
        QUrl(QString("https://api.curseforge.com/v1/categories?gameId=432&classId=%1").arg(getClassId(type))), response));
    QObject::connect(netJob.get(), &Task::failed, [](QString msg) { qDebug() << "Flame failed to get categories:" << msg; });
    return netJob;
}

Task::Ptr FlameAPI::getModCategories(std::shared_ptr<QByteArray> response)
{
    return getCategories(response, ModPlatform::ResourceType::MOD);
}

QList<ModPlatform::Category> FlameAPI::loadModCategories(std::shared_ptr<QByteArray> response)
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
        auto obj = Json::requireObject(doc);
        auto arr = Json::requireArray(obj, "data");

        for (auto val : arr) {
            auto cat = Json::requireObject(val);
            auto id = Json::requireInteger(cat, "id");
            auto name = Json::requireString(cat, "name");
            categories.push_back({ name, QString::number(id) });
        }

    } catch (Json::JsonException& e) {
        qCritical() << "Failed to parse response from a version request.";
        qCritical() << e.what();
        qDebug() << doc;
    }
    return categories;
};

std::optional<ModPlatform::IndexedVersion> FlameAPI::getLatestVersion(QVector<ModPlatform::IndexedVersion> versions,
                                                                      QList<ModPlatform::ModLoaderType> instanceLoaders,
                                                                      ModPlatform::ModLoaderTypes modLoaders,
                                                                      bool checkLoaders)
{
    if (!checkLoaders) {
        std::optional<ModPlatform::IndexedVersion> ver;
        for (auto file_tmp : versions) {
            if (!ver.has_value() || file_tmp.date > ver->date) {
                ver = file_tmp;
            }
        }
        return ver;
    }
    QHash<ModPlatform::ModLoaderType, ModPlatform::IndexedVersion> bestMatch;
    auto checkVersion = [&bestMatch](const ModPlatform::IndexedVersion& version, const ModPlatform::ModLoaderType& loader) {
        if (bestMatch.contains(loader)) {
            auto best = bestMatch.value(loader);
            if (version.date > best.date) {
                bestMatch[loader] = version;
            }
        } else {
            bestMatch[loader] = version;
        }
    };
    for (auto file_tmp : versions) {
        auto loaders = ModPlatform::modLoaderTypesToList(file_tmp.loaders);
        if (loaders.isEmpty()) {
            checkVersion(file_tmp, ModPlatform::ModLoaderType(0));
        } else {
            for (auto loader : loaders) {
                checkVersion(file_tmp, loader);
            }
        }
    }
    // edge case: mod has installed for forge but the instance is fabric => fabric version will be prioritizated on update
    auto currentLoaders = instanceLoaders + ModPlatform::modLoaderTypesToList(modLoaders);
    currentLoaders.append(ModPlatform::ModLoaderType(0));  // add a fallback in case the versions do not define a loader

    for (auto loader : currentLoaders) {
        if (bestMatch.contains(loader)) {
            return bestMatch.value(loader);
        }
    }
    return {};
}
