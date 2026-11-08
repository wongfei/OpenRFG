// Original: library/modules/runtime/task/include/keen/task/taskqueue.hpp
// Implementation: library/modules/runtime/task/sources/global/taskqueue.cpp
//
// TaskQueue, a queue of tasks executed by the task system workers (parameters, sync tasks, push/wait/clear).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/queue.hpp"
#include "keen/base/win32/event_win32.hpp"
#include "keen/base/win32/mutex_win32.hpp"
#include "keen/task/tasktypes.hpp"

namespace keen
{
    class MemoryAllocator;
    struct TaskSystem;

    // size 0x10
    struct TaskQueueParameters
    {
        TaskQueueParameters() // 0x00C5CB30 (.h:15)
        {
            // Magic numbers: 64 byte tasks, 1024 of them, normal priority, 16 byte alignment.
            maxTaskSize = 0x40;
            maxTaskCount = 0x400;
            priority = 1;
            alignment = 0x10;
        }

        uint32 maxTaskSize; // 0x00
        uint32 maxTaskCount; // 0x04
        uint32 priority; // 0x08
        uint32 alignment; // 0x0C
    };

    // size 0x4
    struct QueueSyncTask : public Task
    {
        QueueSyncTask(); // 0x00C7F7B0 (:44)
        static void doNothing(const TaskContext* pContext); // 0x00C7F400 (:49)

    };

    // size 0x6C
    struct TaskQueue
    {
        TaskQueue(); // 0x00C7FB70
        ~TaskQueue(); // 0x00C7FAE0

        TaskSystem* pTaskSystem; // 0x00
        uint32 clientThreadId; // 0x04
        uint8* pCopyBuffer; // 0x08
        uint32 copyBufferSlotIndex; // 0x0C
        Queue<Task*> queue; // 0x10
        Mutex queueMutex; // 0x24
        uint32 pendingTaskCount; // 0x5C
        uint32 inProgressTaskCount; // 0x60
        Event taskCompleteEvent; // 0x64
        uint32 maxTaskSize; // 0x68
    };

    Task* getNextCopyBufferSlot(TaskQueue* pTaskQueue); // 0x00C7FB50 (:63)

    namespace task
    {
        TaskQueue* createTaskQueue(MemoryAllocator* pAllocator, TaskSystem* pTaskSystem, const TaskQueueParameters& parameters); // 0x00C80340 (:71)
        void destroyTaskQueue(MemoryAllocator* pAllocator, TaskQueue* pTaskQueue); // 0x00C80470 (:104)
        void clearQueuedTasks(TaskQueue* pTaskQueue); // 0x00C7F7C0 (:128)
        void waitForTaskQueue(TaskQueue* pTaskQueue); // 0x00C80040 (:142)
        bool isTaskQueueEmpty(TaskQueue* pTaskQueue); // 0x00C7F410 (:164)
        bool pushSyncTaskToQueue(TaskQueue* pTaskQueue); // 0x00C7F420 (:173)
        bool pushTaskToQueue(TaskQueue* pTaskQueue, const Task* pTask, uint32 taskSize); // 0x00C7FBA0 (:213)
        const Task* popTaskFromQueue(TaskQueue* pTaskQueue); // 0x00C7FC50 (:252)
        void executePoppedTask(TaskQueue* pTaskQueue, const TaskContext& taskContext); // 0x00C7F430 (:278)
        void clearTaskQueueTaskSystem(TaskQueue* pTaskQueue); // 0x00C7F460 (:300)
    }
}
