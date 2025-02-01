// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2022 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include "Exception.h"
#include "pathmatcher/IPathMatcher.h"

#include <system_error>

#include <QDir>
#include <QFlags>
#include <QLocalServer>
#include <QObject>
#include <QPair>
#include <QThread>

namespace FS {

class FileSystemException : public ::Exception {
   public:
    FileSystemException(const QString& message) : Exception(message) {}
};

/**
 * write data to a file safely
 */
void write(const QString& filename, const QByteArray& data);

/**
 * append data to a file safely
 */
void appendSafe(const QString& filename, const QByteArray& data);

/**
 * append data to a file
 */
void append(const QString& filename, const QByteArray& data);

/**
 * read data from a file safely
 */
QByteArray read(const QString& filename);

/**
 * Update the last changed timestamp of an existing file
 */
bool updateTimestamp(const QString& filename);

/**
 * Creates all the folders in a path for the specified path
 * last segment of the path is treated as a file name and is ignored!
 */
bool ensureFilePathExists(QString filenamepath);

/**
 * Creates all the folders in a path for the specified path
 * last segment of the path is treated as a folder name and is created!
 */
bool ensureFolderPathExists(const QFileInfo folderPath);

/**
 * Creates all the folders in a path for the specified path
 * last segment of the path is treated as a folder name and is created!
 */
bool ensureFolderPathExists(const QString folderPathName);

/**
 * @brief Copies a directory and it's contents from src to dest
 */
class copy : public QObject {
    Q_OBJECT
   public:
    copy(const QString& src, const QString& dst, QObject* parent = nullptr) : QObject(parent)
    {
        m_src.setPath(src);
        m_dst.setPath(dst);
    }
    copy& followSymlinks(const bool follow)
    {
        m_followSymlinks = follow;
        return *this;
    }
    copy& matcher(const IPathMatcher* filter)
    {
        m_matcher = filter;
        return *this;
    }
    copy& whitelist(bool whitelist)
    {
        m_whitelist = whitelist;
        return *this;
    }
    copy& overwrite(const bool overwrite)
    {
        m_overwrite = overwrite;
        return *this;
    }

    bool operator()(bool dryRun = false) { return operator()(QString(), dryRun); }

    qsizetype totalCopied() { return m_copied; }
    qsizetype totalFailed() { return m_failedPaths.length(); }
    QStringList failed() { return m_failedPaths; }

   signals:
    void fileCopied(const QString& relativeName);
    void copyFailed(const QString& relativeName);
    // TODO: maybe add a "shouldCopy" signal in the future?

   private:
    bool operator()(const QString& offset, bool dryRun = false);

   private:
    bool m_followSymlinks = true;
    const IPathMatcher* m_matcher = nullptr;
    bool m_whitelist = false;
    bool m_overwrite = false;
    QDir m_src;
    QDir m_dst;
    qsizetype m_copied;
    QStringList m_failedPaths;
};

struct LinkPair {
    QString src;
    QString dst;
};

struct LinkResult {
    QString src;
    QString dst;
    QString err_msg;
    int err_value;
};

class ExternalLinkFileProcess : public QThread {
    Q_OBJECT
   public:
    ExternalLinkFileProcess(QString server, bool useHardLinks, QObject* parent = nullptr)
        : QThread(parent), m_useHardLinks(useHardLinks), m_server(server)
    {}

    void run() override
    {
        runLinkFile();
        emit processExited();
    }

   signals:
    void processExited();

   private:
    void runLinkFile();

    bool m_useHardLinks = false;

    QString m_server;
};

/**
 * @brief links (a file / a directory and it's contents) from src to dest
 */
class create_link : public QObject {
    Q_OBJECT
   public:
    create_link(const QList<LinkPair> path_pairs, QObject* parent = nullptr) : QObject(parent) { m_path_pairs.append(path_pairs); }
    create_link(const QString& src, const QString& dst, QObject* parent = nullptr) : QObject(parent)
    {
        LinkPair pair = { src, dst };
        m_path_pairs.append(pair);
    }
    create_link& useHardLinks(const bool useHard)
    {
        m_useHardLinks = useHard;
        return *this;
    }
    create_link& matcher(const IPathMatcher* filter)
    {
        m_matcher = filter;
        return *this;
    }
    create_link& whitelist(bool whitelist)
    {
        m_whitelist = whitelist;
        return *this;
    }
    create_link& linkRecursively(bool recursive)
    {
        m_recursive = recursive;
        return *this;
    }
    create_link& setMaxDepth(int depth)
    {
        m_max_depth = depth;
        return *this;
    }
    create_link& debug(bool d)
    {
        m_debug = d;
        return *this;
    }

    std::error_code getOSError() { return m_os_err; }

    bool operator()(bool dryRun = false) { return operator()(QString(), dryRun); }

    int totalLinked() { return m_linked; }
    int totalToLink() { return static_cast<int>(m_links_to_make.size()); }

    void runPrivileged() { runPrivileged(QString()); }
    void runPrivileged(const QString& offset);

    QList<LinkResult> getResults() { return m_path_results; }

   signals:
    void fileLinked(const QString& srcName, const QString& dstName);
    void linkFailed(const QString& srcName, const QString& dstName, const QString& err_msg, int err_value);
    void finished();
    void finishedPrivileged(bool gotResults);

   private:
    bool operator()(const QString& offset, bool dryRun = false);
    void make_link_list(const QString& offset);
    bool make_links();

   private:
    bool m_useHardLinks = false;
    const IPathMatcher* m_matcher = nullptr;
    bool m_whitelist = false;
    bool m_recursive = true;

    /// @brief >= -1 = infinite, 0 = link files at src/* to dest/*, 1 = link files at src/*/* to dest/*/*, etc.
    int m_max_depth = -1;

    QList<LinkPair> m_path_pairs;
    QList<LinkResult> m_path_results;
    QList<LinkPair> m_links_to_make;

    int m_linked;
    bool m_debug = false;
    std::error_code m_os_err;

    QLocalServer m_linkServer;
};

/**
 * @brief moves a file by renaming it
 * @param source source file path
 * @param dest destination filepath
 *
 */
bool move(const QString& source, const QString& dest);

/**
 * Delete a folder recursively
 */
bool deletePath(QString path);

/**
 * Trash a folder / file
 */
bool trash(QString path, QString* pathInTrash = nullptr);

QString PathCombine(const QString& path1, const QString& path2);
QString PathCombine(const QString& path1, const QString& path2, const QString& path3);
QString PathCombine(const QString& path1, const QString& path2, const QString& path3, const QString& path4);

QString AbsolutePath(const QString& path);

/**
 * @brief depth of path. "foo.txt" -> 0 , "bar/foo.txt" -> 1, /baz/bar/foo.txt -> 2, etc.
 *
 * @param path path to measure
 * @return int number of components before base path
 */
int pathDepth(const QString& path);

/**
 * @brief  cut off segments of path until it is a max of length depth
 *
 * @param path path to truncate
 * @param depth max depth of new path
 * @return QString truncated path
 */
QString pathTruncate(const QString& path, int depth);

/**
 * Resolve an executable
 *
 * Will resolve:
 *   single executable (by name)
 *   relative path
 *   absolute path
 *
 * @return absolute path to executable or null string
 */
QString ResolveExecutable(QString path);

/**
 * Normalize path
 *
 * Any paths inside the current directory will be normalized to relative paths (to current)
 * Other paths will be made absolute
 *
 * Returns false if the path logic somehow filed (and normalizedPath in invalid)
 */
QString NormalizePath(QString path);

QString RemoveInvalidFilenameChars(QString string, QChar replaceWith = '-');

QString RemoveInvalidPathChars(QString string, QChar replaceWith = '-');

QString DirNameFromString(QString string, QString inDir = ".");

/// Checks if the a given Path contains "!"
bool checkProblemticPathJava(QDir folder);

// Get the Directory representing the User's Desktop
QString getDesktopDir();

// Get the Directory representing the User's Applications directory
QString getApplicationsDir();

// Overrides one folder with the contents of another, preserving items exclusive to the first folder
// Equivalent to doing QDir::rename, but allowing for overrides
bool overrideFolder(QString overwritten_path, QString override_path);

/**
 * Creates a shortcut to the specified target file at the specified destination path.
 */
bool createShortcut(QString destination, QString target, QStringList args, QString name, QString icon);

enum class FilesystemType {
    FAT,
    NTFS,
    REFS,
    EXT,
    EXT_2_OLD,
    EXT_2_3_4,
    XFS,
    BTRFS,
    NFS,
    ZFS,
    APFS,
    HFS,
    HFSPLUS,
    HFSX,
    FUSEBLK,
    F2FS,
    BCACHEFS,
    UNKNOWN
};

/**
 * @brief Ordered Mapping of enum types to reported filesystem names
 * this mapping is non exsaustive, it just attempts to capture the filesystems which could be reasonalbly be in use .
 * all string values are in uppercase, use `QString.toUpper()` or equivalent during lookup.
 *
 * QMap is ordered
 *
 */
static const QMap<FilesystemType, QStringList> s_filesystem_type_names = { { FilesystemType::FAT, { "FAT" } },
                                                                           { FilesystemType::NTFS, { "NTFS" } },
                                                                           { FilesystemType::REFS, { "REFS" } },
                                                                           { FilesystemType::EXT_2_OLD, { "EXT_2_OLD", "EXT2_OLD" } },
                                                                           { FilesystemType::EXT_2_3_4,
                                                                             { "EXT2/3/4", "EXT_2_3_4", "EXT2", "EXT3", "EXT4" } },
                                                                           { FilesystemType::EXT, { "EXT" } },
                                                                           { FilesystemType::XFS, { "XFS" } },
                                                                           { FilesystemType::BTRFS, { "BTRFS" } },
                                                                           { FilesystemType::NFS, { "NFS" } },
                                                                           { FilesystemType::ZFS, { "ZFS" } },
                                                                           { FilesystemType::APFS, { "APFS" } },
                                                                           { FilesystemType::HFS, { "HFS" } },
                                                                           { FilesystemType::HFSPLUS, { "HFSPLUS" } },
                                                                           { FilesystemType::HFSX, { "HFSX" } },
                                                                           { FilesystemType::FUSEBLK, { "FUSEBLK" } },
                                                                           { FilesystemType::F2FS, { "F2FS" } },
                                                                           { FilesystemType::BCACHEFS, { "BCACHEFS" } },
                                                                           { FilesystemType::UNKNOWN, { "UNKNOWN" } } };

/**
 * @brief Get the string name of Filesystem enum object
 *
 * @param type
 * @return QString
 */
QString getFilesystemTypeName(FilesystemType type);

/**
 * @brief Get the Filesystem enum object from a name
 *  Does a lookup of the type name and returns an exact match
 *
 * @param name
 * @return FilesystemType
 */
FilesystemType getFilesystemType(const QString& name);

/**
 * @brief Get the Filesystem enum object from a name
 *  Does a fuzzy lookup of the type name and returns an apropreate match
 *
 * @param name
 * @return FilesystemType
 */
FilesystemType getFilesystemTypeFuzzy(const QString& name);

struct FilesystemInfo {
    FilesystemType fsType = FilesystemType::UNKNOWN;
    QString fsTypeName;
    int blockSize;
    qint64 bytesAvailable;
    qint64 bytesFree;
    qint64 bytesTotal;
    QString name;
    QString rootPath;
};

/**
 * @brief path to the near ancestor that exists
 *
 */
QString nearestExistentAncestor(const QString& path);

/**
 * @brief colect information about the filesystem under a file
 *
 */
FilesystemInfo statFS(const QString& path);

static const QList<FilesystemType> s_clone_filesystems = { FilesystemType::BTRFS, FilesystemType::APFS, FilesystemType::ZFS,
                                                           FilesystemType::XFS,   FilesystemType::REFS, FilesystemType::BCACHEFS };

/**
 * @brief if the Filesystem is reflink/clone capable
 *
 */
bool canCloneOnFS(const QString& path);
bool canCloneOnFS(const FilesystemInfo& info);
bool canCloneOnFS(FilesystemType type);

/**
 * @brief if the Filesystems are reflink/clone capable and both are on the same device
 *
 */
bool canClone(const QString& src, const QString& dst);

/**
 * @brief Copies a directory and it's contents from src to dest
 */
class clone : public QObject {
    Q_OBJECT
   public:
    clone(const QString& src, const QString& dst, QObject* parent = nullptr) : QObject(parent)
    {
        m_src.setPath(src);
        m_dst.setPath(dst);
    }
    clone& matcher(const IPathMatcher* filter)
    {
        m_matcher = filter;
        return *this;
    }
    clone& whitelist(bool whitelist)
    {
        m_whitelist = whitelist;
        return *this;
    }

    bool operator()(bool dryRun = false) { return operator()(QString(), dryRun); }

    qsizetype totalCloned() { return m_cloned; }
    qsizetype totalFailed() { return m_failedClones.length(); }

    QList<QPair<QString, QString>> failed() { return m_failedClones; }

   signals:
    void fileCloned(const QString& src, const QString& dst);
    void cloneFailed(const QString& src, const QString& dst);

   private:
    bool operator()(const QString& offset, bool dryRun = false);

   private:
    const IPathMatcher* m_matcher = nullptr;
    bool m_whitelist = false;
    QDir m_src;
    QDir m_dst;
    qsizetype m_cloned;
    QList<QPair<QString, QString>> m_failedClones;
};

/**
 * @brief clone/reflink file from src to dst
 *
 */
bool clone_file(const QString& src, const QString& dst, std::error_code& ec);

#if defined(Q_OS_WIN)
bool win_ioctl_clone(const std::wstring& src_path, const std::wstring& dst_path, std::error_code& ec);
#elif defined(Q_OS_LINUX)
bool linux_ficlone(const std::string& src_path, const std::string& dst_path, std::error_code& ec);
#elif defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
bool macos_bsd_clonefile(const std::string& src_path, const std::string& dst_path, std::error_code& ec);
#endif

static const QList<FilesystemType> s_non_link_filesystems = {
    FilesystemType::FAT,
};

/**
 * @brief if the Filesystem is symlink capable
 *
 */
bool canLinkOnFS(const QString& path);
bool canLinkOnFS(const FilesystemInfo& info);
bool canLinkOnFS(FilesystemType type);

/**
 * @brief if the Filesystem is symlink capable on both ends
 *
 */
bool canLink(const QString& src, const QString& dst);

uintmax_t hardLinkCount(const QString& path);

#ifdef Q_OS_WIN
QString getPathNameInLocal8bit(const QString& file);
#endif

QString getUniqueResourceName(const QString& filePath);

}  // namespace FS
