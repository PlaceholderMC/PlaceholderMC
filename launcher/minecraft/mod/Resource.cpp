#include "Resource.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <tuple>

#include "FileSystem.h"
#include "StringUtils.h"

Resource::Resource(QObject* parent) : QObject(parent) {}

Resource::Resource(QFileInfo file_info) : QObject()
{
    setFile(file_info);
}

void Resource::setFile(QFileInfo file_info)
{
    m_file_info = file_info;
    parseFile();
}

static std::tuple<QString, qint64> calculateFileSize(const QFileInfo& file)
{
    if (file.isDir()) {
        auto dir = QDir(file.absoluteFilePath());
        dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
        auto count = dir.count();
        auto str = QObject::tr("item");
        if (count != 1)
            str = QObject::tr("items");
        return { QString("%1 %2").arg(QString::number(count), str), count };
    }
    return { StringUtils::humanReadableFileSize(file.size(), true), file.size() };
}

void Resource::parseFile()
{
    QString file_name{ m_file_info.fileName() };

    m_type = ResourceType::UNKNOWN;

    m_internal_id = file_name;

    std::tie(m_size_str, m_size_info) = calculateFileSize(m_file_info);
    if (m_file_info.isDir()) {
        m_type = ResourceType::FOLDER;
        m_name = file_name;
    } else if (m_file_info.isFile()) {
        if (file_name.endsWith(".disabled")) {
            file_name.chop(9);
            m_enabled = false;
        }

        if (file_name.endsWith(".zip") || file_name.endsWith(".jar")) {
            m_type = ResourceType::ZIPFILE;
            file_name.chop(4);
        } else if (file_name.endsWith(".nilmod")) {
            m_type = ResourceType::ZIPFILE;
            file_name.chop(7);
        } else if (file_name.endsWith(".litemod")) {
            m_type = ResourceType::LITEMOD;
            file_name.chop(8);
        } else {
            m_type = ResourceType::SINGLEFILE;
        }

        m_name = file_name;
    }

    m_changed_date_time = m_file_info.lastModified();
}

auto Resource::name() const -> QString
{
    if (metadata())
        return metadata()->name;

    return m_name;
}

static void removeThePrefix(QString& string)
{
    QRegularExpression regex(QStringLiteral("^(?:the|teh) +"), QRegularExpression::CaseInsensitiveOption);
    string.remove(regex);
    string = string.trimmed();
}

auto Resource::provider() const -> QString
{
    if (metadata())
        return ModPlatform::ProviderCapabilities::readableName(metadata()->provider);

    return tr("Unknown");
}

auto Resource::homepage() const -> QString
{
    if (metadata())
        return ModPlatform::getMetaURL(metadata()->provider, metadata()->project_id);

    return {};
}

bool Resource::lockUpdate() const
{
    if (metadata())
        return metadata()->lockUpdate;

    return false;
}

void Resource::setMetadata(std::shared_ptr<Metadata::ModStruct>&& metadata)
{
    if (status() == ResourceStatus::NO_METADATA)
        setStatus(ResourceStatus::INSTALLED);

    m_metadata = metadata;
}

int Resource::compare(const Resource& other, SortType type) const
{
    switch (type) {
        default:
        case SortType::ENABLED:
            if (enabled() && !other.enabled())
                return 1;
            if (!enabled() && other.enabled())
                return -1;
            break;
        case SortType::NAME: {
            QString this_name{ name() };
            QString other_name{ other.name() };

            // TODO do we need this? it could result in 0 being returned
            removeThePrefix(this_name);
            removeThePrefix(other_name);

            return QString::compare(this_name, other_name, Qt::CaseInsensitive);
        }
        case SortType::DATE:
            if (dateTimeChanged() > other.dateTimeChanged())
                return 1;
            if (dateTimeChanged() < other.dateTimeChanged())
                return -1;
            break;
        case SortType::SIZE: {
            if (this->type() != other.type()) {
                if (this->type() == ResourceType::FOLDER)
                    return -1;
                if (other.type() == ResourceType::FOLDER)
                    return 1;
            }

            if (sizeInfo() > other.sizeInfo())
                return 1;
            if (sizeInfo() < other.sizeInfo())
                return -1;
            break;
        }
        case SortType::PROVIDER: {
            auto compare_result = QString::compare(provider(), other.provider(), Qt::CaseInsensitive);
            if (compare_result != 0)
                return compare_result;
            break;
        }
    }

    return 0;
}

bool Resource::applyFilter(QRegularExpression filter) const
{
    return filter.match(name()).hasMatch();
}

bool Resource::enable(EnableAction action)
{
    if (m_type == ResourceType::UNKNOWN || m_type == ResourceType::FOLDER)
        return false;

    QString path = m_file_info.absoluteFilePath();
    QFile file(path);

    bool enable = true;
    switch (action) {
        case EnableAction::ENABLE:
            enable = true;
            break;
        case EnableAction::DISABLE:
            enable = false;
            break;
        case EnableAction::TOGGLE:
        default:
            enable = !enabled();
            break;
    }

    if (m_enabled == enable)
        return false;

    if (enable) {
        // m_enabled is false, but there's no '.disabled' suffix.
        // TODO: Report error?
        if (!path.endsWith(".disabled"))
            return false;
        path.chop(9);
    } else {
        path += ".disabled";
        if (QFile::exists(path)) {
            path = FS::getUniqueResourceName(path);
        }
    }
    if (!file.rename(path))
        return false;

    setFile(QFileInfo(path));

    m_enabled = enable;
    return true;
}

auto Resource::destroy(const QDir& index_dir, bool preserve_metadata, bool attempt_trash) -> bool
{
    m_type = ResourceType::UNKNOWN;

    if (!preserve_metadata) {
        qDebug() << QString("Destroying metadata for '%1' on purpose").arg(name());
        destroyMetadata(index_dir);
    }

    return (attempt_trash && FS::trash(m_file_info.filePath())) || FS::deletePath(m_file_info.filePath());
}

auto Resource::destroyMetadata(const QDir& index_dir) -> void
{
    if (metadata()) {
        Metadata::remove(index_dir, metadata()->slug);
    } else {
        auto n = name();
        Metadata::remove(index_dir, n);
    }
    m_metadata = nullptr;
}

bool Resource::isSymLinkUnder(const QString& instPath) const
{
    if (isSymLink())
        return true;

    auto instDir = QDir(instPath);

    auto relAbsPath = instDir.relativeFilePath(m_file_info.absoluteFilePath());
    auto relCanonPath = instDir.relativeFilePath(m_file_info.canonicalFilePath());

    return relAbsPath != relCanonPath;
}

bool Resource::isMoreThanOneHardLink() const
{
    return FS::hardLinkCount(m_file_info.absoluteFilePath()) > 1;
}

auto Resource::getOriginalFileName() const -> QString
{
    auto fileName = m_file_info.fileName();
    if (!m_enabled)
        fileName.chop(9);
    return fileName;
}