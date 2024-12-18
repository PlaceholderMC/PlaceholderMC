// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Kenneth Chew <79120643+kthchew@users.noreply.github.com>
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

#ifndef XPCMANAGER_H
#define XPCMANAGER_H
#include <string>
#include <QObject>
#include <QUrl>
Q_FORWARD_DECLARE_OBJC_CLASS(XPCManagerInternal);

class XPCManager {
    XPCManagerInternal* m_internal;
public:
    XPCManager();
    ~XPCManager() = default;

    std::pair<bool, std::string> askToRemoveQuarantine(char* path);
    bool removeQuarantineFromMojangJavaDirectory(NSString* path, NSURL* manifestURL);
    bool applyDownloadQuarantineToDirectory(NSString* path);
    QString getUnsandboxedTemporaryDirectory();
};

#endif //XPCMANAGER_H
