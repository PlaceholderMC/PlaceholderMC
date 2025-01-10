// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "InstanceImportTask.h"

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "NullInstance.h"

#include "QObjectPtr.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameInstanceCreationTask.h"
#include "modplatform/helpers/GetModPackExtraInfoTask.h"
#include "modplatform/modrinth/ModrinthInstanceCreationTask.h"
#include "modplatform/technic/TechnicPackProcessor.h"

#include "settings/INISettingsObject.h"
#include "tasks/Task.h"

#include "net/ApiDownload.h"
#include "tasks/MultipleOptionsTask.h"

#include <QtConcurrentRun>
#include <algorithm>
#include <memory>

#include <quazip/quazipdir.h>

InstanceImportTask::InstanceImportTask(const QUrl& sourceUrl, QWidget* parent, QMap<QString, QString>&& extra_info)
    : m_sourceUrl(sourceUrl), m_extra_info(extra_info), m_parent(parent)
{}

bool InstanceImportTask::abort()
{
    if (!canAbort())
        return false;

    bool wasAborted = false;
    if (m_task)
        wasAborted = m_task->abort();
    Task::abort();
    return wasAborted;
}

void InstanceImportTask::executeTask()
{
    setAbortable(true);

    if (m_sourceUrl.isLocalFile()) {
        m_archivePath = m_sourceUrl.toLocalFile();
        processExtraInfoPack();
    } else {
        setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));

        downloadFromUrl();
    }
}

void InstanceImportTask::downloadFromUrl()
{
    const QString path(m_sourceUrl.host() + '/' + m_sourceUrl.path());

    auto entry = APPLICATION->metacache()->resolveEntry("general", path);
    entry->setStale(true);
    m_archivePath = entry->getFullPath();

    auto filesNetJob = makeShared<NetJob>(tr("Modpack download"), APPLICATION->network());
    filesNetJob->addNetAction(Net::ApiDownload::makeCached(m_sourceUrl, entry));

    connect(filesNetJob.get(), &NetJob::succeeded, this, &InstanceImportTask::processZipPack);
    connect(filesNetJob.get(), &NetJob::progress, this, &InstanceImportTask::setProgress);
    connect(filesNetJob.get(), &NetJob::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(filesNetJob.get(), &NetJob::failed, this, &InstanceImportTask::emitFailed);
    connect(filesNetJob.get(), &NetJob::aborted, this, &InstanceImportTask::emitAborted);
    m_task.reset(filesNetJob);
    filesNetJob->start();
}

QString InstanceImportTask::getRootFromZip(QuaZip* zip, const QString& root)
{
    if (!isRunning()) {
        return {};
    }
    QuaZipDir rootDir(zip, root);
    for (auto&& fileName : rootDir.entryList(QDir::Files)) {
        setDetails(fileName);
        if (fileName == "instance.cfg") {
            qDebug() << "MultiMC:" << true;
            m_modpackType = ModpackType::MultiMC;
            return root;
        }
        if (fileName == "manifest.json") {
            qDebug() << "Flame:" << true;
            m_modpackType = ModpackType::Flame;
            return root;
        }

        QCoreApplication::processEvents();
    }

    // Recurse the search to non-ignored subfolders
    for (auto&& fileName : rootDir.entryList(QDir::Dirs)) {
        if ("overrides/" == fileName)
            continue;

        QString result = getRootFromZip(zip, root + fileName);
        if (!result.isEmpty())
            return result;
    }

    return {};
}

void InstanceImportTask::processZipPack()
{
    setStatus(tr("Attempting to determine instance type"));
    QDir extractDir(m_stagingPath);
    qDebug() << "Attempting to create instance from" << m_archivePath;

    // open the zip and find relevant files in it
    auto packZip = std::make_shared<QuaZip>(m_archivePath);
    if (!packZip->open(QuaZip::mdUnzip)) {
        emitFailed(tr("Unable to open supplied modpack zip file."));
        return;
    }

    QuaZipDir packZipDir(packZip.get());
    qDebug() << "Attempting to determine instance type";

    QString root;

    // NOTE: Prioritize modpack platforms that aren't searched for recursively.
    // Especially Flame has a very common filename for its manifest, which may appear inside overrides for example
    // https://docs.modrinth.com/docs/modpacks/format_definition/#storage
    if (packZipDir.exists("/modrinth.index.json")) {
        // process as Modrinth pack
        qDebug() << "Modrinth:" << true;
        m_modpackType = ModpackType::Modrinth;
    } else if (packZipDir.exists("/bin/modpack.jar") || packZipDir.exists("/bin/version.json")) {
        // process as Technic pack
        qDebug() << "Technic:" << true;
        extractDir.mkpath("minecraft");
        extractDir.cd("minecraft");
        m_modpackType = ModpackType::Technic;
    } else {
        root = getRootFromZip(packZip.get());
        setDetails("");
    }
    if (m_modpackType == ModpackType::Unknown) {
        emitFailed(tr("Archive does not contain a recognized modpack type."));
        return;
    }
    setStatus(tr("Extracting modpack"));

    // make sure we extract just the pack
    auto zipTask = makeShared<MMCZip::ExtractZipTask>(packZip, extractDir, root);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(zipTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(zipTask.get(), &Task::succeeded, this, &InstanceImportTask::extractFinished, Qt::QueuedConnection);
    connect(zipTask.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(zipTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(zipTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);

    connect(zipTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(zipTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_task.reset(zipTask);
    zipTask->start();
}

void InstanceImportTask::extractFinished()
{
    setAbortable(false);
    QDir extractDir(m_stagingPath);

    qDebug() << "Fixing permissions for extracted pack files...";
    QDirIterator it(extractDir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto filepath = it.next();
        QFileInfo file(filepath);
        auto permissions = QFile::permissions(filepath);
        auto origPermissions = permissions;
        if (file.isDir()) {
            // Folder +rwx for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser | QFileDevice::Permission::ExeUser;
        } else {
            // File +rw for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser;
        }
        if (origPermissions != permissions) {
            if (!QFile::setPermissions(filepath, permissions)) {
                logWarning(tr("Could not fix permissions for %1").arg(filepath));
            } else {
                qDebug() << "Fixed" << filepath;
            }
        }
    }

    switch (m_modpackType) {
        case ModpackType::MultiMC:
            processMultiMC();
            return;
        case ModpackType::Technic:
            processTechnic();
            return;
        case ModpackType::Flame:
            processFlame();
            return;
        case ModpackType::Modrinth:
            processModrinth();
            return;
        case ModpackType::Unknown:
            emitFailed(tr("Archive does not contain a recognized modpack type."));
            return;
    }
}

void InstanceImportTask::processFlame()
{
    shared_qobject_ptr<FlameCreationTask> inst_creation_task = nullptr;
    if (!m_extra_info.isEmpty()) {
        auto pack_id_it = m_extra_info.constFind("pack_id");
        Q_ASSERT(pack_id_it != m_extra_info.constEnd());
        auto pack_id = pack_id_it.value();

        auto pack_version_id_it = m_extra_info.constFind("pack_version_id");
        Q_ASSERT(pack_version_id_it != m_extra_info.constEnd());
        auto pack_version_id = pack_version_id_it.value();

        QString original_instance_id;
        auto original_instance_id_it = m_extra_info.constFind("original_instance_id");
        if (original_instance_id_it != m_extra_info.constEnd())
            original_instance_id = original_instance_id_it.value();

        inst_creation_task =
            makeShared<FlameCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id, pack_version_id, original_instance_id);
    } else {
        // FIXME: Find a way to get IDs in directly imported ZIPs
        inst_creation_task = makeShared<FlameCreationTask>(m_stagingPath, m_globalSettings, m_parent, QString(), QString());
    }

    inst_creation_task->setName(*this);
    inst_creation_task->setIcon(m_instIcon);
    inst_creation_task->setGroup(m_instGroup);
    inst_creation_task->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = inst_creation_task.toWeakRef();
    connect(inst_creation_task.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
        }
        emitSucceeded();
    });
    connect(inst_creation_task.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(inst_creation_task.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(inst_creation_task.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(inst_creation_task.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(inst_creation_task.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(inst_creation_task.get(), &Task::aborted, this, &Task::abort);
    connect(inst_creation_task.get(), &Task::abortStatusChanged, this, &Task::setAbortable);

    m_task.reset(inst_creation_task);
    setAbortable(true);
    m_task->start();
}

void InstanceImportTask::processTechnic()
{
    shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor{ new Technic::TechnicPackProcessor };
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded, this, &InstanceImportTask::emitSucceeded);
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this, &InstanceImportTask::emitFailed);
    packProcessor->run(m_globalSettings, name(), m_instIcon, m_stagingPath);
}

void InstanceImportTask::processMultiMC()
{
    QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
    auto instanceSettings = std::make_shared<INISettingsObject>(configPath);

    NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);

    // reset time played on import... because packs.
    instance.resetTimePlayed();

    // set a new nice name
    instance.setName(name());

    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon != "default") {
        instance.setIconKey(m_instIcon);
    } else {
        m_instIcon = instance.iconKey();

        auto importIconPath = IconUtils::findBestIconIn(instance.instanceRoot(), m_instIcon);
        if (importIconPath.isNull() || !QFile::exists(importIconPath))
            importIconPath = IconUtils::findBestIconIn(instance.instanceRoot(), "icon.png");
        if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
            // import icon
            auto iconList = APPLICATION->icons();
            if (iconList->iconFileExists(m_instIcon)) {
                iconList->deleteIcon(m_instIcon);
            }
            iconList->installIcon(importIconPath, m_instIcon);
        }
    }
    emitSucceeded();
}

void InstanceImportTask::processModrinth()
{
    shared_qobject_ptr<ModrinthCreationTask> inst_creation_task = nullptr;
    if (!m_extra_info.isEmpty()) {
        auto pack_id_it = m_extra_info.constFind("pack_id");
        Q_ASSERT(pack_id_it != m_extra_info.constEnd());
        auto pack_id = pack_id_it.value();

        QString pack_version_id;
        auto pack_version_id_it = m_extra_info.constFind("pack_version_id");
        if (pack_version_id_it != m_extra_info.constEnd())
            pack_version_id = pack_version_id_it.value();

        QString original_instance_id;
        auto original_instance_id_it = m_extra_info.constFind("original_instance_id");
        if (original_instance_id_it != m_extra_info.constEnd())
            original_instance_id = original_instance_id_it.value();

        inst_creation_task =
            makeShared<ModrinthCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id, pack_version_id, original_instance_id);
    } else {
        QString pack_id;
        if (!m_sourceUrl.isEmpty()) {
            QRegularExpression regex(R"(data\/([^\/]*)\/versions)");
            pack_id = regex.match(m_sourceUrl.toString()).captured(1);
        }

        // FIXME: Find a way to get the ID in directly imported ZIPs
        inst_creation_task = makeShared<ModrinthCreationTask>(m_stagingPath, m_globalSettings, m_parent, pack_id);
    }

    inst_creation_task->setName(*this);
    inst_creation_task->setIcon(m_instIcon);
    inst_creation_task->setGroup(m_instGroup);
    inst_creation_task->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = inst_creation_task.toWeakRef();
    connect(inst_creation_task.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
        }
        emitSucceeded();
    });
    connect(inst_creation_task.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(inst_creation_task.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(inst_creation_task.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(inst_creation_task.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(inst_creation_task.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(inst_creation_task.get(), &Task::aborted, this, &Task::abort);
    connect(inst_creation_task.get(), &Task::abortStatusChanged, this, &Task::setAbortable);

    m_task.reset(inst_creation_task);
    setAbortable(true);
    m_task->start();
}

void InstanceImportTask::processExtraInfoPack()
{
    if (!m_extra_info.isEmpty()) {
        processZipPack();
        return;
    }
    auto populateExtraInfo = [this](GetModPackExtraInfoTask* task) {
        m_extra_info.insert("pack_id", task->getVersion().addonId.toString());
        m_extra_info.insert("pack_version_id", task->getVersion().version);
        setIcon(task->getLogoName());
    };
    auto infoTask = makeShared<MultipleOptionsTask>(this, "Get Extra info");
    auto flameTask = makeShared<GetModPackExtraInfoTask>(m_archivePath, ModPlatform::ResourceProvider::FLAME);
    auto modrinthTask = makeShared<GetModPackExtraInfoTask>(m_archivePath, ModPlatform::ResourceProvider::MODRINTH);
    infoTask->addTask(flameTask);
    infoTask->addTask(modrinthTask);
    connect(flameTask.get(), &Task::succeeded, [populateExtraInfo, flameTask] { populateExtraInfo(flameTask.get()); });
    connect(modrinthTask.get(), &Task::succeeded, [populateExtraInfo, modrinthTask] { populateExtraInfo(modrinthTask.get()); });
    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(infoTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
        processZipPack();
    });

    connect(infoTask.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(infoTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
    });
    connect(infoTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);

    connect(infoTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(infoTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    task.reset(infoTask);
    infoTask->start();
}
