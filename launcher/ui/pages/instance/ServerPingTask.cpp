#include <QFutureWatcher>

#include "ServerPingTask.h"
#include "McResolver.h"
#include "McClient.h"
#include <Json.h>

unsigned getOnlinePlayers(QJsonObject data) {
    return Json::requireInteger(Json::requireObject(data, "players"), "online");
}

void ServerPingTask::executeTask() {
    qDebug() << "Querying status of " << QString("%1:%2").arg(m_domain).arg(m_port);

    // Resolve the actual IP and port for the server
    McResolver *resolver = new McResolver(nullptr, m_domain, m_port);
    QObject::connect(resolver, &McResolver::succeeded, this, [this, resolver](QString ip, int port) {
        qDebug() << "Resolved Address for" << m_domain << ": " << ip << ":" << port;

        // Now that we have the IP and port, query the server
        McClient *client = new McClient(nullptr, m_domain, ip, port);

        QObject::connect(client, &McClient::succeeded, this, [this](QJsonObject data) {
            m_outputOnlinePlayers = getOnlinePlayers(data);
            qDebug() << "Online players: " << m_outputOnlinePlayers;
            emitSucceeded();
        });
        QObject::connect(client, &McClient::failed, this, [this](QString error) {
            emitFailed(error);
        });

        // Delete McClient object when done
        QObject::connect(client, &McClient::finished, this, [this, client]() {
            client->deleteLater();
        });
        client->getStatusData();
    });
    QObject::connect(resolver, &McResolver::failed, this, [this](QString error) {
        emitFailed(error);
    });

    // Delete McResolver object when done
    QObject::connect(resolver, &McResolver::finished, [resolver]() {
        resolver->deleteLater();
    });
    resolver->ping();
}