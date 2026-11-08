// Original: library/modules/runtime/task/sources/global/taskqueue.cpp
//
// A task queue: pushed tasks are copied into a ring of fixed-size slots and queued by pointer; the task system's
// workers (or waitForTaskQueue on the waiting thread) take them in order. pendingTaskCount counts the tasks not yet
// finished, inProgressTaskCount the ones being run.
//
// Notes (decompiled behaviour, kept as is):
// - pushSyncTaskToQueue only prints that it is not implemented.
// - The copy slot ring has as many slots as the queue, so a task's copy stays valid until maxTaskCount further pushes.

#include "keen/task/taskqueue.hpp"
#include "keen/task/tasksystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/task/sources/global/tasksystem_internal.hpp"

#include <new>
#include <string.h>
#include <windows.h>

namespace keen
{
    QueueSyncTask s_queueSyncTask; // 0x016F0828

    // 0x00C7F3E0 (.h:18)
    TaskContext::TaskContext(const Task* pInTask, uint32 inThreadIndex)
        : pTask(pInTask), threadIndex(inThreadIndex)
    {
    }

    // 0x00C7F7B0 (:44)
    QueueSyncTask::QueueSyncTask()
    {
        header.pTaskFunction = doNothing;
    }

    // 0x00C7F400 (:49)
    void QueueSyncTask::doNothing(const TaskContext* pContext)
    {
    }

    // 0x00C7FB70
    TaskQueue::TaskQueue()
    {
    }

    // 0x00C7FAE0
    TaskQueue::~TaskQueue()
    {
    }

    // 0x00C7FB50 (:63)
    Task* getNextCopyBufferSlot(TaskQueue* pTaskQueue)
    {
        pTaskQueue->copyBufferSlotIndex = (pTaskQueue->copyBufferSlotIndex + 1u) % pTaskQueue->queue.getCapacity();
        return (Task*)(pTaskQueue->pCopyBuffer + pTaskQueue->maxTaskSize * pTaskQueue->copyBufferSlotIndex);
    }

    namespace task
    {
        // 0x00C80340 (:71)
        TaskQueue* createTaskQueue(MemoryAllocator* pAllocator, TaskSystem* pTaskSystem, const TaskQueueParameters& parameters)
        {
            void* pMemory = pAllocator->allocate(sizeof(TaskQueue), 4u, 0u, nullptr);
            TaskQueue* pTaskQueue = pMemory != nullptr ? new (pMemory) TaskQueue() : nullptr;
            pTaskQueue->pTaskSystem = pTaskSystem;
            pTaskQueue->maxTaskSize = (parameters.maxTaskSize - 1u + parameters.alignment) & ~(parameters.alignment - 1u);
            pTaskQueue->pCopyBuffer = (uint8*)pAllocator->allocate(parameters.maxTaskCount * pTaskQueue->maxTaskSize, parameters.alignment, 0u, nullptr);
            pTaskQueue->copyBufferSlotIndex = 0u;
            pTaskQueue->queue.create(pAllocator, parameters.maxTaskCount, 0x10u);
            pTaskQueue->queueMutex.create("TaskQueueMutex");
            pTaskQueue->pendingTaskCount = 0u;
            pTaskQueue->inProgressTaskCount = 0u;
            pTaskQueue->taskCompleteEvent.create("TaskQueueTaskComplete", 0u);
            pTaskQueue->clientThreadId = getCurrentThreadId();
            if (!addTaskQueue(pTaskSystem, pTaskQueue, parameters.priority))
            {
                // The copy buffer and the event are not released (original leak).
                pTaskQueue->queueMutex.destroy();
                pTaskQueue->queue.destroy(pAllocator);
                pTaskQueue->~TaskQueue();
                pAllocator->free(pTaskQueue);
                return nullptr;
            }
            return pTaskQueue;
        }

        // 0x00C80470 (:104)
        void destroyTaskQueue(MemoryAllocator* pAllocator, TaskQueue* pTaskQueue)
        {
            clearQueuedTasks(pTaskQueue);
            waitForTaskQueue(pTaskQueue);
            removeTaskQueue(pTaskQueue->pTaskSystem, pTaskQueue);
            pAllocator->free(pTaskQueue->pCopyBuffer);
            pTaskQueue->taskCompleteEvent.destroy();
            pTaskQueue->queueMutex.destroy();
            pTaskQueue->queue.destroy(pAllocator);
            pTaskQueue->~TaskQueue();
            pAllocator->free(pTaskQueue);
        }

        // 0x00C7F7C0 (:128)
        void clearQueuedTasks(TaskQueue* pTaskQueue)
        {
            MutexLock lock(&pTaskQueue->queueMutex);
            InterlockedExchangeAdd((volatile LONG*)&pTaskQueue->pendingTaskCount, -(LONG)pTaskQueue->queue.getSize());
            pTaskQueue->queue.clear();
        }

        // 0x00C80040 (:142)
        // Runs queued tasks on the calling thread (thread index 0) until none are pending or in progress, sleeping on
        // the completion event while the workers hold the rest.
        void waitForTaskQueue(TaskQueue* pTaskQueue)
        {
            while (pTaskQueue->pendingTaskCount != 0u || pTaskQueue->inProgressTaskCount != 0u)
            {
                pTaskQueue->queueMutex.lock(0u);
                if (pTaskQueue->queue.getSize() == 0u || (pTaskQueue->queue.peek() == &s_queueSyncTask && pTaskQueue->inProgressTaskCount != 0u))
                {
                    pTaskQueue->queueMutex.unlock();
                    pTaskQueue->taskCompleteEvent.sleepUntilSignaled();
                    continue;
                }
                InterlockedIncrement((volatile LONG*)&pTaskQueue->inProgressTaskCount);
                Task* pTask = pTaskQueue->queue.pop();
                pTaskQueue->queueMutex.unlock();
                if (pTask == nullptr)
                {
                    pTaskQueue->taskCompleteEvent.sleepUntilSignaled();
                    continue;
                }
                const TaskContext taskContext(pTask, 0u);
                pTask->header.pTaskFunction(&taskContext);
                InterlockedDecrement((volatile LONG*)&pTaskQueue->pendingTaskCount);
                InterlockedDecrement((volatile LONG*)&pTaskQueue->inProgressTaskCount);
                pTaskQueue->taskCompleteEvent.signal();
            }
        }

        // 0x00C7F410 (:164)
        bool isTaskQueueEmpty(TaskQueue* pTaskQueue)
        {
            return pTaskQueue->pendingTaskCount == 0u;
        }

        // 0x00C7F420 (:173)
        bool pushSyncTaskToQueue(TaskQueue* pTaskQueue)
        {
            debug::nativePrintArgs("[task] pushSyncTaskToQueue currently not implemented.\n");
            return false;
        }

        // 0x00C7FBA0 (:213)
        bool pushTaskToQueue(TaskQueue* pTaskQueue, const Task* pTask, uint32 taskSize)
        {
            {
                MutexLock lock(&pTaskQueue->queueMutex);
                if (pTaskQueue->queue.isFull())
                {
                    debug::nativePrintArgs("[task] Can't push task to queue since queue is full.\n");
                    return false;
                }
                Task* pCopy = getNextCopyBufferSlot(pTaskQueue);
                memcpy(pCopy, pTask, taskSize);
                pTaskQueue->queue.push(pCopy);
                InterlockedIncrement((volatile LONG*)&pTaskQueue->pendingTaskCount);
            }
            signalPendingTasks(pTaskQueue->pTaskSystem, 1u);
            return true;
        }

        // 0x00C7FC50 (:252)
        const Task* popTaskFromQueue(TaskQueue* pTaskQueue)
        {
            MutexLock lock(&pTaskQueue->queueMutex);
            if (pTaskQueue->queue.getSize() == 0u || (pTaskQueue->queue.peek() == &s_queueSyncTask && pTaskQueue->inProgressTaskCount != 0u))
            {
                return nullptr;
            }
            InterlockedIncrement((volatile LONG*)&pTaskQueue->inProgressTaskCount);
            return pTaskQueue->queue.pop();
        }

        // 0x00C7F430 (:278)
        void executePoppedTask(TaskQueue* pTaskQueue, const TaskContext& taskContext)
        {
            taskContext.pTask->header.pTaskFunction(&taskContext);
            InterlockedDecrement((volatile LONG*)&pTaskQueue->pendingTaskCount);
            InterlockedDecrement((volatile LONG*)&pTaskQueue->inProgressTaskCount);
            pTaskQueue->taskCompleteEvent.signal();
        }

        // 0x00C7F460 (:300)
        void clearTaskQueueTaskSystem(TaskQueue* pTaskQueue)
        {
            pTaskQueue->pTaskSystem = nullptr;
        }
    }
}
