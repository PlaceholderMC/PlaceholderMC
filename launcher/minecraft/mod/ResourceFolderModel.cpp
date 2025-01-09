#include "ResourceFolderModel.h"
#include <QMessageBox>

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QStyle>
#include <QThreadPool>
#include <QUrl>
#include <utility>

#include "Application.h"
#include "FileSystem.h"

#include "minecraft/mod/tasks/ResourceFolderLoadTask.h"

#include "Json.h"
#include "minecraft/mod/tasks/LocalResourceUpdateTask.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"
#include "settings/Setting.h"
#include "tasks/Task.h"
#include "ui/dialogs/CustomMessageBox.h"

ResourceFolderModel::ResourceFolderModel(const QDir& dir, BaseInstance* instance, bool is_indexed, bool create_dir, QObject* parent)
    : QAbstractListModel(parent), m_dir(dir), m_instance(instance), m_watcher(this), m_is_indexed(is_indexed)
{
    if (create_dir) {
        FS::ensureFolderPathExists(m_dir.absolutePath());
    }

    m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    m_dir.setSorting(QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ResourceFolderModel::directoryChanged);
    connect(&m_helper_thread_task, &ConcurrentTask::finished, this, [this] { m_helper_thread_task.clear(); });
    if (APPLICATION_DYN) {  // in tests the application macro doesn't work
        m_helper_thread_task.setMaxConcurrent(APPLICATION->settings()->get("NumberOfConcurrentTasks").toInt());
    }
}

ResourceFolderModel::~ResourceFolderModel()
{
    while (!QThreadPool::globalInstance()->waitForDone(100))
        QCoreApplication::processEvents();
}

bool ResourceFolderModel::startWatching(const QStringList& paths)
{
    // Remove orphaned metadata next time
    m_first_folder_load = true;

    if (m_is_watching)
        return false;

    auto couldnt_be_watched = m_watcher.addPaths(paths);
    for (auto path : paths) {
        if (couldnt_be_watched.contains(path))
            qDebug() << "Failed to start watching " << path;
        else
            qDebug() << "Started watching " << path;
    }

    update();

    m_is_watching = !m_is_watching;
    return m_is_watching;
}

bool ResourceFolderModel::stopWatching(const QStringList& paths)
{
    if (!m_is_watching)
        return false;

    auto couldnt_be_stopped = m_watcher.removePaths(paths);
    for (auto path : paths) {
        if (couldnt_be_stopped.contains(path))
            qDebug() << "Failed to stop watching " << path;
        else
            qDebug() << "Stopped watching " << path;
    }

    m_is_watching = !m_is_watching;
    return !m_is_watching;
}

bool ResourceFolderModel::installResource(QString original_path)
{
    // NOTE: fix for GH-1178: remove trailing slash to avoid issues with using the empty result of QFileInfo::fileName
    original_path = FS::NormalizePath(original_path);
    QFileInfo file_info(original_path);

    if (!file_info.exists() || !file_info.isReadable()) {
        qWarning() << "Caught attempt to install non-existing file or file-like object:" << original_path;
        return false;
    }
    qDebug() << "Installing: " << file_info.absoluteFilePath();

    Resource resource(file_info);
    if (!resource.valid()) {
        qWarning() << original_path << "is not a valid resource. Ignoring it.";
        return false;
    }

    auto new_path = FS::NormalizePath(m_dir.filePath(file_info.fileName()));
    if (original_path == new_path) {
        qWarning() << "Overwriting the mod (" << original_path << ") with itself makes no sense...";
        return false;
    }

    switch (resource.type()) {
        case ResourceType::SINGLEFILE:
        case ResourceType::ZIPFILE:
        case ResourceType::LITEMOD: {
            if (QFile::exists(new_path) || QFile::exists(new_path + QString(".disabled"))) {
                if (!FS::deletePath(new_path)) {
                    qCritical() << "Cleaning up new location (" << new_path << ") was unsuccessful!";
                    return false;
                }
                qDebug() << new_path << "has been deleted.";
            }

            if (!QFile::copy(original_path, new_path)) {
                qCritical() << "Copy from" << original_path << "to" << new_path << "has failed.";
                return false;
            }

            FS::updateTimestamp(new_path);

            QFileInfo new_path_file_info(new_path);
            resource.setFile(new_path_file_info);

            if (!m_is_watching)
                return update();

            return true;
        }
        case ResourceType::FOLDER: {
            if (QFile::exists(new_path)) {
                qDebug() << "Ignoring folder '" << original_path << "', it would merge with" << new_path;
                return false;
            }

            if (!FS::copy(original_path, new_path)()) {
                qWarning() << "Copy of folder from" << original_path << "to" << new_path << "has (potentially partially) failed.";
                return false;
            }

            QFileInfo newpathInfo(new_path);
            resource.setFile(newpathInfo);

            if (!m_is_watching)
                return update();

            return true;
        }
        default:
            break;
    }
    return false;
}

bool ResourceFolderModel::installResourceWithFlameMetadata(QString path, ModPlatform::IndexedVersion& vers)
{
    if (vers.addonId.isValid()) {
        ModPlatform::IndexedPack pack{
            vers.addonId,
            ModPlatform::ResourceProvider::FLAME,
        };

        QEventLoop loop;

        auto response = std::make_shared<QByteArray>();
        auto job = FlameAPI().getProject(vers.addonId.toString(), response);

        QObject::connect(job.get(), &Task::failed, [&loop] { loop.quit(); });
        QObject::connect(job.get(), &Task::aborted, &loop, &QEventLoop::quit);
        QObject::connect(job.get(), &Task::succeeded, [response, this, &vers, &loop, &pack] {
            QJsonParseError parse_error{};
            QJsonDocument doc = QJsonDocument::fromJson(*response, &parse_error);
            if (parse_error.error != QJsonParseError::NoError) {
                qWarning() << "Error while parsing JSON response for mod info at " << parse_error.offset
                           << " reason: " << parse_error.errorString();
                qDebug() << *response;
                return;
            }
            try {
                auto obj = Json::requireObject(Json::requireObject(doc), "data");
                FlameMod::loadIndexedPack(pack, obj);
            } catch (const JSONValidationError& e) {
                qDebug() << doc;
                qWarning() << "Error while reading mod info: " << e.cause();
            }
            LocalResourceUpdateTask update_metadata(indexDir(), pack, vers);
            QObject::connect(&update_metadata, &Task::finished, &loop, &QEventLoop::quit);
            update_metadata.start();
        });

        job->start();

        loop.exec();
    }

    return installResource(std::move(path));
}

bool ResourceFolderModel::uninstallResource(QString file_name, bool preserve_metadata)
{
    for (auto& resource : m_resources) {
        if (resource->fileinfo().fileName() == file_name) {
            auto res = resource->destroy(indexDir(), preserve_metadata, false);

            update();

            return res;
        }
    }
    return false;
}

bool ResourceFolderModel::deleteResources(const QModelIndexList& indexes)
{
    if (indexes.isEmpty())
        return true;

    for (auto i : indexes) {
        if (i.column() != 0)
            continue;

        auto& resource = m_resources.at(i.row());
        resource->destroy(indexDir());
    }

    update();

    return true;
}

void ResourceFolderModel::deleteMetadata(const QModelIndexList& indexes)
{
    if (indexes.isEmpty())
        return;

    for (auto i : indexes) {
        if (i.column() != 0)
            continue;

        auto& resource = m_resources.at(i.row());
        resource->destroyMetadata(indexDir());
    }

    update();
}

bool ResourceFolderModel::setResourceEnabled(const QModelIndexList& indexes, EnableAction action)
{
    if (indexes.isEmpty())
        return true;

    bool succeeded = true;
    for (auto const& idx : indexes) {
        if (!validateIndex(idx) || idx.column() != 0)
            continue;

        int row = idx.row();

        auto& resource = m_resources[row];

        // Preserve the row, but change its ID
        auto old_id = resource->internal_id();
        if (!resource->enable(action)) {
            succeeded = false;
            continue;
        }

        auto new_id = resource->internal_id();

        m_resources_index.remove(old_id);
        m_resources_index[new_id] = row;

        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex()) - 1));
    }

    return succeeded;
}

static QMutex s_update_task_mutex;
bool ResourceFolderModel::update()
{
    // We hold a lock here to prevent race conditions on the m_current_update_task reset.
    QMutexLocker lock(&s_update_task_mutex);

    // Already updating, so we schedule a future update and return.
    if (m_current_update_task) {
        m_scheduled_update = true;
        return false;
    }

    m_current_update_task.reset(createUpdateTask());
    if (!m_current_update_task)
        return false;

    connect(m_current_update_task.get(), &Task::succeeded, this, &ResourceFolderModel::onUpdateSucceeded,
            Qt::ConnectionType::QueuedConnection);
    connect(m_current_update_task.get(), &Task::failed, this, &ResourceFolderModel::onUpdateFailed, Qt::ConnectionType::QueuedConnection);
    connect(
        m_current_update_task.get(), &Task::finished, this,
        [this] {
            m_current_update_task.reset();
            if (m_scheduled_update) {
                m_scheduled_update = false;
                update();
            } else {
                emit updateFinished();
            }
        },
        Qt::ConnectionType::QueuedConnection);

    QThreadPool::globalInstance()->start(m_current_update_task.get());

    return true;
}

void ResourceFolderModel::resolveResource(Resource::Ptr res)
{
    if (!res->shouldResolve()) {
        return;
    }

    Task::Ptr task{ createParseTask(*res) };
    if (!task)
        return;

    int ticket = m_next_resolution_ticket.fetch_add(1);

    res->setResolving(true, ticket);
    m_active_parse_tasks.insert(ticket, task);

    connect(
        task.get(), &Task::succeeded, this, [this, ticket, res] { onParseSucceeded(ticket, res->internal_id()); },
        Qt::ConnectionType::QueuedConnection);
    connect(
        task.get(), &Task::failed, this, [this, ticket, res] { onParseFailed(ticket, res->internal_id()); },
        Qt::ConnectionType::QueuedConnection);
    connect(
        task.get(), &Task::finished, this,
        [this, ticket] {
            m_active_parse_tasks.remove(ticket);
            emit parseFinished();
        },
        Qt::ConnectionType::QueuedConnection);

    m_helper_thread_task.addTask(task);

    if (!m_helper_thread_task.isRunning()) {
        QThreadPool::globalInstance()->start(&m_helper_thread_task);
    }
}

void ResourceFolderModel::onUpdateSucceeded()
{
    auto update_results = static_cast<ResourceFolderLoadTask*>(m_current_update_task.get())->result();

    auto& new_resources = update_results->resources;

    auto current_list = m_resources_index.keys();
    QSet<QString> current_set(current_list.begin(), current_list.end());

    auto new_list = new_resources.keys();
    QSet<QString> new_set(new_list.begin(), new_list.end());

    applyUpdates(current_set, new_set, new_resources);
}

void ResourceFolderModel::onParseSucceeded(int ticket, QString resource_id)
{
    auto iter = m_active_parse_tasks.constFind(ticket);
    if (iter == m_active_parse_tasks.constEnd() || !m_resources_index.contains(resource_id))
        return;

    int row = m_resources_index[resource_id];
    emit dataChanged(index(row), index(row, columnCount(QModelIndex()) - 1));
}

Task* ResourceFolderModel::createUpdateTask()
{
    auto index_dir = indexDir();
    auto task = new ResourceFolderLoadTask(dir(), index_dir, m_is_indexed, m_first_folder_load,
                                           [this](const QFileInfo& file) { return createResource(file); });
    m_first_folder_load = false;
    return task;
}

bool ResourceFolderModel::hasPendingParseTasks() const
{
    return !m_active_parse_tasks.isEmpty();
}

void ResourceFolderModel::directoryChanged(QString path)
{
    update();
}

Qt::DropActions ResourceFolderModel::supportedDropActions() const
{
    // copy from outside, move from within and other resource lists
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags ResourceFolderModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    auto flags = defaultFlags | Qt::ItemIsDropEnabled;
    if (index.isValid())
        flags |= Qt::ItemIsUserCheckable;
    return flags;
}

QStringList ResourceFolderModel::mimeTypes() const
{
    QStringList types;
    types << "text/uri-list";
    return types;
}

bool ResourceFolderModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int, int, const QModelIndex&)
{
    if (action == Qt::IgnoreAction) {
        return true;
    }

    // check if the action is supported
    if (!data || !(action & supportedDropActions())) {
        return false;
    }

    // files dropped from outside?
    if (data->hasUrls()) {
        auto urls = data->urls();
        for (auto url : urls) {
            // only local files may be dropped...
            if (!url.isLocalFile()) {
                continue;
            }
            // TODO: implement not only copy, but also move
            // FIXME: handle errors here
            installResource(url.toLocalFile());
        }
        return true;
    }
    return false;
}

bool ResourceFolderModel::validateIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return false;

    int row = index.row();
    if (row < 0 || row >= m_resources.size())
        return false;

    return true;
}

QVariant ResourceFolderModel::data(const QModelIndex& index, int role) const
{
    if (!validateIndex(index))
        return {};

    int row = index.row();
    int column = index.column();

    switch (role) {
        case Qt::DisplayRole:
            switch (column) {
                case NameColumn:
                    return m_resources[row]->name();
                case DateColumn:
                    return m_resources[row]->dateTimeChanged();
                case ProviderColumn:
                    return m_resources[row]->provider();
                case SizeColumn:
                    return m_resources[row]->sizeStr();
                default:
                    return {};
            }
        case Qt::ToolTipRole:
            if (column == NameColumn) {
                if (at(row).isSymLinkUnder(instDirPath())) {
                    return m_resources[row]->internal_id() +
                           tr("\nWarning: This resource is symbolically linked from elsewhere. Editing it will also change the original."
                              "\nCanonical Path: %1")
                               .arg(at(row).fileinfo().canonicalFilePath());
                    ;
                }
                if (at(row).isMoreThanOneHardLink()) {
                    return m_resources[row]->internal_id() +
                           tr("\nWarning: This resource is hard linked elsewhere. Editing it will also change the original.");
                }
            }

            return m_resources[row]->internal_id();
        case Qt::DecorationRole: {
            if (column == NameColumn && (at(row).isSymLinkUnder(instDirPath()) || at(row).isMoreThanOneHardLink()))
                return APPLICATION->getThemedIcon("status-yellow");

            return {};
        }
        case Qt::CheckStateRole:
            switch (column) {
                case ActiveColumn:
                    return m_resources[row]->enabled() ? Qt::Checked : Qt::Unchecked;
                default:
                    return {};
            }
        default:
            return {};
    }
}

bool ResourceFolderModel::setData(const QModelIndex& index, [[maybe_unused]] const QVariant& value, int role)
{
    int row = index.row();
    if (row < 0 || row >= rowCount(index.parent()) || !index.isValid())
        return false;

    if (role == Qt::CheckStateRole) {
        if (m_instance != nullptr && m_instance->isRunning()) {
            auto response =
                CustomMessageBox::selectable(nullptr, tr("Confirm toggle"),
                                             tr("If you enable/disable this resource while the game is running it may crash your game.\n"
                                                "Are you sure you want to do this?"),
                                             QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                    ->exec();

            if (response != QMessageBox::Yes)
                return false;
        }
        return setResourceEnabled({ index }, EnableAction::TOGGLE);
    }

    return false;
}

QVariant ResourceFolderModel::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const
{
    switch (role) {
        case Qt::DisplayRole:
            switch (section) {
                case ActiveColumn:
                case NameColumn:
                case DateColumn:
                case ProviderColumn:
                case SizeColumn:
                    return columnNames().at(section);
                default:
                    return {};
            }
        case Qt::ToolTipRole: {
            //: Here, resource is a generic term for external resources, like Mods, Resource Packs, Shader Packs, etc.
            switch (section) {
                case ActiveColumn:
                    return tr("Is the resource enabled?");
                case NameColumn:
                    return tr("The name of the resource.");
                case DateColumn:
                    return tr("The date and time this resource was last changed (or added).");
                case ProviderColumn:
                    return tr("The source provider of the resource.");
                case SizeColumn:
                    return tr("The size of the resource.");
                default:
                    return {};
            }
        }
        default:
            break;
    }

    return {};
}

void ResourceFolderModel::setupHeaderAction(QAction* act, int column)
{
    Q_ASSERT(act);

    act->setText(columnNames().at(column));
}

void ResourceFolderModel::saveColumns(QTreeView* tree)
{
    auto const setting_name = QString("UI/%1_Page/Columns").arg(id());
    auto setting = (m_instance->settings()->contains(setting_name)) ? m_instance->settings()->getSetting(setting_name)
                                                                    : m_instance->settings()->registerSetting(setting_name);

    setting->set(tree->header()->saveState());
}

void ResourceFolderModel::loadColumns(QTreeView* tree)
{
    for (auto i = 0; i < m_columnsHiddenByDefault.size(); ++i) {
        tree->setColumnHidden(i, m_columnsHiddenByDefault[i]);
    }

    auto const setting_name = QString("UI/%1_Page/Columns").arg(id());
    auto setting = (m_instance->settings()->contains(setting_name)) ? m_instance->settings()->getSetting(setting_name)
                                                                    : m_instance->settings()->registerSetting(setting_name);

    tree->header()->restoreState(setting->get().toByteArray());
}

QMenu* ResourceFolderModel::createHeaderContextMenu(QTreeView* tree)
{
    auto menu = new QMenu(tree);

    menu->addSeparator()->setText(tr("Show / Hide Columns"));

    for (int col = 0; col < columnCount(); ++col) {
        // Skip creating actions for columns that should not be hidden
        if (!m_columnsHideable.at(col))
            continue;
        auto act = new QAction(menu);
        setupHeaderAction(act, col);

        act->setCheckable(true);
        act->setChecked(!tree->isColumnHidden(col));

        connect(act, &QAction::toggled, tree, [this, col, tree](bool toggled) {
            tree->setColumnHidden(col, !toggled);
            for (int c = 0; c < columnCount(); ++c) {
                if (m_column_resize_modes.at(c) == QHeaderView::ResizeToContents)
                    tree->resizeColumnToContents(c);
            }
            saveColumns(tree);
        });

        menu->addAction(act);
    }

    return menu;
}

QSortFilterProxyModel* ResourceFolderModel::createFilterProxyModel(QObject* parent)
{
    return new ProxyModel(parent);
}

SortType ResourceFolderModel::columnToSortKey(size_t column) const
{
    Q_ASSERT(m_column_sort_keys.size() == columnCount());
    return m_column_sort_keys.at(column);
}

/* Standard Proxy Model for createFilterProxyModel */
[[nodiscard]] bool ResourceFolderModel::ProxyModel::filterAcceptsRow(int source_row,
                                                                     [[maybe_unused]] const QModelIndex& source_parent) const
{
    auto* model = qobject_cast<ResourceFolderModel*>(sourceModel());
    if (!model)
        return true;

    const auto& resource = model->at(source_row);

    return resource.applyFilter(filterRegularExpression());
}

[[nodiscard]] bool ResourceFolderModel::ProxyModel::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    auto* model = qobject_cast<ResourceFolderModel*>(sourceModel());
    if (!model || !source_left.isValid() || !source_right.isValid() || source_left.column() != source_right.column()) {
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }

    // we are now guaranteed to have two valid indexes in the same column... we love the provided invariants unconditionally and
    // proceed.

    auto column_sort_key = model->columnToSortKey(source_left.column());
    auto const& resource_left = model->at(source_left.row());
    auto const& resource_right = model->at(source_right.row());

    auto compare_result = resource_left.compare(resource_right, column_sort_key);
    if (compare_result == 0)
        return QSortFilterProxyModel::lessThan(source_left, source_right);

    return compare_result < 0;
}

QString ResourceFolderModel::instDirPath() const
{
    return QFileInfo(m_instance->instanceRoot()).absoluteFilePath();
}

void ResourceFolderModel::onParseFailed(int ticket, QString resource_id)
{
    auto iter = m_active_parse_tasks.constFind(ticket);
    if (iter == m_active_parse_tasks.constEnd() || !m_resources_index.contains(resource_id))
        return;

    auto removed_index = m_resources_index[resource_id];
    auto removed_it = m_resources.begin() + removed_index;
    Q_ASSERT(removed_it != m_resources.end());

    beginRemoveRows(QModelIndex(), removed_index, removed_index);
    m_resources.erase(removed_it);

    // update index
    m_resources_index.clear();
    int idx = 0;
    for (auto const& mod : qAsConst(m_resources)) {
        m_resources_index[mod->internal_id()] = idx;
        idx++;
    }
    endRemoveRows();
}

void ResourceFolderModel::applyUpdates(QSet<QString>& current_set, QSet<QString>& new_set, QMap<QString, Resource::Ptr>& new_resources)
{
    // see if the kept resources changed in some way
    {
        QSet<QString> kept_set = current_set;
        kept_set.intersect(new_set);

        for (auto const& kept : kept_set) {
            auto row_it = m_resources_index.constFind(kept);
            Q_ASSERT(row_it != m_resources_index.constEnd());
            auto row = row_it.value();

            auto& new_resource = new_resources[kept];
            auto const& current_resource = m_resources.at(row);

            if (new_resource->dateTimeChanged() == current_resource->dateTimeChanged()) {
                // no significant change, ignore...
                continue;
            }

            // If the resource is resolving, but something about it changed, we don't want to
            // continue the resolving.
            if (current_resource->isResolving()) {
                auto ticket = current_resource->resolutionTicket();
                if (m_active_parse_tasks.contains(ticket)) {
                    auto task = (*m_active_parse_tasks.find(ticket)).get();
                    task->abort();
                }
            }

            m_resources[row].reset(new_resource);
            resolveResource(m_resources.at(row));
            emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex()) - 1));
        }
    }

    // remove resources no longer present
    {
        QSet<QString> removed_set = current_set;
        removed_set.subtract(new_set);

        QList<int> removed_rows;
        for (auto& removed : removed_set)
            removed_rows.append(m_resources_index[removed]);

        std::sort(removed_rows.begin(), removed_rows.end(), std::greater<int>());

        for (auto& removed_index : removed_rows) {
            auto removed_it = m_resources.begin() + removed_index;

            Q_ASSERT(removed_it != m_resources.end());

            if ((*removed_it)->isResolving()) {
                auto ticket = (*removed_it)->resolutionTicket();
                if (m_active_parse_tasks.contains(ticket)) {
                    auto task = (*m_active_parse_tasks.find(ticket)).get();
                    task->abort();
                }
            }

            beginRemoveRows(QModelIndex(), removed_index, removed_index);
            m_resources.erase(removed_it);
            endRemoveRows();
        }
    }

    // add new resources to the end
    {
        QSet<QString> added_set = new_set;
        added_set.subtract(current_set);

        // When you have a Qt build with assertions turned on, proceeding here will abort the application
        if (added_set.size() > 0) {
            beginInsertRows(QModelIndex(), static_cast<int>(m_resources.size()),
                            static_cast<int>(m_resources.size() + added_set.size() - 1));

            for (auto& added : added_set) {
                auto res = new_resources[added];
                m_resources.append(res);
                resolveResource(m_resources.last());
            }

            endInsertRows();
        }
    }

    // update index
    {
        m_resources_index.clear();
        int idx = 0;
        for (auto const& mod : qAsConst(m_resources)) {
            m_resources_index[mod->internal_id()] = idx;
            idx++;
        }
    }
}
Resource::Ptr ResourceFolderModel::find(QString id)
{
    auto iter =
        std::find_if(m_resources.constBegin(), m_resources.constEnd(), [&](Resource::Ptr const& r) { return r->internal_id() == id; });
    if (iter == m_resources.constEnd())
        return nullptr;
    return *iter;
}
QList<Resource*> ResourceFolderModel::allResources()
{
    QList<Resource*> result;
    result.reserve(m_resources.size());
    for (const Resource ::Ptr& resource : m_resources)
        result.append((resource.get()));
    return result;
}
QList<Resource*> ResourceFolderModel::selectedResources(const QModelIndexList& indexes)
{
    QList<Resource*> result;
    for (const QModelIndex& index : indexes) {
        if (index.column() != 0)
            continue;
        result.append(&at(index.row()));
    }
    return result;
}
