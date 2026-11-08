// Original: volition/vlib/code/memory/static_mempool.cpp
//
// Bump allocator over a fixed buffer. An alignment of 0 means the pool's default alignment. Pools with the single
// allocation flag (mp_flags 0x01) refuse a second allocation. Only the newest block can be resized (realloc). The
// inline accessors of static_mempool.h are defined here too.

#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

// Message buffer of failed allocations (file static, inferred: the PDB does not record it).
static char overflow[512];

// 0x0058A2F0 (:18)
static_mempool_base::static_mempool_base()
    : mempool_base("Untitled", 0)
{
    pool = nullptr;
    def_align = 0;
    max_size = 0;
    current_pos = 0;
}

// 0x0058A350 (:26)
static_mempool_base::static_mempool_base(const char* name)
    : mempool_base(name, 0)
{
    pool = nullptr;
    def_align = 0;
    max_size = 0;
    current_pos = 0;
}

// 0x005AC200 (:42)
static_mempool_base::static_mempool_base(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
    : mempool_base(pool_name, mp_flags)
{
    pool = nullptr;
    def_align = 0;
    max_size = 0;
    current_pos = 0;
    // A direct (non-virtual) call: the constructor runs before any derived vtable is installed.
    static_mempool_base::init_static_mempool(mem, mem_size, pool_name, align, mp_flags);
}

// 0x0058A3B0 (:53)
static_mempool_base::~static_mempool_base()
{
}

// 0x0058A3C0 (:63)
bool static_mempool_base::is_owned() const
{
    return true;
}

// 0x0058A3D0 (:80)
void static_mempool_base::set_thread_ownership()
{
}

// 0x0058A3E0 (:92)
void* static_mempool_base::record_last_alloc(unsigned int offset, unsigned int size)
{
    last_alloc = pool + offset + current_pos;
    current_pos = current_pos + offset + size;
    if (peak_usage < current_pos)
    {
        peak_usage = current_pos;
    }
    return last_alloc;
}

// 0x0058A420 (:122)
void static_mempool_base::init_static_mempool(const void* mem, unsigned int mem_size, const char* pool_name, unsigned int align, unsigned char mp_flags)
{
    base_init(pool_name, mp_flags);
    if (mem == nullptr)
    {
        keen::debug::nativePrintArgs("[vlib] mempool %s initialized with NULL and size %d\n", name, mem_size);
    }
    pool = (unsigned char*)mem;
    max_size = mem_size;
    def_align = align;
    current_pos = 0;
}

// 0x0045B020 (.h:40)
void* static_mempool_base::get_base()
{
    return pool;
}

// 0x0045B030 (.h:43)
bool static_mempool_base::is_dynamic() const
{
    return false;
}

// 0x0058A340 (.h:52)
unsigned int static_mempool_base::space_free() const
{
    return max_size - current_pos;
}

// 0x0045B040 (.h:53)
unsigned int static_mempool_base::space_used() const
{
    return max_size - space_free();
}

// 0x0045B060 (.h:54)
unsigned int static_mempool_base::space_max() const
{
    return max_size;
}

unsigned int static_mempool_base::get_alignment() const
{
    return def_align;
}

// 0x0058A4A0 (:142)
unsigned int static_mempool_base::mark()
{
    return current_pos;
}

// 0x005AC280 (:152)
bool static_mempool_base::restore_to_mark(unsigned int pos)
{
    if (!assert_permission())
    {
        return false;
    }
    current_pos = pos;
    return true;
}

// 0x0058A4B0 (:173)
bool static_mempool_base::can_alloc(unsigned int size, unsigned int align) const
{
    if (!is_owned() || is_locked() || (current_pos != 0 && (flags & 1) != 0))
    {
        return false;
    }
    if (align == 0)
    {
        align = def_align;
    }
    const unsigned int misalignment = (unsigned int)(pool + current_pos) % align;
    const unsigned int padding = misalignment == 0 ? 0 : align - misalignment;
    return padding + size <= space_free();
}

// 0x005AC2A0 (:206)
void* static_mempool_base::alloc(unsigned int size, unsigned int align)
{
    if (!assert_permission())
    {
        return nullptr;
    }
    if (current_pos != 0 && (flags & 1) != 0)
    {
        keen::debug::nativePrintArgs("Cannot alloc twice from SINGLE_ALLOC mempool");
        return nullptr;
    }
    if (align == 0)
    {
        align = def_align;
    }
    const unsigned int misalignment = (unsigned int)(pool + current_pos) % align;
    const unsigned int padding = misalignment == 0 ? 0 : align - misalignment;
    if (space_free() < padding + size)
    {
        _snprintf_safe(overflow, sizeof(overflow), "Failed aligned alloc of\n%d bytes in %s pool\n(max pool size is %d\nspace left is %d)\n", size, name, max_size, space_free());
        keen::debug::nativePrintArgs(overflow);
        return nullptr;
    }
    return record_last_alloc(padding, size);
}

// 0x0058A520 (:248)
void* static_mempool_base::alloc_all(unsigned int align)
{
    unsigned int size = space_free();
    if (align == 0)
    {
        align = def_align;
    }
    if (!can_alloc(size, 0))
    {
        size -= align;
    }
    return alloc(size, align);
}

// 0x005AC370 (:273)
void* static_mempool_base::realloc(void* mem, unsigned int new_size)
{
    if (!assert_permission() || mem != last_alloc)
    {
        return nullptr;
    }
    const unsigned int new_pos = (unsigned int)(last_alloc + new_size - pool);
    if (space_free() < new_pos - current_pos)
    {
        return nullptr;
    }
    current_pos = new_pos;
    return last_alloc;
}

// 0x0058A560 (:316)
bool static_mempool_base::contains_address(const void* mem) const
{
    return pool <= mem && mem < pool + max_size;
}

// 0x0058A580 (:334)
bool static_mempool_base::pad_to_page(unsigned int page_size)
{
    const unsigned int misalignment = (unsigned int)(pool + current_pos) % page_size;
    if (misalignment != 0 && page_size - misalignment != 0)
    {
        return alloc(page_size - misalignment, 1) != nullptr;
    }
    return true;
}

// 0x0058A5C0 (:356)
bool static_mempool_base::release_bytes(unsigned int num_bytes)
{
    return restore_to_mark(current_pos - num_bytes);
}

// 0x0058A5E0 (:377)
bool static_mempool_base::clear()
{
    return restore_to_mark(0);
}
