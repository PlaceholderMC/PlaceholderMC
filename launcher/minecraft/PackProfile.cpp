// SPDX-FileCopyrightText: 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
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

#include <Version.h>
#include <qlogging.h>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <utility>

#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "meta/Index.h"
#include "meta/JsonFormat.h"
#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/ProfileUtils.h"

#include "ComponentUpdateTask.h"
#include "PackProfile.h"
#include "PackProfile_p.h"
#include "modplatform/ModIndex.h"

#include "minecraft/Logging.h"

#include "ui/dialogs/CustomMessageBox.h"

PackProfile::PackProfile(MinecraftInstance* instance) : QAbstractListModel()
{
    d.reset(new PackProfileData);
    d->m_instance = instance;
    d->m_saveTimer.setSingleShot(true);
    d->m_saveTimer.setInterval(5000);
    d->interactionDisabled = instance->isRunning();
    connect(d->m_instance, &BaseInstance::runningStatusChanged, this, &PackProfile::disableInteraction);
    connect(&d->m_saveTimer, &QTimer::timeout, this, &PackProfile::save_internal);
}

PackProfile::~PackProfile()
{
    saveNow();
}

// BEGIN: component file format

static const int currentComponentsFileVersion = 1;

static QJsonObject componentToJsonV1(ComponentPtr component)
{
    QJsonObject obj;
    // critical
    obj.insert("uid", component->m_uid);
    if (!component->m_version.isEmpty()) {
        obj.insert("version", component->m_version);
    }
    if (component->m_dependencyOnly) {
        obj.insert("dependencyOnly", true);
    }
    if (component->m_important) {
        obj.insert("important", true);
    }
    if (component->m_disabled) {
        obj.insert("disabled", true);
    }

    // cached
    if (!component->m_cachedVersion.isEmpty()) {
        obj.insert("cachedVersion", component->m_cachedVersion);
    }
    if (!component->m_cachedName.isEmpty()) {
        obj.insert("cachedName", component->m_cachedName);
    }
    Meta::serializeRequires(obj, &component->m_cachedRequires, "cachedRequires");
    Meta::serializeRequires(obj, &component->m_cachedConflicts, "cachedConflicts");
    if (component->m_cachedVolatile) {
        obj.insert("cachedVolatile", true);
    }
    return obj;
}

static ComponentPtr componentFromJsonV1(PackProfile* parent, const QString& componentJsonPattern, const QJsonObject& obj)
{
    // critical
    auto uid = Json::requireString(obj.value("uid"));
    auto filePath = componentJsonPattern.arg(uid);
    auto component = makeShared<Component>(parent, uid);
    component->m_version = Json::ensureString(obj.value("version"));
    component->m_dependencyOnly = Json::ensureBoolean(obj.value("dependencyOnly"), false);
    component->m_important = Json::ensureBoolean(obj.value("important"), false);

    // cached
    // TODO @RESILIENCE: ignore invalid values/structure here?
    component->m_cachedVersion = Json::ensureString(obj.value("cachedVersion"));
    component->m_cachedName = Json::ensureString(obj.value("cachedName"));
    Meta::parseRequires(obj, &component->m_cachedRequires, "cachedRequires");
    Meta::parseRequires(obj, &component->m_cachedConflicts, "cachedConflicts");
    component->m_cachedVolatile = Json::ensureBoolean(obj.value("volatile"), false);
    bool disabled = Json::ensureBoolean(obj.value("disabled"), false);
    component->setEnabled(!disabled);
    return component;
}

// Save the given component container data to a file
static bool savePackProfile(const QString& filename, const ComponentContainer& container)
{
    QJsonObject obj;
    obj.insert("formatVersion", currentComponentsFileVersion);
    QJsonArray orderArray;
    for (auto component : container) {
        orderArray.append(componentToJsonV1(component));
    }
    obj.insert("components", orderArray);
    QSaveFile outFile(filename);
    if (!outFile.open(QFile::WriteOnly)) {
        qCCritical(instanceProfileC) << "Couldn't open" << outFile.fileName() << "for writing:" << outFile.errorString();
        return false;
    }
    auto data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (outFile.write(data) != data.size()) {
        qCCritical(instanceProfileC) << "Couldn't write all the data into" << outFile.fileName() << "because:" << outFile.errorString();
        return false;
    }
    if (!outFile.commit()) {
        qCCritical(instanceProfileC) << "Couldn't save" << outFile.fileName() << "because:" << outFile.errorString();
    }
    return true;
}

// Read the given file into component containers
static PackProfile::Result loadPackProfile(PackProfile* parent,
                                           const QString& filename,
                                           const QString& componentJsonPattern,
                                           ComponentContainer& container)
{
    QFile componentsFile(filename);
    if (!componentsFile.exists()) {
        auto message = QObject::tr("Components file %1 doesn't exist. This should never happen.").arg(filename);
        qCWarning(instanceProfileC) << message;
        return PackProfile::Result::Error(message);
    }
    if (!componentsFile.open(QFile::ReadOnly)) {
        auto message = QObject::tr("Couldn't open %1 for reading: %2").arg(componentsFile.fileName(), componentsFile.errorString());
        qCCritical(instanceProfileC) << message;
        qCWarning(instanceProfileC) << "Ignoring overridden order";
        return PackProfile::Result::Error(message);
    }

    // and it's valid JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(componentsFile.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        auto message = QObject::tr("Couldn't parse %1 as json: %2").arg(componentsFile.fileName(), error.errorString());
        qCCritical(instanceProfileC) << message;
        qCWarning(instanceProfileC) << "Ignoring overridden order";
        return PackProfile::Result::Error(message);
    }

    // and then read it and process it if all above is true.
    try {
        auto obj = Json::requireObject(doc);
        // check order file version.
        auto version = Json::requireInteger(obj.value("formatVersion"));
        if (version != currentComponentsFileVersion) {
            throw JSONValidationError(QObject::tr("Invalid component file version, expected %1").arg(currentComponentsFileVersion));
        }
        auto orderArray = Json::requireArray(obj.value("components"));
        for (auto item : orderArray) {
            auto comp_obj = Json::requireObject(item, "Component must be an object.");
            container.append(componentFromJsonV1(parent, componentJsonPattern, comp_obj));
        }
    } catch ([[maybe_unused]] const JSONValidationError& err) {
        auto message = QObject::tr("Couldn't parse %1 : bad file format").arg(componentsFile.fileName());
        qCCritical(instanceProfileC) << message;
        qCWarning(instanceProfileC) << "error:" << err.what();
        container.clear();
        return PackProfile::Result::Error(message);
    }
    return PackProfile::Result::Success();
}

// END: component file format

// BEGIN: save/load logic

void PackProfile::saveNow()
{
    if (saveIsScheduled()) {
        d->m_saveTimer.stop();
        save_internal();
    }
}

bool PackProfile::saveIsScheduled() const
{
    return d->dirty;
}

void PackProfile::buildingFromScratch()
{
    d->loaded = true;
    d->dirty = true;
}

void PackProfile::scheduleSave()
{
    if (!d->loaded) {
        qDebug() << d->m_instance->name() << "|" << "Component list should never save if it didn't successfully load";
        return;
    }
    if (!d->dirty) {
        d->dirty = true;
        qDebug() << d->m_instance->name() << "|" << "Component list save is scheduled";
    }
    d->m_saveTimer.start();
}

RuntimeContext PackProfile::runtimeContext()
{
    return d->m_instance->runtimeContext();
}

QString PackProfile::componentsFilePath() const
{
    return FS::PathCombine(d->m_instance->instanceRoot(), "mmc-pack.json");
}

QString PackProfile::patchesPattern() const
{
    return FS::PathCombine(d->m_instance->instanceRoot(), "patches", "%1.json");
}

QString PackProfile::patchFilePathForUid(const QString& uid) const
{
    return patchesPattern().arg(uid);
}

void PackProfile::save_internal()
{
    qDebug() << d->m_instance->name() << "|" << "Component list save performed now";
    auto filename = componentsFilePath();
    savePackProfile(filename, d->components);
    d->dirty = false;
}

PackProfile::Result PackProfile::load()
{
    auto filename = componentsFilePath();

    // load the new component list and swap it with the current one...
    ComponentContainer newComponents;
    if (auto result = loadPackProfile(this, filename, patchesPattern(), newComponents); !result) {
        qCritical() << d->m_instance->name() << "|" << "Failed to load the component config";
        return result;
    }
    // FIXME: actually use fine-grained updates, not this...
    beginResetModel();
    // disconnect all the old components
    for (auto component : d->components) {
        disconnect(component.get(), &Component::dataChanged, this, &PackProfile::componentDataChanged);
    }
    d->components.clear();
    d->componentIndex.clear();
    for (auto component : newComponents) {
        if (d->componentIndex.contains(component->m_uid)) {
            qWarning() << d->m_instance->name() << "|" << "Ignoring duplicate component entry" << component->m_uid;
            continue;
        }
        connect(component.get(), &Component::dataChanged, this, &PackProfile::componentDataChanged);
        d->components.append(component);
        d->componentIndex[component->m_uid] = component;
    }
    endResetModel();
    d->loaded = true;
    return Result::Success();
}

PackProfile::Result PackProfile::reload(Net::Mode netmode)
{
    // Do not reload when the update/resolve task is running. It is in control.
    if (d->m_updateTask) {
        return Result::Success();
    }

    // flush any scheduled saves to not lose state
    saveNow();

    // FIXME: differentiate when a reapply is required by propagating state from components
    invalidateLaunchProfile();

    if (auto result = load(); !result) {
        return result;
    }
    resolve(netmode);
    return Result::Success();
}

Task::Ptr PackProfile::getCurrentTask()
{
    return d->m_updateTask;
}

void PackProfile::resolve(Net::Mode netmode)
{
    auto updateTask = new ComponentUpdateTask(ComponentUpdateTask::Mode::Resolution, netmode, this);
    d->m_updateTask.reset(updateTask);
    connect(updateTask, &ComponentUpdateTask::succeeded, this, &PackProfile::updateSucceeded);
    connect(updateTask, &ComponentUpdateTask::failed, this, &PackProfile::updateFailed);
    connect(updateTask, &ComponentUpdateTask::aborted, this, [this] { updateFailed(tr("Aborted")); });
    d->m_updateTask->start();
}

void PackProfile::updateSucceeded()
{
    qCDebug(instanceProfileC) << d->m_instance->name() << "|" << "Component list update/resolve task succeeded";
    d->m_updateTask.reset();
    invalidateLaunchProfile();
}

void PackProfile::updateFailed(const QString& error)
{
    qCDebug(instanceProfileC) << d->m_instance->name() << "|" << "Component list update/resolve task failed " << "Reason:" << error;
    d->m_updateTask.reset();
    invalidateLaunchProfile();
}

// END: save/load

void PackProfile::appendComponent(ComponentPtr component)
{
    insertComponent(d->components.size(), component);
}

void PackProfile::insertComponent(size_t index, ComponentPtr component)
{
    auto id = component->getID();
    if (id.isEmpty()) {
        qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "Attempt to add a component with empty ID!";
        return;
    }
    if (d->componentIndex.contains(id)) {
        qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "Attempt to add a component that is already present!";
        return;
    }
    beginInsertRows(QModelIndex(), static_cast<int>(index), static_cast<int>(index));
    d->components.insert(index, component);
    d->componentIndex[id] = component;
    endInsertRows();
    connect(component.get(), &Component::dataChanged, this, &PackProfile::componentDataChanged);
    scheduleSave();
}

void PackProfile::componentDataChanged()
{
    auto objPtr = qobject_cast<Component*>(sender());
    if (!objPtr) {
        qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "PackProfile got dataChanged signal from a non-Component!";
        return;
    }
    if (objPtr->getID() == "net.minecraft") {
        emit minecraftChanged();
    }
    // figure out which one is it... in a seriously dumb way.
    int index = 0;
    for (auto component : d->components) {
        if (component.get() == objPtr) {
            emit dataChanged(createIndex(index, 0), createIndex(index, columnCount(QModelIndex()) - 1));
            scheduleSave();
            return;
        }
        index++;
    }
    qCWarning(instanceProfileC) << d->m_instance->name() << "|"
                                << "PackProfile got dataChanged signal from a Component which does not belong to it!";
}

bool PackProfile::remove(const int index)
{
    auto patch = getComponent(index);
    if (!patch->isRemovable()) {
        qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "is non-removable";
        return false;
    }

    if (!removeComponent_internal(patch)) {
        qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "could not be removed";
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    d->components.removeAt(index);
    d->componentIndex.remove(patch->getID());
    endRemoveRows();
    invalidateLaunchProfile();
    scheduleSave();
    return true;
}

bool PackProfile::remove(const QString& id)
{
    int i = 0;
    for (auto patch : d->components) {
        if (patch->getID() == id) {
            return remove(i);
        }
        i++;
    }
    return false;
}

bool PackProfile::customize(int index)
{
    auto patch = getComponent(index);
    if (!patch->isCustomizable()) {
        qCDebug(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "is not customizable";
        return false;
    }
    if (!patch->customize()) {
        qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "could not be customized";
        return false;
    }
    invalidateLaunchProfile();
    scheduleSave();
    return true;
}

bool PackProfile::revertToBase(int index)
{
    auto patch = getComponent(index);
    if (!patch->isRevertible()) {
        qCDebug(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "is not revertible";
        return false;
    }
    if (!patch->revert()) {
        qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Patch" << patch->getID() << "could not be reverted";
        return false;
    }
    invalidateLaunchProfile();
    scheduleSave();
    return true;
}

ComponentPtr PackProfile::getComponent(const QString& id)
{
    auto iter = d->componentIndex.find(id);
    if (iter == d->componentIndex.end()) {
        return nullptr;
    }
    return (*iter);
}

ComponentPtr PackProfile::getComponent(size_t index)
{
    if (index >= static_cast<size_t>(d->components.size())) {
        return nullptr;
    }
    return d->components[index];
}

QVariant PackProfile::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int column = index.column();

    if (row < 0 || row >= d->components.size())
        return QVariant();

    auto patch = d->components.at(row);

    switch (role) {
        case Qt::CheckStateRole: {
            if (column == NameColumn)
                return patch->isEnabled() ? Qt::Checked : Qt::Unchecked;
            return QVariant();
        }
        case Qt::DisplayRole: {
            switch (column) {
                case NameColumn:
                    return patch->getName();
                case VersionColumn: {
                    if (patch->isCustom()) {
                        return QString("%1 (Custom)").arg(patch->getVersion());
                    } else {
                        return patch->getVersion();
                    }
                }
                default:
                    return QVariant();
            }
        }
        case Qt::DecorationRole: {
            if (column == NameColumn) {
                auto severity = patch->getProblemSeverity();
                switch (severity) {
                    case ProblemSeverity::Warning:
                        return "warning";
                    case ProblemSeverity::Error:
                        return "error";
                    default:
                        return QVariant();
                }
            }
            return QVariant();
        }
    }
    return QVariant();
}

bool PackProfile::setData(const QModelIndex& index, [[maybe_unused]] const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(index.parent())) {
        return false;
    }

    if (role == Qt::CheckStateRole) {
        auto component = d->components[index.row()];
        if (component->setEnabled(!component->isEnabled())) {
            return true;
        }
    }
    return false;
}

QVariant PackProfile::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
                case NameColumn:
                    return tr("Name");
                case VersionColumn:
                    return tr("Version");
                default:
                    return QVariant();
            }
        }
    }
    return QVariant();
}

// FIXME: zero precision mess
Qt::ItemFlags PackProfile::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags outFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    int row = index.row();

    if (row < 0 || row >= d->components.size()) {
        return Qt::NoItemFlags;
    }

    auto patch = d->components.at(row);
    // TODO: this will need fine-tuning later...
    if (patch->canBeDisabled() && !d->interactionDisabled) {
        outFlags |= Qt::ItemIsUserCheckable;
    }
    return outFlags;
}

int PackProfile::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->components.size();
}

int PackProfile::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}

void PackProfile::move(const int index, const MoveDirection direction)
{
    int theirIndex;
    if (direction == MoveUp) {
        theirIndex = index - 1;
    } else {
        theirIndex = index + 1;
    }

    if (index < 0 || index >= d->components.size())
        return;
    if (theirIndex >= rowCount())
        theirIndex = rowCount() - 1;
    if (theirIndex == -1)
        theirIndex = rowCount() - 1;
    if (index == theirIndex)
        return;
    int togap = theirIndex > index ? theirIndex + 1 : theirIndex;

    auto from = getComponent(index);
    auto to = getComponent(theirIndex);

    if (!from || !to || !to->isMoveable() || !from->isMoveable()) {
        return;
    }
    beginMoveRows(QModelIndex(), index, index, QModelIndex(), togap);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    d->components.swapItemsAt(index, theirIndex);
#else
    d->components.swap(index, theirIndex);
#endif
    endMoveRows();
    invalidateLaunchProfile();
    scheduleSave();
}

void PackProfile::invalidateLaunchProfile()
{
    d->m_profile.reset();
}

void PackProfile::installJarMods(QStringList selectedFiles)
{
    // FIXME: get rid of _internal
    installJarMods_internal(selectedFiles);
}

void PackProfile::installCustomJar(QString selectedFile)
{
    // FIXME: get rid of _internal
    installCustomJar_internal(selectedFile);
}

bool PackProfile::installComponents(QStringList selectedFiles)
{
    const QString patchDir = FS::PathCombine(d->m_instance->instanceRoot(), "patches");
    if (!FS::ensureFolderPathExists(patchDir))
        return false;

    bool result = true;
    for (const QString& source : selectedFiles) {
        const QFileInfo sourceInfo(source);

        auto versionFile = ProfileUtils::parseJsonFile(sourceInfo, false);
        const QString target = FS::PathCombine(patchDir, versionFile->uid + ".json");

        if (!QFile::copy(source, target)) {
            qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "Component" << source << "could not be copied to target"
                                        << target;
            result = false;
            continue;
        }

        appendComponent(makeShared<Component>(this, versionFile->uid, versionFile));
    }

    scheduleSave();
    invalidateLaunchProfile();

    return result;
}

void PackProfile::installAgents(QStringList selectedFiles)
{
    // FIXME: get rid of _internal
    installAgents_internal(selectedFiles);
}

bool PackProfile::installEmpty(const QString& uid, const QString& name)
{
    QString patchDir = FS::PathCombine(d->m_instance->instanceRoot(), "patches");
    if (!FS::ensureFolderPathExists(patchDir)) {
        return false;
    }
    auto f = std::make_shared<VersionFile>();
    f->name = name;
    f->uid = uid;
    f->version = "1";
    QString patchFileName = FS::PathCombine(patchDir, uid + ".json");
    QFile file(patchFileName);
    if (!file.open(QFile::WriteOnly)) {
        qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Error opening" << file.fileName()
                                     << "for reading:" << file.errorString();
        return false;
    }
    file.write(OneSixVersionFormat::versionFileToJson(f).toJson());
    file.close();

    appendComponent(makeShared<Component>(this, f->uid, f));
    scheduleSave();
    invalidateLaunchProfile();
    return true;
}

bool PackProfile::removeComponent_internal(ComponentPtr patch)
{
    bool ok = true;
    // first, remove the patch file. this ensures it's not used anymore
    auto fileName = patch->getFilename();
    if (fileName.size()) {
        QFile patchFile(fileName);
        if (patchFile.exists() && !patchFile.remove()) {
            qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "File" << fileName
                                         << "could not be removed because:" << patchFile.errorString();
            return false;
        }
    }

    // FIXME: we need a generic way of removing local resources, not just jar mods...
    auto preRemoveJarMod = [this](LibraryPtr jarMod) -> bool {
        if (!jarMod->isLocal()) {
            return true;
        }
        QStringList jar, temp1, temp2, temp3;
        jarMod->getApplicableFiles(d->m_instance->runtimeContext(), jar, temp1, temp2, temp3, d->m_instance->jarmodsPath().absolutePath());
        QFileInfo finfo(jar[0]);
        if (finfo.exists()) {
            QFile jarModFile(jar[0]);
            if (!jarModFile.remove()) {
                qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "File" << jar[0]
                                             << "could not be removed because:" << jarModFile.errorString();
                return false;
            }
            return true;
        }
        return true;
    };

    auto vFile = patch->getVersionFile();
    if (vFile) {
        auto& jarMods = vFile->jarMods;
        for (auto& jarmod : jarMods) {
            ok &= preRemoveJarMod(jarmod);
        }
    }
    return ok;
}

bool PackProfile::installJarMods_internal(QStringList filepaths)
{
    QString patchDir = FS::PathCombine(d->m_instance->instanceRoot(), "patches");
    if (!FS::ensureFolderPathExists(patchDir)) {
        return false;
    }

    if (!FS::ensureFolderPathExists(d->m_instance->jarModsDir())) {
        return false;
    }

    for (auto filepath : filepaths) {
        QFileInfo sourceInfo(filepath);
        QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString target_filename = id + ".jar";
        QString target_id = "custom.jarmod." + id;
        QString target_name = sourceInfo.completeBaseName() + " (jar mod)";
        QString finalPath = FS::PathCombine(d->m_instance->jarModsDir(), target_filename);

        QFileInfo targetInfo(finalPath);
        Q_ASSERT(!targetInfo.exists());

        if (!QFile::copy(sourceInfo.absoluteFilePath(), QFileInfo(finalPath).absoluteFilePath())) {
            return false;
        }

        auto f = std::make_shared<VersionFile>();
        auto jarMod = std::make_shared<Library>();
        jarMod->setRawName(GradleSpecifier("custom.jarmods:" + id + ":1"));
        jarMod->setFilename(target_filename);
        jarMod->setDisplayName(sourceInfo.completeBaseName());
        jarMod->setHint("local");
        f->jarMods.append(jarMod);
        f->name = target_name;
        f->uid = target_id;
        QString patchFileName = FS::PathCombine(patchDir, target_id + ".json");

        QFile file(patchFileName);
        if (!file.open(QFile::WriteOnly)) {
            qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Error opening" << file.fileName()
                                         << "for reading:" << file.errorString();
            return false;
        }
        file.write(OneSixVersionFormat::versionFileToJson(f).toJson());
        file.close();

        appendComponent(makeShared<Component>(this, f->uid, f));
    }
    scheduleSave();
    invalidateLaunchProfile();
    return true;
}

bool PackProfile::installCustomJar_internal(QString filepath)
{
    QString patchDir = FS::PathCombine(d->m_instance->instanceRoot(), "patches");
    if (!FS::ensureFolderPathExists(patchDir)) {
        return false;
    }

    QString libDir = d->m_instance->getLocalLibraryPath();
    if (!FS::ensureFolderPathExists(libDir)) {
        return false;
    }

    auto specifier = GradleSpecifier("custom:customjar:1");
    QFileInfo sourceInfo(filepath);
    QString target_filename = specifier.getFileName();
    QString target_id = specifier.artifactId();
    QString target_name = sourceInfo.completeBaseName() + " (custom jar)";
    QString finalPath = FS::PathCombine(libDir, target_filename);

    QFileInfo jarInfo(finalPath);
    if (jarInfo.exists()) {
        if (!FS::deletePath(finalPath)) {
            return false;
        }
    }
    if (!QFile::copy(filepath, finalPath)) {
        return false;
    }

    auto f = std::make_shared<VersionFile>();
    auto jarMod = std::make_shared<Library>();
    jarMod->setRawName(specifier);
    jarMod->setDisplayName(sourceInfo.completeBaseName());
    jarMod->setHint("local");
    f->mainJar = jarMod;
    f->name = target_name;
    f->uid = target_id;
    QString patchFileName = FS::PathCombine(patchDir, target_id + ".json");

    QFile file(patchFileName);
    if (!file.open(QFile::WriteOnly)) {
        qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Error opening" << file.fileName()
                                     << "for reading:" << file.errorString();
        return false;
    }
    file.write(OneSixVersionFormat::versionFileToJson(f).toJson());
    file.close();

    appendComponent(makeShared<Component>(this, f->uid, f));

    scheduleSave();
    invalidateLaunchProfile();
    return true;
}

bool PackProfile::installAgents_internal(QStringList filepaths)
{
    // FIXME code duplication
    const QString patchDir = FS::PathCombine(d->m_instance->instanceRoot(), "patches");
    if (!FS::ensureFolderPathExists(patchDir))
        return false;

    const QString libDir = d->m_instance->getLocalLibraryPath();
    if (!FS::ensureFolderPathExists(libDir))
        return false;

    for (const QString& source : filepaths) {
        const QFileInfo sourceInfo(source);
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString targetBaseName = id + ".jar";
        const QString targetId = "custom.agent." + id;
        const QString targetName = sourceInfo.completeBaseName() + " (agent)";
        const QString target = FS::PathCombine(d->m_instance->getLocalLibraryPath(), targetBaseName);

        const QFileInfo targetInfo(target);
        Q_ASSERT(!targetInfo.exists());

        if (!QFile::copy(source, target))
            return false;

        auto versionFile = std::make_shared<VersionFile>();

        auto agent = std::make_shared<Library>();

        agent->setRawName("custom.agents:" + id + ":1");
        agent->setFilename(targetBaseName);
        agent->setDisplayName(sourceInfo.completeBaseName());
        agent->setHint("local");

        versionFile->agents.append(std::make_shared<Agent>(agent, QString()));

        versionFile->name = targetName;
        versionFile->uid = targetId;

        QFile patchFile(FS::PathCombine(patchDir, targetId + ".json"));

        if (!patchFile.open(QFile::WriteOnly)) {
            qCCritical(instanceProfileC) << d->m_instance->name() << "|" << "Error opening" << patchFile.fileName()
                                         << "for reading:" << patchFile.errorString();
            return false;
        }

        patchFile.write(OneSixVersionFormat::versionFileToJson(versionFile).toJson());
        patchFile.close();

        appendComponent(makeShared<Component>(this, versionFile->uid, versionFile));
    }

    scheduleSave();
    invalidateLaunchProfile();

    return true;
}

std::shared_ptr<LaunchProfile> PackProfile::getProfile() const
{
    if (!d->m_profile) {
        try {
            auto profile = std::make_shared<LaunchProfile>();
            for (auto file : d->components) {
                qCDebug(instanceProfileC) << d->m_instance->name() << "|" << "Applying" << file->getID()
                                          << (file->getProblemSeverity() == ProblemSeverity::Error ? "ERROR" : "GOOD");
                file->applyTo(profile.get());
            }
            d->m_profile = profile;
        } catch (const Exception& error) {
            qCWarning(instanceProfileC) << d->m_instance->name() << "|" << "Couldn't apply profile patches because: " << error.cause();
        }
    }
    return d->m_profile;
}

bool PackProfile::setComponentVersion(const QString& uid, const QString& version, bool important)
{
    auto iter = d->componentIndex.find(uid);
    if (iter != d->componentIndex.end()) {
        ComponentPtr component = *iter;
        // set existing
        if (component->revert()) {
            // set new version
            auto oldVersion = component->getVersion();
            component->setVersion(version);
            component->setImportant(important);

            if (important) {
                component->setUpdateAction(UpdateAction{ UpdateActionImportantChanged{ oldVersion } });
                resolve(Net::Mode::Online);
            }

            return true;
        }
        return false;
    } else {
        // add new
        auto component = makeShared<Component>(this, uid);
        component->m_version = version;
        component->m_important = important;
        appendComponent(component);
        return true;
    }
}

QString PackProfile::getComponentVersion(const QString& uid) const
{
    const auto iter = d->componentIndex.find(uid);
    if (iter != d->componentIndex.end()) {
        return (*iter)->getVersion();
    }
    return QString();
}

void PackProfile::disableInteraction(bool disable)
{
    if (d->interactionDisabled != disable) {
        d->interactionDisabled = disable;
        auto size = d->components.size();
        if (size) {
            emit dataChanged(index(0), index(size - 1));
        }
    }
}

std::optional<ModPlatform::ModLoaderTypes> PackProfile::getModLoaders()
{
    ModPlatform::ModLoaderTypes result;
    bool has_any_loader = false;

    QMapIterator<QString, ModloaderMapEntry> i(Component::KNOWN_MODLOADERS);

    while (i.hasNext()) {
        i.next();
        if (auto c = getComponent(i.key()); c != nullptr && c->isEnabled()) {
            result |= i.value().type;
            has_any_loader = true;
        }
    }

    if (!has_any_loader)
        return {};
    return result;
}

std::optional<ModPlatform::ModLoaderTypes> PackProfile::getSupportedModLoaders()
{
    auto loadersOpt = getModLoaders();
    if (!loadersOpt.has_value())
        return loadersOpt;
    auto loaders = loadersOpt.value();
    // TODO: remove this or add version condition once Quilt drops official Fabric support
    if (loaders & ModPlatform::Quilt)
        loaders |= ModPlatform::Fabric;
    if (getComponentVersion("net.minecraft") == "1.20.1" && (loaders & ModPlatform::NeoForge))
        loaders |= ModPlatform::Forge;
    return loaders;
}

QList<ModPlatform::ModLoaderType> PackProfile::getModLoadersList()
{
    QList<ModPlatform::ModLoaderType> result;
    for (auto c : d->components) {
        if (c->isEnabled() && Component::KNOWN_MODLOADERS.contains(c->getID())) {
            result.append(Component::KNOWN_MODLOADERS[c->getID()].type);
        }
    }

    // TODO: remove this or add version condition once Quilt drops official Fabric support
    if (result.contains(ModPlatform::Quilt) && !result.contains(ModPlatform::Fabric)) {
        result.append(ModPlatform::Fabric);
    }
    if (getComponentVersion("net.minecraft") == "1.20.1" && result.contains(ModPlatform::NeoForge) &&
        !result.contains(ModPlatform::Forge)) {
        result.append(ModPlatform::Forge);
    }
    return result;
}
