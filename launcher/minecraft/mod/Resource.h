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

#include <QDateTime>
#include <QFileInfo>
#include <QObject>
#include <QPointer>

#include "MetadataHandler.h"
#include "QObjectPtr.h"

enum class ResourceType {
    UNKNOWN,     //!< Indicates an unspecified resource type.
    ZIPFILE,     //!< The resource is a zip file containing the resource's class files.
    SINGLEFILE,  //!< The resource is a single file (not a zip file).
    FOLDER,      //!< The resource is in a folder on the filesystem.
    LITEMOD,     //!< The resource is a litemod
};

enum class ResourceStatus {
    INSTALLED,      // Both JAR and Metadata are present
    NOT_INSTALLED,  // Only the Metadata is present
    NO_METADATA,    // Only the JAR is present
    UNKNOWN,        // Default status
};

enum class SortType {
    NAME,
    DATE,
    VERSION,
    ENABLED,
    PACK_FORMAT,
    PROVIDER,
    SIZE,
    SIDE,
    MC_VERSIONS,
    LOADERS,
    RELEASE_TYPE,
    CATEGORY,
    LOCK_UPDATE
};

enum class EnableAction { ENABLE, DISABLE, TOGGLE };

/** General class for managed resources. It mirrors a file in disk, with some more info
 *  for display and house-keeping purposes.
 *
 *  Subclass it to add additional data / behavior, such as Mods or Resource packs.
 */
class Resource : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(Resource)
   public:
    using Ptr = shared_qobject_ptr<Resource>;
    using WeakPtr = QPointer<Resource>;

    Resource(QObject* parent = nullptr);
    Resource(QFileInfo file_info);
    Resource(QString file_path) : Resource(QFileInfo(file_path)) {}

    ~Resource() override = default;

    void setFile(QFileInfo file_info);
    void parseFile();

    [[nodiscard]] auto fileinfo() const -> QFileInfo { return m_file_info; }
    [[nodiscard]] auto dateTimeChanged() const -> QDateTime { return m_changed_date_time; }
    [[nodiscard]] auto internal_id() const -> QString { return m_internal_id; }
    [[nodiscard]] auto type() const -> ResourceType { return m_type; }
    [[nodiscard]] bool enabled() const { return m_enabled; }
    [[nodiscard]] auto getOriginalFileName() const -> QString;
    [[nodiscard]] QString sizeStr() const { return m_size_str; }
    [[nodiscard]] qint64 sizeInfo() const { return m_size_info; }

    [[nodiscard]] virtual auto name() const -> QString;
    [[nodiscard]] virtual bool valid() const { return m_type != ResourceType::UNKNOWN; }

    [[nodiscard]] auto status() const -> ResourceStatus { return m_status; };
    [[nodiscard]] auto metadata() -> std::shared_ptr<Metadata::ModStruct> { return m_metadata; }
    [[nodiscard]] auto metadata() const -> std::shared_ptr<const Metadata::ModStruct> { return m_metadata; }
    [[nodiscard]] auto provider() const -> QString;
    [[nodiscard]] virtual auto homepage() const -> QString;
    [[nodiscard]] QString categories() const;
    [[nodiscard]] bool lockUpdate() const;

    void setStatus(ResourceStatus status) { m_status = status; }
    void setMetadata(std::shared_ptr<Metadata::ModStruct>&& metadata);
    void setMetadata(const Metadata::ModStruct& metadata) { setMetadata(std::make_shared<Metadata::ModStruct>(metadata)); }

    /** Compares two Resources, for sorting purposes, considering a ascending order, returning:
     *  > 0: 'this' comes after 'other'
     *  = 0: 'this' is equal to 'other'
     *  < 0: 'this' comes before 'other'
     */
    [[nodiscard]] virtual int compare(Resource const& other, SortType type = SortType::NAME) const;

    /** Returns whether the given filter should filter out 'this' (false),
     *  or if such filter includes the Resource (true).
     */
    [[nodiscard]] virtual bool applyFilter(QRegularExpression filter) const;
    [[nodiscard]] bool checkCategories(QRegularExpression filter, bool& match) const;

    /** Changes the enabled property, according to 'action'.
     *
     *  Returns whether a change was applied to the Resource's properties.
     */
    bool enable(EnableAction action);

    [[nodiscard]] auto shouldResolve() const -> bool { return !m_is_resolving && !m_is_resolved; }
    [[nodiscard]] auto isResolving() const -> bool { return m_is_resolving; }
    [[nodiscard]] auto isResolved() const -> bool { return m_is_resolved; }
    [[nodiscard]] auto resolutionTicket() const -> int { return m_resolution_ticket; }

    void setResolving(bool resolving, int resolutionTicket)
    {
        m_is_resolving = resolving;
        m_resolution_ticket = resolutionTicket;
    }

    // Delete all files of this resource.
    auto destroy(const QDir& index_dir, bool preserve_metadata = false, bool attempt_trash = true) -> bool;
    // Delete the metadata only.
    auto destroyMetadata(const QDir& index_dir) -> void;

    [[nodiscard]] auto isSymLink() const -> bool { return m_file_info.isSymLink(); }

    /**
     * @brief Take a instance path, checks if the file pointed to by the resource is a symlink or under a symlink in that instance
     *
     * @param instPath path to an instance directory
     * @return true
     * @return false
     */
    [[nodiscard]] bool isSymLinkUnder(const QString& instPath) const;

    [[nodiscard]] bool isMoreThanOneHardLink() const;

   protected:
    /* The file corresponding to this resource. */
    QFileInfo m_file_info;
    /* The cached date when this file was last changed. */
    QDateTime m_changed_date_time;

    /* Internal ID for internal purposes. Properties such as human-readability should not be assumed. */
    QString m_internal_id;
    /* Name as reported via the file name. In the absence of a better name, this is shown to the user. */
    QString m_name;

    /* The type of file we're dealing with. */
    ResourceType m_type = ResourceType::UNKNOWN;

    /* Installation status of the resource. */
    ResourceStatus m_status = ResourceStatus::UNKNOWN;

    std::shared_ptr<Metadata::ModStruct> m_metadata = nullptr;

    /* Whether the resource is enabled (e.g. shows up in the game) or not. */
    bool m_enabled = true;

    /* Used to keep trach of pending / concluded actions on the resource. */
    bool m_is_resolving = false;
    bool m_is_resolved = false;
    int m_resolution_ticket = 0;
    QString m_size_str;
    qint64 m_size_info;
};
