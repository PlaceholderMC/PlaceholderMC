#include "FlameCheckUpdate.h"
#include "Application.h"
#include "FlameAPI.h"
#include "FlameModIndex.h"

#include <QHash>
#include <memory>

#include "Json.h"

#include "QObjectPtr.h"
#include "ResourceDownloadTask.h"

#include "minecraft/mod/tasks/GetModDependenciesTask.h"

#include "modplatform/ModIndex.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

static FlameAPI api;

bool FlameCheckUpdate::abort()
{
    bool result = false;
    if (m_task && m_task->canAbort()) {
        result = m_task->abort();
    }
    Task::abort();
    return result;
}

/* Check for update:
 * - Get latest version available
 * - Compare hash of the latest version with the current hash
 * - If equal, no updates, else, there's updates, so add to the list
 * */
void FlameCheckUpdate::executeTask()
{
    setStatus(tr("Preparing resources for CurseForge..."));

    auto netJob = new NetJob("Get latest versions", APPLICATION->network());
    connect(netJob, &Task::finished, this, &FlameCheckUpdate::collectBlockedMods);

    connect(netJob, &Task::progress, this, &FlameCheckUpdate::setProgress);
    connect(netJob, &Task::stepProgress, this, &FlameCheckUpdate::propagateStepProgress);
    connect(netJob, &Task::details, this, &FlameCheckUpdate::setDetails);
    for (auto* resource : m_resources) {
        auto versions_url_optional = api.getVersionsURL({ { resource->metadata()->project_id.toString() }, m_game_versions });
        if (!versions_url_optional.has_value())
            continue;

        auto response = std::make_shared<QByteArray>();
        auto task = Net::ApiDownload::makeByteArray(versions_url_optional.value(), response);

        connect(task.get(), &Task::succeeded, this, [this, resource, response] { getLatestVersionCallback(resource, response); });
        netJob->addNetAction(task);
    }
    m_task.reset(netJob);
    m_task->start();
}

void FlameCheckUpdate::getLatestVersionCallback(Resource* resource, std::shared_ptr<QByteArray> response)
{
    QJsonParseError parse_error{};
    QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from latest mod version at " << parse_error.offset
                   << " reason: " << parse_error.errorString();
        qWarning() << *response;
        return;
    }

    // Fake pack with the necessary info to pass to the download task :)
    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->name = resource->name();
    pack->slug = resource->metadata()->slug;
    pack->addonId = resource->metadata()->project_id;
    pack->provider = ModPlatform::ResourceProvider::FLAME;
    try {
        auto obj = Json::requireObject(doc);
        auto arr = Json::requireArray(obj, "data");

        FlameMod::loadIndexedPackVersions(*pack.get(), arr);
    } catch (Json::JsonException& e) {
        qCritical() << "Failed to parse response from a version request.";
        qCritical() << e.what();
        qDebug() << doc;
    }
    auto latest_ver = api.getLatestVersion(pack->versions, m_loaders_list, resource->metadata()->loaders);

    setStatus(tr("Parsing the API response from CurseForge for '%1'...").arg(resource->name()));

    if (!latest_ver.has_value() || !latest_ver->addonId.isValid()) {
        QString reason;
        if (dynamic_cast<Mod*>(resource) != nullptr)
            reason =
                tr("No valid version found for this resource. It's probably unavailable for the current game "
                   "version / mod loader.");
        else
            reason = tr("No valid version found for this resource. It's probably unavailable for the current game version.");

        emit checkFailed(resource, reason);
        return;
    }

    if (latest_ver->downloadUrl.isEmpty() && latest_ver->fileId != resource->metadata()->file_id) {
        m_blocked[resource] = latest_ver->fileId.toString();
        return;
    }

    if (!latest_ver->hash.isEmpty() &&
        (resource->metadata()->hash != latest_ver->hash || resource->status() == ResourceStatus::NOT_INSTALLED)) {
        auto old_version = resource->metadata()->version_number;
        if (old_version.isEmpty()) {
            if (resource->status() == ResourceStatus::NOT_INSTALLED)
                old_version = tr("Not installed");
            else
                old_version = tr("Unknown");
        }

        auto download_task = makeShared<ResourceDownloadTask>(pack, latest_ver.value(), m_resource_model);
        m_updates.emplace_back(pack->name, resource->metadata()->hash, old_version, latest_ver->version, latest_ver->version_type,
                               api.getModFileChangelog(latest_ver->addonId.toInt(), latest_ver->fileId.toInt()),
                               ModPlatform::ResourceProvider::FLAME, download_task, resource->enabled());
    }
    m_deps.append(std::make_shared<GetModDependenciesTask::PackDependency>(pack, latest_ver.value()));
}

void FlameCheckUpdate::collectBlockedMods()
{
    QStringList addonIds;
    QHash<QString, Resource*> quickSearch;
    for (auto const& resource : m_blocked.keys()) {
        auto addonId = resource->metadata()->project_id.toString();
        addonIds.append(addonId);
        quickSearch[addonId] = resource;
    }

    auto response = std::make_shared<QByteArray>();
    Task::Ptr projTask;

    if (addonIds.isEmpty()) {
        emitSucceeded();
        return;
    } else if (addonIds.size() == 1) {
        projTask = api.getProject(*addonIds.begin(), response);
    } else {
        projTask = api.getProjects(addonIds, response);
    }

    connect(projTask.get(), &Task::succeeded, this, [this, response, addonIds, quickSearch] {
        QJsonParseError parse_error{};
        auto doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Flame projects task at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;
            return;
        }

        try {
            QJsonArray entries;
            if (addonIds.size() == 1)
                entries = { Json::requireObject(Json::requireObject(doc), "data") };
            else
                entries = Json::requireArray(Json::requireObject(doc), "data");

            for (auto entry : entries) {
                auto entry_obj = Json::requireObject(entry);

                auto id = QString::number(Json::requireInteger(entry_obj, "id"));

                auto resource = quickSearch.find(id).value();

                ModPlatform::IndexedPack pack;
                try {
                    setStatus(tr("Parsing API response from CurseForge for '%1'...").arg(resource->name()));

                    FlameMod::loadIndexedPack(pack, entry_obj);
                    auto recover_url = QString("%1/download/%2").arg(pack.websiteUrl, m_blocked[resource]);
                    emit checkFailed(resource, tr("Resource has a new update available, but is not downloadable using CurseForge."),
                                     recover_url);
                } catch (Json::JsonException& e) {
                    qDebug() << e.cause();
                    qDebug() << entries;
                }
            }
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
    });

    connect(projTask.get(), &Task::finished, this, &FlameCheckUpdate::emitSucceeded);  // do not care much about error
    connect(projTask.get(), &Task::progress, this, &FlameCheckUpdate::setProgress);
    connect(projTask.get(), &Task::stepProgress, this, &FlameCheckUpdate::propagateStepProgress);
    connect(projTask.get(), &Task::details, this, &FlameCheckUpdate::setDetails);
    m_task.reset(projTask);
    m_task->start();
}