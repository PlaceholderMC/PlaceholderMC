// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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
 */
#include "java/download/ArchiveDownloadTask.h"
#include <memory>
#include "MMCZip.h"

#include "Application.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

namespace Java {
ArchiveDownloadTask::ArchiveDownloadTask(QUrl url, QString final_path, QString checksumType, QString checksumHash)
    : m_url(url), m_final_path(final_path), m_checksum_type(checksumType), m_checksum_hash(checksumHash)
{}

void ArchiveDownloadTask::executeTask()
{
    // JRE found ! download the zip
    setStatus(tr("Downloading Java"));

    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("java", m_url.fileName());

    auto download = makeShared<NetJob>(QString("JRE::DownloadJava"), APPLICATION->network());
    auto action = Net::Download::makeCached(m_url, entry);
    if (!m_checksum_hash.isEmpty() && !m_checksum_type.isEmpty()) {
        auto hashType = QCryptographicHash::Algorithm::Sha1;
        if (m_checksum_type == "sha256") {
            hashType = QCryptographicHash::Algorithm::Sha256;
        }
        action->addValidator(new Net::ChecksumValidator(hashType, QByteArray::fromHex(m_checksum_hash.toUtf8())));
    }
    download->addNetAction(action);
    auto fullPath = entry->getFullPath();

    connect(download.get(), &Task::failed, this, &ArchiveDownloadTask::emitFailed);
    connect(download.get(), &Task::progress, this, &ArchiveDownloadTask::setProgress);
    connect(download.get(), &Task::stepProgress, this, &ArchiveDownloadTask::propagateStepProgress);
    connect(download.get(), &Task::status, this, &ArchiveDownloadTask::setStatus);
    connect(download.get(), &Task::details, this, &ArchiveDownloadTask::setDetails);
    connect(download.get(), &Task::succeeded, [this, fullPath] {
        // This should do all of the extracting and creating folders
        extractJava(fullPath);
    });
    m_task = download;
    m_task->start();
}

void ArchiveDownloadTask::extractJava(QString input)
{
    setStatus(tr("Extracting java"));

    auto archive = MMCZip::ExtractKArchive::newArchive(input);
    if (!archive->open(QIODevice::ReadOnly)) {
        emitFailed(tr("Unable to open supplied archive file."));
        return;
    }
    auto files = archive->directory()->entries();
    if (files.isEmpty()) {
        emitFailed(tr("No files were found in the supplied zip file,"));
        return;
    }
    m_task = makeShared<MMCZip::ExtractKArchive>(archive, m_final_path, files[0]);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(m_task.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(m_task.get(), &Task::succeeded, this, &ArchiveDownloadTask::emitSucceeded);
    connect(m_task.get(), &Task::aborted, this, &ArchiveDownloadTask::emitAborted);
    connect(m_task.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(reason);
    });
    connect(m_task.get(), &Task::stepProgress, this, &ArchiveDownloadTask::propagateStepProgress);

    connect(m_task.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(m_task.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = status;
        stepProgress(*progressStep);
    });
    m_task->start();
}

bool ArchiveDownloadTask::abort()
{
    auto aborted = canAbort();
    if (m_task)
        aborted = m_task->abort();
    emitAborted();
    return aborted;
};
}  // namespace Java