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
#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>

#include <memory>

#include "FileSystem.h"
#include "net/NetJob.h"
#include "net/PasteUpload.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

#include <BuildConfig.h>
#include <DesktopServices.h>
#include <settings/SettingsObject.h>
#include "Application.h"

constexpr int MaxMclogsLines = 25000;
constexpr int InitialMclogsLines = 10000;
constexpr int FinalMclogsLines = 14900;

QString truncateLogForMclogs(const QString& logContent)
{
    QStringList lines = logContent.split("\n");
    if (lines.size() > MaxMclogsLines) {
        QString truncatedLog = lines.mid(0, InitialMclogsLines).join("\n");
        truncatedLog +=
            "\n\n\n\n\n\n\n\n\n\n"
            "------------------------------------------------------------\n"
            "----------------------- Log truncated ----------------------\n"
            "------------------------------------------------------------\n"
            "----- Middle portion omitted to fit mclo.gs size limits ----\n"
            "------------------------------------------------------------\n"
            "\n\n\n\n\n\n\n\n\n\n";
        truncatedLog += lines.mid(lines.size() - FinalMclogsLines - 1).join("\n");
        return truncatedLog;
    }
    return logContent;
}

std::optional<QString> GuiUtil::uploadPaste(const QString& name, const QFileInfo& filePath, QWidget* parentWidget)
{
    return uploadPaste(name, FS::read(filePath.absoluteFilePath()), parentWidget);
};

std::optional<QString> GuiUtil::uploadPaste(const QString& name, const QString& log, QWidget* parentWidget)
{
    ProgressDialog dialog(parentWidget);
    auto pasteType = static_cast<PasteUpload::PasteType>(APPLICATION->settings()->get("PastebinType").toInt());
    auto baseURL = APPLICATION->settings()->get("PastebinCustomAPIBase").toString();
    bool shouldTruncate = false;

    if (baseURL.isEmpty())
        baseURL = PasteUpload::PasteTypes[pasteType].defaultBase;

    if (auto url = QUrl(baseURL); url.isValid()) {
        auto response = CustomMessageBox::selectable(parentWidget, QObject::tr("Confirm Upload"),
                                                     QObject::tr("You are about to upload \"%1\" to %2.\n"
                                                                 "You should double-check for personal information.\n\n"
                                                                 "Are you sure?")
                                                         .arg(name, url.host()),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return {};

        if (baseURL == "https://api.mclo.gs" && log.count("\n") > MaxMclogsLines) {
            auto truncateResponse = CustomMessageBox::selectable(
                                        parentWidget, QObject::tr("Confirm Truncation"),
                                        QObject::tr("The log has %1 lines, exceeding mclo.gs' limit of %2.\n"
                                                    "The launcher can keep the first %3 and last %4 lines, trimming the middle.\n\n"
                                                    "If you choose 'No', mclo.gs will only keep the first %2 lines, cutting off "
                                                    "potentially useful info like crashes at the end.\n\n"
                                                    "Proceed with truncation?")
                                            .arg(log.count("\n"))
                                            .arg(MaxMclogsLines)
                                            .arg(InitialMclogsLines)
                                            .arg(FinalMclogsLines),
                                        QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No)
                                        ->exec();

            if (truncateResponse == QMessageBox::Cancel) {
                return {};
            }
            shouldTruncate = truncateResponse == QMessageBox::Yes;
        }
    }

    auto result = std::make_shared<PasteUpload::Result>();
    auto job = NetJob::Ptr(new NetJob("Log Upload", APPLICATION->network()));

    job->addNetAction(PasteUpload::make(shouldTruncate ? truncateLogForMclogs(log) : log, pasteType, baseURL, result));
    QObject::connect(job.get(), &Task::failed, [parentWidget](QString reason) {
        CustomMessageBox::selectable(parentWidget, QObject::tr("Failed to upload logs!"), reason, QMessageBox::Critical)->show();
    });
    QObject::connect(job.get(), &Task::aborted, [parentWidget] {
        CustomMessageBox::selectable(parentWidget, QObject::tr("Logs upload aborted"),
                                     QObject::tr("The task has been aborted by the user."), QMessageBox::Information)
            ->show();
    });

    if (dialog.execWithTask(job.get()) == QDialog::Accepted) {
        if (!result->error.isEmpty() || !result->extra_message.isEmpty()) {
            QString message = QObject::tr("Error: %1").arg(result->error);
            if (!result->extra_message.isEmpty()) {
                message += QObject::tr("\nError message: %1").arg(result->extra_message);
            }
            CustomMessageBox::selectable(parentWidget, QObject::tr("Failed to upload logs!"), message, QMessageBox::Critical)->show();
            return {};
        }
        if (result->link.isEmpty()) {
            CustomMessageBox::selectable(parentWidget, QObject::tr("Failed to upload logs!"), "The upload link is empty",
                                         QMessageBox::Critical)
                ->show();
            return {};
        }
        setClipboardText(result->link);
        CustomMessageBox::selectable(
            parentWidget, QObject::tr("Upload finished"),
            QObject::tr("The <a href=\"%1\">link to the uploaded log</a> has been placed in your clipboard.").arg(result->link),
            QMessageBox::Information)
            ->exec();
        return result->link;
    }
    return {};
}

void GuiUtil::setClipboardText(QString text)
{
    for (auto rule : PasteUpload::AnonimizeRules) {
        text.replace(rule.reg, rule.with);
    }
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
