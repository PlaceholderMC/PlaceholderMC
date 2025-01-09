#pragma once
#include <quazip/quazip.h>
#include <quazip/quazipdir.h>
#include "InstanceTask.h"
#include "PackHelpers.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "net/NetJob.h"

#include "net/NetJob.h"

#include <optional>

namespace LegacyFTB {

class PackInstallTask : public InstanceTask {
    Q_OBJECT

   public:
    explicit PackInstallTask(shared_qobject_ptr<QNetworkAccessManager> network, const Modpack& pack, QString version);
    virtual ~PackInstallTask() {}

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    //! Entry point for tasks.
    virtual void executeTask() override;

   private:
    void downloadPack();
    void unzip();
    void install();

   private slots:

    void onUnzipFinished();
    void onUnzipCanceled();

   private: /* data */
    shared_qobject_ptr<QNetworkAccessManager> m_network;
    bool abortable = false;
    std::unique_ptr<QuaZip> m_packZip;
    QFuture<std::optional<QStringList>> m_extractFuture;
    QFutureWatcher<std::optional<QStringList>> m_extractFutureWatcher;
    NetJob::Ptr netJobContainer;
    QString archivePath;

    Modpack m_pack;
    QString m_version;
};

}  // namespace LegacyFTB
