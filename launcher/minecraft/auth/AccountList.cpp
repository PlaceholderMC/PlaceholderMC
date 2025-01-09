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

#include "AccountList.h"
#include "AccountData.h"
#include "tasks/Task.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>
#include <QTimer>

#include <QDebug>

#include <FileSystem.h>
#include <QSaveFile>

enum AccountListVersion { MojangMSA = 3 };

AccountList::AccountList(QObject* parent) : QAbstractListModel(parent)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &AccountList::fillQueue);
    m_nextTimer = new QTimer(this);
    m_nextTimer->setSingleShot(true);
    connect(m_nextTimer, &QTimer::timeout, this, &AccountList::tryNext);
}

AccountList::~AccountList() noexcept {}

int AccountList::findAccountByProfileId(const QString& profileId) const
{
    for (int i = 0; i < count(); i++) {
        MinecraftAccountPtr account = at(i);
        if (account->profileId() == profileId) {
            return i;
        }
    }
    return -1;
}

MinecraftAccountPtr AccountList::getAccountByProfileName(const QString& profileName) const
{
    for (int i = 0; i < count(); i++) {
        MinecraftAccountPtr account = at(i);
        if (account->profileName() == profileName) {
            return account;
        }
    }
    return nullptr;
}

const MinecraftAccountPtr AccountList::at(int i) const
{
    return MinecraftAccountPtr(m_accounts.at(i));
}

QStringList AccountList::profileNames() const
{
    QStringList out;
    for (auto& account : m_accounts) {
        auto profileName = account->profileName();
        if (profileName.isEmpty()) {
            continue;
        }
        out.append(profileName);
    }
    return out;
}

void AccountList::addAccount(const MinecraftAccountPtr account)
{
    // NOTE: Do not allow adding something that's already there. We shouldn't let it continue
    // because of the signal / slot connections after this.
    if (m_accounts.contains(account)) {
        qDebug() << "Tried to add account that's already on the accounts list!";
        return;
    }

    // hook up notifications for changes in the account
    connect(account.get(), &MinecraftAccount::changed, this, &AccountList::accountChanged);
    connect(account.get(), &MinecraftAccount::activityChanged, this, &AccountList::accountActivityChanged);

    // override/replace existing account with the same profileId
    auto profileId = account->profileId();
    if (profileId.size()) {
        auto existingAccount = findAccountByProfileId(profileId);
        if (existingAccount != -1) {
            qDebug() << "Replacing old account with a new one with the same profile ID!";

            MinecraftAccountPtr existingAccountPtr = m_accounts[existingAccount];
            m_accounts[existingAccount] = account;
            if (m_defaultAccount == existingAccountPtr) {
                m_defaultAccount = account;
            }
            // disconnect notifications for changes in the account being replaced
            existingAccountPtr->disconnect(this);
            emit dataChanged(index(existingAccount), index(existingAccount, columnCount(QModelIndex()) - 1));
            onListChanged();
            return;
        }
    }

    // if we don't have this profileId yet, add the account to the end
    int row = m_accounts.count();
    qDebug() << "Inserting account at index" << row;

    beginInsertRows(QModelIndex(), row, row);
    m_accounts.append(account);
    endInsertRows();

    onListChanged();
}

void AccountList::removeAccount(QModelIndex index)
{
    int row = index.row();
    if (index.isValid() && row >= 0 && row < m_accounts.size()) {
        auto& account = m_accounts[row];
        if (account == m_defaultAccount) {
            m_defaultAccount = nullptr;
            onDefaultAccountChanged();
        }
        account->disconnect(this);

        beginRemoveRows(QModelIndex(), row, row);
        m_accounts.removeAt(index.row());
        endRemoveRows();
        onListChanged();
    }
}

MinecraftAccountPtr AccountList::defaultAccount() const
{
    return m_defaultAccount;
}

void AccountList::setDefaultAccount(MinecraftAccountPtr newAccount)
{
    if (!newAccount && m_defaultAccount) {
        int idx = 0;
        auto previousDefaultAccount = m_defaultAccount;
        m_defaultAccount = nullptr;
        for (MinecraftAccountPtr account : m_accounts) {
            if (account == previousDefaultAccount) {
                emit dataChanged(index(idx), index(idx, columnCount(QModelIndex()) - 1));
            }
            idx++;
        }
        onDefaultAccountChanged();
    } else {
        auto currentDefaultAccount = m_defaultAccount;
        int currentDefaultAccountIdx = -1;
        auto newDefaultAccount = m_defaultAccount;
        int newDefaultAccountIdx = -1;
        int idx = 0;
        for (MinecraftAccountPtr account : m_accounts) {
            if (account == newAccount) {
                newDefaultAccount = account;
                newDefaultAccountIdx = idx;
            }
            if (currentDefaultAccount == account) {
                currentDefaultAccountIdx = idx;
            }
            idx++;
        }
        if (currentDefaultAccount != newDefaultAccount) {
            emit dataChanged(index(currentDefaultAccountIdx), index(currentDefaultAccountIdx, columnCount(QModelIndex()) - 1));
            emit dataChanged(index(newDefaultAccountIdx), index(newDefaultAccountIdx, columnCount(QModelIndex()) - 1));
            m_defaultAccount = newDefaultAccount;
            onDefaultAccountChanged();
        }
    }
}

void AccountList::accountChanged()
{
    // the list changed. there is no doubt.
    onListChanged();
}

void AccountList::accountActivityChanged(bool active)
{
    MinecraftAccount* account = qobject_cast<MinecraftAccount*>(sender());
    bool found = false;
    for (int i = 0; i < count(); i++) {
        if (at(i).get() == account) {
            emit dataChanged(index(i), index(i, columnCount(QModelIndex()) - 1));
            found = true;
            break;
        }
    }
    if (found) {
        emit listActivityChanged();
        if (active) {
            beginActivity();
        } else {
            endActivity();
        }
    }
}

void AccountList::onListChanged()
{
    if (m_autosave)
        // TODO: Alert the user if this fails.
        saveList();

    emit listChanged();
}

void AccountList::onDefaultAccountChanged()
{
    if (m_autosave)
        saveList();

    emit defaultAccountChanged();
}

int AccountList::count() const
{
    return m_accounts.count();
}

QString getAccountStatus(AccountState status)
{
    switch (status) {
        case AccountState::Unchecked:
            return QObject::tr("Unchecked", "Account status");
        case AccountState::Offline:
            return QObject::tr("Offline", "Account status");
        case AccountState::Online:
            return QObject::tr("Ready", "Account status");
        case AccountState::Working:
            return QObject::tr("Working", "Account status");
        case AccountState::Errored:
            return QObject::tr("Errored", "Account status");
        case AccountState::Expired:
            return QObject::tr("Expired", "Account status");
        case AccountState::Disabled:
            return QObject::tr("Disabled", "Account status");
        case AccountState::Gone:
            return QObject::tr("Gone", "Account status");
        default:
            return QObject::tr("Unknown", "Account status");
    }
}

QVariant AccountList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() > count())
        return QVariant();

    MinecraftAccountPtr account = at(index.row());

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ProfileNameColumn:
                    return account->profileName();
                case NameColumn:
                    return account->accountDisplayString();
                case TypeColumn: {
                    switch (account->accountType()) {
                        case AccountType::MSA: {
                            return tr("MSA", "Account type");
                        }
                        case AccountType::Offline: {
                            return tr("Offline", "Account type");
                        }
                    }
                    return tr("Unknown", "Account type");
                }
                case StatusColumn:
                    return getAccountStatus(account->accountState());
                default:
                    return QVariant();
            }

        case Qt::ToolTipRole:
            return account->accountDisplayString();

        case PointerRole:
            return QVariant::fromValue(account);

        case Qt::CheckStateRole:
            if (index.column() == ProfileNameColumn)
                return account == m_defaultAccount ? Qt::Checked : Qt::Unchecked;
            return QVariant();

        default:
            return QVariant();
    }
}

QVariant AccountList::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const
{
    switch (role) {
        case Qt::DisplayRole:
            switch (section) {
                case ProfileNameColumn:
                    return tr("Username");
                case NameColumn:
                    return tr("Account");
                case TypeColumn:
                    return tr("Type");
                case StatusColumn:
                    return tr("Status");
                default:
                    return QVariant();
            }

        case Qt::ToolTipRole:
            switch (section) {
                case ProfileNameColumn:
                    return tr("Minecraft username associated with the account.");
                case NameColumn:
                    return tr("User name of the account.");
                case TypeColumn:
                    return tr("Type of the account (MSA or Offline)");
                case StatusColumn:
                    return tr("Current status of the account.");
                default:
                    return QVariant();
            }

        default:
            return QVariant();
    }
}

int AccountList::rowCount(const QModelIndex& parent) const
{
    // Return count
    return parent.isValid() ? 0 : count();
}

int AccountList::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}

Qt::ItemFlags AccountList::flags(const QModelIndex& index) const
{
    if (index.row() < 0 || index.row() >= rowCount(index.parent()) || !index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool AccountList::setData(const QModelIndex& idx, const QVariant& value, int role)
{
    if (idx.row() < 0 || idx.row() >= rowCount(idx.parent()) || !idx.isValid()) {
        return false;
    }

    if (role == Qt::CheckStateRole) {
        if (value == Qt::Checked) {
            MinecraftAccountPtr account = at(idx.row());
            setDefaultAccount(account);
        } else if (m_defaultAccount == at(idx.row()))
            setDefaultAccount(nullptr);
    }

    emit dataChanged(idx, index(idx.row(), columnCount(QModelIndex()) - 1));
    return true;
}

bool AccountList::loadList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't load Mojang account list. No file path given and no default set.";
        return false;
    }

    QFile file(m_listFilePath);

    // Try to open the file and fail if we can't.
    // TODO: We should probably report this error to the user.
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << QString("Failed to read the account list file (%1).").arg(m_listFilePath).toUtf8();
        return false;
    }

    // Read the file and close it.
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    // Fail if the JSON is invalid.
    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << QString("Failed to parse account list file: %1 at offset %2")
                           .arg(parseError.errorString(), QString::number(parseError.offset))
                           .toUtf8();
        return false;
    }

    // Make sure the root is an object.
    if (!jsonDoc.isObject()) {
        qCritical() << "Invalid account list JSON: Root should be an array.";
        return false;
    }

    QJsonObject root = jsonDoc.object();

    // Make sure the format version matches.
    auto listVersion = root.value("formatVersion").toVariant().toInt();
    if (listVersion == AccountListVersion::MojangMSA)
        return loadV3(root);

    QString newName = "accounts-old.json";
    qWarning() << "Unknown format version when loading account list. Existing one will be renamed to" << newName;
    // Attempt to rename the old version.
    file.rename(newName);
    return false;
}

bool AccountList::loadV3(QJsonObject& root)
{
    beginResetModel();
    QJsonArray accounts = root.value("accounts").toArray();
    for (QJsonValue accountVal : accounts) {
        QJsonObject accountObj = accountVal.toObject();
        MinecraftAccountPtr account = MinecraftAccount::loadFromJsonV3(accountObj);
        if (account.get() != nullptr) {
            auto profileId = account->profileId();
            if (profileId.size()) {
                if (findAccountByProfileId(profileId) != -1) {
                    continue;
                }
            }
            connect(account.get(), &MinecraftAccount::changed, this, &AccountList::accountChanged);
            connect(account.get(), &MinecraftAccount::activityChanged, this, &AccountList::accountActivityChanged);
            m_accounts.append(account);
            if (accountObj.value("active").toBool(false)) {
                m_defaultAccount = account;
            }
        } else {
            qWarning() << "Failed to load an account.";
        }
    }
    endResetModel();
    return true;
}

bool AccountList::saveList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't save Mojang account list. No file path given and no default set.";
        return false;
    }

    // make sure the parent folder exists
    if (!FS::ensureFilePathExists(m_listFilePath))
        return false;

    // make sure the file wasn't overwritten with a folder before (fixes a bug)
    QFileInfo finfo(m_listFilePath);
    if (finfo.isDir()) {
        QDir badDir(m_listFilePath);
        badDir.removeRecursively();
    }

    qDebug() << "Writing account list to" << m_listFilePath;

    qDebug() << "Building JSON data structure.";
    // Build the JSON document to write to the list file.
    QJsonObject root;

    root.insert("formatVersion", AccountListVersion::MojangMSA);

    // Build a list of accounts.
    qDebug() << "Building account array.";
    QJsonArray accounts;
    for (MinecraftAccountPtr account : m_accounts) {
        QJsonObject accountObj = account->saveToJson();
        if (m_defaultAccount == account) {
            accountObj["active"] = true;
        }
        accounts.append(accountObj);
    }

    // Insert the account list into the root object.
    root.insert("accounts", accounts);

    // Create a JSON document object to convert our JSON to bytes.
    QJsonDocument doc(root);

    // Now that we're done building the JSON object, we can write it to the file.
    qDebug() << "Writing account list to file.";
    QSaveFile file(m_listFilePath);

    // Try to open the file and fail if we can't.
    // TODO: We should probably report this error to the user.
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << QString("Failed to read the account list file (%1).").arg(m_listFilePath).toUtf8();
        return false;
    }

    // Write the JSON to the file.
    file.write(doc.toJson());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (file.commit()) {
        qDebug() << "Saved account list to" << m_listFilePath;
        return true;
    } else {
        qDebug() << "Failed to save accounts to" << m_listFilePath;
        return false;
    }
}

void AccountList::setListFilePath(QString path, bool autosave)
{
    m_listFilePath = path;
    m_autosave = autosave;
}

bool AccountList::anyAccountIsValid()
{
    for (auto account : m_accounts) {
        if (account->ownsMinecraft()) {
            return true;
        }
    }
    return false;
}

void AccountList::fillQueue()
{
    if (m_defaultAccount && m_defaultAccount->shouldRefresh()) {
        auto idToRefresh = m_defaultAccount->internalId();
        m_refreshQueue.push_back(idToRefresh);
        qDebug() << "AccountList: Queued default account with internal ID " << idToRefresh << " to refresh first";
    }

    for (int i = 0; i < count(); i++) {
        auto account = at(i);
        if (account == m_defaultAccount) {
            continue;
        }

        if (account->shouldRefresh()) {
            auto idToRefresh = account->internalId();
            queueRefresh(idToRefresh);
        }
    }
    tryNext();
}

void AccountList::requestRefresh(QString accountId)
{
    auto index = m_refreshQueue.indexOf(accountId);
    if (index != -1) {
        m_refreshQueue.removeAt(index);
    }
    m_refreshQueue.push_front(accountId);
    qDebug() << "AccountList: Pushed account with internal ID " << accountId << " to the front of the queue";
    if (!isActive()) {
        tryNext();
    }
}

void AccountList::queueRefresh(QString accountId)
{
    if (m_refreshQueue.indexOf(accountId) != -1) {
        return;
    }
    m_refreshQueue.push_back(accountId);
    qDebug() << "AccountList: Queued account with internal ID " << accountId << " to refresh";
}

void AccountList::tryNext()
{
    while (m_refreshQueue.length()) {
        auto accountId = m_refreshQueue.front();
        m_refreshQueue.pop_front();
        for (int i = 0; i < count(); i++) {
            auto account = at(i);
            if (account->internalId() == accountId) {
                m_currentTask = account->refresh();
                if (m_currentTask) {
                    connect(m_currentTask.get(), &Task::succeeded, this, &AccountList::authSucceeded);
                    connect(m_currentTask.get(), &Task::failed, this, &AccountList::authFailed);
                    m_currentTask->start();
                    qDebug() << "RefreshSchedule: Processing account " << account->accountDisplayString() << " with internal ID "
                             << accountId;
                    return;
                }
            }
        }
        qDebug() << "RefreshSchedule: Account with with internal ID " << accountId << " not found.";
    }
    // if we get here, no account needed refreshing. Schedule refresh in an hour.
    m_refreshTimer->start(1000 * 3600);
}

void AccountList::authSucceeded()
{
    qDebug() << "RefreshSchedule: Background account refresh succeeded";
    m_currentTask.reset();
    m_nextTimer->start(1000 * 20);
}

void AccountList::authFailed(QString reason)
{
    qDebug() << "RefreshSchedule: Background account refresh failed: " << reason;
    m_currentTask.reset();
    m_nextTimer->start(1000 * 20);
}

bool AccountList::isActive() const
{
    return m_activityCount != 0;
}

void AccountList::beginActivity()
{
    bool activating = m_activityCount == 0;
    m_activityCount++;
    if (activating) {
        emit activityChanged(true);
    }
}

void AccountList::endActivity()
{
    if (m_activityCount == 0) {
        qWarning() << m_name << " - Activity count would become below zero";
        return;
    }
    bool deactivating = m_activityCount == 1;
    m_activityCount--;
    if (deactivating) {
        emit activityChanged(false);
    }
}
