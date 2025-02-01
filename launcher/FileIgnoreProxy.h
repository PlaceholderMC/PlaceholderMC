// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include <QFileInfo>
#include <QSortFilterProxyModel>
#include "SeparatorPrefixTree.h"

class FileIgnoreProxy : public QSortFilterProxyModel {
    Q_OBJECT

   public:
    FileIgnoreProxy(QString root, QObject* parent);
    // NOTE: Sadly, we have to do sorting ourselves.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

    QString relPath(const QString& path) const;

    bool setFilterState(QModelIndex index, Qt::CheckState state);

    bool shouldExpand(QModelIndex index);

    void setBlockedPaths(QStringList paths);

    inline const SeparatorPrefixTree<'/'>& blockedPaths() const { return m_blocked; }
    inline SeparatorPrefixTree<'/'>& blockedPaths() { return m_blocked; }

    // list of file names that need to be removed completely from model
    inline QStringList& ignoreFilesWithName() { return m_ignoreFiles; }
    // list of relative paths that need to be removed completely from model
    inline SeparatorPrefixTree<'/'>& ignoreFilesWithPath() { return m_ignoreFilePaths; }

    bool filterFile(const QString& fileName) const;

    void loadBlockedPathsFromFile(const QString& fileName);

    void saveBlockedPathsToFile(const QString& fileName);

   protected:
    bool filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const;
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;

    bool ignoreFile(QFileInfo file) const;

   private:
    const QString m_root;
    SeparatorPrefixTree<'/'> m_blocked;
    QStringList m_ignoreFiles;
    SeparatorPrefixTree<'/'> m_ignoreFilePaths;
};
