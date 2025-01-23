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

#include "VersionSelectDialog.h"

#include <QDebug>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "DesktopServices.h"
#include "ui/widgets/VersionSelectWidget.h"

#include "BaseVersion.h"
#include "BaseVersionList.h"

VersionSelectDialog::VersionSelectDialog(BaseVersionList* vlist, QString title, QWidget* parent, bool cancelable) : QDialog(parent)
{
    setObjectName(QStringLiteral("VersionSelectDialog"));
    resize(400, 347);
    m_verticalLayout = new QVBoxLayout(this);
    m_verticalLayout->setObjectName(QStringLiteral("verticalLayout"));

    m_versionWidget = new VersionSelectWidget(parent);
    m_verticalLayout->addWidget(m_versionWidget);

    m_horizontalLayout = new QHBoxLayout();
    m_horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));

    m_refreshButton = new QPushButton(this);
    m_refreshButton->setObjectName(QStringLiteral("refreshButton"));
    m_horizontalLayout->addWidget(m_refreshButton);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setObjectName(QStringLiteral("buttonBox"));
    m_buttonBox->setOrientation(Qt::Horizontal);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Ok"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_horizontalLayout->addWidget(m_buttonBox);

    m_verticalLayout->addLayout(m_horizontalLayout);

    retranslate();

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_versionWidget->view(), &QAbstractItemView::doubleClicked, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QMetaObject::connectSlotsByName(this);
    setWindowModality(Qt::WindowModal);
    setWindowTitle(title);

    m_vlist = vlist;

    if (!cancelable) {
        m_buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
    }
    if (DesktopServices::isGameScope()) {
        this->showFullScreen();
        this->setFixedSize(this->width(), this->height());
    }
}

void VersionSelectDialog::retranslate()
{
    // FIXME: overrides custom title given in constructor!
    setWindowTitle(tr("Choose Version"));
    m_refreshButton->setToolTip(tr("Reloads the version list."));
    m_refreshButton->setText(tr("&Refresh"));
}

void VersionSelectDialog::setCurrentVersion(const QString& version)
{
    m_currentVersion = version;
    m_versionWidget->setCurrentVersion(version);
}

void VersionSelectDialog::setEmptyString(QString emptyString)
{
    m_versionWidget->setEmptyString(emptyString);
}

void VersionSelectDialog::setEmptyErrorString(QString emptyErrorString)
{
    m_versionWidget->setEmptyErrorString(emptyErrorString);
}

void VersionSelectDialog::setResizeOn(int column)
{
    resizeOnColumn = column;
}

int VersionSelectDialog::exec()
{
    QDialog::open();
    m_versionWidget->initialize(m_vlist, true);
    m_versionWidget->selectSearch();
    if (resizeOnColumn != -1) {
        m_versionWidget->setResizeOn(resizeOnColumn);
    }
    return QDialog::exec();
}

void VersionSelectDialog::selectRecommended()
{
    m_versionWidget->selectRecommended();
}

BaseVersion::Ptr VersionSelectDialog::selectedVersion() const
{
    return m_versionWidget->selectedVersion();
}

void VersionSelectDialog::on_refreshButton_clicked()
{
    m_versionWidget->loadList();
}

void VersionSelectDialog::setExactFilter(BaseVersionList::ModelRoles role, QString filter)
{
    m_versionWidget->setExactFilter(role, filter);
}

void VersionSelectDialog::setExactIfPresentFilter(BaseVersionList::ModelRoles role, QString filter)
{
    m_versionWidget->setExactIfPresentFilter(role, filter);
}

void VersionSelectDialog::setFuzzyFilter(BaseVersionList::ModelRoles role, QString filter)
{
    m_versionWidget->setFuzzyFilter(role, filter);
}
