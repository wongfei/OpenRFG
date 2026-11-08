// Original: volition/vlib/code/os/thread.h
// Implementation: volition/vlib/code/os/thread.cpp
//
// Volition thread API on top of keen::Thread. Threads live in a fixed table Thread_slot[32] with a free list;
// the handle returned by thread_spawn is (slot index + generation * 0x10000), 0xFFFF on failure (decompiled).
// thread_entry runs the user function, records its exit code in the 64-entry ring Thread_exit, and frees the slot.
// The vlib priority (0 = highest .. 7 = lowest) maps to a keen priority of 1000 - priority / 7 * 2000.
#pragma once

#include "keen/base/thread.hpp"

enum thread_prior
{
    THREAD_PRIOR_HIGHEST = 0,
    THREAD_PRIOR_HIGH_PLUS3 = 1,
    THREAD_PRIOR_HIGH_PLUS2 = 2,
    THREAD_PRIOR_HIGH_PLUS1 = 3,
    THREAD_PRIOR_HIGH = 4,
    THREAD_PRIOR_NORMAL = 5,
    THREAD_PRIOR_LOW = 6,
    THREAD_PRIOR_LOWEST = 7,
    THREAD_PRIOR_COUNT = 8,
};

// Placement inferred (thread.cpp defines reset()).
// size 0x68
struct thread_slot
{
    thread_slot(); // 0x0058C130
    ~thread_slot(); // 0x0058C140
    void reset(); // 0x0058C100 (:70)

    unsigned int m_handle; // 0x00
    int (*m_thread_func)(void*); // 0x04
    void* m_lp_param; // 0x08
    unsigned int m_next_free_slot; // 0x0C
    keen::Thread m_keen_thread; // 0x10, its argument is this slot
    bool m_in_use; // 0x64
};

// Placement inferred (thread.cpp defines reset()).
// size 0x8
struct thread_exit_info
{
    void reset(); // 0x0058C120 (:88)

    unsigned int m_handle; // 0x00
    int m_exit_code; // 0x04
};

extern thread_slot Thread_slot[32]; // 0x01D56CA0

int thread_entry(const keen::Thread& thread); // 0x0058C150 (:122)
bool thread_system_init(); // 0x0058C1E0 (:155)
unsigned int thread_spawn(int (*thread_func)(void*), void* lp_param, unsigned int cpu, thread_prior priority, const char* p_name, int stack_size); // 0x005AE870 (:194)
bool thread_finished(unsigned int thread, int* exit_code); // 0x005AE960 (:249)
void thread_yield(); // 0x0058C250 (:281)
void thread_wait(unsigned int thread); // 0x0058C260 (:288)
int thread_wait_until_done(unsigned int thread); // 0x0058C290 (:304)
unsigned int get_current_thread_id(); // 0x0058C310 (:335)
bool set_thread_priority(unsigned int thread, thread_prior priority); // 0x0058C320 (:344)
thread_prior get_thread_priority(unsigned int thread); // 0x0058C330 (:360)
thread_prior get_vlib_thread_priority_from_system_priority(int system_priority); // 0x0058C340 (:378)
