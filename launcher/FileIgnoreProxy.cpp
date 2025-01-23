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

#include "FileIgnoreProxy.h"

#include <QDebug>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QStack>
#include "FileSystem.h"
#include "SeparatorPrefixTree.h"
#include "StringUtils.h"

FileIgnoreProxy::FileIgnoreProxy(QString root, QObject* parent) : QSortFilterProxyModel(parent), m_root(root) {}
// NOTE: Sadly, we have to do sorting ourselves.
bool FileIgnoreProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    QFileSystemModel* fsm = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsm) {
        return QSortFilterProxyModel::lessThan(left, right);
    }
    bool asc = sortOrder() == Qt::AscendingOrder ? true : false;

    QFileInfo leftFileInfo = fsm->fileInfo(left);
    QFileInfo rightFileInfo = fsm->fileInfo(right);

    if (!leftFileInfo.isDir() && rightFileInfo.isDir()) {
        return !asc;
    }
    if (leftFileInfo.isDir() && !rightFileInfo.isDir()) {
        return asc;
    }

    // sort and proxy model breaks the original model...
    if (sortColumn() == 0) {
        return StringUtils::naturalCompare(leftFileInfo.fileName(), rightFileInfo.fileName(), Qt::CaseInsensitive) < 0;
    }
    if (sortColumn() == 1) {
        auto leftSize = leftFileInfo.size();
        auto rightSize = rightFileInfo.size();
        if ((leftSize == rightSize) || (leftFileInfo.isDir() && rightFileInfo.isDir())) {
            return StringUtils::naturalCompare(leftFileInfo.fileName(), rightFileInfo.fileName(), Qt::CaseInsensitive) < 0 ? asc : !asc;
        }
        return leftSize < rightSize;
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

Qt::ItemFlags FileIgnoreProxy::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    auto sourceIndex = mapToSource(index);
    Qt::ItemFlags flags = sourceIndex.flags();
    if (index.column() == 0) {
        flags |= Qt::ItemIsUserCheckable;
        if (sourceIndex.model()->hasChildren(sourceIndex)) {
            flags |= Qt::ItemIsAutoTristate;
        }
    }

    return flags;
}

QVariant FileIgnoreProxy::data(const QModelIndex& index, int role) const
{
    QModelIndex sourceIndex = mapToSource(index);

    if (index.column() == 0 && role == Qt::CheckStateRole) {
        QFileSystemModel* fsm = qobject_cast<QFileSystemModel*>(sourceModel());
        auto blockedPath = relPath(fsm->filePath(sourceIndex));
        auto cover = m_blocked.cover(blockedPath);
        if (!cover.isNull()) {
            return QVariant(Qt::Unchecked);
        } else if (m_blocked.exists(blockedPath)) {
            return QVariant(Qt::PartiallyChecked);
        } else {
            return QVariant(Qt::Checked);
        }
    }

    return sourceIndex.data(role);
}

bool FileIgnoreProxy::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.column() == 0 && role == Qt::CheckStateRole) {
        Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
        return setFilterState(index, state);
    }

    QModelIndex sourceIndex = mapToSource(index);
    return QSortFilterProxyModel::sourceModel()->setData(sourceIndex, value, role);
}

QString FileIgnoreProxy::relPath(const QString& path) const
{
    return QDir(m_root).relativeFilePath(path);
}

bool FileIgnoreProxy::setFilterState(QModelIndex index, Qt::CheckState state)
{
    QFileSystemModel* fsm = qobject_cast<QFileSystemModel*>(sourceModel());

    if (!fsm) {
        return false;
    }

    QModelIndex sourceIndex = mapToSource(index);
    auto blockedPath = relPath(fsm->filePath(sourceIndex));
    bool changed = false;
    if (state == Qt::Unchecked) {
        // blocking a path
        auto& node = m_blocked.insert(blockedPath);
        // get rid of all blocked nodes below
        node.clear();
        changed = true;
    } else if (state == Qt::Checked || state == Qt::PartiallyChecked) {
        if (!m_blocked.remove(blockedPath)) {
            auto cover = m_blocked.cover(blockedPath);
            qDebug() << "Blocked by cover" << cover;
            // uncover
            m_blocked.remove(cover);
            // block all contents, except for any cover
            QModelIndex rootIndex = fsm->index(FS::PathCombine(m_root, cover));
            QModelIndex doing = rootIndex;
            int row = 0;
            QStack<QModelIndex> todo;
            while (1) {
                auto node = fsm->index(row, 0, doing);
                if (!node.isValid()) {
                    if (!todo.size()) {
                        break;
                    } else {
                        doing = todo.pop();
                        row = 0;
                        continue;
                    }
                }
                auto relpath = relPath(fsm->filePath(node));
                if (blockedPath.startsWith(relpath))  // cover found?
                {
                    // continue processing cover later
                    todo.push(node);
                } else {
                    // or just block this one.
                    m_blocked.insert(relpath);
                }
                row++;
            }
        }
        changed = true;
    }
    if (changed) {
        // update the thing
        emit dataChanged(index, index, { Qt::CheckStateRole });
        // update everything above index
        QModelIndex up = index.parent();
        while (1) {
            if (!up.isValid())
                break;
            emit dataChanged(up, up, { Qt::CheckStateRole });
            up = up.parent();
        }
        // and everything below the index
        QModelIndex doing = index;
        int row = 0;
        QStack<QModelIndex> todo;
        while (1) {
            auto node = this->index(row, 0, doing);
            if (!node.isValid()) {
                if (!todo.size()) {
                    break;
                } else {
                    doing = todo.pop();
                    row = 0;
                    continue;
                }
            }
            emit dataChanged(node, node, { Qt::CheckStateRole });
            todo.push(node);
            row++;
        }
        // siblings and unrelated nodes are ignored
    }
    return true;
}

bool FileIgnoreProxy::shouldExpand(QModelIndex index)
{
    QModelIndex sourceIndex = mapToSource(index);
    QFileSystemModel* fsm = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsm) {
        return false;
    }
    auto blockedPath = relPath(fsm->filePath(sourceIndex));
    auto found = m_blocked.find(blockedPath);
    if (found) {
        return !found->leaf();
    }
    return false;
}

void FileIgnoreProxy::setBlockedPaths(QStringList paths)
{
    beginResetModel();
    m_blocked.clear();
    m_blocked.insert(paths);
    endResetModel();
}

bool FileIgnoreProxy::filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const
{
    Q_UNUSED(source_parent)

    // adjust the columns you want to filter out here
    // return false for those that will be hidden
    if (source_column == 2 || source_column == 3)
        return false;

    return true;
}

bool FileIgnoreProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QFileSystemModel* fsm = qobject_cast<QFileSystemModel*>(sourceModel());

    auto fileInfo = fsm->fileInfo(index);
    return !ignoreFile(fileInfo);
}

bool FileIgnoreProxy::ignoreFile(QFileInfo fileInfo) const
{
    return m_ignoreFiles.contains(fileInfo.fileName()) || m_ignoreFilePaths.covers(relPath(fileInfo.absoluteFilePath()));
}

bool FileIgnoreProxy::filterFile(const QString& fileName) const
{
    return m_blocked.covers(fileName) || ignoreFile(QFileInfo(QDir(m_root), fileName));
}

void FileIgnoreProxy::loadBlockedPathsFromFile(const QString& fileName)
{
    QFile ignoreFile(fileName);
    if (!ignoreFile.open(QIODevice::ReadOnly)) {
        return;
    }
    auto ignoreData = ignoreFile.readAll();
    auto string = QString::fromUtf8(ignoreData);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    setBlockedPaths(string.split('\n', Qt::SkipEmptyParts));
#else
    setBlockedPaths(string.split('\n', QString::SkipEmptyParts));
#endif
}

void FileIgnoreProxy::saveBlockedPathsToFile(const QString& fileName)
{
    auto ignoreData = blockedPaths().toStringList().join('\n').toUtf8();
    try {
        FS::write(fileName, ignoreData);
    } catch (const Exception& e) {
        qWarning() << e.cause();
    }
}
