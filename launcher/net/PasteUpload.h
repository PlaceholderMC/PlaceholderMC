// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Lenny McLennington <lenny@sneed.church>
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

#pragma once

#include <QBuffer>
#include <QNetworkReply>
#include <QString>
#include <array>
#include "net/NetRequest.h"

class PasteSink;
class PasteUpload : public Net::NetRequest {
    friend PasteSink;

   public:
    enum PasteType : int {
        // 0x0.st
        NullPointer,
        // hastebin.com
        Hastebin,
        // paste.gg
        PasteGG,
        // mclo.gs
        Mclogs,
        // Helpful to get the range of valid values on the enum for input sanitisation:
        First = NullPointer,
        Last = Mclogs
    };

    struct PasteTypeInfo {
        const QString name;
        const QString defaultBase;
        const QString endpointPath;
    };

    static std::array<PasteTypeInfo, 4> PasteTypes;

    PasteUpload(QString text, QString url, PasteType pasteType);
    virtual ~PasteUpload() = default;

    QString pasteLink() { return m_pasteLink; }

   private:
    virtual QNetworkReply* getReply(QNetworkRequest&) override;

   private:
    QString m_pasteLink;
    QString m_baseUrl;
    PasteType m_pasteType;
    QByteArray m_text;
};
