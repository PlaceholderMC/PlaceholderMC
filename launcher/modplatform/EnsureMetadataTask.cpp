#include "EnsureMetadataTask.h"

#include <MurmurHash2.h>
#include <QDebug>

#include "Application.h"
#include "Json.h"

#include "QObjectPtr.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/tasks/LocalResourceUpdateTask.h"

#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "modplatform/helpers/HashUtils.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "modplatform/modrinth/ModrinthPackIndex.h"

static ModrinthAPI modrinth_api;
static FlameAPI flame_api;

EnsureMetadataTask::EnsureMetadataTask(Resource* resource, QDir dir, ModPlatform::ResourceProvider prov)
    : Task(), m_indexDir(dir), m_provider(prov), m_hashingTask(nullptr), m_currentTask(nullptr)
{
    auto hashTask = createNewHash(resource);
    if (!hashTask)
        return;
    connect(hashTask.get(), &Hashing::Hasher::resultsReady, [this, resource](QString hash) { m_resources.insert(hash, resource); });
    connect(hashTask.get(), &Task::failed, [this, resource] { emitFail(resource, "", RemoveFromList::No); });
    m_hashingTask = hashTask;
}

EnsureMetadataTask::EnsureMetadataTask(QList<Resource*>& resources, QDir dir, ModPlatform::ResourceProvider prov)
    : Task(), m_indexDir(dir), m_provider(prov), m_currentTask(nullptr)
{
    auto hashTask = makeShared<ConcurrentTask>("MakeHashesTask", APPLICATION->settings()->get("NumberOfConcurrentTasks").toInt());
    m_hashingTask = hashTask;
    for (auto* resource : resources) {
        auto hash_task = createNewHash(resource);
        if (!hash_task)
            continue;
        connect(hash_task.get(), &Hashing::Hasher::resultsReady, [this, resource](QString hash) { m_resources.insert(hash, resource); });
        connect(hash_task.get(), &Task::failed, [this, resource] { emitFail(resource, "", RemoveFromList::No); });
        hashTask->addTask(hash_task);
    }
}

EnsureMetadataTask::EnsureMetadataTask(QHash<QString, Resource*>& resources, QDir dir, ModPlatform::ResourceProvider prov)
    : Task(), m_resources(resources), m_indexDir(dir), m_provider(prov), m_currentTask(nullptr)
{}

Hashing::Hasher::Ptr EnsureMetadataTask::createNewHash(Resource* resource)
{
    if (!resource || !resource->valid() || resource->type() == ResourceType::FOLDER)
        return nullptr;

    return Hashing::createHasher(resource->fileinfo().absoluteFilePath(), m_provider);
}

QString EnsureMetadataTask::getExistingHash(Resource* resource)
{
    // Check for already computed hashes
    // (linear on the number of mods vs. linear on the size of the mod's JAR)
    auto it = m_resources.keyValueBegin();
    while (it != m_resources.keyValueEnd()) {
        if ((*it).second == resource)
            break;
        it++;
    }

    // We already have the hash computed
    if (it != m_resources.keyValueEnd()) {
        return (*it).first;
    }

    // No existing hash
    return {};
}

bool EnsureMetadataTask::abort()
{
    // Prevent sending signals to a dead object
    disconnect(this, 0, 0, 0);

    if (m_currentTask)
        return m_currentTask->abort();
    return true;
}

void EnsureMetadataTask::executeTask()
{
    setStatus(tr("Checking if resources have metadata..."));

    for (auto* resource : m_resources) {
        if (!resource->valid()) {
            qDebug() << "Resource" << resource->name() << "is invalid!";
            emitFail(resource);
            continue;
        }

        // They already have the right metadata :o
        if (resource->status() != ResourceStatus::NO_METADATA && resource->metadata() && resource->metadata()->provider == m_provider) {
            qDebug() << "Resource" << resource->name() << "already has metadata!";
            emitReady(resource);
            continue;
        }

        // Folders don't have metadata
        if (resource->type() == ResourceType::FOLDER) {
            emitReady(resource);
        }
    }

    Task::Ptr version_task;

    switch (m_provider) {
        case (ModPlatform::ResourceProvider::MODRINTH):
            version_task = modrinthVersionsTask();
            break;
        case (ModPlatform::ResourceProvider::FLAME):
            version_task = flameVersionsTask();
            break;
    }

    auto invalidade_leftover = [this] {
        for (auto resource = m_resources.constBegin(); resource != m_resources.constEnd(); resource++)
            emitFail(resource.value(), resource.key(), RemoveFromList::No);
        m_resources.clear();

        emitSucceeded();
    };

    connect(version_task.get(), &Task::finished, this, [this, invalidade_leftover] {
        Task::Ptr project_task;

        switch (m_provider) {
            case (ModPlatform::ResourceProvider::MODRINTH):
                project_task = modrinthProjectsTask();
                break;
            case (ModPlatform::ResourceProvider::FLAME):
                project_task = flameProjectsTask();
                break;
        }

        if (!project_task) {
            invalidade_leftover();
            return;
        }

        connect(project_task.get(), &Task::finished, this, [this, invalidade_leftover, project_task] {
            invalidade_leftover();
            project_task->deleteLater();
            if (m_currentTask)
                m_currentTask.reset();
        });
        connect(project_task.get(), &Task::failed, this, &EnsureMetadataTask::emitFailed);

        m_currentTask = project_task;
        project_task->start();
    });

    if (m_resources.size() > 1)
        setStatus(tr("Requesting metadata information from %1...").arg(ModPlatform::ProviderCapabilities::readableName(m_provider)));
    else if (!m_resources.empty())
        setStatus(tr("Requesting metadata information from %1 for '%2'...")
                      .arg(ModPlatform::ProviderCapabilities::readableName(m_provider), m_resources.begin().value()->name()));

    m_currentTask = version_task;
    version_task->start();
}

void EnsureMetadataTask::emitReady(Resource* resource, QString key, RemoveFromList remove)
{
    if (!resource) {
        qCritical() << "Tried to mark a null resource as ready.";
        if (!key.isEmpty())
            m_resources.remove(key);

        return;
    }

    qDebug() << QString("Generated metadata for %1").arg(resource->name());
    emit metadataReady(resource);

    if (remove == RemoveFromList::Yes) {
        if (key.isEmpty())
            key = getExistingHash(resource);
        m_resources.remove(key);
    }
}

void EnsureMetadataTask::emitFail(Resource* resource, QString key, RemoveFromList remove)
{
    if (!resource) {
        qCritical() << "Tried to mark a null resource as failed.";
        if (!key.isEmpty())
            m_resources.remove(key);

        return;
    }

    qDebug() << QString("Failed to generate metadata for %1").arg(resource->name());
    emit metadataFailed(resource);

    if (remove == RemoveFromList::Yes) {
        if (key.isEmpty())
            key = getExistingHash(resource);
        m_resources.remove(key);
    }
}

// Modrinth

Task::Ptr EnsureMetadataTask::modrinthVersionsTask()
{
    auto hash_type = ModPlatform::ProviderCapabilities::hashType(ModPlatform::ResourceProvider::MODRINTH).first();

    auto response = std::make_shared<QByteArray>();
    auto ver_task = modrinth_api.currentVersions(m_resources.keys(), hash_type, response);

    // Prevents unfortunate timings when aborting the task
    if (!ver_task)
        return Task::Ptr{ nullptr };

    connect(ver_task.get(), &Task::succeeded, this, [this, response] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth::CurrentVersions at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;

            failed(parse_error.errorString());
            return;
        }

        try {
            auto entries = Json::requireObject(doc);
            for (auto& hash : m_resources.keys()) {
                auto resource = m_resources.find(hash).value();
                try {
                    auto entry = Json::requireObject(entries, hash);

                    setStatus(tr("Parsing API response from Modrinth for '%1'...").arg(resource->name()));
                    qDebug() << "Getting version for" << resource->name() << "from Modrinth";

                    m_tempVersions.insert(hash, Modrinth::loadIndexedPackVersion(entry));
                } catch (Json::JsonException& e) {
                    qDebug() << e.cause();
                    qDebug() << entries;

                    emitFail(resource);
                }
            }
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
    });

    return ver_task;
}

Task::Ptr EnsureMetadataTask::modrinthProjectsTask()
{
    QHash<QString, QString> addonIds;
    for (auto const& data : m_tempVersions)
        addonIds.insert(data.addonId.toString(), data.hash);

    auto response = std::make_shared<QByteArray>();
    Task::Ptr proj_task;

    if (addonIds.isEmpty()) {
        qWarning() << "No addonId found!";
    } else if (addonIds.size() == 1) {
        proj_task = modrinth_api.getProject(*addonIds.keyBegin(), response);
    } else {
        proj_task = modrinth_api.getProjects(addonIds.keys(), response);
    }

    // Prevents unfortunate timings when aborting the task
    if (!proj_task)
        return Task::Ptr{ nullptr };

    connect(proj_task.get(), &Task::succeeded, this, [this, response, addonIds] {
        QJsonParseError parse_error{};
        auto doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth projects task at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;
            return;
        }

        QJsonArray entries;

        try {
            if (addonIds.size() == 1)
                entries = { doc.object() };
            else
                entries = Json::requireArray(doc);
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }

        for (auto entry : entries) {
            ModPlatform::IndexedPack pack;

            try {
                auto entry_obj = Json::requireObject(entry);

                Modrinth::loadIndexedPack(pack, entry_obj);
            } catch (Json::JsonException& e) {
                qDebug() << e.cause();
                qDebug() << doc;

                // Skip this entry, since it has problems
                continue;
            }

            auto hash = addonIds.find(pack.addonId.toString()).value();

            auto resource_iter = m_resources.find(hash);
            if (resource_iter == m_resources.end()) {
                qWarning() << "Invalid project id from the API response.";
                continue;
            }

            auto* resource = resource_iter.value();

            setStatus(tr("Parsing API response from Modrinth for '%1'...").arg(resource->name()));

            updateMetadata(pack, m_tempVersions.find(hash).value(), resource);
        }
    });

    return proj_task;
}

// Flame
Task::Ptr EnsureMetadataTask::flameVersionsTask()
{
    auto response = std::make_shared<QByteArray>();

    QList<uint> fingerprints;
    for (auto& murmur : m_resources.keys()) {
        fingerprints.push_back(murmur.toUInt());
    }

    auto ver_task = flame_api.matchFingerprints(fingerprints, response);

    connect(ver_task.get(), &Task::succeeded, this, [this, response] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth::CurrentVersions at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *response;

            failed(parse_error.errorString());
            return;
        }

        try {
            auto doc_obj = Json::requireObject(doc);
            auto data_obj = Json::requireObject(doc_obj, "data");
            auto data_arr = Json::requireArray(data_obj, "exactMatches");

            if (data_arr.isEmpty()) {
                qWarning() << "No matches found for fingerprint search!";

                return;
            }

            for (auto match : data_arr) {
                auto match_obj = Json::ensureObject(match, {});
                auto file_obj = Json::ensureObject(match_obj, "file", {});

                if (match_obj.isEmpty() || file_obj.isEmpty()) {
                    qWarning() << "Fingerprint match is empty!";

                    return;
                }

                auto fingerprint = QString::number(Json::ensureVariant(file_obj, "fileFingerprint").toUInt());
                auto resource = m_resources.find(fingerprint);
                if (resource == m_resources.end()) {
                    qWarning() << "Invalid fingerprint from the API response.";
                    continue;
                }

                setStatus(tr("Parsing API response from CurseForge for '%1'...").arg((*resource)->name()));

                m_tempVersions.insert(fingerprint, FlameMod::loadIndexedPackVersion(file_obj));
            }

        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
    });

    return ver_task;
}

Task::Ptr EnsureMetadataTask::flameProjectsTask()
{
    QHash<QString, QString> addonIds;
    for (auto const& hash : m_resources.keys()) {
        if (m_tempVersions.contains(hash)) {
            auto data = m_tempVersions.find(hash).value();

            auto id_str = data.addonId.toString();
            if (!id_str.isEmpty())
                addonIds.insert(data.addonId.toString(), hash);
        }
    }

    auto response = std::make_shared<QByteArray>();
    Task::Ptr proj_task;

    if (addonIds.isEmpty()) {
        qWarning() << "No addonId found!";
    } else if (addonIds.size() == 1) {
        proj_task = flame_api.getProject(*addonIds.keyBegin(), response);
    } else {
        proj_task = flame_api.getProjects(addonIds.keys(), response);
    }

    // Prevents unfortunate timings when aborting the task
    if (!proj_task)
        return Task::Ptr{ nullptr };

    connect(proj_task.get(), &Task::succeeded, this, [this, response, addonIds] {
        QJsonParseError parse_error{};
        auto doc = QJsonDocument::fromJson(*response, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth projects task at " << parse_error.offset
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
                auto hash = addonIds.find(id).value();
                auto resource = m_resources.find(hash).value();

                ModPlatform::IndexedPack pack;
                try {
                    setStatus(tr("Parsing API response from CurseForge for '%1'...").arg(resource->name()));

                    FlameMod::loadIndexedPack(pack, entry_obj);

                } catch (Json::JsonException& e) {
                    qDebug() << e.cause();
                    qDebug() << entries;

                    emitFail(resource);
                }
                updateMetadata(pack, m_tempVersions.find(hash).value(), resource);
            }
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
    });

    return proj_task;
}

void EnsureMetadataTask::updateMetadata(ModPlatform::IndexedPack& pack, ModPlatform::IndexedVersion& ver, Resource* resource)
{
    try {
        // Prevent file name mismatch
        ver.fileName = resource->fileinfo().fileName();
        if (ver.fileName.endsWith(".disabled"))
            ver.fileName.chop(9);

        auto task = makeShared<LocalResourceUpdateTask>(m_indexDir, pack, ver);

        connect(task.get(), &Task::finished, this, [this, &pack, resource] { updateMetadataCallback(pack, resource); });

        m_updateMetadataTasks[ModPlatform::ProviderCapabilities::name(pack.provider) + pack.addonId.toString()] = task;
        task->start();
    } catch (Json::JsonException& e) {
        qDebug() << e.cause();

        emitFail(resource);
    }
}

void EnsureMetadataTask::updateMetadataCallback(ModPlatform::IndexedPack& pack, Resource* resource)
{
    QDir tmpIndexDir(m_indexDir);
    auto metadata = Metadata::get(tmpIndexDir, pack.slug);
    if (!metadata.isValid()) {
        qCritical() << "Failed to generate metadata at last step!";
        emitFail(resource);
        return;
    }

    resource->setMetadata(metadata);

    emitReady(resource);
}
