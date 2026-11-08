// Original: volition/render_lib/code/rl_material_library.cpp
//
// The material library: equal materials (rl_material_base::compare) share one bucket, found through Matlib_entry_hash
// and addressed by a handle (bucket index in bits 0-15, a per-thread guard counter above). A bucket keeps its
// registered materials (the first is the current instance) and an optional override material. The current render
// material and the override handle are per thread.

#include "volition/render_lib/code/rl_material_library.h"

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "keen/base/trace.hpp"

// The thread-local state of the material library (TLS offsets 0x18-0x28; names inferred).
static thread_local unsigned int Matlib_override_handle = 0xFFFFFFFF; // TLS 0x18
static thread_local unsigned int Matlib_render_material_handle = 0xFFFFFFFF; // TLS 0x1C
static thread_local int Matlib_render_material_pass = 0; // TLS 0x20
static thread_local unsigned int Matlib_next_handle_guard = 0; // TLS 0x28

// Magic numbers: 2560 buckets and material entries, 16 deferred overrides.
static object_pool<matlib_entry, 2560, 1> Matlib_entry_server; // 0x01C2C500
static object_pool<matlib_bucket, 2560, 1> Matlib_bucket_pool; // 0x01CA4A20
static object_pool<rl_deferred_material_handle_override, 16, 1> Deferred_handle_override_pool; // 0x01C31F40
data_hash_table<mat_key_wrapper, matlib_bucket*, &hash_mat_key_wrapper, 5120, unsigned short> Matlib_entry_hash(false); // 0x01C371C8

extern bool rl_is_locked();

// 0x004D5D10 (:92)
bool mat_key_wrapper::operator==(const mat_key_wrapper& other_mat) const
{
    if (mat == other_mat.mat)
    {
        return true;
    }
    return mat->compare(*other_mat.mat, false);
}

// 0x004C2C60 (:151)
// rl_material_base::hash inlined (the hash is cached).
unsigned int hash_mat_key_wrapper(mat_key_wrapper key, unsigned int size)
{
    return key.mat->hash() % size;
}

// 0x00511340 (:158)
// The entries of every bucket come from Matlib_entry_server.
matlib_bucket::matlib_bucket()
{
    init(&Matlib_entry_server);
    m_override_material = nullptr;
    m_cur_material_instance = nullptr;
}

// 0x004C2C90 (:165)
void matlib_bucket::destroy()
{
    if (m_override_material != nullptr)
    {
        m_override_material->set_is_override(false);
        m_override_material = nullptr;
    }
    m_cur_material_instance = nullptr;
}

// 0x00511360 (:182)
void matlib_bucket::add_material(rl_material_base* mat_p)
{
    matlib_entry* entry_p = alloc(0);
    if (entry_p == nullptr)
    {
        return;
    }
    entry_p->mat = mat_p;
    if (m_cur_material_instance == nullptr)
    {
        m_cur_material_instance = entry_p;
    }
}

// 0x00527AC0 (:204)
// Removes every entry of the material; the first remaining entry becomes current.
void matlib_bucket::remove_material(rl_material_base* mat_p)
{
    thread_access.enter();
    matlib_entry* entry_p = get_first(0);
    while (entry_p != nullptr)
    {
        matlib_entry* next_p = get_next(entry_p);
        if (entry_p->mat == mat_p)
        {
            if (m_cur_material_instance == entry_p)
            {
                m_cur_material_instance = nullptr;
            }
            free(entry_p);
        }
        entry_p = next_p;
    }
    thread_access.leave();
    if (m_cur_material_instance == nullptr)
    {
        m_cur_material_instance = get_first(0);
    }
}

// 0x004C2CB0 (:230)
void matlib_bucket::set_override_material(rl_material_base* mat_p)
{
    if (m_override_material != nullptr)
    {
        m_override_material->set_is_override(false);
    }
    m_override_material = mat_p;
    if (mat_p != nullptr)
    {
        mat_p->set_is_override(true);
    }
}

// 0x00511390 (:247)
matlib_bucket* alloc_bucket()
{
    matlib_bucket* bucket_p = Matlib_bucket_pool.alloc(0);
    if (bucket_p != nullptr)
    {
        // Magic number: the guard in bits 16-31.
        bucket_p->m_handle = ((unsigned int)Matlib_bucket_pool.get_index(bucket_p) & 0xFFFF) | Matlib_next_handle_guard++ << 16;
    }
    return bucket_p;
}

// 0x00527B80 (:263)
void free_bucket(matlib_bucket* bucket_p)
{
    bucket_p->destroy();
    Matlib_bucket_pool.free(bucket_p);
}

// 0x004E8F20 (:275)
matlib_bucket* get_bucket_by_handle(unsigned int mat_handle)
{
    const unsigned int index = mat_handle & 0xFFFF;
    if (index >= Matlib_bucket_pool.get_capacity())
    {
        return nullptr;
    }
    matlib_bucket* bucket_p = Matlib_bucket_pool.get_entry(index);
    return bucket_p != nullptr && bucket_p->m_handle == mat_handle ? bucket_p : nullptr;
}

// 0x004E8F50 (:286)
// The current material of a handle, or its override (with respect_override).
rl_material_base* resolve_material_handle(unsigned int mat_h, bool respect_override)
{
    matlib_bucket* bucket_p = get_bucket_by_handle(mat_h);
    if (bucket_p == nullptr || bucket_p->m_cur_material_instance == nullptr || bucket_p->m_cur_material_instance->mat == nullptr)
    {
        return nullptr;
    }
    if (respect_override && bucket_p->m_override_material != nullptr)
    {
        return bucket_p->m_override_material;
    }
    return bucket_p->m_cur_material_instance->mat;
}

// 0x004E8FA0 (:309)
void rl_material_library_system_init()
{
    Matlib_entry_hash.clear();
}

// 0x004C2CD0 (:316)
void rl_material_library_system_shutdown()
{
}

// 0x00527BB0 (:330)
// Adds the material to the bucket of an equal material (or a new bucket) and returns the bucket handle.
unsigned int rl_material_library_register(rl_material_base* mat_p)
{
    matlib_bucket* bucket_p = nullptr;
    mat_key_wrapper key;
    key.mat = mat_p;
    if (Matlib_entry_hash.get(key, bucket_p) && bucket_p != nullptr)
    {
        mat_key_wrapper current_key;
        current_key.mat = bucket_p->m_cur_material_instance->mat;
        Matlib_entry_hash.remove(current_key);
    }
    else
    {
        bucket_p = alloc_bucket();
        if (bucket_p == nullptr)
        {
            keen::debug::nativePrintArgs("rl_material_library_register: alloc_bucket() failed. bucket_p->m_handle: %u , mat_p: 0x%x\n");
            return 0xFFFFFFFF;
        }
    }
    bucket_p->add_material(mat_p);
    mat_p->set_registered(true);
    if (bucket_p->m_cur_material_instance != nullptr)
    {
        mat_key_wrapper current_key;
        current_key.mat = bucket_p->m_cur_material_instance->mat;
        // hash_table::add plus the entry data (data_hash_table::add).
        Matlib_entry_hash.insert_key_data(current_key, bucket_p);
        return bucket_p->m_handle;
    }
    keen::debug::nativePrintArgs("rl_material_library_register: this bucket has no valid material instance for some reason, time to kill it. bucket_p->m_handle: %u , mat_p: 0x%x\n");
    free_bucket(bucket_p);
    return 0xFFFFFFFF;
}

// 0x00527CA0 (:385)
// Removes the material from its bucket; the bucket is re-keyed by its new current material or freed when empty.
void rl_material_library_unregister(rl_material_base* mat_p)
{
    if (mat_p == nullptr)
    {
        keen::debug::nativePrintArgs("rl_material_library_unregister: failed! mat_p == nullptr\n");
        return;
    }
    matlib_bucket* bucket_p = nullptr;
    mat_key_wrapper key;
    key.mat = mat_p;
    Matlib_entry_hash.get(key, bucket_p);
    if (bucket_p == nullptr)
    {
        keen::debug::nativePrintArgs("rl_material_library_unregister: failed! bucket_p == nullptr. mat_p: 0x%x\n");
        return;
    }
    mat_key_wrapper current_key;
    current_key.mat = bucket_p->m_cur_material_instance->mat;
    Matlib_entry_hash.remove(current_key);
    bucket_p->remove_material(mat_p);
    mat_p->set_registered(false);
    if (bucket_p->m_cur_material_instance != nullptr)
    {
        current_key.mat = bucket_p->m_cur_material_instance->mat;
        Matlib_entry_hash.insert_key_data(current_key, bucket_p);
        return;
    }
    free_bucket(bucket_p);
}

// 0x0054BD30 (:431)
// The deferred form (RLDO_OVERRIDE_MATERIAL_HANDLE).
bool rl_material_library_override_handle(rl_deferred_material_handle_override* def_op_p)
{
    if (def_op_p == nullptr)
    {
        return false;
    }
    const bool result = rl_material_library_override_handle(def_op_p->mat_h, def_op_p->mat_p);
    if ((unsigned int)Deferred_handle_override_pool.get_index(def_op_p) < Deferred_handle_override_pool.get_capacity())
    {
        Deferred_handle_override_pool.free(def_op_p);
    }
    return result;
}

// 0x0053F120 (:447)
// Sets the override material of a handle; while the renderer is locked the change is deferred to rl_unlock.
bool rl_material_library_override_handle(unsigned int mat_h, rl_material_base* mat_p)
{
    if (rl_is_locked())
    {
        rl_deferred_material_handle_override* data_p = Deferred_handle_override_pool.alloc(0);
        if (data_p == nullptr)
        {
            return false;
        }
        data_p->mat_h = mat_h;
        data_p->mat_p = mat_p;
        rl_add_deferred_op(RLDO_OVERRIDE_MATERIAL_HANDLE, data_p);
        return true;
    }
    matlib_bucket* bucket_p = get_bucket_by_handle(mat_h);
    if (bucket_p == nullptr)
    {
        return false;
    }
    bucket_p->set_override_material(mat_p);
    return true;
}

// 0x005113F0 (:478)
// The override handle (rl_material_library_set_override) replaces the material.
void rl_material_library_set_render_material(rl_renderer& renderer, unsigned int mat_h, int pass_index)
{
    if (Matlib_override_handle != 0xFFFFFFFF)
    {
        pass_index = 0;
        mat_h = Matlib_override_handle;
    }
    if (Matlib_render_material_handle != 0xFFFFFFFF)
    {
        rl_material_library_clear_render_material(renderer);
    }
    Matlib_render_material_handle = mat_h;
    Matlib_render_material_pass = pass_index;
    rl_material_base* mat_p = resolve_material_handle(mat_h, true);
    if (mat_p != nullptr)
    {
        mat_p->set_as_render_material(renderer, renderer.get_state_store(), pass_index);
    }
}

// 0x004E8FB0 (:500)
void rl_material_library_clear_render_material(rl_renderer& renderer)
{
    const unsigned int mat_h = Matlib_render_material_handle;
    if (mat_h == 0xFFFFFFFF)
    {
        return;
    }
    const int pass_index = Matlib_render_material_pass;
    Matlib_render_material_handle = 0xFFFFFFFF;
    Matlib_render_material_pass = 0;
    rl_material_base* mat_p = resolve_material_handle(mat_h, true);
    if (mat_p != nullptr)
    {
        mat_p->cleanup_render_material(renderer, renderer.get_state_store(), pass_index);
    }
}

// 0x004E9040 (:523)
bool rl_material_library_material_is_transparent(unsigned int mat_h)
{
    rl_material_base* mat_p = resolve_material_handle(mat_h, true);
    return mat_p != nullptr && mat_p->get_is_base_transparent();
}

// 0x004E9090 (:536)
bool rl_material_library_material_has_flags(unsigned int mat_h, int pass_index, unsigned int flags, bool depthPass)
{
    rl_material_base* mat_p = resolve_material_handle(mat_h, true);
    return mat_p != nullptr && mat_p->get_has_flags(pass_index, flags, depthPass);
}

// 0x004E90F0 (:555)
// Clones the current material (not the override).
rl_material_base* rl_material_library_clone(unsigned int mat_h, mempool_base* pool)
{
    rl_material_base* mat_p = resolve_material_handle(mat_h, false);
    if (mat_p == nullptr)
    {
        return nullptr;
    }
    return mat_p->clone(pool);
}

// 0x004C2CE0 (:567)
void rl_material_library_set_override(unsigned int mat_h)
{
    Matlib_override_handle = mat_h;
}

// 0x004C2D00 (:572)
unsigned int rl_material_library_get_override()
{
    return Matlib_override_handle;
}

// 0x004E9140 (:577)
int rl_material_library_get_num_passes(unsigned int mat_h, bool depthPass)
{
    const unsigned int handle = Matlib_override_handle != 0xFFFFFFFF ? Matlib_override_handle : mat_h;
    rl_material_base* mat_p = resolve_material_handle(handle, true);
    if (mat_p == nullptr)
    {
        return 0;
    }
    return mat_p->get_num_passes(depthPass);
}

// 0x004E91B0 (:594)
bool rl_material_library_can_combine_drawcalls(unsigned int mat_h)
{
    const unsigned int handle = Matlib_override_handle != 0xFFFFFFFF ? Matlib_override_handle : mat_h;
    rl_material_base* mat_p = resolve_material_handle(handle, true);
    if (mat_p == nullptr)
    {
        return true;
    }
    return mat_p->can_combine_drawcalls();
}

// 0x004E9220 (:612)
rl_material_base* rl_material_library_get_material(unsigned int mat_h)
{
    return resolve_material_handle(mat_h, false);
}
