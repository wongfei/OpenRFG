// Original: volition/vlib/code/memory/mempool_base.cpp
//
// The pool interface: name, flags and the lock/ownership checks every modifying call goes through. The inline
// accessors of mempool_base.h are defined here too. Pool operator new returns null (with a log line) when the pool
// is exhausted.

#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <string.h>

// 0x00589430 (:28)
void mempool_base::base_init(const char* pool_name, unsigned char mp_flags)
{
    if (pool_name == nullptr)
    {
        pool_name = "Untitled";
    }
    strncpy(name, pool_name, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    thread_id = 0;
    flags = mp_flags;
    lock_id = 0;
    locked = false;
    peak_usage = 0;
}

// 0x00589480 (:74)
void mempool_base::destroy()
{
}

// 0x00589490 (:97)
mempool_base::mempool_base(const char* pool_name, unsigned char mp_flags)
{
    base_init(pool_name, mp_flags);
}

// 0x005894E0 (:102)
mempool_base::~mempool_base()
{
}

// 0x005AB260 (:141)
bool mempool_base::assert_permission() const
{
    if (locked)
    {
        char message[512];
        _snprintf_safe(message, sizeof(message), "Cannot perform operation on a locked mempool (%s)\n", name);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    if (!is_owned())
    {
        char message[512];
        _snprintf_safe(message, sizeof(message), "Cannot perform operation on an unowned mempool (%s)\n", name);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    return true;
}

// 0x00563AF0 (.h:77)
const char* mempool_base::get_name() const
{
    return name;
}

// 0x004A9B20 (.h:78)
unsigned char mempool_base::get_flags() const
{
    return flags;
}

// 0x005894F0 (:219)
void mempool_base::lock()
{
    locked = true;
    lock_id = thread_id;
}

// 0x00589500 (:229)
void mempool_base::unlock()
{
    if (!is_owned())
    {
        keen::debug::nativePrintArgs("Only the thread that locked the pool can unlock it try unlock_ownership()");
        return;
    }
    locked = false;
    lock_id = 0;
}

// 0x00589530 (:244)
bool mempool_base::is_locked() const
{
    return locked;
}

// 0x007B98C0 (.h:115)
unsigned int mempool_base::peak_used() const
{
    return peak_usage;
}

void mempool_base::reset_peak_used()
{
    peak_usage = 0;
}

// 0x00705120 (mempool_base.h:161)
void* operator new(size_t size, mempool_base& pool)
{
    void* p = pool.alloc((unsigned int)size, 0);
    if (p == nullptr)
    {
        keen::debug::nativePrintArgs("Pool operator new out of memory\n");
    }
    return p;
}

// 0x0045AB80 (mempool_base.h:170)
void* operator new[](size_t size, mempool_base& pool)
{
    void* p = pool.alloc((unsigned int)size, 0);
    if (p == nullptr)
    {
        keen::debug::nativePrintArgs("Pool operator new out of memory\n");
    }
    return p;
}
