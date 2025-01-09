// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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
 */

#include "FastFileIconProvider.h"

#include <QApplication>
#include <QStyle>

QIcon FastFileIconProvider::icon(const QFileInfo& info) const
{
    bool link = info.isSymbolicLink() || info.isAlias() || info.isShortcut();
    QStyle::StandardPixmap icon;

    if (info.isDir()) {
        if (link)
            icon = QStyle::SP_DirLinkIcon;
        else
            icon = QStyle::SP_DirIcon;
    } else {
        if (link)
            icon = QStyle::SP_FileLinkIcon;
        else
            icon = QStyle::SP_FileIcon;
    }

    return QApplication::style()->standardIcon(icon);
}