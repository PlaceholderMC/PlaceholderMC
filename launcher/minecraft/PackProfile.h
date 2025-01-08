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

#pragma once

#include <QAbstractListModel>

#include <QList>
#include <QString>
#include <memory>
#include <optional>

#include "Component.h"
#include "LaunchProfile.h"
#include "modplatform/ModIndex.h"
#include "net/Mode.h"

class MinecraftInstance;
struct PackProfileData;
class ComponentUpdateTask;

class PackProfile : public QAbstractListModel {
    Q_OBJECT
    friend ComponentUpdateTask;

   public:
    enum Columns { NameColumn = 0, VersionColumn, NUM_COLUMNS };

    struct Result {
        bool success;
        QString error;

        // Implicit conversion to bool
        operator bool() const { return success; }

        // Factory methods for convenience
        static Result Success() { return { true, "" }; }

        static Result Error(const QString& errorMessage) { return { false, errorMessage }; }
    };

    explicit PackProfile(MinecraftInstance* instance);
    virtual ~PackProfile();

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// call this to explicitly mark the component list as loaded - this is used to build a new component list from scratch.
    void buildingFromScratch();

    /// install more jar mods
    void installJarMods(QStringList selectedFiles);

    /// install a jar/zip as a replacement for the main jar
    void installCustomJar(QString selectedFile);

    /// install MMC/Prism component files
    bool installComponents(QStringList selectedFiles);

    /// install Java agent files
    void installAgents(QStringList selectedFiles);

    enum MoveDirection { MoveUp, MoveDown };
    /// move component file # up or down the list
    void move(int index, MoveDirection direction);

    /// remove component file # - including files/records
    bool remove(int index);

    /// remove component file by id - including files/records
    bool remove(const QString& id);

    bool customize(int index);

    bool revertToBase(int index);

    /// reload the list, reload all components, resolve dependencies
    Result reload(Net::Mode netmode);

    // reload all components, resolve dependencies
    void resolve(Net::Mode netmode);

    /// get current running task...
    Task::Ptr getCurrentTask();

    std::shared_ptr<LaunchProfile> getProfile() const;

    // NOTE: used ONLY by MinecraftInstance to provide legacy version mappings from instance config
    void setOldConfigVersion(const QString& uid, const QString& version);

    QString getComponentVersion(const QString& uid) const;

    bool setComponentVersion(const QString& uid, const QString& version, bool important = false);

    bool installEmpty(const QString& uid, const QString& name);

    QString patchFilePathForUid(const QString& uid) const;

    /// if there is a save scheduled, do it now.
    void saveNow();

    /// helper method, returns RuntimeContext of instance
    RuntimeContext runtimeContext();

   signals:
    void minecraftChanged();

   public:
    /// get the profile component by id
    ComponentPtr getComponent(const QString& id);

    /// get the profile component by index
    ComponentPtr getComponent(size_t index);

    /// Add the component to the internal list of patches
    // todo(merged): is this the best approach
    void appendComponent(ComponentPtr component);

    std::optional<ModPlatform::ModLoaderTypes> getModLoaders();
    // this returns aditional loaders(Quilt supports fabric and NeoForge supports Forge)
    std::optional<ModPlatform::ModLoaderTypes> getSupportedModLoaders();
    QList<ModPlatform::ModLoaderType> getModLoadersList();

    /// apply the component patches. Catches all the errors and returns true/false for success/failure
    void invalidateLaunchProfile();

   private:
    void scheduleSave();
    bool saveIsScheduled() const;

    /// insert component so that its index is ideally the specified one (returns real index)
    void insertComponent(size_t index, ComponentPtr component);

    QString componentsFilePath() const;
    QString patchesPattern() const;

   private slots:
    void save_internal();
    void updateSucceeded();
    void updateFailed(const QString& error);
    void componentDataChanged();
    void disableInteraction(bool disable);

   private:
    Result load();
    bool installJarMods_internal(QStringList filepaths);
    bool installCustomJar_internal(QString filepath);
    bool installAgents_internal(QStringList filepaths);
    bool removeComponent_internal(ComponentPtr patch);

   private: /* data */
    std::unique_ptr<PackProfileData> d;
};
