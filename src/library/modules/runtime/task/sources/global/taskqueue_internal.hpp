// Original: library/modules/runtime/task/sources/global/taskqueue_internal.hpp
//
// TaskQueueContext, the per-queue execution state. Placement inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct QueueSyncTask;
    struct TaskQueue;

    // The sync task marker: a queue whose next task is this one waits until its tasks in progress are done. Nothing
    // pushes it (pushSyncTaskToQueue is not implemented in rfg.exe). Defined in taskqueue.cpp.
    extern QueueSyncTask s_queueSyncTask; // 0x016F0828

    // size 0x8
    struct TaskQueueContext
    {
        TaskQueue* pQueue; // 0x00
        uint32 priority; // 0x04
    };
}
