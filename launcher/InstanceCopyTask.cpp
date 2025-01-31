#include "InstanceCopyTask.h"
#include <QDebug>
#include <QtConcurrentRun>
#include <memory>
#include "FileSystem.h"
#include "NullInstance.h"
#include "pathmatcher/RegexpMatcher.h"
#include "settings/INISettingsObject.h"
#include "tasks/Task.h"

InstanceCopyTask::InstanceCopyTask(InstancePtr origInstance, const InstanceCopyPrefs& prefs)
{
    m_origInstance = origInstance;
    m_keepPlaytime = prefs.isKeepPlaytimeEnabled();
    m_useLinks = prefs.isUseSymLinksEnabled();
    m_linkRecursively = prefs.isLinkRecursivelyEnabled();
    m_useHardLinks = prefs.isLinkRecursivelyEnabled() && prefs.isUseHardLinksEnabled();
    m_copySaves = prefs.isLinkRecursivelyEnabled() && prefs.isDontLinkSavesEnabled() && prefs.isCopySavesEnabled();
    m_useClone = prefs.isUseCloneEnabled();

    QString filters = prefs.getSelectedFiltersAsRegex();
    if (m_useLinks || m_useHardLinks) {
        if (!filters.isEmpty())
            filters += "|";
        filters += "instance.cfg";
    }

    qDebug() << "CopyFilters:" << filters;

    if (!filters.isEmpty()) {
        // Set regex filter:
        // FIXME: get this from the original instance type...
        auto matcherReal = new RegexpMatcher(filters);
        matcherReal->caseSensitive(false);
        m_matcher.reset(matcherReal);
    }
}

void InstanceCopyTask::executeTask()
{
    setStatus(tr("Copying instance %1").arg(m_origInstance->name()));

    m_copyFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
        if (m_useClone) {
            FS::clone folderClone(m_origInstance->instanceRoot(), m_stagingPath);
            folderClone.matcher(m_matcher);

            folderClone(true);
            setProgress(0, folderClone.totalCloned());
            connect(&folderClone, &FS::clone::fileCloned,
                    [this](QString src, QString dst) { setProgress(m_progress + 1, m_progressTotal); });
            return folderClone();
        }
        if (m_useLinks || m_useHardLinks) {
            std::unique_ptr<FS::copy> savesCopy;
            if (m_copySaves) {
                QFileInfo mcDir(FS::PathCombine(m_stagingPath, "minecraft"));
                QFileInfo dotMCDir(FS::PathCombine(m_stagingPath, ".minecraft"));

                QString staging_mc_dir;
                if (dotMCDir.exists() && !mcDir.exists())
                    staging_mc_dir = dotMCDir.filePath();
                else
                    staging_mc_dir = mcDir.filePath();

                savesCopy = std::make_unique<FS::copy>(FS::PathCombine(m_origInstance->gameRoot(), "saves"),
                                                       FS::PathCombine(staging_mc_dir, "saves"));
                savesCopy->followSymlinks(true);
                (*savesCopy)(true);
                setProgress(0, savesCopy->totalCopied());
                connect(savesCopy.get(), &FS::copy::fileCopied, [this](QString src) { setProgress(m_progress + 1, m_progressTotal); });
            }
            FS::create_link folderLink(m_origInstance->instanceRoot(), m_stagingPath);
            int depth = m_linkRecursively ? -1 : 0;  // we need to at least link the top level instead of the instance folder
            folderLink.linkRecursively(true).setMaxDepth(depth).useHardLinks(m_useHardLinks).matcher(m_matcher);

            folderLink(true);
            setProgress(0, m_progressTotal + folderLink.totalToLink());
            connect(&folderLink, &FS::create_link::fileLinked,
                    [this](QString src, QString dst) { setProgress(m_progress + 1, m_progressTotal); });
            bool there_were_errors = false;

            if (!folderLink()) {
#if defined Q_OS_WIN32
                if (!m_useHardLinks) {
                    setProgress(0, m_progressTotal);
                    qDebug() << "EXPECTED: Link failure, Windows requires permissions for symlinks";

                    qDebug() << "attempting to run with privelage";

                    QEventLoop loop;
                    bool got_priv_results = false;

                    connect(&folderLink, &FS::create_link::finishedPrivileged, this, [&got_priv_results, &loop](bool gotResults) {
                        if (!gotResults) {
                            qDebug() << "Privileged run exited without results!";
                        }
                        got_priv_results = gotResults;
                        loop.quit();
                    });
                    folderLink.runPrivileged();

                    loop.exec();  // wait for the finished signal

                    for (auto result : folderLink.getResults()) {
                        if (result.err_value != 0) {
                            there_were_errors = true;
                        }
                    }

                    if (savesCopy) {
                        there_were_errors |= !(*savesCopy)();
                    }

                    return got_priv_results && !there_were_errors;
                }
#else
                qDebug() << "Link Failed!" << folderLink.getOSError().value() << folderLink.getOSError().message().c_str();
#endif
                return false;
            }

            if (savesCopy) {
                there_were_errors |= !(*savesCopy)();
            }

            return !there_were_errors;
        }
        FS::copy folderCopy(m_origInstance->instanceRoot(), m_stagingPath);
        folderCopy.followSymlinks(false).matcher(m_matcher);

        folderCopy(true);
        setProgress(0, folderCopy.totalCopied());
        connect(&folderCopy, &FS::copy::fileCopied, [this](QString src) { setProgress(m_progress + 1, m_progressTotal); });
        return folderCopy();
    });
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &InstanceCopyTask::copyFinished);
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &InstanceCopyTask::copyAborted);
    m_copyFutureWatcher.setFuture(m_copyFuture);
}

void InstanceCopyTask::copyFinished()
{
    auto successful = m_copyFuture.result();
    if (!successful) {
        emitFailed(tr("Instance folder copy failed."));
        return;
    }

    // FIXME: shouldn't this be able to report errors?
    auto instanceSettings = std::make_shared<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg"));

    InstancePtr inst(new NullInstance(m_globalSettings, instanceSettings, m_stagingPath));
    inst->setName(name());
    inst->setIconKey(m_instIcon);
    if (!m_keepPlaytime) {
        inst->resetTimePlayed();
    }
    if (m_useLinks) {
        inst->addLinkedInstanceId(m_origInstance->id());
        auto allowed_symlinks_file = QFileInfo(FS::PathCombine(inst->gameRoot(), "allowed_symlinks.txt"));

        QByteArray allowed_symlinks;
        if (allowed_symlinks_file.exists()) {
            allowed_symlinks.append(FS::read(allowed_symlinks_file.filePath()));
            if (allowed_symlinks.right(1) != "\n")
                allowed_symlinks.append("\n");  // we want to be on a new line
        }
        allowed_symlinks.append(m_origInstance->gameRoot().toUtf8());
        allowed_symlinks.append("\n");
        if (allowed_symlinks_file.isSymLink())
            FS::deletePath(
                allowed_symlinks_file
                    .filePath());  // we dont want to modify the original. also make sure the resulting file is not itself a link.

        try {
            FS::write(allowed_symlinks_file.filePath(), allowed_symlinks);
        } catch (const FS::FileSystemException& e) {
            qCritical() << "Failed to write symlink :" << e.cause();
        }
    }

    emitSucceeded();
}

void InstanceCopyTask::copyAborted()
{
    emitFailed(tr("Instance folder copy has been aborted."));
    return;
}

bool InstanceCopyTask::abort()
{
    if (m_copyFutureWatcher.isRunning()) {
        m_copyFutureWatcher.cancel();
        // NOTE: Here we don't do `emitAborted()` because it will be done when `m_copyFutureWatcher` actually cancels, which may not occur
        // immediately.
        return true;
    }
    return false;
}