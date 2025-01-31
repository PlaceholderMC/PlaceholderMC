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

#include "VersionProxyModel.h"
#include <Version.h>
#include <meta/VersionList.h>
#include <QPixmapCache>
#include <QSortFilterProxyModel>
#include "Application.h"

class VersionFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
   public:
    VersionFilterModel(VersionProxyModel* parent) : QSortFilterProxyModel(parent)
    {
        m_parent = parent;
        setSortRole(BaseVersionList::SortRole);
        sort(0, Qt::DescendingOrder);
    }

    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
    {
        const auto& filters = m_parent->filters();
        const QString& search = m_parent->search();
        const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

        if (!search.isEmpty() && !sourceModel()->data(idx, BaseVersionList::VersionRole).toString().contains(search, Qt::CaseInsensitive))
            return false;

        for (auto it = filters.begin(); it != filters.end(); ++it) {
            auto data = sourceModel()->data(idx, it.key());
            auto match = data.toString();
            if (!it.value()->accepts(match)) {
                return false;
            }
        }
        return true;
    }

    void filterChanged() { invalidateFilter(); }

   private:
    VersionProxyModel* m_parent;
};

VersionProxyModel::VersionProxyModel(QObject* parent) : QAbstractProxyModel(parent)
{
    filterModel = new VersionFilterModel(this);
    connect(filterModel, &QAbstractItemModel::dataChanged, this, &VersionProxyModel::sourceDataChanged);
    connect(filterModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &VersionProxyModel::sourceRowsAboutToBeInserted);
    connect(filterModel, &QAbstractItemModel::rowsInserted, this, &VersionProxyModel::sourceRowsInserted);
    connect(filterModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &VersionProxyModel::sourceRowsAboutToBeRemoved);
    connect(filterModel, &QAbstractItemModel::rowsRemoved, this, &VersionProxyModel::sourceRowsRemoved);
    // FIXME: implement when needed
    /*
    connect(replacing, &QAbstractItemModel::rowsAboutToBeMoved, this, &VersionProxyModel::sourceRowsAboutToBeMoved);
    connect(replacing, &QAbstractItemModel::rowsMoved, this, &VersionProxyModel::sourceRowsMoved);
    connect(replacing, &QAbstractItemModel::layoutAboutToBeChanged, this, &VersionProxyModel::sourceLayoutAboutToBeChanged);
    connect(replacing, &QAbstractItemModel::layoutChanged, this, &VersionProxyModel::sourceLayoutChanged);
    */
    connect(filterModel, &QAbstractItemModel::modelAboutToBeReset, this, &VersionProxyModel::sourceAboutToBeReset);
    connect(filterModel, &QAbstractItemModel::modelReset, this, &VersionProxyModel::sourceReset);

    QAbstractProxyModel::setSourceModel(filterModel);
}

QVariant VersionProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= m_columns.size())
        return QVariant();
    if (orientation != Qt::Horizontal)
        return QVariant();
    auto column = m_columns[section];
    if (role == Qt::DisplayRole) {
        switch (column) {
            case Name:
                return tr("Version");
            case ParentVersion:
                return tr("Minecraft");  // FIXME: this should come from metadata
            case Branch:
                return tr("Branch");
            case Type:
                return tr("Type");
            case CPUArchitecture:
                return tr("Architecture");
            case Path:
                return tr("Path");
            case JavaName:
                return tr("Java Name");
            case JavaMajor:
                return tr("Major Version");
            case Time:
                return tr("Released");
        }
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
            case Name:
                return tr("The name of the version.");
            case ParentVersion:
                return tr("Minecraft version");  // FIXME: this should come from metadata
            case Branch:
                return tr("The version's branch");
            case Type:
                return tr("The version's type");
            case CPUArchitecture:
                return tr("CPU Architecture");
            case Path:
                return tr("Filesystem path to this version");
            case JavaName:
                return tr("The alternative name of the Java version");
            case JavaMajor:
                return tr("The Java major version");
            case Time:
                return tr("Release date of this version");
        }
    }
    return QVariant();
}

QVariant VersionProxyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    auto column = m_columns[index.column()];
    auto parentIndex = mapToSource(index);
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case Name: {
                    QString version = sourceModel()->data(parentIndex, BaseVersionList::VersionRole).toString();
                    if (version == m_currentVersion) {
                        return tr("%1 (installed)").arg(version);
                    }
                    return version;
                }
                case ParentVersion:
                    return sourceModel()->data(parentIndex, BaseVersionList::ParentVersionRole);
                case Branch:
                    return sourceModel()->data(parentIndex, BaseVersionList::BranchRole);
                case Type:
                    return sourceModel()->data(parentIndex, BaseVersionList::TypeRole);
                case CPUArchitecture:
                    return sourceModel()->data(parentIndex, BaseVersionList::CPUArchitectureRole);
                case Path:
                    return sourceModel()->data(parentIndex, BaseVersionList::PathRole);
                case JavaName:
                    return sourceModel()->data(parentIndex, BaseVersionList::JavaNameRole);
                case JavaMajor:
                    return sourceModel()->data(parentIndex, BaseVersionList::JavaMajorRole);
                case Time:
                    return sourceModel()->data(parentIndex, Meta::VersionList::TimeRole).toDate();
                default:
                    return QVariant();
            }
        }
        case Qt::ToolTipRole: {
            if (column == Name && hasRecommended) {
                auto value = sourceModel()->data(parentIndex, BaseVersionList::RecommendedRole);
                if (value.toBool()) {
                    return tr("Recommended");
                } else if (hasLatest) {
                    auto value = sourceModel()->data(parentIndex, BaseVersionList::LatestRole);
                    if (value.toBool()) {
                        return tr("Latest");
                    }
                }
            } else {
                return sourceModel()->data(parentIndex, BaseVersionList::VersionIdRole);
            }
        }
        case Qt::DecorationRole: {
            switch (column) {
                case Name: {
                    if (hasRecommended) {
                        auto recommenced = sourceModel()->data(parentIndex, BaseVersionList::RecommendedRole);
                        if (recommenced.toBool()) {
                            return APPLICATION->getThemedIcon("star");
                        } else if (hasLatest) {
                            auto latest = sourceModel()->data(parentIndex, BaseVersionList::LatestRole);
                            if (latest.toBool()) {
                                return APPLICATION->getThemedIcon("bug");
                            }
                        }
                        QPixmap pixmap;
                        QPixmapCache::find("placeholder", &pixmap);
                        if (!pixmap) {
                            QPixmap px(16, 16);
                            px.fill(Qt::transparent);
                            QPixmapCache::insert("placeholder", px);
                            return px;
                        }
                        return pixmap;
                    }
                }
                default: {
                    return QVariant();
                }
            }
        }
        default: {
            if (roles.contains((BaseVersionList::ModelRoles)role)) {
                return sourceModel()->data(parentIndex, role);
            }
            return QVariant();
        }
    }
}

QModelIndex VersionProxyModel::parent([[maybe_unused]] const QModelIndex& child) const
{
    return QModelIndex();
}

QModelIndex VersionProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (sourceIndex.isValid()) {
        return index(sourceIndex.row(), 0);
    }
    return QModelIndex();
}

QModelIndex VersionProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (proxyIndex.isValid()) {
        return sourceModel()->index(proxyIndex.row(), 0);
    }
    return QModelIndex();
}

QModelIndex VersionProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    // no trees here... shoo
    if (parent.isValid()) {
        return QModelIndex();
    }
    if (row < 0 || row >= sourceModel()->rowCount())
        return QModelIndex();
    if (column < 0 || column >= columnCount())
        return QModelIndex();
    return QAbstractItemModel::createIndex(row, column);
}

int VersionProxyModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

int VersionProxyModel::rowCount(const QModelIndex& parent) const
{
    if (sourceModel()) {
        return sourceModel()->rowCount(parent);
    }
    return 0;
}

void VersionProxyModel::sourceDataChanged(const QModelIndex& source_top_left, const QModelIndex& source_bottom_right)
{
    if (source_top_left.parent() != source_bottom_right.parent())
        return;

    // whole row is getting changed
    auto topLeft = createIndex(source_top_left.row(), 0);
    auto bottomRight = createIndex(source_bottom_right.row(), columnCount() - 1);
    emit dataChanged(topLeft, bottomRight);
}

void VersionProxyModel::setSourceModel(QAbstractItemModel* replacingRaw)
{
    auto replacing = dynamic_cast<BaseVersionList*>(replacingRaw);
    beginResetModel();

    m_columns.clear();
    if (!replacing) {
        roles.clear();
        filterModel->setSourceModel(replacing);
        endResetModel();
        return;
    }

    roles = replacing->providesRoles();
    if (roles.contains(BaseVersionList::VersionRole)) {
        m_columns.push_back(Name);
    }
    /*
    if(roles.contains(BaseVersionList::ParentVersionRole))
    {
        m_columns.push_back(ParentVersion);
    }
    */
    if (roles.contains(BaseVersionList::CPUArchitectureRole)) {
        m_columns.push_back(CPUArchitecture);
    }
    if (roles.contains(BaseVersionList::PathRole)) {
        m_columns.push_back(Path);
    }
    if (roles.contains(BaseVersionList::JavaNameRole)) {
        m_columns.push_back(JavaName);
    }
    if (roles.contains(BaseVersionList::JavaMajorRole)) {
        m_columns.push_back(JavaMajor);
    }
    if (roles.contains(Meta::VersionList::TimeRole)) {
        m_columns.push_back(Time);
    }
    if (roles.contains(BaseVersionList::BranchRole)) {
        m_columns.push_back(Branch);
    }
    if (roles.contains(BaseVersionList::TypeRole)) {
        m_columns.push_back(Type);
    }
    if (roles.contains(BaseVersionList::RecommendedRole)) {
        hasRecommended = true;
    }
    if (roles.contains(BaseVersionList::LatestRole)) {
        hasLatest = true;
    }
    filterModel->setSourceModel(replacing);

    endResetModel();
}

QModelIndex VersionProxyModel::getRecommended() const
{
    if (!roles.contains(BaseVersionList::RecommendedRole)) {
        return index(0, 0);
    }
    int recommended = 0;
    for (int i = 0; i < rowCount(); i++) {
        auto value = sourceModel()->data(mapToSource(index(i, 0)), BaseVersionList::RecommendedRole);
        if (value.toBool()) {
            recommended = i;
        }
    }
    return index(recommended, 0);
}

QModelIndex VersionProxyModel::getVersion(const QString& version) const
{
    int found = -1;
    for (int i = 0; i < rowCount(); i++) {
        auto value = sourceModel()->data(mapToSource(index(i, 0)), BaseVersionList::VersionRole);
        if (value.toString() == version) {
            found = i;
        }
    }
    if (found == -1) {
        return QModelIndex();
    }
    return index(found, 0);
}

void VersionProxyModel::clearFilters()
{
    m_filters.clear();
    m_search.clear();
    filterModel->filterChanged();
}

void VersionProxyModel::setFilter(const BaseVersionList::ModelRoles column, Filter* f)
{
    m_filters[column].reset(f);
    filterModel->filterChanged();
}

void VersionProxyModel::setSearch(const QString& search)
{
    m_search = search;
    filterModel->filterChanged();
}

const VersionProxyModel::FilterMap& VersionProxyModel::filters() const
{
    return m_filters;
}

const QString& VersionProxyModel::search() const
{
    return m_search;
}

void VersionProxyModel::sourceAboutToBeReset()
{
    beginResetModel();
}

void VersionProxyModel::sourceReset()
{
    endResetModel();
}

void VersionProxyModel::sourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    beginInsertRows(parent, first, last);
}

void VersionProxyModel::sourceRowsInserted([[maybe_unused]] const QModelIndex& parent,
                                           [[maybe_unused]] int first,
                                           [[maybe_unused]] int last)
{
    endInsertRows();
}

void VersionProxyModel::sourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    beginRemoveRows(parent, first, last);
}

void VersionProxyModel::sourceRowsRemoved([[maybe_unused]] const QModelIndex& parent, [[maybe_unused]] int first, [[maybe_unused]] int last)
{
    endRemoveRows();
}

void VersionProxyModel::setCurrentVersion(const QString& version)
{
    m_currentVersion = version;
}

#include "VersionProxyModel.moc"
