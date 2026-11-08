// Original: library/modules/runtime/task/sources/global/tasksystem_internal.hpp
//
// TaskSystem and the worker thread contexts. Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/base/win32/mutex_win32.hpp"
#include "keen/base/win32/semaphore_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"
#include "library/modules/runtime/task/sources/global/taskqueue_internal.hpp"

namespace keen
{
    struct TaskSystem;

    // size 0x5C
    struct WorkerThreadContext
    {
        WorkerThreadContext(); // 0x00C7F790
        ~WorkerThreadContext(); // 0x00C7F7A0

        Thread thread; // 0x00
        uint32 threadIndex; // 0x54
        TaskSystem* pTaskSystem; // 0x58
    };

    // size 0x58
    struct TaskSystem
    {
        TaskSystem(); // 0x00C7F810
        ~TaskSystem(); // 0x00C7F730

        uint32 clientThreadId; // 0x00
        Array<WorkerThreadContext> workerThreadContexts; // 0x04
        SizedArray<TaskQueueContext> taskQueueContexts; // 0x0C
        Mutex queueArrayMutex; // 0x18
        Semaphore wakeUpSemaphore; // 0x50
    };
}
