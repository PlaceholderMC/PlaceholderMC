// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2023 flowln <flowlnlnln@gmail.com>
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

#pragma once

#include <QDebug>
#include <QList>
#include <QString>
#include <QStringView>

class QUrl;

class Version {
   public:
    Version(QString str);
    Version() = default;

    bool operator<(const Version& other) const;
    bool operator<=(const Version& other) const;
    bool operator>(const Version& other) const;
    bool operator>=(const Version& other) const;
    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;

    QString toString() const { return m_string; }
    bool isEmpty() const { return m_string.isEmpty(); }

    friend QDebug operator<<(QDebug debug, const Version& v);

   private:
    struct Section {
        explicit Section(QString fullString) : m_fullString(std::move(fullString))
        {
            qsizetype cutoff = m_fullString.size();
            for (int i = 0; i < m_fullString.size(); i++) {
                if (!m_fullString[i].isDigit()) {
                    cutoff = i;
                    break;
                }
            }

            auto numPart = QStringView{ m_fullString }.left(cutoff);

            if (!numPart.isEmpty()) {
                m_isNull = false;
                m_numPart = numPart.toInt();
            }

            auto stringPart = QStringView{ m_fullString }.mid(cutoff);

            if (!stringPart.isEmpty()) {
                m_isNull = false;
                m_stringPart = stringPart.toString();
            }
        }

        explicit Section() = default;

        bool m_isNull = true;

        int m_numPart = 0;
        QString m_stringPart;

        QString m_fullString;

        [[nodiscard]] inline bool isAppendix() const { return m_stringPart.startsWith('+'); }
        [[nodiscard]] inline bool isPreRelease() const { return m_stringPart.startsWith('-') && m_stringPart.length() > 1; }

        inline bool operator==(const Section& other) const
        {
            if (m_isNull && !other.m_isNull)
                return false;
            if (!m_isNull && other.m_isNull)
                return false;

            if (!m_isNull && !other.m_isNull) {
                return (m_numPart == other.m_numPart) && (m_stringPart == other.m_stringPart);
            }

            return true;
        }

        inline bool operator<(const Section& other) const
        {
            static auto unequal_is_less = [](Section const& non_null) -> bool {
                if (non_null.m_stringPart.isEmpty())
                    return non_null.m_numPart == 0;
                return (non_null.m_stringPart != QLatin1Char('.')) && non_null.isPreRelease();
            };

            if (!m_isNull && other.m_isNull)
                return unequal_is_less(*this);
            if (m_isNull && !other.m_isNull)
                return !unequal_is_less(other);

            if (!m_isNull && !other.m_isNull) {
                if (m_numPart < other.m_numPart)
                    return true;
                if (m_numPart == other.m_numPart && m_stringPart < other.m_stringPart)
                    return true;

                if (!m_stringPart.isEmpty() && other.m_stringPart.isEmpty())
                    return false;
                if (m_stringPart.isEmpty() && !other.m_stringPart.isEmpty())
                    return true;

                return false;
            }

            return m_fullString < other.m_fullString;
        }

        inline bool operator!=(const Section& other) const { return !(*this == other); }
        inline bool operator>(const Section& other) const { return !(*this < other || *this == other); }
    };

   private:
    QString m_string;
    QList<Section> m_sections;

    void parse();
};
