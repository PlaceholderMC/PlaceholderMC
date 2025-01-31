// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Lenny McLennington <lenny@sneed.church>
 *  Copyright (C) 2022 Swirl <swurl@swurl.xyz>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "PasteUpload.h"
#include "Application.h"

#include <QDebug>
#include <QFile>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <memory>

#include "net/ByteArraySink.h"

class PasteSink : public Net::ByteArraySink {
   public:
    PasteSink(PasteUpload* p) : Net::ByteArraySink(std::make_shared<QByteArray>()), m_d(p) {};
    virtual ~PasteSink() = default;

   public:
    auto finalize(QNetworkReply& reply) -> Task::State override
    {
        if (!finalizeAllValidators(reply)) {
            m_fail_reason = "Failed to finalize validators";
            return Task::State::Failed;
        }

        int statusCode = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply.error() != QNetworkReply::NetworkError::NoError) {
            m_fail_reason = QObject::tr("Network error: %1").arg(reply.errorString());
            return Task::State::Failed;
        } else if (statusCode != 200 && statusCode != 201) {
            QString reasonPhrase = reply.attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
            m_fail_reason =
                QObject::tr("Error: %1 returned unexpected status code %2 %3").arg(m_d->url().toString()).arg(statusCode).arg(reasonPhrase);
            return Task::State::Failed;
        }

        switch (m_d->m_pasteType) {
            case PasteUpload::NullPointer:
                m_d->m_pasteLink = QString::fromUtf8(*m_output.get()).trimmed();
                break;
            case PasteUpload::Hastebin: {
                QJsonDocument jsonDoc{ QJsonDocument::fromJson(*m_output.get()) };
                QJsonObject jsonObj{ jsonDoc.object() };
                if (jsonObj.contains("key") && jsonObj["key"].isString()) {
                    QString key = jsonDoc.object()["key"].toString();
                    m_d->m_pasteLink = m_d->m_baseUrl + "/" + key;
                } else {
                    m_fail_reason = QObject::tr("Error: %1 returned a malformed response body").arg(m_d->url().toString());
                    return Task::State::Failed;
                }
                break;
            }
            case PasteUpload::Mclogs: {
                QJsonDocument jsonDoc{ QJsonDocument::fromJson(*m_output.get()) };
                QJsonObject jsonObj{ jsonDoc.object() };
                if (jsonObj.contains("success") && jsonObj["success"].isBool()) {
                    bool success = jsonObj["success"].toBool();
                    if (success) {
                        m_d->m_pasteLink = jsonObj["url"].toString();
                    } else {
                        QString error = jsonObj["error"].toString();
                        m_fail_reason = QObject::tr("Error: %1 returned an error: %2").arg(m_d->url().toString(), error);
                        return Task::State::Failed;
                    }
                } else {
                    m_fail_reason = QObject::tr("Error: %1 returned a malformed response body").arg(m_d->url().toString());
                    return Task::State::Failed;
                }
                break;
            }
            case PasteUpload::PasteGG:
                QJsonDocument jsonDoc{ QJsonDocument::fromJson(*m_output.get()) };
                QJsonObject jsonObj{ jsonDoc.object() };
                if (jsonObj.contains("status") && jsonObj["status"].isString()) {
                    QString status = jsonObj["status"].toString();
                    if (status == "success") {
                        m_d->m_pasteLink = m_d->m_baseUrl + "/p/anonymous/" + jsonObj["result"].toObject()["id"].toString();
                    } else {
                        QString error = jsonObj["error"].toString();
                        QString message =
                            (jsonObj.contains("message") && jsonObj["message"].isString()) ? jsonObj["message"].toString() : "none";
                        m_fail_reason = QObject::tr("Error: %1 returned an error code: %2\nError message: %3")
                                            .arg(m_d->url().toString(), error, message);
                        return Task::State::Failed;
                    }
                } else {
                    m_fail_reason = QObject::tr("Error: %1 returned a malformed response body").arg(m_d->url().toString());
                    return Task::State::Failed;
                }
                break;
        }
        return Task::State::Succeeded;
    }

   private:
    PasteUpload* m_d;
};

std::array<PasteUpload::PasteTypeInfo, 4> PasteUpload::PasteTypes = { { { "0x0.st", "https://0x0.st", "" },
                                                                        { "hastebin", "https://hst.sh", "/documents" },
                                                                        { "paste.gg", "https://paste.gg", "/api/v1/pastes" },
                                                                        { "mclo.gs", "https://api.mclo.gs", "/1/log" } } };

PasteUpload::PasteUpload(QString text, QString baseUrl, PasteType pasteType)
    : m_baseUrl(baseUrl), m_pasteType(pasteType), m_text(text.toUtf8())
{
    if (m_baseUrl == "")
        m_baseUrl = PasteTypes.at(pasteType).defaultBase;

    // HACK: Paste's docs say the standard API path is at /api/<version> but the official instance paste.gg doesn't follow that??
    if (pasteType == PasteGG && m_baseUrl == PasteTypes.at(pasteType).defaultBase)
        m_url = "https://api.paste.gg/v1/pastes";
    else
        m_url = m_baseUrl + PasteTypes.at(pasteType).endpointPath;

    m_sink.reset(new PasteSink(this));
}

QNetworkReply* PasteUpload::getReply(QNetworkRequest& request)
{
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgentUncached().toUtf8());

    switch (m_pasteType) {
        case NullPointer: {
            QHttpMultiPart* multiPart = new QHttpMultiPart{ QHttpMultiPart::FormDataType };

            QHttpPart filePart;
            filePart.setBody(m_text);
            filePart.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"file\"; filename=\"log.txt\"");
            multiPart->append(filePart);

            auto rep = m_network->post(request, multiPart);
            multiPart->setParent(rep);

            return rep;
        }
        case Hastebin: {
            request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgentUncached().toUtf8());
            return m_network->post(request, m_text);
        }
        case Mclogs: {
            QUrlQuery postData;
            postData.addQueryItem("content", m_text);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            return m_network->post(request, postData.toString().toUtf8());
        }
        case PasteGG: {
            QJsonObject obj;
            QJsonDocument doc;
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            obj.insert("expires", QDateTime::currentDateTimeUtc().addDays(100).toString(Qt::DateFormat::ISODate));

            QJsonArray files;
            QJsonObject logFileInfo;
            QJsonObject logFileContentInfo;
            logFileContentInfo.insert("format", "text");
            logFileContentInfo.insert("value", QString::fromUtf8(m_text));
            logFileInfo.insert("name", "log.txt");
            logFileInfo.insert("content", logFileContentInfo);
            files.append(logFileInfo);

            obj.insert("files", files);

            doc.setObject(obj);
            return m_network->post(request, doc.toJson());
        }
    }
    return nullptr;
}
