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

#include "AboutDialog.h"
#include <QIcon>
#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "ui_AboutDialog.h"

#include <net/NetJob.h>
#include <qobject.h>

namespace {
QString getLink(QString link, QString name)
{
    return QString("&lt;<a href='%1'>%2</a>&gt;").arg(link).arg(name);
}

QString getWebsite(QString link)
{
    return getLink(link, QObject::tr("Website"));
}

QString getGitHub(QString username)
{
    return getLink("https://github.com/" + username, "GitHub");
}

// Credits
// This is a hack, but I can't think of a better way to do this easily without screwing with QTextDocument...
QString getCreditsHtml()
{
    QString output;
    QTextStream stream(&output);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
    stream << "<center>\n";

    //: %1 is the name of the launcher, determined at build time, e.g. "Prism Launcher Developers"
    stream << "<h3>" << QObject::tr("%1 Developers", "About Credits").arg(BuildConfig.LAUNCHER_DISPLAYNAME) << "</h3>\n";
    stream << QString("<p>Sefa Eyeoglu (Scrumplex) %1</p>\n").arg(getWebsite("https://scrumplex.net"));
    stream << QString("<p>d-513 %1</p>\n").arg(getGitHub("d-513"));
    stream << QString("<p>txtsd %1</p>\n").arg(getWebsite("https://ihavea.quest"));
    stream << QString("<p>timoreo %1</p>\n").arg(getGitHub("timoreo22"));
    stream << QString("<p>ZekeZ %1</p>\n").arg(getGitHub("ZekeZDev"));
    stream << QString("<p>cozyGalvinism %1</p>\n").arg(getGitHub("cozyGalvinism"));
    stream << QString("<p>DioEgizio %1</p>\n").arg(getGitHub("DioEgizio"));
    stream << QString("<p>flowln %1</p>\n").arg(getGitHub("flowln"));
    stream << QString("<p>ViRb3 %1</p>\n").arg(getGitHub("ViRb3"));
    stream << QString("<p>Rachel Powers (Ryex) %1</p>\n").arg(getGitHub("Ryex"));
    stream << QString("<p>TayouVR %1</p>\n").arg(getGitHub("TayouVR"));
    stream << QString("<p>TheKodeToad %1</p>\n").arg(getGitHub("TheKodeToad"));
    stream << QString("<p>getchoo %1</p>\n").arg(getGitHub("getchoo"));
    stream << QString("<p>Alexandru Tripon (Trial97) %1</p>\n").arg(getGitHub("Trial97"));
    stream << "<br />\n";

    // TODO: possibly retrieve from git history at build time?
    //: %1 is the name of the launcher, determined at build time, e.g. "Prism Launcher Developers"
    stream << "<h3>" << QObject::tr("%1 Developers", "About Credits").arg("MultiMC") << "</h3>\n";
    stream << "<p>Andrew Okin &lt;<a href='mailto:forkk@forkk.net'>forkk@forkk.net</a>&gt;</p>\n";
    stream << QString("<p>Petr Mrázek &lt;<a href='mailto:peterix@gmail.com'>peterix@gmail.com</a>&gt;</p>\n");
    stream << "<p>Sky Welch &lt;<a href='mailto:multimc@bunnies.io'>multimc@bunnies.io</a>&gt;</p>\n";
    stream << "<p>Jan (02JanDal) &lt;<a href='mailto:02jandal@gmail.com'>02jandal@gmail.com</a>&gt;</p>\n";
    stream << "<p>RoboSky &lt;<a href='https://twitter.com/RoboSky_'>@RoboSky_</a>&gt;</p>\n";
    stream << "<br />\n";

    stream << "<h3>" << QObject::tr("With thanks to", "About Credits") << "</h3>\n";
    stream << QString("<p>Boba %1</p>\n").arg(getWebsite("https://bobaonline.neocities.org/"));
    stream << QString("<p>AutiOne %1</p>\n").arg(getWebsite("https://auti.one/"));
    stream << QString("<p>Fulmine %1</p>\n").arg(getWebsite("https://fulmine.xyz/"));
    stream << QString("<p>ely %1</p>\n").arg(getGitHub("elyrodso"));
    stream << QString("<p>gon sawa %1</p>\n").arg(getGitHub("gonsawa"));
    stream << QString("<p>Pankakes</p>\n");
    stream << QString("<p>tobimori %1</p>\n").arg(getGitHub("tobimori"));
    stream << "<p>Orochimarufan &lt;<a href='mailto:orochimarufan.x3@gmail.com'>orochimarufan.x3@gmail.com</a>&gt;</p>\n";
    stream << "<p>TakSuyu &lt;<a href='mailto:taksuyu@gmail.com'>taksuyu@gmail.com</a>&gt;</p>\n";
    stream << "<p>Kilobyte &lt;<a href='mailto:stiepen22@gmx.de'>stiepen22@gmx.de</a>&gt;</p>\n";
    stream << "<p>Rootbear75 &lt;<a href='https://twitter.com/rootbear75'>@rootbear75</a>&gt;</p>\n";
    stream << "<p>Zeker Zhayard &lt;<a href='https://twitter.com/zeker_zhayard'>@Zeker_Zhayard</a>&gt;</p>\n";
    stream << "<p>Everyone who helped establish our branding!</p>\n";
    stream
        << "<p>And everyone else who <a href='https://github.com/PrismLauncher/PrismLauncher/graphs/contributors'>contributed</a>!</p>\n";
    stream << "<br />\n";

    stream << "</center>\n";
    return output;
}

QString getLicenseHtml()
{
    QFile dataFile(":/documents/COPYING.md");
    dataFile.open(QIODevice::ReadOnly);
    QString output = markdownToHTML(dataFile.readAll());
    return output;
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString launcherName = BuildConfig.LAUNCHER_DISPLAYNAME;

    setWindowTitle(tr("About %1").arg(launcherName));

    QString chtml = getCreditsHtml();
    ui->creditsText->setHtml(StringUtils::htmlListPatch(chtml));

    QString lhtml = getLicenseHtml();
    ui->licenseText->setHtml(StringUtils::htmlListPatch(lhtml));

    ui->urlLabel->setOpenExternalLinks(true);

    ui->icon->setPixmap(APPLICATION->getThemedIcon("logo").pixmap(64));
    ui->title->setText(launcherName);

    ui->versionLabel->setText(BuildConfig.printableVersionString());

    if (!BuildConfig.BUILD_PLATFORM.isEmpty())
        ui->platformLabel->setText(tr("Platform") + ": " + BuildConfig.BUILD_PLATFORM);
    else
        ui->platformLabel->setVisible(false);

    if (!BuildConfig.GIT_COMMIT.isEmpty())
        ui->commitLabel->setText(tr("Commit: %1").arg(BuildConfig.GIT_COMMIT));
    else
        ui->commitLabel->setVisible(false);

    if (!BuildConfig.BUILD_DATE.isEmpty())
        ui->buildDateLabel->setText(tr("Build date: %1").arg(BuildConfig.BUILD_DATE));
    else
        ui->buildDateLabel->setVisible(false);

    if (!BuildConfig.VERSION_CHANNEL.isEmpty())
        ui->channelLabel->setText(tr("Channel") + ": " + BuildConfig.VERSION_CHANNEL);
    else
        ui->channelLabel->setVisible(false);

    QString urlText("<html><head/><body><p><a href=\"%1\">%1</a></p></body></html>");
    ui->urlLabel->setText(urlText.arg(BuildConfig.LAUNCHER_GIT));

    ui->copyLabel->setText(BuildConfig.LAUNCHER_COPYRIGHT);

    connect(ui->closeButton, SIGNAL(clicked()), SLOT(close()));

    connect(ui->aboutQt, &QPushButton::clicked, &QApplication::aboutQt);
    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
