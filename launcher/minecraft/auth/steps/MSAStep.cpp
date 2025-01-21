// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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

#include "MSAStep.h"

#include <QAbstractOAuth2>
#include <QNetworkRequest>
#include <QOAuthHttpServerReplyHandler>
#include <QOAuthOobReplyHandler>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

bool isSchemeHandlerRegistered()
{
#ifdef Q_OS_LINUX
    QProcess process;
    process.start("xdg-mime", { "query", "default", "x-scheme-handler/" + BuildConfig.LAUNCHER_APP_BINARY_NAME });
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();

    return output.contains(BuildConfig.LAUNCHER_APP_BINARY_NAME);

#elif defined(Q_OS_WIN)
    QString regPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    QSettings settings(regPath, QSettings::NativeFormat);

    return settings.contains("shell/open/command/.");
#endif
    return true;
}

class CustomOAuthOobReplyHandler : public QOAuthOobReplyHandler {
    Q_OBJECT

   public:
    explicit CustomOAuthOobReplyHandler(QObject* parent = nullptr) : QOAuthOobReplyHandler(parent)
    {
        connect(APPLICATION, &Application::oauthReplyRecieved, this, &QOAuthOobReplyHandler::callbackReceived);
    }
    ~CustomOAuthOobReplyHandler() override
    {
        disconnect(APPLICATION, &Application::oauthReplyRecieved, this, &QOAuthOobReplyHandler::callbackReceived);
    }
    QString callback() const override { return BuildConfig.LAUNCHER_APP_BINARY_NAME + "://oauth/microsoft"; }
};

MSAStep::MSAStep(AccountData* data, bool silent) : AuthStep(data), m_silent(silent)
{
    m_clientId = APPLICATION->getMSAClientID();
    if (QCoreApplication::applicationFilePath().startsWith("/tmp/.mount_") || APPLICATION->isPortable() || !isSchemeHandlerRegistered())

    {
        auto replyHandler = new QOAuthHttpServerReplyHandler(this);
        replyHandler->setCallbackText(QString(R"XXX(
    <noscript>
      <meta http-equiv="Refresh" content="0; URL=%1" />
    </noscript>
    Login Successful, redirecting...
    <script>
      window.location.replace("%1");
    </script>
    )XXX")
                                          .arg(BuildConfig.LOGIN_CALLBACK_URL));
        oauth2.setReplyHandler(replyHandler);
    } else {
        oauth2.setReplyHandler(new CustomOAuthOobReplyHandler(this));
    }
    oauth2.setAuthorizationUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"));
    oauth2.setAccessTokenUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    oauth2.setScope("XboxLive.SignIn XboxLive.offline_access");
    oauth2.setClientIdentifier(m_clientId);
    oauth2.setNetworkAccessManager(APPLICATION->network().get());

    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this] {
        m_data->msaClientID = oauth2.clientIdentifier();
        m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
        m_data->msaToken.notAfter = oauth2.expirationAt();
        m_data->msaToken.extra = oauth2.extraTokens();
        m_data->msaToken.refresh_token = oauth2.refreshToken();
        m_data->msaToken.token = oauth2.token();
        emit finished(AccountTaskState::STATE_WORKING, tr("Got "));
    });
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, &MSAStep::authorizeWithBrowser);
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::requestFailed, this, [this, silent](const QAbstractOAuth2::Error err) {
        auto state = AccountTaskState::STATE_FAILED_HARD;
        if (oauth2.status() == QAbstractOAuth::Status::Granted || silent) {
            if (err == QAbstractOAuth2::Error::NetworkError) {
                state = AccountTaskState::STATE_OFFLINE;
            } else {
                state = AccountTaskState::STATE_FAILED_SOFT;
            }
        }
        auto message = tr("Microsoft user authentication failed.");
        if (silent) {
            message = tr("Failed to refresh token.");
        }
        qWarning() << message;
        emit finished(state, message);
    });
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::error, this,
            [this](const QString& error, const QString& errorDescription, const QUrl& uri) {
                qWarning() << "Failed to login because" << error << errorDescription;
                emit finished(AccountTaskState::STATE_FAILED_HARD, errorDescription);
            });

    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::extraTokensChanged, this,
            [this](const QVariantMap& tokens) { m_data->msaToken.extra = tokens; });

    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::clientIdentifierChanged, this,
            [this](const QString& clientIdentifier) { m_data->msaClientID = clientIdentifier; });
}

QString MSAStep::describe()
{
    return tr("Logging in with Microsoft account.");
}

void MSAStep::perform()
{
    if (m_silent) {
        if (m_data->msaClientID != m_clientId) {
            emit finished(AccountTaskState::STATE_DISABLED,
                          tr("Microsoft user authentication failed - client identification has changed."));
            return;
        }
        if (m_data->msaToken.refresh_token.isEmpty()) {
            emit finished(AccountTaskState::STATE_DISABLED, tr("Microsoft user authentication failed - refresh token is empty."));
            return;
        }
        oauth2.setRefreshToken(m_data->msaToken.refresh_token);
        oauth2.refreshAccessToken();
    } else {
        oauth2.setModifyParametersFunction(
            [](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* map) { map->insert("prompt", "select_account"); });

        *m_data = AccountData();
        m_data->msaClientID = m_clientId;
        oauth2.grant();
    }
}

#include "MSAStep.moc"