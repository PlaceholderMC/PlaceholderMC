#include "MinecraftLoadAndCheck.h"
#include "MinecraftInstance.h"
#include "PackProfile.h"

MinecraftLoadAndCheck::MinecraftLoadAndCheck(MinecraftInstance* inst, Net::Mode netmode) : m_inst(inst), m_netmode(netmode) {}

void MinecraftLoadAndCheck::executeTask()
{
    // add offline metadata load task
    auto components = m_inst->getPackProfile();
    if (auto result = components->reload(m_netmode); !result) {
        emitFailed(result.error);
        return;
    }
    m_task = components->getCurrentTask();

    if (!m_task) {
        emitSucceeded();
        return;
    }
    connect(m_task.get(), &Task::succeeded, this, &MinecraftLoadAndCheck::emitSucceeded);
    connect(m_task.get(), &Task::failed, this, &MinecraftLoadAndCheck::emitFailed);
    connect(m_task.get(), &Task::aborted, this, [this] { emitFailed(tr("Aborted")); });
    connect(m_task.get(), &Task::progress, this, &MinecraftLoadAndCheck::setProgress);
    connect(m_task.get(), &Task::stepProgress, this, &MinecraftLoadAndCheck::propagateStepProgress);
    connect(m_task.get(), &Task::status, this, &MinecraftLoadAndCheck::setStatus);
    connect(m_task.get(), &Task::details, this, &MinecraftLoadAndCheck::setDetails);
}

bool MinecraftLoadAndCheck::canAbort() const
{
    if (m_task) {
        return m_task->canAbort();
    }
    return true;
}

bool MinecraftLoadAndCheck::abort()
{
    if (m_task && m_task->canAbort()) {
        auto status = m_task->abort();
        emitFailed("Aborted.");
        return status;
    }
    return Task::abort();
}