// Original: library/modules/runtime/task/sources/global/tasksystem.cpp
//
// The task system: one worker thread per bit of the affinity mask, woken by a semaphore that counts pending tasks.
// A worker takes the next task from the registered queues in priority order (popNextQueueTask), runs it and signals
// the queue's completion event. With an empty mask there are no workers and tasks only run inside waitForTaskQueue on
// the waiting thread; GameFramework::createTaskSystem builds rfg.exe's task system that way
// (maxTaskSystemWorkerThreads is 0).
//
// Notes (decompiled behaviour, kept as is):
// - Worker thread indices start at 1 (0 is the thread that waits for a queue).
// - Only the lowest 16 affinity bits get a worker.

#include "keen/task/tasksystem.hpp"
#include "keen/task/taskqueue.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/task/sources/global/tasksystem_internal.hpp"

#include <new>
#include <windows.h>

namespace keen
{
    static uint32 countBits(uint32 value)
    {
        value = ((value >> 1) & 0x55555555u) + (value & 0x55555555u);
        value = ((value >> 2) & 0x33333333u) + (value & 0x33333333u);
        value = ((value >> 4) & 0x0F0F0F0Fu) + (value & 0x0F0F0F0Fu);
        value = ((value >> 8) & 0x00FF00FFu) + (value & 0x00FF00FFu);
        return (value >> 16) + (value & 0xFFFFu);
    }

    // 0x00C0E0F0 (.h:31)
    TaskSystemParameters::TaskSystemParameters()
    {
        threadAffinityMask = task::getDefaultThreadAffinityMask();
        // Magic numbers: 32 KB worker stacks, 4 queues.
        workerThreadStackSize = 0x8000u;
        maxTaskQueueCount = 4u;
    }

    // 0x00C7F810
    TaskSystem::TaskSystem()
    {
    }

    // 0x00C7F730
    TaskSystem::~TaskSystem()
    {
    }

    // 0x00C7F790
    WorkerThreadContext::WorkerThreadContext()
    {
    }

    // 0x00C7F7A0
    WorkerThreadContext::~WorkerThreadContext()
    {
    }

    namespace task
    {
        // 0x00C7F470 (:47)
        // Every hardware thread except the first; with more than two left, the highest one is left free too.
        uint32 getDefaultThreadAffinityMask()
        {
            const uint32 availableMask = getAvailableCpuHardwareThreadMask();
            uint32 mask = availableMask & ~1u;
            if (countBits(availableMask & ~1u) > 2u)
            {
                if (mask == 0u)
                {
                    return availableMask & 0x7FFFFFFEu;
                }
                unsigned long highestBit;
                _BitScanReverse(&highestBit, mask);
                mask &= ~(1u << highestBit);
            }
            return mask;
        }

        // 0x00C7F550 (:68)
        void initializeTaskSystemParameters(TaskSystemParameters* pParameters, uint32 affinity)
        {
            pParameters->threadAffinityMask = affinity;
        }

        // 0x00C80130 (:75)
        TaskSystem* createTaskSystem(MemoryAllocator* pAllocator, const TaskSystemParameters& parameters)
        {
            void* pMemory = pAllocator->allocate(sizeof(TaskSystem), 4u, 0u, nullptr);
            TaskSystem* pTaskSystem = pMemory != nullptr ? new (pMemory) TaskSystem() : nullptr;

            pTaskSystem->workerThreadContexts.create(pAllocator, countBits(parameters.threadAffinityMask), 0x10u);
            pTaskSystem->taskQueueContexts.create(pAllocator, parameters.maxTaskQueueCount, 0x10u);
            pTaskSystem->queueArrayMutex.create("TaskSystemQueueArray");
            // Magic number: the semaphore counts up to 0xFFFF pending tasks.
            pTaskSystem->wakeUpSemaphore.create("TaskSystemWakeUp", 0, 0xFFFF);
            pTaskSystem->clientThreadId = getCurrentThreadId();

            uint32 createdCount = 0u;
            uint32 affinityBit = 1u;
            for (uint32 bitIndex = 0u; bitIndex < 16u && createdCount < pTaskSystem->workerThreadContexts.getSize(); ++bitIndex)
            {
                if ((parameters.threadAffinityMask & affinityBit) != 0u)
                {
                    WorkerThreadContext* pContext = &pTaskSystem->workerThreadContexts[createdCount];
                    ++createdCount;
                    pContext->pTaskSystem = pTaskSystem;
                    pContext->threadIndex = createdCount;
                    pContext->thread.create("TaskSystem", taskSystemWorkerThread, 0, parameters.workerThreadStackSize, affinityBit);
                    pContext->thread.start(pContext);
                }
                affinityBit = _rotl(affinityBit, 1);
            }
            return pTaskSystem;
        }

        // 0x00C80520 (:132)
        void destroyTaskSystem(MemoryAllocator* pAllocator, TaskSystem* pTaskSystem)
        {
            Array<WorkerThreadContext>& workers = pTaskSystem->workerThreadContexts;
            for (uint32 i = 0u; i < workers.getSize(); ++i)
            {
                workers[i].thread.requestQuit();
            }
            if (workers.getSize() != 0u)
            {
                pTaskSystem->wakeUpSemaphore.incrementValue((int)workers.getSize());
            }
            for (uint32 i = 0u; i < workers.getSize(); ++i)
            {
                workers[i].thread.join((uint32)Thread::TimeOutInfinite);
                workers[i].thread.destroy();
            }
            pTaskSystem->wakeUpSemaphore.destroy();
            pTaskSystem->queueArrayMutex.destroy();
            pTaskSystem->taskQueueContexts.destroy(pAllocator);
            // The array's destroy, with the element destructors (array.inl's destroy runs none).
            if (workers.getStart() != nullptr)
            {
                for (uint32 i = workers.getSize(); i != 0u; --i)
                {
                    workers[i - 1u].~WorkerThreadContext();
                }
                workers.destroy(pAllocator);
            }
            pTaskSystem->~TaskSystem();
            pAllocator->free(pTaskSystem);
        }

        // 0x00C7F840 (:168)
        uint32 getWorkerThreadCount(TaskSystem* pTaskSystem)
        {
            return pTaskSystem->workerThreadContexts.getSize();
        }

        // 0x00C7F850 (:177)
        // Queues are kept sorted by descending priority; a new queue goes after those of the same priority.
        bool addTaskQueue(TaskSystem* pTaskSystem, TaskQueue* pTaskQueue, uint32 priority)
        {
            MutexLock lock(&pTaskSystem->queueArrayMutex);
            SizedArray<TaskQueueContext>& contexts = pTaskSystem->taskQueueContexts;
            if (contexts.isFull())
            {
                debug::nativePrintArgs("[task] Maximum number of queues of %u in task system reached. Can't add any more queues.\n", contexts.getCapacity());
                return false;
            }
            TaskQueueContext* pInsert = contexts.getStart();
            TaskQueueContext* pEnd = contexts.getEnd();
            while (pInsert != pEnd && priority <= pInsert->priority)
            {
                ++pInsert;
            }
            TaskQueueContext context;
            context.pQueue = pTaskQueue;
            context.priority = priority;
            contexts.insert(pInsert, context);
            return true;
        }

        // 0x00C7FCC0 (:209)
        bool removeTaskQueue(TaskSystem* pTaskSystem, TaskQueue* pTaskQueue)
        {
            MutexLock lock(&pTaskSystem->queueArrayMutex);
            SizedArray<TaskQueueContext>& contexts = pTaskSystem->taskQueueContexts;
            TaskQueueContext* pContext = contexts.getStart();
            TaskQueueContext* pEnd = contexts.getEnd();
            for (;; ++pContext)
            {
                if (pContext == pEnd)
                {
                    debug::nativePrintArgs("[task] Can't remove this queue from task system because it was not found.\n");
                    return false;
                }
                if (pContext->pQueue == pTaskQueue)
                {
                    break;
                }
            }
            pTaskQueue->pTaskSystem = nullptr;
            if (!contexts.isEmpty())
            {
                contexts.erase(pContext);
            }
            return true;
        }

        // 0x00C7F8F0 (:234)
        void signalPendingTasks(TaskSystem* pTaskSystem, uint32 pendingTaskCount)
        {
            if (pTaskSystem->workerThreadContexts.getSize() != 0u)
            {
                pTaskSystem->wakeUpSemaphore.incrementValue((int)pendingTaskCount);
            }
        }
    }

    // 0x00C7FD60 (:247)
    // A queue whose next task is the sync task is skipped while it still has tasks in progress.
    const Task* popNextQueueTask(TaskQueue** ppSourceQueue, const WorkerThreadContext* pContext)
    {
        MutexLock systemLock(&pContext->pTaskSystem->queueArrayMutex);
        for (uint32 i = 0u; i < pContext->pTaskSystem->taskQueueContexts.getSize(); ++i)
        {
            TaskQueue* pQueue = pContext->pTaskSystem->taskQueueContexts[i].pQueue;
            pQueue->queueMutex.lock(0u);
            if (pQueue->queue.getSize() == 0u || (pQueue->queue.peek() == &s_queueSyncTask && pQueue->inProgressTaskCount != 0u))
            {
                pQueue->queueMutex.unlock();
                continue;
            }
            InterlockedIncrement((volatile LONG*)&pQueue->inProgressTaskCount);
            Task* pTask = pQueue->queue.pop();
            pQueue->queueMutex.unlock();
            if (pTask != nullptr)
            {
                *ppSourceQueue = pQueue;
                return pTask;
            }
        }
        return nullptr;
    }

    // 0x00C7FE50 (:273)
    int taskSystemWorkerThread(const Thread& thread)
    {
        const WorkerThreadContext* pContext = (const WorkerThreadContext*)thread.getArgument();
        while (!thread.isQuitRequested())
        {
            if (!pContext->pTaskSystem->wakeUpSemaphore.tryDecrementValue((uint32)Semaphore::TimeOutInfinite))
            {
                continue;
            }
            TaskQueue* pQueue = nullptr;
            const Task* pTask = popNextQueueTask(&pQueue, pContext);
            if (pTask != nullptr)
            {
                const TaskContext taskContext(pTask, pContext->threadIndex);
                pTask->header.pTaskFunction(&taskContext);
                InterlockedDecrement((volatile LONG*)&pQueue->pendingTaskCount);
                InterlockedDecrement((volatile LONG*)&pQueue->inProgressTaskCount);
                pQueue->taskCompleteEvent.signal();
            }
        }
        return 0;
    }
}
