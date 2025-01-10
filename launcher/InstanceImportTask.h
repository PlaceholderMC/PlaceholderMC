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

#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QUrl>
#include "InstanceTask.h"

class QuaZip;

class InstanceImportTask : public InstanceTask {
    Q_OBJECT
   public:
    explicit InstanceImportTask(const QUrl& sourceUrl, QWidget* parent = nullptr, QMap<QString, QString>&& extra_info = {});
    virtual ~InstanceImportTask() = default;
    bool abort() override;

   protected:
    //! Entry point for tasks.
    virtual void executeTask() override;

   private:
    void processMultiMC();
    void processTechnic();
    void processFlame();
    void processModrinth();
    QString getRootFromZip(QuaZip* zip, const QString& root = "");

   private slots:
    void processZipPack();
    void processExtraInfoPack();
    void extractFinished();

   private: /* data */
    QUrl m_sourceUrl;
    QString m_archivePath;
    Task::Ptr m_task;
    enum class ModpackType {
        Unknown,
        MultiMC,
        Technic,
        Flame,
        Modrinth,
    } m_modpackType = ModpackType::Unknown;

    // Extra info we might need, that's available before, but can't be derived from
    // the source URL / the resource it points to alone.
    QMap<QString, QString> m_extra_info;

    // FIXME: nuke
    QWidget* m_parent;
    void downloadFromUrl();
};
