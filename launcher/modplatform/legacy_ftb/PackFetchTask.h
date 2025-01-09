#pragma once

#include <QByteArray>
#include <QObject>
#include <QTemporaryDir>
#include <memory>
#include "PackHelpers.h"
#include "net/NetJob.h"

namespace LegacyFTB {

class PackFetchTask : public QObject {
    Q_OBJECT

   public:
    PackFetchTask(shared_qobject_ptr<QNetworkAccessManager> network) : QObject(nullptr), m_network(network) {};
    virtual ~PackFetchTask() = default;

    void fetch();
    void fetchPrivate(const QStringList& toFetch);

   private:
    shared_qobject_ptr<QNetworkAccessManager> m_network;
    NetJob::Ptr jobPtr;

    std::shared_ptr<QByteArray> publicModpacksXmlFileData = std::make_shared<QByteArray>();
    std::shared_ptr<QByteArray> thirdPartyModpacksXmlFileData = std::make_shared<QByteArray>();

    bool parseAndAddPacks(QByteArray& data, PackType packType, ModpackList& list);
    ModpackList publicPacks;
    ModpackList thirdPartyPacks;

   protected slots:
    void fileDownloadFinished();
    void fileDownloadFailed(QString reason);
    void fileDownloadAborted();

   signals:
    void finished(ModpackList publicPacks, ModpackList thirdPartyPacks);
    void failed(QString reason);
    void aborted();

    void privateFileDownloadFinished(const Modpack& modpack);
    void privateFileDownloadFailed(QString reason, QString packCode);
};

}  // namespace LegacyFTB
