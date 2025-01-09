/*

Code has been taken from https://github.com/natefoo/lionshead and loosely
translated to C++ laced with Qt.

MIT License

Copyright (c) 2017 Nate Coraor

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "distroutils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

#include <functional>

Sys::DistributionInfo Sys::read_os_release()
{
    Sys::DistributionInfo out;
    QStringList files = { "/etc/os-release", "/usr/lib/os-release" };
    QString name;
    QString version;
    for (auto& file : files) {
        if (!QFile::exists(file)) {
            continue;
        }
        QSettings settings(file, QSettings::IniFormat);
        if (settings.contains("ID")) {
            name = settings.value("ID").toString().toLower();
        } else if (settings.contains("NAME")) {
            name = settings.value("NAME").toString().toLower();
        } else {
            continue;
        }

        if (settings.contains("VERSION_ID")) {
            version = settings.value("VERSION_ID").toString().toLower();
        } else if (settings.contains("VERSION")) {
            version = settings.value("VERSION").toString().toLower();
        }
        break;
    }
    if (name.isEmpty()) {
        return out;
    }
    out.distributionName = name;
    out.distributionVersion = version;
    return out;
}

bool Sys::main_lsb_info(Sys::LsbInfo& out)
{
    int status = 0;
    QProcess lsbProcess;
    QStringList arguments;
    arguments << "-a";
    lsbProcess.start("lsb_release", arguments);
    lsbProcess.waitForFinished();
    status = lsbProcess.exitStatus();
    QString output = lsbProcess.readAllStandardOutput();
    qDebug() << output;
    lsbProcess.close();
    if (status == 0) {
        auto lines = output.split('\n');
        for (auto line : lines) {
            int index = line.indexOf(':');
            auto key = line.left(index).trimmed();
            auto value = line.mid(index + 1).toLower().trimmed();
            if (key == "Distributor ID")
                out.distributor = value;
            else if (key == "Release")
                out.version = value;
            else if (key == "Description")
                out.description = value;
            else if (key == "Codename")
                out.codename = value;
        }
        return !out.distributor.isEmpty();
    }
    return false;
}

bool Sys::fallback_lsb_info(Sys::LsbInfo& out)
{
    // running lsb_release failed, try to read the file instead
    // /etc/lsb-release format, if the file even exists, is non-standard.
    // Only the `lsb_release` command is specified by LSB. Nonetheless, some
    // distributions install an /etc/lsb-release as part of the base
    // distribution, but `lsb_release` remains optional.
    QString file = "/etc/lsb-release";
    if (QFile::exists(file)) {
        QSettings settings(file, QSettings::IniFormat);
        if (settings.contains("DISTRIB_ID")) {
            out.distributor = settings.value("DISTRIB_ID").toString().toLower();
        }
        if (settings.contains("DISTRIB_RELEASE")) {
            out.version = settings.value("DISTRIB_RELEASE").toString().toLower();
        }
        return !out.distributor.isEmpty();
    }
    return false;
}

void Sys::lsb_postprocess(Sys::LsbInfo& lsb, Sys::DistributionInfo& out)
{
    QString dist = lsb.distributor;
    QString vers = lsb.version;
    if (dist.startsWith("redhatenterprise")) {
        dist = "rhel";
    } else if (dist == "archlinux") {
        dist = "arch";
    } else if (dist.startsWith("suse")) {
        if (lsb.description.startsWith("opensuse")) {
            dist = "opensuse";
        } else if (lsb.description.startsWith("suse linux enterprise")) {
            dist = "sles";
        }
    } else if (dist == "debian" and vers == "testing") {
        vers = lsb.codename;
    } else {
        // ubuntu, debian, gentoo, scientific, slackware, ... ?
        auto parts = dist.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size()) {
            dist = parts[0];
        }
    }
    if (!dist.isEmpty()) {
        out.distributionName = dist;
        out.distributionVersion = vers;
    }
}

Sys::DistributionInfo Sys::read_lsb_release()
{
    LsbInfo lsb;
    if (!main_lsb_info(lsb)) {
        if (!fallback_lsb_info(lsb)) {
            return Sys::DistributionInfo();
        }
    }
    Sys::DistributionInfo out;
    lsb_postprocess(lsb, out);
    return out;
}

QString Sys::_extract_distribution(const QString& x)
{
    QString release = x.toLower();
    if (release.startsWith("red hat enterprise")) {
        return "rhel";
    }
    if (release.startsWith("suse linux enterprise")) {
        return "sles";
    }
    QStringList list = release.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (list.size()) {
        return list[0];
    }
    return QString();
}

QString Sys::_extract_version(const QString& x)
{
    QRegularExpression versionish_string(QRegularExpression::anchoredPattern("\\d+(?:\\.\\d+)*$"));
    QStringList list = x.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (int i = list.size() - 1; i >= 0; --i) {
        QString chunk = list[i];
        if (versionish_string.match(chunk).hasMatch()) {
            return chunk;
        }
    }
    return QString();
}

Sys::DistributionInfo Sys::read_legacy_release()
{
    struct checkEntry {
        QString file;
        std::function<QString(const QString&)> extract_distro;
        std::function<QString(const QString&)> extract_version;
    };
    QList<checkEntry> checks = {
        { "/etc/arch-release", [](const QString&) { return "arch"; }, [](const QString&) { return "rolling"; } },
        { "/etc/slackware-version", &Sys::_extract_distribution, &Sys::_extract_version },
        { QString(), &Sys::_extract_distribution, &Sys::_extract_version },
        { "/etc/debian_version", [](const QString&) { return "debian"; }, [](const QString& x) { return x; } },
    };
    for (auto& check : checks) {
        QStringList files;
        if (check.file.isNull()) {
            QDir etcDir("/etc");
            etcDir.setNameFilters({ "*-release" });
            etcDir.setFilter(QDir::Files | QDir::NoDot | QDir::NoDotDot | QDir::Readable | QDir::Hidden);
            files = etcDir.entryList();
        } else {
            files.append(check.file);
        }
        for (auto file : files) {
            QFile relfile(file);
            if (!relfile.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QString contents = QString::fromUtf8(relfile.readLine()).trimmed();
            QString dist = check.extract_distro(contents);
            QString vers = check.extract_version(contents);
            if (!dist.isEmpty()) {
                Sys::DistributionInfo out;
                out.distributionName = dist;
                out.distributionVersion = vers;
                return out;
            }
        }
    }
    return Sys::DistributionInfo();
}
