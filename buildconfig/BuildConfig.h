// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#pragma once
#include <QList>
#include <QString>

/**
 * \brief The Config class holds all the build-time information passed from the build system.
 */
class Config {
   public:
    Config();
    QString LAUNCHER_NAME;
    QString LAUNCHER_APP_BINARY_NAME;
    QString LAUNCHER_DISPLAYNAME;
    QString LAUNCHER_COPYRIGHT;
    QString LAUNCHER_DOMAIN;
    QString LAUNCHER_CONFIGFILE;
    QString LAUNCHER_GIT;
    QString LAUNCHER_DESKTOPFILENAME;
    QString LAUNCHER_SVGFILENAME;

    /// The major version number.
    int VERSION_MAJOR;
    /// The minor version number.
    int VERSION_MINOR;

    /**
     * The version channel
     * This is used by the updater to determine what channel the current version came from.
     */
    QString VERSION_CHANNEL;

    bool UPDATER_ENABLED = false;
    bool JAVA_DOWNLOADER_ENABLED = false;

    /// A short string identifying this build's platform or distribution.
    QString BUILD_PLATFORM;

    /// A short string identifying this build's valid artifacts int he updater. For example, "lin64" or "win32".
    QString BUILD_ARTIFACT;

    /// A string containing the build timestamp
    QString BUILD_DATE;

    /// A string identifying the compiler use to build
    QString COMPILER_NAME;

    /// A string identifying the compiler version used to build
    QString COMPILER_VERSION;

    /// A string identifying the compiler target system os
    QString COMPILER_TARGET_SYSTEM;

    /// A String identifying the compiler target system version
    QString COMPILER_TARGET_SYSTEM_VERSION;

    /// A String identifying the compiler target processor
    QString COMPILER_TARGET_SYSTEM_PROCESSOR;

    /// URL for the updater's channel
    QString UPDATER_GITHUB_REPO;

    /// The public key used to sign releases for the Sparkle updater appcast
    QString MAC_SPARKLE_PUB_KEY;

    /// URL for the Sparkle updater's appcast
    QString MAC_SPARKLE_APPCAST_URL;

    /// User-Agent to use.
    QString USER_AGENT;

    /// The git commit hash of this build
    QString GIT_COMMIT;

    /// The git tag of this build
    QString GIT_TAG;

    /// The git refspec of this build
    QString GIT_REFSPEC;

    /**
     * This is used to fetch the news RSS feed.
     * It defaults in CMakeLists.txt to "https://multimc.org/rss.xml"
     */
    QString NEWS_RSS_URL;

    /**
     * URL that gets opened when the user clicks "More News"
     */
    QString NEWS_OPEN_URL;

    /**
     * URL (with arg %1 to be substituted with page-id) that gets opened when the user requests help
     */
    QString HELP_URL;

    /**
     * URL that gets opened when the user succesfully logins.
     */
    QString LOGIN_CALLBACK_URL;

    /**
     * Client ID you can get from Imgur when you register an application
     */
    QString IMGUR_CLIENT_ID;

    /**
     * Client ID you can get from Microsoft Identity Platform when you register an application
     */
    QString MSA_CLIENT_ID;

    /**
     * Client API key for CurseForge
     */
    QString FLAME_API_KEY;

    /**
     * Metadata repository URL prefix
     */
    QString META_URL;

    QString GLFW_LIBRARY_NAME;
    QString OPENAL_LIBRARY_NAME;

    QString BUG_TRACKER_URL;
    QString TRANSLATIONS_URL;
    QString MATRIX_URL;
    QString DISCORD_URL;
    QString SUBREDDIT_URL;

    QString RESOURCE_BASE = "https://resources.download.minecraft.net/";
    QString LIBRARY_BASE = "https://libraries.minecraft.net/";
    QString IMGUR_BASE_URL = "https://api.imgur.com/3/";
    QString FMLLIBS_BASE_URL;
    QString TRANSLATION_FILES_URL;

    QString MODPACKSCH_API_BASE_URL = "https://api.modpacks.ch/";

    QString LEGACY_FTB_CDN_BASE_URL = "https://dist.creeper.host/FTB2/";

    QString ATL_DOWNLOAD_SERVER_URL = "https://download.nodecdn.net/containers/atl/";
    QString ATL_API_BASE_URL = "https://api.atlauncher.com/v1/";

    QString TECHNIC_API_BASE_URL = "https://api.technicpack.net/";
    /**
     * The build that is reported to the Technic API.
     */
    QString TECHNIC_API_BUILD = "multimc";

    QString MODRINTH_STAGING_URL = "https://staging-api.modrinth.com/v2";
    QString MODRINTH_PROD_URL = "https://api.modrinth.com/v2";
    QStringList MODRINTH_MRPACK_HOSTS{ "cdn.modrinth.com", "github.com", "raw.githubusercontent.com", "gitlab.com" };

    QString FLAME_BASE_URL = "https://api.curseforge.com/v1";

    QString versionString() const;
    /**
     * \brief Converts the Version to a string.
     * \return The version number in string format (major.minor.revision.build).
     */
    QString printableVersionString() const;

    /**
     * \brief Compiler ID String
     * \return a string of the form "Name - Version"  of just "Name" if the version is empty
     */
    QString compilerID() const;

    /**
     * \brief System ID String
     * \return a string of the form "OS Verison Processor"
     */
    QString systemID() const;
};

extern const Config BuildConfig;
