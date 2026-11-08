// Original: volition/vlib/code/os/cs_wrapper.h
// Implementation: volition/vlib/code/os/cs_wrapper.cpp
//
// Critical sections. cs_wrapper is a keen::Mutex (the Win32 CRITICAL_SECTION of the original PC port);
// cs_spinlock is an inline 64-bit spin lock. Both have RAII scope helpers.
#pragma once

#include <stdint.h>

#include "keen/base/mutex.hpp"

// size 0x3C
class cs_wrapper
{
public:
    cs_wrapper(int __debug_handle); // 0x0058ACF0 (:22)
    ~cs_wrapper(); // 0x0058AD20 (:30)
    void enter(); // 0x0058AD30 (:36)
    void leave(); // 0x0058AD40 (:42)

private:
    keen::Mutex m_mutex; // 0x00
    int debug_handle; // 0x38
};

// size 0x4
class cs_scope
{
public:
    cs_scope(cs_wrapper& pcs); // 0x00482B90 (.h:48)
    ~cs_scope(); // 0x00482BB0 (.h:49)

private:
    cs_wrapper* my_cs; // 0x00
};

// size 0x8
class cs_spinlock
{
public:
    cs_spinlock(); // 0x00482BC0 (.h:73)
    ~cs_spinlock(); // 0x00482BD0 (.h:74)
    void enter(); // 0x00482BE0 (.h:77)
    void leave(); // 0x0048CD40 (.h:109)

private:
    uint64_t lockValue; // 0x00
};

// size 0x4
class csspinlock_scope
{
public:
    csspinlock_scope(cs_spinlock& pcs); // 0x00482C70 (.h:132)
    ~csspinlock_scope(); // 0x0048CDD0 (.h:133)

private:
    cs_spinlock* my_cs; // 0x00
};
