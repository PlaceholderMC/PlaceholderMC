// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include "DataPack.h"

#include <QDebug>
#include <QMap>
#include <QRegularExpression>

#include "Version.h"

// Values taken from:
// https://minecraft.wiki/w/Pack_format#List_of_data_pack_formats
static const QMap<int, std::pair<Version, Version>> s_pack_format_versions = { { 4, { Version("1.13"), Version("1.14.4") } },
                                                                               { 5, { Version("1.15"), Version("1.16.1") } },
                                                                               { 6, { Version("1.16.2"), Version("1.16.5") } },
                                                                               { 7, { Version("1.17"), Version("1.17.1") } },
                                                                               { 8, { Version("1.18"), Version("1.18.1") } },
                                                                               { 9, { Version("1.18.2"), Version("1.18.2") } },
                                                                               { 10, { Version("1.19"), Version("1.19.3") } },
                                                                               { 11, { Version("23w03a"), Version("23w05a") } },
                                                                               { 12, { Version("1.19.4"), Version("1.19.4") } },
                                                                               { 13, { Version("23w12a"), Version("23w14a") } },
                                                                               { 14, { Version("23w16a"), Version("23w17a") } },
                                                                               { 15, { Version("1.20"), Version("1.20.1") } },
                                                                               { 16, { Version("23w31a"), Version("23w31a") } },
                                                                               { 17, { Version("23w32a"), Version("23w35a") } },
                                                                               { 18, { Version("1.20.2"), Version("1.20.2") } },
                                                                               { 19, { Version("23w40a"), Version("23w40a") } },
                                                                               { 20, { Version("23w41a"), Version("23w41a") } },
                                                                               { 21, { Version("23w42a"), Version("23w42a") } },
                                                                               { 22, { Version("23w43a"), Version("23w43b") } },
                                                                               { 23, { Version("23w44a"), Version("23w44a") } },
                                                                               { 24, { Version("23w45a"), Version("23w45a") } },
                                                                               { 25, { Version("23w46a"), Version("23w46a") } },
                                                                               { 26, { Version("1.20.3"), Version("1.20.4") } },
                                                                               { 27, { Version("23w51a"), Version("23w51b") } },
                                                                               { 28, { Version("24w05a"), Version("24w05b") } },
                                                                               { 29, { Version("24w04a"), Version("24w04a") } },
                                                                               { 30, { Version("24w05a"), Version("24w05b") } },
                                                                               { 31, { Version("24w06a"), Version("24w06a") } },
                                                                               { 32, { Version("24w07a"), Version("24w07a") } },
                                                                               { 33, { Version("24w09a"), Version("24w09a") } },
                                                                               { 34, { Version("24w10a"), Version("24w10a") } },
                                                                               { 35, { Version("24w11a"), Version("24w11a") } },
                                                                               { 36, { Version("24w12a"), Version("24w12a") } },
                                                                               { 37, { Version("24w13a"), Version("24w13a") } },
                                                                               { 38, { Version("24w14a"), Version("24w14a") } },
                                                                               { 39, { Version("1.20.5-pre1"), Version("1.20.5-pre1") } },
                                                                               { 40, { Version("1.20.5-pre2"), Version("1.20.5-pre2") } },
                                                                               { 41, { Version("1.20.5"), Version("1.20.6") } },
                                                                               { 42, { Version("24w18a"), Version("24w18a") } },
                                                                               { 43, { Version("24w19a"), Version("24w19b") } },
                                                                               { 44, { Version("24w20a"), Version("24w20a") } },
                                                                               { 45, { Version("21w21a"), Version("21w21b") } },
                                                                               { 46, { Version("1.21-pre1"), Version("1.21-pre1") } },
                                                                               { 47, { Version("1.21-pre2"), Version("1.21-pre2") } },
                                                                               { 48, { Version("1.21"), Version("1.21") } } };

void DataPack::setPackFormat(int new_format_id)
{
    QMutexLocker locker(&m_data_lock);

    if (!s_pack_format_versions.contains(new_format_id)) {
        qWarning() << "Pack format '" << new_format_id << "' is not a recognized data pack id!";
    }

    m_pack_format = new_format_id;
}

void DataPack::setDescription(QString new_description)
{
    QMutexLocker locker(&m_data_lock);

    m_description = new_description;
}

std::pair<Version, Version> DataPack::compatibleVersions() const
{
    if (!s_pack_format_versions.contains(m_pack_format)) {
        return { {}, {} };
    }

    return s_pack_format_versions.constFind(m_pack_format).value();
}

int DataPack::compare(const Resource& other, SortType type) const
{
    auto const& cast_other = static_cast<DataPack const&>(other);
    if (type == SortType::PACK_FORMAT) {
        auto this_ver = packFormat();
        auto other_ver = cast_other.packFormat();

        if (this_ver > other_ver)
            return 1;
        if (this_ver < other_ver)
            return -1;
    } else {
        return Resource::compare(other, type);
    }
    return 0;
}

bool DataPack::applyFilter(QRegularExpression filter) const
{
    if (filter.match(description()).hasMatch())
        return true;

    if (filter.match(QString::number(packFormat())).hasMatch())
        return true;

    if (filter.match(compatibleVersions().first.toString()).hasMatch())
        return true;
    if (filter.match(compatibleVersions().second.toString()).hasMatch())
        return true;

    return Resource::applyFilter(filter);
}

bool DataPack::valid() const
{
    return m_pack_format != 0;
}
