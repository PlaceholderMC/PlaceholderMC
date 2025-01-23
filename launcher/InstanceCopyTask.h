#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QUrl>
#include "BaseInstance.h"
#include "BaseVersion.h"
#include "InstanceCopyPrefs.h"
#include "InstanceTask.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"

class InstanceCopyTask : public InstanceTask {
    Q_OBJECT
   public:
    explicit InstanceCopyTask(InstancePtr origInstance, const InstanceCopyPrefs& prefs);

   protected:
    //! Entry point for tasks.
    virtual void executeTask() override;
    bool abort() override;
    void copyFinished();
    void copyAborted();

   private:
    /* data */
    InstancePtr m_origInstance;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
    IPathMatcher::Ptr m_matcher;
    bool m_keepPlaytime;
    bool m_useLinks = false;
    bool m_useHardLinks = false;
    bool m_copySaves = false;
    bool m_linkRecursively = false;
    bool m_useClone = false;
};
