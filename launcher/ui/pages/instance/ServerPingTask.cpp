#include <QFutureWatcher>

#include "ServerPingTask.h"
#include "McResolver.h"
#include "McClient.h"

void ServerPingTask::executeTask() {
    qDebug() << "Querying status of " << QString("%1:%2").arg(m_domain).arg(m_port);

    // Resolve the actual IP and port for the server
    McResolver *resolver = new McResolver(nullptr, m_domain, m_port);
    QObject::connect(resolver, &McResolver::succeeded, this, [this, resolver](QString ip, int port) {
        qDebug() << "Resolved Address for" << m_domain << ": " << ip << ":" << port;

        // Now that we have the IP and port, query the server
        McClient *client = new McClient(nullptr, m_domain, ip, port);
        auto onlineFuture = client->getOnlinePlayers();

        // Wait for query to finish
        QFutureWatcher<int> *watcher = new QFutureWatcher<int>();
        QObject::connect(watcher, &QFutureWatcher<int>::finished, this, [this, client, onlineFuture, watcher]() {
            client->deleteLater();
            watcher->deleteLater();

            int online = onlineFuture.result();
            if (online == -1) {
                qDebug() << "Failed to get online players";
                emitFailed();
                return;
            } else {
                qDebug() << "Online players: " << online;
                m_outputOnlinePlayers = online;
                emitSucceeded();
            }
        });
        watcher->setFuture(onlineFuture);
    });

    // Delete McResolver object when done
    QObject::connect(resolver, &McResolver::finished, [resolver]() {
        resolver->deleteLater();
    });
    resolver->ping();
}