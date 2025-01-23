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

#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

const QVector<PasteUpload::RegReplace> PasteUpload::AnonimizeRules = {
    RegReplace(QRegularExpression("C:\\\\Users\\\\([^\\\\]+)\\\\", QRegularExpression::CaseInsensitiveOption),
               "C:\\Users\\********\\"),  // windows
    RegReplace(QRegularExpression("C:\\/Users\\/([^\\/]+)\\/", QRegularExpression::CaseInsensitiveOption),
               "C:/Users/********/"),  // windows with forward slashes
    RegReplace(QRegularExpression("(?<!\\\\w)\\/home\\/[^\\/]+\\/", QRegularExpression::CaseInsensitiveOption),
               "/home/********/"),  // linux
    RegReplace(QRegularExpression("(?<!\\\\w)\\/Users\\/[^\\/]+\\/", QRegularExpression::CaseInsensitiveOption),
               "/Users/********/"),  // macos
    RegReplace(QRegularExpression("\\(Session ID is [^\\)]+\\)", QRegularExpression::CaseInsensitiveOption),
               "(Session ID is <SESSION_TOKEN>)"),  // SESSION_TOKEN
    RegReplace(QRegularExpression("new refresh token: \"[^\"]+\"", QRegularExpression::CaseInsensitiveOption),
               "new refresh token: \"<TOKEN>\""),  // refresh token
    RegReplace(QRegularExpression("\"device_code\" :  \"[^\"]+\"", QRegularExpression::CaseInsensitiveOption),
               "\"device_code\" :  \"<DEVICE_CODE>\""),  // device code
};

const std::array<PasteUpload::PasteTypeInfo, 4> PasteUpload::PasteTypes = { { { "0x0.st", "https://0x0.st", "" },
                                                                              { "hastebin", "https://hst.sh", "/documents" },
                                                                              { "paste.gg", "https://paste.gg", "/api/v1/pastes" },
                                                                              { "mclo.gs", "https://api.mclo.gs", "/1/log" } } };

QNetworkReply* PasteUpload::getReply(QNetworkRequest& request)
{
    for (auto rule : AnonimizeRules) {
        m_log.replace(rule.reg, rule.with);
    }

    switch (m_paste_type) {
        case PasteUpload::NullPointer: {
            QHttpMultiPart* multiPart = new QHttpMultiPart{ QHttpMultiPart::FormDataType };

            QHttpPart filePart;
            filePart.setBody(m_log.toUtf8());
            filePart.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"file\"; filename=\"log.txt\"");
            multiPart->append(filePart);

            return m_network->post(request, multiPart);
        }
        case PasteUpload::Hastebin: {
            return m_network->post(request, m_log.toUtf8());
        }
        case PasteUpload::Mclogs: {
            QUrlQuery postData;
            postData.addQueryItem("content", m_log);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            return m_network->post(request, postData.toString().toUtf8());
        }
        case PasteUpload::PasteGG: {
            QJsonObject obj;
            QJsonDocument doc;
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            obj.insert("expires", QDateTime::currentDateTimeUtc().addDays(100).toString(Qt::DateFormat::ISODate));

            QJsonArray files;
            QJsonObject logFileInfo;
            QJsonObject logFileContentInfo;
            logFileContentInfo.insert("format", "text");
            logFileContentInfo.insert("value", m_log);
            logFileInfo.insert("name", "log.txt");
            logFileInfo.insert("content", logFileContentInfo);
            files.append(logFileInfo);

            obj.insert("files", files);

            doc.setObject(obj);
            return m_network->post(request, doc.toJson());
        }
    }

    return nullptr;
};

auto PasteUpload::Sink::init(QNetworkRequest& request) -> Task::State
{
    m_output.clear();
    return Task::State::Running;
};

auto PasteUpload::Sink::write(QByteArray& data) -> Task::State
{
    m_output.append(data);
    return Task::State::Running;
}

auto PasteUpload::Sink::abort() -> Task::State
{
    m_output.clear();
    return Task::State::Failed;
}

auto PasteUpload::Sink::finalize(QNetworkReply&) -> Task::State
{
    switch (m_paste_type) {
        case PasteUpload::NullPointer:
            m_result->link = QString::fromUtf8(m_output).trimmed();
            break;
        case PasteUpload::Hastebin: {
            QJsonParseError jsonError;
            auto doc = QJsonDocument::fromJson(m_output, &jsonError);
            if (jsonError.error != QJsonParseError::NoError) {
                qDebug() << "hastebin server did not reply with JSON" << jsonError.errorString();
                return Task::State::Failed;
            }
            auto obj = doc.object();
            if (obj.contains("key") && obj["key"].isString()) {
                QString key = doc.object()["key"].toString();
                m_result->link = m_base_url + "/" + key;
            } else {
                qDebug() << "Log upload failed:" << doc.toJson();
                return Task::State::Failed;
            }
            break;
        }
        case PasteUpload::Mclogs: {
            QJsonParseError jsonError;
            auto doc = QJsonDocument::fromJson(m_output, &jsonError);
            if (jsonError.error != QJsonParseError::NoError) {
                qDebug() << "mclogs server did not reply with JSON" << jsonError.errorString();
                return Task::State::Failed;
            }
            auto obj = doc.object();
            if (obj.contains("success") && obj["success"].isBool()) {
                bool success = obj["success"].toBool();
                if (success) {
                    m_result->link = obj["url"].toString();
                } else {
                    m_result->error = obj["error"].toString();
                }
            } else {
                qDebug() << "Log upload failed:" << doc.toJson();
                return Task::State::Failed;
            }
            break;
        }
        case PasteUpload::PasteGG:
            QJsonParseError jsonError;
            auto doc = QJsonDocument::fromJson(m_output, &jsonError);
            if (jsonError.error != QJsonParseError::NoError) {
                qDebug() << "pastegg server did not reply with JSON" << jsonError.errorString();
                return Task::State::Failed;
            }
            auto obj = doc.object();
            if (obj.contains("status") && obj["status"].isString()) {
                QString status = obj["status"].toString();
                if (status == "success") {
                    m_result->link = m_base_url + "/p/anonymous/" + obj["result"].toObject()["id"].toString();
                } else {
                    m_result->error = obj["error"].toString();
                    m_result->extra_message = (obj.contains("message") && obj["message"].isString()) ? obj["message"].toString() : "none";
                }
            } else {
                qDebug() << "Log upload failed:" << doc.toJson();
                return Task::State::Failed;
            }
            break;
    }
    return Task::State::Succeeded;
}

Net::NetRequest::Ptr PasteUpload::make(const QString& log,
                                       const PasteUpload::PasteType pasteType,
                                       const QString customBaseURL,
                                       ResultPtr result)
{
    auto base = PasteUpload::PasteTypes.at(pasteType);
    QString baseUrl = customBaseURL.isEmpty() ? base.defaultBase : customBaseURL;
    auto up = makeShared<PasteUpload>(log, pasteType);

    // HACK: Paste's docs say the standard API path is at /api/<version> but the official instance paste.gg doesn't follow that??
    if (pasteType == PasteUpload::PasteGG && baseUrl == base.defaultBase)
        up->m_url = "https://api.paste.gg/v1/pastes";
    else
        up->m_url = baseUrl + base.endpointPath;

    up->m_sink.reset(new Sink(pasteType, baseUrl, result));
    return up;
}
