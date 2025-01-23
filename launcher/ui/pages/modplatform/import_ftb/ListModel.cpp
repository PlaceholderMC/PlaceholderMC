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

#include "ListModel.h"
#include <qfileinfo.h>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QProcessEnvironment>
#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "StringUtils.h"
#include "modplatform/import_ftb/PackHelpers.h"
#include "ui/widgets/ProjectItem.h"

namespace FTBImportAPP {

QString getFTBRoot()
{
    QString partialPath = QDir::homePath();
#if defined(Q_OS_OSX)
    partialPath = FS::PathCombine(partialPath, "Library/Application Support");
#endif
    return FS::PathCombine(partialPath, ".ftba");
}

QString getDynamicPath()
{
    auto settingsPath = FS::PathCombine(getFTBRoot(), "storage", "settings.json");
    if (!QFileInfo::exists(settingsPath))
        settingsPath = FS::PathCombine(getFTBRoot(), "bin", "settings.json");
    if (!QFileInfo::exists(settingsPath)) {
        qWarning() << "The ftb app setings doesn't exist.";
        return {};
    }
    try {
        auto doc = Json::requireDocument(FS::read(settingsPath));
        return Json::requireString(Json::requireObject(doc), "instanceLocation");
    } catch (const Exception& e) {
        qCritical() << "Could not read ftb settings file: " << e.cause();
    }
    return {};
}

ListModel::ListModel(QObject* parent) : QAbstractListModel(parent), m_instances_path(getDynamicPath()) {}

void ListModel::update()
{
    beginResetModel();
    m_modpacks.clear();

    auto wasPathAdded = [this](QString path) {
        for (auto pack : m_modpacks) {
            if (pack.path == path)
                return true;
        }
        return false;
    };

    auto scanPath = [this, wasPathAdded](QString path) {
        if (path.isEmpty())
            return;
        if (auto instancesInfo = QFileInfo(path); !instancesInfo.exists() || !instancesInfo.isDir())
            return;
        QDirIterator directoryIterator(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden,
                                       QDirIterator::FollowSymlinks);
        while (directoryIterator.hasNext()) {
            auto currentPath = directoryIterator.next();
            if (!wasPathAdded(currentPath)) {
                auto modpack = parseDirectory(currentPath);
                if (!modpack.path.isEmpty())
                    m_modpacks.append(modpack);
            }
        }
    };

    scanPath(APPLICATION->settings()->get("FTBAppInstancesPath").toString());
    scanPath(m_instances_path);

    endResetModel();
}

QVariant ListModel::data(const QModelIndex& index, int role) const
{
    int pos = index.row();
    if (pos >= m_modpacks.size() || pos < 0 || !index.isValid()) {
        return QVariant();
    }

    auto pack = m_modpacks.at(pos);
    switch (role) {
        case Qt::ToolTipRole:
            return tr("Minecraft %1").arg(pack.mcVersion);
        case Qt::DecorationRole:
            return pack.icon;
        case Qt::UserRole: {
            QVariant v;
            v.setValue(pack);
            return v;
        }
        case Qt::DisplayRole:
            return pack.name;
        case Qt::SizeHintRole:
            return QSize(0, 58);
        // Custom data
        case UserDataTypes::TITLE:
            return pack.name;
        case UserDataTypes::DESCRIPTION:
            return tr("Minecraft %1").arg(pack.mcVersion);
        case UserDataTypes::SELECTED:
            return false;
        case UserDataTypes::INSTALLED:
            return false;
        default:
            break;
    }

    return {};
}

FilterModel::FilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    m_currentSorting = Sorting::ByGameVersion;
    m_sortings.insert(tr("Sort by Name"), Sorting::ByName);
    m_sortings.insert(tr("Sort by Game Version"), Sorting::ByGameVersion);
}

bool FilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    Modpack leftPack = sourceModel()->data(left, Qt::UserRole).value<Modpack>();
    Modpack rightPack = sourceModel()->data(right, Qt::UserRole).value<Modpack>();

    if (m_currentSorting == Sorting::ByGameVersion) {
        Version lv(leftPack.mcVersion);
        Version rv(rightPack.mcVersion);
        return lv < rv;

    } else if (m_currentSorting == Sorting::ByName) {
        return StringUtils::naturalCompare(leftPack.name, rightPack.name, Qt::CaseSensitive) >= 0;
    }

    // UHM, some inavlid value set?!
    qWarning() << "Invalid sorting set!";
    return true;
}

bool FilterModel::filterAcceptsRow([[maybe_unused]] int sourceRow, [[maybe_unused]] const QModelIndex& sourceParent) const
{
    if (m_searchTerm.isEmpty()) {
        return true;
    }
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    Modpack pack = sourceModel()->data(index, Qt::UserRole).value<Modpack>();
    return pack.name.contains(m_searchTerm, Qt::CaseInsensitive);
}

void FilterModel::setSearchTerm(const QString term)
{
    m_searchTerm = term.trimmed();
    invalidate();
}

const QMap<QString, FilterModel::Sorting> FilterModel::getAvailableSortings()
{
    return m_sortings;
}

QString FilterModel::translateCurrentSorting()
{
    return m_sortings.key(m_currentSorting);
}

void FilterModel::setSorting(Sorting s)
{
    m_currentSorting = s;
    invalidate();
}

FilterModel::Sorting FilterModel::getCurrentSorting()
{
    return m_currentSorting;
}
void ListModel::setPath(QString path)
{
    APPLICATION->settings()->set("FTBAppInstancesPath", path);
    update();
}

QString ListModel::getUserPath()
{
    auto path = APPLICATION->settings()->get("FTBAppInstancesPath").toString();
    if (path.isEmpty())
        path = m_instances_path;
    return path;
}
}  // namespace FTBImportAPP