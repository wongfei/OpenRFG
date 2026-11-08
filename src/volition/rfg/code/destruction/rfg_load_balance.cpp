// Original: volition/rfg/code/destruction/rfg_load_balance.cpp
//
// Destruction load balancing (decompiled; header comment in rfg_load_balance.h). Compiled in
// rfg_destruction_unity.cpp.
//
// RFG_load (0..1) measures the destruction load: the recent new movers (Recent_history_objects / 100, decaying 5% a
// frame) plus the last Havok step's time (gamestate_gp_last_havok_sim_time / 26250), 1 when Havok runs out of memory. In
// single player, every 1-3 s (sooner under load) up to 10-30 removable movers and debris go away (general movers
// first, then debris, then rfg_movers), destroyed when not visible and faded otherwise; a destruction gets fewer
// shards and fewer good quality ones (rfgl_desired_values). A multiplayer server instead keeps at most 10 moving and
// 20 resting synced movers (mp_destruction_sync_throttle).
//
// Notes (decompiled behaviour, kept as is):
// - rfgl_find_and_delete_* scan at most twice the number of objects to delete per call, from where the last call
//   stopped.

#include "volition/rfg/code/destruction/rfg_load_balance.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/vlib/code/math/rand.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"

#include <stdlib.h>

static const int MAX_THROTTLE_MOVERS = 128; // per list in mp_destruction_sync_throttle
static const float FULL_LOAD_SIM_TIME = 26250.0f; // gamestate_gp_last_havok_sim_time counted as full load
static const float FULL_LOAD_HISTORY = 100.0f; // recent new movers counted as full load

// One mover and its sync score in mp_destruction_sync_throttle. Placement inferred.
// size 0x8
struct mp_throttle_info
{
    int score; // 0x00
    object_mover* omp; // 0x04
};

// ---- file-static globals
static bool RFG_load_balance = true; // 0x016500FB
static int Max_quality_objects_per_frame = 10; // 0x01650340
static float Recent_history_decay_rate = 0.95f; // 0x01650344
static float Recent_quality_decay_rate = 0.95f; // 0x01650348
static int Max_mp_throttle_active_sync_movers = 10; // 0x0165034C
static int Max_mp_throttle_inactive_sync_movers = 20; // 0x01650350
static int Desired_destroyable_peak = 1600; // 0x01650354, destroyable render instances in use (of 2100) before extra movers go
static bool Rfgl_init; // 0x01EF827E
static float RFG_load; // 0x01EF829C
static float Recent_history_objects; // 0x01EF82AC
static float Recent_quality_objects; // 0x01EF82B0
// RFG_load_delete_stamp and RFG_load_*_index: rfg_destruction.cpp (earlier in the unity, also used there).

// 0x00715470
// The current load (see the file comment); out of Havok memory deletes at once.
static float rfgl_recalc_load()
{
    float load = Recent_history_objects / FULL_LOAD_HISTORY + (float)gamestate_gp_last_havok_sim_time() / FULL_LOAD_SIM_TIME;
    if (havok_get_status(HAVOK_SYSTEM_MEMORY) != HAVOK_STATUS_OK)
    {
        load = 1.0f;
        RFG_load_delete_stamp.set(0);
    }
    if (load - 0.0f < 0.0f)
    {
        load = 0.0f;
    }
    if (0.0f <= load - 1.0f)
    {
        load = 1.0f;
    }
    return load;
}

// 0x00715520
// LTCG: objp in ESI. A resting (or any, out of Havok memory) object at least 15 m from the camera: debris always,
// movers when removable (0x4), not fading already, without script point effects (unless out of memory) and, when
// large (0x2), not visible.
static bool rfgl_should_delete(object* objp)
{
    if (objp == nullptr || objp->obj_flags.destroyed)
    {
        return false;
    }
    const havok_system_status status = havok_get_status(HAVOK_SYSTEM_MEMORY);
    if (status == HAVOK_STATUS_OK && objp->havok_handle != 0xFFFFFFFF && havok_body_is_active(objp->havok_handle))
    {
        return false;
    }
    const float dist_sqr = vec_dist_squared(objp->pos, Rfg_camera.real_pos);
    if (status != HAVOK_STATUS_ERROR && dist_sqr < 225.0f) // 15 m
    {
        return false;
    }
    if (objp->get_obj_type() != OT_MOVER)
    {
        return true;
    }
    object_mover* mover = (object_mover*)objp;
    if ((mover->mover_flags & 4) == 0 || object_mover_is_fading_for_delete(mover))
    {
        return false;
    }
    if (status == HAVOK_STATUS_OK && mover->has_script_point_effect(0xFFFFFFFF, nullptr))
    {
        return false;
    }
    if ((mover->mover_flags & 2) != 0)
    {
        vector bmin;
        vector bmax;
        mover->get_bbox(bmin, bmax);
        if (game_render_is_object_visible(bmin, bmax))
        {
            return false;
        }
    }
    return true;
}

// 0x00724A60
// General movers (find_gm) or rfg_movers.
static int rfgl_find_and_delete_object_mover(int objects_to_delete, bool find_gm)
{
    int& index = find_gm ? RFG_load_gm_index : RFG_load_rm_index;
    const int num = find_gm ? World.get_num_objects<general_mover>() : World.get_num_objects<rfg_mover>();
    if (num < 1)
    {
        return 0;
    }
    if (num <= index)
    {
        index = 0;
    }
    int deleted = 0;
    const havok_system_status status = havok_get_status(HAVOK_SYSTEM_MEMORY);
    for (int scanned = 0; index < num && scanned < objects_to_delete * 2; scanned++)
    {
        object_mover* mover = find_gm ? (object_mover*)World.get_object<general_mover>(index) : (object_mover*)World.get_object<rfg_mover>(index);
        if (rfgl_should_delete(mover))
        {
            vector bmin;
            vector bmax;
            mover->get_bbox(bmin, bmax);
            if (status == HAVOK_STATUS_ERROR || !object_mover_is_visible(mover) || !game_render_is_object_visible(bmin, bmax))
            {
                World.destroy_object(mover, false);
            }
            else
            {
                object_mover_mark_for_fade(mover, int_rand_range(0, 2000));
            }
            if (++deleted == objects_to_delete)
            {
                return deleted;
            }
        }
        index++;
    }
    return deleted;
}

// 0x00724B90
static int rfgl_find_and_delete_debris_object(int objects_to_delete)
{
    const int num = World.get_num_objects<object_debris>();
    if (num < 1)
    {
        return 0;
    }
    if (num <= RFG_load_od_index)
    {
        RFG_load_od_index = 0;
    }
    int deleted = 0;
    const havok_system_status status = havok_get_status(HAVOK_SYSTEM_MEMORY);
    for (int scanned = 0; RFG_load_od_index < num && scanned < objects_to_delete * 2; scanned++)
    {
        object_debris* debris = World.get_object<object_debris>(RFG_load_od_index);
        if (rfgl_should_delete(debris))
        {
            vector bmin;
            vector bmax;
            debris->get_bbox(bmin, bmax);
            if (status == HAVOK_STATUS_ERROR || !game_render_is_object_visible(bmin, bmax))
            {
                World.destroy_object(debris, false);
            }
            else
            {
                object_debris_mark_for_fade(debris, int_rand_range(0, 2000), 1000, false);
            }
            if (++deleted == objects_to_delete)
            {
                return deleted;
            }
        }
        RFG_load_od_index++;
    }
    return deleted;
}

// 0x00724C90
// 10 objects at no load up to 30 at full load (or, for general movers, as many as the render instances in use exceed
// the desired peak: at 2100 in use a new shard gets no instance and is destroyed at once), again in 3 s (1 s at full
// load).
static void rfgl_maybe_delete_objects()
{
    if (!RFG_load_balance)
    {
        return;
    }
    const int count = 10 - (int)(RFG_load * -20.0f);
    RFG_load_delete_stamp.set(3000 - (int)(RFG_load * 2000.0f));
    if (count < 1)
    {
        return;
    }
    int movers = rl_destroyable_model_instance::get_objects_used() - Desired_destroyable_peak; // inlined
    if (movers < count)
    {
        movers = count;
    }
    const int deleted = rfgl_find_and_delete_object_mover(movers, true);
    if (deleted < count)
    {
        const int deleted_debris = rfgl_find_and_delete_debris_object(count - deleted);
        if (deleted + deleted_debris < count)
        {
            rfgl_find_and_delete_object_mover(count - (deleted + deleted_debris), false);
        }
    }
}

// 0x00715600
// Higher scores first.
static int throttle_info_compare(const void* arg1, const void* arg2)
{
    return ((const mp_throttle_info*)arg2)->score - ((const mp_throttle_info*)arg1)->score;
}

// 0x0071CE30
// LTCG: omp in ESI. Grows with mass, pieces (rfg_movers), size (each extent at least 1 m) and motion (x10).
static int compute_object_mover_throttle_score(object_mover* omp)
{
    const float mass = omp->get_mass();
    float pieces = 1.0f;
    if (omp->get_obj_type() == OT_MOVER && omp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover* rm = (rfg_mover*)omp;
        if (rm->rdd.is_loaded() && rm->rdd.get_data()->shape != nullptr)
        {
            pieces = (float)rm->rdd.get_data()->shape->num_existing_child_shapes;
        }
    }
    vector bmin;
    vector bmax;
    omp->get_local_bbox(bmin, bmax);
    float ex = bmax.x - bmin.x;
    float ey = bmax.y - bmin.y;
    float ez = bmax.z - bmin.z;
    if (0.0f <= 1.0f - ex)
    {
        ex = 1.0f;
    }
    if (0.0f <= 1.0f - ey)
    {
        ey = 1.0f;
    }
    const float extent_z = 0.0f <= 1.0f - ez ? 1.0f : ez;
    const float volume = ey * ex * extent_z;
    const float motion = havok_body_is_active(omp->havok_handle) ? 10.0f : 1.0f;
    return (int)sqrtf(pieces * mass * volume * motion);
}

// 0x0071CFD0
// LTCG: omp in EAX. A moving mover joins the active list; a resting one the inactive list (it may fade 3-5 s after it
// came to rest).
static void add_mover_to_list(object_mover* omp, int& active_sync_count, int max_sync_info_counts, mp_throttle_info* active_sync_info, int& inactive_sync_count, mp_throttle_info* inactive_sync_info)
{
    if (omp->havok_handle == 0xFFFFFFFF || omp->obj_flags.destroyed)
    {
        return;
    }
    if (havok_body_is_active(omp->havok_handle))
    {
        omp->inactive_fade_time.invalidate();
        if (active_sync_count < max_sync_info_counts)
        {
            mp_throttle_info& info = active_sync_info[active_sync_count++];
            info.score = compute_object_mover_throttle_score(omp);
            info.omp = omp;
        }
        return;
    }
    if (!omp->inactive_fade_time.valid())
    {
        omp->inactive_fade_time.set_rand(3000, 5000);
    }
    if (inactive_sync_count < max_sync_info_counts)
    {
        mp_throttle_info& info = inactive_sync_info[inactive_sync_count++];
        info.score = compute_object_mover_throttle_score(omp);
        info.omp = omp;
    }
}

// 0x00715610
// LTCG: max_active_counts in ECX.
static void throttle_get_max_mover_counts(int& max_active_counts, int& max_inactive_counts)
{
    max_active_counts = Max_mp_throttle_active_sync_movers;
    max_inactive_counts = Max_mp_throttle_inactive_sync_movers;
}

// 0x00724D30
// The synced movers (general movers with a destruction uid, movable non pristine rfg_movers that may fade; not those
// set aside by flag 0x800): the moving ones beyond the 10 best scored fade at once; resting ones fade once their rest
// timer is up (rfg_movers only when small), and beyond the 20 best scored.
static void mp_destruction_sync_throttle()
{
    if (multi_game_is_migrating())
    {
        return;
    }
    mp_throttle_info active[MAX_THROTTLE_MOVERS];
    mp_throttle_info inactive[MAX_THROTTLE_MOVERS];
    int num_active = 0;
    int num_inactive = 0;
    const int num_general = World.get_num_objects<general_mover>();
    for (int i = 0; i < num_general; i++)
    {
        general_mover* gm = World.get_object<general_mover>(i);
        if ((gm->mover_flags & 0x800) == 0 && gm->destruction_uid != 0xFFFFFFFF)
        {
            add_mover_to_list(gm, num_active, MAX_THROTTLE_MOVERS, active, num_inactive, inactive);
        }
    }
    const int num_rfg = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num_rfg; i++)
    {
        rfg_mover* rm = World.get_object<rfg_mover>(i);
        if ((rm->mover_flags & 0x800) == 0 && !rm->flags.pristine && !rm->flags.mp_no_fade_destroy && rm->get_move_type() != RFGM_FIXED)
        {
            add_mover_to_list(rm, num_active, MAX_THROTTLE_MOVERS, active, num_inactive, inactive);
        }
    }
    int max_active;
    int max_inactive;
    throttle_get_max_mover_counts(max_active, max_inactive); // inlined
    if (max_active < num_active)
    {
        qsort(active, num_active, sizeof(mp_throttle_info), throttle_info_compare);
        for (int i = max_active; i < num_active; i++)
        {
            object_mover_mark_for_fade(active[i].omp, 0);
        }
    }
    int num_faded = 0;
    for (int i = 0; i < num_inactive; i++)
    {
        object_mover* omp = inactive[i].omp;
        if (!omp->inactive_fade_time.elapsed())
        {
            continue;
        }
        if (omp->get_sub_type() == OT_SUB_MOVER_RFG && !rfg_dyn_is_shard_small((rfg_mover*)omp)) // inlined
        {
            continue;
        }
        object_mover_mark_for_fade(omp, 0);
        num_faded++;
    }
    if (max_inactive < num_inactive - num_faded)
    {
        int num_kept = num_inactive;
        if (0 < num_faded)
        {
            num_kept = 0;
            for (int i = 0; i < num_inactive; i++)
            {
                if (!object_mover_is_fading_for_delete(inactive[i].omp))
                {
                    inactive[num_kept++] = inactive[i];
                }
            }
        }
        qsort(inactive, num_kept, sizeof(mp_throttle_info), throttle_info_compare);
        for (int i = max_inactive; i < num_kept; i++)
        {
            object_mover_mark_for_fade(inactive[i].omp, 0);
        }
    }
}

// 0x00715630 (:641)
void rfgl_system_init()
{
    if (!Rfgl_init)
    {
        Rfgl_init = true;
    }
}

// 0x00715650 (:650)
void rfgl_system_shutdown()
{
    if (Rfgl_init)
    {
        Rfgl_init = false;
    }
}

// 0x00715670 (:659)
void rfgl_level_init_pre()
{
    RFG_load_rm_index = 0;
    RFG_load_gm_index = 0;
    RFG_load_od_index = 0;
    RFG_load_delete_stamp.set(3000);
}

// 0x007156A0 (:668)
void rfgl_level_shutdown()
{
}

// 0x007250D0 (:673)
// Not on multiplayer clients. The recent counts decay (quality at most half a frame's budget).
void rfgl_do_frame()
{
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    RFG_load = rfgl_recalc_load(); // inlined
    if (!Multiplayer)
    {
        if (RFG_load_delete_stamp.elapsed())
        {
            rfgl_maybe_delete_objects();
        }
    }
    else
    {
        mp_destruction_sync_throttle();
    }
    if (Recent_history_objects <= 1.0f)
    {
        Recent_history_objects = 0.0f;
    }
    else
    {
        Recent_history_objects = Recent_history_decay_rate * Recent_history_objects;
    }
    if (Recent_quality_objects <= 1.0f)
    {
        Recent_quality_objects = 0.0f;
        return;
    }
    Recent_quality_objects = Recent_quality_decay_rate * Recent_quality_objects;
    if (Recent_quality_objects - 0.0f < 0.0f)
    {
        Recent_quality_objects = 0.0f;
    }
    if (0.0f <= Recent_quality_objects - (float)Max_quality_objects_per_frame * 0.5f)
    {
        Recent_quality_objects = (float)Max_quality_objects_per_frame * 0.5f;
    }
}

// 0x00725290 (:706)
// Everything removable goes (outside Havok's critical operations).
void rfgl_emergency_cleanup()
{
    if (Havok_world->m_criticalOperationsLockCount != 0)
    {
        return;
    }
    RFG_load_gm_index = 0;
    RFG_load_rm_index = 0;
    RFG_load_od_index = 0;
    rfgl_find_and_delete_object_mover(World.get_num_objects<general_mover>(), true);
    rfgl_find_and_delete_object_mover(World.get_num_objects<rfg_mover>(), false);
    rfgl_find_and_delete_debris_object(World.get_num_objects<object_debris>());
}

// 0x007156B0 (:723)
// Removable movers count as recent; those of the good quality move types (normal, lite, ultra lite) count against the
// quality budget.
void rfgl_object_created(object_mover* objp)
{
    if (objp->obj_flags.destroyed || objp->get_obj_type() != OT_MOVER)
    {
        return;
    }
    const int sub_type = objp->get_sub_type();
    if ((sub_type != OT_SUB_MOVER_GENERAL && sub_type != OT_SUB_MOVER_RFG) || (objp->mover_flags & 4) == 0)
    {
        return;
    }
    const int move_type = sub_type == OT_SUB_MOVER_GENERAL ? ((general_mover*)objp)->move_type : ((rfg_mover*)objp)->get_move_type();
    Recent_quality_objects += move_type != -1 && move_type < RFGM_WORLD_ONLY ? 1.0f : 0.0f;
    Recent_history_objects += 1.0f;
}

// 0x00715750 (:757)
void rfgl_object_destroyed(object_mover* objp)
{
}

// 0x0071D080 (:773)
// For incoming_objects new shards: how many to delete (more under load and after much recent destruction, at least
// all beyond 20; heavy destruction, 0x1, and in single player far or unseen destruction, 0x20/0x40, count as load 1.5)
// and how many get a good move type (the frame's quality budget). Out of Havok memory all are deleted. Multiplayer
// has no quality shards and deletes nothing under 5 (at most 0xFFF).
void rfgl_desired_values(rfgl_balance_values& bvl, int incoming_objects, int flags)
{
    if (havok_get_status(HAVOK_SYSTEM_MEMORY) != HAVOK_STATUS_OK)
    {
        bvl.delete_count = incoming_objects;
        bvl.quality_count = 0;
        return;
    }
    if (!RFG_load_balance)
    {
        bvl.delete_count = 0;
        bvl.quality_count = 0;
    }
    else
    {
        const int quality_left = Max_quality_objects_per_frame - (int)Recent_quality_objects;
        float load = RFG_load;
        if ((flags & (Multiplayer ? 1 : 0x61)) != 0)
        {
            load = 1.5f;
        }
        int delete_count = (int)(Recent_history_objects * 0.015f * load * load * (float)incoming_objects);
        if (delete_count <= incoming_objects - 20)
        {
            delete_count = incoming_objects - 20;
        }
        bvl.delete_count = delete_count;
        bvl.quality_count = quality_left <= incoming_objects - delete_count ? quality_left : incoming_objects - delete_count;
    }
    if (Multiplayer)
    {
        bvl.quality_count = 0;
        if (bvl.delete_count < 5)
        {
            bvl.delete_count = 0;
        }
        if (0xFFE < bvl.delete_count)
        {
            bvl.delete_count = 0xFFF; // fits the network field
        }
    }
}

// 0x00715760 (:821)
void rfgl_render_debug()
{
}
