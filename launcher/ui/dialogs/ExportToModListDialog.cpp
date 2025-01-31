// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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
 */

#include "ExportToModListDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QTextEdit>
#include "DesktopServices.h"
#include "FileSystem.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "modplatform/helpers/ExportToModList.h"
#include "ui_ExportToModListDialog.h"

#include <QFileDialog>
#include <QFileSystemModel>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPushButton>

const QHash<ExportToModList::Formats, QString> ExportToModListDialog::exampleLines = {
    { ExportToModList::HTML, "<li><a href=\"{url}\">{name}</a> [{version}] by {authors}</li>" },
    { ExportToModList::MARKDOWN, "[{name}]({url}) [{version}] by {authors}" },
    { ExportToModList::PLAINTXT, "{name} ({url}) [{version}] by {authors}" },
    { ExportToModList::JSON, "{\"name\":\"{name}\",\"url\":\"{url}\",\"version\":\"{version}\",\"authors\":\"{authors}\"}," },
    { ExportToModList::CSV, "{name},{url},{version},\"{authors}\"" },
};

ExportToModListDialog::ExportToModListDialog(QString name, QList<Mod*> mods, QWidget* parent)
    : QDialog(parent), m_mods(mods), m_template_changed(false), m_name(name), ui(new Ui::ExportToModListDialog)
{
    ui->setupUi(this);
    enableCustom(false);

    connect(ui->formatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportToModListDialog::formatChanged);
    connect(ui->authorsCheckBox, &QCheckBox::stateChanged, this, &ExportToModListDialog::trigger);
    connect(ui->versionCheckBox, &QCheckBox::stateChanged, this, &ExportToModListDialog::trigger);
    connect(ui->urlCheckBox, &QCheckBox::stateChanged, this, &ExportToModListDialog::trigger);
    connect(ui->filenameCheckBox, &QCheckBox::stateChanged, this, &ExportToModListDialog::trigger);
    connect(ui->authorsButton, &QPushButton::clicked, this, [this](bool) { addExtra(ExportToModList::Authors); });
    connect(ui->versionButton, &QPushButton::clicked, this, [this](bool) { addExtra(ExportToModList::Version); });
    connect(ui->urlButton, &QPushButton::clicked, this, [this](bool) { addExtra(ExportToModList::Url); });
    connect(ui->filenameButton, &QPushButton::clicked, this, [this](bool) { addExtra(ExportToModList::FileName); });
    connect(ui->templateText, &QTextEdit::textChanged, this, [this] {
        if (ui->templateText->toPlainText() != exampleLines[m_format])
            ui->formatComboBox->setCurrentIndex(5);
        triggerImp();
    });
    connect(ui->copyButton, &QPushButton::clicked, this, [this](bool) {
        this->ui->finalText->selectAll();
        this->ui->finalText->copy();
    });

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Save)->setText(tr("Save"));
    triggerImp();
    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    }
}

ExportToModListDialog::~ExportToModListDialog()
{
    delete ui;
}

void ExportToModListDialog::formatChanged(int index)
{
    switch (index) {
        case 0: {
            enableCustom(false);
            ui->resultText->show();
            m_format = ExportToModList::HTML;
            break;
        }
        case 1: {
            enableCustom(false);
            ui->resultText->show();
            m_format = ExportToModList::MARKDOWN;
            break;
        }
        case 2: {
            enableCustom(false);
            ui->resultText->hide();
            m_format = ExportToModList::PLAINTXT;
            break;
        }
        case 3: {
            enableCustom(false);
            ui->resultText->hide();
            m_format = ExportToModList::JSON;
            break;
        }
        case 4: {
            enableCustom(false);
            ui->resultText->hide();
            m_format = ExportToModList::CSV;
            break;
        }
        case 5: {
            m_template_changed = true;
            enableCustom(true);
            ui->resultText->hide();
            m_format = ExportToModList::CUSTOM;
            break;
        }
    }
    triggerImp();
}

void ExportToModListDialog::triggerImp()
{
    if (m_format == ExportToModList::CUSTOM) {
        ui->finalText->setPlainText(ExportToModList::exportToModList(m_mods, ui->templateText->toPlainText()));
        return;
    }
    auto opt = 0;
    if (ui->authorsCheckBox->isChecked())
        opt |= ExportToModList::Authors;
    if (ui->versionCheckBox->isChecked())
        opt |= ExportToModList::Version;
    if (ui->urlCheckBox->isChecked())
        opt |= ExportToModList::Url;
    if (ui->filenameCheckBox->isChecked())
        opt |= ExportToModList::FileName;
    auto txt = ExportToModList::exportToModList(m_mods, m_format, static_cast<ExportToModList::OptionalData>(opt));
    ui->finalText->setPlainText(txt);
    switch (m_format) {
        case ExportToModList::CUSTOM:
            return;
        case ExportToModList::HTML:
            ui->resultText->setHtml(StringUtils::htmlListPatch(txt));
            break;
        case ExportToModList::MARKDOWN:
            ui->resultText->setHtml(StringUtils::htmlListPatch(markdownToHTML(txt)));
            break;
        case ExportToModList::PLAINTXT:
            break;
        case ExportToModList::JSON:
            break;
        case ExportToModList::CSV:
            break;
    }
    auto exampleLine = exampleLines[m_format];
    if (!m_template_changed && ui->templateText->toPlainText() != exampleLine)
        ui->templateText->setPlainText(exampleLine);
}

void ExportToModListDialog::done(int result)
{
    if (result == Accepted) {
        const QString filename = FS::RemoveInvalidFilenameChars(m_name);
        const QString output =
            QFileDialog::getSaveFileName(this, tr("Export %1").arg(m_name), FS::PathCombine(QDir::homePath(), filename + extension()),
                                         tr("File") + " (*.txt *.html *.md *.json *.csv)", nullptr);

        if (output.isEmpty())
            return;

        try {
            FS::write(output, ui->finalText->toPlainText().toUtf8());
        } catch (const FS::FileSystemException& e) {
            qCritical() << "Failed to save mod list file :" << e.cause();
        }
    }

    QDialog::done(result);
}

QString ExportToModListDialog::extension()
{
    switch (m_format) {
        case ExportToModList::HTML:
            return ".html";
        case ExportToModList::MARKDOWN:
            return ".md";
        case ExportToModList::PLAINTXT:
            return ".txt";
        case ExportToModList::CUSTOM:
            return ".txt";
        case ExportToModList::JSON:
            return ".json";
        case ExportToModList::CSV:
            return ".csv";
    }
    return ".txt";
}

void ExportToModListDialog::addExtra(ExportToModList::OptionalData option)
{
    if (m_format != ExportToModList::CUSTOM)
        return;
    switch (option) {
        case ExportToModList::Authors:
            ui->templateText->insertPlainText("{authors}");
            break;
        case ExportToModList::Url:
            ui->templateText->insertPlainText("{url}");
            break;
        case ExportToModList::Version:
            ui->templateText->insertPlainText("{version}");
            break;
        case ExportToModList::FileName:
            ui->templateText->insertPlainText("{filename}");
            break;
    }
}
void ExportToModListDialog::enableCustom(bool enabled)
{
    ui->authorsCheckBox->setHidden(enabled);
    ui->authorsButton->setHidden(!enabled);

    ui->versionCheckBox->setHidden(enabled);
    ui->versionButton->setHidden(!enabled);

    ui->urlCheckBox->setHidden(enabled);
    ui->urlButton->setHidden(!enabled);

    ui->filenameCheckBox->setHidden(enabled);
    ui->filenameButton->setHidden(!enabled);
}
