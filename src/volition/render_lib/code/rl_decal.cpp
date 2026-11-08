// Original: volition/render_lib/code/rl_decal.cpp
//
// rl_decal: decals on renderable instances (the base class and the decal registry).
//
// VFX.md FX7: ported whole. The registry (Decal_hash_table) maps a decal handle to the ring of decals created under it
// (sys_prev/sys_next; one handle can put a decal on several instances, for example every sub mesh a blast touches). A
// handle reserved while render_lib is locked maps to -1 until rl_unlock creates its decals (rl_decal_maybe_defer_creation,
// rl_decal_process_deferred_create). Each owner keeps its own decal list (next/prev, rl_renderable_instance::add_decal);
// rl_decal_process_decals fades the list's decals out at the end of their lifetime and releases them.
//
// Decal flags (m_decal_flags; names inferred): 8 = double sided (no culling), 0x10 = fading out.

#include "volition/render_lib/code/rl_decal.h"

#include "volition/render_lib/code/rl_decal_internal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_planar_decal.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"
#include "keen/base/trace.hpp"

// The handle of a reservation whose decals are not created yet.
static rl_decal* const Decal_reserved = (rl_decal*)0xFFFFFFFF;

// ---- file-static globals
// Magic number: 2,048 decal handles.
static data_hash_table<unsigned int, rl_decal*, &hash_uint, 2048, unsigned short> Decal_hash_table(false); // 0x01C1A9F0
// The decal creations queued while render_lib is locked. Magic number: 256 at once.
static object_pool<rl_decal_deferred_info, 256, 1> Deferred_create_pool; // 0x01CC5C70
static unsigned int Decal_id; // 0x01CD2864

// 0x004BCC80 (rl_decal_internal.h:30)
rl_decal_deferred_info::rl_decal_deferred_info()
{
    m_inst = nullptr;
}

rl_decal_deferred_info::~rl_decal_deferred_info()
{
}

// 0x004E3F80 (:62)
rl_decal::rl_decal()
{
}

// 0x004BD9D0 (:66)
rl_decal::~rl_decal()
{
}

// 0x004E3FD0 (:74)
// The flags, color and timing of the parameters; fully opaque, born now.
void rl_decal::init(const rl_decal_parameters* params)
{
    m_decal_flags = 0;
    if (params->m_double_sided)
    {
        m_decal_flags = RLDF_DOUBLE_SIDED;
    }
    m_color = params->m_color;
    m_fade_pct = 1.0f;
    m_birth_time = (int)rl_render_lib::get_instance()->get_frametime_total_ms();
    m_fade_time = params->m_fade_time;
    m_lifetime = params->m_lifetime;
}

// 0x004BD9E0 (:87)
void rl_decal::set_owner(rl_renderable_instance* owner)
{
    m_owner = owner;
}

// 0x004E4030 (:92)
// Once: the lifetime ends fadetime_ms from now, over which the decal fades.
void rl_decal::fade_out(int fadetime_ms)
{
    if ((m_decal_flags & RLDF_FADING_OUT) == 0)
    {
        m_fade_time = fadetime_ms;
        m_lifetime = ((int)rl_render_lib::get_instance()->get_frametime_total_ms() - m_birth_time) + fadetime_ms;
        m_decal_flags |= RLDF_FADING_OUT;
    }
}

// 0x004E4060 (:101)
float rl_decal::get_age_ms() const
{
    return (float)(unsigned int)((int)rl_render_lib::get_instance()->get_frametime_total_ms() - m_birth_time);
}

// 0x004E3F90 (.h:68)
unsigned int rl_decal::get_material() const
{
    return 0xFFFFFFFF;
}

// 0x005236D0 (:110)
// A new handle (the next id) for the decal, or the decal added to the ring of an existing (or reserved) handle. A
// null decal only reserves a new handle. The decal's ring starts as itself.
// Notes (decompiled behaviour, kept as is): a full table drops the entry silently and still returns the handle.
unsigned int rl_decal_register(rl_decal* decal_p, unsigned int existing_handle)
{
    unsigned int handle = existing_handle;
    if (existing_handle == 0xFFFFFFFF)
    {
        handle = Decal_id++;
        if (decal_p == nullptr)
        {
            Decal_hash_table.insert_key_data(handle, Decal_reserved);
            return handle;
        }
        decal_p->sys_next = decal_p;
        decal_p->sys_prev = decal_p;
        Decal_hash_table.insert_key_data(handle, decal_p);
    }
    else
    {
        if (decal_p == nullptr)
        {
            return existing_handle;
        }
        rl_decal** head_pp = Decal_hash_table.get_ptr(existing_handle);
        if (head_pp == nullptr)
        {
            decal_p->sys_next = decal_p;
            decal_p->sys_prev = decal_p;
            Decal_hash_table.insert_key_data(handle, decal_p);
        }
        else if (*head_pp == Decal_reserved || *head_pp == nullptr)
        {
            decal_p->sys_next = decal_p;
            decal_p->sys_prev = decal_p;
            *head_pp = decal_p;
        }
        else
        {
            // At the end of the ring (before the head).
            decal_p->sys_prev = (*head_pp)->sys_prev;
            decal_p->sys_next = *head_pp;
            (*head_pp)->sys_prev->sys_next = decal_p;
            (*head_pp)->sys_prev = decal_p;
        }
    }
    decal_p->set_decal_handle(handle);
    return handle;
}

// 0x0050C510 (:163)
// Out of its handle's ring; the handle goes when its ring is empty (or only reserved).
void rl_decal_unregister(rl_decal* decal_p)
{
    if (decal_p == nullptr)
    {
        return;
    }
    const unsigned int handle = decal_p->get_decal_handle();
    rl_decal** head_pp = Decal_hash_table.get_ptr(handle);
    if (head_pp == nullptr)
    {
        return;
    }
    if (*head_pp != Decal_reserved)
    {
        if (*head_pp == decal_p)
        {
            if (decal_p->sys_next == decal_p)
            {
                decal_p->sys_next = nullptr;
                decal_p->sys_prev = nullptr;
                *head_pp = nullptr;
                Decal_hash_table.remove(handle);
                return;
            }
            *head_pp = decal_p->sys_next;
        }
        decal_p->sys_prev->sys_next = decal_p->sys_next;
        decal_p->sys_next->sys_prev = decal_p->sys_prev;
        decal_p->sys_next = nullptr;
        decal_p->sys_prev = nullptr;
    }
    if (*head_pp == nullptr || *head_pp == Decal_reserved)
    {
        Decal_hash_table.remove(handle);
    }
}

// 0x00539080 (:189)
// Every decal of the handle off its owner and released, and the handle removed; deferred while render_lib is locked.
void rl_decal_release(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_DECAL, (void*)handle);
        return;
    }
    rl_decal* head = nullptr;
    Decal_hash_table.get(handle, head);
    if (head != Decal_reserved)
    {
        rl_decal* decal_p = head;
        while (decal_p != nullptr)
        {
            rl_decal* next = decal_p->sys_next != head ? decal_p->sys_next : nullptr;
            if (decal_p->get_owner() != nullptr)
            {
                decal_p->get_owner()->remove_decal(decal_p);
            }
            if (head == decal_p && decal_p->sys_next == decal_p)
            {
                head = nullptr;
            }
            else
            {
                if (head == decal_p)
                {
                    head = decal_p->sys_next;
                }
                decal_p->sys_prev->sys_next = decal_p->sys_next;
                decal_p->sys_next->sys_prev = decal_p->sys_prev;
            }
            decal_p->sys_prev = nullptr;
            decal_p->sys_next = nullptr;
            decal_p->release();
            decal_p = next;
        }
    }
    Decal_hash_table.remove(handle);
}

// 0x00539140 (:226)
// Only planar decals exist.
rl_decal* rl_decal_create(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient)
{
    if (params == nullptr)
    {
        return nullptr;
    }
    if (params->m_type != RLDT_PLANAR)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    return rl_planar_decal_create(params, parent_pos, parent_orient);
}

// 0x00539190 (:253)
// While render_lib is locked: the handle reserved (a new one unless one is given), the parameters queued for
// rl_unlock's add_decal on the instance; true. Otherwise false (the caller creates the decal now).
// Notes (decompiled behaviour, kept as is): true even when the queue is full and the decal is dropped.
bool rl_decal_maybe_defer_creation(unsigned int& reserved_decal_handle, const rl_decal_parameters* params, rl_renderable_instance* inst_p)
{
    if (params == nullptr || inst_p == nullptr || !rl_is_locked())
    {
        return false;
    }
    unsigned int handle = reserved_decal_handle;
    if (reserved_decal_handle == 0xFFFFFFFF)
    {
        handle = Decal_id++;
        Decal_hash_table.insert_key_data(handle, Decal_reserved);
    }
    reserved_decal_handle = handle;
    rl_decal_deferred_info* info = Deferred_create_pool.alloc(0);
    if (info != nullptr)
    {
        info->m_inst = inst_p;
        info->m_params = *params;
        info->m_reserved_decal_handle = reserved_decal_handle;
        rl_add_deferred_op(RLDO_CREATE_DECAL, info);
    }
    return true;
}

// 0x005237C0 (:279)
void rl_decal_process_deferred_create(rl_decal_deferred_info* info)
{
    if (info != nullptr && info->m_inst != nullptr)
    {
        info->m_inst->add_decal(&info->m_params, info->m_reserved_decal_handle);
        Deferred_create_pool.free(info);
    }
}

// 0x0050C590 (:287)
void rl_decal_setup_decal_for_render(rl_renderer& renderer, rl_decal* decal_p)
{
    if (decal_p == nullptr)
    {
        return;
    }
    if (decal_p->get_type() == RLDT_PLANAR)
    {
        rl_planar_decal_setup_decal_for_render(renderer, (rl_planar_decal*)decal_p);
        return;
    }
    VLIB_CRITICAL_FAILURE();
}

// 0x0050C5C0 (:302)
void rl_decal_setup_decal_instance_for_render(rl_renderer& renderer, rl_decal* decal_p)
{
    if (decal_p == nullptr)
    {
        return;
    }
    if (decal_p->get_type() == RLDT_PLANAR)
    {
        rl_planar_decal_setup_decal_instance_for_render(renderer, (rl_planar_decal*)decal_p);
        return;
    }
    VLIB_CRITICAL_FAILURE();
}

// 0x004BD9F0 (:316)
// A planar decal's box: half its width, length and depth around the origin.
void rl_decal_get_local_aabb(rl_aabb& local_aabb, const rl_decal_parameters* decal_params)
{
    if (decal_params == nullptr)
    {
        return;
    }
    if (decal_params->m_type != RLDT_PLANAR)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    const rl_planar_decal_parameters& planar = decal_params->m_planar_params;
    local_aabb.max.x = planar.m_width * 0.5f;
    local_aabb.max.y = planar.m_length * 0.5f;
    local_aabb.max.z = planar.m_depth * 0.5f;
    local_aabb.min.x = local_aabb.max.x * -1.0f;
    local_aabb.min.y = local_aabb.max.y * -1.0f;
    local_aabb.min.z = local_aabb.max.z * -1.0f;
}

// 0x0050C5F0 (:333)
// The color of the handle's decals (only those of one generic instance unless gen_handle is -1).
void rl_decal_update_color(unsigned int handle, const rl_color_float& new_color, unsigned int gen_handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    rl_decal* head = nullptr;
    Decal_hash_table.get(handle, head);
    if (head == Decal_reserved)
    {
        return;
    }
    rl_decal* decal_p = head;
    while (decal_p != nullptr)
    {
        if (gen_handle == 0xFFFFFFFF || decal_p->get_generic_ri_handle() == gen_handle)
        {
            decal_p->update_color(new_color);
        }
        decal_p = decal_p->sys_next;
        if (decal_p == head)
        {
            return;
        }
    }
}

// 0x0050C660 (:353)
// rl_decal::fade_out on the handle's decals (only those of one generic instance unless gen_handle is -1).
void rl_decal_fade_out(unsigned int handle, int fadetime_ms, unsigned int gen_handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    rl_decal* head = nullptr;
    Decal_hash_table.get(handle, head);
    if (head == Decal_reserved)
    {
        return;
    }
    rl_decal* decal_p = head;
    while (decal_p != nullptr)
    {
        if (gen_handle == 0xFFFFFFFF || decal_p->get_generic_ri_handle() == gen_handle)
        {
            decal_p->fade_out(fadetime_ms);
        }
        decal_p = decal_p->sys_next;
        if (decal_p == head)
        {
            return;
        }
    }
}

// 0x0050C6F0 (:374)
// The fade of an owner's decals with a finite lifetime (opaque until fade_time before the end); expired ones are
// taken off the owner, unregistered and released.
void rl_decal_process_decals(rl_decal** decal_list)
{
    rl_decal* decal_p = *decal_list;
    while (decal_p != nullptr)
    {
        const int lifetime = decal_p->m_lifetime;
        // The next decal of the owner's ring, read before this one can go.
        rl_decal* next = decal_p->next != *decal_list ? decal_p->next : nullptr;
        if (lifetime != -1)
        {
            const int age = (int)rl_render_lib::get_instance()->get_frametime_total_ms() - decal_p->m_birth_time;
            const int fade_start = lifetime - decal_p->m_fade_time;
            if (age < lifetime)
            {
                if (fade_start < age)
                {
                    decal_p->update_fade_pct(1.0f - (float)(age - fade_start) / (float)decal_p->m_fade_time);
                }
                else
                {
                    decal_p->update_fade_pct(1.0f);
                }
            }
            else
            {
                if (decal_p->get_owner() != nullptr)
                {
                    decal_p->get_owner()->remove_decal(decal_p);
                }
                rl_decal_unregister(decal_p);
                decal_p->release();
            }
        }
        decal_p = next;
    }
}
