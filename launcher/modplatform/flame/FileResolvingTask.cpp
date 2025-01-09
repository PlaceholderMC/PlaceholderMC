// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include "FileResolvingTask.h"
#include <algorithm>

#include "Json.h"
#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "modplatform/modrinth/ModrinthAPI.h"

#include "modplatform/modrinth/ModrinthPackIndex.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

static const FlameAPI flameAPI;
static ModrinthAPI modrinthAPI;

Flame::FileResolvingTask::FileResolvingTask(Flame::Manifest& toProcess) : m_manifest(toProcess) {}

bool Flame::FileResolvingTask::abort()
{
    bool aborted = true;
    if (m_task) {
        aborted = m_task->abort();
    }
    return aborted ? Task::abort() : false;
}

void Flame::FileResolvingTask::executeTask()
{
    if (m_manifest.files.isEmpty()) {  // no file to resolve so leave it empty and emit success immediately
        emitSucceeded();
        return;
    }
    setStatus(tr("Resolving mod IDs..."));
    setProgress(0, 3);
    m_result.reset(new QByteArray());

    QStringList fileIds;
    for (auto file : m_manifest.files) {
        fileIds.push_back(QString::number(file.fileId));
    }
    m_task = flameAPI.getFiles(fileIds, m_result);

    auto step_progress = std::make_shared<TaskStepProgress>();
    connect(m_task.get(), &Task::finished, this, [this, step_progress]() {
        step_progress->state = TaskStepState::Succeeded;
        stepProgress(*step_progress);
        netJobFinished();
    });
    connect(m_task.get(), &Task::failed, this, [this, step_progress](QString reason) {
        step_progress->state = TaskStepState::Failed;
        stepProgress(*step_progress);
        emitFailed(reason);
    });
    connect(m_task.get(), &Task::stepProgress, this, &FileResolvingTask::propagateStepProgress);
    connect(m_task.get(), &Task::progress, this, [this, step_progress](qint64 current, qint64 total) {
        qDebug() << "Resolve slug progress" << current << total;
        step_progress->update(current, total);
        stepProgress(*step_progress);
    });
    connect(m_task.get(), &Task::status, this, [this, step_progress](QString status) {
        step_progress->status = status;
        stepProgress(*step_progress);
    });

    m_task->start();
}

void Flame::FileResolvingTask::netJobFinished()
{
    setProgress(1, 3);
    // job to check modrinth for blocked projects
    QJsonDocument doc;
    QJsonArray array;

    try {
        doc = Json::requireDocument(*m_result);
        array = Json::requireArray(doc.object()["data"]);
    } catch (Json::JsonException& e) {
        qCritical() << "Non-JSON data returned from the CF API";
        qCritical() << e.cause();

        emitFailed(tr("Invalid data returned from the API."));

        return;
    }

    QStringList hashes;
    for (QJsonValueRef file : array) {
        try {
            auto obj = Json::requireObject(file);
            auto version = FlameMod::loadIndexedPackVersion(obj);
            auto fileid = version.fileId.toInt();
            m_manifest.files[fileid].version = version;
            auto url = QUrl(version.downloadUrl, QUrl::TolerantMode);
            if (!url.isValid() && "sha1" == version.hash_type && !version.hash.isEmpty()) {
                hashes.push_back(version.hash);
            }
        } catch (Json::JsonException& e) {
            qCritical() << "Non-JSON data returned from the CF API";
            qCritical() << e.cause();

            emitFailed(tr("Invalid data returned from the API."));

            return;
        }
    }
    if (hashes.isEmpty()) {
        getFlameProjects();
        return;
    }
    m_result.reset(new QByteArray());
    m_task = modrinthAPI.currentVersions(hashes, "sha1", m_result);
    (dynamic_cast<NetJob*>(m_task.get()))->setAskRetry(false);
    auto step_progress = std::make_shared<TaskStepProgress>();
    connect(m_task.get(), &Task::finished, this, [this, step_progress]() {
        step_progress->state = TaskStepState::Succeeded;
        stepProgress(*step_progress);
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*m_result, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth::CurrentVersions at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *m_result;

            getFlameProjects();
            return;
        }

        try {
            auto entries = Json::requireObject(doc);
            for (auto& out : m_manifest.files) {
                auto url = QUrl(out.version.downloadUrl, QUrl::TolerantMode);
                if (!url.isValid() && "sha1" == out.version.hash_type && !out.version.hash.isEmpty()) {
                    try {
                        auto entry = Json::requireObject(entries, out.version.hash);

                        auto file = Modrinth::loadIndexedPackVersion(entry);

                        // If there's more than one mod loader for this version, we can't know for sure
                        // which file is relative to each loader, so it's best to not use any one and
                        // let the user download it manually.
                        if (!file.loaders || hasSingleModLoaderSelected(file.loaders)) {
                            out.version.downloadUrl = file.downloadUrl;
                            qDebug() << "Found alternative on modrinth " << out.version.fileName;
                        }
                    } catch (Json::JsonException& e) {
                        qDebug() << e.cause();
                        qDebug() << entries;
                    }
                }
            }
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
        getFlameProjects();
    });
    connect(m_task.get(), &Task::failed, this, [this, step_progress](QString reason) {
        step_progress->state = TaskStepState::Failed;
        stepProgress(*step_progress);
    });
    connect(m_task.get(), &Task::stepProgress, this, &FileResolvingTask::propagateStepProgress);
    connect(m_task.get(), &Task::progress, this, [this, step_progress](qint64 current, qint64 total) {
        qDebug() << "Resolve slug progress" << current << total;
        step_progress->update(current, total);
        stepProgress(*step_progress);
    });
    connect(m_task.get(), &Task::status, this, [this, step_progress](QString status) {
        step_progress->status = status;
        stepProgress(*step_progress);
    });

    m_task->start();
}

void Flame::FileResolvingTask::getFlameProjects()
{
    setProgress(2, 3);
    m_result.reset(new QByteArray());
    QStringList addonIds;
    for (auto file : m_manifest.files) {
        addonIds.push_back(QString::number(file.projectId));
    }

    m_task = flameAPI.getProjects(addonIds, m_result);

    auto step_progress = std::make_shared<TaskStepProgress>();
    connect(m_task.get(), &Task::succeeded, this, [this, step_progress] {
        QJsonParseError parse_error{};
        auto doc = QJsonDocument::fromJson(*m_result, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response from Modrinth projects task at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qWarning() << *m_result;
            return;
        }

        try {
            QJsonArray entries;
            entries = Json::requireArray(Json::requireObject(doc), "data");

            for (auto entry : entries) {
                auto entry_obj = Json::requireObject(entry);
                auto id = Json::requireInteger(entry_obj, "id");
                auto file = std::find_if(m_manifest.files.begin(), m_manifest.files.end(),
                                         [id](const Flame::File& file) { return file.projectId == id; });
                if (file == m_manifest.files.end()) {
                    continue;
                }

                setStatus(tr("Parsing API response from CurseForge for '%1'...").arg(file->version.fileName));
                FlameMod::loadIndexedPack(file->pack, entry_obj);
            }
        } catch (Json::JsonException& e) {
            qDebug() << e.cause();
            qDebug() << doc;
        }
        step_progress->state = TaskStepState::Succeeded;
        stepProgress(*step_progress);
        emitSucceeded();
    });

    connect(m_task.get(), &Task::failed, this, [this, step_progress](QString reason) {
        step_progress->state = TaskStepState::Failed;
        stepProgress(*step_progress);
        emitFailed(reason);
    });
    connect(m_task.get(), &Task::stepProgress, this, &FileResolvingTask::propagateStepProgress);
    connect(m_task.get(), &Task::progress, this, [this, step_progress](qint64 current, qint64 total) {
        qDebug() << "Resolve slug progress" << current << total;
        step_progress->update(current, total);
        stepProgress(*step_progress);
    });
    connect(m_task.get(), &Task::status, this, [this, step_progress](QString status) {
        step_progress->status = status;
        stepProgress(*step_progress);
    });

    m_task->start();
}
