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

#include "WorldList.h"

#include <FileSystem.h>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QMimeData>
#include <QString>
#include <QUrl>
#include <QUuid>
#include <Qt>
#include "Application.h"

WorldList::WorldList(const QString& dir, BaseInstance* instance) : QAbstractListModel(), m_instance(instance), m_dir(dir)
{
    FS::ensureFolderPathExists(m_dir.absolutePath());
    m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    m_dir.setSorting(QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
    m_watcher = new QFileSystemWatcher(this);
    is_watching = false;
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &WorldList::directoryChanged);
}

void WorldList::startWatching()
{
    if (is_watching) {
        return;
    }
    update();
    is_watching = m_watcher->addPath(m_dir.absolutePath());
    if (is_watching) {
        qDebug() << "Started watching " << m_dir.absolutePath();
    } else {
        qDebug() << "Failed to start watching " << m_dir.absolutePath();
    }
}

void WorldList::stopWatching()
{
    if (!is_watching) {
        return;
    }
    is_watching = !m_watcher->removePath(m_dir.absolutePath());
    if (!is_watching) {
        qDebug() << "Stopped watching " << m_dir.absolutePath();
    } else {
        qDebug() << "Failed to stop watching " << m_dir.absolutePath();
    }
}

bool WorldList::update()
{
    if (!isValid())
        return false;

    QList<World> newWorlds;
    m_dir.refresh();
    auto folderContents = m_dir.entryInfoList();
    // if there are any untracked files...
    for (QFileInfo entry : folderContents) {
        if (!entry.isDir())
            continue;

        World w(entry);
        if (w.isValid()) {
            newWorlds.append(w);
        }
    }
    beginResetModel();
    worlds.swap(newWorlds);
    endResetModel();
    return true;
}

void WorldList::directoryChanged(QString path)
{
    update();
}

bool WorldList::isValid()
{
    return m_dir.exists() && m_dir.isReadable();
}

QString WorldList::instDirPath() const
{
    return QFileInfo(m_instance->instanceRoot()).absoluteFilePath();
}

bool WorldList::deleteWorld(int index)
{
    if (index >= worlds.size() || index < 0)
        return false;
    World& m = worlds[index];
    if (m.destroy()) {
        beginRemoveRows(QModelIndex(), index, index);
        worlds.removeAt(index);
        endRemoveRows();
        emit changed();
        return true;
    }
    return false;
}

bool WorldList::deleteWorlds(int first, int last)
{
    for (int i = first; i <= last; i++) {
        World& m = worlds[i];
        m.destroy();
    }
    beginRemoveRows(QModelIndex(), first, last);
    worlds.erase(worlds.begin() + first, worlds.begin() + last + 1);
    endRemoveRows();
    emit changed();
    return true;
}

bool WorldList::resetIcon(int row)
{
    if (row >= worlds.size() || row < 0)
        return false;
    World& m = worlds[row];
    if (m.resetIcon()) {
        emit dataChanged(index(row), index(row), { WorldList::IconFileRole });
        return true;
    }
    return false;
}

int WorldList::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant WorldList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int column = index.column();

    if (row < 0 || row >= worlds.size())
        return QVariant();

    QLocale locale;

    auto& world = worlds[row];
    switch (role) {
        case Qt::DisplayRole:
            switch (column) {
                case NameColumn:
                    return world.name();

                case GameModeColumn:
                    return world.gameType().toTranslatedString();

                case LastPlayedColumn:
                    return world.lastPlayed();

                case SizeColumn:
                    return locale.formattedDataSize(world.bytes());

                case InfoColumn:
                    if (world.isSymLinkUnder(instDirPath())) {
                        return tr("This world is symbolically linked from elsewhere.");
                    }
                    if (world.isMoreThanOneHardLink()) {
                        return tr("\nThis world is hard linked elsewhere.");
                    }
                    return "";
                default:
                    return QVariant();
            }

        case Qt::UserRole:
            if (column == SizeColumn)
                return QVariant::fromValue<qlonglong>(world.bytes());
            return data(index, Qt::DisplayRole);

        case Qt::ToolTipRole: {
            if (column == InfoColumn) {
                if (world.isSymLinkUnder(instDirPath())) {
                    return tr("Warning: This world is symbolically linked from elsewhere. Editing it will also change the original."
                              "\nCanonical Path: %1")
                        .arg(world.canonicalFilePath());
                }
                if (world.isMoreThanOneHardLink()) {
                    return tr("Warning: This world is hard linked elsewhere. Editing it will also change the original.");
                }
            }
            return world.folderName();
        }
        case ObjectRole: {
            return QVariant::fromValue<void*>((void*)&world);
        }
        case FolderRole: {
            return QDir::toNativeSeparators(dir().absoluteFilePath(world.folderName()));
        }
        case SeedRole: {
            return QVariant::fromValue<qlonglong>(world.seed());
        }
        case NameRole: {
            return world.name();
        }
        case LastPlayedRole: {
            return world.lastPlayed();
        }
        case SizeRole: {
            return QVariant::fromValue<qlonglong>(world.bytes());
        }
        case IconFileRole: {
            return world.iconFile();
        }
        default:
            return QVariant();
    }
}

QVariant WorldList::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const
{
    switch (role) {
        case Qt::DisplayRole:
            switch (section) {
                case NameColumn:
                    return tr("Name");
                case GameModeColumn:
                    return tr("Game Mode");
                case LastPlayedColumn:
                    return tr("Last Played");
                case SizeColumn:
                    //: World size on disk
                    return tr("Size");
                case InfoColumn:
                    //: special warnings?
                    return tr("Info");
                default:
                    return QVariant();
            }

        case Qt::ToolTipRole:
            switch (section) {
                case NameColumn:
                    return tr("The name of the world.");
                case GameModeColumn:
                    return tr("Game mode of the world.");
                case LastPlayedColumn:
                    return tr("Date and time the world was last played.");
                case SizeColumn:
                    return tr("Size of the world on disk.");
                case InfoColumn:
                    return tr("Information and warnings about the world.");
                default:
                    return QVariant();
            }
        default:
            return QVariant();
    }
}

QStringList WorldList::mimeTypes() const
{
    QStringList types;
    types << "text/uri-list";
    return types;
}

class WorldMimeData : public QMimeData {
    Q_OBJECT

   public:
    WorldMimeData(QList<World> worlds) { m_worlds = worlds; }
    QStringList formats() const { return QMimeData::formats() << "text/uri-list"; }

   protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVariant retrieveData(const QString& mimetype, QMetaType type) const
#else
    QVariant retrieveData(const QString& mimetype, QVariant::Type type) const
#endif
    {
        QList<QUrl> urls;
        for (auto& world : m_worlds) {
            if (!world.isValid() || !world.isOnFS())
                continue;
            QString worldPath = world.container().absoluteFilePath();
            qDebug() << worldPath;
            urls.append(QUrl::fromLocalFile(worldPath));
        }
        const_cast<WorldMimeData*>(this)->setUrls(urls);
        return QMimeData::retrieveData(mimetype, type);
    }

   private:
    QList<World> m_worlds;
};

QMimeData* WorldList::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.size() == 0)
        return new QMimeData();

    QList<World> worlds_;
    for (auto idx : indexes) {
        if (idx.column() != 0)
            continue;
        int row = idx.row();
        if (row < 0 || row >= this->worlds.size())
            continue;
        worlds_.append(this->worlds[row]);
    }
    if (!worlds_.size()) {
        return new QMimeData();
    }
    return new WorldMimeData(worlds_);
}

Qt::ItemFlags WorldList::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid())
        return Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    else
        return Qt::ItemIsDropEnabled | defaultFlags;
}

Qt::DropActions WorldList::supportedDragActions() const
{
    // move to other mod lists or VOID
    return Qt::MoveAction;
}

Qt::DropActions WorldList::supportedDropActions() const
{
    // copy from outside, move from within and other mod lists
    return Qt::CopyAction | Qt::MoveAction;
}

void WorldList::installWorld(QFileInfo filename)
{
    qDebug() << "installing: " << filename.absoluteFilePath();
    World w(filename);
    if (!w.isValid()) {
        return;
    }
    w.install(m_dir.absolutePath());
}

bool WorldList::dropMimeData(const QMimeData* data,
                             Qt::DropAction action,
                             [[maybe_unused]] int row,
                             [[maybe_unused]] int column,
                             [[maybe_unused]] const QModelIndex& parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    // check if the action is supported
    if (!data || !(action & supportedDropActions()))
        return false;
    // files dropped from outside?
    if (data->hasUrls()) {
        bool was_watching = is_watching;
        if (was_watching)
            stopWatching();
        auto urls = data->urls();
        for (auto url : urls) {
            // only local files may be dropped...
            if (!url.isLocalFile())
                continue;
            QString filename = url.toLocalFile();

            QFileInfo worldInfo(filename);

            if (!m_dir.entryInfoList().contains(worldInfo)) {
                installWorld(worldInfo);
            }
        }
        if (was_watching)
            startWatching();
        return true;
    }
    return false;
}

#include "WorldList.moc"
