// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "NetRequest.h"

#include <QDateTime>
#include <QFileInfo>
#include <QNetworkReply>
#include <QUrl>
#include <memory>

#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#endif
#include "BuildConfig.h"

#include "MMCTime.h"
#include "StringUtils.h"

namespace Net {

void NetRequest::addValidator(Validator* v)
{
    m_sink->addValidator(v);
}

void NetRequest::executeTask()
{
    setStatus(tr("Requesting %1").arg(StringUtils::truncateUrlHumanFriendly(m_url, 80)));

    if (getState() == Task::State::AbortedByUser) {
        qCWarning(logCat) << getUid().toString() << "Attempt to start an aborted Request:" << m_url.toString();
        emit aborted();
        emit finished();
        return;
    }

    QNetworkRequest request(m_url);
    m_state = m_sink->init(request);
    switch (m_state) {
        case State::Succeeded:
            qCDebug(logCat) << getUid().toString() << "Request cache hit " << m_url.toString();
            emit succeeded();
            emit finished();
            return;
        case State::Running:
            qCDebug(logCat) << getUid().toString() << "Runninng " << m_url.toString();
            break;
        case State::Inactive:
        case State::Failed:
            emit failed("Failed to initialize sink");
            emit finished();
            return;
        case State::AbortedByUser:
            emit aborted();
            emit finished();
            return;
    }

#if defined(LAUNCHER_APPLICATION)
    auto user_agent = APPLICATION->getUserAgent();
#else
    auto user_agent = BuildConfig.USER_AGENT;
#endif

    request.setHeader(QNetworkRequest::UserAgentHeader, user_agent.toUtf8());
    for (auto& header_proxy : m_headerProxies) {
        header_proxy->writeHeaders(request);
    }

#if defined(LAUNCHER_APPLICATION)
    request.setTransferTimeout(APPLICATION->settings()->get("RequestTimeout").toInt() * 1000);
#else
    request.setTransferTimeout();
#endif

    m_last_progress_time = m_clock.now();
    m_last_progress_bytes = 0;

    auto rep = getReply(request);
    if (rep == nullptr)  // it failed
        return;
    m_reply.reset(rep);
    connect(rep, &QNetworkReply::uploadProgress, this, &NetRequest::onProgress);
    connect(rep, &QNetworkReply::downloadProgress, this, &NetRequest::onProgress);
    connect(rep, &QNetworkReply::finished, this, &NetRequest::downloadFinished);
    connect(rep, &QNetworkReply::errorOccurred, this, &NetRequest::downloadError);
    connect(rep, &QNetworkReply::sslErrors, this, &NetRequest::sslErrors);
    connect(rep, &QNetworkReply::readyRead, this, &NetRequest::downloadReadyRead);
}

void NetRequest::onProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto now = m_clock.now();
    auto elapsed = now - m_last_progress_time;

    // use milliseconds for speed precision
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    auto bytes_received_since = bytesReceived - m_last_progress_bytes;
    auto dl_speed_bps = (double)bytes_received_since / elapsed_ms.count() * 1000;
    auto remaining_time_s = (bytesTotal - bytesReceived) / dl_speed_bps;

    //: Current amount of bytes downloaded, out of the total amount of bytes in the download
    QString dl_progress =
        tr("%1 / %2").arg(StringUtils::humanReadableFileSize(bytesReceived)).arg(StringUtils::humanReadableFileSize(bytesTotal));

    QString dl_speed_str;
    if (elapsed_ms.count() > 0) {
        auto str_eta = bytesTotal > 0 ? Time::humanReadableDuration(remaining_time_s) : tr("unknown");
        //: Download speed, in bytes per second (remaining download time in parenthesis)
        dl_speed_str = tr("%1 /s (%2)").arg(StringUtils::humanReadableFileSize(dl_speed_bps)).arg(str_eta);
    } else {
        //: Download speed at 0 bytes per second
        dl_speed_str = tr("0 B/s");
    }

    setDetails(dl_progress + "\n" + dl_speed_str);

    setProgress(bytesReceived, bytesTotal);
}

void NetRequest::downloadError(QNetworkReply::NetworkError error)
{
    if (error == QNetworkReply::OperationCanceledError) {
        qCCritical(logCat) << getUid().toString() << "Aborted " << m_url.toString();
        m_state = State::Failed;
    } else {
        if (m_options & Option::AcceptLocalFiles) {
            if (m_sink->hasLocalData()) {
                m_state = State::Succeeded;
                return;
            }
        }
        // error happened during download.
        qCCritical(logCat) << getUid().toString() << "Failed" << m_url.toString() << "with reason" << error;
        if (m_reply)
            qCCritical(logCat) << getUid().toString() << "HTTP Status" << replyStatusCode() << ";error" << errorString();
        m_state = State::Failed;
    }
}

void NetRequest::sslErrors(const QList<QSslError>& errors)
{
    int i = 1;
    for (auto error : errors) {
        qCCritical(logCat) << getUid().toString() << "Request" << m_url.toString() << "SSL Error #" << i << " : " << error.errorString();
        auto cert = error.certificate();
        qCCritical(logCat) << getUid().toString() << "Certificate in question:\n" << cert.toText();
        i++;
    }
}

auto NetRequest::handleRedirect() -> bool
{
    QUrl redirect = m_reply->header(QNetworkRequest::LocationHeader).toUrl();
    if (!redirect.isValid()) {
        if (!m_reply->hasRawHeader("Location")) {
            // no redirect -> it's fine to continue
            return false;
        }
        // there is a Location header, but it's not correct. we need to apply some workarounds...
        QByteArray redirectBA = m_reply->rawHeader("Location");
        if (redirectBA.size() == 0) {
            // empty, yet present redirect header? WTF?
            return false;
        }
        QString redirectStr = QString::fromUtf8(redirectBA);

        if (redirectStr.startsWith("//")) {
            /*
             * IF the URL begins with //, we need to insert the URL scheme.
             * See: https://bugreports.qt.io/browse/QTBUG-41061
             * See: http://tools.ietf.org/html/rfc3986#section-4.2
             */
            redirectStr = m_reply->url().scheme() + ":" + redirectStr;
        } else if (redirectStr.startsWith("/")) {
            /*
             * IF the URL begins with /, we need to process it as a relative URL
             */
            auto url = m_reply->url();
            url.setPath(redirectStr, QUrl::TolerantMode);
            redirectStr = url.toString();
        }

        /*
         * Next, make sure the URL is parsed in tolerant mode. Qt doesn't parse the location header in tolerant mode, which causes issues.
         * FIXME: report Qt bug for this
         */
        redirect = QUrl(redirectStr, QUrl::TolerantMode);
        if (!redirect.isValid()) {
            qCWarning(logCat) << getUid().toString() << "Failed to parse redirect URL:" << redirectStr;
            downloadError(QNetworkReply::ProtocolFailure);
            return false;
        }
        qCDebug(logCat) << getUid().toString() << "Fixed location header:" << redirect;
    } else {
        qCDebug(logCat) << getUid().toString() << "Location header:" << redirect;
    }

    m_url = QUrl(redirect.toString());
    qCDebug(logCat) << getUid().toString() << "Following redirect to " << m_url.toString();
    executeTask();

    return true;
}

void NetRequest::downloadFinished()
{
    // handle HTTP redirection first
    if (handleRedirect()) {
        qCDebug(logCat) << getUid().toString() << "Request redirected:" << m_url.toString();
        return;
    }

    // if the download failed before this point ...
    if (m_state == State::Succeeded)  // pretend to succeed so we continue processing :)
    {
        qCDebug(logCat) << getUid().toString() << "Request failed but we are allowed to proceed:" << m_url.toString();
        m_sink->abort();
        emit succeeded();
        emit finished();
        return;
    } else if (m_state == State::Failed) {
        qCDebug(logCat) << getUid().toString() << "Request failed in previous step:" << m_url.toString();
        m_sink->abort();
        emit failed(m_reply->errorString());
        emit finished();
        return;
    } else if (m_state == State::AbortedByUser) {
        qCDebug(logCat) << getUid().toString() << "Request aborted in previous step:" << m_url.toString();
        m_sink->abort();
        emit aborted();
        emit finished();
        return;
    }

    // make sure we got all the remaining data, if any
    auto data = m_reply->readAll();
    if (data.size()) {
        qCDebug(logCat) << getUid().toString() << "Writing extra" << data.size() << "bytes";
        m_state = m_sink->write(data);
        if (m_state != State::Succeeded) {
            qCDebug(logCat) << getUid().toString() << "Request failed to write:" << m_url.toString();
            m_sink->abort();
            emit failed("failed to write in sink");
            emit finished();
            return;
        }
    }

    // otherwise, finalize the whole graph
    m_state = m_sink->finalize(*m_reply.get());
    if (m_state != State::Succeeded) {
        qCDebug(logCat) << getUid().toString() << "Request failed to finalize:" << m_url.toString();
        m_sink->abort();
        emit failed("failed to finalize the request");
        emit finished();
        return;
    }

    qCDebug(logCat) << getUid().toString() << "Request succeeded:" << m_url.toString();
    emit succeeded();
    emit finished();
}

void NetRequest::downloadReadyRead()
{
    if (m_state == State::Running) {
        auto data = m_reply->readAll();
        m_state = m_sink->write(data);
        if (m_state == State::Failed) {
            qCCritical(logCat) << getUid().toString() << "Failed to process response chunk";
        }
        // qDebug() << "Request" << m_url.toString() << "gained" << data.size() << "bytes";
    } else {
        qCCritical(logCat) << getUid().toString() << "Cannot write download data! illegal status " << m_status;
    }
}

auto NetRequest::abort() -> bool
{
    m_state = State::AbortedByUser;
    if (m_reply) {
        disconnect(m_reply.get(), &QNetworkReply::errorOccurred, nullptr, nullptr);
        m_reply->abort();
    }
    return true;
}

int NetRequest::replyStatusCode() const
{
    return m_reply ? m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : -1;
}

QNetworkReply::NetworkError NetRequest::error() const
{
    return m_reply ? m_reply->error() : QNetworkReply::NoError;
}

QUrl NetRequest::url() const
{
    return m_url;
}

QString NetRequest::errorString() const
{
    return m_reply ? m_reply->errorString() : "";
}
}  // namespace Net
