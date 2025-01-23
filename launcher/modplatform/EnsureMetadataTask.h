#pragma once

#include "ModIndex.h"
#include "net/NetJob.h"

#include "modplatform/helpers/HashUtils.h"

#include "minecraft/mod/Resource.h"
#include "tasks/ConcurrentTask.h"

class Mod;
class QDir;

class EnsureMetadataTask : public Task {
    Q_OBJECT

   public:
    EnsureMetadataTask(Resource*, QDir, ModPlatform::ResourceProvider = ModPlatform::ResourceProvider::MODRINTH);
    EnsureMetadataTask(QList<Resource*>&, QDir, ModPlatform::ResourceProvider = ModPlatform::ResourceProvider::MODRINTH);
    EnsureMetadataTask(QHash<QString, Resource*>&, QDir, ModPlatform::ResourceProvider = ModPlatform::ResourceProvider::MODRINTH);

    ~EnsureMetadataTask() = default;

    Task::Ptr getHashingTask() { return m_hashingTask; }
    void setLockUpdate(bool lockUpdate) { m_lockUpdate = lockUpdate; }

   public slots:
    bool abort() override;
   protected slots:
    void executeTask() override;

   private:
    // FIXME: Move to their own namespace
    auto modrinthVersionsTask() -> Task::Ptr;
    auto modrinthProjectsTask() -> Task::Ptr;

    auto flameVersionsTask() -> Task::Ptr;
    auto flameProjectsTask() -> Task::Ptr;

    // Helpers
    enum class RemoveFromList { Yes, No };
    void emitReady(Resource*, QString key = {}, RemoveFromList = RemoveFromList::Yes);
    void emitFail(Resource*, QString key = {}, RemoveFromList = RemoveFromList::Yes);

    // Hashes and stuff
    auto createNewHash(Resource*) -> Hashing::Hasher::Ptr;
    auto getExistingHash(Resource*) -> QString;

   private slots:
    void modrinthCallback(ModPlatform::IndexedPack& pack, ModPlatform::IndexedVersion& ver, Resource*);
    void flameCallback(ModPlatform::IndexedPack& pack, ModPlatform::IndexedVersion& ver, Resource*);

   signals:
    void metadataReady(Resource*);
    void metadataFailed(Resource*);

   private:
    QHash<QString, Resource*> m_resources;
    QDir m_index_dir;
    ModPlatform::ResourceProvider m_provider;

    QHash<QString, ModPlatform::IndexedVersion> m_temp_versions;
    Task::Ptr m_hashingTask;
    Task::Ptr m_current_task;
    bool m_lockUpdate = false;
};
