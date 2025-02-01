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

#include "FileSystem.h"
#include <QPair>

#include "BuildConfig.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QUrl>
#include <QtNetwork>
#include <system_error>

#include "DesktopServices.h"
#include "PSaveFile.h"
#include "StringUtils.h"

#if defined Q_OS_WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sys/utime.h>
#include <versionhelpers.h>
#include <windows.h>
#include <winnls.h>
#include <string>
// for ShellExecute
#include <Shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#else
#include <utime.h>
#endif

// Snippet from https://github.com/gulrak/filesystem#using-it-as-single-file-header

#ifdef __APPLE__
#include <Availability.h>  // for deployment target to support pre-catalina targets without std::fs
#endif                     // __APPLE__

#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (defined(__cplusplus) && __cplusplus >= 201703L)) && defined(__has_include)
#if __has_include(<filesystem>) && (!defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif  // MacOS min version check
#endif  // Other OSes version check

#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

// clone
#if defined(Q_OS_LINUX)
#include <errno.h>
#include <fcntl.h> /* Definition of FICLONE* constants */
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <sys/attr.h>
#include <sys/clonefile.h>
#elif defined(Q_OS_WIN)
// winbtrfs clone vs rundll32 shellbtrfs.dll,ReflinkCopy
#include <fileapi.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
// refs
#include <winioctl.h>
#if defined(__MINGW32__)
#include <crtdbg.h>
#endif
#endif

#if defined(Q_OS_WIN)

#if defined(__MINGW32__)

struct _DUPLICATE_EXTENTS_DATA {
    HANDLE FileHandle;
    LARGE_INTEGER SourceFileOffset;
    LARGE_INTEGER TargetFileOffset;
    LARGE_INTEGER ByteCount;
};

using DUPLICATE_EXTENTS_DATA = _DUPLICATE_EXTENTS_DATA;
using PDUPLICATE_EXTENTS_DATA = _DUPLICATE_EXTENTS_DATA*;

struct _FSCTL_GET_INTEGRITY_INFORMATION_BUFFER {
    WORD ChecksumAlgorithm;  // Checksum algorithm. e.g. CHECKSUM_TYPE_UNCHANGED, CHECKSUM_TYPE_NONE, CHECKSUM_TYPE_CRC32
    WORD Reserved;           // Must be 0
    DWORD Flags;             // FSCTL_INTEGRITY_FLAG_xxx
    DWORD ChecksumChunkSizeInBytes;
    DWORD ClusterSizeInBytes;
};

using FSCTL_GET_INTEGRITY_INFORMATION_BUFFER = _FSCTL_GET_INTEGRITY_INFORMATION_BUFFER;
using PFSCTL_GET_INTEGRITY_INFORMATION_BUFFER = _FSCTL_GET_INTEGRITY_INFORMATION_BUFFER*;

struct _FSCTL_SET_INTEGRITY_INFORMATION_BUFFER {
    WORD ChecksumAlgorithm;  // Checksum algorithm. e.g. CHECKSUM_TYPE_UNCHANGED, CHECKSUM_TYPE_NONE, CHECKSUM_TYPE_CRC32
    WORD Reserved;           // Must be 0
    DWORD Flags;             // FSCTL_INTEGRITY_FLAG_xxx
};

using FSCTL_SET_INTEGRITY_INFORMATION_BUFFER = _FSCTL_SET_INTEGRITY_INFORMATION_BUFFER;
using PFSCTL_SET_INTEGRITY_INFORMATION_BUFFER = _FSCTL_SET_INTEGRITY_INFORMATION_BUFFER*;

#endif

#ifndef FSCTL_DUPLICATE_EXTENTS_TO_FILE
#define FSCTL_DUPLICATE_EXTENTS_TO_FILE CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 209, METHOD_BUFFERED, FILE_WRITE_DATA)
#endif

#ifndef FSCTL_GET_INTEGRITY_INFORMATION
#define FSCTL_GET_INTEGRITY_INFORMATION \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 159, METHOD_BUFFERED, FILE_ANY_ACCESS)  // FSCTL_GET_INTEGRITY_INFORMATION_BUFFER
#endif

#ifndef FSCTL_SET_INTEGRITY_INFORMATION
#define FSCTL_SET_INTEGRITY_INFORMATION \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 160, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)  // FSCTL_SET_INTEGRITY_INFORMATION_BUFFER
#endif

#ifndef ERROR_NOT_CAPABLE
#define ERROR_NOT_CAPABLE 775L
#endif

#ifndef ERROR_BLOCK_TOO_MANY_REFERENCES
#define ERROR_BLOCK_TOO_MANY_REFERENCES 347L
#endif

#endif

namespace FS {

void ensureExists(const QDir& dir)
{
    if (!QDir().mkpath(dir.absolutePath())) {
        throw FileSystemException("Unable to create folder " + dir.dirName() + " (" + dir.absolutePath() + ")");
    }
}

void write(const QString& filename, const QByteArray& data)
{
    ensureExists(QFileInfo(filename).dir());
    PSaveFile file(filename);
    if (!file.open(PSaveFile::WriteOnly)) {
        throw FileSystemException("Couldn't open " + filename + " for writing: " + file.errorString());
    }
    if (data.size() != file.write(data)) {
        throw FileSystemException("Error writing data to " + filename + ": " + file.errorString());
    }
    if (!file.commit()) {
        throw FileSystemException("Error while committing data to " + filename + ": " + file.errorString());
    }
}

void appendSafe(const QString& filename, const QByteArray& data)
{
    ensureExists(QFileInfo(filename).dir());
    QByteArray buffer;
    try {
        buffer = read(filename);
    } catch (FileSystemException&) {
        buffer = QByteArray();
    }
    buffer.append(data);
    PSaveFile file(filename);
    if (!file.open(PSaveFile::WriteOnly)) {
        throw FileSystemException("Couldn't open " + filename + " for writing: " + file.errorString());
    }
    if (buffer.size() != file.write(buffer)) {
        throw FileSystemException("Error writing data to " + filename + ": " + file.errorString());
    }
    if (!file.commit()) {
        throw FileSystemException("Error while committing data to " + filename + ": " + file.errorString());
    }
}

void append(const QString& filename, const QByteArray& data)
{
    ensureExists(QFileInfo(filename).dir());
    QFile file(filename);
    if (!file.open(QFile::Append)) {
        throw FileSystemException("Couldn't open " + filename + " for writing: " + file.errorString());
    }
    if (data.size() != file.write(data)) {
        throw FileSystemException("Error writing data to " + filename + ": " + file.errorString());
    }
}

QByteArray read(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        throw FileSystemException("Unable to open " + filename + " for reading: " + file.errorString());
    }
    const qint64 size = file.size();
    QByteArray data(int(size), 0);
    const qint64 ret = file.read(data.data(), size);
    if (ret == -1 || ret != size) {
        throw FileSystemException("Error reading data from " + filename + ": " + file.errorString());
    }
    return data;
}

bool updateTimestamp(const QString& filename)
{
#ifdef Q_OS_WIN32
    std::wstring filename_utf_16 = filename.toStdWString();
    return (_wutime64(filename_utf_16.c_str(), nullptr) == 0);
#else
    QByteArray filenameBA = QFile::encodeName(filename);
    return (utime(filenameBA.data(), nullptr) == 0);
#endif
}

bool ensureFilePathExists(QString filenamepath)
{
    QFileInfo a(filenamepath);
    QDir dir;
    QString ensuredPath = a.path();
    bool success = dir.mkpath(ensuredPath);
    return success;
}

bool ensureFolderPathExists(const QFileInfo folderPath)
{
    QDir dir;
    QString ensuredPath = folderPath.filePath();
    if (folderPath.exists())
        return true;

    bool success = dir.mkpath(ensuredPath);
    return success;
}

bool ensureFolderPathExists(const QString folderPathName)
{
    return ensureFolderPathExists(QFileInfo(folderPathName));
}

bool copyFileAttributes(QString src, QString dst)
{
#ifdef Q_OS_WIN32
    auto attrs = GetFileAttributesW(src.toStdWString().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return SetFileAttributesW(dst.toStdWString().c_str(), attrs);
#endif
    return true;
}

// needs folders to exists
void copyFolderAttributes(QString src, QString dst, QString relative)
{
    auto path = PathCombine(src, relative);
    QDir dsrc(src);
    while ((path = QFileInfo(path).path()).length() >= src.length()) {
        auto dst_path = PathCombine(dst, dsrc.relativeFilePath(path));
        copyFileAttributes(path, dst_path);
    }
}

/**
 * @brief Copies a directory and it's contents from src to dest
 * @param offset subdirectory form src to copy to dest
 * @return if there was an error during the filecopy
 */
bool copy::operator()(const QString& offset, bool dryRun)
{
    using copy_opts = fs::copy_options;
    m_copied = 0;  // reset counter
    m_failedPaths.clear();

// NOTE always deep copy on windows. the alternatives are too messy.
#if defined Q_OS_WIN32
    m_followSymlinks = true;
#endif

    auto src = PathCombine(m_src.absolutePath(), offset);
    auto dst = PathCombine(m_dst.absolutePath(), offset);

    std::error_code err;

    fs::copy_options opt = copy_opts::none;

    // The default behavior is to follow symlinks
    if (!m_followSymlinks)
        opt |= copy_opts::copy_symlinks;

    if (m_overwrite)
        opt |= copy_opts::overwrite_existing;

    // Function that'll do the actual copying
    auto copy_file = [this, dryRun, src, dst, opt, &err](QString src_path, QString relative_dst_path) {
        if (m_matcher && (m_matcher->matches(relative_dst_path) != m_whitelist))
            return;

        auto dst_path = PathCombine(dst, relative_dst_path);
        if (!dryRun) {
            ensureFilePathExists(dst_path);
#ifdef Q_OS_WIN32
            copyFolderAttributes(src, dst, relative_dst_path);
#endif
            fs::copy(StringUtils::toStdString(src_path), StringUtils::toStdString(dst_path), opt, err);
        }
        if (err) {
            qWarning() << "Failed to copy files:" << QString::fromStdString(err.message());
            qDebug() << "Source file:" << src_path;
            qDebug() << "Destination file:" << dst_path;
            m_failedPaths.append(dst_path);
            emit copyFailed(relative_dst_path);
            return;
        }
        m_copied++;
        emit fileCopied(relative_dst_path);
    };

    // We can't use copy_opts::recursive because we need to take into account the
    // blacklisted paths, so we iterate over the source directory, and if there's no blacklist
    // match, we copy the file.
    QDir src_dir(src);
    QDirIterator source_it(src, QDir::Filter::Files | QDir::Filter::Hidden, QDirIterator::Subdirectories);

    while (source_it.hasNext()) {
        auto src_path = source_it.next();
        auto relative_path = src_dir.relativeFilePath(src_path);

        copy_file(src_path, relative_path);
    }

    // If the root src is not a directory, the previous iterator won't run.
    if (!fs::is_directory(StringUtils::toStdString(src)))
        copy_file(src, "");

    return err.value() == 0;
}

/// qDebug print support for the LinkPair struct
QDebug operator<<(QDebug debug, const LinkPair& lp)
{
    QDebugStateSaver saver(debug);

    debug.nospace() << "LinkPair{ src: " << lp.src << " , dst: " << lp.dst << " }";
    return debug;
}

bool create_link::operator()(const QString& offset, bool dryRun)
{
    m_linked = 0;  // reset counter
    m_path_results.clear();
    m_links_to_make.clear();

    m_path_results.clear();

    make_link_list(offset);

    if (!dryRun)
        return make_links();

    return true;
}

/**
 * @brief Make a list of all the links to make
 * @param offset subdirectory of src to link to dest
 */
void create_link::make_link_list(const QString& offset)
{
    for (auto pair : m_path_pairs) {
        const QString& srcPath = pair.src;
        const QString& dstPath = pair.dst;

        auto src = PathCombine(QDir(srcPath).absolutePath(), offset);
        auto dst = PathCombine(QDir(dstPath).absolutePath(), offset);

        // you can't hard link a directory so make sure if we deal with a directory we do so recursively
        if (m_useHardLinks)
            m_recursive = true;

        // Function that'll do the actual linking
        auto link_file = [this, dst](QString src_path, QString relative_dst_path) {
            if (m_matcher && (m_matcher->matches(relative_dst_path) != m_whitelist)) {
                qDebug() << "path" << relative_dst_path << "in black list or not in whitelist";
                return;
            }

            auto dst_path = PathCombine(dst, relative_dst_path);
            LinkPair link = { src_path, dst_path };
            m_links_to_make.append(link);
        };

        if ((!m_recursive) || !fs::is_directory(StringUtils::toStdString(src))) {
            if (m_debug)
                qDebug() << "linking single file or dir:" << src << "to" << dst;
            link_file(src, "");
        } else {
            if (m_debug)
                qDebug() << "linking recursively:" << src << "to" << dst << ", max_depth:" << m_max_depth;
            QDir src_dir(src);
            QDirIterator source_it(src, QDir::Filter::Files | QDir::Filter::Hidden, QDirIterator::Subdirectories);

            QStringList linkedPaths;

            while (source_it.hasNext()) {
                auto src_path = source_it.next();
                auto relative_path = src_dir.relativeFilePath(src_path);

                if (m_max_depth >= 0 && pathDepth(relative_path) > m_max_depth) {
                    relative_path = pathTruncate(relative_path, m_max_depth);
                    src_path = src_dir.filePath(relative_path);
                    if (linkedPaths.contains(src_path)) {
                        continue;
                    }
                }

                linkedPaths.append(src_path);

                link_file(src_path, relative_path);
            }
        }
    }
}

bool create_link::make_links()
{
    for (auto link : m_links_to_make) {
        QString src_path = link.src;
        QString dst_path = link.dst;
        auto src_path_std = StringUtils::toStdString(link.src);
        auto dst_path_std = StringUtils::toStdString(link.dst);

        ensureFilePathExists(dst_path);
        if (m_useHardLinks) {
            if (m_debug)
                qDebug() << "making hard link:" << src_path << "to" << dst_path;
            fs::create_hard_link(src_path_std, dst_path_std, m_os_err);
        } else if (fs::is_directory(src_path_std)) {
            if (m_debug)
                qDebug() << "making directory_symlink:" << src_path << "to" << dst_path;
            fs::create_directory_symlink(src_path_std, dst_path_std, m_os_err);
        } else {
            if (m_debug)
                qDebug() << "making symlink:" << src_path << "to" << dst_path;
            fs::create_symlink(src_path_std, dst_path_std, m_os_err);
        }

        if (m_os_err) {
            qWarning() << "Failed to link files:" << QString::fromStdString(m_os_err.message());
            qDebug() << "Source file:" << src_path;
            qDebug() << "Destination file:" << dst_path;
            qDebug() << "Error category:" << m_os_err.category().name();
            qDebug() << "Error code:" << m_os_err.value();
            emit linkFailed(src_path, dst_path, QString::fromStdString(m_os_err.message()), m_os_err.value());
        } else {
            m_linked++;
            emit fileLinked(src_path, dst_path);
        }
        if (m_os_err)
            return false;
    }
    return true;
}

void create_link::runPrivileged(const QString& offset)
{
    m_linked = 0;  // reset counter
    m_path_results.clear();
    m_links_to_make.clear();

    bool gotResults = false;

    make_link_list(offset);

    QString serverName = BuildConfig.LAUNCHER_APP_BINARY_NAME + "_filelink_server" + StringUtils::getRandomAlphaNumeric();

    connect(&m_linkServer, &QLocalServer::newConnection, this, [this, &gotResults]() {
        qDebug() << "Client connected, sending out pairs";
        // construct block of data to send
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);

        qint32 blocksize = quint32(sizeof(quint32));
        for (auto link : m_links_to_make) {
            blocksize += quint32(link.src.size());
            blocksize += quint32(link.dst.size());
        }
        qDebug() << "About to write block of size:" << blocksize;
        out << blocksize;

        out << quint32(m_links_to_make.length());
        for (auto link : m_links_to_make) {
            out << link.src;
            out << link.dst;
        }

        QLocalSocket* clientConnection = m_linkServer.nextPendingConnection();
        connect(clientConnection, &QLocalSocket::disconnected, clientConnection, &QLocalSocket::deleteLater);

        connect(clientConnection, &QLocalSocket::readyRead, this, [&, clientConnection]() {
            QDataStream in;
            quint32 blockSize = 0;
            in.setDevice(clientConnection);

            qDebug() << "Reading path results from client";
            qDebug() << "bytes available" << clientConnection->bytesAvailable();

            // Relies on the fact that QDataStream serializes a quint32 into
            // sizeof(quint32) bytes
            if (clientConnection->bytesAvailable() < (int)sizeof(quint32))
                return;
            qDebug() << "reading block size";
            in >> blockSize;

            qDebug() << "blocksize is" << blockSize;
            qDebug() << "bytes available" << clientConnection->bytesAvailable();
            if (clientConnection->bytesAvailable() < blockSize || in.atEnd())
                return;

            quint32 numResults;
            in >> numResults;
            qDebug() << "numResults" << numResults;

            for (quint32 i = 0; i < numResults; i++) {
                FS::LinkResult result;
                in >> result.src;
                in >> result.dst;
                in >> result.err_msg;
                qint32 err_value;
                in >> err_value;
                result.err_value = err_value;
                if (result.err_value) {
                    qDebug() << "privileged link fail" << result.src << "to" << result.dst << "code" << result.err_value << result.err_msg;
                    emit linkFailed(result.src, result.dst, result.err_msg, result.err_value);
                } else {
                    qDebug() << "privileged link success" << result.src << "to" << result.dst;
                    m_linked++;
                    emit fileLinked(result.src, result.dst);
                }
                m_path_results.append(result);
            }
            gotResults = true;
            qDebug() << "results received, closing connection";
            clientConnection->close();
        });

        qint64 byteswritten = clientConnection->write(block);
        bool bytesflushed = clientConnection->flush();
        qDebug() << "block flushed" << byteswritten << bytesflushed;
    });

    qDebug() << "Listening on pipe" << serverName;
    if (!m_linkServer.listen(serverName)) {
        qDebug() << "Unable to start local pipe server on" << serverName << ":" << m_linkServer.errorString();
        return;
    }

    ExternalLinkFileProcess* linkFileProcess = new ExternalLinkFileProcess(serverName, m_useHardLinks, this);
    connect(linkFileProcess, &ExternalLinkFileProcess::processExited, this, [this, gotResults]() { emit finishedPrivileged(gotResults); });
    connect(linkFileProcess, &ExternalLinkFileProcess::finished, linkFileProcess, &QObject::deleteLater);

    linkFileProcess->start();
}

void ExternalLinkFileProcess::runLinkFile()
{
    QString fileLinkExe =
        PathCombine(QCoreApplication::instance()->applicationDirPath(), BuildConfig.LAUNCHER_APP_BINARY_NAME + "_filelink");
    QString params = "-s " + m_server;

    params += " -H " + QVariant(m_useHardLinks).toString();

#if defined Q_OS_WIN32
    SHELLEXECUTEINFO ShExecInfo;

    fileLinkExe = fileLinkExe + ".exe";

    qDebug() << "Running: runas" << fileLinkExe << params;

    LPCWSTR programNameWin = (const wchar_t*)fileLinkExe.utf16();
    LPCWSTR paramsWin = (const wchar_t*)params.utf16();

    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfoa
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;  // Optional. A handle to the owner window, used to display and position any UI that the system might produce
                             // while executing this function.
    ShExecInfo.lpVerb = L"runas";  // elevate to admin, show UAC
    ShExecInfo.lpFile = programNameWin;
    ShExecInfo.lpParameters = paramsWin;
    ShExecInfo.lpDirectory = NULL;
    ShExecInfo.nShow = SW_HIDE;
    ShExecInfo.hInstApp = NULL;

    ShellExecuteEx(&ShExecInfo);

    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
    CloseHandle(ShExecInfo.hProcess);
#endif

    qDebug() << "Process exited";
}

bool moveByCopy(const QString& source, const QString& dest)
{
    if (!copy(source, dest)()) {  // copy
        qDebug() << "Copy of" << source << "to" << dest << "failed!";
        return false;
    }
    if (!deletePath(source)) {  // remove original
        qDebug() << "Deletion of" << source << "failed!";
        return false;
    };
    return true;
}

bool move(const QString& source, const QString& dest)
{
    std::error_code err;

    ensureFilePathExists(dest);
    fs::rename(StringUtils::toStdString(source), StringUtils::toStdString(dest), err);

    if (err.value() != 0) {
        if (moveByCopy(source, dest))
            return true;
        qDebug() << "Move of" << source << "to" << dest << "failed!";
        qWarning() << "Failed to move file:" << QString::fromStdString(err.message()) << QString::number(err.value());
        return false;
    }
    return true;
}

bool deletePath(QString path)
{
    std::error_code err;

    fs::remove_all(StringUtils::toStdString(path), err);

    if (err) {
        qWarning() << "Failed to remove files:" << QString::fromStdString(err.message());
    }

    return err.value() == 0;
}

bool trash(QString path, QString* pathInTrash)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    return false;
#else
    // FIXME: Figure out trash in Flatpak. Qt seemingly doesn't use the Trash portal
    if (DesktopServices::isFlatpak())
        return false;
#if defined Q_OS_WIN32
    if (IsWindowsServer())
        return false;
#endif
    return QFile::moveToTrash(path, pathInTrash);
#endif
}

QString PathCombine(const QString& path1, const QString& path2)
{
    if (!path1.size())
        return path2;
    if (!path2.size())
        return path1;
    return QDir::cleanPath(path1 + QDir::separator() + path2);
}

QString PathCombine(const QString& path1, const QString& path2, const QString& path3)
{
    return PathCombine(PathCombine(path1, path2), path3);
}

QString PathCombine(const QString& path1, const QString& path2, const QString& path3, const QString& path4)
{
    return PathCombine(PathCombine(path1, path2, path3), path4);
}

QString AbsolutePath(const QString& path)
{
    return QFileInfo(path).absolutePath();
}

int pathDepth(const QString& path)
{
    if (path.isEmpty())
        return 0;

    QFileInfo info(path);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    auto parts = QDir::toNativeSeparators(info.path()).split(QDir::separator(), QString::SkipEmptyParts);
#else
    auto parts = QDir::toNativeSeparators(info.path()).split(QDir::separator(), Qt::SkipEmptyParts);
#endif

    int numParts = parts.length();
    numParts -= parts.count(".");
    numParts -= parts.count("..") * 2;

    return numParts;
}

QString pathTruncate(const QString& path, int depth)
{
    if (path.isEmpty() || (depth < 0))
        return "";

    QString trunc = QFileInfo(path).path();

    if (pathDepth(trunc) > depth) {
        return pathTruncate(trunc, depth);
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    auto parts = QDir::toNativeSeparators(trunc).split(QDir::separator(), QString::SkipEmptyParts);
#else
    auto parts = QDir::toNativeSeparators(trunc).split(QDir::separator(), Qt::SkipEmptyParts);
#endif

    if (parts.startsWith(".") && !path.startsWith(".")) {
        parts.removeFirst();
    }
    if (QDir::toNativeSeparators(path).startsWith(QDir::separator())) {
        parts.prepend("");
    }

    trunc = parts.join(QDir::separator());

    return trunc;
}

QString ResolveExecutable(QString path)
{
    if (path.isEmpty()) {
        return QString();
    }
    if (!path.contains('/')) {
        path = QStandardPaths::findExecutable(path);
    }
    QFileInfo pathInfo(path);
    if (!pathInfo.exists() || !pathInfo.isExecutable()) {
        return QString();
    }
    return pathInfo.absoluteFilePath();
}

/**
 * Normalize path
 *
 * Any paths inside the current folder will be normalized to relative paths (to current)
 * Other paths will be made absolute
 */
QString NormalizePath(QString path)
{
    QDir a = QDir::currentPath();
    QString currentAbsolute = a.absolutePath();

    QDir b(path);
    QString newAbsolute = b.absolutePath();

    if (newAbsolute.startsWith(currentAbsolute)) {
        return a.relativeFilePath(newAbsolute);
    } else {
        return newAbsolute;
    }
}

static const QString BAD_WIN_CHARS = "<>:\"|?*\r\n";
static const QString BAD_NTFS_CHARS = "<>:\"|?*";
static const QString BAD_HFS_CHARS = ":";

static const QString BAD_FILENAME_CHARS = BAD_WIN_CHARS + "\\/";

QString RemoveInvalidFilenameChars(QString string, QChar replaceWith)
{
    for (int i = 0; i < string.length(); i++)
        if (string.at(i) < ' ' || BAD_FILENAME_CHARS.contains(string.at(i)))
            string[i] = replaceWith;
    return string;
}

QString RemoveInvalidPathChars(QString path, QChar replaceWith)
{
    QString invalidChars;
#ifdef Q_OS_WIN
    invalidChars = BAD_WIN_CHARS;
#endif

    // the null character is ignored in this check as it was not a problem until now
    switch (statFS(path).fsType) {
        case FilesystemType::FAT:  // similar to NTFS
        /* fallthrough */
        case FilesystemType::NTFS:
        /* fallthrough */
        case FilesystemType::REFS:  // similar to NTFS(should be available only on windows)
            invalidChars += BAD_NTFS_CHARS;
            break;
        // case FilesystemType::EXT:
        // case FilesystemType::EXT_2_OLD:
        // case FilesystemType::EXT_2_3_4:
        // case FilesystemType::XFS:
        // case FilesystemType::BTRFS:
        // case FilesystemType::NFS:
        // case FilesystemType::ZFS:
        case FilesystemType::APFS:
        /* fallthrough */
        case FilesystemType::HFS:
        /* fallthrough */
        case FilesystemType::HFSPLUS:
        /* fallthrough */
        case FilesystemType::HFSX:
            invalidChars += BAD_HFS_CHARS;
            break;
        // case FilesystemType::FUSEBLK:
        // case FilesystemType::F2FS:
        // case FilesystemType::UNKNOWN:
        default:
            break;
    }

    if (invalidChars.size() != 0) {
        for (int i = 0; i < path.length(); i++) {
            if (path.at(i) < ' ' || invalidChars.contains(path.at(i))) {
                path[i] = replaceWith;
            }
        }
    }

    return path;
}

QString DirNameFromString(QString string, QString inDir)
{
    int num = 0;
    QString baseName = RemoveInvalidFilenameChars(string, '-');
    QString dirName;
    do {
        if (num == 0) {
            dirName = baseName;
        } else {
            dirName = baseName + "(" + QString::number(num) + ")";
        }

        // If it's over 9000
        if (num > 9000)
            return "";
        num++;
    } while (QFileInfo(PathCombine(inDir, dirName)).exists());
    return dirName;
}

// Does the folder path contain any '!'? If yes, return true, otherwise false.
// (This is a problem for Java)
bool checkProblemticPathJava(QDir folder)
{
    QString pathfoldername = folder.absolutePath();
    return pathfoldername.contains("!", Qt::CaseInsensitive);
}

QString getDesktopDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

QString getApplicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

// Cross-platform Shortcut creation
bool createShortcut(QString destination, QString target, QStringList args, QString name, QString icon)
{
    if (destination.isEmpty()) {
        destination = PathCombine(getDesktopDir(), RemoveInvalidFilenameChars(name));
    }
    if (!ensureFilePathExists(destination)) {
        qWarning() << "Destination path can't be created!";
        return false;
    }
#if defined(Q_OS_MACOS)
    QDir application = destination + ".app/";

    if (application.exists()) {
        qWarning() << "Application already exists!";
        return false;
    }

    if (!application.mkpath(".")) {
        qWarning() << "Couldn't create application";
        return false;
    }

    QDir content = application.path() + "/Contents/";
    QDir resources = content.path() + "/Resources/";
    QDir binaryDir = content.path() + "/MacOS/";
    QFile info = content.path() + "/Info.plist";

    if (!(content.mkpath(".") && resources.mkpath(".") && binaryDir.mkpath("."))) {
        qWarning() << "Couldn't create directories within application";
        return false;
    }
    info.open(QIODevice::WriteOnly | QIODevice::Text);

    QFile(icon).rename(resources.path() + "/Icon.icns");

    // Create the Command file
    QString exec = binaryDir.path() + "/Run.command";

    QFile f(exec);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&f);

    QString argstring;
    if (!args.empty())
        argstring = " \"" + args.join("\" \"") + "\"";

    stream << "#!/bin/bash" << "\n";
    stream << "\"" << target << "\" " << argstring << "\n";

    stream.flush();
    f.close();

    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    // Generate the Info.plist
    QTextStream infoStream(&info);
    infoStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
                  "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "
                  "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
                  "<plist version=\"1.0\">\n"
                  "<dict>\n"
                  "    <key>CFBundleExecutable</key>\n"
                  "    <string>Run.command</string>\n"  // The path to the executable
                  "    <key>CFBundleIconFile</key>\n"
                  "    <string>Icon.icns</string>\n"
                  "    <key>CFBundleName</key>\n"
                  "    <string>"
               << name
               << "</string>\n"  // Name of the application
                  "    <key>CFBundlePackageType</key>\n"
                  "    <string>APPL</string>\n"
                  "    <key>CFBundleShortVersionString</key>\n"
                  "    <string>1.0</string>\n"
                  "    <key>CFBundleVersion</key>\n"
                  "    <string>1.0</string>\n"
                  "</dict>\n"
                  "</plist>";

    return true;
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
    if (!destination.endsWith(".desktop"))  // in case of isFlatpak destination is already populated
        destination += ".desktop";
    QFile f(destination);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&f);

    QString argstring;
    if (!args.empty())
        argstring = " '" + args.join("' '") + "'";

    stream << "[Desktop Entry]" << "\n";
    stream << "Type=Application" << "\n";
    stream << "Categories=Game;ActionGame;AdventureGame;Simulation" << "\n";
    stream << "Exec=\"" << target.toLocal8Bit() << "\"" << argstring.toLocal8Bit() << "\n";
    stream << "Name=" << name.toLocal8Bit() << "\n";
    if (!icon.isEmpty()) {
        stream << "Icon=" << icon.toLocal8Bit() << "\n";
    }

    stream.flush();
    f.close();

    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    return true;
#elif defined(Q_OS_WIN)
    QFileInfo targetInfo(target);

    if (!targetInfo.exists()) {
        qWarning() << "Target file does not exist!";
        return false;
    }

    target = targetInfo.absoluteFilePath();

    if (target.length() >= MAX_PATH) {
        qWarning() << "Target file path is too long!";
        return false;
    }

    if (!icon.isEmpty() && icon.length() >= MAX_PATH) {
        qWarning() << "Icon path is too long!";
        return false;
    }

    destination += ".lnk";

    if (destination.length() >= MAX_PATH) {
        qWarning() << "Destination path is too long!";
        return false;
    }

    QString argStr;
    int argCount = args.count();
    for (int i = 0; i < argCount; i++) {
        if (args[i].contains(' ')) {
            argStr.append('"').append(args[i]).append('"');
        } else {
            argStr.append(args[i]);
        }

        if (i < argCount - 1) {
            argStr.append(" ");
        }
    }

    if (argStr.length() >= MAX_PATH) {
        qWarning() << "Arguments string is too long!";
        return false;
    }

    HRESULT hres;

    // ...yes, you need to initialize the entire COM stack just to make a shortcut
    hres = CoInitialize(nullptr);
    if (FAILED(hres)) {
        qWarning() << "Failed to initialize COM!";
        return false;
    }

    WCHAR wsz[MAX_PATH];

    IShellLink* psl;

    // create an IShellLink instance - this stores the shortcut's attributes
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        wmemset(wsz, 0, MAX_PATH);
        target.toWCharArray(wsz);
        psl->SetPath(wsz);

        wmemset(wsz, 0, MAX_PATH);
        argStr.toWCharArray(wsz);
        psl->SetArguments(wsz);

        wmemset(wsz, 0, MAX_PATH);
        targetInfo.absolutePath().toWCharArray(wsz);
        psl->SetWorkingDirectory(wsz);  // "Starts in" attribute

        if (!icon.isEmpty()) {
            wmemset(wsz, 0, MAX_PATH);
            icon.toWCharArray(wsz);
            psl->SetIconLocation(wsz, 0);
        }

        // query an IPersistFile interface from our IShellLink instance
        // this is the interface that will actually let us save the shortcut to disk!
        IPersistFile* ppf;
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hres)) {
            wmemset(wsz, 0, MAX_PATH);
            destination.toWCharArray(wsz);
            hres = ppf->Save(wsz, TRUE);
            if (FAILED(hres)) {
                qWarning() << "IPresistFile->Save() failed";
                qWarning() << "hres = " << hres;
            }
            ppf->Release();
        } else {
            qWarning() << "Failed to query IPersistFile interface from IShellLink instance";
            qWarning() << "hres = " << hres;
        }
        psl->Release();
    } else {
        qWarning() << "Failed to create IShellLink instance";
        qWarning() << "hres = " << hres;
    }

    // go away COM, nobody likes you
    CoUninitialize();

    return SUCCEEDED(hres);
#else
    qWarning("Desktop Shortcuts not supported on your platform!");
    return false;
#endif
}

bool overrideFolder(QString overwritten_path, QString override_path)
{
    using copy_opts = fs::copy_options;

    if (!FS::ensureFolderPathExists(overwritten_path))
        return false;

    std::error_code err;
    fs::copy_options opt = copy_opts::recursive | copy_opts::overwrite_existing;

    // FIXME: hello traveller! Apparently std::copy does NOT overwrite existing files on GNU libstdc++ on Windows?
    fs::copy(StringUtils::toStdString(override_path), StringUtils::toStdString(overwritten_path), opt, err);

    if (err) {
        qCritical() << QString("Failed to apply override from %1 to %2").arg(override_path, overwritten_path);
        qCritical() << "Reason:" << QString::fromStdString(err.message());
    }

    return err.value() == 0;
}

QString getFilesystemTypeName(FilesystemType type)
{
    auto iter = s_filesystem_type_names.constFind(type);
    if (iter != s_filesystem_type_names.constEnd()) {
        return iter.value().constFirst();
    }
    return getFilesystemTypeName(FilesystemType::UNKNOWN);
}

FilesystemType getFilesystemTypeFuzzy(const QString& name)
{
    for (auto iter = s_filesystem_type_names.constBegin(); iter != s_filesystem_type_names.constEnd(); ++iter) {
        auto fs_names = iter.value();
        for (auto fs_name : fs_names) {
            if (name.toUpper().contains(fs_name.toUpper()))
                return iter.key();
        }
    }
    return FilesystemType::UNKNOWN;
}

FilesystemType getFilesystemType(const QString& name)
{
    for (auto iter = s_filesystem_type_names.constBegin(); iter != s_filesystem_type_names.constEnd(); ++iter) {
        auto fs_names = iter.value();
        if (fs_names.contains(name.toUpper()))
            return iter.key();
    }
    return FilesystemType::UNKNOWN;
}

/**
 * @brief path to the near ancestor that exists
 *
 */
QString nearestExistentAncestor(const QString& path)
{
    if (QFileInfo::exists(path))
        return path;

    QDir dir(path);
    if (!dir.makeAbsolute())
        return {};
    do {
        dir.setPath(QDir::cleanPath(dir.filePath(QStringLiteral(".."))));
    } while (!dir.exists() && !dir.isRoot());

    return dir.exists() ? dir.path() : QString();
}

/**
 * @brief colect information about the filesystem under a file
 *
 */
FilesystemInfo statFS(const QString& path)
{
    FilesystemInfo info;

    QStorageInfo storage_info(nearestExistentAncestor(path));

    info.fsTypeName = storage_info.fileSystemType();

    info.fsType = getFilesystemTypeFuzzy(info.fsTypeName);

    info.blockSize = storage_info.blockSize();
    info.bytesAvailable = storage_info.bytesAvailable();
    info.bytesFree = storage_info.bytesFree();
    info.bytesTotal = storage_info.bytesTotal();

    info.name = storage_info.name();
    info.rootPath = storage_info.rootPath();

    return info;
}

/**
 * @brief if the Filesystem is reflink/clone capable
 *
 */
bool canCloneOnFS(const QString& path)
{
    FilesystemInfo info = statFS(path);
    return canCloneOnFS(info);
}
bool canCloneOnFS(const FilesystemInfo& info)
{
    return canCloneOnFS(info.fsType);
}
bool canCloneOnFS(FilesystemType type)
{
    return s_clone_filesystems.contains(type);
}

/**
 * @brief if the Filesystem is reflink/clone capable and both paths are on the same device
 *
 */
bool canClone(const QString& src, const QString& dst)
{
    auto srcVInfo = statFS(src);
    auto dstVInfo = statFS(dst);

    bool sameDevice = srcVInfo.rootPath == dstVInfo.rootPath;

    return sameDevice && canCloneOnFS(srcVInfo) && canCloneOnFS(dstVInfo);
}

/**
 * @brief reflink/clones a directory and it's contents from src to dest
 * @param offset subdirectory form src to copy to dest
 * @return if there was an error during the filecopy
 */
bool clone::operator()(const QString& offset, bool dryRun)
{
    if (!canClone(m_src.absolutePath(), m_dst.absolutePath())) {
        qWarning() << "Can not clone: not same device or not clone/reflink filesystem";
        qDebug() << "Source path:" << m_src.absolutePath();
        qDebug() << "Destination path:" << m_dst.absolutePath();
        emit cloneFailed(m_src.absolutePath(), m_dst.absolutePath());
        return false;
    }

    m_cloned = 0;  // reset counter
    m_failedClones.clear();

    auto src = PathCombine(m_src.absolutePath(), offset);
    auto dst = PathCombine(m_dst.absolutePath(), offset);

    std::error_code err;

    // Function that'll do the actual cloneing
    auto cloneFile = [this, dryRun, dst, &err](QString src_path, QString relative_dst_path) {
        if (m_matcher && (m_matcher->matches(relative_dst_path) != m_whitelist))
            return;

        auto dst_path = PathCombine(dst, relative_dst_path);
        if (!dryRun) {
            ensureFilePathExists(dst_path);
            clone_file(src_path, dst_path, err);
        }
        if (err) {
            qDebug() << "Failed to clone files: error" << err.value() << "message" << QString::fromStdString(err.message());
            qDebug() << "Source file:" << src_path;
            qDebug() << "Destination file:" << dst_path;
            m_failedClones.append(qMakePair(src_path, dst_path));
            emit cloneFailed(src_path, dst_path);
            return;
        }
        m_cloned++;
        emit fileCloned(src_path, dst_path);
    };

    // We can't use copy_opts::recursive because we need to take into account the
    // blacklisted paths, so we iterate over the source directory, and if there's no blacklist
    // match, we copy the file.
    QDir src_dir(src);
    QDirIterator source_it(src, QDir::Filter::Files | QDir::Filter::Hidden, QDirIterator::Subdirectories);

    while (source_it.hasNext()) {
        auto src_path = source_it.next();
        auto relative_path = src_dir.relativeFilePath(src_path);

        cloneFile(src_path, relative_path);
    }

    // If the root src is not a directory, the previous iterator won't run.
    if (!fs::is_directory(StringUtils::toStdString(src)))
        cloneFile(src, "");

    return err.value() == 0;
}

/**
 * @brief clone/reflink file from src to dst
 *
 */
bool clone_file(const QString& src, const QString& dst, std::error_code& ec)
{
    auto src_path = StringUtils::toStdString(QDir::toNativeSeparators(QFileInfo(src).absoluteFilePath()));
    auto dst_path = StringUtils::toStdString(QDir::toNativeSeparators(QFileInfo(dst).absoluteFilePath()));

    FilesystemInfo srcinfo = statFS(src);
    FilesystemInfo dstinfo = statFS(dst);

    if ((srcinfo.rootPath != dstinfo.rootPath) || (srcinfo.fsType != dstinfo.fsType)) {
        ec = std::make_error_code(std::errc::not_supported);
        qWarning() << "reflink/clone must be to the same device and filesystem! src and dst root filesystems do not match.";
        return false;
    }

#if defined(Q_OS_WIN)

    if (!win_ioctl_clone(src_path, dst_path, ec)) {
        qDebug() << "failed win_ioctl_clone";
        qWarning() << "clone/reflink not supported on windows outside of btrfs or ReFS!";
        qWarning() << "check out https://github.com/maharmstone/btrfs for btrfs support!";
        return false;
    }

#elif defined(Q_OS_LINUX)

    if (!linux_ficlone(src_path, dst_path, ec)) {
        qDebug() << "failed linux_ficlone:";
        return false;
    }

#elif defined(Q_OS_MACOS)

    if (!macos_bsd_clonefile(src_path, dst_path, ec)) {
        qDebug() << "failed macos_bsd_clonefile:";
        return false;
    }

#else

    qWarning() << "clone/reflink not supported! unknown OS";
    ec = std::make_error_code(std::errc::not_supported);
    return false;

#endif

    return true;
}

#if defined(Q_OS_WIN)

static long RoundUpToPowerOf2(long originalValue, long roundingMultiplePowerOf2)
{
    long mask = roundingMultiplePowerOf2 - 1;
    return (originalValue + mask) & ~mask;
}

bool win_ioctl_clone(const std::wstring& src_path, const std::wstring& dst_path, std::error_code& ec)
{
    /**
     * This algorithm inspired from https://github.com/0xbadfca11/reflink
     * LICENSE MIT
     *
     *  Additional references
     *  https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_duplicate_extents_to_file
     *  https://github.com/microsoft/CopyOnWrite/blob/main/lib/Windows/WindowsCopyOnWriteFilesystem.cs#L94
     */

    HANDLE hSourceFile = CreateFileW(src_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hSourceFile == INVALID_HANDLE_VALUE) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to open source file" << src_path.c_str();
        return false;
    }

    ULONG fs_flags;
    if (!GetVolumeInformationByHandleW(hSourceFile, nullptr, 0, nullptr, nullptr, &fs_flags, nullptr, 0)) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to get Filesystem information for " << src_path.c_str();
        CloseHandle(hSourceFile);
        return false;
    }
    if (!(fs_flags & FILE_SUPPORTS_BLOCK_REFCOUNTING)) {
        SetLastError(ERROR_NOT_CAPABLE);
        ec = std::error_code(GetLastError(), std::system_category());
        qWarning() << "Filesystem at " << src_path.c_str() << " does not support reflink";
        CloseHandle(hSourceFile);
        return false;
    }

    FILE_END_OF_FILE_INFO sourceFileLength;
    if (!GetFileSizeEx(hSourceFile, &sourceFileLength.EndOfFile)) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to size of source file" << src_path.c_str();
        CloseHandle(hSourceFile);
        return false;
    }
    FILE_BASIC_INFO sourceFileBasicInfo;
    if (!GetFileInformationByHandleEx(hSourceFile, FileBasicInfo, &sourceFileBasicInfo, sizeof(sourceFileBasicInfo))) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to source file info" << src_path.c_str();
        CloseHandle(hSourceFile);
        return false;
    }
    ULONG junk;
    FSCTL_GET_INTEGRITY_INFORMATION_BUFFER sourceFileIntegrity;
    if (!DeviceIoControl(hSourceFile, FSCTL_GET_INTEGRITY_INFORMATION, nullptr, 0, &sourceFileIntegrity, sizeof(sourceFileIntegrity), &junk,
                         nullptr)) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to source file integrity info" << src_path.c_str();
        CloseHandle(hSourceFile);
        return false;
    }

    HANDLE hDestFile = CreateFileW(dst_path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, 0, hSourceFile);

    if (hDestFile == INVALID_HANDLE_VALUE) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to open dest file" << dst_path.c_str();
        CloseHandle(hSourceFile);
        return false;
    }
    FILE_DISPOSITION_INFO destFileDispose = { TRUE };
    if (!SetFileInformationByHandle(hDestFile, FileDispositionInfo, &destFileDispose, sizeof(destFileDispose))) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to set dest file info" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }

    if (!DeviceIoControl(hDestFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &junk, nullptr)) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to set dest sparseness" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }
    FSCTL_SET_INTEGRITY_INFORMATION_BUFFER setDestFileintegrity = { sourceFileIntegrity.ChecksumAlgorithm, sourceFileIntegrity.Reserved,
                                                                    sourceFileIntegrity.Flags };
    if (!DeviceIoControl(hDestFile, FSCTL_SET_INTEGRITY_INFORMATION, &setDestFileintegrity, sizeof(setDestFileintegrity), nullptr, 0,
                         nullptr, nullptr)) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to set dest file integrity info" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }
    if (!SetFileInformationByHandle(hDestFile, FileEndOfFileInfo, &sourceFileLength, sizeof(sourceFileLength))) {
        ec = std::error_code(GetLastError(), std::system_category());
        qDebug() << "Failed to set dest file size" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }

    const LONG64 splitThreshold = (1LL << 32) - sourceFileIntegrity.ClusterSizeInBytes;

    DUPLICATE_EXTENTS_DATA dupExtent;
    dupExtent.FileHandle = hSourceFile;
    for (LONG64 offset = 0, remain = RoundUpToPowerOf2(sourceFileLength.EndOfFile.QuadPart, sourceFileIntegrity.ClusterSizeInBytes);
         remain > 0; offset += splitThreshold, remain -= splitThreshold) {
        dupExtent.SourceFileOffset.QuadPart = dupExtent.TargetFileOffset.QuadPart = offset;
        dupExtent.ByteCount.QuadPart = std::min(splitThreshold, remain);

        if (!DeviceIoControl(hDestFile, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dupExtent, sizeof(dupExtent), nullptr, 0, &junk, nullptr)) {
            DWORD err = GetLastError();
            QString additionalMessage;
            if (err == ERROR_BLOCK_TOO_MANY_REFERENCES) {
                static const int MaxClonesPerFile = 8175;
                additionalMessage =
                    QString(
                        " This is ERROR_BLOCK_TOO_MANY_REFERENCES and may mean you have surpassed the maximum "
                        "allowed %1 references for a single file. "
                        "See "
                        "https://docs.microsoft.com/en-us/windows-server/storage/refs/block-cloning#functionality-restrictions-and-remarks")
                        .arg(MaxClonesPerFile);
            }
            ec = std::error_code(err, std::system_category());
            qDebug() << "Failed copy-on-write cloning of" << src_path.c_str() << "to" << dst_path.c_str() << "with error" << err
                     << additionalMessage;
            CloseHandle(hSourceFile);
            CloseHandle(hDestFile);
            return false;
        }
    }

    if (!(sourceFileBasicInfo.FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)) {
        FILE_SET_SPARSE_BUFFER setDestSparse = { FALSE };
        if (!DeviceIoControl(hDestFile, FSCTL_SET_SPARSE, &setDestSparse, sizeof(setDestSparse), nullptr, 0, &junk, nullptr)) {
            qDebug() << "Failed to set dest file sparseness" << dst_path.c_str();
            CloseHandle(hSourceFile);
            CloseHandle(hDestFile);
            return false;
        }
    }

    sourceFileBasicInfo.CreationTime.QuadPart = 0;
    if (!SetFileInformationByHandle(hDestFile, FileBasicInfo, &sourceFileBasicInfo, sizeof(sourceFileBasicInfo))) {
        qDebug() << "Failed to set dest file creation time" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }
    if (!FlushFileBuffers(hDestFile)) {
        qDebug() << "Failed to flush dest file buffer" << dst_path.c_str();
        CloseHandle(hSourceFile);
        CloseHandle(hDestFile);
        return false;
    }
    destFileDispose = { FALSE };
    bool result = !!SetFileInformationByHandle(hDestFile, FileDispositionInfo, &destFileDispose, sizeof(destFileDispose));

    CloseHandle(hSourceFile);
    CloseHandle(hDestFile);

    return result;
}

#elif defined(Q_OS_LINUX)

bool linux_ficlone(const std::string& src_path, const std::string& dst_path, std::error_code& ec)
{
    // https://man7.org/linux/man-pages/man2/ioctl_ficlone.2.html

    int src_fd = open(src_path.c_str(), O_RDONLY);
    if (src_fd == -1) {
        qDebug() << "Failed to open file:" << src_path.c_str();
        qDebug() << "Error:" << strerror(errno);
        ec = std::make_error_code(static_cast<std::errc>(errno));
        return false;
    }
    int dst_fd = open(dst_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (dst_fd == -1) {
        qDebug() << "Failed to open file:" << dst_path.c_str();
        qDebug() << "Error:" << strerror(errno);
        ec = std::make_error_code(static_cast<std::errc>(errno));
        close(src_fd);
        return false;
    }
    // attempt to clone
    if (ioctl(dst_fd, FICLONE, src_fd) == -1) {
        qDebug() << "Failed to clone file:" << src_path.c_str() << "to" << dst_path.c_str();
        qDebug() << "Error:" << strerror(errno);
        ec = std::make_error_code(static_cast<std::errc>(errno));
        close(src_fd);
        close(dst_fd);
        return false;
    }
    if (close(src_fd)) {
        qDebug() << "Failed to close file:" << src_path.c_str();
        qDebug() << "Error:" << strerror(errno);
    }
    if (close(dst_fd)) {
        qDebug() << "Failed to close file:" << dst_path.c_str();
        qDebug() << "Error:" << strerror(errno);
    }
    return true;
}

#elif defined(Q_OS_MACOS)

bool macos_bsd_clonefile(const std::string& src_path, const std::string& dst_path, std::error_code& ec)
{
    // clonefile(const char * src, const char * dst, int flags);
    // https://www.manpagez.com/man/2/clonefile/

    qDebug() << "attempting file clone via clonefile" << src_path.c_str() << "to" << dst_path.c_str();
    if (clonefile(src_path.c_str(), dst_path.c_str(), 0) == -1) {
        qDebug() << "Failed to clone file:" << src_path.c_str() << "to" << dst_path.c_str();
        qDebug() << "Error:" << strerror(errno);
        ec = std::make_error_code(static_cast<std::errc>(errno));
        return false;
    }
    return true;
}
#endif

/**
 * @brief if the Filesystem is symlink capable
 *
 */
bool canLinkOnFS(const QString& path)
{
    FilesystemInfo info = statFS(path);
    return canLinkOnFS(info);
}
bool canLinkOnFS(const FilesystemInfo& info)
{
    return canLinkOnFS(info.fsType);
}
bool canLinkOnFS(FilesystemType type)
{
    return !s_non_link_filesystems.contains(type);
}
/**
 * @brief if the Filesystem is symlink capable on both ends
 *
 */
bool canLink(const QString& src, const QString& dst)
{
    return canLinkOnFS(src) && canLinkOnFS(dst);
}

uintmax_t hardLinkCount(const QString& path)
{
    std::error_code err;
    int count = fs::hard_link_count(StringUtils::toStdString(path), err);
    if (err) {
        qWarning() << "Failed to count hard links for" << path << ":" << QString::fromStdString(err.message());
        count = 0;
    }
    return count;
}

#ifdef Q_OS_WIN
// returns 8.3 file format from long path
QString shortPathName(const QString& file)
{
    auto input = file.toStdWString();
    std::wstring output;
    long length = GetShortPathNameW(input.c_str(), NULL, 0);
    if (length == 0)
        return {};
    // NOTE: this resizing might seem weird...
    // when GetShortPathNameW fails, it returns length including null character
    // when it succeeds, it returns length excluding null character
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364989(v=vs.85).aspx
    output.resize(length);
    if (GetShortPathNameW(input.c_str(), (LPWSTR)output.c_str(), length) == 0)
        return {};
    output.resize(length - 1);
    QString ret = QString::fromStdWString(output);
    return ret;
}

// if the string survives roundtrip through local 8bit encoding...
bool fitsInLocal8bit(const QString& string)
{
    return string == QString::fromLocal8Bit(string.toLocal8Bit());
}

QString getPathNameInLocal8bit(const QString& file)
{
    if (!fitsInLocal8bit(file)) {
        auto path = shortPathName(file);
        if (!path.isEmpty()) {
            return path;
        }
        // in case shortPathName fails just return the path as is
    }
    return file;
}
#endif

QString getUniqueResourceName(const QString& filePath)
{
    auto newFileName = filePath;
    if (!newFileName.endsWith(".disabled")) {
        return newFileName;  // prioritize enabled mods
    }
    newFileName.chop(9);
    if (!QFile::exists(newFileName)) {
        return filePath;
    }
    QFileInfo fileInfo(filePath);
    auto baseName = fileInfo.completeBaseName();
    auto path = fileInfo.absolutePath();

    int counter = 1;
    do {
        if (counter == 1) {
            newFileName = FS::PathCombine(path, baseName + ".duplicate");
        } else {
            newFileName = FS::PathCombine(path, baseName + ".duplicate" + QString::number(counter));
        }
        counter++;
    } while (QFile::exists(newFileName));

    return newFileName;
}
}  // namespace FS
