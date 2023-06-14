// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PolyMC - Minecraft Launcher
 *  Copyright (C) 2022,2023 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "LauncherPartLaunch.h"

#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QtGlobal>

#include "BuildConfig.h"
#include "MangoHud.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "FileSystem.h"
#include "Commandline.h"
#include "Application.h"

#ifdef Q_OS_LINUX
#include "gamemode_client.h"
#endif

LauncherPartLaunch::LauncherPartLaunch(LaunchTask *parent) : LaunchStep(parent)
{
    auto instance = parent->instance();
    if (instance->settings()->get("CloseAfterLaunch").toBool())
    {
        std::shared_ptr<QMetaObject::Connection> connection{new QMetaObject::Connection};
        *connection = connect(&m_process, &LoggedProcess::log, this, [=](QStringList lines, MessageLevel::Enum level) {
            qDebug() << lines;
            if (lines.filter(QRegularExpression(".*Setting user.+", QRegularExpression::CaseInsensitiveOption)).length() != 0)
            {
                APPLICATION->closeAllWindows();
                disconnect(*connection);
            }
        });
    }

    connect(&m_process, &LoggedProcess::log, this, &LauncherPartLaunch::logLines);
    connect(&m_process, &LoggedProcess::stateChanged, this, &LauncherPartLaunch::on_state);
}

#ifdef Q_OS_WIN
// returns 8.3 file format from long path
#include <windows.h>
QString shortPathName(const QString & file)
{
    auto input = file.toStdWString();
    std::wstring output;
    long length = GetShortPathNameW(input.c_str(), NULL, 0);
    // NOTE: this resizing might seem weird...
    // when GetShortPathNameW fails, it returns length including null character
    // when it succeeds, it returns length excluding null character
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364989(v=vs.85).aspx
    output.resize(length);
    GetShortPathNameW(input.c_str(),(LPWSTR)output.c_str(),length);
    output.resize(length-1);
    QString ret = QString::fromStdWString(output);
    return ret;
}
#endif

// if the string survives roundtrip through local 8bit encoding...
bool fitsInLocal8bit(const QString & string)
{
    return string == QString::fromLocal8Bit(string.toLocal8Bit());
}

void LauncherPartLaunch::executeTask()
{
    QString jarPath = APPLICATION->getJarPath("NewLaunch.jar");
    if (jarPath.isEmpty())
    {
        const char *reason = QT_TR_NOOP("Launcher library could not be found. Please check your installation.");
        emit logLine(tr(reason), MessageLevel::Fatal);
        emitFailed(tr(reason));
        return;
    }

    auto instance = m_parent->instance();
    std::shared_ptr<MinecraftInstance> minecraftInstance = std::dynamic_pointer_cast<MinecraftInstance>(instance);

    m_launchScript = minecraftInstance->createLaunchScript(m_session, m_serverToJoin);
    QStringList args = minecraftInstance->javaArguments();
    QString allArgs = args.join(", ");
    emit logLine("Java Arguments:\n[" + m_parent->censorPrivateInfo(allArgs) + "]\n\n", MessageLevel::Launcher);

    m_process.setProcessEnvironment(instance->createLaunchEnvironment());

    // make detachable - this will keep the process running even if the object is destroyed
    m_process.setDetachable(true);

    auto classPath = minecraftInstance->getClassPath();
    classPath.prepend(jarPath);

    auto natPath = minecraftInstance->getNativePath();
#ifdef Q_OS_WIN
    if (!fitsInLocal8bit(natPath))
    {
        args << "-Djava.library.path=" + shortPathName(natPath);
    }
    else
    {
        args << "-Djava.library.path=" + natPath;
    }
#else
    args << "-Djava.library.path=" + natPath;
#endif
 
    args << "-cp";
#ifdef Q_OS_WIN
    QStringList processed;
    for(auto & item: classPath)
    {
        if (!fitsInLocal8bit(item))
        {
            processed << shortPathName(item);
        }
        else
        {
            processed << item;
        }
    }
    args << processed.join(';');
#else
    args << classPath.join(':');
#endif
    args << "org.prismlauncher.EntryPoint";

    qDebug() << args.join(' ');

    args.prepend(FS::ResolveExecutable(instance->settings()->get("JavaPath").toString()));

    const auto wantSandbox = minecraftInstance->settings()->get("EnableSandboxing").toBool();
    auto canSandbox = false;

#ifdef Q_OS_LINUX
    const auto bwrapPath = MangoHud::getBwrapBinary();
    canSandbox = !bwrapPath.isEmpty();
#endif

    if (wantSandbox && !canSandbox) {
        const char *reason = QT_TR_NOOP("Sandboxing was requested, but is NOT available on your system.\nPlease turn off sandboxing to proceed launching.");
        emit logLine(tr(reason), MessageLevel::Error);
        emitFailed(tr(reason));
        return;
    }

#ifdef Q_OS_LINUX
    if (wantSandbox && canSandbox) {
        QString actualRuntimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        QString sandboxedRuntimeDir = "/tmp";

        const static QStringList systemBinds{
            "/etc",
            "/usr",
            "/bin",
            "/sbin",
            "/lib",
            "/lib32",
            "/lib64",
            "/sys/class",
            "/sys/dev/char",
            "/sys/devices/pci0000:00",
            "/sys/devices/system/cpu",
            "/run/systemd/resolve",
        };

        QStringList bwrapArgs{};
        bwrapArgs << bwrapPath; // will either be taken out using takeFirst or passed to wrapper command
        bwrapArgs << "--unshare-all";
        bwrapArgs << "--share-net";
        bwrapArgs << "--die-with-parent";
        bwrapArgs << "--unsetenv" << "DBUS_SESSION_BUS_ADDRESS";

        // default binds
        bwrapArgs << "--dev" << "/dev";
        bwrapArgs << "--dev-bind-try" << "/dev/dri" << "/dev/dri";
        bwrapArgs << "--proc" << "/proc";
        bwrapArgs << "--setenv" << "XDG_RUNTIME_DIR" << sandboxedRuntimeDir;

        for (auto path : systemBinds) {
            bwrapArgs << "--ro-bind-try" << path << path;
        }

        // desktop integration
        bwrapArgs << "--ro-bind-try" << QString("%1/pulse").arg(actualRuntimeDir) << QString("%1/pulse").arg(sandboxedRuntimeDir);
        bwrapArgs << "--ro-bind-try" << QString("%1/pipewire-0").arg(actualRuntimeDir) << QString("%1/pipewire-0").arg(sandboxedRuntimeDir);
        {
            auto display = qEnvironmentVariable("DISPLAY");
            auto xAuthPath = qEnvironmentVariable("XAUTHORITY",
                    FS::PathCombine(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), ".Xauthority"));
            auto wlDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");

            if (display.startsWith(':')) {
                auto x11Socket = QString("/tmp/.X11-unix/X%1").arg(display.mid(1));
                bwrapArgs << "--ro-bind-try" << x11Socket << x11Socket;
            }

            bwrapArgs << "--ro-bind-try" << xAuthPath << xAuthPath;

            if (wlDisplay.startsWith('/'))
                bwrapArgs << "--ro-bind-try" << wlDisplay << wlDisplay;
            else
                bwrapArgs << "--ro-bind-try" << FS::PathCombine(actualRuntimeDir, wlDisplay) << FS::PathCombine(sandboxedRuntimeDir, wlDisplay);
        }

        if (minecraftInstance->settings()->get("EnableMangoHud").toBool() && APPLICATION->capabilities() & Application::SupportsMangoHud) {
            auto mangoHudConfigPath = qEnvironmentVariable("MANGOHUD_CONFIGFILE",
                    FS::PathCombine(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), "MangoHud")); 
            bwrapArgs << "--ro-bind-try" << mangoHudConfigPath << mangoHudConfigPath;
        }

        // launcher
        {
            QString instPath = QDir::toNativeSeparators(QDir(minecraftInstance->instanceRoot()).absolutePath());
            QString assetsPath = QDir::toNativeSeparators(QDir("assets").absolutePath());
            QString librariesPath = QDir::toNativeSeparators(QDir("libraries").absolutePath());

            bwrapArgs << "--bind" << instPath << instPath;
            bwrapArgs << "--ro-bind" << assetsPath << assetsPath;
            bwrapArgs << "--ro-bind" << librariesPath << librariesPath;

            // also add NewLaunch.jar just to be safe. This is probably covered by /usr or LINUX_BWRAP_EXTRA_ARGS though
            bwrapArgs << "--ro-bind" << jarPath << jarPath;
        }

        bwrapArgs << Commandline::splitArgs(BuildConfig.LINUX_BWRAP_EXTRA_ARGS);
        // TODO: add args from environment variable?
        bwrapArgs << Commandline::splitArgs(minecraftInstance->settings()->get("BwrapExtraArgs").toString());

        bwrapArgs << "--";
        args = bwrapArgs + args;
    }
#endif

    qDebug() << args.join(' ');

    QString wrapperCommandStr = instance->getWrapperCommand().trimmed();
    if(!wrapperCommandStr.isEmpty())
    {
        auto wrapperArgs = Commandline::splitArgs(wrapperCommandStr);
        auto wrapperCommand = wrapperArgs.takeFirst();
        auto realWrapperCommand = QStandardPaths::findExecutable(wrapperCommand);
        if (realWrapperCommand.isEmpty())
        {
            const char *reason = QT_TR_NOOP("The wrapper command \"%1\" couldn't be found.");
            emit logLine(QString(reason).arg(wrapperCommand), MessageLevel::Fatal);
            emitFailed(tr(reason).arg(wrapperCommand));
            return;
        }
        emit logLine("Wrapper command is:\n" + wrapperCommandStr + "\n\n", MessageLevel::Launcher);
        m_process.start(wrapperCommand, wrapperArgs + args);
    }
    else
    {
        m_process.start(args.takeFirst(), args);
    }

#ifdef Q_OS_LINUX
    if (instance->settings()->get("EnableFeralGamemode").toBool() && APPLICATION->capabilities() & Application::SupportsGameMode)
    {
        auto pid = m_process.processId();
        if (pid)
        {
            gamemode_request_start_for(pid);
        }
    }
#endif
}

void LauncherPartLaunch::on_state(LoggedProcess::State state)
{
    switch(state)
    {
        case LoggedProcess::FailedToStart:
        {
            //: Error message displayed if instace can't start
            const char *reason = QT_TR_NOOP("Could not launch Minecraft!");
            emit logLine(reason, MessageLevel::Fatal);
            emitFailed(tr(reason));
            return;
        }
        case LoggedProcess::Aborted:
        case LoggedProcess::Crashed:
        {
            m_parent->setPid(-1);
            emitFailed(tr("Game crashed."));
            return;
        }
        case LoggedProcess::Finished:
        {
            auto instance = m_parent->instance();
            if (instance->settings()->get("CloseAfterLaunch").toBool())
                APPLICATION->showMainWindow();

            m_parent->setPid(-1);
            // if the exit code wasn't 0, report this as a crash
            auto exitCode = m_process.exitCode();
            if(exitCode != 0)
            {
                emitFailed(tr("Game crashed."));
                return;
            }
            //FIXME: make this work again
            // m_postlaunchprocess.processEnvironment().insert("INST_EXITCODE", QString(exitCode));
            // run post-exit
            emitSucceeded();
            break;
        }
        case LoggedProcess::Running:
            emit logLine(QString("Minecraft process ID: %1\n\n").arg(m_process.processId()), MessageLevel::Launcher);
            m_parent->setPid(m_process.processId());
            m_parent->instance()->setLastLaunch();
            // send the launch script to the launcher part
            m_process.write(m_launchScript.toUtf8());

            mayProceed = true;
            emit readyForLaunch();
            break;
        default:
            break;
    }
}

void LauncherPartLaunch::setWorkingDirectory(const QString &wd)
{
    m_process.setWorkingDirectory(wd);
}

void LauncherPartLaunch::proceed()
{
    if(mayProceed)
    {
        QString launchString("launch\n");
        m_process.write(launchString.toUtf8());
        mayProceed = false;
    }
}

bool LauncherPartLaunch::abort()
{
    if(mayProceed)
    {
        mayProceed = false;
        QString launchString("abort\n");
        m_process.write(launchString.toUtf8());
    }
    else
    {
        auto state = m_process.state();
        if (state == LoggedProcess::Running || state == LoggedProcess::Starting)
        {
            m_process.kill();
        }
    }
    return true;
}
