#include "pch.h"

// 全局资源，用于时间片轮转，指示时间片状态
volatile uint64_t render_status = 1;
SRWLOCK render_status_lock; // 用于保护RenderStatus的读写
CONDITION_VARIABLE render_status_cond; // 用于等待RenderStatus变为1

void initRenderStatus()
{
    InitializeSRWLock(&render_status_lock);
    InitializeConditionVariable(&render_status_cond);
}

void setRenderStatus(int flag)
{
    AcquireSRWLockExclusive(&render_status_lock);
    render_status = flag;
    ReleaseSRWLockExclusive(&render_status_lock);
}
int getRenderStatus()
{
    AcquireSRWLockShared(&render_status_lock);
    int flag = render_status;
    ReleaseSRWLockShared(&render_status_lock);
    return flag;
}
void waitRenderStatus(int flag)
{
    AcquireSRWLockShared(&render_status_lock);
    while (render_status != flag)
    {
        SleepConditionVariableSRW(&render_status_cond, &render_status_lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
    }
    ReleaseSRWLockShared(&render_status_lock);
}
void wakeRender()
{
    AcquireSRWLockExclusive(&render_status_lock);
    render_status = 1;
    WakeAllConditionVariable(&render_status_cond);
    ReleaseSRWLockExclusive(&render_status_lock);
}