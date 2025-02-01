// SPDX-FileCopyrightText: 2022 Sefa Eyeoglu <contact@scrumplex.net>
// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
// SPDX-FileCopyrightText: 2022 kumquat-ir <66188216+kumquat-ir@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *  Copyright (C) 2022 kumquat-ir <66188216+kumquat-ir@users.noreply.github.com>
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

#include <QDialog>
#include <QList>
#include <QString>

#include <QFileSystemWatcher>

#include "tasks/ConcurrentTask.h"

class QPushButton;

struct BlockedMod {
    QString name;
    QString websiteUrl;
    QString hash;
    bool matched;
    QString localPath;
    QString targetFolder;
    bool move = false;
};

QT_BEGIN_NAMESPACE
namespace Ui {
class BlockedModsDialog;
}
QT_END_NAMESPACE

class BlockedModsDialog : public QDialog {
    Q_OBJECT

   public:
    BlockedModsDialog(QWidget* parent, const QString& title, const QString& text, QList<BlockedMod>& mods, QString hash_type = "sha1");

    ~BlockedModsDialog() override;

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

   protected slots:
    void done(int r) override;

   private:
    Ui::BlockedModsDialog* ui;
    QList<BlockedMod>& m_mods;
    QFileSystemWatcher m_watcher;
    shared_qobject_ptr<ConcurrentTask> m_hashingTask;
    QSet<QString> m_pendingHashPaths;
    bool m_rehashPending;
    QString m_hashType;

    void openAll(bool missingOnly);
    void addDownloadFolder();
    void update();
    void directoryChanged(QString path);
    void setupWatch();
    void watchPath(QString path, bool watch_recursive = false);
    void scanPaths();
    void scanPath(QString path, bool start_task);
    void addHashTask(QString path);
    void buildHashTask(QString path);
    void checkMatchHash(QString hash, QString path);
    void validateMatchedMods();
    void runHashTask();
    void hashTaskFinished();

    bool checkValidPath(QString path);
    bool allModsMatched();
};

QDebug operator<<(QDebug debug, const BlockedMod& m);
