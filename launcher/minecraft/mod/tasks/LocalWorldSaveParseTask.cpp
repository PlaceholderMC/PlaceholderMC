
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

#include "LocalWorldSaveParseTask.h"

#include "FileSystem.h"

#include <quazip/quazip.h>
#include <quazip/quazipdir.h>
#include <quazip/quazipfile.h>

#include <QDir>
#include <QFileInfo>

namespace WorldSaveUtils {

bool process(WorldSave& pack, ProcessingLevel level)
{
    switch (pack.type()) {
        case ResourceType::FOLDER:
            return WorldSaveUtils::processFolder(pack, level);
        case ResourceType::ZIPFILE:
            return WorldSaveUtils::processZIP(pack, level);
        default:
            qWarning() << "Invalid type for world save parse task!";
            return false;
    }
}

/// @brief checks a folder structure to see if it contains a level.dat
/// @param dir the path to check
/// @param saves used in recursive call if a "saves" dir was found
/// @return std::tuple of (
///             bool <found level.dat>,
///             QString <name of folder containing level.dat>,
///             bool <saves folder found>
///         )
static std::tuple<bool, QString, bool> contains_level_dat(QDir dir, bool saves = false)
{
    for (auto const& entry : dir.entryInfoList()) {
        if (!entry.isDir()) {
            continue;
        }
        if (!saves && entry.fileName() == "saves") {
            return contains_level_dat(QDir(entry.filePath()), true);
        }
        QFileInfo level_dat(FS::PathCombine(entry.filePath(), "level.dat"));
        if (level_dat.exists() && level_dat.isFile()) {
            return std::make_tuple(true, entry.fileName(), saves);
        }
    }
    return std::make_tuple(false, "", saves);
}

bool processFolder(WorldSave& save, ProcessingLevel level)
{
    Q_ASSERT(save.type() == ResourceType::FOLDER);

    auto [found, save_dir_name, found_saves_dir] = contains_level_dat(QDir(save.fileinfo().filePath()));

    if (!found) {
        return false;
    }

    save.setSaveDirName(save_dir_name);

    if (found_saves_dir) {
        save.setSaveFormat(WorldSaveFormat::MULTI);
    } else {
        save.setSaveFormat(WorldSaveFormat::SINGLE);
    }

    if (level == ProcessingLevel::BasicInfoOnly) {
        return true;  // only need basic info already checked
    }

    // reserved for more intensive processing

    return true;  // all tests passed
}

/// @brief checks a folder structure to see if it contains a level.dat
/// @param zip the zip file to check
/// @return std::tuple of (
///             bool <found level.dat>,
///             QString <name of folder containing level.dat>,
///             bool <saves folder found>
///         )
static std::tuple<bool, QString, bool> contains_level_dat(QuaZip& zip)
{
    bool saves = false;
    QuaZipDir zipDir(&zip);
    if (zipDir.exists("/saves")) {
        saves = true;
        zipDir.cd("/saves");
    }

    for (auto const& entry : zipDir.entryList()) {
        zipDir.cd(entry);
        if (zipDir.exists("level.dat")) {
            return std::make_tuple(true, entry, saves);
        }
        zipDir.cd("..");
    }
    return std::make_tuple(false, "", saves);
}

bool processZIP(WorldSave& save, ProcessingLevel level)
{
    Q_ASSERT(save.type() == ResourceType::ZIPFILE);

    QuaZip zip(save.fileinfo().filePath());
    if (!zip.open(QuaZip::mdUnzip))
        return false;  // can't open zip file

    auto [found, save_dir_name, found_saves_dir] = contains_level_dat(zip);

    if (save_dir_name.endsWith("/")) {
        save_dir_name.chop(1);
    }

    if (!found) {
        return false;
    }

    save.setSaveDirName(save_dir_name);

    if (found_saves_dir) {
        save.setSaveFormat(WorldSaveFormat::MULTI);
    } else {
        save.setSaveFormat(WorldSaveFormat::SINGLE);
    }

    if (level == ProcessingLevel::BasicInfoOnly) {
        zip.close();
        return true;  // only need basic info already checked
    }

    // reserved for more intensive processing

    zip.close();

    return true;
}

bool validate(QFileInfo file)
{
    WorldSave sp{ file };
    return WorldSaveUtils::process(sp, ProcessingLevel::BasicInfoOnly) && sp.valid();
}

}  // namespace WorldSaveUtils

LocalWorldSaveParseTask::LocalWorldSaveParseTask(int token, WorldSave& save) : Task(false), m_token(token), m_save(save) {}

bool LocalWorldSaveParseTask::abort()
{
    m_aborted = true;
    return true;
}

void LocalWorldSaveParseTask::executeTask()
{
    if (!WorldSaveUtils::process(m_save)) {
        emitFailed("this is not a world");
        return;
    }

    if (m_aborted)
        emitAborted();
    else
        emitSucceeded();
}
