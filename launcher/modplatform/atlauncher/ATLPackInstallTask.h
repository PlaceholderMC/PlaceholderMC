// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *      Copyright 2020-2021 Jamie Mansfield <jmansfield@cadixdev.org>
 *      Copyright 2021 Petr Mrazek <peterix@gmail.com>
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

#pragma once

#include <meta/VersionList.h>
#include "ATLPackManifest.h"

#include "InstanceTask.h"
#include "meta/Version.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/NetJob.h"
#include "settings/INISettingsObject.h"

#include <memory>
#include <optional>

namespace ATLauncher {

enum class InstallMode {
    Install,
    Reinstall,
    Update,
};

class UserInteractionSupport {
   public:
    /**
     * Requests a user interaction to select which optional mods should be installed.
     */
    virtual std::optional<QVector<QString>> chooseOptionalMods(const PackVersion& version, QVector<ATLauncher::VersionMod> mods) = 0;

    /**
     * Requests a user interaction to select a component version from a given version list
     * and constrained to a given Minecraft version.
     */
    virtual QString chooseVersion(Meta::VersionList::Ptr vlist, QString minecraftVersion) = 0;

    /**
     * Requests a user interaction to display a message.
     */
    virtual void displayMessage(QString message) = 0;

    virtual ~UserInteractionSupport() = default;
};

class PackInstallTask : public InstanceTask {
    Q_OBJECT

   public:
    explicit PackInstallTask(UserInteractionSupport* support,
                             QString packName,
                             QString version,
                             InstallMode installMode = InstallMode::Install);
    virtual ~PackInstallTask() { delete m_support; }

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    virtual void executeTask() override;

   private slots:
    void onDownloadSucceeded();
    void onDownloadFailed(QString reason);
    void onDownloadAborted();

    void onModsDownloaded();
    void onModsExtracted();

   private:
    QString getDirForModType(ModType type, QString raw);
    QString getVersionForLoader(QString uid);
    QString detectLibrary(VersionLibrary library);

    bool createLibrariesComponent(QString instanceRoot, std::shared_ptr<PackProfile> profile);
    bool createPackComponent(QString instanceRoot, std::shared_ptr<PackProfile> profile);

    void deleteExistingFiles();
    void installConfigs();
    void extractConfigs();
    void downloadMods();
    bool extractMods(const QMap<QString, VersionMod>& toExtract,
                     const QMap<QString, VersionMod>& toDecomp,
                     const QMap<QString, QString>& toCopy);
    void install();

   private:
    UserInteractionSupport* m_support;

    bool abortable = false;

    NetJob::Ptr jobPtr;
    std::shared_ptr<QByteArray> response = std::make_shared<QByteArray>();

    InstallMode m_install_mode;
    QString m_pack_name;
    QString m_pack_safe_name;
    QString m_version_name;
    PackVersion m_version;

    QMap<QString, VersionMod> modsToExtract;
    QMap<QString, VersionMod> modsToDecomp;
    QMap<QString, QString> modsToCopy;

    QString archivePath;
    QStringList jarmods;
    Meta::Version::Ptr minecraftVersion;
    QMap<QString, Meta::Version::Ptr> componentsToInstall;

    QFuture<std::optional<QStringList>> m_extractFuture;
    QFutureWatcher<std::optional<QStringList>> m_extractFutureWatcher;

    QFuture<bool> m_modExtractFuture;
    QFutureWatcher<bool> m_modExtractFutureWatcher;
};

}  // namespace ATLauncher
