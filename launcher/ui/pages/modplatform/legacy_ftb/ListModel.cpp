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

#include "ListModel.h"
#include "Application.h"
#include "net/ApiDownload.h"
#include "net/HttpMetaCache.h"
#include "net/NetJob.h"

#include <Version.h>
#include "StringUtils.h"
#include "ui/widgets/ProjectItem.h"

#include <QLabel>
#include <QtMath>

#include <RWStorage.h>

#include <BuildConfig.h>

namespace LegacyFTB {

FilterModel::FilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    currentSorting = Sorting::ByGameVersion;
    sortings.insert(tr("Sort by Name"), Sorting::ByName);
    sortings.insert(tr("Sort by Game Version"), Sorting::ByGameVersion);
}

bool FilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    Modpack leftPack = sourceModel()->data(left, Qt::UserRole).value<Modpack>();
    Modpack rightPack = sourceModel()->data(right, Qt::UserRole).value<Modpack>();

    if (currentSorting == Sorting::ByGameVersion) {
        Version lv(leftPack.mcVersion);
        Version rv(rightPack.mcVersion);
        return lv < rv;

    } else if (currentSorting == Sorting::ByName) {
        return StringUtils::naturalCompare(leftPack.name, rightPack.name, Qt::CaseSensitive) >= 0;
    }

    // UHM, some inavlid value set?!
    qWarning() << "Invalid sorting set!";
    return true;
}

bool FilterModel::filterAcceptsRow([[maybe_unused]] int sourceRow, [[maybe_unused]] const QModelIndex& sourceParent) const
{
    if (searchTerm.isEmpty()) {
        return true;
    }
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    Modpack pack = sourceModel()->data(index, Qt::UserRole).value<Modpack>();
    if (searchTerm.startsWith("#"))
        return pack.packCode == searchTerm.mid(1);
    return pack.name.contains(searchTerm, Qt::CaseInsensitive);
}

void FilterModel::setSearchTerm(const QString term)
{
    searchTerm = term.trimmed();
    invalidate();
}

const QMap<QString, FilterModel::Sorting> FilterModel::getAvailableSortings()
{
    return sortings;
}

QString FilterModel::translateCurrentSorting()
{
    return sortings.key(currentSorting);
}

void FilterModel::setSorting(Sorting s)
{
    currentSorting = s;
    invalidate();
}

FilterModel::Sorting FilterModel::getCurrentSorting()
{
    return currentSorting;
}

ListModel::ListModel(QObject* parent) : QAbstractListModel(parent) {}

ListModel::~ListModel() {}

QString ListModel::translatePackType(PackType type) const
{
    switch (type) {
        case PackType::Public:
            return tr("Public Modpack");
        case PackType::ThirdParty:
            return tr("Third Party Modpack");
        case PackType::Private:
            return tr("Private Modpack");
    }
    qWarning() << "Unknown FTB modpack type:" << int(type);
    return QString();
}

int ListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : modpacks.size();
}

int ListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant ListModel::data(const QModelIndex& index, int role) const
{
    int pos = index.row();
    if (pos >= modpacks.size() || pos < 0 || !index.isValid()) {
        return QString("INVALID INDEX %1").arg(pos);
    }

    Modpack pack = modpacks.at(pos);
    switch (role) {
        case Qt::ToolTipRole: {
            if (pack.description.length() > 100) {
                // some magic to prevent to long tooltips and replace html linebreaks
                QString edit = pack.description.left(97);
                edit = edit.left(edit.lastIndexOf("<br>")).left(edit.lastIndexOf(" ")).append("...");
                return edit;
            }
            return pack.description;
        }
        case Qt::DecorationRole: {
            if (m_logoMap.contains(pack.logo)) {
                return (m_logoMap.value(pack.logo));
            }
            QIcon icon = APPLICATION->getThemedIcon("screenshot-placeholder");
            ((ListModel*)this)->requestLogo(pack.logo);
            return icon;
        }
        case Qt::UserRole: {
            QVariant v;
            v.setValue(pack);
            return v;
        }
        case Qt::ForegroundRole: {
            if (pack.broken) {
                // FIXME: Hardcoded color
                return QColor(255, 0, 50);
            } else if (pack.bugged) {
                // FIXME: Hardcoded color
                // bugged pack, currently only indicates bugged xml
                return QColor(244, 229, 66);
            }
        }
        case Qt::DisplayRole:
            return pack.name;
        case Qt::SizeHintRole:
            return QSize(0, 58);
        // Custom data
        case UserDataTypes::TITLE:
            return pack.name;
        case UserDataTypes::DESCRIPTION:
            return pack.description;
        case UserDataTypes::SELECTED:
            return false;
        case UserDataTypes::INSTALLED:
            return false;
        default:
            break;
    }

    return {};
}

void ListModel::fill(ModpackList modpacks_)
{
    beginResetModel();
    this->modpacks = modpacks_;
    endResetModel();
}

void ListModel::addPack(const Modpack& modpack)
{
    beginResetModel();
    this->modpacks.append(modpack);
    endResetModel();
}

void ListModel::clear()
{
    beginResetModel();
    modpacks.clear();
    endResetModel();
}

Modpack ListModel::at(int row)
{
    return modpacks.at(row);
}

void ListModel::remove(int row)
{
    if (row < 0 || row >= modpacks.size()) {
        qWarning() << "Attempt to remove FTB modpacks with invalid row" << row;
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    modpacks.removeAt(row);
    endRemoveRows();
}

void ListModel::logoLoaded(QString logo, QIcon out)
{
    m_loadingLogos.removeAll(logo);
    m_logoMap.insert(logo, out);
    emit dataChanged(createIndex(0, 0), createIndex(1, 0));
}

void ListModel::logoFailed(QString logo)
{
    m_failedLogos.append(logo);
    m_loadingLogos.removeAll(logo);
}

void ListModel::requestLogo(QString file)
{
    if (m_loadingLogos.contains(file) || m_failedLogos.contains(file)) {
        return;
    }

    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("FTBPacks", QString("logos/%1").arg(file));
    NetJob* job = new NetJob(QString("FTB Icon Download for %1").arg(file), APPLICATION->network());
    job->setAskRetry(false);
    job->addNetAction(Net::ApiDownload::makeCached(QUrl(QString(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "static/%1").arg(file)), entry));

    auto fullPath = entry->getFullPath();
    QObject::connect(job, &NetJob::finished, this, [this, file, fullPath, job] {
        job->deleteLater();
        emit logoLoaded(file, QIcon(fullPath));
        if (waitingCallbacks.contains(file)) {
            waitingCallbacks.value(file)(fullPath);
        }
    });

    QObject::connect(job, &NetJob::failed, this, [this, file, job] {
        job->deleteLater();
        emit logoFailed(file);
    });

    job->start();

    m_loadingLogos.append(file);
}

void ListModel::getLogo(const QString& logo, LogoCallback callback)
{
    if (m_logoMap.contains(logo)) {
        callback(APPLICATION->metacache()->resolveEntry("FTBPacks", QString("logos/%1").arg(logo))->getFullPath());
    } else {
        requestLogo(logo);
    }
}

Qt::ItemFlags ListModel::flags(const QModelIndex& index) const
{
    return QAbstractListModel::flags(index);
}

}  // namespace LegacyFTB
