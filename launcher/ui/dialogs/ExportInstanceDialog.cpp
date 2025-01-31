// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ExportInstanceDialog.h"
#include <BaseInstance.h>
#include <MMCZip.h>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QMessageBox>
#include "DesktopServices.h"
#include "FileIgnoreProxy.h"
#include "QObjectPtr.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui_ExportInstanceDialog.h"

#include <FileSystem.h>
#include <icons/IconList.h>
#include <QDebug>
#include <QFileInfo>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QStack>
#include <functional>
#include "Application.h"
#include "SeparatorPrefixTree.h"

ExportInstanceDialog::ExportInstanceDialog(InstancePtr instance, QWidget* parent)
    : QDialog(parent), ui(new Ui::ExportInstanceDialog), m_instance(instance)
{
    ui->setupUi(this);
    auto model = new QFileSystemModel(this);
    model->setIconProvider(&icons);
    auto root = instance->instanceRoot();
    proxyModel = new FileIgnoreProxy(root, this);
    proxyModel->setSourceModel(model);
    auto prefix = QDir(instance->instanceRoot()).relativeFilePath(instance->gameRoot());
    proxyModel->ignoreFilesWithPath().insert({ FS::PathCombine(prefix, "logs"), FS::PathCombine(prefix, "crash-reports") });
    proxyModel->ignoreFilesWithName().append({ ".DS_Store", "thumbs.db", "Thumbs.db" });
    proxyModel->ignoreFilesWithPath().insert(
        { FS::PathCombine(prefix, ".cache"), FS::PathCombine(prefix, ".fabric"), FS::PathCombine(prefix, ".quilt") });
    loadPackIgnore();

    ui->treeView->setModel(proxyModel);
    ui->treeView->setRootIndex(proxyModel->mapFromSource(model->index(root)));
    ui->treeView->sortByColumn(0, Qt::AscendingOrder);

    connect(proxyModel, SIGNAL(rowsInserted(QModelIndex, int, int)), SLOT(rowsInserted(QModelIndex, int, int)));

    model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden);
    model->setRootPath(root);
    auto headerView = ui->treeView->header();
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));

    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    }
}

ExportInstanceDialog::~ExportInstanceDialog()
{
    delete ui;
}

/// Save icon to instance's folder is needed
void SaveIcon(InstancePtr m_instance)
{
    auto iconKey = m_instance->iconKey();
    auto iconList = APPLICATION->icons();
    auto mmcIcon = iconList->icon(iconKey);
    if (!mmcIcon || mmcIcon->isBuiltIn()) {
        return;
    }
    auto path = mmcIcon->getFilePath();
    if (!path.isNull()) {
        QFileInfo inInfo(path);
        FS::copy(path, FS::PathCombine(m_instance->instanceRoot(), inInfo.fileName()))();
        return;
    }
    auto& image = mmcIcon->m_images[mmcIcon->type()];
    auto& icon = image.icon;
    auto sizes = icon.availableSizes();
    if (sizes.size() == 0) {
        return;
    }
    auto areaOf = [](QSize size) { return size.width() * size.height(); };
    QSize largest = sizes[0];
    // find variant with largest area
    for (auto size : sizes) {
        if (areaOf(largest) < areaOf(size)) {
            largest = size;
        }
    }
    auto pixmap = icon.pixmap(largest);
    pixmap.save(FS::PathCombine(m_instance->instanceRoot(), iconKey + ".png"));
}

void ExportInstanceDialog::doExport()
{
    auto name = FS::RemoveInvalidFilenameChars(m_instance->name());

    const QString output = QFileDialog::getSaveFileName(this, tr("Export %1").arg(m_instance->name()),
                                                        FS::PathCombine(QDir::homePath(), name + ".zip"), "Zip (*.zip)", nullptr);
    if (output.isEmpty()) {
        QDialog::done(QDialog::Rejected);
        return;
    }

    SaveIcon(m_instance);

    auto files = QFileInfoList();
    if (!MMCZip::collectFileListRecursively(m_instance->instanceRoot(), nullptr, &files,
                                            std::bind(&FileIgnoreProxy::filterFile, proxyModel, std::placeholders::_1))) {
        QMessageBox::warning(this, tr("Error"), tr("Unable to export instance"));
        QDialog::done(QDialog::Rejected);
        return;
    }

    auto task = makeShared<MMCZip::ExportToZipTask>(output, m_instance->instanceRoot(), files, "", true, true);

    connect(task.get(), &Task::failed, this,
            [this, output](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task.get(), &Task::finished, this, [task] { task->deleteLater(); });

    ProgressDialog progress(this);
    progress.setSkipButton(true, tr("Abort"));
    auto result = progress.execWithTask(task.get());
    QDialog::done(result);
}

void ExportInstanceDialog::done(int result)
{
    savePackIgnore();
    if (result == QDialog::Accepted) {
        doExport();
        return;
    }
    QDialog::done(result);
}

void ExportInstanceDialog::rowsInserted(QModelIndex parent, int top, int bottom)
{
    // WARNING: possible off-by-one?
    for (int i = top; i < bottom; i++) {
        auto node = proxyModel->index(i, 0, parent);
        if (proxyModel->shouldExpand(node)) {
            auto expNode = node.parent();
            if (!expNode.isValid()) {
                continue;
            }
            ui->treeView->expand(node);
        }
    }
}

QString ExportInstanceDialog::ignoreFileName()
{
    return FS::PathCombine(m_instance->instanceRoot(), ".packignore");
}

void ExportInstanceDialog::loadPackIgnore()
{
    auto filename = ignoreFileName();
    QFile ignoreFile(filename);
    if (!ignoreFile.open(QIODevice::ReadOnly)) {
        return;
    }
    auto ignoreData = ignoreFile.readAll();
    auto string = QString::fromUtf8(ignoreData);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    proxyModel->setBlockedPaths(string.split('\n', Qt::SkipEmptyParts));
#else
    proxyModel->setBlockedPaths(string.split('\n', QString::SkipEmptyParts));
#endif
}

void ExportInstanceDialog::savePackIgnore()
{
    auto ignoreData = proxyModel->blockedPaths().toStringList().join('\n').toUtf8();
    auto filename = ignoreFileName();
    try {
        FS::write(filename, ignoreData);
    } catch (const Exception& e) {
        qWarning() << e.cause();
    }
}
