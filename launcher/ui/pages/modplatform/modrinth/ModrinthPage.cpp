// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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
 *      Copyright 2021-2022 kb1000
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

#include "ModrinthPage.h"
#include "Version.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_ModrinthPage.h"

#include "ModrinthModel.h"

#include "BuildConfig.h"
#include "InstanceImportTask.h"
#include "Json.h"
#include "Markdown.h"
#include "StringUtils.h"

#include "ui/widgets/ProjectItem.h"

#include "net/ApiDownload.h"

#include <QComboBox>
#include <QKeyEvent>
#include <QPushButton>

ModrinthPage::ModrinthPage(NewInstanceDialog* dialog, QWidget* parent)
    : QWidget(parent), ui(new Ui::ModrinthPage), dialog(dialog), m_fetch_progress(this, false)
{
    ui->setupUi(this);
    createFilterWidget();

    ui->searchEdit->installEventFilter(this);
    m_model = new Modrinth::ModpackListModel(this);
    ui->packView->setModel(m_model);

    ui->versionSelectionBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->versionSelectionBox->view()->parentWidget()->setMaximumHeight(300);

    m_search_timer.setTimerType(Qt::TimerType::CoarseTimer);
    m_search_timer.setSingleShot(true);

    connect(&m_search_timer, &QTimer::timeout, this, &ModrinthPage::triggerSearch);

    m_fetch_progress.hideIfInactive(true);
    m_fetch_progress.setFixedHeight(24);
    m_fetch_progress.progressFormat("");

    ui->verticalLayout->insertWidget(1, &m_fetch_progress);

    ui->sortByBox->addItem(tr("Sort by Relevance"));
    ui->sortByBox->addItem(tr("Sort by Total Downloads"));
    ui->sortByBox->addItem(tr("Sort by Follows"));
    ui->sortByBox->addItem(tr("Sort by Newest"));
    ui->sortByBox->addItem(tr("Sort by Last Updated"));

    connect(ui->sortByBox, SIGNAL(currentIndexChanged(int)), this, SLOT(triggerSearch()));
    connect(ui->packView->selectionModel(), &QItemSelectionModel::currentChanged, this, &ModrinthPage::onSelectionChanged);
    connect(ui->versionSelectionBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModrinthPage::onVersionSelectionChanged);

    ui->packView->setItemDelegate(new ProjectItemDelegate(this));
    ui->packDescription->setMetaEntry(metaEntryBase());
}

ModrinthPage::~ModrinthPage()
{
    delete ui;
}

void ModrinthPage::retranslate()
{
    ui->retranslateUi(this);
}

void ModrinthPage::openedImpl()
{
    BasePage::openedImpl();
    suggestCurrent();
    triggerSearch();
}

bool ModrinthPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = reinterpret_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return) {
            this->triggerSearch();
            keyEvent->accept();
            return true;
        } else {
            if (m_search_timer.isActive())
                m_search_timer.stop();

            m_search_timer.start(350);
        }
    }
    return QObject::eventFilter(watched, event);
}

bool checkVersionFilters(const Modrinth::ModpackVersion& v, std::shared_ptr<ModFilterWidget::Filter> filter)
{
    if (!filter)
        return true;
    return ((!filter->loaders || !v.loaders || filter->loaders & v.loaders) &&  // loaders
            (filter->releases.empty() ||                                        // releases
             std::find(filter->releases.cbegin(), filter->releases.cend(), v.version_type) != filter->releases.cend()) &&
            filter->checkMcVersions({ v.gameVersion }));  // gameVersion}
}

void ModrinthPage::onSelectionChanged(QModelIndex curr, [[maybe_unused]] QModelIndex prev)
{
    ui->versionSelectionBox->clear();

    if (!curr.isValid()) {
        if (isOpened) {
            dialog->setSuggestedPack();
        }
        return;
    }

    current = m_model->data(curr, Qt::UserRole).value<Modrinth::Modpack>();
    auto name = current.name;

    if (!current.extraInfoLoaded) {
        qDebug() << "Loading modrinth modpack information";

        auto netJob = new NetJob(QString("Modrinth::PackInformation(%1)").arg(current.name), APPLICATION->network());
        auto response = std::make_shared<QByteArray>();

        QString id = current.id;

        netJob->addNetAction(Net::ApiDownload::makeByteArray(QString("%1/project/%2").arg(BuildConfig.MODRINTH_PROD_URL, id), response));

        QObject::connect(netJob, &NetJob::succeeded, this, [this, response, id, curr] {
            if (id != current.id) {
                return;  // wrong request?
            }

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
            if (parse_error.error != QJsonParseError::NoError) {
                qWarning() << "Error while parsing JSON response from Modrinth at " << parse_error.offset
                           << " reason: " << parse_error.errorString();
                qWarning() << *response;
                return;
            }

            auto obj = Json::requireObject(doc);

            try {
                Modrinth::loadIndexedInfo(current, obj);
            } catch (const JSONValidationError& e) {
                qDebug() << *response;
                qWarning() << "Error while reading modrinth modpack version: " << e.cause();
            }

            updateUI();

            QVariant current_updated;
            current_updated.setValue(current);

            if (!m_model->setData(curr, current_updated, Qt::UserRole))
                qWarning() << "Failed to cache extra info for the current pack!";

            suggestCurrent();
        });
        QObject::connect(netJob, &NetJob::finished, this, [response, netJob] { netJob->deleteLater(); });
        connect(netJob, &NetJob::failed,
                [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });
        netJob->start();
    } else
        updateUI();

    if (!current.versionsLoaded || m_filterWidget->changed()) {
        qDebug() << "Loading modrinth modpack versions";

        auto netJob = new NetJob(QString("Modrinth::PackVersions(%1)").arg(current.name), APPLICATION->network());
        auto response = std::make_shared<QByteArray>();

        QString id = current.id;

        netJob->addNetAction(
            Net::ApiDownload::makeByteArray(QString("%1/project/%2/version").arg(BuildConfig.MODRINTH_PROD_URL, id), response));

        QObject::connect(netJob, &NetJob::succeeded, this, [this, response, id, curr] {
            if (id != current.id) {
                return;  // wrong request?
            }

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
            if (parse_error.error != QJsonParseError::NoError) {
                qWarning() << "Error while parsing JSON response from Modrinth at " << parse_error.offset
                           << " reason: " << parse_error.errorString();
                qWarning() << *response;
                return;
            }

            try {
                Modrinth::loadIndexedVersions(current, doc);
            } catch (const JSONValidationError& e) {
                qDebug() << *response;
                qWarning() << "Error while reading modrinth modpack version: " << e.cause();
            }
            auto pred = [this](const Modrinth::ModpackVersion& v) { return !checkVersionFilters(v, m_filterWidget->getFilter()); };
            current.versions.removeIf(pred);
            for (auto version : current.versions) {
                auto release_type = version.version_type.isValid() ? QString(" [%1]").arg(version.version_type.toString()) : "";
                auto mcVersion = !version.gameVersion.isEmpty() && !version.name.contains(version.gameVersion)
                                     ? QString(" for %1").arg(version.gameVersion)
                                     : "";
                auto versionStr = !version.name.contains(version.version) ? version.version : "";
                ui->versionSelectionBox->addItem(QString("%1%2 — %3%4").arg(version.name, mcVersion, versionStr, release_type),
                                                 QVariant(version.id));
            }

            QVariant current_updated;
            current_updated.setValue(current);

            if (!m_model->setData(curr, current_updated, Qt::UserRole))
                qWarning() << "Failed to cache versions for the current pack!";

            suggestCurrent();
        });
        QObject::connect(netJob, &NetJob::finished, this, [response, netJob] { netJob->deleteLater(); });
        connect(netJob, &NetJob::failed,
                [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->exec(); });
        netJob->start();

    } else {
        for (auto version : current.versions) {
            if (!version.name.contains(version.version))
                ui->versionSelectionBox->addItem(QString("%1 - %2").arg(version.name, version.version), QVariant(version.id));
            else
                ui->versionSelectionBox->addItem(version.name, QVariant(version.id));
        }

        suggestCurrent();
    }
}

void ModrinthPage::updateUI()
{
    QString text = "";

    if (current.extra.projectUrl.isEmpty())
        text = current.name;
    else
        text = "<a href=\"" + current.extra.projectUrl + "\">" + current.name + "</a>";

    // TODO: Implement multiple authors with links
    text += "<br>" + tr(" by ") + QString("<a href=%1>%2</a>").arg(std::get<1>(current.author).toString(), std::get<0>(current.author));

    if (current.extraInfoLoaded) {
        if (current.extra.status == "archived") {
            text += "<br><br>" + tr("<b>This project has been archived. It will not receive any further updates unless the author decides "
                                    "to unarchive the project.</b>");
        }

        if (!current.extra.donate.isEmpty()) {
            text += "<br><br>" + tr("Donate information: ");
            auto donateToStr = [](Modrinth::DonationData& donate) -> QString {
                return QString("<a href=\"%1\">%2</a>").arg(donate.url, donate.platform);
            };
            QStringList donates;
            for (auto& donate : current.extra.donate) {
                donates.append(donateToStr(donate));
            }
            text += donates.join(", ");
        }

        if (!current.extra.issuesUrl.isEmpty() || !current.extra.sourceUrl.isEmpty() || !current.extra.wikiUrl.isEmpty() ||
            !current.extra.discordUrl.isEmpty()) {
            text += "<br><br>" + tr("External links:") + "<br>";
        }

        if (!current.extra.issuesUrl.isEmpty())
            text += "- " + tr("Issues: <a href=%1>%1</a>").arg(current.extra.issuesUrl) + "<br>";
        if (!current.extra.wikiUrl.isEmpty())
            text += "- " + tr("Wiki: <a href=%1>%1</a>").arg(current.extra.wikiUrl) + "<br>";
        if (!current.extra.sourceUrl.isEmpty())
            text += "- " + tr("Source code: <a href=%1>%1</a>").arg(current.extra.sourceUrl) + "<br>";
        if (!current.extra.discordUrl.isEmpty())
            text += "- " + tr("Discord: <a href=%1>%1</a>").arg(current.extra.discordUrl) + "<br>";
    }

    text += "<hr>";

    text += markdownToHTML(current.extra.body.toUtf8());

    ui->packDescription->setHtml(StringUtils::htmlListPatch(text + current.description));
    ui->packDescription->flush();
}

void ModrinthPage::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (selectedVersion.isEmpty()) {
        dialog->setSuggestedPack();
        return;
    }

    for (auto& ver : current.versions) {
        if (ver.id == selectedVersion) {
            QMap<QString, QString> extra_info;
            extra_info.insert("pack_id", current.id);
            extra_info.insert("pack_version_id", ver.id);

            dialog->setSuggestedPack(current.name, ver.version, new InstanceImportTask(ver.download_url, this, std::move(extra_info)));
            auto iconName = current.iconName;
            m_model->getLogo(iconName, current.iconUrl.toString(),
                             [this, iconName](QString logo) { dialog->setSuggestedIconFromFile(logo, iconName); });

            break;
        }
    }
}

void ModrinthPage::triggerSearch()
{
    ui->packView->selectionModel()->setCurrentIndex({}, QItemSelectionModel::SelectionFlag::ClearAndSelect);
    ui->packView->clearSelection();
    ui->packDescription->clear();
    ui->versionSelectionBox->clear();
    m_model->searchWithTerm(ui->searchEdit->text(), ui->sortByBox->currentIndex(), m_filterWidget->getFilter(), m_filterWidget->changed());
    m_fetch_progress.watch(m_model->activeSearchJob().get());
}

void ModrinthPage::onVersionSelectionChanged(int index)
{
    if (index == -1) {
        selectedVersion = "";
        return;
    }
    selectedVersion = ui->versionSelectionBox->itemData(index).toString();
    suggestCurrent();
}

void ModrinthPage::setSearchTerm(QString term)
{
    ui->searchEdit->setText(term);
}

QString ModrinthPage::getSerachTerm() const
{
    return ui->searchEdit->text();
}

void ModrinthPage::createFilterWidget()
{
    auto widget = ModFilterWidget::create(nullptr, true, this);
    m_filterWidget.swap(widget);
    auto old = ui->splitter->replaceWidget(0, m_filterWidget.get());
    // because we replaced the widget we also need to delete it
    if (old) {
        delete old;
    }

    connect(ui->filterButton, &QPushButton::clicked, this, [this] { m_filterWidget->setHidden(!m_filterWidget->isHidden()); });

    connect(m_filterWidget.get(), &ModFilterWidget::filterChanged, this, &ModrinthPage::triggerSearch);
    auto response = std::make_shared<QByteArray>();
    m_categoriesTask = ModrinthAPI::getModCategories(response);
    QObject::connect(m_categoriesTask.get(), &Task::succeeded, [this, response]() {
        auto categories = ModrinthAPI::loadCategories(response, "modpack");
        m_filterWidget->setCategories(categories);
    });
    m_categoriesTask->start();
}