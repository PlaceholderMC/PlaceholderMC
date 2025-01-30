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

#pragma once

#include "Resource.h"

#include <QMutex>

class Version;

/* TODO:
 *
 * Store localized descriptions
 * */

class DataPack : public Resource {
    Q_OBJECT
   public:
    DataPack(QObject* parent = nullptr) : Resource(parent) {}
    DataPack(QFileInfo file_info) : Resource(file_info) {}

    /** Gets the numerical ID of the pack format. */
    [[nodiscard]] int packFormat() const { return m_pack_format; }
    /** Gets, respectively, the lower and upper versions supported by the set pack format. */
    [[nodiscard]] std::pair<Version, Version> compatibleVersions() const;

    /** Gets the description of the data pack. */
    [[nodiscard]] QString description() const { return m_description; }

    /** Thread-safe. */
    void setPackFormat(int new_format_id);

    /** Thread-safe. */
    void setDescription(QString new_description);

    bool valid() const override;

    [[nodiscard]] int compare(Resource const& other, SortType type) const override;
    [[nodiscard]] bool applyFilter(QRegularExpression filter) const override;

    virtual QString directory() { return "/data"; }

   protected:
    mutable QMutex m_data_lock;

    /* The 'version' of a data pack, as defined in the pack.mcmeta file.
     * See https://minecraft.wiki/w/Data_pack#pack.mcmeta
     */
    int m_pack_format = 0;

    /** The data pack's description, as defined in the pack.mcmeta file.
     */
    QString m_description;
};
