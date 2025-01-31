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

#include "ExportPackDialog.h"
#include "DesktopServices.h"
#include "minecraft/mod/ModFolderModel.h"
#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlamePackExportTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui_ExportPackDialog.h"

#include <QFileDialog>
#include <QFileSystemModel>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPushButton>
#include "FastFileIconProvider.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"

ExportPackDialog::ExportPackDialog(InstancePtr instance, QWidget* parent, ModPlatform::ResourceProvider provider)
    : QDialog(parent), instance(instance), ui(new Ui::ExportPackDialog), m_provider(provider)
{
    Q_ASSERT(m_provider == ModPlatform::ResourceProvider::MODRINTH || m_provider == ModPlatform::ResourceProvider::FLAME);

    ui->setupUi(this);
    ui->name->setPlaceholderText(instance->name());
    ui->name->setText(instance->settings()->get("ExportName").toString());
    ui->version->setText(instance->settings()->get("ExportVersion").toString());
    ui->optionalFiles->setChecked(instance->settings()->get("ExportOptionalFiles").toBool());

    if (m_provider == ModPlatform::ResourceProvider::MODRINTH) {
        setWindowTitle(tr("Export Modrinth Pack"));

        ui->authorLabel->hide();
        ui->author->hide();

        ui->summary->setPlainText(instance->settings()->get("ExportSummary").toString());
    } else {
        setWindowTitle(tr("Export CurseForge Pack"));

        ui->summaryLabel->hide();
        ui->summary->hide();

        ui->author->setText(instance->settings()->get("ExportAuthor").toString());
    }

    // ensure a valid pack is generated
    // the name and version fields mustn't be empty
    connect(ui->name, &QLineEdit::textEdited, this, &ExportPackDialog::validate);
    connect(ui->version, &QLineEdit::textEdited, this, &ExportPackDialog::validate);
    // the instance name can technically be empty
    validate();

    QFileSystemModel* model = new QFileSystemModel(this);
    model->setIconProvider(&icons);

    // use the game root - everything outside cannot be exported
    const QDir root(instance->gameRoot());
    proxy = new FileIgnoreProxy(instance->gameRoot(), this);
    proxy->ignoreFilesWithPath().insert({ "logs", "crash-reports", ".cache", ".fabric", ".quilt" });
    proxy->ignoreFilesWithName().append({ ".DS_Store", "thumbs.db", "Thumbs.db" });
    proxy->setSourceModel(model);

    const QDir::Filters filter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden);

    for (const QString& file : root.entryList(filter)) {
        if (!(file == "mods" || file == "coremods" || file == "datapacks" || file == "config" || file == "options.txt" ||
              file == "servers.dat"))
            proxy->blockedPaths().insert(file);
    }

    MinecraftInstance* mcInstance = dynamic_cast<MinecraftInstance*>(instance.get());
    if (mcInstance) {
        for (auto& resourceModel : mcInstance->resourceLists())
            if (resourceModel->indexDir().exists())
                proxy->ignoreFilesWithPath().insert(root.relativeFilePath(resourceModel->indexDir().absolutePath()));
    }

    ui->files->setModel(proxy);
    ui->files->setRootIndex(proxy->mapFromSource(model->index(instance->gameRoot())));
    ui->files->sortByColumn(0, Qt::AscendingOrder);

    model->setFilter(filter);
    model->setRootPath(instance->gameRoot());

    QHeaderView* headerView = ui->files->header();
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));

    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    }
}

ExportPackDialog::~ExportPackDialog()
{
    delete ui;
}

void ExportPackDialog::done(int result)
{
    auto settings = instance->settings();
    settings->set("ExportName", ui->name->text());
    settings->set("ExportVersion", ui->version->text());
    settings->set("ExportOptionalFiles", ui->optionalFiles->isChecked());

    if (m_provider == ModPlatform::ResourceProvider::MODRINTH)
        settings->set("ExportSummary", ui->summary->toPlainText());
    else
        settings->set("ExportAuthor", ui->author->text());

    if (result == Accepted) {
        const QString name = ui->name->text().isEmpty() ? instance->name() : ui->name->text();
        const QString filename = FS::RemoveInvalidFilenameChars(name);

        QString output;
        if (m_provider == ModPlatform::ResourceProvider::MODRINTH) {
            output = QFileDialog::getSaveFileName(this, tr("Export %1").arg(name), FS::PathCombine(QDir::homePath(), filename + ".mrpack"),
                                                  tr("Modrinth pack") + " (*.mrpack *.zip)", nullptr);
            if (output.isEmpty())
                return;
            if (!(output.endsWith(".zip") || output.endsWith(".mrpack")))
                output.append(".mrpack");
        } else {
            output = QFileDialog::getSaveFileName(this, tr("Export %1").arg(name), FS::PathCombine(QDir::homePath(), filename + ".zip"),
                                                  tr("CurseForge pack") + " (*.zip)", nullptr);
            if (output.isEmpty())
                return;
            if (!output.endsWith(".zip"))
                output.append(".zip");
        }

        Task* task;
        if (m_provider == ModPlatform::ResourceProvider::MODRINTH) {
            task = new ModrinthPackExportTask(name, ui->version->text(), ui->summary->toPlainText(), ui->optionalFiles->isChecked(),
                                              instance, output, std::bind(&FileIgnoreProxy::filterFile, proxy, std::placeholders::_1));
        } else {
            task = new FlamePackExportTask(name, ui->version->text(), ui->author->text(), ui->optionalFiles->isChecked(), instance, output,
                                           std::bind(&FileIgnoreProxy::filterFile, proxy, std::placeholders::_1));
        }

        connect(task, &Task::failed,
                [this](const QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
        connect(task, &Task::aborted, [this] {
            CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)
                ->show();
        });
        connect(task, &Task::finished, [task] { task->deleteLater(); });

        ProgressDialog progress(this);
        progress.setSkipButton(true, tr("Abort"));
        if (progress.execWithTask(task) != QDialog::Accepted)
            return;
    }

    QDialog::done(result);
}

void ExportPackDialog::validate()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setDisabled(m_provider == ModPlatform::ResourceProvider::MODRINTH && ui->version->text().isEmpty());
}
