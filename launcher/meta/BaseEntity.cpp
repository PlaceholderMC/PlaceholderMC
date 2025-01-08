/* Copyright 2015-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BaseEntity.h"

#include "Exception.h"
#include "FileSystem.h"
#include "Json.h"
#include "modplatform/helpers/HashUtils.h"
#include "net/ApiDownload.h"
#include "net/ChecksumValidator.h"
#include "net/HttpMetaCache.h"
#include "net/Mode.h"
#include "net/NetJob.h"

#include "Application.h"
#include "BuildConfig.h"
#include "tasks/Task.h"

namespace Meta {

class ParsingValidator : public Net::Validator {
   public: /* con/des */
    ParsingValidator(BaseEntity* entity) : m_entity(entity) {};
    virtual ~ParsingValidator() = default;

   public: /* methods */
    bool init(QNetworkRequest&) override
    {
        m_data.clear();
        return true;
    }
    bool write(QByteArray& data) override
    {
        this->m_data.append(data);
        return true;
    }
    bool abort() override
    {
        m_data.clear();
        return true;
    }
    bool validate(QNetworkReply&) override
    {
        auto fname = m_entity->localFilename();
        try {
            auto doc = Json::requireDocument(m_data, fname);
            auto obj = Json::requireObject(doc, fname);
            m_entity->parse(obj);
            return true;
        } catch (const Exception& e) {
            qWarning() << "Unable to parse response:" << e.cause();
            return false;
        }
    }

   private: /* data */
    QByteArray m_data;
    BaseEntity* m_entity;
};

QUrl BaseEntity::url() const
{
    auto s = APPLICATION->settings();
    QString metaOverride = s->get("MetaURLOverride").toString();
    if (metaOverride.isEmpty()) {
        return QUrl(BuildConfig.META_URL).resolved(localFilename());
    }
    return QUrl(metaOverride).resolved(localFilename());
}

Task::Ptr BaseEntity::loadTask(Net::Mode mode)
{
    if (m_task && m_task->isRunning()) {
        return m_task;
    }
    m_task.reset(new BaseEntityLoadTask(this, mode));
    return m_task;
}

bool BaseEntity::isLoaded() const
{
    // consider it loaded only if the main hash is either empty and was remote loadded or the hashes match and was loaded
    return m_sha256.isEmpty() ? m_load_status == LoadStatus::Remote : m_load_status != LoadStatus::NotLoaded && m_sha256 == m_file_sha256;
}

void BaseEntity::setSha256(QString sha256)
{
    m_sha256 = sha256;
}

BaseEntity::LoadStatus BaseEntity::status() const
{
    return m_load_status;
}

BaseEntityLoadTask::BaseEntityLoadTask(BaseEntity* parent, Net::Mode mode) : m_entity(parent), m_mode(mode) {}

void BaseEntityLoadTask::executeTask()
{
    const QString fname = QDir("meta").absoluteFilePath(m_entity->localFilename());
    auto hashMatches = false;
    // the file exists on disk try to load it
    if (QFile::exists(fname)) {
        try {
            QByteArray fileData;
            // read local file if nothing is loaded yet
            if (m_entity->m_load_status == BaseEntity::LoadStatus::NotLoaded || m_entity->m_file_sha256.isEmpty()) {
                setStatus(tr("Loading local file"));
                fileData = FS::read(fname);
                m_entity->m_file_sha256 = Hashing::hash(fileData, Hashing::Algorithm::Sha256);
            }

            // on online the hash needs to match
            hashMatches = m_entity->m_sha256 == m_entity->m_file_sha256;
            if (m_mode == Net::Mode::Online && !m_entity->m_sha256.isEmpty() && !hashMatches) {
                throw Exception("mismatched checksum");
            }

            // load local file
            if (m_entity->m_load_status == BaseEntity::LoadStatus::NotLoaded) {
                auto doc = Json::requireDocument(fileData, fname);
                auto obj = Json::requireObject(doc, fname);
                m_entity->parse(obj);
                m_entity->m_load_status = BaseEntity::LoadStatus::Local;
            }

        } catch (const Exception& e) {
            qCritical() << QString("Unable to parse file %1: %2").arg(fname, e.cause());
            // just make sure it's gone and we never consider it again.
            FS::deletePath(fname);
            m_entity->m_load_status = BaseEntity::LoadStatus::NotLoaded;
        }
    }
    // if we need remote update, run the update task
    auto wasLoadedOffline = m_entity->m_load_status != BaseEntity::LoadStatus::NotLoaded && m_mode == Net::Mode::Offline;
    // if has is not present allways fetch from remote(e.g. the main index file), else only fetch if hash doesn't match
    auto wasLoadedRemote = m_entity->m_sha256.isEmpty() ? m_entity->m_load_status == BaseEntity::LoadStatus::Remote : hashMatches;
    if (wasLoadedOffline || wasLoadedRemote) {
        emitSucceeded();
        return;
    }
    m_task.reset(new NetJob(QObject::tr("Download of meta file %1").arg(m_entity->localFilename()), APPLICATION->network()));
    auto url = m_entity->url();
    auto entry = APPLICATION->metacache()->resolveEntry("meta", m_entity->localFilename());
    entry->setStale(true);
    auto dl = Net::ApiDownload::makeCached(url, entry);
    /*
     * The validator parses the file and loads it into the object.
     * If that fails, the file is not written to storage.
     */
    if (!m_entity->m_sha256.isEmpty())
        dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Algorithm::Sha256, m_entity->m_sha256));
    dl->addValidator(new ParsingValidator(m_entity));
    m_task->addNetAction(dl);
    m_task->setAskRetry(false);
    connect(m_task.get(), &Task::failed, this, &BaseEntityLoadTask::emitFailed);
    connect(m_task.get(), &Task::succeeded, this, [this]() {
        m_entity->m_load_status = BaseEntity::LoadStatus::Remote;
        m_entity->m_file_sha256 = m_entity->m_sha256;
        emitSucceeded();
    });

    connect(m_task.get(), &Task::progress, this, &Task::setProgress);
    connect(m_task.get(), &Task::stepProgress, this, &BaseEntityLoadTask::propagateStepProgress);
    connect(m_task.get(), &Task::status, this, &Task::setStatus);
    connect(m_task.get(), &Task::details, this, &Task::setDetails);

    m_task->start();
}

bool BaseEntityLoadTask::canAbort() const
{
    return m_task ? m_task->canAbort() : false;
}

bool BaseEntityLoadTask::abort()
{
    if (m_task) {
        Task::abort();
        return m_task->abort();
    }
    return Task::abort();
}

}  // namespace Meta
