// Original: volition/vlib/code/os/thread.cpp
//
// vlib_unity.obj is a unity build, so file statics are placed by the functions that use them. Initial values are
// read from the binary's .data section.
//
// Notes (decompiled behaviour, kept as is):
// - thread_finished reports "finished" for a handle whose slot was reused, and returns 0xDEADBEEF as the exit code
//   when the exit ring no longer holds the handle; thread_wait_until_done does the same.
// - set_thread_priority / get_thread_priority / get_vlib_thread_priority_from_system_priority are constant in this
//   build.

#include "volition/vlib/code/os/thread.h"

#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

static unsigned int Thread_next_free_slot = 0xFFFFFFFF; // 0x0151965C, head of the Thread_slot free list
static thread_exit_info Thread_exit[64]; // 0x01CD3520, ring written by thread_entry
static bool Thread_initialized; // 0x01CE876B
static unsigned int Thread_next_handle; // 0x01CE8790, generation, += 0x10000 per spawn
static unsigned int Thread_next_exit; // 0x01CE8794
static cs_wrapper Thread_cs(0); // 0x01D500E4

thread_slot Thread_slot[32]; // 0x01D56CA0

// 0x0058C130
thread_slot::thread_slot()
{
}

// 0x0058C140
thread_slot::~thread_slot()
{
}

// 0x0058C100 (:70)
void thread_slot::reset()
{
    m_handle = 0xFFFF;
    m_thread_func = nullptr;
    m_lp_param = nullptr;
    m_next_free_slot = 0xFFFFFFFF;
    m_in_use = false;
}

// 0x0058C120 (:88)
void thread_exit_info::reset()
{
    m_handle = 0xFFFF;
    m_exit_code = 0;
}

// 0x0058C150 (:122)
int thread_entry(const keen::Thread& thread)
{
    thread_slot* slot = (thread_slot*)thread.getArgument();
    const int exit_code = slot->m_thread_func(slot->m_lp_param);

    Thread_cs.enter();
    Thread_exit[Thread_next_exit].m_handle = slot->m_handle;
    Thread_exit[Thread_next_exit].m_exit_code = exit_code;
    Thread_next_exit = (Thread_next_exit + 1) & 0x3F;
    slot->m_next_free_slot = Thread_next_free_slot;
    slot->m_handle = 0xFFFF;
    slot->m_thread_func = nullptr;
    slot->m_lp_param = nullptr;
    slot->m_in_use = false;
    Thread_next_free_slot = (unsigned int)(slot - Thread_slot);
    Thread_cs.leave();
    return exit_code;
}

// 0x0058C1E0 (:155)
bool thread_system_init()
{
    if (!Thread_initialized)
    {
        for (unsigned int i = 0; i < 32; i++)
        {
            Thread_slot[i].reset();
            Thread_slot[i].m_next_free_slot = i + 1;
        }
        Thread_next_free_slot = 0;
        Thread_slot[31].m_next_free_slot = 0xFFFFFFFF;
        for (int i = 0; i < 64; i++)
        {
            Thread_exit[i].reset();
        }
        Thread_initialized = true;
    }
    return true;
}

// 0x005AE870 (:194)
unsigned int thread_spawn(int (*thread_func)(void*), void* lp_param, unsigned int cpu, thread_prior priority, const char* p_name, int stack_size)
{
    Thread_cs.enter();
    const unsigned int index = Thread_next_free_slot;
    if (index == 0xFFFFFFFF)
    {
        VLIB_CRITICAL_FAILURE();
        Thread_cs.leave();
        return 0xFFFF;
    }

    thread_slot* slot = &Thread_slot[index];
    Thread_next_free_slot = slot->m_next_free_slot;
    slot->m_handle = Thread_next_handle + index;
    slot->m_thread_func = thread_func;
    slot->m_lp_param = lp_param;
    slot->m_next_free_slot = 0xFFFFFFFF;
    if (p_name == nullptr)
    {
        p_name = "";
    }
    slot->m_in_use = true;
    slot->m_keen_thread.create(p_name, thread_entry, (int)(((float)(int)priority / 7.0f) * -2000.0f + 1000.0f), (unsigned int)stack_size, cpu);
    slot->m_keen_thread.start(slot);
    Thread_next_handle += 0x10000;
    const unsigned int handle = slot->m_handle;
    Thread_cs.leave();
    return handle;
}

// 0x005AE960 (:249)
bool thread_finished(unsigned int thread, int* exit_code)
{
    Thread_cs.enter();
    if (Thread_slot[thread & 0xFFFF].m_handle == thread)
    {
        Thread_cs.leave();
        return false;
    }
    if (exit_code != nullptr)
    {
        *exit_code = (int)0xDEADBEEF;
        for (int i = 0; i < 64; i++)
        {
            if (Thread_exit[i].m_handle == thread)
            {
                *exit_code = Thread_exit[i].m_exit_code;
                break;
            }
        }
    }
    Thread_cs.leave();
    return true;
}

// 0x0058C250 (:281)
void thread_yield()
{
    keen::sleepCurrentThreadMilliseconds(1);
}

// 0x0058C260 (:288)
void thread_wait(unsigned int thread)
{
    thread_slot* slot = &Thread_slot[thread & 0xFFFF];
    if (slot->m_handle == thread && slot->m_in_use)
    {
        slot->m_keen_thread.join(0xFFFFFFFF);
    }
}

// 0x0058C290 (:304)
int thread_wait_until_done(unsigned int thread)
{
    thread_slot* slot = &Thread_slot[thread & 0xFFFF];
    if (slot->m_handle == thread && slot->m_in_use)
    {
        slot->m_keen_thread.join(0xFFFFFFFF);
    }
    Thread_cs.enter();
    for (int i = 0; i < 64; i++)
    {
        if (Thread_exit[i].m_handle == thread)
        {
            const int exit_code = Thread_exit[i].m_exit_code;
            Thread_cs.leave();
            return exit_code;
        }
    }
    Thread_cs.leave();
    return (int)0xDEADBEEF;
}

// 0x0058C310 (:335)
unsigned int get_current_thread_id()
{
    return keen::getCurrentThreadId();
}

// 0x0058C320 (:344)
bool set_thread_priority(unsigned int thread, thread_prior priority)
{
    return true;
}

// 0x0058C330 (:360)
thread_prior get_thread_priority(unsigned int thread)
{
    return THREAD_PRIOR_LOWEST;
}

// 0x0058C340 (:378)
thread_prior get_vlib_thread_priority_from_system_priority(int system_priority)
{
    return THREAD_PRIOR_HIGHEST;
}
