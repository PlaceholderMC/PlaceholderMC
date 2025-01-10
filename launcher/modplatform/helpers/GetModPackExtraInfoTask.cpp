// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "modplatform/helpers/GetModPackExtraInfoTask.h"
#include <memory>
#include "Application.h"
#include "Json.h"
#include "QObjectPtr.h"
#include "icons/IconList.h"
#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "modplatform/helpers/HashUtils.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "modplatform/modrinth/ModrinthPackIndex.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

GetModPackExtraInfoTask::GetModPackExtraInfoTask(QString path, ModPlatform::ResourceProvider provider) : m_path(path), m_provider(provider)
{
    switch (m_provider) {
        case ModPlatform::ResourceProvider::MODRINTH:
            m_api = std::make_unique<ModrinthAPI>();
            break;
        case ModPlatform::ResourceProvider::FLAME:
            m_api = std::make_unique<FlameAPI>();
            break;
    }
}

bool GetModPackExtraInfoTask::abort()
{
    if (m_current_task)
        m_current_task->abort();

    emitAborted();
    return true;
}

void GetModPackExtraInfoTask::executeTask()
{
    setStatus(tr("Generating file hash"));
    setProgress(1, 4);
    auto hashTask = Hashing::createHasher(m_path, m_provider);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(hashTask.get(), &Hashing::Hasher::resultsReady, this, &GetModPackExtraInfoTask::hashDone);
    connect(hashTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(hashTask.get(), &Task::aborted, this, &GetModPackExtraInfoTask::emitAborted);
    connect(hashTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(hashTask.get(), &Task::stepProgress, this, &GetModPackExtraInfoTask::propagateStepProgress);

    connect(hashTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(hashTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_current_task.reset(hashTask);
    hashTask->start();
}

void GetModPackExtraInfoTask::hashDone(QString result)
{
    setStatus(tr("Matching hash with version"));
    setProgress(2, 4);
    auto verTask = m_api->getVersionFromHash(result, m_version);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(verTask.get(), &Task::succeeded, this, &GetModPackExtraInfoTask::getProjectInfo);
    connect(verTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(verTask.get(), &Task::aborted, this, &GetModPackExtraInfoTask::emitAborted);
    connect(verTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(verTask.get(), &Task::stepProgress, this, &GetModPackExtraInfoTask::propagateStepProgress);

    connect(verTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(verTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_current_task.reset(verTask);
    verTask->start();
}

void GetModPackExtraInfoTask::getProjectInfo()
{
    if (!m_version.addonId.isValid()) {
        emitFailed(tr("Version not found"));
        return;
    }
    setStatus(tr("Get project information"));
    setProgress(3, 4);
    auto responseInfo = std::make_shared<QByteArray>();
    auto projectTask = m_api->getProject(m_version.addonId.toString(), responseInfo);
    connect(projectTask.get(), &Task::succeeded, [responseInfo, this] {
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(*responseInfo, &parse_error);
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Error while parsing JSON response for mod info at " << parse_error.offset
                       << " reason: " << parse_error.errorString();
            qDebug() << *responseInfo;
            emitFailed(tr("Error parsing project info"));
            return;
        }
        try {
            switch (m_provider) {
                case ModPlatform::ResourceProvider::MODRINTH: {
                    auto obj = Json::requireObject(doc);
                    Modrinth::loadIndexedPack(m_pack, obj);
                    break;
                }
                case ModPlatform::ResourceProvider::FLAME: {
                    auto obj = Json::requireObject(Json::requireObject(doc), "data");
                    FlameMod::loadIndexedPack(m_pack, obj);
                    break;
                }
            }
        } catch (const JSONValidationError& e) {
            qDebug() << doc;
            qWarning() << "Error while reading mod info: " << e.cause();
            emitFailed(tr("Error parsing project info"));
            return;
        }
        getLogo();
    });

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(projectTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(projectTask.get(), &Task::aborted, this, &GetModPackExtraInfoTask::emitAborted);
    connect(projectTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(projectTask.get(), &Task::stepProgress, this, &GetModPackExtraInfoTask::propagateStepProgress);

    connect(projectTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(projectTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_current_task.reset(projectTask);
    projectTask->start();
}

void GetModPackExtraInfoTask::getLogo()
{
    m_logo_name = m_provider == ModPlatform::ResourceProvider::MODRINTH ? ("modrinth_" + m_pack.slug)
                                                                        : ("curseforge_" + m_pack.logoName.section(".", 0, 0));
    QString providerName = m_provider == ModPlatform::ResourceProvider::MODRINTH ? "Modrinth" : "Flame";

    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry(providerName, QString("logos/%1").arg(m_logo_name.section(".", 0, 0)));
    auto logoTask = makeShared<NetJob>(QString("%1 Icon Download %2").arg(providerName, m_logo_name), APPLICATION->network());
    logoTask->addNetAction(Net::Download::makeCached(QUrl(m_pack.logoUrl), entry));

    m_logo_full_path = entry->getFullPath();
    QObject::connect(logoTask.get(), &NetJob::succeeded, this, [this] {
        APPLICATION->icons()->installIcon(m_logo_full_path, m_logo_name);
        emitSucceeded();
    });

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(logoTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(logoTask.get(), &Task::aborted, this, &GetModPackExtraInfoTask::emitAborted);
    connect(logoTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(logoTask.get(), &Task::stepProgress, this, &GetModPackExtraInfoTask::propagateStepProgress);

    connect(logoTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(logoTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_current_task.reset(logoTask);

    logoTask->start();
}