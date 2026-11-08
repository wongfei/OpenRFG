// Original: library/modules/runtime/task/include/keen/task/tasktypes.hpp
//
// The task record types. A task is a header with its function; the function gets a
// TaskContext with the task and the worker thread index.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Task;

    // size 0x8
    struct TaskContext
    {
        TaskContext(const Task* pInTask, uint32 inThreadIndex); // 0x00C7F3E0 (.h:18)

        const Task* pTask; // 0x00
        const uint32 threadIndex; // 0x04
    };

    // size 0x4
    struct TaskHeader
    {
        void (*pTaskFunction)(const TaskContext*); // 0x00
    };

    // size 0x4
    struct Task
    {
        TaskHeader header; // 0x00
    };
}
