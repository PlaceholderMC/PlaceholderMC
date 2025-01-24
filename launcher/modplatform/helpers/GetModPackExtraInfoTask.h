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
#pragma once

#include <memory>
#include "modplatform/ModIndex.h"
#include "modplatform/helpers/NetworkResourceAPI.h"
#include "tasks/Task.h"

class GetModPackExtraInfoTask : public Task {
    Q_OBJECT
   public:
    GetModPackExtraInfoTask(QString path, ModPlatform::ResourceProvider provider);
    virtual ~GetModPackExtraInfoTask() = default;

    bool canAbort() const override { return true; }

    ModPlatform::IndexedVersion getVersion() { return m_version; };
    ModPlatform::IndexedPack getPack() { return m_pack; };
    QString getLogoName() { return m_logo_name.isEmpty() ? "default" : m_logo_name; };
    QString getLogoFullPath() { return m_logo_full_path; };

   public slots:
    bool abort() override;

   protected slots:
    void executeTask() override;

   private slots:
    void hashDone(QString result);
    void getProjectInfo();
    void getLogo();

   private:
    QString m_path;
    Task::Ptr m_current_task;
    ModPlatform::ResourceProvider m_provider;
    std::unique_ptr<NetworkResourceAPI> m_api;

    ModPlatform::IndexedVersion m_version;
    ModPlatform::IndexedPack m_pack;
    QString m_logo_name;
    QString m_logo_full_path;
};