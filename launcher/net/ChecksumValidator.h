// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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

#include "Validator.h"

#include <QCryptographicHash>
#include <QFile>

namespace Net {
class ChecksumValidator : public Validator {
   public:
    ChecksumValidator(QCryptographicHash::Algorithm algorithm, QString expectedHex)
        : Net::ChecksumValidator(algorithm, QByteArray::fromHex(expectedHex.toLatin1()))
    {}
    ChecksumValidator(QCryptographicHash::Algorithm algorithm, QByteArray expected = QByteArray())
        : m_checksum(algorithm), m_expected(expected) {};
    virtual ~ChecksumValidator() = default;

   public:
    auto init(QNetworkRequest&) -> bool override
    {
        m_checksum.reset();
        return true;
    }

    auto write(QByteArray& data) -> bool override
    {
        m_checksum.addData(data);
        return true;
    }

    auto abort() -> bool override
    {
        m_checksum.reset();
        return true;
    }

    auto validate(QNetworkReply&) -> bool override
    {
        if (m_expected.size() && m_expected != hash()) {
            qCritical() << "Checksum mismatch, download is bad.";
            return false;
        }
        return true;
    }

    auto hash() -> QByteArray { return m_checksum.result(); }

    void setExpected(QByteArray expected) { m_expected = expected; }

   private:
    QCryptographicHash m_checksum;
    QByteArray m_expected;
};
}  // namespace Net
