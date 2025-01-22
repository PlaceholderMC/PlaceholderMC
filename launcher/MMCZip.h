// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2023-2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include <quazip.h>
#include <quazip/JlCompress.h>
#include <QDir>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

#if defined(LAUNCHER_APPLICATION)
#include "minecraft/mod/Mod.h"
#endif
#include "tasks/Task.h"

#include <KArchive>
#include <KTar>
#include <KZip>
#include <QIODevice>
#include <QStack>
namespace MMCZip {
using FilterFunction = std::function<bool(const QString&)>;

/**
 * Merge two zip files, using a filter function
 */
bool mergeZipFiles(QuaZip* into, QFileInfo from, QSet<QString>& contained, const FilterFunction& filter = nullptr);

/**
 * Compress directory, by providing a list of files to compress
 * \param zip target archive
 * \param dir directory that will be compressed (to compress with relative paths)
 * \param files list of files to compress
 * \param followSymlinks should follow symlinks when compressing file data
 * \return true for success or false for failure
 */
bool compressDirFiles(QuaZip* zip, QString dir, QFileInfoList files, bool followSymlinks = false);

/**
 * Compress directory, by providing a list of files to compress
 * \param fileCompressed target archive file
 * \param dir directory that will be compressed (to compress with relative paths)
 * \param files list of files to compress
 * \param followSymlinks should follow symlinks when compressing file data
 * \return true for success or false for failure
 */
bool compressDirFiles(QString fileCompressed, QString dir, QFileInfoList files, bool followSymlinks = false);

#if defined(LAUNCHER_APPLICATION)
/**
 * take a source jar, add mods to it, resulting in target jar
 */
bool createModdedJar(QString sourceJarPath, QString targetJarPath, const QList<Mod*>& mods);
#endif
/**
 * Find a single file in archive by file name (not path)
 *
 * \param ignore_paths paths to skip when recursing the search
 *
 * \return the path prefix where the file is
 */
QString findFolderOfFileInZip(QuaZip* zip, const QString& what, const QStringList& ignore_paths = {}, const QString& root = QString(""));

/**
 * Find a multiple files of the same name in archive by file name
 * If a file is found in a path, no deeper paths are searched
 *
 * \return true if anything was found
 */
bool findFilesInZip(QuaZip* zip, const QString& what, QStringList& result, const QString& root = QString());

/**
 * Extract a subdirectory from an archive
 */
std::optional<QStringList> extractSubDir(QuaZip* zip, const QString& subdir, const QString& target);

bool extractRelFile(QuaZip* zip, const QString& file, const QString& target);

/**
 * Extract a whole archive.
 *
 * \param fileCompressed The name of the archive.
 * \param dir The directory to extract to, the current directory if left empty.
 * \return The list of the full paths of the files extracted, empty on failure.
 */
std::optional<QStringList> extractDir(QString fileCompressed, QString dir);

/**
 * Extract a subdirectory from an archive
 *
 * \param fileCompressed The name of the archive.
 * \param subdir The directory within the archive to extract
 * \param dir The directory to extract to, the current directory if left empty.
 * \return The list of the full paths of the files extracted, empty on failure.
 */
std::optional<QStringList> extractDir(QString fileCompressed, QString subdir, QString dir);

/**
 * Extract a single file from an archive into a directory
 *
 * \param fileCompressed The name of the archive.
 * \param file The file within the archive to extract
 * \param dir The directory to extract to, the current directory if left empty.
 * \return true for success or false for failure
 */
bool extractFile(QString fileCompressed, QString file, QString dir);

/**
 * Populate a QFileInfoList with a directory tree recursively, while allowing to excludeFilter what shouldn't be included.
 * \param rootDir directory to start off
 * \param subDir subdirectory, should be nullptr for first invocation
 * \param files resulting list of QFileInfo
 * \param excludeFilter function to excludeFilter which files shouldn't be included (returning true means to excude)
 * \return true for success or false for failure
 */
bool collectFileListRecursively(const QString& rootDir, const QString& subDir, QFileInfoList* files, FilterFunction excludeFilter);

#if defined(LAUNCHER_APPLICATION)
class ExportToZipTask : public Task {
    Q_OBJECT
   public:
    ExportToZipTask(QString outputPath,
                    QDir dir,
                    QFileInfoList files,
                    QString destinationPrefix = "",
                    bool followSymlinks = false,
                    bool utf8Enabled = false)
        : m_output_path(outputPath)
        , m_output(outputPath)
        , m_dir(dir)
        , m_files(files)
        , m_destination_prefix(destinationPrefix)
        , m_follow_symlinks(followSymlinks)
    {
        setAbortable(true);
        m_output.setUtf8Enabled(utf8Enabled);
    };
    ExportToZipTask(QString outputPath,
                    QString dir,
                    QFileInfoList files,
                    QString destinationPrefix = "",
                    bool followSymlinks = false,
                    bool utf8Enabled = false)
        : ExportToZipTask(outputPath, QDir(dir), files, destinationPrefix, followSymlinks, utf8Enabled) {};

    virtual ~ExportToZipTask() = default;

    void setExcludeFiles(QStringList excludeFiles) { m_exclude_files = excludeFiles; }
    void addExtraFile(QString fileName, QByteArray data) { m_extra_files.insert(fileName, data); }

    using ZipResult = std::optional<QString>;

   protected:
    virtual void executeTask() override;
    bool abort() override;

    ZipResult exportZip();
    void finish();

   private:
    QString m_output_path;
    QuaZip m_output;
    QDir m_dir;
    QFileInfoList m_files;
    QString m_destination_prefix;
    bool m_follow_symlinks;
    QStringList m_exclude_files;
    QHash<QString, QByteArray> m_extra_files;

    QFuture<ZipResult> m_build_zip_future;
    QFutureWatcher<ZipResult> m_build_zip_watcher;
};

class ExtractZipTask : public Task {
    Q_OBJECT
   public:
    ExtractZipTask(QString input, QDir outputDir, QString subdirectory = "")
        : ExtractZipTask(std::make_shared<QuaZip>(input), outputDir, subdirectory)
    {}
    ExtractZipTask(std::shared_ptr<QuaZip> input, QDir outputDir, QString subdirectory = "")
        : m_input(input), m_output_dir(outputDir), m_subdirectory(subdirectory)
    {}
    virtual ~ExtractZipTask() = default;

    using ZipResult = std::optional<QString>;

   protected:
    virtual void executeTask() override;
    bool abort() override;

    ZipResult extractZip();
    void finish();

   private:
    std::shared_ptr<QuaZip> m_input;
    QDir m_output_dir;
    QString m_subdirectory;

    QFuture<ZipResult> m_zip_future;
    QFutureWatcher<ZipResult> m_zip_watcher;
};

class ExtractKArchive : public Task {
   public:
    ExtractKArchive(QString fileName, QDir outputDir, QString subdirectory = "")
        : ExtractKArchive(newArchive(fileName), outputDir, subdirectory)
    {}
    ExtractKArchive(KArchive* a, QDir outputDir, QString subdirectory = "")
        : m_archive(a), m_output_dir(outputDir), m_subdirectory(subdirectory)
    {
        setAbortable(true);
    }
    ~ExtractKArchive()
    {
        if (m_archive) {
            if (m_archive->isOpen()) {
                m_archive->close();
            }
            delete m_archive;
        }
    }

    static KArchive* newArchive(QString filename)
    {
        if (filename.endsWith(".zip") || filename.endsWith(".jar")) {
            return new KZip(filename);
        }
        return new KTar(filename);
    }

   protected:
    static bool sortByPosition(const KArchiveFile* file1, const KArchiveFile* file2) { return file1->position() < file2->position(); }
    virtual void executeTask() override
    {
        m_needs_abort = false;
#define CAN_CONTINUE     \
    if (m_needs_abort) { \
        emitAborted();   \
        return;          \
    }
        if (!m_archive->isOpen()) {
            if (!m_archive->open(QIODevice::ReadOnly)) {
                emitFailed(tr("unable to open file"));
                return;
            }
        }
        CAN_CONTINUE;
        setStatus(tr("collect files"));
        QDir root;
        const QString destDir(m_output_dir.absolutePath());  // get directory path without any "." or ".."

        QList<const KArchiveFile*> fileList;
        QMap<qint64, QString> fileToDir;

        // placeholders for iterated items
        QStack<const KArchiveDirectory*> dirStack;
        QStack<QString> dirNameStack;

        auto kdir = m_archive->directory();
        if (!m_subdirectory.isEmpty()) {
            auto subDir = kdir->entry(m_subdirectory);
            kdir = nullptr;
            if (subDir && subDir->isDirectory()) {
                kdir = dynamic_cast<const KArchiveDirectory*>(subDir);
            }
            if (!kdir) {
                emitFailed(tr("unable to find subdirecotry"));
                return;
            }
        }
        CAN_CONTINUE;

        dirStack.push(kdir);         // init stack at current directory
        dirNameStack.push(destDir);  // ... with given path
        do {
            CAN_CONTINUE;
            const KArchiveDirectory* curDir = dirStack.pop();

            // extract only to specified folder if it is located within archive's extraction folder
            // otherwise put file under root position in extraction folder
            QString curDirName = dirNameStack.pop();
            if (!QDir(curDirName).absolutePath().startsWith(destDir)) {
                qWarning() << "Attempted export into folder" << curDirName << "which is outside of the extraction root folder" << destDir
                           << "."
                           << "Changing export of contained files to extraction root folder.";
                curDirName = destDir;
            }

            if (!root.mkpath(curDirName)) {
                emitFailed(tr("unable to create directory:%1").arg(curDirName));
                return;
            }

            const QStringList dirEntries = curDir->entries();
            for (QStringList::const_iterator it = dirEntries.begin(); it != dirEntries.end(); ++it) {
                CAN_CONTINUE;
                const KArchiveEntry* curEntry = curDir->entry(*it);
                if (!curEntry->symLinkTarget().isEmpty()) {
                    QString linkName = curDirName + QLatin1Char('/') + curEntry->name();
                    // To create a valid link on Windows, linkName must have a .lnk file extension.
#ifdef Q_OS_WIN
                    if (!linkName.endsWith(QLatin1String(".lnk"))) {
                        linkName += QLatin1String(".lnk");
                    }
#endif
                    QFile symLinkTarget(curEntry->symLinkTarget());
                    if (!symLinkTarget.link(linkName)) {
                        qWarning() << "symlink(" << curEntry->symLinkTarget() << ',' << linkName << ") failed";
                    }
                } else {
                    if (curEntry->isFile()) {
                        const KArchiveFile* curFile = dynamic_cast<const KArchiveFile*>(curEntry);
                        if (curFile) {
                            fileList.append(curFile);
                            fileToDir.insert(curFile->position(), curDirName);
                        }
                    }

                    if (curEntry->isDirectory()) {
                        const KArchiveDirectory* ad = dynamic_cast<const KArchiveDirectory*>(curEntry);
                        if (ad) {
                            dirStack.push(ad);
                            dirNameStack.push(curDirName + QLatin1Char('/') + curEntry->name());
                        }
                    }
                }
            }
        } while (!dirStack.isEmpty());

        std::sort(fileList.begin(), fileList.end(), sortByPosition);  // sort on d->pos, so we have a linear access
        setStatus(tr("extract files"));

        setProgress(0, fileList.size());
        for (auto f : fileList) {
            CAN_CONTINUE;
            setProgress(m_progress + 1, m_progressTotal);
            qint64 pos = f->position();
            if (!f->copyTo(fileToDir[pos])) {
                emitFailed(tr("failed to create file:%1").arg(fileToDir[pos]));
                return;
            }
        }
        emitSucceeded();
    };
    bool abort() override
    {
        m_needs_abort = true;
        return true;
    }

   private:
    KArchive* m_archive;
    QDir m_output_dir;
    QString m_subdirectory;

    bool m_needs_abort = false;
};
#endif
}  // namespace MMCZip
