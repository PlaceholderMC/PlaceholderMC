// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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

#include "ProfileSetupDialog.h"
#include "net/RawHeaderProxy.h"
#include "ui_ProfileSetupDialog.h"

#include <QAction>
#include <QDebug>
#include <QJsonDocument>
#include <QPushButton>
#include <QRegularExpressionValidator>

#include "ui/dialogs/ProgressDialog.h"

#include <Application.h>
#include "minecraft/auth/Parsers.h"
#include "net/Upload.h"

ProfileSetupDialog::ProfileSetupDialog(MinecraftAccountPtr accountToSetup, QWidget* parent)
    : QDialog(parent), m_accountToSetup(accountToSetup), ui(new Ui::ProfileSetupDialog)
{
    ui->setupUi(this);
    ui->errorLabel->setVisible(false);

    goodIcon = APPLICATION->getThemedIcon("status-good");
    yellowIcon = APPLICATION->getThemedIcon("status-yellow");
    badIcon = APPLICATION->getThemedIcon("status-bad");

    QRegularExpression permittedNames("[a-zA-Z0-9_]{3,16}");
    auto nameEdit = ui->nameEdit;
    nameEdit->setValidator(new QRegularExpressionValidator(permittedNames));
    nameEdit->setClearButtonEnabled(true);
    validityAction = nameEdit->addAction(yellowIcon, QLineEdit::LeadingPosition);
    connect(nameEdit, &QLineEdit::textEdited, this, &ProfileSetupDialog::nameEdited);

    checkStartTimer.setSingleShot(true);
    connect(&checkStartTimer, &QTimer::timeout, this, &ProfileSetupDialog::startCheck);

    setNameStatus(NameStatus::NotSet, QString());

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
}

ProfileSetupDialog::~ProfileSetupDialog()
{
    delete ui;
}

void ProfileSetupDialog::on_buttonBox_accepted()
{
    setupProfile(currentCheck);
}

void ProfileSetupDialog::on_buttonBox_rejected()
{
    reject();
}

void ProfileSetupDialog::setNameStatus(ProfileSetupDialog::NameStatus status, QString errorString = QString())
{
    nameStatus = status;
    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    switch (nameStatus) {
        case NameStatus::Available: {
            validityAction->setIcon(goodIcon);
            okButton->setEnabled(true);
        } break;
        case NameStatus::NotSet:
        case NameStatus::Pending:
            validityAction->setIcon(yellowIcon);
            okButton->setEnabled(false);
            break;
        case NameStatus::Exists:
        case NameStatus::Error:
            validityAction->setIcon(badIcon);
            okButton->setEnabled(false);
            break;
    }
    if (!errorString.isEmpty()) {
        ui->errorLabel->setText(errorString);
        ui->errorLabel->setVisible(true);
    } else {
        ui->errorLabel->setVisible(false);
    }
}

void ProfileSetupDialog::nameEdited(const QString& name)
{
    if (!ui->nameEdit->hasAcceptableInput()) {
        setNameStatus(NameStatus::NotSet, tr("Name is too short - must be between 3 and 16 characters long."));
        return;
    }
    scheduleCheck(name);
}

void ProfileSetupDialog::scheduleCheck(const QString& name)
{
    queuedCheck = name;
    setNameStatus(NameStatus::Pending);
    checkStartTimer.start(1000);
}

void ProfileSetupDialog::startCheck()
{
    if (isChecking) {
        return;
    }
    if (queuedCheck.isNull()) {
        return;
    }
    checkName(queuedCheck);
}

void ProfileSetupDialog::checkName(const QString& name)
{
    if (isChecking) {
        return;
    }

    currentCheck = name;
    isChecking = true;

    QUrl url(QString("https://api.minecraftservices.com/minecraft/profile/name/%1/available").arg(name));
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_accountToSetup->accessToken()).toUtf8() } };

    m_check_response.reset(new QByteArray());
    if (m_check_task)
        disconnect(m_check_task.get(), nullptr, this, nullptr);
    m_check_task = Net::Download::makeByteArray(url, m_check_response);
    m_check_task->addHeaderProxy(new Net::RawHeaderProxy(headers));

    connect(m_check_task.get(), &Task::finished, this, &ProfileSetupDialog::checkFinished);

    m_check_task->setNetwork(APPLICATION->network());
    m_check_task->start();
}

void ProfileSetupDialog::checkFinished()
{
    if (m_check_task->error() == QNetworkReply::NoError) {
        auto doc = QJsonDocument::fromJson(*m_check_response);
        auto root = doc.object();
        auto statusValue = root.value("status").toString("INVALID");
        if (statusValue == "AVAILABLE") {
            setNameStatus(NameStatus::Available);
        } else if (statusValue == "DUPLICATE") {
            setNameStatus(NameStatus::Exists, tr("Minecraft profile with name %1 already exists.").arg(currentCheck));
        } else if (statusValue == "NOT_ALLOWED") {
            setNameStatus(NameStatus::Exists, tr("The name %1 is not allowed.").arg(currentCheck));
        } else {
            setNameStatus(NameStatus::Error, tr("Unhandled profile name status: %1").arg(statusValue));
        }
    } else {
        setNameStatus(NameStatus::Error, tr("Failed to check name availability."));
    }
    isChecking = false;
}

void ProfileSetupDialog::setupProfile(const QString& profileName)
{
    if (isWorking) {
        return;
    }

    QString payloadTemplate("{\"profileName\":\"%1\"}");

    QUrl url("https://api.minecraftservices.com/minecraft/profile");
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_accountToSetup->accessToken()).toUtf8() } };

    m_profile_response.reset(new QByteArray());
    m_profile_task = Net::Upload::makeByteArray(url, m_profile_response, payloadTemplate.arg(profileName).toUtf8());
    m_profile_task->addHeaderProxy(new Net::RawHeaderProxy(headers));

    connect(m_profile_task.get(), &Task::finished, this, &ProfileSetupDialog::setupProfileFinished);

    m_profile_task->setNetwork(APPLICATION->network());
    m_profile_task->start();

    isWorking = true;

    auto button = ui->buttonBox->button(QDialogButtonBox::Cancel);
    button->setEnabled(false);
}

namespace {

struct MojangError {
    static MojangError fromJSON(QByteArray data)
    {
        MojangError out;
        out.rawError = QString::fromUtf8(data);
        auto doc = QJsonDocument::fromJson(data, &out.parseError);

        out.fullyParsed = false;
        if (!out.parseError.error) {
            auto object = doc.object();
            out.fullyParsed = true;
            out.fullyParsed &= Parsers::getString(object.value("path"), out.path);
            out.fullyParsed &= Parsers::getString(object.value("error"), out.error);
            out.fullyParsed &= Parsers::getString(object.value("errorMessage"), out.errorMessage);
        }

        return out;
    }

    QString rawError;
    QJsonParseError parseError;
    bool fullyParsed;

    QString path;
    QString error;
    QString errorMessage;
};

}  // namespace

void ProfileSetupDialog::setupProfileFinished()
{
    isWorking = false;
    if (m_profile_task->error() == QNetworkReply::NoError) {
        /*
         * data contains the profile in the response
         * ... we could parse it and update the account, but let's just return back to the normal login flow instead...
         */
        accept();
    } else {
        auto parsedError = MojangError::fromJSON(*m_profile_response);
        ui->errorLabel->setVisible(true);

        QString errorMessage = tr("Network error(%1): %2 ;HTTP Status: %3")
                                   .arg(QString::number(m_profile_task->error()), m_profile_task->errorString(),
                                        QString::number(m_profile_task->replyStatusCode()));

        if (parsedError.fullyParsed) {
            errorMessage += "Path: " + parsedError.path + "\n";
            errorMessage += "Error: " + parsedError.error + "\n";
            errorMessage += "Error message: " + parsedError.errorMessage + "\n";
        } else {
            errorMessage += "Failed to parse Mojang error: " + parsedError.parseError.errorString() + "\n";
            errorMessage += "Raw contents:\n" + parsedError.rawError + "\n";
        }

        ui->errorLabel->setText(tr("The server returned the following error:") + "\n\n" + errorMessage);
        qDebug() << parsedError.rawError;
        auto button = ui->buttonBox->button(QDialogButtonBox::Cancel);
        button->setEnabled(true);
    }
}
