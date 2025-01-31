// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *      Copyright 2021 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include "AtlOptionalModDialog.h"
#include "ui_AtlOptionalModDialog.h"

#include <QInputDialog>
#include <QMessageBox>
#include "Application.h"
#include "BuildConfig.h"
#include "Json.h"
#include "modplatform/atlauncher/ATLShareCode.h"

#include "net/ApiDownload.h"

AtlOptionalModListModel::AtlOptionalModListModel(QWidget* parent,
                                                 const ATLauncher::PackVersion& version,
                                                 QVector<ATLauncher::VersionMod> mods)
    : QAbstractListModel(parent), m_version(version), m_mods(mods)
{
    // fill mod index
    for (int i = 0; i < m_mods.size(); i++) {
        auto mod = m_mods.at(i);
        m_index[mod.name] = i;
    }

    // set initial state
    for (int i = 0; i < m_mods.size(); i++) {
        auto mod = m_mods.at(i);
        m_selection[mod.name] = false;
        setMod(mod, i, mod.selected, false);
    }
}

QVector<QString> AtlOptionalModListModel::getResult()
{
    QVector<QString> result;

    for (const auto& mod : m_mods) {
        if (m_selection[mod.name]) {
            result.push_back(mod.name);
        }
    }

    return result;
}

int AtlOptionalModListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_mods.size();
}

int AtlOptionalModListModel::columnCount(const QModelIndex& parent) const
{
    // Enabled, Name, Description
    return parent.isValid() ? 0 : 3;
}

QVariant AtlOptionalModListModel::data(const QModelIndex& index, int role) const
{
    auto row = index.row();
    auto mod = m_mods.at(row);

    if (role == Qt::DisplayRole) {
        if (index.column() == NameColumn) {
            return mod.name;
        }
        if (index.column() == DescriptionColumn) {
            return mod.description;
        }
    } else if (role == Qt::ToolTipRole) {
        if (index.column() == DescriptionColumn) {
            return mod.description;
        }
    } else if (role == Qt::ForegroundRole) {
        if (!mod.colour.isEmpty() && m_version.colours.contains(mod.colour)) {
            return QColor(QString("#%1").arg(m_version.colours[mod.colour]));
        }
    } else if (role == Qt::CheckStateRole) {
        if (index.column() == EnabledColumn) {
            return m_selection[mod.name] ? Qt::Checked : Qt::Unchecked;
        }
    }

    return {};
}

bool AtlOptionalModListModel::setData(const QModelIndex& index, [[maybe_unused]] const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole) {
        auto row = index.row();
        auto mod = m_mods.at(row);

        toggleMod(mod, row);
        return true;
    }

    return false;
}

QVariant AtlOptionalModListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case EnabledColumn:
                return QString();
            case NameColumn:
                return QString("Name");
            case DescriptionColumn:
                return QString("Description");
        }
    }

    return {};
}

Qt::ItemFlags AtlOptionalModListModel::flags(const QModelIndex& index) const
{
    auto flags = QAbstractListModel::flags(index);
    if (index.isValid() && index.column() == EnabledColumn) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

void AtlOptionalModListModel::useShareCode(const QString& code)
{
    m_jobPtr.reset(new NetJob("Atl::Request", APPLICATION->network()));
    auto url = QString(BuildConfig.ATL_API_BASE_URL + "share-codes/" + code);
    m_jobPtr->addNetAction(Net::ApiDownload::makeByteArray(QUrl(url), m_response));

    connect(m_jobPtr.get(), &NetJob::succeeded, this, &AtlOptionalModListModel::shareCodeSuccess);
    connect(m_jobPtr.get(), &NetJob::failed, this, &AtlOptionalModListModel::shareCodeFailure);

    m_jobPtr->start();
}

void AtlOptionalModListModel::shareCodeSuccess()
{
    m_jobPtr.reset();

    QJsonParseError parse_error{};
    auto doc = QJsonDocument::fromJson(*m_response, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from ATL at " << parse_error.offset << " reason: " << parse_error.errorString();
        qWarning() << *m_response;
        return;
    }
    auto obj = doc.object();

    ATLauncher::ShareCodeResponse response;
    try {
        ATLauncher::loadShareCodeResponse(response, obj);
    } catch (const JSONValidationError& e) {
        qDebug() << QString::fromUtf8(*m_response);
        qWarning() << "Error while reading response from ATLauncher: " << e.cause();
        return;
    }

    if (response.error) {
        // fixme: plumb in an error message
        qWarning() << "ATLauncher API Response Error" << response.message;
        return;
    }

    // FIXME: verify pack and version, error if not matching.

    // Clear the current selection
    for (const auto& mod : m_mods) {
        m_selection[mod.name] = false;
    }

    // Make the selections, as per the share code.
    for (const auto& mod : response.data.mods) {
        m_selection[mod.name] = mod.selected;
    }

    emit dataChanged(AtlOptionalModListModel::index(0, EnabledColumn), AtlOptionalModListModel::index(m_mods.size() - 1, EnabledColumn));
}

void AtlOptionalModListModel::shareCodeFailure([[maybe_unused]] const QString& reason)
{
    m_jobPtr.reset();

    // fixme: plumb in an error message
}

void AtlOptionalModListModel::selectRecommended()
{
    for (const auto& mod : m_mods) {
        m_selection[mod.name] = mod.recommended;
    }

    emit dataChanged(AtlOptionalModListModel::index(0, EnabledColumn), AtlOptionalModListModel::index(m_mods.size() - 1, EnabledColumn));
}

void AtlOptionalModListModel::clearAll()
{
    for (const auto& mod : m_mods) {
        m_selection[mod.name] = false;
    }

    emit dataChanged(AtlOptionalModListModel::index(0, EnabledColumn), AtlOptionalModListModel::index(m_mods.size() - 1, EnabledColumn));
}

void AtlOptionalModListModel::toggleMod(const ATLauncher::VersionMod& mod, int index)
{
    auto enable = !m_selection[mod.name];

    // If there is a warning for the mod, display that first (if we would be enabling the mod)
    if (enable && !mod.warning.isEmpty() && m_version.warnings.contains(mod.warning)) {
        auto message = QString("%1<br><br>%2").arg(m_version.warnings[mod.warning], tr("Are you sure that you want to enable this mod?"));

        // fixme: avoid casting here
        auto result = QMessageBox::warning((QWidget*)this->parent(), tr("Warning"), message, QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }
    }

    setMod(mod, index, enable);
}

void AtlOptionalModListModel::setMod(const ATLauncher::VersionMod& mod, int index, bool enable, bool shouldEmit)
{
    if (m_selection[mod.name] == enable)
        return;

    m_selection[mod.name] = enable;

    // disable other mods in the group, if applicable
    if (enable && !mod.group.isEmpty()) {
        for (int i = 0; i < m_mods.size(); i++) {
            if (index == i)
                continue;
            auto other = m_mods.at(i);

            if (mod.group == other.group) {
                setMod(other, i, false, shouldEmit);
            }
        }
    }

    for (const auto& dependencyName : mod.depends) {
        auto dependencyIndex = m_index[dependencyName];
        auto dependencyMod = m_mods.at(dependencyIndex);

        // enable/disable dependencies
        if (enable) {
            setMod(dependencyMod, dependencyIndex, true, shouldEmit);
        }

        // if the dependency is 'effectively hidden', then track which mods
        // depend on it - so we can efficiently disable it when no more dependents
        // depend on it.
        auto dependents = m_dependents[dependencyName];

        if (enable) {
            dependents.append(mod.name);
        } else {
            dependents.removeAll(mod.name);

            // if there are no longer any dependents, let's disable the mod
            if (dependencyMod.effectively_hidden && dependents.isEmpty()) {
                setMod(dependencyMod, dependencyIndex, false, shouldEmit);
            }
        }
    }

    // disable mods that depend on this one, if disabling
    if (!enable) {
        auto dependents = m_dependents[mod.name];
        for (const auto& dependencyName : dependents) {
            auto dependencyIndex = m_index[dependencyName];
            auto dependencyMod = m_mods.at(dependencyIndex);

            setMod(dependencyMod, dependencyIndex, false, shouldEmit);
        }
    }

    if (shouldEmit) {
        emit dataChanged(AtlOptionalModListModel::index(index, EnabledColumn), AtlOptionalModListModel::index(index, EnabledColumn));
    }
}

AtlOptionalModDialog::AtlOptionalModDialog(QWidget* parent, const ATLauncher::PackVersion& version, QVector<ATLauncher::VersionMod> mods)
    : QDialog(parent), ui(new Ui::AtlOptionalModDialog)
{
    ui->setupUi(this);

    listModel = new AtlOptionalModListModel(this, version, mods);
    ui->treeView->setModel(listModel);

    ui->treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->treeView->header()->setSectionResizeMode(AtlOptionalModListModel::NameColumn, QHeaderView::ResizeToContents);
    ui->treeView->header()->setSectionResizeMode(AtlOptionalModListModel::DescriptionColumn, QHeaderView::Stretch);

    connect(ui->shareCodeButton, &QPushButton::clicked, this, &AtlOptionalModDialog::useShareCode);
    connect(ui->selectRecommendedButton, &QPushButton::clicked, listModel, &AtlOptionalModListModel::selectRecommended);
    connect(ui->clearAllButton, &QPushButton::clicked, listModel, &AtlOptionalModListModel::clearAll);
    connect(ui->installButton, &QPushButton::clicked, this, &QDialog::accept);
}

AtlOptionalModDialog::~AtlOptionalModDialog()
{
    delete ui;
}

void AtlOptionalModDialog::useShareCode()
{
    bool ok;
    auto shareCode = QInputDialog::getText(this, tr("Select a share code"), tr("Share code:"), QLineEdit::Normal, "", &ok);

    if (!ok) {
        // If the user cancels the dialog, we don't need to show any error dialogs.
        return;
    }

    if (shareCode.isEmpty()) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("No share code specified!"));
        box.exec();
        return;
    }

    listModel->useShareCode(shareCode);
}
