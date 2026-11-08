// Original: volition/interface_lib/code/vint_callback.cpp
//
// Callbacks (UI.md UI2): every callback has a handle (a running counter) and a name crc, and is chained in the handle
// and name hashes (64 buckets each, circular lists). A callback starts with one reference; the last release destroys
// it (unhash, clean_up, back to its factory). Native callbacks come from a pool of 32. Compiled in
// interface_lib_unity.cpp.

#include "volition/interface_lib/code/vint_callback.h"

#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/thread.h"

// ---- file-static globals (shared with the unity's other files)
static vint_callback* Callback_handle_hash[64]; // 0x01A3D578
static vint_callback* Callback_name_hash[64]; // 0x01A3D678
static unsigned int Callback_next_handle; // 0x01A48678
static vint_callback_native* Active_callbacks; // 0x01A4867C
static vint_callback_native* Avail_callbacks; // 0x01A48680
static vint_callback_native Native_callbacks[32]; // 0x01A48738

// ---- header-inline members

// 0x0045BF80 (.h:53)
vint_callback::vint_callback()
{
}

// 0x0045BF90 (.h:54)
vint_callback::~vint_callback()
{
}

// 0x0045BFA0 (.h:56)
void vint_callback::add_ref()
{
    ++m_ref_count;
}

// 0x0045BFB0 (.h:60)
unsigned int vint_callback::get_handle() const
{
    return m_handle;
}

// 0x0045BFC0 (.h:78)
void vint_callback::clean_up()
{
    m_handle = 0;
}

// 0x0045BFF0 (.h:94)
vint_callback_native::vint_callback_native()
{
}

// 0x0045C000 (.h:95)
vint_callback_native::~vint_callback_native()
{
}

// ---- vint_callback

// 0x00462110 (:73)
bool vint_callback::init(const char* _name)
{
    if (_name == nullptr)
    {
        return false;
    }
    m_handle = ++Callback_next_handle;
    m_name_crc = crc_stri(_name, 0);
    m_ref_count = 1;
    vint_dlist_add(Callback_handle_hash[m_handle & 0x3F], this, &vint_callback::handle_hash_next, &vint_callback::handle_hash_prev);
    vint_dlist_add(Callback_name_hash[m_name_crc & 0x3F], this, &vint_callback::name_hash_next, &vint_callback::name_hash_prev);
    return true;
}

// 0x004621E0 (:95)
void vint_callback::release()
{
    if (m_ref_count == 1)
    {
        destroy();
        return;
    }
    --m_ref_count;
}

// 0x00462200 (:114)
void vint_callback::destroy()
{
    vint_dlist_remove(Callback_handle_hash[m_handle & 0x3F], this, &vint_callback::handle_hash_next, &vint_callback::handle_hash_prev);
    vint_dlist_remove(Callback_name_hash[m_name_crc & 0x3F], this, &vint_callback::name_hash_next, &vint_callback::name_hash_prev);
    clean_up();
    return_to_factory();
}

// 0x0046B1C0 (:124)
void vint_callback::system_init()
{
    for (int i = 0; i < 64; ++i)
    {
        Callback_name_hash[i] = nullptr;
    }
    for (int i = 0; i < 64; ++i)
    {
        Callback_handle_hash[i] = nullptr;
    }
    vint_callback_native::system_init();
}

// 0x004622B0 (:145)
vint_callback* vint_callback::find_by_name(unsigned int _name_crc)
{
    vint_callback* head = Callback_name_hash[_name_crc & 0x3F];
    vint_callback* cb = head;
    while (cb != nullptr)
    {
        if (cb->m_name_crc == _name_crc)
        {
            return cb;
        }
        cb = cb->name_hash_next;
        if (cb == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x004622E0 (:168)
vint_callback* vint_callback::find_by_handle(unsigned int _handle)
{
    if (_handle == 0)
    {
        return nullptr;
    }
    vint_callback* head = Callback_handle_hash[_handle & 0x3F];
    vint_callback* cb = head;
    while (cb != nullptr)
    {
        if (cb->m_handle == _handle)
        {
            return cb;
        }
        cb = cb->handle_hash_next;
        if (cb == head)
        {
            break;
        }
    }
    return nullptr;
}

// ---- vint_callback_native

// 0x00462320 (:195)
vint_callback_native* vint_callback_native::create_new(const char* _name, bool (*_func)(unsigned int, vint_event_type, void*, unsigned int))
{
    if (_name == nullptr || _func == nullptr)
    {
        return nullptr;
    }
    vint_callback_native* cb = Avail_callbacks;
    if (cb == nullptr)
    {
        return nullptr;
    }
    // The init result is ignored (it fails only for a null name, checked above).
    cb->init_native(_name, _func);
    vint_dlist_remove(Avail_callbacks, cb, &vint_callback_native::factory_next, &vint_callback_native::factory_prev);
    vint_dlist_add(Active_callbacks, cb, &vint_callback_native::factory_next, &vint_callback_native::factory_prev);
    return cb;
}

// 0x004623C0 (:222)
bool vint_callback_native::trigger(unsigned int _doc_handle, vint_event_type _type, void* _data, unsigned int _bytes)
{
    if (m_callback_function == nullptr)
    {
        return false;
    }
    return m_callback_function(_doc_handle, _type, _data, _bytes);
}

// 0x004623F0 (:240)
bool vint_callback_native::init_native(const char* _name, bool (*_func)(unsigned int, vint_event_type, void*, unsigned int))
{
    if (!vint_callback::init(_name))
    {
        return false;
    }
    m_callback_function = _func;
    return true;
}

// 0x00462420 (:252)
void vint_callback_native::return_to_factory()
{
    vint_dlist_remove(Active_callbacks, this, &vint_callback_native::factory_next, &vint_callback_native::factory_prev);
    vint_dlist_add(Avail_callbacks, this, &vint_callback_native::factory_next, &vint_callback_native::factory_prev);
}

// 0x00462490 (:258)
void vint_callback_native::system_init()
{
    Vint_thread_id = get_current_thread_id();
    // Original behaviour kept: the pool is added to the free list without clearing it first, so a second
    // system_init (vint_callback::system_init calls this one too) would link the nodes twice.
    for (int i = 0; i < 32; ++i)
    {
        vint_dlist_add(Avail_callbacks, &Native_callbacks[i], &vint_callback_native::factory_next, &vint_callback_native::factory_prev);
    }
}
