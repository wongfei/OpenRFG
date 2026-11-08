// Original: library/modules/runtime/task/include/keen/task/tasksystem.hpp
// Implementation: library/modules/runtime/task/sources/global/tasksystem.cpp
//
// The task system API: worker threads that run task queues (TaskSystemParameters, create/destroy, queue registration).
// TaskSystem itself is defined in sources/global/tasksystem_internal.hpp.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class MemoryAllocator;
    struct Task;
    struct TaskQueue;
    struct TaskSystem;
    class Thread;
    struct WorkerThreadContext;

    // size 0xC
    struct TaskSystemParameters
    {
        TaskSystemParameters(); // 0x00C0E0F0 (.h:31)

        uint32 threadAffinityMask; // 0x00
        uint32 workerThreadStackSize; // 0x04
        uint32 maxTaskQueueCount; // 0x08
    };

    namespace task
    {
        uint32 getDefaultThreadAffinityMask(); // 0x00C7F470 (:47)
        void initializeTaskSystemParameters(TaskSystemParameters* pParameters, uint32 affinity); // 0x00C7F550 (:68)
        TaskSystem* createTaskSystem(MemoryAllocator* pAllocator, const TaskSystemParameters& parameters); // 0x00C80130 (:75)
        void destroyTaskSystem(MemoryAllocator* pAllocator, TaskSystem* pTaskSystem); // 0x00C80520 (:132)
        uint32 getWorkerThreadCount(TaskSystem* pTaskSystem); // 0x00C7F840 (:168)
        bool addTaskQueue(TaskSystem* pTaskSystem, TaskQueue* pTaskQueue, uint32 priority); // 0x00C7F850 (:177)
        bool removeTaskQueue(TaskSystem* pTaskSystem, TaskQueue* pTaskQueue); // 0x00C7FCC0 (:209)
        void signalPendingTasks(TaskSystem* pTaskSystem, uint32 pendingTaskCount); // 0x00C7F8F0 (:234)
    }

    const Task* popNextQueueTask(TaskQueue** ppSourceQueue, const WorkerThreadContext* pContext); // 0x00C7FD60 (:247)
    int taskSystemWorkerThread(const Thread& thread); // 0x00C7FE50 (:273)
}
