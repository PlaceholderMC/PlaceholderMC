/// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PrismLaucher - Minecraft Launcher
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include <QDialog>
#include <QHash>
#include <QUuid>
#include <memory>

#include "QObjectPtr.h"
#include "tasks/Task.h"

#include "ui/widgets/SubTaskProgressBar.h"

class Task;
class SequentialTask;

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog {
    Q_OBJECT

   public:
    explicit ProgressDialog(QWidget* parent = 0);
    ~ProgressDialog();

    void updateSize(bool recenterParent = false);

    int execWithTask(Task* task);
    int execWithTask(std::unique_ptr<Task>&& task);
    int execWithTask(std::unique_ptr<Task>& task);

    void setSkipButton(bool present, QString label = QString());

    Task* getTask();

   public slots:
    void onTaskStarted();
    void onTaskFailed(QString failure);
    void onTaskSucceeded();

    void changeStatus(const QString& status);
    void changeProgress(qint64 current, qint64 total);
    void changeStepProgress(TaskStepProgress const& task_progress);

   private slots:
    void on_skipButton_clicked(bool checked);

   protected:
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void closeEvent(QCloseEvent* e);

   private:
    bool handleImmediateResult(QDialog::DialogCode& result);
    void addTaskProgress(TaskStepProgress const& progress);

   private:
    Ui::ProgressDialog* ui;

    Task* m_task;

    QList<QMetaObject::Connection> m_taskConnections;

    bool m_is_multi_step = false;
    QHash<QUuid, SubTaskProgressBar*> taskProgress;
};
