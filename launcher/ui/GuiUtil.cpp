// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Lenny McLennington <lenny@sneed.church>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2022 TheKodeToad <TheKodeToad@proton.me>
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

#include "GuiUtil.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>

#include "QObjectPtr.h"
#include "net/PasteUpload.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

#include <BuildConfig.h>
#include <DesktopServices.h>
#include <settings/SettingsObject.h>
#include "Application.h"

std::optional<QString> GuiUtil::uploadPaste(const QString& name, const QString& text, QWidget* parentWidget)
{
    ProgressDialog dialog(parentWidget);
    auto pasteTypeSetting = static_cast<PasteUpload::PasteType>(APPLICATION->settings()->get("PastebinType").toInt());
    auto pasteCustomAPIBaseSetting = APPLICATION->settings()->get("PastebinCustomAPIBase").toString();

    {
        QUrl baseUrl;
        if (pasteCustomAPIBaseSetting.isEmpty())
            baseUrl = PasteUpload::PasteTypes[pasteTypeSetting].defaultBase;
        else
            baseUrl = pasteCustomAPIBaseSetting;

        if (baseUrl.isValid()) {
            auto response = CustomMessageBox::selectable(parentWidget, QObject::tr("Confirm Upload"),
                                                         QObject::tr("You are about to upload \"%1\" to %2.\n"
                                                                     "You should double-check for personal information.\n\n"
                                                                     "Are you sure?")
                                                             .arg(name, baseUrl.host()),
                                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                                ->exec();

            if (response != QMessageBox::Yes)
                return {};
        }
    }

    auto paste = makeShared<PasteUpload>(text, pasteCustomAPIBaseSetting, pasteTypeSetting);
    paste->setNetwork(APPLICATION->network());

    dialog.execWithTask(paste.get());
    if (!paste->wasSuccessful()) {
        CustomMessageBox::selectable(parentWidget, QObject::tr("Upload failed"), paste->failReason(), QMessageBox::Critical)->exec();
        return QString();
    } else {
        const QString link = paste->pasteLink();
        setClipboardText(link);
        CustomMessageBox::selectable(
            parentWidget, QObject::tr("Upload finished"),
            QObject::tr("The <a href=\"%1\">link to the uploaded log</a> has been placed in your clipboard.").arg(link),
            QMessageBox::Information)
            ->exec();
        return link;
    }
}

void GuiUtil::setClipboardText(const QString& text)
{
    QApplication::clipboard()->setText(text);
}

static QStringList BrowseForFileInternal(QString context,
                                         QString caption,
                                         QString filter,
                                         QString defaultPath,
                                         QWidget* parentWidget,
                                         bool single)
{
    static QMap<QString, QString> savedPaths;

    QFileDialog w(parentWidget, caption);
    QSet<QString> locations;
    auto f = [&locations](QStandardPaths::StandardLocation l) {
        QString location = QStandardPaths::writableLocation(l);
        QFileInfo finfo(location);
        if (!finfo.exists()) {
            return;
        }
        locations.insert(location);
    };
    f(QStandardPaths::DesktopLocation);
    f(QStandardPaths::DocumentsLocation);
    f(QStandardPaths::DownloadLocation);
    f(QStandardPaths::HomeLocation);
    QList<QUrl> urls;
    for (auto location : locations) {
        urls.append(QUrl::fromLocalFile(location));
    }
    urls.append(QUrl::fromLocalFile(defaultPath));

    w.setFileMode(single ? QFileDialog::ExistingFile : QFileDialog::ExistingFiles);
    w.setAcceptMode(QFileDialog::AcceptOpen);
    w.setNameFilter(filter);

    QString pathToOpen;
    if (savedPaths.contains(context)) {
        pathToOpen = savedPaths[context];
    } else {
        pathToOpen = defaultPath;
    }
    if (!pathToOpen.isEmpty()) {
        QFileInfo finfo(pathToOpen);
        if (finfo.exists() && finfo.isDir()) {
            w.setDirectory(finfo.absoluteFilePath());
        }
    }

    w.setSidebarUrls(urls);

    if (w.exec()) {
        savedPaths[context] = w.directory().absolutePath();
        return w.selectedFiles();
    }
    savedPaths[context] = w.directory().absolutePath();
    return {};
}

QString GuiUtil::BrowseForFile(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget)
{
    auto resultList = BrowseForFileInternal(context, caption, filter, defaultPath, parentWidget, true);
    if (resultList.size()) {
        return resultList[0];
    }
    return QString();
}

QStringList GuiUtil::BrowseForFiles(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget)
{
    return BrowseForFileInternal(context, caption, filter, defaultPath, parentWidget, false);
}
