#pragma once

#include "modplatform/CheckUpdateTask.h"

class FlameCheckUpdate : public CheckUpdateTask {
    Q_OBJECT

   public:
    FlameCheckUpdate(QList<Resource*>& resources,
                     std::list<Version>& mcVersions,
                     QList<ModPlatform::ModLoaderType> loadersList,
                     std::shared_ptr<ResourceFolderModel> resourceModel)
        : CheckUpdateTask(resources, mcVersions, std::move(loadersList), std::move(resourceModel))
    {}

   public slots:
    bool abort() override;

   protected slots:
    void executeTask() override;
   private slots:
    void getLatestVersionCallback(Resource* resource, std::shared_ptr<QByteArray> response);
    void collectBlockedMods();

   private:
    Task::Ptr m_task = nullptr;

    QHash<Resource*, QString> m_blocked;
};
