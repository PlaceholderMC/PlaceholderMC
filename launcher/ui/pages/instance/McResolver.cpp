#include <QObject>
#include <QDnsLookup>
#include <QtNetwork/qtcpsocket.h>
#include <QHostInfo>

#include "McResolver.h"

McResolver::McResolver(QObject *parent, QString domain, int port): QObject(parent), m_constrDomain(domain), m_constrPort(port) {}

void McResolver::ping() {
    pingWithDomainSRV(m_constrDomain, m_constrPort);
}

void McResolver::pingWithDomainSRV(QString domain, int port) {
    QDnsLookup *lookup = new QDnsLookup(this);
    lookup->setName(QString("_minecraft._tcp.%1").arg(domain));
    lookup->setType(QDnsLookup::SRV);

    connect(lookup, &QDnsLookup::finished, this, [this, domain, port]() {
        QDnsLookup *lookup = qobject_cast<QDnsLookup *>(sender());

        lookup->deleteLater();

        if (lookup->error() != QDnsLookup::NoError) {
            qDebug() << QString("Warning: SRV record lookup failed (%1), trying A record lookup").arg(lookup->errorString());
            pingWithDomainA(domain, port);
            return;
        }

        auto records = lookup->serviceRecords();
        if (records.isEmpty()) {
            qDebug() << "Warning: no SRV entries found for domain, trying A record lookup";
            pingWithDomainA(domain, port);
            return;
        }

        const auto& firstRecord = records.at(0);
        QString domain = firstRecord.target();
        int port = firstRecord.port();
        pingWithDomainA(domain, port);
    });

    lookup->lookup();
}

void McResolver::pingWithDomainA(QString domain, int port) {
    QHostInfo::lookupHost(domain, this, [this, port](const QHostInfo &hostInfo){
        if (hostInfo.error() != QHostInfo::NoError) {
            emitFail("A record lookup failed");
            return;
        }

        auto records = hostInfo.addresses();
        if (records.isEmpty()) {
            emitFail("No A entries found for domain");
            return;
        }
        
        const auto& firstRecord = records.at(0);
        emitSucceed(firstRecord.toString(), port);
    });        
}

void McResolver::emitFail(QString error) {
    qDebug() << "Ping error:" << error;
    emit failed();
    emit finished();
}

void McResolver::emitSucceed(QString ip, int port) {
    emit succeeded(ip, port);
    emit finished();
}
