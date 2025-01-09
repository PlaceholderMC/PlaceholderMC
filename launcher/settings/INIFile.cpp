// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 flowln <flowlnlnln@gmail.com>
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

#include "settings/INIFile.h"
#include <FileSystem.h>

#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

#include <QSettings>

INIFile::INIFile() {}

bool INIFile::saveFile(QString fileName)
{
    if (!contains("ConfigVersion"))
        insert("ConfigVersion", "1.2");
    QSettings _settings_obj{ fileName, QSettings::Format::IniFormat };
    _settings_obj.setFallbacksEnabled(false);
    _settings_obj.clear();

    for (Iterator iter = begin(); iter != end(); iter++)
        _settings_obj.setValue(iter.key(), iter.value());

    _settings_obj.sync();

    if (auto status = _settings_obj.status(); status != QSettings::Status::NoError) {
        // Shouldn't be possible!
        Q_ASSERT(status != QSettings::Status::FormatError);

        if (status == QSettings::Status::AccessError)
            qCritical() << "An access error occurred (e.g. trying to write to a read-only file).";

        return false;
    }

    return true;
}

QString unescape(QString orig)
{
    QString out;
    QChar prev = QChar::Null;
    for (auto c : orig) {
        if (prev == '\\') {
            if (c == 'n')
                out += '\n';
            else if (c == 't')
                out += '\t';
            else if (c == '#')
                out += '#';
            else
                out += c;
            prev = QChar::Null;
        } else {
            if (c == '\\') {
                prev = c;
                continue;
            }
            out += c;
            prev = QChar::Null;
        }
    }
    return out;
}

QString unquote(QString str)
{
    if ((str.contains(QChar(';')) || str.contains(QChar('=')) || str.contains(QChar(','))) && str.endsWith("\"") && str.startsWith("\"")) {
        str = str.mid(1, str.length() - 2);
    }
    return str;
}

bool parseOldFileFormat(QIODevice& device, QSettings::SettingsMap& map)
{
    QTextStream in(device.readAll());

    QStringList lines = in.readAll().split('\n');
    for (int i = 0; i < lines.count(); i++) {
        QString& lineRaw = lines[i];
        // Ignore comments.
        int commentIndex = 0;
        QString line = lineRaw;
        // Search for comments until no more escaped # are available
        while ((commentIndex = line.indexOf('#', commentIndex + 1)) != -1) {
            if (commentIndex > 0 && line.at(commentIndex - 1) == '\\') {
                continue;
            }
            line = line.left(lineRaw.indexOf('#')).trimmed();
        }

        int eqPos = line.indexOf('=');
        if (eqPos == -1)
            continue;
        QString key = line.left(eqPos).trimmed();
        QString valueStr = line.right(line.length() - eqPos - 1).trimmed();

        valueStr = unquote(unescape(valueStr));

        QVariant value(valueStr);
        map.insert(key, value);
    }

    return true;
}

bool INIFile::loadFile(QString fileName)
{
    QSettings _settings_obj{ fileName, QSettings::Format::IniFormat };
    _settings_obj.setFallbacksEnabled(false);

    if (auto status = _settings_obj.status(); status != QSettings::Status::NoError) {
        if (status == QSettings::Status::AccessError)
            qCritical() << "An access error occurred (e.g. trying to write to a read-only file).";
        if (status == QSettings::Status::FormatError)
            qCritical() << "A format error occurred (e.g. loading a malformed INI file).";
        return false;
    }
    if (!_settings_obj.value("ConfigVersion").isValid()) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QSettings::SettingsMap map;
        parseOldFileFormat(file, map);
        file.close();
        for (auto&& key : map.keys())
            insert(key, map.value(key));
        insert("ConfigVersion", "1.2");
    } else if (_settings_obj.value("ConfigVersion").toString() == "1.1") {
        for (auto&& key : _settings_obj.allKeys()) {
            if (auto valueStr = _settings_obj.value(key).toString();
                (valueStr.contains(QChar(';')) || valueStr.contains(QChar('=')) || valueStr.contains(QChar(','))) &&
                valueStr.endsWith("\"") && valueStr.startsWith("\"")) {
                insert(key, unquote(valueStr));
            } else
                insert(key, _settings_obj.value(key));
        }
        insert("ConfigVersion", "1.2");
    } else
        for (auto&& key : _settings_obj.allKeys())
            insert(key, _settings_obj.value(key));
    return true;
}

bool INIFile::loadFile(QByteArray data)
{
    QTemporaryFile file;
    if (!file.open())
        return false;
    file.write(data);
    file.flush();
    file.close();
    auto loaded = loadFile(file.fileName());
    file.remove();
    return loaded;
}

QVariant INIFile::get(QString key, QVariant def) const
{
    if (!this->contains(key))
        return def;
    else
        return this->operator[](key);
}

void INIFile::set(QString key, QVariant val)
{
    this->operator[](key) = val;
}
