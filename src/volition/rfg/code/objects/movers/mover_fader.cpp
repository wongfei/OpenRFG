// Original: volition/rfg/code/objects/movers/mover_fader.cpp
//
// Mover faders (VFX.md FX8): a piece of a mover (a deleted shard, a regrown or repaired piece, a nano dissolve) drawn by
// its own mesh instance while it fades. A fader lives in Fader_pool (255) and in a ring per owner (Fader_hash, keyed by
// the owner's handle). The gameplay side (mover_fader_process) advances the fade and works out the world transform
// (relative to the owner with flag 1, animated towards the target with flag 0x10); the render side
// (mover_fader_update_render_all, an update render task) creates the mesh instance (deferred while render_lib is
// locked), sets its opacity (fading out with flag 4, in with flag 2; none with a material effect, flag 8) and
// transform, and frees finished faders. Flag 0x20 moves the mover's decals for the piece onto the fader, flag 0x40
// marks a fader destroyed without its callbacks.

#include "volition/rfg/code/objects/movers/mover_fader.h"

#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/hash_table.h"

class rl_renderer;

// ---- file-local types (rfg.pdb)
// size 0xD4
struct mover_fader
{
    mover_fader(); // 0x00A26470

    unsigned int handle; // 0x00
    unsigned int owner; // 0x04
    unsigned int uid; // 0x08
    mover_fader_info info; // 0x0C
    float fade_pct; // 0x90
    rl_mesh_instance* r_inst_p; // 0x94
    vector wpos; // 0x98
    matrix worient; // 0xA4
    unsigned int mat_fx_handle; // 0xC8
    mover_fader* next; // 0xCC
    mover_fader* prev; // 0xD0
};
static_assert(sizeof(mover_fader) == 0xD4, "mover_fader");

// ---- file-static globals
static data_hash_table<unsigned int,mover_fader*,&hash_uint,255,unsigned char> Fader_hash(false); // 0x01695658
static bool Mover_fader_init; // 0x0302F0DB
static unsigned int Mover_fader_uid; // 0x0302F114
static object_pool<mover_fader,255,1> Fader_pool; // 0x0302F230
static game_render_task Update_render_task_fader; // 0x0303CAC8

// 0x00A26470
mover_fader::mover_fader()
{
}

// ---- internal functions

// 0x00A133D0 (:81)
// Whether the piece at the transform is outside the view (game_render_cull_oriented_bounding_box).
// Unreferenced in rfg.exe (mover_fader_create inlines the same test).
static bool maybe_cull_fader(const vector& pos, const matrix& orient, const mover_fader_info* info)
{
    vector bmin;
    vector bmax;
    info->mesh_p->get_subpiece_local_bb((int)info->submesh_index, bmin, bmax);
    return game_render_cull_oriented_bounding_box(bmin, bmax, pos, orient);
}

// 0x00A381A0 (:90)
// The owner's decals of the piece move onto the fader's instance.
static void transfer_decals(mover_fader* fader_p)
{
    if (fader_p == nullptr)
    {
        return;
    }
    rfg_mover* mover = nullptr;
    if (fader_p->owner != 0xFFFFFFFF)
    {
        mover = World.object_cast<rfg_mover>(World.object_from_handle(fader_p->owner));
    }
    // DEVIATION: rfg.exe calls transfer_decals_to_fader on a null mover when the owner is gone (a null read); skipped.
    if (mover == nullptr)
    {
        return;
    }
    mover->transfer_decals_to_fader(fader_p->r_inst_p, fader_p->uid);
}

// 0x00A381E0 (:99)
// The fader's mesh instance (the piece's sub mesh with its material map), its decals (respect_decals and flag 0x20)
// and its material effect (flag 8, lasting the fade).
static bool create_ri_for_fader(mover_fader* fader_p, bool respect_decals)
{
    fader_p->r_inst_p = rl_mesh_instance::create();
    if (fader_p->r_inst_p == nullptr)
    {
        return false;
    }
    fader_p->r_inst_p->init(fader_p->info.mesh_p, fader_p->info.material_map_p, fader_p->info.submesh_index);
    fader_p->r_inst_p->calculate_local_aabb();
    if (respect_decals && (fader_p->info.flags & 0x20) != 0)
    {
        transfer_decals(fader_p);
    }
    if ((fader_p->info.flags & 8) != 0)
    {
        material_effect_parameters params;
        // Magic number: the fade duration in milliseconds (a double constant).
        params.lifetime_ms = (int)((double)fader_p->info.fade_duration * 1000.0);
        params.time_passed_ms = 0;
        params.flags = fader_p->info.mat_fx_flags;
        params.cb_data = nullptr;
        params.update_render_done_cb = nullptr;
        params.world_pos = ZERO_VECTOR;
        params.use_local_pos = false;
        params.use_time_in_w = true;
        fader_p->mat_fx_handle = material_effects_apply(fader_p->r_inst_p, fader_p->info.material_effect_id, &params);
    }
    return true;
}

// 0x00A8B130 (:129)
// The process callback (unless destroyed with flag 0x40), then the fader leaves its owner's ring (the ring's head moves
// on when it was the head).
static void destroy_fader_process(mover_fader* fader_p)
{
    if (fader_p->info.process_done_cb != nullptr && (fader_p->info.flags & 0x40) == 0)
    {
        fader_p->info.process_done_cb(fader_p->owner, fader_p->uid);
    }
    mover_fader* head = nullptr;
    Fader_hash.get(fader_p->owner, head);
    Fader_hash.remove(fader_p->owner);
    if (head == fader_p)
    {
        if (fader_p->next == fader_p)
        {
            fader_p->next = nullptr;
            fader_p->prev = nullptr;
            return;
        }
        head = fader_p->next;
    }
    if (fader_p->prev != nullptr)
    {
        fader_p->prev->next = fader_p->next;
    }
    if (fader_p->next != nullptr)
    {
        fader_p->next->prev = fader_p->prev;
    }
    fader_p->next = nullptr;
    fader_p->prev = nullptr;
    if (head != nullptr)
    {
        Fader_hash.insert_key_data(head->owner, head);
    }
}

// 0x00A8B200 (:148)
// The render callback (unless destroyed with flag 0x40), the material effect and the mesh instance go; the fader is
// freed.
static void destroy_fader_update_render(mover_fader* fader_p)
{
    if (fader_p->info.update_render_done_cb != nullptr && (fader_p->info.flags & 0x40) == 0)
    {
        fader_p->info.update_render_done_cb(fader_p->owner, fader_p->uid);
    }
    if (fader_p->mat_fx_handle != 0xFFFFFFFF)
    {
        material_effects_remove(fader_p->mat_fx_handle);
        fader_p->mat_fx_handle = 0xFFFFFFFF;
    }
    if (fader_p->r_inst_p != nullptr)
    {
        fader_p->r_inst_p->release();
        fader_p->r_inst_p = nullptr;
    }
    Fader_pool.free(fader_p);
}

// 0x00A8B270 (:170)
// Finished faders are destroyed; the others get their mesh instance (without decals when it was deferred), opacity and
// world transform.
static void mover_fader_update_render_all(void*, rl_renderer*)
{
    Fader_pool.thread_access.enter();
    mover_fader* next = nullptr;
    for (mover_fader* fader_p = Fader_pool.get_first(0); fader_p != nullptr; fader_p = next)
    {
        next = Fader_pool.get_next(fader_p);
        if (1.0f <= fader_p->fade_pct)
        {
            destroy_fader_update_render(fader_p);
            continue;
        }
        if (fader_p->r_inst_p == nullptr && !create_ri_for_fader(fader_p, false))
        {
            continue;
        }
        rl_mesh_instance* r_inst = fader_p->r_inst_p;
        if (r_inst == nullptr)
        {
            continue;
        }
        const unsigned int flags = fader_p->info.flags;
        if ((flags & 8) == 0)
        {
            float opacity = 1.0f;
            if ((flags & 4) != 0)
            {
                opacity = 1.0f - fader_p->fade_pct;
            }
            else if ((flags & 2) != 0)
            {
                opacity = fader_p->fade_pct;
            }
            r_inst->set_opacity(opacity);
        }
        r_inst->rl_scene_entity::set_position(fader_p->wpos);
        r_inst->rl_scene_entity::set_rotation(fader_p->worient);
        r_inst->update_partition_system();
    }
    Fader_pool.thread_access.leave();
}

// ---- public functions

// 0x00A13410 (:209)
// The transform between the start and the target at the fade: the position lerped, the orientation slerped (the
// start's quaternion flipped onto the target's hemisphere).
void calculate_animated_transform(mover_fader* fader_p, matrix& out_orient, vector& out_pos)
{
    const float t = fader_p->fade_pct;
    const float s = 1.0f - t;
    const mover_fader_info& info = fader_p->info;
    out_pos.set(info.pos.x * s + info.target_pos.x * t, info.pos.y * s + info.target_pos.y * t, info.pos.z * s + info.target_pos.z * t);
    quaternion from;
    from.set(info.orient);
    quaternion to;
    to.set(info.target_orient);
    if (quaternion_dot(from, to) < 0.0f)
    {
        from = quaternion(-from.x, -from.y, -from.z, -from.w);
    }
    quaternion result;
    pltind_slerp(result, from, to, fader_p->fade_pct);
    result.extract_matrix(out_orient);
}

// Not in the binary: Fader_hash emptied (the hash table clear, inlined in rfg.exe).
static void mover_fader_hash_reset()
{
    Fader_hash.clear();
}

// 0x00AA2320 (:234)
// Registers the update render task and empties the hash and the pool.
void mover_fader_system_init()
{
    if (Mover_fader_init)
    {
        return;
    }
    Mover_fader_init = true;
    Update_render_task_fader.m_callback = mover_fader_update_render_all;
    game_render_update_render_task_register(&Update_render_task_fader);
    mover_fader_hash_reset();
    Fader_pool.thread_access.enter();
    for (mover_fader* fader_p = Fader_pool.get_first(0); fader_p != nullptr; fader_p = Fader_pool.free(fader_p))
    {
    }
    Fader_pool.thread_access.leave();
}

// 0x00A8B510 (:250)
// Every fader's mesh instance is released and the fader freed (no callbacks, no material effect removal).
void mover_fader_level_shutdown()
{
    Fader_pool.thread_access.enter();
    mover_fader* next = nullptr;
    for (mover_fader* fader_p = Fader_pool.get_first(0); fader_p != nullptr; fader_p = next)
    {
        next = Fader_pool.get_next(fader_p);
        if (fader_p->r_inst_p != nullptr)
        {
            fader_p->r_inst_p->release();
            fader_p->r_inst_p = nullptr;
        }
        Fader_pool.free(fader_p);
    }
    Fader_pool.thread_access.leave();
    mover_fader_hash_reset();
}

// 0x00AA23D0 (:268)
// Notes (decompiled behaviour, kept as is): the update render task stays registered.
void mover_fader_system_shutdown()
{
    if (!Mover_fader_init)
    {
        return;
    }
    Mover_fader_init = false;
    mover_fader_hash_reset();
    Fader_pool.thread_access.enter();
    for (mover_fader* fader_p = Fader_pool.get_first(0); fader_p != nullptr; fader_p = Fader_pool.free(fader_p))
    {
    }
    Fader_pool.thread_access.leave();
}

// 0x00A8B610 (:284)
// Advances every fade by the frame time (a destroyed fader, flag 0x40, finishes at once); a finished fader leaves its
// owner's ring (the render side frees it), the others get their world transform.
// Notes (decompiled behaviour, kept as is): with flag 1 and no owner the world transform is not updated.
void mover_fader_process()
{
    Fader_pool.thread_access.enter();
    for (mover_fader* fader_p = Fader_pool.get_first(0); fader_p != nullptr; fader_p = Fader_pool.get_next(fader_p))
    {
        fader_p->fade_pct = Frametime / fader_p->info.fade_duration + fader_p->fade_pct;
        if ((fader_p->info.flags & 0x40) != 0)
        {
            fader_p->fade_pct = 1.0f;
        }
        if (!(fader_p->fade_pct < 1.0f))
        {
            destroy_fader_process(fader_p);
            continue;
        }
        vector pos = fader_p->info.pos;
        matrix orient = fader_p->info.orient;
        if ((fader_p->info.flags & 0x10) != 0)
        {
            calculate_animated_transform(fader_p, orient, pos);
        }
        if ((fader_p->info.flags & 1) == 0)
        {
            if ((fader_p->info.flags & 0x10) != 0)
            {
                fader_p->wpos = pos;
                fader_p->worient = orient;
            }
            continue;
        }
        object* owner = World.object_from_handle(fader_p->owner);
        if (owner == nullptr)
        {
            continue;
        }
        fader_p->worient = orient * owner->orient;
        fader_p->wpos = pos;
        fader_p->wpos.unrotate(owner->orient);
        fader_p->wpos.x = fader_p->wpos.x + owner->pos.x;
        fader_p->wpos.y = owner->pos.y + fader_p->wpos.y;
        fader_p->wpos.z = owner->pos.z + fader_p->wpos.z;
    }
    Fader_pool.thread_access.leave();
}

// 0x00A8B8C0 (:327)
// A fader for the piece (info copied) when it is in view: its world transform (relative to the owner with flag 1), its
// mesh instance now unless render_lib is locked, and its place at the end of the owner's ring. Returns its handle
// (pool index << 16 | a running uid), -1 when culled or the pool is full.
unsigned int mover_fader_create(unsigned int owner, unsigned int uid, mover_fader_info* c_info)
{
    vector wpos = ZERO_VECTOR;
    matrix worient = IDENTITY_MATRIX;
    object* owner_obj = World.object_from_handle(owner);
    if ((c_info->flags & 1) == 0 || owner_obj == nullptr)
    {
        worient = c_info->orient;
        wpos = c_info->pos;
    }
    else
    {
        worient = c_info->orient * owner_obj->orient;
        wpos = c_info->pos;
        wpos.unrotate(owner_obj->orient);
        wpos.y = owner_obj->pos.y + wpos.y;
        wpos.x = owner_obj->pos.x + wpos.x;
        wpos.z = owner_obj->pos.z + wpos.z;
    }
    vector bmin;
    vector bmax;
    c_info->mesh_p->get_subpiece_local_bb((int)c_info->submesh_index, bmin, bmax);
    if (game_render_cull_oriented_bounding_box(bmin, bmax, wpos, worient))
    {
        return 0xFFFFFFFF;
    }
    mover_fader* fader_p = Fader_pool.alloc(0);
    if (fader_p == nullptr)
    {
        return 0xFFFFFFFF;
    }
    fader_p->handle = (unsigned int)Fader_pool.get_index(fader_p) << 16 | (Mover_fader_uid & 0xFFFF);
    fader_p->uid = uid;
    Mover_fader_uid++;
    fader_p->owner = owner;
    fader_p->info = *c_info;
    fader_p->fade_pct = 0.0f;
    fader_p->mat_fx_handle = 0xFFFFFFFF;
    fader_p->worient = worient;
    fader_p->wpos = wpos;
    if (!rl_is_locked())
    {
        create_ri_for_fader(fader_p, true);
    }
    mover_fader* head = nullptr;
    Fader_hash.get(owner, head);
    Fader_hash.remove(owner);
    if (head == nullptr)
    {
        fader_p->next = fader_p;
        fader_p->prev = fader_p;
        head = fader_p;
    }
    else
    {
        fader_p->prev = head->prev;
        fader_p->next = head;
        head->prev->next = fader_p;
        head->prev = fader_p;
    }
    Fader_hash.insert_key_data(owner, head);
    return fader_p->handle;
}

// 0x00A8BBD0 (:381)
// The owner's faders of the uid are marked destroyed (flag 0x40, handle -1) and, unless render_lib is locked, taken
// down at once (else by the next process and update render).
// Notes (decompiled behaviour, kept as is): the walk continues from a fader taken down (its ring links are cleared, so
// the walk stops there).
void mover_fader_destroy(unsigned int owner, unsigned int uid)
{
    mover_fader* head = nullptr;
    Fader_hash.get(owner, head);
    mover_fader* fader_p = head;
    while (fader_p != nullptr)
    {
        if (fader_p->uid == uid)
        {
            fader_p->info.flags |= 0x40;
            fader_p->handle = 0xFFFFFFFF;
            if (!rl_is_locked())
            {
                destroy_fader_process(fader_p);
                destroy_fader_update_render(fader_p);
            }
        }
        fader_p = fader_p->next;
        if (fader_p == head)
        {
            return;
        }
    }
}

// 0x00A5EFD0 (:403)
// Every fader of the owner is marked destroyed (flag 0x40); process and update render take them down.
void mover_fader_destroy_all(unsigned int owner)
{
    mover_fader* head = nullptr;
    Fader_hash.get(owner, head);
    mover_fader* fader_p = head;
    while (fader_p != nullptr)
    {
        fader_p->info.flags |= 0x40;
        fader_p = fader_p->next;
        if (fader_p == head)
        {
            return;
        }
    }
}

// 0x00A5F020 (:415)
// The local transform of the owner's fader with the uid (animated with flag 0x10); unchanged when there is none.
void mover_fader_get_current_transform(unsigned int owner, unsigned int uid, matrix& rot, vector& pos)
{
    mover_fader* head = nullptr;
    Fader_hash.get(owner, head);
    mover_fader* fader_p = head;
    if (fader_p == nullptr)
    {
        return;
    }
    while (fader_p->uid != uid)
    {
        fader_p = fader_p->next;
        if (fader_p == head || fader_p == nullptr)
        {
            return;
        }
    }
    if ((fader_p->info.flags & 0x10) != 0)
    {
        calculate_animated_transform(fader_p, rot, pos);
        return;
    }
    rot = fader_p->info.orient;
    pos = fader_p->info.pos;
}
