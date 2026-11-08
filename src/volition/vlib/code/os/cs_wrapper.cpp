// Original: volition/vlib/code/os/cs_wrapper.cpp
//
// cs_wrapper is a keen::Mutex. cs_spinlock (inline in cs_wrapper.h in the original) is a recursive spin lock: the low
// dword of lockValue is the owning thread id, the high dword the recursion depth minus one. Waiting threads sleep
// for 1 microsecond (0 ms) between attempts.

#include "volition/vlib/code/os/cs_wrapper.h"

#include "keen/base/thread.hpp"

#include <intrin.h>

// 0x0058ACF0 (:22)
cs_wrapper::cs_wrapper(int __debug_handle)
{
    m_mutex.create("cs_wrapper Mutex");
    debug_handle = __debug_handle;
}

// 0x0058AD20 (:30)
cs_wrapper::~cs_wrapper()
{
    m_mutex.destroy();
}

// 0x0058AD30 (:36)
void cs_wrapper::enter()
{
    m_mutex.lock(0);
}

// 0x0058AD40 (:42)
void cs_wrapper::leave()
{
    m_mutex.unlock();
}

// 0x00482B90 (.h:48)
cs_scope::cs_scope(cs_wrapper& pcs)
{
    my_cs = &pcs;
    pcs.enter();
}

// 0x00482BB0 (.h:49)
cs_scope::~cs_scope()
{
    my_cs->leave();
}

// 0x00482BC0 (.h:73)
cs_spinlock::cs_spinlock()
{
    lockValue = 0;
}

// 0x00482BD0 (.h:74)
cs_spinlock::~cs_spinlock()
{
}

// 0x00482BE0 (.h:77)
void cs_spinlock::enter()
{
    const uint32_t thread_id = keen::getCurrentThreadId();
    for (;;)
    {
        const uint64_t value = lockValue;
        const uint32_t owner = (uint32_t)value;
        if (owner == thread_id)
        {
            // Already owned by this thread: increase the depth.
            const uint64_t new_value = ((uint64_t)((uint32_t)(value >> 32) + 1) << 32) | thread_id;
            if ((uint64_t)_InterlockedCompareExchange64((volatile long long*)&lockValue, (long long)new_value, (long long)value) == value)
            {
                break;
            }
        }
        else if (owner == 0)
        {
            if (_InterlockedCompareExchange64((volatile long long*)&lockValue, (long long)thread_id, 0) == 0)
            {
                break;
            }
        }
        keen::sleepCurrentThreadMicroseconds(1);
    }
    _ReadWriteBarrier();
}

// 0x0048CD40 (.h:109)
void cs_spinlock::leave()
{
    _ReadWriteBarrier();
    const uint32_t thread_id = keen::getCurrentThreadId();
    for (;;)
    {
        const uint64_t value = lockValue;
        const uint32_t depth = (uint32_t)(value >> 32);
        const uint64_t new_value = depth != 0 ? (((uint64_t)(depth - 1) << 32) | thread_id) : 0;
        if ((uint64_t)_InterlockedCompareExchange64((volatile long long*)&lockValue, (long long)new_value, (long long)value) == value)
        {
            return;
        }
    }
}

// 0x00482C70 (.h:132)
csspinlock_scope::csspinlock_scope(cs_spinlock& pcs)
{
    my_cs = &pcs;
    pcs.enter();
}

// 0x0048CDD0 (.h:133)
csspinlock_scope::~csspinlock_scope()
{
    my_cs->leave();
}
