// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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
 */

#pragma once

#include <QDir>

#include "modplatform/ModIndex.h"
#include "tasks/Task.h"

class LocalResourceUpdateTask : public Task {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<LocalResourceUpdateTask>;

    explicit LocalResourceUpdateTask(QDir index_dir,
                                     ModPlatform::IndexedPack& project,
                                     ModPlatform::IndexedVersion& version,
                                     bool lockUpdate = false);

    auto canAbort() const -> bool override { return true; }
    auto abort() -> bool override;

   protected slots:
    //! Entry point for tasks.
    void executeTask() override;

   signals:
    void hasOldResource(QString name, QString filename);

   private:
    QDir m_index_dir;
    ModPlatform::IndexedPack m_project;
    ModPlatform::IndexedVersion m_version;
    bool m_lockUpdate = false;
};
