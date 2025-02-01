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
#include "FileSystem.h"
#include "MMCZip.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"

ExportPackDialog::ExportPackDialog(InstancePtr instance, QWidget* parent, ModPlatform::ResourceProvider provider)
    : QDialog(parent), m_instance(instance), m_ui(new Ui::ExportPackDialog), m_provider(provider)
{
    Q_ASSERT(m_provider == ModPlatform::ResourceProvider::MODRINTH || m_provider == ModPlatform::ResourceProvider::FLAME);

    m_ui->setupUi(this);
    m_ui->name->setPlaceholderText(instance->name());
    m_ui->name->setText(instance->settings()->get("ExportName").toString());
    m_ui->version->setText(instance->settings()->get("ExportVersion").toString());
    m_ui->optionalFiles->setChecked(instance->settings()->get("ExportOptionalFiles").toBool());

    if (m_provider == ModPlatform::ResourceProvider::MODRINTH) {
        setWindowTitle(tr("Export Modrinth Pack"));

        m_ui->authorLabel->hide();
        m_ui->author->hide();

        m_ui->summary->setPlainText(instance->settings()->get("ExportSummary").toString());
    } else {
        setWindowTitle(tr("Export CurseForge Pack"));

        m_ui->summaryLabel->hide();
        m_ui->summary->hide();

        m_ui->author->setText(instance->settings()->get("ExportAuthor").toString());
    }

    // ensure a valid pack is generated
    // the name and version fields mustn't be empty
    connect(m_ui->name, &QLineEdit::textEdited, this, &ExportPackDialog::validate);
    connect(m_ui->version, &QLineEdit::textEdited, this, &ExportPackDialog::validate);
    // the instance name can technically be empty
    validate();

    QFileSystemModel* model = new QFileSystemModel(this);
    model->setIconProvider(&m_icons);

    // use the game root - everything outside cannot be exported
    const QDir instanceRoot(instance->instanceRoot());
    m_proxy = new FileIgnoreProxy(instance->instanceRoot(), this);
    auto prefix = QDir(instance->instanceRoot()).relativeFilePath(instance->gameRoot());
    for (auto path : { "logs", "crash-reports", ".cache", ".fabric", ".quilt" }) {
        m_proxy->ignoreFilesWithPath().insert(FS::PathCombine(prefix, path));
    }
    m_proxy->ignoreFilesWithName().append({ ".DS_Store", "thumbs.db", "Thumbs.db" });
    m_proxy->setSourceModel(model);
    m_proxy->loadBlockedPathsFromFile(ignoreFileName());

    const QDir::Filters filter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden);

    MinecraftInstance* mcInstance = dynamic_cast<MinecraftInstance*>(instance.get());
    if (mcInstance) {
        for (auto& resourceModel : mcInstance->resourceLists())
            if (resourceModel->indexDir().exists())
                m_proxy->ignoreFilesWithPath().insert(instanceRoot.relativeFilePath(resourceModel->indexDir().absolutePath()));
    }

    m_ui->files->setModel(m_proxy);
    m_ui->files->setRootIndex(m_proxy->mapFromSource(model->index(instance->gameRoot())));
    m_ui->files->sortByColumn(0, Qt::AscendingOrder);

    model->setFilter(filter);
    model->setRootPath(instance->gameRoot());

    QHeaderView* headerView = m_ui->files->header();
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(0, QHeaderView::Stretch);

    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
}

ExportPackDialog::~ExportPackDialog()
{
    delete m_ui;
}

void ExportPackDialog::done(int result)
{
    m_proxy->saveBlockedPathsToFile(ignoreFileName());
    auto settings = m_instance->settings();
    settings->set("ExportName", m_ui->name->text());
    settings->set("ExportVersion", m_ui->version->text());
    settings->set("ExportOptionalFiles", m_ui->optionalFiles->isChecked());

    if (m_provider == ModPlatform::ResourceProvider::MODRINTH)
        settings->set("ExportSummary", m_ui->summary->toPlainText());
    else
        settings->set("ExportAuthor", m_ui->author->text());

    if (result == Accepted) {
        const QString name = m_ui->name->text().isEmpty() ? m_instance->name() : m_ui->name->text();
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
            task = new ModrinthPackExportTask(name, m_ui->version->text(), m_ui->summary->toPlainText(), m_ui->optionalFiles->isChecked(),
                                              m_instance, output, std::bind(&FileIgnoreProxy::filterFile, m_proxy, std::placeholders::_1));
        } else {
            task = new FlamePackExportTask(name, m_ui->version->text(), m_ui->author->text(), m_ui->optionalFiles->isChecked(), m_instance,
                                           output, std::bind(&FileIgnoreProxy::filterFile, m_proxy, std::placeholders::_1));
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
    m_ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setDisabled(m_provider == ModPlatform::ResourceProvider::MODRINTH && m_ui->version->text().isEmpty());
}

QString ExportPackDialog::ignoreFileName()
{
    return FS::PathCombine(m_instance->instanceRoot(), ".packignore");
}
