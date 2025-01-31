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
 *      Copyright 2021 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include <QAbstractListModel>
#include <QDialog>

#include "modplatform/atlauncher/ATLPackManifest.h"
#include "net/NetJob.h"

namespace Ui {
class AtlOptionalModDialog;
}

class AtlOptionalModListModel : public QAbstractListModel {
    Q_OBJECT

   public:
    enum Columns {
        EnabledColumn = 0,
        NameColumn,
        DescriptionColumn,
    };

    AtlOptionalModListModel(QWidget* parent, const ATLauncher::PackVersion& version, QVector<ATLauncher::VersionMod> mods);

    QVector<QString> getResult();

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void useShareCode(const QString& code);

   public slots:
    void shareCodeSuccess();
    void shareCodeFailure(const QString& reason);

    void selectRecommended();
    void clearAll();

   private:
    void toggleMod(const ATLauncher::VersionMod& mod, int index);
    void setMod(const ATLauncher::VersionMod& mod, int index, bool enable, bool shouldEmit = true);

   private:
    NetJob::Ptr m_jobPtr;
    std::shared_ptr<QByteArray> m_response = std::make_shared<QByteArray>();

    ATLauncher::PackVersion m_version;
    QVector<ATLauncher::VersionMod> m_mods;

    QMap<QString, bool> m_selection;
    QMap<QString, int> m_index;
    QMap<QString, QVector<QString>> m_dependents;
};

class AtlOptionalModDialog : public QDialog {
    Q_OBJECT

   public:
    AtlOptionalModDialog(QWidget* parent, const ATLauncher::PackVersion& version, QVector<ATLauncher::VersionMod> mods);
    ~AtlOptionalModDialog() override;

    QVector<QString> getResult() { return listModel->getResult(); }

    void useShareCode();

   private:
    Ui::AtlOptionalModDialog* ui;

    AtlOptionalModListModel* listModel;
};
