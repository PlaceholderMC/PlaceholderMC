// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "PackInstallTask.h"

#include <QtConcurrent>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "minecraft/GradleSpecifier.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

#include "Application.h"
#include "BuildConfig.h"

#include "net/ApiDownload.h"

namespace LegacyFTB {

PackInstallTask::PackInstallTask(shared_qobject_ptr<QNetworkAccessManager> network, const Modpack& pack, QString version)
{
    m_pack = pack;
    m_version = version;
    m_network = network;
}

void PackInstallTask::executeTask()
{
    downloadPack();
}

void PackInstallTask::downloadPack()
{
    setStatus(tr("Downloading zip for %1").arg(m_pack.name));
    setProgress(1, 4);
    setAbortable(false);

    auto path = QString("%1/%2/%3").arg(m_pack.dir, m_version.replace(".", "_"), m_pack.file);
    auto entry = APPLICATION->metacache()->resolveEntry("FTBPacks", path);
    entry->setStale(true);
    archivePath = entry->getFullPath();
    netJobContainer.reset(new NetJob("Download FTB Pack", m_network));
    QString url;
    if (m_pack.type == PackType::Private) {
        url = QString(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "privatepacks/%1").arg(path);
    } else {
        url = QString(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "modpacks/%1").arg(path);
    }
    netJobContainer->addNetAction(Net::ApiDownload::makeCached(url, entry));

    connect(netJobContainer.get(), &NetJob::succeeded, this, &PackInstallTask::unzip);
    connect(netJobContainer.get(), &NetJob::failed, this, &PackInstallTask::emitFailed);
    connect(netJobContainer.get(), &NetJob::stepProgress, this, &PackInstallTask::propagateStepProgress);
    connect(netJobContainer.get(), &NetJob::aborted, this, &PackInstallTask::emitAborted);

    netJobContainer->start();

    setAbortable(true);
    progress(1, 4);
}

void PackInstallTask::unzip()
{
    setStatus(tr("Extracting modpack"));
    setAbortable(false);
    progress(2, 4);

    QDir extractDir(m_stagingPath);

    m_packZip.reset(new QuaZip(archivePath));
    if (!m_packZip->open(QuaZip::mdUnzip)) {
        emitFailed(tr("Failed to open modpack file %1!").arg(archivePath));
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_extractFuture = QtConcurrent::run(QThreadPool::globalInstance(), QOverload<QString, QString>::of(MMCZip::extractDir), archivePath,
                                        extractDir.absolutePath() + "/unzip");
#else
    m_extractFuture =
        QtConcurrent::run(QThreadPool::globalInstance(), MMCZip::extractDir, archivePath, extractDir.absolutePath() + "/unzip");
#endif
    connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::finished, this, &PackInstallTask::onUnzipFinished);
    connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::canceled, this, &PackInstallTask::onUnzipCanceled);
    m_extractFutureWatcher.setFuture(m_extractFuture);
}

void PackInstallTask::onUnzipFinished()
{
    install();
}

void PackInstallTask::onUnzipCanceled()
{
    emitAborted();
}

void PackInstallTask::install()
{
    setStatus(tr("Installing modpack"));
    progress(3, 4);
    QDir unzipMcDir(m_stagingPath + "/unzip/minecraft");
    if (unzipMcDir.exists()) {
        // ok, found minecraft dir, move contents to instance dir
        if (!FS::move(m_stagingPath + "/unzip/minecraft", m_stagingPath + "/minecraft")) {
            emitFailed(tr("Failed to move unpacked Minecraft!"));
            return;
        }
    }

    QString instanceConfigPath = FS::PathCombine(m_stagingPath, "instance.cfg");
    auto instanceSettings = std::make_shared<INISettingsObject>(instanceConfigPath);
    instanceSettings->suspendSave();

    MinecraftInstance instance(m_globalSettings, instanceSettings, m_stagingPath);
    auto components = instance.getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion("net.minecraft", m_pack.mcVersion, true);

    bool fallback = true;

    // handle different versions
    QFile packJson(m_stagingPath + "/minecraft/pack.json");
    QDir jarmodDir = QDir(m_stagingPath + "/unzip/instMods");
    if (packJson.exists()) {
        packJson.open(QIODevice::ReadOnly | QIODevice::Text);
        QJsonDocument doc = QJsonDocument::fromJson(packJson.readAll());
        packJson.close();

        // we only care about the libs
        QJsonArray libs = doc.object().value("libraries").toArray();

        foreach (const QJsonValue& value, libs) {
            QString nameValue = value.toObject().value("name").toString();
            if (!nameValue.startsWith("net.minecraftforge")) {
                continue;
            }

            GradleSpecifier forgeVersion(nameValue);

            components->setComponentVersion("net.minecraftforge", forgeVersion.version().replace(m_pack.mcVersion, "").replace("-", ""));
            packJson.remove();
            fallback = false;
            break;
        }
    }

    if (jarmodDir.exists()) {
        qDebug() << "Found jarmods, installing...";

        QStringList jarmods;
        for (auto info : jarmodDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
            qDebug() << "Jarmod:" << info.fileName();
            jarmods.push_back(info.absoluteFilePath());
        }

        components->installJarMods(jarmods);
        fallback = false;
    }

    // just nuke unzip directory, it s not needed anymore
    FS::deletePath(m_stagingPath + "/unzip");

    if (fallback) {
        // TODO: Some fallback mechanism... or just keep failing!
        emitFailed(tr("No installation method found!"));
        return;
    }

    components->saveNow();

    progress(4, 4);

    instance.setName(name());
    if (m_instIcon == "default") {
        m_instIcon = "ftb_logo";
    }
    instance.setIconKey(m_instIcon);
    instanceSettings->resumeSave();

    emitSucceeded();
}

bool PackInstallTask::abort()
{
    if (!canAbort()) {
        return false;
    }

    netJobContainer->abort();
    return InstanceTask::abort();
}

}  // namespace LegacyFTB
