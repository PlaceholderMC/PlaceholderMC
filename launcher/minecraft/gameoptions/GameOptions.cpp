#include "GameOptions.h"
#include <QDebug>
#include <QSaveFile>
#include "FileSystem.h"

namespace {
bool load(const QString& path, std::vector<GameOptionItem>& contents, int& version)
{
    contents.clear();
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Failed to read options file.";
        return false;
    }
    version = 0;
    while (!file.atEnd()) {
        auto line = file.readLine();
        if (line.endsWith('\n')) {
            line.chop(1);
        }
        auto separatorIndex = line.indexOf(':');
        if (separatorIndex == -1) {
            continue;
        }
        auto key = QString::fromUtf8(line.data(), separatorIndex);
        auto value = QString::fromUtf8(line.data() + separatorIndex + 1, line.size() - 1 - separatorIndex);
        qDebug() << "!!" << key << "!!";
        if (key == "version") {
            version = value.toInt();
            continue;
        }
        contents.emplace_back(GameOptionItem{ key, value });
    }
    qDebug() << "Loaded" << path << "with version:" << version;
    return true;
}
bool save(const QString& path, std::vector<GameOptionItem>& mapping, int version)
{
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (version != 0) {
        QString versionLine = QString("version:%1\n").arg(version);
        out.write(versionLine.toUtf8());
    }
    auto iter = mapping.begin();
    while (iter != mapping.end()) {
        out.write(iter->key.toUtf8());
        out.write(":");
        out.write(iter->value.toUtf8());
        out.write("\n");
        iter++;
    }
    return out.commit();
}
}  // namespace

GameOptions::GameOptions(const QString& path) : path(path)
{
    reload();
}

QVariant GameOptions::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QAbstractListModel::headerData(section, orientation, role);
    }
    switch (section) {
        case 0:
            return tr("Key");
        case 1:
            return tr("Value");
        default:
            return QVariant();
    }
}

QVariant GameOptions::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int column = index.column();

    if (row < 0 || row >= int(contents.size()))
        return QVariant();

    if (role == Qt::DisplayRole) {
        if (column == 0)
            return contents[row].key;
        return contents[row].value;
    }
    return QVariant();
}

int GameOptions::rowCount(const QModelIndex&) const
{
    return static_cast<int>(contents.size());
}

int GameOptions::columnCount(const QModelIndex&) const
{
    return 2;
}

bool GameOptions::isLoaded() const
{
    return loaded;
}

bool GameOptions::reload()
{
    beginResetModel();
    loaded = load(path, contents, version);
    endResetModel();
    return loaded;
}

bool GameOptions::save()
{
    return ::save(path, contents, version);
}
