#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <qtconcurrentrun.h>

#include <Exception.h>
#include "McClient.h"
#include "Json.h"

// 7 first bits 
#define SEGMENT_BITS 0x7F
// last bit
#define CONTINUE_BIT 0x80

McClient::McClient(QObject *parent, QString domain, QString ip, short port): QObject(parent), m_domain(domain), m_ip(ip), m_port(port) {}

void McClient::getStatusData() {
    qDebug() << "Connecting to socket..";

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "Connected to socket successfully";
        sendRequest();

        connect(&m_socket, &QTcpSocket::readyRead, this, &McClient::readRawResponse);
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this]() {
        emitFail("Socket disconnected: " + m_socket.errorString());
    });

    m_socket.connectToHost(m_ip, m_port);
}

void McClient::sendRequest() {
    QByteArray data;
    writeVarInt(data, 0x00); // packet ID
    writeVarInt(data, 763); // hardcoded protocol version (763 = 1.20.1)
    writeVarInt(data, m_domain.size()); // server address length
    writeString(data, m_domain.toStdString()); // server address
    writeFixedInt(data, m_port, 2); // server port
    writeVarInt(data, 0x01); // next state
    writePacketToSocket(data); // send handshake packet

    writeVarInt(data, 0x00); // packet ID
    writePacketToSocket(data); // send status packet
}

void McClient::readRawResponse() {
    if (m_responseReadState == 2) {
        return;
    }

    m_resp.append(m_socket.readAll());
    if (m_responseReadState == 0 && m_resp.size() >= 5) {
        m_wantedRespLength = readVarInt(m_resp);
        m_responseReadState = 1;
    }
    
    if (m_responseReadState == 1 && m_resp.size() >= m_wantedRespLength) {
        if (m_resp.size() > m_wantedRespLength) {
            qDebug() << "Warning: Packet length doesn't match actual packet size (" << m_wantedRespLength << " expected vs " << m_resp.size() << " received)";
        }
        parseResponse();
        m_responseReadState = 2;
    }
}

void McClient::parseResponse() {
    qDebug() << "Received response successfully";

    int packetID = readVarInt(m_resp);
    if (packetID != 0x00) {
        throw Exception(
            QString("Packet ID doesn't match expected value (0x00 vs 0x%1)")
            .arg(packetID, 0, 16)
        );
    }

    Q_UNUSED(readVarInt(m_resp)); // json length

    // 'resp' should now be the JSON string
    QJsonDocument doc = QJsonDocument::fromJson(m_resp);
    emitSucceed(doc.object());
}

// From https://wiki.vg/Protocol#VarInt_and_VarLong
void McClient::writeVarInt(QByteArray &data, int value) {
    while ((value & ~SEGMENT_BITS)) { // check if the value is too big to fit in 7 bits
        // Write 7 bits
        data.append((value & SEGMENT_BITS) | CONTINUE_BIT);

        // Erase theses 7 bits from the value to write
        // Note: >>> means that the sign bit is shifted with the rest of the number rather than being left alone
        value >>= 7;
    }
    data.append(value);
}

// From https://wiki.vg/Protocol#VarInt_and_VarLong
int McClient::readVarInt(QByteArray &data) {
    int value = 0;
    int position = 0;
    char currentByte;

    while (position < 32) {
        currentByte = readByte(data);
        value |= (currentByte & SEGMENT_BITS) << position;

        if ((currentByte & CONTINUE_BIT) == 0) break;

        position += 7;
    }

    if (position >= 32) throw Exception("VarInt is too big");

    return value;
}

char McClient::readByte(QByteArray &data) {
    if (data.isEmpty()) {
        throw Exception("No more bytes to read");
    }

    char byte = data.at(0);
    data.remove(0, 1);
    return byte;
}

// write number with specified size in big endian format
void McClient::writeFixedInt(QByteArray &data, int value, int size) {
    for (int i = size - 1; i >= 0; i--) {
        data.append((value >> (i * 8)) & 0xFF);
    }
}

void McClient::writeString(QByteArray &data, const std::string &value) {
    data.append(value.c_str());
}

void McClient::writePacketToSocket(QByteArray &data) {
    // we prefix the packet with its length
    QByteArray dataWithSize;
    writeVarInt(dataWithSize, data.size());
    dataWithSize.append(data);

    // write it to the socket
    m_socket.write(dataWithSize);
    m_socket.flush();

    data.clear();
}


void McClient::emitFail(QString error) {
    qDebug() << "Minecraft server ping for status error:" << error;
    emit failed(error);
    emit finished();
}

void McClient::emitSucceed(QJsonObject data) {
    emit succeeded(data);
    emit finished();
}
