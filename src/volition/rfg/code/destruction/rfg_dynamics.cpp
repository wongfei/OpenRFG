// Original: volition/rfg/code/destruction/rfg_dynamics.cpp
//
// The destruction dynamics (decompiled; header comment in rfg_dynamics.h). Compiled in rfg_destruction_unity.cpp.
//
// Damage reaches rfg_movers as collision events (RFG_dyn_collisions, guarded by Collision_event_wrapper): explosions,
// direct damage, link breaks, piece arrays, shatter, shape cutters, repairs, slices, shardify requests, destruction
// mode changes and multiplayer vehicle / destroy events, added by rfg_dyn_add_pre_collide_* and by Havok contacts
// (rfg_dyn_add_pre_collide_contact). rfg_dyn_do_collide_post sorts and applies them (rfg_dyn_apply_*): damage weakens
// links and anchors and breaks pieces off (shards: general movers for single pieces, new rfg_movers for groups), and
// the movers whose links changed are shardified: the pieces no longer anchored are collected into shards
// (rfg_dyn_collect_unanchored_pieces) and split off through the shardify jobs (setup, work, cleanup). Destroyed regions
// are recorded as rfg_destruction_events for the dynamic hole requests of the pathfinding.
//
// rfg_shard_info::flags, from the decompiled uses: 0x4 no mass / nano, 0x80 has broken links (impulse from the
// neighbours), 0x200 deleted pieces, 0x400 direct impulse.
//
// Notes (decompiled behaviour, kept as is):
// - get_subpiece_index_from_object and rfg_dyn_filter_destruction read the general mover's subpiece index without
//   checking that the object is a mover.

#include "volition/rfg/code/destruction/rfg_dynamics.h"

#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/glass.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_rbb_tree.h"
#include "volition/rfg/code/destruction/rfg_shape_cutter.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/lockfree_queues.h"
#include "volition/vlib/code/os/scheduler.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

#include <intrin.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MAX_RFG_DYN_COLLISIONS = 1024; // RFG_dyn_collisions
static const int MAX_DESTRUCTION_RESULTS = 20; // object_mover_destruction_results arrays
static const int MAX_SHARDIFY_DELETE_COUNT = 0xFFF; // fits the network field
static const int MAX_EVENT_DATA = 7; // rfg_collision_event_base data (direct damage, links, invulnerable handles)
static const int MAX_SP_ARRAY_KEYS = 15; // rfg_collision_event_base direct_sps
static const int MAX_CONTACT_IDS = 10; // rfg_collision_havok_base::id
static const float SHARD_SYNC_MIN_SIZE = 1.5f; // metres per axis for a synced shard in multiplayer
static const int MAX_SCRIPT_POINT_EFFECT_INSTS = 4; // level_object_inst_properties::m_script_point_effect_insts

// The movers created and destroyed by this frame's destruction, for the network. Placement inferred.
// size 0x1A0
struct object_mover_destruction_results
{
    int num_synced_general_movers_created_by_destruction; // 0x00
    int num_rfg_movers_created_by_destruction; // 0x04
    int num_rfg_movers_destroyed_by_destruction; // 0x08
    int num_shardify_delete_counts; // 0x0C
    general_mover* synced_general_movers_created_by_destruction[MAX_DESTRUCTION_RESULTS]; // 0x10
    rfg_mover* rfg_movers_created_by_destruction[MAX_DESTRUCTION_RESULTS]; // 0x60
    rfg_mover* rfg_movers_destroyed_by_destruction[MAX_DESTRUCTION_RESULTS]; // 0xB0
    shardify_delete_record shardify_delete_count_records[MAX_DESTRUCTION_RESULTS]; // 0x100
};

enum destruction_mode_type
{
    DM_PREGAME = 0,
    DM_NORMAL = 1,
    NUM_DESTRUCTION_MODES = 2,
};

// A material effect trail left by a moving shard. Placement inferred.
// size 0x14
struct rfg_destruction_trail
{
    rfg_destruction_trail(); // 0x00719780

    union
    {
        unsigned int trail_handle; // 0x00
        unsigned int effect_handle; // 0x00
    };
    rfg_effect_material* rfg_effect_material_p; // 0x04
    unsigned int owner_handle; // 0x08
    timestamp lifetime; // 0x0C
    unsigned int flags; // 0x10
};

// An rfg_mover queued for shardify. Placement inferred.
// size 0xC
struct rfg_mover_process_info
{
    rfg_mover* rmp; // 0x00
    unsigned int handle; // 0x04
    unsigned int destruction_uid; // 0x08
};

// ---- globals
object_pool<rfg_destruction_event, 1000, 1> RFG_destruction_events; // 0x01F3E600 (0x01069B40)
unsigned int* Destruction_event_history; // 0x01EF8274
fober_lock_free_queue* volatile Setup_queue; // 0x01EF828C, shardify jobs (process_setup / work / cleanup)
fober_lock_free_queue* volatile Work_queue; // 0x01EF8290
fober_lock_free_queue* volatile Cleanup_queue; // 0x01EF8294
volatile int Num_to_work_on; // 0x01EF8298

// ---- file-static globals
static bool Mp_allow_slice = true; // 0x016500FA
static int Last_regrow_framecount = -1; // 0x016502A4
static float Small_shard_length_limit = 3.0f; // 0x016502A8
static float Small_shard_volume_limit = 10.0f; // 0x016502AC
static float Too_small_shard_length_limit = 1.5f; // 0x016502B0
static float Too_small_shard_volume_limit = 0.1f; // 0x016502B4
static float Too_small_mass = 200.0f; // 0x016502B8
static unsigned int Link_area_damage_scalar = 15; // 0x016502BC
static float Torque_width_ratio_scalar = 0.22f; // 0x016502C0
static float Torque_height_ratio_scalar = 0.5f; // 0x016502C4
static float Shatter_effect_large_yield_pct = 2.5f; // 0x016502C8
static float Shatter_effect_medium_yield_pct = 1.25f; // 0x016502CC
static float Destruction_explosion_impulse_scalar = 1.75f; // 0x016502D0
static float Destruction_impact_impulse_scalar = 0.01f; // 0x016502D4
static float Destruction_direct_damage_impulse_scalar = 0.003f; // 0x016502D8
static float Shatter_impulse_scalar = 5.0f; // 0x016502DC
static float Shatter_radius_scalar = 0.04f; // 0x016502E0
static unsigned int Shatter_min_damage = 200000; // 0x016502E4
static unsigned int Slice_min_damage = 1000000; // 0x016502E8
static float Slice_tall_safe_zone = 0.2f; // 0x016502EC
static unsigned int Explode_min_damage = 5000000; // 0x016502F0
static float Explosion_directional_pct = 0.5f; // 0x016502F4
static unsigned int Repair_rate = 45; // 0x016502F8
static int Explosion_pct[3] = {52, 45, 3}; // 0x016502FC, small / medium / large explosion effects (percent)
static float RFG_blast_radius = -1.0f; // 0x01650308
static int RFG_uber_heavy_destruction_damage_multiplier = 3; // 0x0165030C
static int RFG_heavy_destruction_damage_multiplier = 2; // 0x01650310
static int RFG_uber_heavy_destruction_explosion_framecount = -1; // 0x01650314
static int RFG_uber_heavy_destruction_explosion_min_delay = 10; // 0x01650318
static int RFG_uber_heavy_destruction_slice_framecount = -1; // 0x0165031C
static int RFG_uber_heavy_destruction_slice_min_delay = 10; // 0x01650320
static int RFG_uber_heavy_destruction_explosion_chance = 5; // 0x01650324
static float Large_explosion_radius = 5.5f; // 0x01650328
static float Salvage_spawn_prob_factor = 5.0f; // 0x0165032C
static float Salvage_spawn_dead_prob = 0.25f; // 0x01650330
static float Shape_cutter_jitter_radius = 1.0f; // 0x01650334
static float Nano_jitter_radius_mult = 1.25f; // 0x01650338
static unsigned int Sim_thread_marks[16]; // 0x01E2BA10
static unsigned int General_mover_destruction_uid; // 0x01E38330
static unsigned int Common_mempool_mark; // 0x01E38334
static unsigned int Rfg_mover_destruction_uid; // 0x01E3833C
static rfg_mover* Rfg_mover_process_shardify_candidate_list[4000]; // 0x01E80208
static destruction_mode_type Destruction_mode; // 0x01E84088
static object_mover_destruction_results Mover_destruction_results; // 0x01E84090
static bool Allow_mp_vehicle_mover_collision; // 0x01EF8265
static int Num_rfg_dyn_collisions; // 0x01EF8268
static int Num_events_to_process_this_havok_step; // 0x01EF826C
static volatile unsigned int Rfg_mover_process_shardify_candidate_count; // 0x01EF8270
static unsigned short Destruction_frame; // 0x01EF8278
static bool Rfg_dyn_system_init; // 0x01EF827A
static bool Terminal_havok_step; // 0x01EF827B
static unsigned short Active_destruction_frame; // 0x01EF827C
static unsigned int Destruction_event_history_index; // 0x01EF8280
static unsigned int Destruction_event_history_running_total; // 0x01EF8284
static cs_wrapper Collision_event_wrapper(0); // 0x01F17350 (0x01069B60)
static object_pool<rfg_destruction_trail, 10, 1> Destruction_trail_pool; // 0x01F59180 (0x01069B80)
static rfg_collision_event RFG_dyn_collisions[MAX_RFG_DYN_COLLISIONS]; // 0x020C22F8
static tweak_table_item Tweak_table_item_Salvage_spawn_prob_factor("Salvage_spawn_prob_factor", &Salvage_spawn_prob_factor); // (0x01069BA0)
static tweak_table_item Tweak_table_item_Salvage_spawn_dead_prob("Salvage_spawn_dead_prob", &Salvage_spawn_dead_prob); // (0x01069BC0)

static __declspec(thread) char Rfg_dynamics_message[1024]; // TLS +0x38 (debug messages, never shown)

// Defined in rfg_mover.cpp (:172), which comes later in rfg_destruction_unity.cpp; the PDB gives it file static linkage,
// so the original declared it extern here and defined it static there (MSVC extension C4211).
extern object_pool<rfg_dynamic_link, 2400, 2> RFG_dynamic_link_pool;

// ---- rfg_collision_event (header inlines)
// 0x00711640 (:190)
// Records a contact point of the event's contact manager (at most 10); false when full.
bool rfg_collision_event::add_contact_info(hkpDynamicsContactMgr* mgr, unsigned short id)
{
    const unsigned char count = havok_base.count;
    if (count >= MAX_CONTACT_IDS)
    {
        return false;
    }
    if (mgr != nullptr && mgr == havok_base.mgr && id != 0xFFFF)
    {
        havok_base.id[count] = id;
        havok_base.count++;
    }
    return true;
}

// 0x00711690 (:204)
// Drops a contact point; the event is cancelled (flag 0x1) when its last contact goes away.
bool rfg_collision_event::remove_contact_info(hkpDynamicsContactMgr* mgr, unsigned short id)
{
    if (havok_base.mgr != mgr)
    {
        return false;
    }
    const unsigned char count = havok_base.count;
    for (unsigned int i = 0; (int)i < (int)havok_base.count; i++)
    {
        if (havok_base.id[i] == id)
        {
            const unsigned char last = count - 1;
            havok_base.count = last;
            if (last == 0)
            {
                collision_base.flags |= 1;
                return true;
            }
            if (i != last)
            {
                havok_base.id[i] = havok_base.id[last];
            }
            return true;
        }
    }
    return false;
}

// 0x00711710 (:229)
bool rfg_collision_event::is_first_event(hkpDynamicsContactMgr* mgr, unsigned short id)
{
    return havok_base.count != 0 && havok_base.mgr == mgr && havok_base.id[0] == id;
}

// ---- rfg_shard_info
// 0x00711750 (:283)
void rfg_shard_info::reset()
{
    mp = nullptr;
    q = 0xFFFF;
    num_subpieces = 0;
    mass = 0.0f;
    impulse.zero();
    impulse_pos.zero();
    average_pos.zero();
    bmin.set(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f); // FLT_MAX
    bmax.set(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
    flags = 0;
}

// 0x007117C0 (:297)
bool rfg_shard_info::valid()
{
    return 0 < num_subpieces;
}

// 0x007117D0 (:302)
// Appends a piece to the shard's circular list (through shard_next/shard_prev); the new piece becomes the head.
void rfg_shard_info::add_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (q == 0xFFFF)
    {
        sp->shard_next = index;
        sp->shard_prev = index;
    }
    else
    {
        objects[objects[q].shard_prev].shard_next = index;
        sp->shard_prev = objects[q].shard_prev;
        objects[q].shard_prev = index;
        sp->shard_next = q;
    }
    num_subpieces++;
    q = index;
}

// 0x007118B0 (:310)
void rfg_shard_info::remove_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp, bool clear_flags_and_impulse)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (q == index && sp->shard_next == index)
    {
        sp->shard_next = 0xFFFF;
        sp->shard_prev = 0xFFFF;
        q = 0xFFFF;
    }
    else
    {
        if (q == index)
        {
            q = sp->shard_next;
        }
        objects[sp->shard_prev].shard_next = sp->shard_next;
        objects[sp->shard_next].shard_prev = sp->shard_prev;
        sp->shard_next = 0xFFFF;
        sp->shard_prev = 0xFFFF;
    }
    num_subpieces--;
    if (clear_flags_and_impulse)
    {
        sp->flags &= ~0x148u;
        sp->impulse_info_index = 0xFFFF;
    }
}

// 0x007238E0 (:326)
// Sums the shard's mass and bounds and averages its impulse: pieces with an impulse of their own (direct hits) use it;
// pieces whose links broke (0x800) take the average impulse of their still-linked neighbours when flagged 0x80000000
// (beyond the links they still have). Pieces deleted (0x100) or nano (0x80) mark the shard. The impulse is divided by
// the square root of the contributors, the positions by their count.
void rfg_shard_info::finalize(rfg_destroyable_shape* shape)
{
    int num_impulses = 0;
    int num_positions = 0;
    if (q != 0xFFFF)
    {
        rfg_subpiece* objects = shape->get_object_buffer();
        rfg_subpiece* sp = &objects[q];
        while (sp != nullptr)
        {
            const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
            const rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
            const shard_impulse_info* info = rfg_shape_get_shard_impulse_info(sp); // inlined
            if (info != nullptr)
            {
                impulse += info->impulse_dir;
                impulse_pos += info->impulse_pos;
                average_pos += sp_base->pos;
                num_impulses++;
                num_positions++;
                flags |= 0x400;
            }
            else if ((sp->flags & 0x800) != 0)
            {
                num_positions++;
                average_pos += sp_base->pos;
                flags |= 0x80;
                if ((sp->flags & 0x80000000) != 0)
                {
                    int num_neighbours = 0;
                    vector dir = ZERO_VECTOR;
                    vector dir_pos = ZERO_VECTOR;
                    if (sp->num_links < sp_base->num_links)
                    {
                        const unsigned short* links = sp->links.to_pointer();
                        for (unsigned int l = sp->num_links; (int)l < (int)sp_base->num_links; l++)
                        {
                            const rfg_link_base* link_base = shape->get_link_base(links[l]);
                            int other = link_base->obj[0];
                            if (other == (int)index)
                            {
                                other = link_base->obj[1];
                            }
                            rfg_subpiece* osp = &shape->get_object_buffer()[other];
                            if (osp->get_destroyable() == shape && osp->impulse_info_index != 0xFFFF)
                            {
                                const shard_impulse_info* other_info = rfg_shape_get_shard_impulse_info(osp);
                                if (other_info != nullptr)
                                {
                                    num_neighbours++;
                                    dir += other_info->impulse_dir;
                                    dir_pos += other_info->impulse_pos;
                                }
                            }
                        }
                        if (1 < num_neighbours)
                        {
                            const float scale = 1.0f / (float)num_neighbours;
                            dir *= scale;
                            dir_pos *= scale;
                        }
                    }
                    impulse += dir;
                    num_impulses++;
                    impulse_pos += dir_pos;
                }
            }
            if ((sp->flags & 0x100) != 0)
            {
                flags |= 0x200;
            }
            if ((sp->flags & 0x80) != 0)
            {
                flags |= 4;
            }
            mass = sp_base->mass + mass;
            bbox_expand(bmin, bmax, sp_base->prel_bmin, sp_base->prel_bmax);
            sp->modified_framecount = (unsigned char)Framecount;
            if (sp->shard_next == q)
            {
                break;
            }
            sp = &shape->get_object_buffer()[sp->shard_next];
        }
        if (1 < num_impulses)
        {
            impulse_pos *= 1.0f / (float)num_impulses;
            impulse *= 1.0f / sqrtf((float)num_impulses);
        }
        if (1 < num_positions)
        {
            average_pos *= 1.0f / (float)num_positions;
        }
    }
    if (1 < num_subpieces && mass <= 0.0f)
    {
        flags |= 4;
    }
}

// 0x00711950 (:449)
void rfg_shardify_info::init(rfg_mover* _rm)
{
    rm = _rm;
    q_shard = nullptr;
    num_q_shards = 0;
    destruction_flags = 0;
}

// 0x00711990 (:599)
// Partial: the console command (sets Explosion_pct from three arguments) is not recovered.
void unused_dcf_load_balance_explosion_pct()
{
}

// 0x00711A50 (:667)
static bool box_acts_as_client_for_destruction()
{
    if (!Multiplayer)
    {
        return false;
    }
    if (Multiplayer && !multi_is_server())
    {
        return true;
    }
    return multi_game_is_migrating();
}

// 0x00711A80 (:693)
static bool box_is_single_player_or_acts_as_server_for_destruction()
{
    if (!Multiplayer)
    {
        return true;
    }
    if (Multiplayer && !multi_is_server())
    {
        return false;
    }
    return !multi_game_is_migrating();
}

// 0x00711AC0 (:698)
static bool box_acts_as_server_for_destruction()
{
    return Multiplayer && multi_is_server() && !multi_game_is_migrating();
}

// <rfg_collision_event> 0x00737C10 (:705)
// 0x00719110 etc. (:705)
template<typename T>
static void swap_dynamics(T& v1, T& v2)
{
    T temp = v2;
    v2 = v1;
    v1 = temp;
}

// 0x00711AF0 (:716)
unsigned short rfg_dyn_get_next_destruction_frame()
{
    return Destruction_frame++;
}

// ---- destruction events (the destroyed regions of the dynamic hole requests)
// 0x007324D0 (:724)
// A free event; when the pool is full the oldest one is dropped.
static rfg_destruction_event* get_free_destruction_event()
{
    if (RFG_destruction_events.get_free() < 1)
    {
        rfg_destruction_event* oldest = RFG_destruction_events.get_first(0);
        oldest->flags.in_use = 0; // not checked
        remove_dynamic_hole_request(oldest);
        RFG_destruction_events.free(oldest);
    }
    return RFG_destruction_events.alloc(0);
}

// 0x0072C720 (:741)
void remove_destruction_event(rfg_destruction_event* rde)
{
    rde->flags.in_use = 0;
    remove_dynamic_hole_request(rde);
    RFG_destruction_events.free(rde);
}

// 0x00711B10 (:750)
// LTCG: bmax in ECX, orient in EDX. Whether a box (in orient's frame at pos) can merge into rde: same orientation (up
// to a flip), overlapping boxes (bridges grow 10% and must stay under 25 m across), and the union must not stretch a
// flat event more than 30% along its thin axes; small unions (< 4 m) and unions that add nothing always merge.
static bool destruction_event_can_merge(rfg_destruction_event* rde, const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, bool is_bridge)
{
    if (rde->flags.is_bridge && !is_bridge)
    {
        return false;
    }
    const float sign = orient.fvec * rde->orient.fvec < -0.999f ? -1.0f : 1.0f;
    vector diff = orient.fvec - rde->orient.fvec * sign;
    if (1e-06f < diff * diff)
    {
        return false;
    }
    diff = orient.rvec - rde->orient.rvec * sign;
    if (1e-06f < diff * diff)
    {
        return false;
    }
    vector lmin = bmin;
    vector lmax = bmax;
    bool bridge = false;
    if (rde->flags.is_bridge && is_bridge)
    {
        lmin *= 1.1f;
        lmax *= 1.1f;
        bridge = true;
    }
    if (pos != rde->pos)
    {
        vector offset = pos - rde->pos;
        offset.rotate(orient);
        lmin += offset;
        lmax += offset;
    }
    if (rde->bmax.x <= lmin.x || rde->bmax.y <= lmin.y || rde->bmax.z <= lmin.z || lmax.x <= rde->bmin.x || lmax.y <= rde->bmin.y || lmax.z <= rde->bmin.z)
    {
        return false;
    }
    vector umin = lmin;
    vector umax = lmax;
    bbox_expand(umin, umax, rde->bmin, rde->bmax);
    const float ux = umax.x - umin.x;
    const float uz = umax.z - umin.z;
    if (bridge)
    {
        if (25.0f <= ux)
        {
            return false;
        }
        return uz < 25.0f;
    }
    const float rx = rde->bmax.x - rde->bmin.x;
    const float ry = rde->bmax.y - rde->bmin.y;
    const float rz = rde->bmax.z - rde->bmin.z;
    if (1.0f < ry * ry + rx * rx + rz * rz)
    {
        if (rx <= ry || rz <= ry)
        {
            if (ry <= rx || ry <= rz)
            {
                if (rx < ry)
                {
                    if (rx * 1.3f < ux)
                    {
                        return false;
                    }
                }
                else if (rz < ry && rz * 1.3f < uz)
                {
                    return false;
                }
            }
            else if (rx <= rz)
            {
                if (rx * 1.3f < ux)
                {
                    return false;
                }
            }
            else if (rz * 1.3f < uz)
            {
                return false;
            }
        }
        else if (ry * 1.3f < umax.y - umin.y) // y is the thin axis
        {
            return false;
        }
    }
    if (ux < 4.0f && umax.y - umin.y < 4.0f && uz < 4.0f)
    {
        return true;
    }
    const vector dmax = rde->bmax - umax;
    const vector dmin = rde->bmin - umin;
    if (dmax * dmax + dmin * dmin < 0.001f)
    {
        return true; // rde already holds the box
    }
    const vector grow_max = lmax - umax;
    const vector grow_min = lmin - umin;
    return grow_min.mag_squared() + grow_max.mag_squared() < 0.001f; // the box already holds rde
}

// 0x0072C740 (:860)
// LTCG: temp_rde in ESI. Merges temp_rde's box (moved into rde's frame) into rde and frees temp_rde.
static void rfg_destruction_event_merge(rfg_destruction_event* rde, rfg_destruction_event* temp_rde)
{
    if (rde->pos != temp_rde->pos)
    {
        bbox local_box;
        local_box.min = temp_rde->bmin;
        local_box.max = temp_rde->bmax;
        bbox world_box;
        bbox_instance(world_box, temp_rde->orient, temp_rde->pos, local_box);
        bbox_deinstance(local_box, rde->orient, rde->pos, world_box);
        temp_rde->bmin = local_box.min;
        temp_rde->bmax = local_box.max;
    }
    bbox_expand(rde->bmin, rde->bmax, temp_rde->bmin, temp_rde->bmax);
    rde->flags.is_glass &= temp_rde->flags.is_glass; // glass only when both are
    remove_destruction_event(temp_rde); // inlined
    rde->flags.modified = 1;
}

// 0x00732520 (:887)
// LTCG: orient in EDI.
static void rfg_destruction_event_add_box(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, bool is_bridge, bool is_glass)
{
    rfg_destruction_event* rde = get_free_destruction_event(); // inlined
    if (rde == nullptr)
    {
        return;
    }
    rde->bmin = bmin;
    rde->bmax = bmax;
    rde->pos = pos;
    rde->orient = orient;
    rde->flags.is_bridge = is_bridge;
    rde->flags.is_glass = is_glass;
    rde->last_modified_timestamp.set(0);
    rde->flags.in_use = 1;
    rde->flags.modified = 1;
}

// 0x0072C850 (:909)
// One modified event per frame (searched from the last one) absorbs the events it can merge with and refreshes its
// dynamic hole request.
void rfg_destruction_event_do_frame()
{
    static rfg_destruction_event* rde; // the last processed event
    if (rde == nullptr || !rde->flags.in_use)
    {
        rde = RFG_destruction_events.get_first(0);
    }
    rfg_destruction_event* ev = rde;
    if (ev == nullptr)
    {
        return;
    }
    while (true)
    {
        if (ev->flags.modified)
        {
            ev->flags.modified = 0;
            rde = ev;
            {
                csspinlock_scope lock(RFG_destruction_events.thread_access);
                rfg_destruction_event* other = RFG_destruction_events.get_first(0);
                rfg_destruction_event* next = RFG_destruction_events.get_next(other);
                while (other != nullptr)
                {
                    if (other != rde && destruction_event_can_merge(other, rde->bmin, rde->bmax, rde->pos, rde->orient, rde->flags.is_bridge))
                    {
                        rfg_destruction_event_merge(rde, other);
                    }
                    other = next;
                    next = RFG_destruction_events.get_next(next);
                }
            }
            add_dynamic_hole_request(rde);
            return;
        }
        ev = RFG_destruction_events.get_next(ev);
        if (rde == ev)
        {
            break;
        }
        if (ev == nullptr)
        {
            rde = nullptr;
            return;
        }
    }
    rde = nullptr;
}

// 0x0071B9B0 (:959)
// LTCG: objp in EAX, collision_body in EDI. The mass and velocity at a contact: humans (ragdoll or controller), movable
// rfg_movers and general movers, vehicles (the touched body); mass -1 when unknown.
static void get_contact_point_properties(object* objp, const vector& contact_pos, float& mass, vector& velocity, const hkpCdBody* collision_body)
{
    mass = -1.0f;
    velocity.zero();
    if (objp == nullptr)
    {
        return;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
    {
        human* hp = (human*)objp;
        mass = objp->get_mass();
        if (!human_is_ragdolled(hp))
        {
            human_controller_get_vel(hp, velocity);
            return;
        }
        havok_ragdoll_get_vel(hp->ragdoll_idx, velocity);
        return;
    }
    case OT_MOVER:
        if (objp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* rm = (rfg_mover*)objp;
            rfg_destroyable_shape* shape;
            if (rm->get_move_type() != RFGM_FIXED && (shape = rm->get_shape()) != nullptr)
            {
                mass = shape->mass;
                havok_body_get_point_velocity(objp->havok_handle, contact_pos, velocity);
                return;
            }
        }
        else if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            const int motion_type = havok_body_get_motion_type(objp->havok_handle);
            if (motion_type != hkpMotion::MOTION_FIXED && motion_type != hkpMotion::MOTION_KEYFRAMED)
            {
                mass = objp->get_mass();
                havok_body_get_linear_velocity(objp->havok_handle, velocity);
                return;
            }
        }
        break;
    case OT_VEHICLE:
    {
        hkpRigidBody* body;
        if (collision_body == nullptr)
        {
            body = havok_body_get_pointer(objp->havok_handle);
        }
        else
        {
            body = hkpGetRigidBody(collision_body->getRootCollidable()); // root inlined
        }
        if (body != nullptr)
        {
            mass = havok_body_get_mass(body);
            havok_body_get_point_velocity(body, contact_pos, velocity);
        }
        break;
    }
    case (object_type)0x13:
        break;
    default:
        mass = objp->get_mass();
        havok_body_get_linear_velocity(objp->havok_handle, velocity);
        break;
    }
}

// 0x00712420 (:1032)
// LTCG: objp in EAX. A general mover's piece index, otherwise key (see the notes).
static unsigned int get_subpiece_index_from_object(object* objp, unsigned int key)
{
    if (objp != nullptr && objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return ((general_mover*)objp)->subpiece_index;
    }
    return key;
}

// 0x00723FD0 (:1047)
// LTCG: obj_a in EAX, obj_b in ECX. Whether two pieces of the same destroyable touch through a broken link that is
// flagged 0x1 (no collision between them).
static bool rfg_dyn_filter_destruction(object* obj_a, object* obj_b, unsigned int key_a, unsigned int key_b)
{
    key_a = get_subpiece_index_from_object(obj_a, key_a); // inlined
    key_b = get_subpiece_index_from_object(obj_b, key_b); // inlined
    if (key_a == 0xFFFFFFFF || key_b == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int uid_a = (obj_a != nullptr && obj_a->get_obj_type() == OT_MOVER) ? ((object_mover*)obj_a)->shape_uid : 0xFFFFFFFF;
    const unsigned int uid_b = (obj_b != nullptr && obj_b->get_obj_type() == OT_MOVER) ? ((object_mover*)obj_b)->shape_uid : 0xFFFFFFFF;
    if (uid_a != uid_b)
    {
        return false;
    }
    rfg_destroyable_shape* shape;
    if (obj_a->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        if (!((rfg_mover*)obj_a)->rdd.is_loaded())
        {
            return false;
        }
        shape = ((rfg_mover*)obj_a)->rdd.get_data()->shape;
    }
    else
    {
        if (obj_b->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return false;
        }
        shape = ((rfg_mover*)obj_b)->get_shape();
    }
    if (shape == nullptr)
    {
        return false;
    }
    // walk the broken links (past num_links) of the piece with fewer of them
    rfg_subpiece* sp_a = &shape->get_object_buffer()[key_a];
    rfg_subpiece* sp_b = &shape->get_object_buffer()[key_b];
    const rfg_subpiece_base* base_a = shape->get_subpiece_base(key_a);
    const rfg_subpiece_base* base_b = shape->get_subpiece_base(key_b);
    const rfg_subpiece_base* walk_base = base_b;
    rfg_subpiece* walk_sp = sp_b;
    unsigned int other = key_a;
    if ((int)(base_a->num_links - sp_a->num_links) < (int)(base_b->num_links - sp_b->num_links))
    {
        walk_base = base_a;
        walk_sp = sp_a;
        other = key_b;
    }
    const unsigned short* links = walk_sp->links.to_pointer();
    for (unsigned int l = walk_sp->num_links; (int)l < (int)walk_base->num_links; l++)
    {
        const rfg_link_base* link_base = shape->get_link_base(links[l]);
        if ((unsigned int)(int)link_base->obj[0] == other || (unsigned int)(int)link_base->obj[1] == other)
        {
            return (link_base->flags & 1) != 0;
        }
    }
    return false;
}

// 0x00712440 (:1110)
// LTCG: impulse in ESI, mass in XMM1. Clamps an impulse to the linear velocity limit (20 m/s); none below 1 kg.
static void rfg_dyn_modify_impulse(vector& impulse, float mass)
{
    if (mass < 1.0f)
    {
        impulse = ZERO_VECTOR;
        return;
    }
    vector linear;
    vector angular;
    explosion_modify_impulses(&linear, &angular, &impulse, &ZERO_VECTOR, mass, 1.0f, 1.0f, 20.0f, 2.5f);
    impulse = linear;
}

// 0x007124D0 (:1130)
// LTCG: impulse in EAX, mass in XMM1.
static void rfg_dyn_apply_point_impulse(int havok_handle, const vector& impulse, const vector& impulse_pos, float mass)
{
    vector modified = impulse;
    rfg_dyn_modify_impulse(modified, mass); // inlined
    havok_body_apply_point_impulse(havok_handle, modified, impulse_pos);
}

// 0x00712590 (:1143)
// LTCG: flags in ECX. Big shards (10 pieces or more) and heavy destruction (0x10) collide with more: normal, lite,
// otherwise ultra lite (world only when the destruction flags 0x861 are set, or in multiplayer).
static rfg_move_type get_move_type_for_new_shard(int num_subpieces, int flags)
{
    int level = (flags & 0x10) != 0 ? 1 : 0;
    if (9 < num_subpieces)
    {
        level++;
    }
    if (level == 0)
    {
        if (Multiplayer)
        {
            return RFGM_WORLD_ONLY;
        }
        return (flags & 0x861) != 0 ? RFGM_WORLD_ONLY : RFGM_ULTRA_LITE;
    }
    if (level == 1)
    {
        return RFGM_LITE;
    }
    if (level == 2)
    {
        return RFGM_NORMAL;
    }
    return RFGM_INVALID;
}

// 0x007125E0 (:1202)
// LTCG: child_objp in EAX, host_mover_objp in ECX. Remote children in multiplayer move only to synced movers.
static bool mp_allow_constraint_transfer(object* child_objp, constraint* cp, object_mover* host_mover_objp)
{
    if (Multiplayer && child_objp->remote_obj_flags.remote_object)
    {
        return host_mover_objp->mover_should_sync();
    }
    return true;
}

// 0x0071BB40 (:1217)
// Objects attached (constraints, both directions) to pieces modified this frame that are in subpiece_list move to
// new_parent (repositioned first when the old mover moves); remote children that cannot follow lose their constraint.
static void transfer_attached_objects(rfg_mover* old_parent, object_mover* new_parent, unsigned short subpiece_list)
{
    if (object_mover_is_fading_for_delete(new_parent))
    {
        return;
    }
    rfg_destroyable_shape* shape = old_parent->get_shape();
    if (shape == nullptr)
    {
        return;
    }
    constraint* c = old_parent->host_constraint_ptr;
    while (c != nullptr)
    {
        constraint* next = c->host_constraint_next != old_parent->host_constraint_ptr ? c->host_constraint_next : nullptr;
        if (c->host_attach_index != -1 && shape->get_object_buffer()[c->host_attach_index].modified_framecount == (unsigned char)Framecount && subpiece_list != 0xFFFF)
        {
            for (rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
            {
                if (c->host_attach_index == (int)(sp - shape->get_object_buffer()))
                {
                    object* child = World.object_from_handle(c->child_handle);
                    if (!child->obj_flags.destroyed) // not checked
                    {
                        if (mp_allow_constraint_transfer(child, c, new_parent)) // inlined
                        {
                            if (old_parent->get_move_type() != RFGM_FIXED && child->havok_handle != 0xFFFFFFFF)
                            {
                                havok_body_set_pos_orient(child->havok_handle, child->pos, child->orient);
                            }
                            object_constraint_transfer(true, c, child, old_parent, new_parent, nullptr);
                            break;
                        }
                        World.destroy_object(c, false);
                    }
                }
                if (sp->next == subpiece_list)
                {
                    break;
                }
            }
        }
        c = next;
    }
    c = old_parent->child_constraint_ptr;
    while (c != nullptr)
    {
        constraint* next = c->child_constraint_next != old_parent->child_constraint_ptr ? c->child_constraint_next : nullptr;
        if (c->child_attach_index != -1 && shape->get_object_buffer()[c->child_attach_index].modified_framecount == (unsigned char)Framecount && subpiece_list != 0xFFFF)
        {
            for (rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
            {
                if (c->child_attach_index == (int)(sp - shape->get_object_buffer()))
                {
                    object* host = World.object_from_handle(c->host_handle);
                    if (!host->obj_flags.destroyed) // not checked
                    {
                        if (old_parent->get_move_type() != RFGM_FIXED && host->havok_handle != 0xFFFFFFFF)
                        {
                            havok_body_set_pos_orient(host->havok_handle, host->pos, host->orient);
                        }
                        object_constraint_transfer(false, c, host, old_parent, new_parent, nullptr);
                        break;
                    }
                }
                if (sp->next == subpiece_list)
                {
                    break;
                }
            }
        }
        c = next;
    }
}

// 0x0071BDE0 (:1294)
// Script point effects hosted by pieces in subpiece_list move to new_mover: the effect follows (or stops, when the
// effect info says 0x1), and effects that die with a move (0x2) are killed.
static void transfer_script_point_effect_insts(rfg_mover* old_mover, object_mover* new_mover, unsigned short subpiece_list)
{
    rfg_destroyable_shape* shape = old_mover->get_shape();
    if (shape == nullptr)
    {
        return;
    }
    for (int i = 0; i < MAX_SCRIPT_POINT_EFFECT_INSTS; i++)
    {
        script_point_effect_inst* speip = old_mover->m_level_obj_properties->m_script_point_effect_insts[i];
        if (speip == nullptr || speip->state == SPES_DEAD || subpiece_list == 0xFFFF)
        {
            continue;
        }
        const script_point_effect_info* info = old_mover->m_level_obj_properties->m_base_properties->m_script_point_effect_infos[i];
        for (rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
        {
            if ((unsigned int)(sp - shape->get_object_buffer()) == speip->shape_key)
            {
                new_mover->m_level_obj_properties->m_script_point_effect_insts[i] = speip;
                old_mover->m_level_obj_properties->m_script_point_effect_insts[i] = nullptr;
                speip->host_obj = new_mover;
                object_effect* effect = World.object_from_handle<object_effect>(speip->current_effect);
                if (effect != nullptr)
                {
                    if ((info->spe_flags & 0x1) == 0)
                    {
                        effect->deattach_from_host();
                        effect->attach_to_host(new_mover->get_handle(), speip->script_point_idx);
                    }
                    else
                    {
                        effect->stop(object_effect::SS_STOP_EMITTING);
                        speip->current_effect = 0xFFFFFFFF;
                        speip->state = SPES_STOPPED;
                    }
                }
                if ((info->spe_flags & 0x2) != 0 && speip->state != SPES_PRE_DEATH)
                {
                    new_mover->script_point_kill(i, true);
                }
            }
            if (sp->next == subpiece_list)
            {
                break;
            }
        }
    }
}

// 0x0071BF60 (:1350)
// LTCG: shape in EDI. Constraints attached to pieces that left the shape are destroyed.
static void validate_attached_objects(rfg_mover* mover, rfg_destroyable_shape* shape)
{
    constraint* c = mover->host_constraint_ptr;
    while (c != nullptr)
    {
        constraint* next = c->host_constraint_next != mover->host_constraint_ptr ? c->host_constraint_next : nullptr;
        if (c->host_attach_index == -1 || shape->get_object_buffer()[c->host_attach_index].get_destroyable() != shape)
        {
            World.destroy_object(c, false);
        }
        c = next;
    }
    c = mover->child_constraint_ptr;
    while (c != nullptr)
    {
        constraint* next = c->child_constraint_next != mover->child_constraint_ptr ? c->child_constraint_next : nullptr;
        // the host attach index is tested, the child's piece looked up (kept)
        if (c->host_attach_index == -1 || shape->get_object_buffer()[c->child_attach_index].get_destroyable() != shape)
        {
            World.destroy_object(c, false);
        }
        c = next;
    }
}

// 0x00712610 (:1369)
static bool rfg_dyn_mp_disallow_destruction()
{
    return Multi_match_info.match_state == MGMS_PREGAME;
}

// 0x00712620 (:1375)
static bool rfg_dyn_mp_allow_large_event_count()
{
    return Multi_match_info.match_state == MGMS_COUNTDOWN;
}

// 0x00712630 (:1381)
static bool rfg_dyn_allow_destruction_effects()
{
    if (!Multiplayer)
    {
        return true;
    }
    return !multi_game_mode_is_pregame() && !multi_game_mode_is_countdown();
}

// 0x00712660 (:1390)
static void add_sync_general_mover_to_created_list(general_mover* gmp)
{
    if (Multiplayer && Mover_destruction_results.num_synced_general_movers_created_by_destruction < MAX_DESTRUCTION_RESULTS)
    {
        Mover_destruction_results.synced_general_movers_created_by_destruction[Mover_destruction_results.num_synced_general_movers_created_by_destruction++] = gmp;
    }
}

// 0x00712690 (:1401)
static void add_rfg_mover_to_created_list(rfg_mover* rmp)
{
    if (Multiplayer && Mover_destruction_results.num_rfg_movers_created_by_destruction < MAX_DESTRUCTION_RESULTS)
    {
        Mover_destruction_results.rfg_movers_created_by_destruction[Mover_destruction_results.num_rfg_movers_created_by_destruction++] = rmp;
    }
}

// 0x007126C0 (:1412)
// Once per mover (not after a multiplayer destroy, 0x2).
static void add_rfg_mover_to_destroyed_list(rfg_mover* rmp)
{
    if (Multiplayer && (rmp->mp_info->destroy_causes.data & 0x22) == 0)
    {
        rmp->mp_info->destroy_causes.has_been_on_list = true;
        if (Mover_destruction_results.num_rfg_movers_destroyed_by_destruction < MAX_DESTRUCTION_RESULTS)
        {
            Mover_destruction_results.rfg_movers_destroyed_by_destruction[Mover_destruction_results.num_rfg_movers_destroyed_by_destruction++] = rmp;
        }
    }
}

// 0x0071C040 (:1439)
// LTCG: delete_count in ECX.
static void add_rfg_mover_shardify_delete_count_record(rfg_mover* rmp, int delete_count)
{
    if (!Multiplayer || delete_count <= 0)
    {
        return;
    }
    if (MAX_SHARDIFY_DELETE_COUNT - 1 < delete_count)
    {
        delete_count = MAX_SHARDIFY_DELETE_COUNT;
    }
    if (Mover_destruction_results.num_shardify_delete_counts < MAX_DESTRUCTION_RESULTS)
    {
        shardify_delete_record& record = Mover_destruction_results.shardify_delete_count_records[Mover_destruction_results.num_shardify_delete_counts++];
        record.mover = rmp;
        record.delete_count = delete_count;
    }
}

// 0x00712700 (:1460)
// LTCG: om in ESI. Outside the normal destruction mode (multiplayer pregame) new movers are destroyed at once, without
// a destroy packet.
static void rfg_dyn_do_mode_specific_processing(object_mover* om)
{
    if (!Multiplayer || om == nullptr || Destruction_mode == DM_NORMAL)
    {
        return;
    }
    if (om->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        multi_rfg_mover_info* info = ((rfg_mover*)om)->mp_info;
        info->destroy_causes.destruction = true;
        info->destroy_causes.level_init_destroy = true;
    }
    else
    {
        om->set_is_visible(false);
    }
    om->remote_obj_flags.client_destroy_permission = true;
    om->remote_obj_flags.no_destroy_packet = true;
    World.destroy_object(om, false);
}

// 0x00712770 (:1485)
static unsigned int rfg_dyn_get_rfg_mover_destruction_uid()
{
    return (unsigned int)_InterlockedIncrement((volatile long*)&Rfg_mover_destruction_uid);
}

// 0x00712790 (:1490)
static unsigned int rfg_dyn_get_general_mover_destruction_uid()
{
    return (unsigned int)_InterlockedIncrement((volatile long*)&General_mover_destruction_uid);
}

// 0x007127B0 (:1495)
static rfg_move_type set_multiplayer_rfg_move_type(rfg_move_type move_type)
{
    return move_type == RFGM_NO_COLLISION ? RFGM_NO_COLLISION : RFGM_WORLD_ONLY;
}

// 0x007127C0 (:1512)
// LTCG: sp_base in EAX. A destruction uid for shards synced in multiplayer: forced ones and pieces larger than 1.5 m on
// two axes; -1 otherwise.
static unsigned int get_general_mover_destruction_uid(rfg_subpiece_base* sp_base, bool force_sync)
{
    if (!Multiplayer || MAX_DESTRUCTION_RESULTS <= Mover_destruction_results.num_synced_general_movers_created_by_destruction)
    {
        return 0xFFFFFFFF;
    }
    unsigned char large_axes = SHARD_SYNC_MIN_SIZE < sp_base->prel_bmax.x - sp_base->prel_bmin.x ? 1 : 0;
    if (SHARD_SYNC_MIN_SIZE < sp_base->prel_bmax.y - sp_base->prel_bmin.y)
    {
        large_axes++;
    }
    if (SHARD_SYNC_MIN_SIZE < sp_base->prel_bmax.z - sp_base->prel_bmin.z)
    {
        large_axes++;
    }
    if (force_sync || 1 < large_axes)
    {
        return rfg_dyn_get_general_mover_destruction_uid(); // inlined
    }
    return 0xFFFFFFFF;
}

// 0x00712870 (:1539)
// LTCG: ev_base in EAX, base_pos in ECX. The slice data: the base position, the slice index (as direct damage data)
// and the two angles (in base_b's raw point).
static void add_pre_collide_slice_common(rfg_collision_event_base& ev_base, const vector& base_pos, int slice_index, float angle0, float angle1)
{
    ev_base.base_a.raw.rcollision_point_local = base_pos;
    ev_base.set_slice_index((unsigned int)slice_index); // inlined
    ev_base.base_b.raw.rcollision_point_local.x = angle0;
    ev_base.base_b.raw.rcollision_point_local.y = angle1;
    multi_game_destruction_set_slice_working_data(ev_base);
}

// 0x007128B0 (:1552)
// LTCG: ev in EAX, rmp in ECX, base_pos in ESI. Fills ev as a slice of rmp (the event constructor inlined).
static bool rfg_dyn_add_collide_slice_immediate(rfg_mover* rmp, rfg_collision_event* ev, const vector& base_pos, int slice_index, float angle0, float angle1)
{
    rfg_collision_event_base& base = ev->collision_base;
    base.destruction_type = DET_SLICE;
    base.base_a.obj_handle = rmp->get_handle();
    base.exp_radius_multiplier = 0.0f;
    base.flags = 0;
    base.data_count = 0;
    base.base_a.misc = 0;
    base.base_b.misc = 0;
    base.damage = 0;
    base.compressed_exp_radius_multiplier = 0;
    ev->havok_base.affected_subpiece_a = nullptr;
    ev->havok_base.affected_subpiece_b = nullptr;
    ev->havok_base.count = 0;
    ev->havok_base.mgr = nullptr;
    base.base_b.obj_handle = 0xFFFFFFFF;
    base.base_a.shape_key = 0xFFFFFFFF;
    base.base_b.shape_key = 0xFFFFFFFF;
    base.owner = 0xFFFFFFFF;
    base.exp_unique_id = 0xFF;
    memset(ev->havok_base.id, 0xFF, sizeof(ev->havok_base.id));
    add_pre_collide_slice_common(base, base_pos, slice_index, angle0, angle1); // inlined
    return true;
}

// Defined later in rfg_destruction_unity.cpp (file statics of rfg_collisions.cpp, rfg_mover.cpp and rfg_stress.cpp).
static bool rfg_effect_candidate_allocate(const collision_event* coll_event, unsigned char material_index, unsigned char other_index, material_event_type event_type, material_event_size event_size, material_shape_type shape_type, float effect_volume, const vector& pos, bool priority, unsigned int delay, float energy, const matrix* orient, unsigned int key, unsigned char melee_group_index);
static void rfg_mover_toggle_self_and_children_invulnerable(rfg_mover* mover);
static void stress_evaluation_load_table();
static bool rfg_dyn_apply_slice(rfg_collision_event* ev);

// Circular piece lists through shard_next / shard_prev (the work lists of the shardify).
static void shard_list_remove(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (list == index && sp->shard_next == index)
    {
        list = 0xFFFF;
    }
    else
    {
        if (list == index)
        {
            list = sp->shard_next;
        }
        objects[sp->shard_prev].shard_next = sp->shard_next;
        objects[sp->shard_next].shard_prev = sp->shard_prev;
    }
    sp->shard_next = 0xFFFF;
    sp->shard_prev = 0xFFFF;
}

static void shard_list_append(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (list == 0xFFFF)
    {
        sp->shard_next = index;
        sp->shard_prev = index;
        list = index;
        return;
    }
    sp->shard_prev = objects[list].shard_prev;
    sp->shard_next = list;
    objects[objects[list].shard_prev].shard_next = index;
    objects[list].shard_prev = index;
}

// Joins list b behind list a (both not empty).
static void shard_list_splice(rfg_destroyable_shape* shape, unsigned short a, unsigned short b)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    objects[objects[a].shard_prev].shard_next = b;
    objects[objects[b].shard_prev].shard_next = a;
    const unsigned short prev_b = objects[b].shard_prev;
    objects[b].shard_prev = objects[a].shard_prev;
    objects[a].shard_prev = prev_b;
}

// The piece on the other side of a link.
static rfg_subpiece* linked_subpiece(rfg_destroyable_shape* shape, unsigned int index, unsigned short link_index)
{
    const rfg_link_base* link_base = shape->get_link_base(link_index);
    int other = link_base->obj[0];
    if (other == (int)index)
    {
        other = link_base->obj[1];
    }
    return &shape->get_object_buffer()[other];
}

// Removes every link of a piece.
static void remove_all_links(rfg_destroyable_shape* shape, rfg_subpiece* sp)
{
    while (sp->num_links != 0)
    {
        shape->remove_link(shape->get_link(sp->links.to_pointer()[0]));
    }
}

// 0x00737CE0 (:1583)
// LTCG: sp in EDI. A piece broken off as a general mover at its current transform. Nano pieces (0x8000) fade out within
// 1-2 s with no collision and get the nano fade effect; other faded shards go within 2-5 s. In multiplayer, shards
// synced to the clients (script point hosts or large pieces) stay ultra lite; the others fade and collide with the
// world only. Attached objects and script point effects of the piece follow it.
static general_mover* rfg_dyn_create_shard(rfg_mover* movep, rfg_move_type move_type, bool fade_out, rfg_subpiece* sp)
{
    rfg_destroyable_shape* shape = movep->get_shape();
    const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
    rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
    int fade_time = 0;
    unsigned int destruction_uid;
    if ((sp->flags & 0x8000) == 0)
    {
        if (Multiplayer)
        {
            destruction_uid = get_general_mover_destruction_uid(sp_base, (sp->flags & 0x400000) != 0); // inlined
            if (destruction_uid == 0xFFFFFFFF)
            {
                fade_out = true;
                move_type = set_multiplayer_rfg_move_type(move_type); // inlined
                fade_time = int_rand_range(2000, 5000);
            }
            else
            {
                fade_out = false;
                move_type = RFGM_ULTRA_LITE;
            }
        }
        else
        {
            destruction_uid = 0xFFFFFFFF;
            if (fade_out)
            {
                fade_time = int_rand_range(2000, 5000);
            }
        }
    }
    else
    {
        fade_out = true;
        fade_time = int_rand_range(1000, 2000);
        destruction_uid = 0xFFFFFFFF;
        move_type = RFGM_NO_COLLISION;
    }
    vector pos;
    matrix orient;
    shape->get_subpiece_pos_orient_uncached(sp, pos, orient);
    c_chunk_base* chunk = chunk_get_base_by_uid(movep->chunk_uid);
    gm_prop_block pblock(pos, orient, movep->chunk_uid, movep->chunk_obj_uid, chunk, movep->m_level_obj_properties->m_base_properties->m_name, sp, shape, move_type, destruction_uid, (movep->mover_flags & 0x10) != 0, (movep->mover_flags & 0x4000) != 0, movep->get_handle());
    general_mover* om = World.create_object<general_mover>(pblock, 0xFFFFFFFF);
    if (om == nullptr)
    {
        return nullptr;
    }
    rfg_dyn_do_mode_specific_processing(om);
    if (!om->obj_flags.streamed)
    {
        World.destroy_object(om, false);
        return nullptr;
    }
    sp->set_owner(om);
    if (destruction_uid != 0xFFFFFFFF)
    {
        add_sync_general_mover_to_created_list(om); // inlined
    }
    if (fade_out)
    {
        object_mover_mark_for_fade(om, fade_time);
    }
    if ((sp->flags & 0x8000) != 0 && om->m_level_obj_properties->m_custom_effects[2] == 0xFFFFFFFF)
    {
        om->m_level_obj_properties->m_custom_effects[2] = effects_play(weapons_get_nano_fade_effect(), om->pos, om->orient, true, om->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    // a one-piece list for the transfers
    unsigned short list = (unsigned short)index;
    sp->next = (unsigned short)index;
    sp->prev = (unsigned short)index;
    transfer_attached_objects(movep, om, list);
    transfer_script_point_effect_insts(movep, om, list);
    if (list != (unsigned short)index || sp->next != (unsigned short)index)
    {
        shape->get_object_buffer()[sp->prev].next = sp->next;
        shape->get_object_buffer()[sp->next].prev = sp->prev;
    }
    sp->next = 0xFFFF;
    sp->prev = 0xFFFF;
    return om;
}

// ---- dynamic links (anchor depths: 0 at a world anchor, +1 per link away from it)
// 0x00724140 (:1704)
// The mover on the other side of a piece's link, and its piece (none for a general mover or an unloaded rfg_mover).
bool rfg_dyn_link_get_linked_data(rfg_mover* rm, rfg_subpiece* sp, object_mover*& other_mp, rfg_subpiece*& other_sp, rfg_subpiece_base*& other_sp_base)
{
    rfg_dynamic_link* rdl = (sp != nullptr && sp->dynamic_link_index != 0xFFFF) ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
    if (rm == nullptr || rdl == nullptr)
    {
        return false;
    }
    const int other_side = rm->get_handle() == rdl->mover_handle[0] ? 1 : 0;
    object_mover* other = World.object_from_handle<object_mover>(rdl->mover_handle[other_side]);
    other_mp = other;
    if (other == nullptr)
    {
        return false;
    }
    if (other->get_sub_type() != OT_SUB_MOVER_GENERAL)
    {
        if (other->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return false;
        }
        rfg_destroyable_shape* other_shape = ((rfg_mover*)other)->get_shape();
        if (other_shape != nullptr)
        {
            const unsigned short key = rdl->shape_key[other_side];
            other_sp = &other_shape->get_object_buffer()[key];
            other_sp_base = other_shape->get_subpiece_base(key);
            return other_sp != nullptr && other_sp_base != nullptr;
        }
    }
    other_sp = nullptr;
    other_sp_base = nullptr;
    return true;
}

// 0x00724250 (:1740)
dynamic_link_state rfg_dyn_link_get_state(rfg_mover* rm, rfg_subpiece* sp)
{
    rfg_dynamic_link* rdl = (sp != nullptr && sp->dynamic_link_index != 0xFFFF) ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
    if (rm == nullptr || rdl == nullptr)
    {
        return DLS_INVALID;
    }
    const int this_side = rm->get_handle() != rdl->mover_handle[1] ? 0 : 1;
    const short diff = (short)(rdl->anchor_depth[this_side] - rdl->anchor_depth[1 - this_side]);
    if (diff < 0)
    {
        return DLS_SUPPORTING;
    }
    return diff < 1 ? DLS_NEUTRAL : DLS_SUPPORTED;
}

// 0x0072C9E0 (:1766)
// Lowers a shape's anchor depth and propagates it through its links.
void rfg_dyn_link_update_depth(rfg_mover* rm, rfg_destroyable_shape* shape, unsigned short depth)
{
    if (rm == nullptr || shape == nullptr)
    {
        return;
    }
    if (shape->anchor_depth != 0xFFFF && depth < shape->anchor_depth)
    {
        rm->flags.updating_anchors = 1;
        if (shape->child_obj_list != 0xFFFF)
        {
            for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
            {
                rfg_dyn_link_set_depth(rm, sp, depth, false);
                if (sp->next == shape->child_obj_list)
                {
                    break;
                }
            }
        }
        rm->flags.updating_anchors = 0;
    }
    shape->anchor_depth = depth <= shape->anchor_depth ? depth : shape->anchor_depth;
}

// 0x007242D0 (:1785)
// Sets this side's depth; the other side gets depth + 1 (forced) or the smaller of both, propagated to an rfg_mover
// that is not updating already.
void rfg_dyn_link_set_depth(rfg_mover* rm, rfg_subpiece* sp, unsigned short depth, bool force_other)
{
    rfg_dynamic_link* rdl = (sp != nullptr && sp->dynamic_link_index != 0xFFFF) ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
    if (rm == nullptr || rdl == nullptr)
    {
        return;
    }
    const int this_side = rm->get_handle() != rdl->mover_handle[1] ? 0 : 1;
    const int other_side = 1 - this_side;
    rdl->anchor_depth[this_side] = depth;
    const unsigned short next_depth = depth + 1;
    if (force_other)
    {
        rdl->anchor_depth[other_side] = next_depth;
        return;
    }
    const unsigned short old_depth = rdl->anchor_depth[other_side];
    const unsigned short new_depth = next_depth <= old_depth ? next_depth : old_depth;
    object_mover* other_mp;
    rfg_subpiece* other_sp;
    rfg_subpiece_base* other_base;
    if (new_depth != old_depth && rfg_dyn_link_get_linked_data(rm, sp, other_mp, other_sp, other_base) && other_mp != nullptr && other_mp->get_obj_type() == OT_MOVER && other_mp->get_sub_type() == OT_SUB_MOVER_RFG && !((rfg_mover*)other_mp)->flags.updating_anchors)
    {
        rfg_mover* other = (rfg_mover*)other_mp;
        rfg_dyn_link_update_depth(other, other->get_shape(), new_depth);
    }
}

// 0x007243B0 (:1822)
bool rfg_dyn_link_get_depth(rfg_mover* rm, rfg_subpiece* sp, unsigned short& this_depth, unsigned short& other_depth)
{
    rfg_dynamic_link* rdl = (sp != nullptr && sp->dynamic_link_index != 0xFFFF) ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
    if (rm == nullptr || rdl == nullptr)
    {
        return false;
    }
    const int this_side = rm->get_handle() != rdl->mover_handle[1] ? 0 : 1;
    this_depth = rdl->anchor_depth[this_side];
    other_depth = rdl->anchor_depth[1 - this_side];
    return true;
}

// 0x0072CA80 (:1842)
// Splits the pieces queued in the shape's shardify queue into connected groups (depth first through the remaining
// links, skipping breakable links of shatterable pieces). A group reaching a world anchor (with yield left), a piece
// known anchored (0x20), or, for a mover without anchors, its first piece, stays; a group held only by dynamic links
// stays when the link is not deeper than the other side (the depths are refreshed). The others become shards
// (info->q_shard, allocated from the context pool): nano and break-off pieces (0x80, base flag 0x2) lose their links
// first and the group is retried. Shards over 50 t / 250 t mark heavy destruction (flags 0x1 / 0x2). A movable mover
// with nothing left keeps its largest shard (2+ pieces) as itself.
static void rfg_dyn_collect_unanchored_pieces(rfg_shardify_info* shardify_info)
{
    rfg_mover* rm = shardify_info->rm;
    rfg_destroyable_shape* shape = rm->get_shape();
    rm->find_anchor_points();
    mempool_base* pool = get_this_context_virtual_mempool();
    shardify_info->num_q_shards = 0;
    rfg_shard_info* shard = (rfg_shard_info*)pool->alloc(sizeof(rfg_shard_info), 0);
    shardify_info->q_shard = shard;
    shard->reset(); // inlined
    unsigned short kept_list = 0xFFFF;
    rfg_subpiece* root = shape->child_obj_list != 0xFFFF ? &shape->get_object_buffer()[shape->child_obj_list] : nullptr;
    if ((shape->flags & 0x30) != 0 || root == nullptr || root->num_links == 0)
    {
        root = nullptr;
    }
    while (shape->shardify_q != 0xFFFF)
    {
        rfg_subpiece* start = &shape->get_object_buffer()[shape->shardify_q];
        shape->remove_subpiece_from_shardify_q(start);
        start->flags |= 8;
        unsigned short open_list = (unsigned short)(start - shape->get_object_buffer());
        start->shard_next = open_list;
        start->shard_prev = open_list;
        unsigned short group = 0xFFFF;
        bool anchored = false;
        bool linked = false;
        bool equal_depth = false;
        unsigned short min_depth = 0xFFFF;
        while (open_list != 0xFFFF)
        {
            rfg_subpiece* sp = &shape->get_object_buffer()[shape->get_object_buffer()[open_list].shard_prev]; // the last one
            shard_list_remove(shape, open_list, sp);
            shard_list_append(shape, group, sp);
            if ((sp->flags & 1) != 0 && 0 < sp->anchor_yield)
            {
                anchored = true;
                break;
            }
            rfg_dynamic_link* rdl = sp->dynamic_link_index != 0xFFFF ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
            bool supported_by_link = false;
            if (rdl != nullptr)
            {
                const int other_side = rm->get_handle() != rdl->mover_handle[1] ? 1 : 0;
                const unsigned short other_depth = rdl->anchor_depth[other_side];
                if (other_depth <= rdl->anchor_depth[1 - other_side])
                {
                    equal_depth |= other_depth == rdl->anchor_depth[1 - other_side];
                    linked = true;
                    if (other_depth <= min_depth)
                    {
                        min_depth = other_depth;
                    }
                    supported_by_link = true;
                }
            }
            if (!supported_by_link && sp == root)
            {
                anchored = true;
                break;
            }
            const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
            const rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
            const unsigned short* links = sp->links.to_pointer();
            bool reached_anchor = false;
            for (int l = sp->num_links - 1; l >= 0; l--)
            {
                const rfg_link_base* link_base = shape->get_link_base(links[l]);
                if ((link_base->flags & 4) != 0 && (sp_base->flags & 0x40) != 0)
                {
                    continue; // breakable link of a shatterable piece
                }
                rfg_subpiece* osp = linked_subpiece(shape, index, links[l]);
                if ((osp->flags & 0x20) != 0)
                {
                    reached_anchor = true;
                    break;
                }
                if ((osp->flags & 8) == 0)
                {
                    shape->remove_subpiece_from_shardify_q(osp);
                    osp->flags |= 8;
                    shard_list_append(shape, open_list, osp);
                }
            }
            if (reached_anchor)
            {
                anchored = true;
                break;
            }
        }
        if (group == 0xFFFF)
        {
            group = open_list;
        }
        else if (open_list != 0xFFFF)
        {
            shard_list_splice(shape, group, open_list);
        }
        const bool link_supported = linked && !anchored;
        const unsigned int mark = (anchored || linked) ? 0x20 : 0x40;
        bool links_removed = false;
        rfg_subpiece* sp = group != 0xFFFF ? &shape->get_object_buffer()[group] : nullptr;
        while (sp != nullptr)
        {
            rfg_subpiece* next = sp->shard_next != group ? &shape->get_object_buffer()[sp->shard_next] : nullptr;
            sp->flags |= mark;
            if ((mark & 0x40) == 0)
            {
                if (equal_depth && link_supported && sp->dynamic_link_index != 0xFFFF && RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) != nullptr)
                {
                    rfg_dyn_link_set_depth(rm, sp, min_depth + 1, rfg_dyn_link_get_state(rm, sp) == DLS_SUPPORTING); // inlined
                }
            }
            else
            {
                shard_list_remove(shape, group, sp);
                const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
                const rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
                shard->add_subpiece(shape, sp);
                if ((sp->flags & 0x80) != 0 || (sp_base->flags & 2) != 0)
                {
                    links_removed |= sp->num_links != 0;
                    while (sp->num_links != 0)
                    {
                        const unsigned short link_index = sp->links.to_pointer()[0];
                        shape->remove_link(shape->get_link(link_index));
                        rfg_subpiece* osp = linked_subpiece(shape, index, link_index);
                        osp->flags |= 0x800;
                        if ((osp->flags & 8) == 0)
                        {
                            shape->add_subpiece_to_shardify_q(osp);
                        }
                    }
                    sp->flags |= 0x880;
                }
            }
            sp = next;
        }
        if (shard->num_subpieces < 1)
        {
            if (kept_list == 0xFFFF)
            {
                kept_list = group;
            }
            else if (group != 0xFFFF)
            {
                shard_list_splice(shape, kept_list, group);
            }
        }
        else
        {
            if (links_removed)
            {
                // retry the pieces whose links broke as part of the next groups
                while (shard->q != 0xFFFF)
                {
                    rfg_subpiece* qsp = &shape->get_object_buffer()[shard->q];
                    qsp->flags &= ~0x48u;
                    shard->remove_subpiece(shape, qsp, false); // inlined
                    if ((qsp->flags & 0x800) != 0)
                    {
                        shape->add_subpiece_to_shardify_q(qsp);
                    }
                }
            }
            else
            {
                shard->finalize(shape);
                if (50000.0f < shard->mass)
                {
                    shardify_info->destruction_flags |= 1;
                    if (250000.0f < shard->mass)
                    {
                        shardify_info->destruction_flags |= 2;
                    }
                }
                shardify_info->num_q_shards++;
                pool->alloc(sizeof(rfg_shard_info), 1); // the next entry follows
                shard++;
            }
            shard->reset(); // inlined
        }
    }
    if (rm->get_move_type() == RFGM_FIXED)
    {
        if (kept_list == 0xFFFF)
        {
            return;
        }
    }
    else if (kept_list == 0xFFFF)
    {
        // a falling mover keeps its largest shard
        rfg_shard_info* best = nullptr;
        int best_index = -1;
        for (int i = 0; i < shardify_info->num_q_shards; i++)
        {
            rfg_shard_info* candidate = &shardify_info->q_shard[i];
            if (1 < candidate->num_subpieces && (best == nullptr || best->num_subpieces < candidate->num_subpieces))
            {
                best = candidate;
                best_index = i;
            }
        }
        if (best == nullptr)
        {
            return;
        }
        shardify_info->num_q_shards--;
        while (best->q != 0xFFFF)
        {
            rfg_subpiece* sp = &shape->get_object_buffer()[best->q];
            sp->modified_framecount = (unsigned char)(Framecount - 1);
            best->remove_subpiece(shape, sp, true); // inlined
        }
        if (best_index < shardify_info->num_q_shards)
        {
            *best = shardify_info->q_shard[shardify_info->num_q_shards];
        }
        return;
    }
    do
    {
        rfg_subpiece* sp = &shape->get_object_buffer()[kept_list];
        shard_list_remove(shape, kept_list, sp);
        sp->flags &= ~0x928u;
        sp->impulse_info_index = 0xFFFF;
    } while (kept_list != 0xFFFF);
}

// 0x00732620 (:2139)
// LTCG: shape in EDI, sp in ESI.
static void rfg_dyn_delete_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp)
{
    shape->fade_subpiece(sp, false);
    remove_all_links(shape, sp);
}

// 0x0071C080 (:2155)
// LTCG: objp in ECX. Heavy destruction of a fixed rfg_mover over 50 t (4), uber heavy over 250 t (0xC).
static int rfg_dyn_is_heavy_destruction(object* objp)
{
    if (objp == nullptr || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return 0;
    }
    rfg_mover* rm = (rfg_mover*)objp;
    if (rm->get_move_type() == RFGM_FIXED)
    {
        return 0; // kept: only movable movers count
    }
    rfg_destroyable_shape* shape = rm->get_shape();
    if (shape == nullptr)
    {
        return 0;
    }
    if (shape->mass <= 50000.0f)
    {
        return 0;
    }
    return shape->mass <= 250000.0f ? 4 : 0xC;
}

// 0x0072D680 (:2185)
// LTCG: rm in EAX, flags in ECX. Heavy destruction mode: a fixed mover stops its stress test for 3 s; a movable one
// keeps the mode 10 s (30 s and a slice timer when uber heavy).
static void set_heavy_destruction_flags(rfg_mover* rm, int flags)
{
    rfg_destroyable_shape* shape = rm->get_shape();
    if (shape == nullptr)
    {
        return;
    }
    shape->flags |= flags;
    if (rm->get_move_type() == RFGM_FIXED)
    {
        if (!shape->destruction_mode_timer.valid())
        {
            stress_evaluation_stop(rm);
        }
        shape->destruction_mode_timer.set(3000);
        return;
    }
    if ((shape->flags & 8) == 0)
    {
        shape->destruction_mode_timer.set(10000);
    }
    else
    {
        shape->destruction_mode_timer.set(30000);
        if (!rm->slice_delay.valid())
        {
            rm->slice_delay.set(1500);
        }
    }
}

// 0x00712990 (:2229)
// LTCG: havok_handle in EDI, local_pos in ESI, link_pos in EAX. Tips a shard over around the point it broke at: a
// vertical break (the shard above its link) pushes horizontally away from the link (randomly when right above it);
// a horizontal break pushes down. The impulse grows with the square root of the lever and the mass.
static void shard_apply_torque(const unsigned int havok_handle, const float mass, const vector& local_pos, const vector& link_pos, const vector& parent_pos, const matrix& parent_orient)
{
    if (havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    const vector diff = local_pos - link_pos;
    vector dir;
    float scale;
    float lever;
    if (diff.y <= fabsf(diff.x) || diff.y <= fabsf(diff.z))
    {
        dir = -UP_VECTOR;
        scale = Torque_width_ratio_scalar;
        lever = fabsf(diff.z) + fabsf(diff.x);
    }
    else
    {
        dir.set(diff.x, 0.0f, diff.z);
        const float horizontal = dir.mag_and_normalize_safe();
        scale = Torque_height_ratio_scalar;
        lever = diff.y;
        if (horizontal < 0.1f)
        {
            dir.x = fl_rand_range(-1.0f, 1.0f);
            dir.z = sqrtf(1.0f - dir.x * dir.x);
        }
    }
    vector impulse = dir * (sqrtf(lever) * scale * mass);
    impulse.unrotate(parent_orient);
    impulse.y = -fabsf(impulse.y);
    vector impulse_pos(local_pos.x + diff.x, local_pos.y + fabsf(diff.y), local_pos.z + diff.z);
    vec_change_to_global_coords(impulse_pos, parent_pos, parent_orient);
    havok_body_apply_point_impulse(havok_handle, impulse, impulse_pos);
}

// 0x00724420 (:2286)
// LTCG: objp in ECX. The effect material most of an rfg_mover's broken pieces (0x800) are made of; others report
// their own.
static unsigned char rfg_dyn_get_dominant_effect_material(object* objp)
{
    if (objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return objp->get_effect_material(0xFFFFFFFF, nullptr, nullptr);
    }
    rfg_destroyable_shape* shape = ((rfg_mover*)objp)->get_shape();
    if (shape == nullptr)
    {
        return 0xFF;
    }
    const int num_materials = rfg_get_effect_material_count();
    int* counts = (int*)_alloca(num_materials * sizeof(int));
    memset(counts, 0, num_materials * sizeof(int));
    unsigned char dominant = 0xFF;
    int dominant_count = 0;
    const unsigned short head = shape->child_obj_list;
    if (head != 0xFFFF)
    {
        unsigned short index = head;
        do
        {
            rfg_subpiece* sp = &shape->get_object_buffer()[index];
            if ((sp->flags & 0x800) != 0)
            {
                const unsigned char effect = rfg_get_effect_material_index(shape->get_subpiece_base(index)->physical_material_index); // inlined
                counts[effect]++; // kept: 0xFF (no effect material) is counted out of range
                if (dominant_count < counts[effect])
                {
                    dominant = effect;
                    dominant_count = counts[effect];
                }
            }
            index = sp->next;
        } while (index != head);
    }
    return dominant;
}

// Not in the binary: the glass part of rfg_dyn_shardify_setup (inlined there; VFX.md FX8). A deleted glass piece
// becomes a glass object (its render sub mesh, its pane transform from the piece position and its dlod orientation,
// the impact at the piece's box point nearest the shard impulse) and, with fewer than 128 glass effects playing, the
// material's shatter effect on the pane (facing along the impulse; tinted by the glass texture, scaled to the pane)
// and its detach effect at the impact along the impulse.
// Notes (decompiled behaviour, kept as is): the effect material index looks up a physical material for the shape
// type; the tint is left unset (size 0) for the detach effect when the shatter effect did not play; the mover's data
// is read without checking it is loaded.
static void rfg_dyn_shatter_glass_piece(rfg_mover* rm, rfg_destroyable_shape* shape, rfg_shard_info* shard, const rfg_subpiece_base* sp_base, unsigned int index, unsigned char effect_material)
{
    rfg_base_destroyable_shape* base = shape->parent;
    const int submesh_index = base->base_extra_data.to_pointer()[index].render_subpiece;
    const rfg_dlod_base& dlod = base->base_dlods.to_pointer()[(unsigned short)sp_base->dlod_key];
    const matrix43 subinst_xform(sp_base->pos, dlod.orient);
    vector impact_point;
    bbox_find_closest_point(impact_point, sp_base->prel_bmin, sp_base->prel_bmax, shard->impulse_pos);
    impact_point.unrotate(rm->orient);
    impact_point.x = impact_point.x + rm->pos.x;
    impact_point.y = rm->pos.y + impact_point.y;
    impact_point.z = rm->pos.z + impact_point.z;
    vector impact_force = shard->impulse;
    impact_force.unrotate(rm->orient);
    glass_obj* glass = glass_obj_create(rm, submesh_index, subinst_xform, impact_point, impact_force);
    if (glass != nullptr)
    {
        rm->add_glass_object(glass);
    }
    // Magic number: at most 128 glass effects at once.
    if (object_effect::get_num_effects_in_category(EFFECTS_CATEGORY_GLASS) >= 0x80)
    {
        return;
    }
    float largest = sp_base->prel_bmax.x - sp_base->prel_bmin.x;
    const float ey = sp_base->prel_bmax.y - sp_base->prel_bmin.y;
    const float ez = sp_base->prel_bmax.z - sp_base->prel_bmin.z;
    if (largest - ey < 0.0f)
    {
        largest = ey;
    }
    if (largest - ez < 0.0f)
    {
        largest = ez;
    }
    const rfg_effect_material* material = rfg_get_effect_material_by_index(effect_material);
    const material_shape_type shape_type = rfg_material_get_default_shape_type(effect_material);
    const material_event_size shatter_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_SHATTER, largest); // inlined
    const unsigned int shatter_effect = material->effect_candidate[MATERIAL_EVENT_SHATTER] != nullptr ? material->effect_candidate[MATERIAL_EVENT_SHATTER]->get_event_handle(shape_type, shatter_size, 0xFF, 0xFF, 0xFF) : 0xFFFFFFFF;
    const material_event_size detach_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_DETACH, largest); // inlined
    const unsigned int detach_effect = material->effect_candidate[MATERIAL_EVENT_DETACH] != nullptr ? material->effect_candidate[MATERIAL_EVENT_DETACH]->get_event_handle(shape_type, detach_size, 0xFF, 0xFF, 0xFF) : 0xFFFFFFFF;
    // The pane in the world (the sub instance turned to face forward, as glass_obj_create).
    matrix pane_orient = subinst_xform.m_rotation;
    // Magic number: about -pi/2 (rfg.exe's constant).
    pane_orient.rotate_around_local_x(-1.57077f);
    const vector& local = subinst_xform.m_translation;
    const matrix43 mover_xform(rm->pos, rm->orient);
    const matrix& mr = mover_xform.m_rotation;
    const vector pane_pos(mover_xform.m_translation.x + mr.rvec.x * local.x + mr.uvec.x * local.y + mr.fvec.x * local.z, mover_xform.m_translation.y + mr.rvec.y * local.x + mr.uvec.y * local.y + mr.fvec.y * local.z,
                          mover_xform.m_translation.z + mr.rvec.z * local.x + mr.uvec.z * local.y + mr.fvec.z * local.z);
    matrix43 pane(pane_pos, pane_orient * mr);
    if (pane.m_rotation.fvec.y * impact_force.y + pane.m_rotation.fvec.x * impact_force.x + pane.m_rotation.fvec.z * impact_force.z < 0.0f)
    {
        pane.m_rotation.fvec.set(-pane.m_rotation.fvec.x, -pane.m_rotation.fvec.y, -pane.m_rotation.fvec.z);
    }
    rl_color_float tint;
    float object_size = 0.0f;
    const unsigned int shatter_handle = effects_play(shatter_effect, pane.m_translation, pane.m_rotation, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_GLASS);
    object* shatter_obj = shatter_handle != 0xFFFFFFFF ? World.object_from_handle(shatter_handle) : nullptr;
    const float force_mag = sqrtf(impact_force.x * impact_force.x + impact_force.y * impact_force.y + impact_force.z * impact_force.z);
    if (shatter_obj != nullptr && shatter_obj->get_obj_type() == OT_EFFECT)
    {
        object_effect* shatter = (object_effect*)shatter_obj;
        rl_destroyable_model_instance* model = rm->rdd.get_data()->model_instance_p;
        glass_get_tint(model, submesh_index, tint);
        shatter->set_material_tint(tint);
        const rl_submesh_data& submesh = ((rl_mesh*)model->get_renderable())->get_mesh_data()->sub_meshes[submesh_index];
        shatter->set_expression_variable(EFFECTS_VAR_SCALE_X, (submesh.bmax.x - submesh.bmin.x) * 0.5f);
        shatter->set_expression_variable(EFFECTS_VAR_SCALE_Y, (submesh.bmax.z - submesh.bmin.z) * 0.5f);
        object_size = vec_dist(vector(submesh.bmin.x, 0.0f, submesh.bmin.y), vector(submesh.bmax.x, 0.0f, submesh.bmax.y));
        shatter->set_expression_variable(EFFECTS_VAR_OBJECT_SIZE, object_size);
        shatter->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, force_mag);
    }
    // Magic number: no force pushes along the pane's facing.
    if (force_mag < 0.0001f)
    {
        impact_force = pane.m_rotation.fvec;
    }
    matrix detach_orient;
    detach_orient.make(impact_force.copy_normalize());
    const unsigned int detach_handle = effects_play(detach_effect, impact_point, detach_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    object* detach_obj = detach_handle != 0xFFFFFFFF ? World.object_from_handle(detach_handle) : nullptr;
    if (detach_obj != nullptr && detach_obj->get_obj_type() == OT_EFFECT)
    {
        object_effect* detach = (object_effect*)detach_obj;
        detach->set_material_tint(tint);
        detach->set_expression_variable(EFFECTS_VAR_OBJECT_SIZE, object_size);
        detach->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, sqrtf(impact_force.x * impact_force.x + impact_force.y * impact_force.y + impact_force.z * impact_force.z));
    }
}

// 0x007380C0 (:2327)
// Collects the shards of a mover and prepares them: light shards (< 2 kg, at least 1 kg) fade, big ones (over 3 m or
// 10 m3) get a move type from their size and the heavy destruction flags; small ones are sorted (< 3 pieces first)
// and the load balance (rfgl_desired_values; in multiplayer the server's delete count) deletes some of them (flag 0x4)
// and fades the others, giving the quality budget to the medium ones. Then for each shard: a destruction event box
// (single player, fixed movers), dynamic links and shape state removed, deleted shards' pieces faded and unlinked with
// a load-balance dust effect. Destruction flags: 0x1/0x2 heavy, 0x20 far from the camera (75 m), 0x40 not visible,
// 0x800 large explosion. Returns false without shards.
//
// Glass pieces (base flag 0x40 with piece flag 0x80000000) shatter (rfg_dyn_shatter_glass_piece).
static bool rfg_dyn_shardify_setup(rfg_shardify_info* shardify_info)
{
    rfg_mover* rm = shardify_info->rm;
    rfg_destroyable_shape* shape = rm->get_shape();
    const bool fixed = rm->get_move_type() == RFGM_FIXED;
    multi_game_destruction_record_last_frame(rm);
    if (!fixed)
    {
        havok_body_get_pos_orient(rm->havok_handle, rm->pos, rm->orient);
    }
    rfg_dyn_collect_unanchored_pieces(shardify_info);
    if (shardify_info->num_q_shards == 0)
    {
        return false;
    }
    rm->flags.pristine = 0;
    if ((shape->flags & 4) != 0)
    {
        shardify_info->destruction_flags |= 1;
        if ((shape->flags & 8) != 0)
        {
            shardify_info->destruction_flags |= 2;
        }
    }
    if (rm->flags.large_explosion)
    {
        rm->flags.large_explosion = 0;
        shardify_info->destruction_flags |= 0x800;
    }
    if (!Multiplayer)
    {
        if (5625.0f < vec_dist_squared(rm->pos, Rfg_camera.real_pos)) // 75 m
        {
            shardify_info->destruction_flags |= 0x20;
        }
        vector bmin;
        vector bmax;
        rm->get_bbox(bmin, bmax);
        if (!game_render_is_object_visible(bmin, bmax))
        {
            shardify_info->destruction_flags |= 0x40;
        }
    }
    int num_small = 0;
    int num_medium = 0;
    int num_deleted = 0;
    for (int i = 0; i < shardify_info->num_q_shards; i++)
    {
        rfg_shard_info* shard = &shardify_info->q_shard[i];
        shard->move_type = RFGM_WORLD_ONLY;
        if (shard->mass < 2.0f)
        {
            if (shard->mass - 1.0f < 0.0f)
            {
                shard->mass = 1.0f;
            }
            shard->flags |= 8;
        }
        if ((shard->flags & 4) != 0)
        {
            num_deleted++;
            continue;
        }
        const float ex = shard->bmax.x - shard->bmin.x;
        const float ey = shard->bmax.y - shard->bmin.y;
        const float ez = shard->bmax.z - shard->bmin.z;
        float largest = 0.0f <= ex - ey ? ex : ey;
        if (0.0f <= ez - largest)
        {
            largest = ez;
        }
        if (Small_shard_length_limit < largest || Small_shard_volume_limit < ex * ey * ez)
        {
            shard->flags |= 0x10;
            shard->move_type = get_move_type_for_new_shard(shard->num_subpieces, shard->flags | shardify_info->destruction_flags); // inlined
        }
        else
        {
            int target;
            if (shard->num_subpieces < 3)
            {
                target = num_small++;
            }
            else
            {
                target = num_small + num_medium++;
            }
            if (i != target)
            {
                swap_dynamics(shardify_info->q_shard[target], shardify_info->q_shard[i]);
            }
        }
    }
    if (0 < num_small + num_medium)
    {
        rfgl_balance_values balance;
        int quality;
        if (box_is_single_player_or_acts_as_server_for_destruction()) // inlined
        {
            rfgl_desired_values(balance, shardify_info->num_q_shards, shardify_info->destruction_flags);
            balance.delete_count -= num_deleted;
            quality = balance.quality_count;
            if (0 < balance.quality_count && 0 < num_medium)
            {
                const int count = balance.quality_count < num_medium ? balance.quality_count : num_medium;
                for (int k = 0; k < count; k++)
                {
                    rfg_shard_info* shard = &shardify_info->q_shard[num_small + k];
                    shard->move_type = get_move_type_for_new_shard(shard->num_subpieces, shard->flags | shardify_info->destruction_flags); // inlined
                }
                quality = balance.quality_count - count;
            }
        }
        else
        {
            balance.delete_count = multi_game_destruction_get_client_balance_delete_count(rm);
            balance.quality_count = 0;
            quality = 0;
        }
        if (0 < num_small)
        {
            if (Multiplayer)
            {
                if (box_acts_as_server_for_destruction()) // inlined
                {
                    add_rfg_mover_shardify_delete_count_record(rm, balance.delete_count); // inlined
                }
                if (balance.delete_count <= 0)
                {
                    balance.delete_count = 0;
                }
            }
            // spread the quality move types and the deletes over the small shards
            int next_quality = shardify_info->num_q_shards;
            int quality_stride = 0;
            if (0 < quality)
            {
                quality_stride = (int)ceilf((float)num_small / (float)quality);
                next_quality = 0;
            }
            float delete_step = 0.0f;
            float next_delete = (float)shardify_info->num_q_shards;
            if (0 < balance.delete_count)
            {
                next_delete = 0.0f;
                delete_step = (float)num_small / (float)balance.delete_count;
            }
            for (int i = 0; i < num_small; i++)
            {
                rfg_shard_info* shard = &shardify_info->q_shard[i];
                if (i < (int)next_delete)
                {
                    if (i < next_quality)
                    {
                        shard->flags |= 8;
                    }
                    else
                    {
                        shard->move_type = get_move_type_for_new_shard(shard->num_subpieces, shard->flags | shardify_info->destruction_flags); // inlined
                        next_quality = quality_stride + i;
                    }
                }
                else
                {
                    shard->flags |= 4;
                    next_delete = (float)(i - (int)floorf(next_delete)) + delete_step + next_delete;
                }
            }
        }
    }
    vector region_min(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f); // FLT_MAX
    vector region_max(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
    for (int i = 0; i < shardify_info->num_q_shards; i++)
    {
        rfg_shard_info* shard = &shardify_info->q_shard[i];
        if (!Multiplayer && !wrecking_crew_is_active() && fixed && !rm->flags.fully_destroyed)
        {
            bool is_glass = false;
            if (shard->num_subpieces == 1)
            {
                rfg_subpiece* first = shard->q != 0xFFFF ? &shape->get_object_buffer()[shard->q] : nullptr;
                is_glass = (first->flags & 0x80000000) != 0;
            }
            rfg_destruction_event_add_box(shard->bmin, shard->bmax, rm->pos, rm->orient, rm->mover_is_type(MOVER_TYPE_BRIDGE) != 0, is_glass);
        }
        rfg_subpiece* sp = shard->q != 0xFFFF ? &shape->get_object_buffer()[shard->q] : nullptr;
        while (sp != nullptr)
        {
            rfg_subpiece* next = sp->shard_next != shard->q ? &shape->get_object_buffer()[sp->shard_next] : nullptr;
            if ((sp->flags & 2) != 0)
            {
                rfg_mover_delete_dynamic_link(rm, sp, true);
            }
            shape->set_shape_state(sp, false);
            if ((shard->flags & 4) != 0)
            {
                shape->fade_subpiece(sp, false);
                remove_all_links(shape, sp);
                shard->remove_subpiece(shape, sp, true); // inlined
                if (rfg_dyn_allow_destruction_effects() && (sp->flags & 0x200000) == 0) // inlined
                {
                    const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
                    const rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
                    const unsigned char effect_material = rfg_get_effect_material_index(sp_base->physical_material_index); // inlined
                    if ((sp_base->flags & 0x40) == 0)
                    {
                        if (fixed)
                        {
                            const vector effect_pos = sp_base->pos.copy_unrotate(rm->orient) + rm->pos;
                            const float ex = sp_base->prel_bmax.x - sp_base->prel_bmin.x;
                            const float ey = sp_base->prel_bmax.y - sp_base->prel_bmin.y;
                            const float ez = sp_base->prel_bmax.z - sp_base->prel_bmin.z;
                            float largest = ex - ey < 0.0f ? ey : ex;
                            if (largest - ez < 0.0f)
                            {
                                largest = ez;
                            }
                            const material_event_size size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_LOAD_BALANCE, largest); // inlined
                            collision_event coll_event;
                            coll_event.m_object_a = nullptr;
                            coll_event.m_object_b = nullptr;
                            // kept: the effect material index looks up a physical material
                            rfg_effect_candidate_allocate(&coll_event, effect_material, 0xFF, MATERIAL_EVENT_LOAD_BALANCE, size, rfg_material_get_default_shape_type(effect_material), 1.0f, effect_pos, false, 0, 0.0f, nullptr, 0xFFFFFFFF, 0xFF);
                        }
                    }
                    else if ((sp->flags & 0x80000000) != 0)
                    {
                        rfg_dyn_shatter_glass_piece(rm, shape, shard, sp_base, index, effect_material);
                    }
                }
            }
            sp = next;
        }
        if (fixed)
        {
            bbox_expand(region_min, region_max, shard->bmin, shard->bmax);
        }
    }
    if (fixed)
    {
        havok_activate_region(rm->pos, rm->orient, region_min, region_max);
        return true;
    }
    havok_body_force_activate(rm->havok_handle);
    return true;
}

// 0x0073CDF0 (:2695)
// Creates the shard objects: single pieces become general movers (deleted when that fails) and may drop salvage
// (more the more destroyed the building is, all of it once killed, Salvage_spawn_dead_prob for movable movers);
// groups become new rfg_movers on a new shape built from the old one's base (same tree), getting a share of the
// salvage. A group that cannot be created is deleted.
static void rfg_dyn_shardify_work(rfg_shardify_info* shardify_info)
{
    rfg_mover* old_mover = shardify_info->rm;
    rfg_destroyable_shape* shape = old_mover->get_shape();
    for (int i = 0; i < shardify_info->num_q_shards; i++)
    {
        rfg_shard_info* shard = &shardify_info->q_shard[i];
        if (shard->num_subpieces <= 0)
        {
            continue;
        }
        if (shard->num_subpieces == 1)
        {
            rfg_subpiece* sp = shard->q != 0xFFFF ? &shape->get_object_buffer()[shard->q] : nullptr;
            shard->remove_subpiece(shape, sp, true);
            const bool fade_out = (shard->flags & 8) != 0 || shard->move_type == RFGM_NO_COLLISION;
            general_mover* gm = rfg_dyn_create_shard(old_mover, shard->move_type, fade_out, sp);
            shard->mp = gm;
            if (gm == nullptr)
            {
                shape->fade_subpiece(sp, false);
                remove_all_links(shape, sp);
            }
            if ((salvage_should_spawn() || old_mover->flags.mining) && 0 < old_mover->salvage_num_pieces && shard->mp != nullptr)
            {
                const rfg_physical_material* material = rfg_get_physical_material_by_index(shard->mp->get_physical_material(0xFFFFFFFF, nullptr, nullptr)); // inlined
                if (material != nullptr && (material->flags & 1) == 0)
                {
                    float count = 1.0f;
                    if (shape->parent != nullptr && 0 < shape->parent->num_objects)
                    {
                        count = (1.0f - (float)shape->num_existing_child_shapes / (float)shape->parent->num_objects) / old_mover->game_destroyed_pct;
                        count = count * count;
                    }
                    count = (float)old_mover->salvage_num_pieces * count;
                    if ((old_mover->mover_flags & 4) == 0)
                    {
                        if (old_mover->is_killed())
                        {
                            count = (float)old_mover->salvage_num_pieces;
                        }
                    }
                    else if (Salvage_spawn_dead_prob - 1.0f < 0.0f)
                    {
                        count = 1.0f;
                    }
                    else
                    {
                        count = Salvage_spawn_dead_prob;
                    }
                    for (; 1.0f <= count || fl_rand() <= count; count -= 1.0f)
                    {
                        item* salvage = salvage_create_item(old_mover->gameplay_props->salvage_material, shard->mp->pos, shard->mp->orient);
                        if (salvage == nullptr)
                        {
                            continue;
                        }
                        vector impulse = shardify_info->q_shard->impulse; // the first shard's impulse
                        impulse.x = impulse.x - -1000.0f < 0.0f ? -1000.0f : (0.0f <= impulse.x - 1000.0f ? 1000.0f : impulse.x);
                        impulse.y = impulse.y - -1000.0f < 0.0f ? -1000.0f : (0.0f <= impulse.y - 1000.0f ? 1000.0f : impulse.y);
                        impulse.z = impulse.z - -1000.0f < 0.0f ? -1000.0f : (0.0f <= impulse.z - 1000.0f ? 1000.0f : impulse.z);
                        impulse.x += 0.0f;
                        impulse.y += 3.0f; // and up
                        impulse.z += 0.0f;
                        havok_body_apply_point_impulse(salvage->havok_handle, impulse, shard->mp->pos);
                        if (--old_mover->salvage_num_pieces == 0)
                        {
                            break;
                        }
                    }
                }
            }
            continue;
        }
        rfg_destroyable_shape* new_shape = rfg_shape_new(shape->parent, old_mover->chunk_uid, old_mover->pos, old_mover->orient, old_mover->shape_uid);
        while (shard->q != 0xFFFF)
        {
            rfg_subpiece* sp = &shape->get_object_buffer()[shard->q];
            shard->remove_subpiece(shape, sp, true); // inlined
            new_shape->set_shape_state(sp, true);
        }
        new_shape->aabb_tree = shape->aabb_tree;
        if (shape->aabb_tree != shape->parent->aabb_tree.to_pointer())
        {
            new_shape->flags |= 0x800; // the tree must be rebuilt
        }
        new_shape->local_bmin = shard->bmin;
        new_shape->local_bmax = shard->bmax;
        new_shape->recalc_shape_properties();
        if (!Multiplayer)
        {
            new_shape->flags |= (shardify_info->destruction_flags & 3) * 4; // heavy destruction 0x4 / 0x8
        }
        c_chunk_base* chunk = chunk_get_base_by_uid(old_mover->chunk_uid);
        obj_rfgm_props pblock(old_mover, shard->move_type, chunk->filename, new_shape, rfg_dyn_get_rfg_mover_destruction_uid()); // uid inlined
        shard->mp = World.create_object<rfg_mover>(pblock, 0xFFFFFFFF);
        if (shard->mp == nullptr || !shard->mp->obj_flags.streamed)
        {
            while (new_shape->child_obj_list != 0xFFFF)
            {
                rfg_subpiece* sp = &new_shape->get_object_buffer()[new_shape->child_obj_list];
                new_shape->set_shape_state(sp, false);
                shape->fade_subpiece(sp, false);
                remove_all_links(shape, sp);
            }
            if (shard->mp != nullptr)
            {
                World.destroy_object(shard->mp, false);
                shard->mp = nullptr;
            }
            new_shape->delete_me();
            new_shape->removeReference();
            continue;
        }
        rfg_dyn_do_mode_specific_processing(shard->mp);
        shard->mp->gameplay_props = old_mover->gameplay_props;
        if (Multiplayer && box_acts_as_server_for_destruction() && shard->move_type == RFGM_WORLD_ONLY) // inlined
        {
            object_mover_mark_for_fade(shard->mp, int_rand_range(1000, 2000));
        }
        activity_register_damage_mark_objects(old_mover, shard->mp);
        if (0 < old_mover->salvage_num_pieces && shard->mp != nullptr)
        {
            rfg_mover* new_mover = World.object_from_handle<rfg_mover>(shard->mp->get_handle());
            const int share = (old_mover->salvage_num_pieces * new_shape->num_existing_child_shapes) / (shape->num_existing_child_shapes + new_shape->num_existing_child_shapes);
            old_mover->salvage_num_pieces -= share;
            new_mover->salvage_num_pieces = share; // not checked
        }
    }
}

// 0x0073F6E0 (:2881)
// Finishes the shards: they inherit the mover's velocities (movable movers), the creator chain's root (for their
// render instance), heavy destruction and attached objects (new rfg_movers), get their impulse (or a tipping torque for
// big shards of fixed buildings), dust trails (a third of the fast moving pieces) and groan or detach effects. The
// mover itself is destroyed when nothing (or one piece of a movable mover) is left; otherwise its mass, tree, inertia
// (a box) and collision filter are refreshed and a fixed mover gets a priority stress test, a movable one a lighter
// collision type when it became small.
static void rfg_dyn_shardify_cleanup(rfg_shardify_info* shardify_info)
{
    rfg_mover* rm = shardify_info->rm;
    rfg_destroyable_shape* shape = rm->get_shape();
    vector linear_velocity;
    vector angular_velocity;
    havok_body_get_linear_velocity(rm->havok_handle, linear_velocity);
    havok_body_get_angular_velocity(rm->havok_handle, angular_velocity);
    object_mover* root = rm;
    while (root->creator_handle != 0xFFFFFFFF)
    {
        root = World.object_from_handle<object_mover>(root->creator_handle); // not checked
    }
    for (int i = 0; i < shardify_info->num_q_shards; i++)
    {
        rfg_shard_info* shard = &shardify_info->q_shard[i];
        object_mover* mp = shard->mp;
        if (mp == nullptr || mp->havok_handle == 0xFFFFFFFF)
        {
            continue;
        }
        mp->obj_flags.no_save = rm->obj_flags.no_save;
        if (mp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* new_mover = (rfg_mover*)mp;
            rfg_destroyable_shape* new_shape = new_mover->get_shape();
            new_shape->recalc_rbb_tree();
            add_rfg_mover_to_created_list(new_mover); // inlined
            new_mover->flags.pristine = 0;
            if ((shardify_info->destruction_flags & 1) != 0)
            {
                set_heavy_destruction_flags(new_mover, shape->flags);
            }
            transfer_attached_objects(rm, new_mover, new_shape->child_obj_list);
            transfer_script_point_effect_insts(rm, new_mover, new_shape->child_obj_list);
        }
        mp->creator_handle = root->get_handle();
        mp->set_render_created();
        if ((shard->flags & 0x10) != 0)
        {
            mp->mover_flags |= 2;
        }
        havok_body_force_activate(mp->havok_handle);
        if (rm->get_move_type() != RFGM_FIXED)
        {
            havok_body_set_linear_velocity(mp->havok_handle, linear_velocity);
            havok_body_set_angular_velocity(mp->havok_handle, angular_velocity);
        }
        material_event_type effect_type = INVALID_MATERIAL_EVENT;
        unsigned char effect_material = 0xFF;
        material_event_size effect_size = INVALID_MATERIAL_EVENT_SIZE;
        material_shape_type effect_shape = INVALID_MATERIAL_EVENT_SHAPE_TYPE;
        bool play_effect = false;
        if (rm->get_move_type() == RFGM_FIXED && (shard->flags & 0x480) != 0)
        {
            play_effect = true;
            effect_material = rfg_dyn_get_dominant_effect_material(mp);
            const material_event_size groan_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_GROAN, shard->mass); // inlined
            effect_size = groan_size;
            effect_shape = rfg_material_get_shape_type(shard->bmin, shard->bmax); // inlined
            bool groan = false;
            if (groan_size != INVALID_MATERIAL_EVENT_SIZE)
            {
                const rfg_effect_material* material = rfg_get_effect_material_by_index(effect_material);
                if (material->effect_candidate[MATERIAL_EVENT_GROAN] != nullptr && material->effect_candidate[MATERIAL_EVENT_GROAN]->get_event_handle(effect_shape, groan_size, 0xFF, 0xFF, 0xFF) != 0xFFFFFFFF)
                {
                    effect_type = MATERIAL_EVENT_GROAN;
                    groan = true;
                }
            }
            if (!groan)
            {
                effect_type = MATERIAL_EVENT_DETACH;
                const float ex = shard->bmax.x - shard->bmin.x;
                const float ey = shard->bmax.y - shard->bmin.y;
                const float ez = shard->bmax.z - shard->bmin.z;
                float largest = ex - ey < 0.0f ? ey : ex;
                if (0.0f <= ez - largest)
                {
                    largest = ez;
                }
                const rfg_effect_material* material = rfg_get_effect_material_by_index(effect_material);
                if (material->effect_candidate[MATERIAL_EVENT_DETACH] == nullptr)
                {
                    effect_size = INVALID_MATERIAL_EVENT_SIZE;
                    play_effect = false;
                }
                else
                {
                    effect_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_DETACH, largest); // inlined
                    // heavy destruction skips the small (and, uber heavy, the medium) detach effects
                    play_effect = ((shard->flags & 1) == 0 || effect_size != MATERIAL_EVENT_SIZE_SMALL) && ((shard->flags & 2) == 0 || MATERIAL_EVENT_SIZE_MEDIUM < effect_size);
                }
            }
        }
        if ((shard->flags & 0x400) == 0 || shard->impulse * shard->impulse <= shard->mass)
        {
            if (rm->get_move_type() == RFGM_FIXED && (shard->flags & 0x10) != 0 && mp->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                rfg_destroyable_shape* new_shape = ((rfg_mover*)mp)->get_shape();
                shard_apply_torque(mp->havok_handle, shard->mass, new_shape->center_of_mass, shard->average_pos, rm->pos, rm->orient);
            }
        }
        else
        {
            shard->impulse.unrotate(rm->orient);
            vec_change_to_global_coords(shard->impulse_pos, rm->pos, rm->orient);
            rfg_dyn_apply_point_impulse(mp->havok_handle, shard->impulse, shard->impulse_pos, shard->mass);
            if (mp->get_sub_type() == OT_SUB_MOVER_GENERAL && vrand() % 100 < 34 && 0 < Destruction_trail_pool.get_free())
            {
                const rfg_physical_material* material = rfg_get_physical_material_by_index(mp->get_physical_material(0xFFFFFFFF, nullptr, nullptr));
                unsigned int trail = material->effect_material->trail_effects[0];
                bool nano = false;
                if ((shard->flags & 0x200) != 0 && (material->flags & 0x40) != 0 && material->effect_material->trail_effects[1] != 0xFFFFFFFF && int_rand_chance(25))
                {
                    trail = material->effect_material->trail_effects[1];
                    nano = true;
                }
                vector velocity;
                if (trail != 0xFFFFFFFF && (havok_body_get_linear_velocity(mp->havok_handle, velocity), 16.0f < velocity.z * velocity.z + velocity.x * velocity.x))
                {
                    rfg_destruction_trail* t = Destruction_trail_pool.alloc(0);
                    t->trail_handle = trail;
                    t->rfg_effect_material_p = material->effect_material;
                    t->owner_handle = mp->get_handle();
                    t->lifetime.invalidate();
                    t->flags = nano ? 2 : 0;
                }
            }
            if (effect_type == MATERIAL_EVENT_DETACH)
            {
                play_effect = play_effect && 0 < (int)effect_size;
            }
        }
        if (play_effect)
        {
            const vector effect_pos = shard->average_pos.copy_unrotate(rm->orient) + rm->pos;
            rfg_effect_candidate_create(effect_material, effect_pos, effect_type, effect_size, effect_shape, false, 0, nullptr, nullptr, 0xFFFFFFFF, 0xFF, 0xFF);
        }
    }
    rm->do_frame_report();
    shape->recalc_shape_properties();
    if ((shape->mass <= 1.0f && shape->mass != 1.0f) || shape->num_existing_child_shapes == 0 || (shape->num_existing_child_shapes == 1 && rm->get_move_type() != RFGM_FIXED))
    {
        add_rfg_mover_to_destroyed_list(rm); // inlined
        rm->remote_obj_flags.client_destroy_permission = true;
        rm->remote_obj_flags.no_destroy_packet = true;
        if (rm->mp_info != nullptr)
        {
            rm->mp_info->destroy_causes.destruction = true;
        }
        World.destroy_object(rm, false);
        havok_activate_region(rm->pos, rm->orient, shape->local_bmin, shape->local_bmax);
        return;
    }
    shape->recalc_rbb_tree();
    rfg_destroyable_shape* current = rm->get_shape();
    if (rm->havok_handle != 0xFFFFFFFF && current != nullptr)
    {
        // a box of the shape's extents
        const float m = current->mass * 0.33333334f;
        const float hx = (current->local_bmax.x - current->local_bmin.x) * 0.5f;
        const float hy = (current->local_bmax.y - current->local_bmin.y) * 0.5f;
        const float hz = (current->local_bmax.z - current->local_bmin.z) * 0.5f;
        matrix tensor;
        tensor.rvec.set((hz * hz + hy * hy) * m, 0.0f, 0.0f);
        tensor.uvec.set(0.0f, (hz * hz + hx * hx) * m, 0.0f);
        tensor.fvec.set(0.0f, 0.0f, (hy * hy + hx * hx) * m);
        havok_body_set_mass_props(rm->havok_handle, current->mass, current->center_of_mass, tensor);
    }
    validate_attached_objects(rm, shape);
    if (rm->get_move_type() == RFGM_FIXED || (shape->flags & 4) == 0)
    {
        havok_entity_update_collision_filter(havok_body_get_pointer(rm->havok_handle), false, true);
    }
    if (rm->get_move_type() == RFGM_FIXED)
    {
        if ((shardify_info->destruction_flags & 1) != 0)
        {
            set_heavy_destruction_flags(rm, 4);
        }
        rm->flags.under_stress = 0;
        stress_evaluation_start(rm, false, true); // inlined
        return;
    }
    if ((rm->mover_flags & 2) != 0)
    {
        vector bmin;
        vector bmax;
        rm->get_bbox(bmin, bmax);
        const float ex = bmax.x - bmin.x;
        const float ey = bmax.y - bmin.y;
        const float ez = bmax.z - bmin.z;
        float largest = 0.0f <= ex - ey ? ex : ey;
        if (0.0f <= ez - largest)
        {
            largest = ez;
        }
        if (largest <= Small_shard_length_limit && ex * ey * ez <= Small_shard_volume_limit)
        {
            rm->mover_flags &= ~2u; // no longer large
        }
    }
    if (rm->get_move_type() == RFGM_NORMAL)
    {
        const rfg_move_type move_type = get_move_type_for_new_shard(shape->num_existing_child_shapes, (rm->mover_flags & 2) != 0 ? 0x10 : 0); // inlined
        if (move_type != RFGM_NORMAL)
        {
            rm->set_move_type(move_type, false); // inlined
        }
    }
}

// 0x00712CB0 (:3137)
static material_event_size get_rand_explosion_size()
{
    int roll = rand() % 100;
    for (int size = MATERIAL_EVENT_SIZE_SMALL; size < NUM_MATERIAL_EVENT_SIZES; size++)
    {
        if (roll < Explosion_pct[size])
        {
            return (material_event_size)size;
        }
        roll -= Explosion_pct[size];
    }
    return MATERIAL_EVENT_SIZE_SMALL;
}

// 0x00712CE0 (:3155)
// Not on multiplayer clients (or without Mp_allow_slice), and at most every 10 frames.
static bool rfg_dyn_can_slice()
{
    if (Multiplayer && (!Mp_allow_slice || !multi_is_server() || multi_game_is_migrating()))
    {
        return false;
    }
    return RFG_uber_heavy_destruction_slice_min_delay <= Framecount - RFG_uber_heavy_destruction_slice_framecount;
}

// 0x007399E0 (:3176)
// LTCG: shape in ECX, hit_pos in EDX. Slices a heavy building at hit_pos with a random tilt (up to 45 degrees):
// tall ones horizontally (not too close to the top or the bottom), others along x or z; only shapes at least 7.5 m
// along the cut axis. Immediate slices are applied at once, others queued as events.
static bool rfg_dyn_slice_shape(rfg_mover* rm, rfg_destroyable_shape* shape, const vector& hit_pos, bool immediate)
{
    if (!rfg_dyn_can_slice()) // inlined
    {
        return false;
    }
    rm->slice_delay.set_rand(500, 1500);
    float bmin[3] = {shape->local_bmin.x, shape->local_bmin.y, shape->local_bmin.z};
    float values[3] = {shape->local_bmax.x, shape->local_bmax.y, shape->local_bmax.z}; // the extents, then the angles
    const float height = values[1] - bmin[1];
    int axis;
    if (height <= values[0] - bmin[0] || height <= values[2] - bmin[2])
    {
        axis = (rand() & 1) * 2; // x or z
    }
    else
    {
        axis = 1;
        if (hit_pos.y < bmin[1] + Slice_tall_safe_zone * height || values[1] - Slice_tall_safe_zone * height < hit_pos.y)
        {
            return false;
        }
    }
    if (values[axis] - bmin[axis] < 7.5f)
    {
        return false;
    }
    RFG_uber_heavy_destruction_slice_framecount = Framecount;
    values[axis] = fl_rand_range(-0.7853982f, 0.7853982f); // pi / 4
    values[(axis + 1) % 3] = 0.0f;
    const float angle1 = fl_rand_range(-0.7853982f, 0.7853982f);
    values[(axis + 2) % 3] = angle1;
    if (immediate)
    {
        rfg_collision_event ev;
        rfg_dyn_add_collide_slice_immediate(rm, &ev, hit_pos, axis, values[axis], angle1);
        return rfg_dyn_apply_slice(&ev);
    }
    rfg_dyn_add_pre_collide_slice(rm, hit_pos, axis, values[axis], angle1, false); // inlined
    return true;
}

// 0x00732650 (:3235)
// LTCG: shape in EDI, damage in XMM0. Shatterable pieces (base flag 0x40) within a radius growing with the square root
// of the damage lose their links and dynamic links and fly away from the hit (along the hit direction), with an impulse
// falling off with the distance (between 1 and 10 times their mass; randomized in single player).
static void rfg_dyn_shatter(rfg_mover* rm, rfg_destroyable_shape* shape, float damage, const vector& local_pos, const vector& local_dir)
{
    if (rm == nullptr || shape == nullptr)
    {
        return;
    }
    const float impulse_scale = Shatter_impulse_scalar * sqrtf(damage);
    const float min_radius = Multiplayer ? 0.1f : 0.0f;
    float radius = Shatter_radius_scalar * sqrtf(damage);
    if (radius - min_radius < 0.0f)
    {
        radius = min_radius;
    }
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, local_pos, radius, hits);
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(key);
        if ((sp_base->flags & 0x40) == 0)
        {
            continue;
        }
        rfg_subpiece* sp = &shape->get_object_buffer()[key];
        rfg_mover_delete_dynamic_link(rm, sp, true);
        shape->add_subpiece_to_shardify_q(sp);
        vector dir = sp_base->pos - local_pos;
        const float dist = dir.mag_and_normalize_safe();
        dir += local_dir;
        const float rnd = Multiplayer ? 1.0f : fl_rand_range(0.75f, 1.25f);
        float magnitude = (1.0f - dist / radius) * impulse_scale * rnd;
        if (magnitude - sp_base->mass < 0.0f)
        {
            magnitude = sp_base->mass;
        }
        if (0.0f <= magnitude - sp_base->mass * 10.0f)
        {
            magnitude = sp_base->mass * 10.0f;
        }
        sp->add_impulse(dir * magnitude, sp_base->pos);
        while (sp->num_links != 0)
        {
            const unsigned short link_index = sp->links.to_pointer()[0];
            shape->remove_link(shape->get_link(link_index));
            shape->add_subpiece_to_shardify_q(linked_subpiece(shape, key, link_index));
        }
    }
}

// 0x00712D20 (:3316)
// LTCG: num_events_to_process in ESI. The server's running total of destruction events over the last 360 frames.
static void rfg_dyn_record_events_processed(const unsigned int num_events_to_process)
{
    if (Multiplayer && multi_is_server() && Destruction_event_history != nullptr)
    {
        Destruction_event_history_running_total += num_events_to_process - Destruction_event_history[Destruction_event_history_index];
        Destruction_event_history_index = (Destruction_event_history_index + 1) % 360;
        Destruction_event_history[Destruction_event_history_index] = num_events_to_process;
    }
}

// 0x00712D70 (:3339)
// LTCG: num_delayed_events in ESI.
static void rfg_dyn_record_events_delayed(const unsigned int num_delayed_events)
{
    if (Multiplayer && multi_is_server() && num_delayed_events != 0 && Destruction_event_history != nullptr)
    {
        Destruction_event_history_running_total -= num_delayed_events;
        Destruction_event_history[Destruction_event_history_index] -= num_delayed_events;
    }
}

// 0x00712DA0 (:3354)
// The server allows low priority events while the last 360 frames held fewer than 450.
static bool rfg_dyn_allow_low_priority_events()
{
    if (Multiplayer && multi_is_server() && !multi_game_is_migrating())
    {
        return Destruction_event_history_running_total < 450;
    }
    return true;
}

// 0x00732A30 (:3368)
// LTCG: movep in EAX. Damage to one piece: its dynamic link loses the damage (deleted when broken), its world anchor
// loses it (pieces to delete, 0x80, lose it all; anchored small pieces only take link damage); then each breakable
// link loses Link_area_damage_scalar x damage scaled by its area and by the distance of the link point from pos (the
// pieces' overlap centre, or the closest point to the neighbour), or 253/256 of its strength when weakening
// (0x1000). Pieces to delete and forced breaks take whole links. Broken links queue both pieces for shardify
// (explosions mark them 0x100) and push the neighbour (impulse_func, brittleness x overkill). Returns whether anything
// broke.
static bool rfg_dyn_apply_damage_to_subpiece(rfg_mover* movep, rfg_subpiece* sp, const unsigned int damage, const vector& pos, const destruction_event_type damage_type, bool weaken_structure, bool force_break_links, void (*impulse_func)(unsigned int, rfg_subpiece*, rfg_subpiece_base*, const vector&, const vector&), const vector* dir)
{
    rfg_destroyable_shape* shape = movep->get_shape();
    const unsigned int index = (unsigned int)(sp - shape->get_object_buffer());
    rfg_subpiece_base* sp_base = shape->get_subpiece_base(index);
    bool broke = false;
    rfg_dynamic_link* rdl = (sp != nullptr && sp->dynamic_link_index != 0xFFFF) ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
    if (rdl != nullptr && (sp_base->flags & 0x20) == 0)
    {
        rdl->yield_limit -= (int)damage;
        if (rdl->yield_limit < 1)
        {
            broke = true;
            rfg_mover_delete_dynamic_link(movep, sp, true);
        }
    }
    if (0 < sp->anchor_yield)
    {
        bool released = false;
        if ((sp_base->flags & 0x20) == 0)
        {
            if ((sp->flags & 0x80) != 0)
            {
                sp->anchor_yield = -1;
                released = true;
            }
            else
            {
                sp->anchor_yield -= (int)damage;
            }
        }
        if (released || sp->anchor_yield <= 0)
        {
            broke = true;
            shape->add_subpiece_to_shardify_q(sp);
            sp->flags |= 0x80;
        }
        else
        {
            // anchored pieces bigger than a small shard keep their links
            const float ex = sp_base->prel_bmax.x - sp_base->prel_bmin.x;
            const float ey = sp_base->prel_bmax.y - sp_base->prel_bmin.y;
            const float ez = sp_base->prel_bmax.z - sp_base->prel_bmin.z;
            float largest = 0.0f <= ex - ey ? ex : ey;
            if (0.0f <= ez - largest)
            {
                largest = ez;
            }
            if (Small_shard_length_limit < largest || Small_shard_volume_limit < ex * ey * ez)
            {
                return broke;
            }
        }
    }
    unsigned int link_damage = Link_area_damage_scalar * damage;
    if (0x7FFFFF <= link_damage)
    {
        link_damage = 0x7FFFFF;
    }
    link_damage <<= 8; // fixed point x256
    const float diagonal_sqr = vec_dist_squared(sp_base->prel_bmin, sp_base->prel_bmax);
    for (int l = sp->num_links - 1; l >= 0; l--)
    {
        const unsigned short link_index = sp->links.to_pointer()[l];
        const rfg_link_base* link_base = shape->get_link_base(link_index);
        if ((link_base->flags & 2) != 0)
        {
            continue; // unbreakable
        }
        rfg_link* link = shape->get_link(link_index);
        long long amount;
        if (weaken_structure)
        {
            sp->flags |= 0x1000;
            amount = (long long)(link->yield_limit << 8) * 253;
        }
        else
        {
            rfg_subpiece* osp = linked_subpiece(shape, index, link_index);
            const rfg_subpiece_base* other_base = shape->get_subpiece_base((unsigned int)(osp - shape->get_object_buffer()));
            vector imin;
            vector imax;
            vector link_point;
            if (ix_box_box_intersect(sp_base->prel_bmin, sp_base->prel_bmax, other_base->prel_bmin, other_base->prel_bmax, imin, imax))
            {
                link_point.set((imin.x + imax.x) * 0.5f, (imin.y + imax.y) * 0.5f, (imax.z + imin.z) * 0.5f);
            }
            else
            {
                bbox_find_closest_point(link_point, sp_base->prel_bmin, sp_base->prel_bmax, other_base->pos);
            }
            float falloff = (1.0f - vec_dist_squared(pos, link_point) / diagonal_sqr) * link_base->area;
            if (falloff - 0.01f < 0.0f)
            {
                falloff = 0.01f;
            }
            if (0.0f <= falloff - 1.0f)
            {
                falloff = 1.0f;
            }
            amount = (long long)(int)(falloff * 256.0f) * (long long)(int)link_damage;
        }
        int yield_damage = (int)(amount >> 16);
        if (((sp->flags & 0x80) != 0 || force_break_links) && yield_damage < link->yield_limit)
        {
            yield_damage = -1;
        }
        if (yield_damage < 0)
        {
            yield_damage = link->yield_limit; // the whole link
        }
        link->yield_limit -= yield_damage;
        if (link->yield_limit < 1)
        {
            broke = true;
            shape->remove_link(link);
            rfg_subpiece* osp = linked_subpiece(shape, index, link_index);
            if (damage_type == DET_EXPLOSION)
            {
                osp->flags |= 0x100;
            }
            shape->add_subpiece_to_shardify_q(osp);
            if (impulse_func != nullptr)
            {
                const rfg_physical_material* material = rfg_get_physical_material_by_index(sp_base->physical_material_index); // inlined
                const unsigned int overkill = (unsigned int)(long long)(material->brittleness * (float)-link->yield_limit);
                impulse_func(overkill, osp, shape->get_subpiece_base((unsigned int)(osp - shape->get_object_buffer())), pos, *dir);
            }
        }
    }
    if (broke)
    {
        shape->add_subpiece_to_shardify_q(sp);
        if (damage_type == DET_EXPLOSION)
        {
            sp->flags |= 0x100;
        }
    }
    if (impulse_func != nullptr && (sp->flags & 0x10) != 0)
    {
        impulse_func(damage, sp, sp_base, pos, *dir);
    }
    return broke;
}

// 0x00712DE0 (:3560)
// LTCG: ma in XMM1, va in EAX, mb in XMM0, vb in ESI, v_cm_b in EDI. Squared velocities in the centre of mass frame (or
// the plain squared velocities without valid masses).
static void rfg_dyn_calc_vel_sqr_cm(float ma, const vector& va, float mb, const vector& vb, float& v_cm_a, float& v_cm_b)
{
    if (0.0f <= ma && 0.0f <= mb)
    {
        const float total = ma + mb;
        const vector v_cm((va.x * ma + vb.x * mb) / total, (va.y * ma + vb.y * mb) / total, (va.z * ma + vb.z * mb) / total);
        v_cm_a = vec_dist_squared(va, v_cm);
        v_cm_b = vec_dist_squared(vb, v_cm);
        return;
    }
    v_cm_a = va.y * va.y + va.x * va.x + va.z * va.z;
    v_cm_b = vb.x * vb.x + vb.y * vb.y + vb.z * vb.z;
}

// 0x00712FB0 (:3580)
// LTCG: ma in XMM0, v_cm_a in XMM2. Kinetic energy in the centre of mass frame (bodies without mass add nothing).
static float rfg_dyn_calc_energy(float ma, float v_cm_a, float mb, float v_cm_b)
{
    const float ea = 0.0f < ma ? ma * v_cm_a : 0.0f;
    const float eb = 0.0f < mb ? mb * v_cm_b : 0.0f;
    return (eb + ea) * 0.5f;
}

// 0x00732FF0 (:3596)
// LTCG: movep in EAX, subpiece_index in ECX. Damage to a piece still in the mover: pieces to delete are flagged (0x80);
// heavy destruction multiplies the damage in single player. Returns the piece (nullptr when it left).
static rfg_subpiece* rfg_dyn_apply_stress_to_subpiece(rfg_mover* movep, const int subpiece_index, unsigned int damage, const vector& stress_pos, const destruction_event_type det, bool delete_piece, bool weaken_structure, bool force_break_links, void (*impulse_func)(unsigned int, rfg_subpiece*, rfg_subpiece_base*, const vector&, const vector&), const vector* stress_dir)
{
    rfg_destroyable_shape* shape = movep->rdd.is_loaded() ? movep->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return nullptr;
    }
    rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_index];
    if (sp->get_destroyable() != shape)
    {
        return nullptr;
    }
    if (delete_piece)
    {
        sp->flags |= 0x80;
    }
    else if ((shape->flags & 4) != 0 && !Multiplayer)
    {
        damage *= (shape->flags & 8) != 0 ? RFG_uber_heavy_destruction_damage_multiplier : RFG_heavy_destruction_damage_multiplier;
    }
    rfg_dyn_apply_damage_to_subpiece(movep, sp, damage, stress_pos, det, weaken_structure, force_break_links, impulse_func, stress_dir);
    return sp;
}

// 0x0071C0E0 (:3635)
// LTCG: obj in EAX.
static bool is_multi_vehicle_rfg_mover_collision(const object* obj)
{
    return obj != nullptr && obj->get_obj_type() == OT_VEHICLE;
}

// ---- applying the events
static rfg_collision_event* get_sp_array_event(unsigned int target_handle, unsigned int target_shape_key);

// The player that caused an event (nullptr for anything else).
static player* event_owner_player(const rfg_collision_event* ev)
{
    return World.object_from_handle<player>(ev->collision_base.owner);
}

// The live rfg_mover an event targets.
static rfg_mover* event_target_mover(const rfg_collision_event* ev)
{
    rfg_mover* rm = World.object_from_handle<rfg_mover>(ev->collision_base.base_a.obj_handle);
    if (rm == nullptr || rm->obj_flags.destroyed)
    {
        return nullptr;
    }
    return rm;
}

// 0x007330B0 (:3646)
// The pieces within the explosion's secondary radius (and inside its shape) take its structural damage, falling off
// with the distance to the piece (its closest point moved a quarter towards its centre), along the explosion's
// direction blended in (Explosion_directional_pct). Pieces within the primary radius are also pushed (impulse
// scaled by their mass against their material's density). A movable mover gets the summed impulse; a fixed one an
// explosion event (jitter). Explosions over Large_explosion_radius mark the mover (large debris effects).
static void rfg_dyn_apply_explosion(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    player* pp = event_owner_player(ev);
    if (!Multiplayer && pp == nullptr && rm->player_damage_only())
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(pp, rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    const explosion_info* e_info = explosion_lookup_by_unique_id(ev->collision_base.exp_unique_id);
    if (e_info == nullptr)
    {
        return;
    }
    if (Large_explosion_radius < e_info->m_secondary_radius)
    {
        rm->flags.large_explosion = 1;
    }
    const float radius = ev->collision_base.exp_radius_multiplier * e_info->m_radius;
    const float secondary_radius = ev->collision_base.exp_radius_multiplier * e_info->m_secondary_radius;
    const bool no_direction = ev->collision_base.base_a.collision_normal_local.is_null();
    vector explosion_dir;
    if (!no_direction)
    {
        explosion_dir = ev->collision_base.base_a.collision_normal_local;
    }
    const vector& explosion_pos = ev->collision_base.base_a.collision_point_local;
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, explosion_pos, secondary_radius, hits);
    if (hits.getSize() < 1)
    {
        return;
    }
    vector total_impulse = ZERO_VECTOR;
    if ((shape->flags & 0x1000) != 0)
    {
        rfg_dyn_shatter(rm, shape, (float)e_info->m_structural_damage, explosion_pos, ZERO_VECTOR);
    }
    const int structural_damage = e_info->m_structural_damage << 8; // fixed point x256
    int num_impulses = 0;
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(key);
        if (!explosion_is_bbox_in_shape(sp_base->prel_bmin, sp_base->prel_bmax, e_info, explosion_pos, &explosion_dir))
        {
            continue;
        }
        vector closest;
        bbox_find_closest_point(closest, sp_base->prel_bmin, sp_base->prel_bmax, explosion_pos);
        const vector stress_pos((sp_base->pos.x - closest.x) * 0.25f + closest.x, (sp_base->pos.y - closest.y) * 0.25f + closest.y, (sp_base->pos.z - closest.z) * 0.25f + closest.z);
        vector dir = stress_pos - explosion_pos;
        const float dist = dir.mag_and_normalize_safe();
        if (secondary_radius < dist)
        {
            continue;
        }
        const float falloff = fl_parameter(dist, secondary_radius, 0.0f, false);
        if (!no_direction)
        {
            dir = dir * (1.0f - Explosion_directional_pct) + explosion_dir * Explosion_directional_pct;
            dir.normalize_safe();
        }
        const bool in_primary = dist <= radius;
        const unsigned int damage = (unsigned int)(((long long)(int)(falloff * 256.0f) * (long long)structural_damage) >> 16);
        rfg_subpiece* sp = rfg_dyn_apply_stress_to_subpiece(rm, key, damage, stress_pos, ev->collision_base.destruction_type, false, ((e_info->flags >> 1) & 1) != 0, false, nullptr, nullptr);
        if (in_primary)
        {
            num_impulses++;
            const vector impulse = dir * (rfg_dyn_get_impulse_scalar(sp_base->mass, sp_base->physical_material_index) * e_info->m_impulse_magnitude * falloff);
            total_impulse += impulse;
            if (sp != nullptr && (sp->flags & 0x10) != 0)
            {
                sp->add_impulse(impulse, stress_pos);
            }
        }
    }
    vector world_pos;
    vector world_impulse;
    convert_point_direction_from_local_to_world(world_pos, world_impulse, explosion_pos, total_impulse, rm->pos, rm->orient);
    if (0 < num_impulses && rm->get_move_type() != RFGM_FIXED)
    {
        rfg_dyn_apply_point_impulse(rm->havok_handle, world_impulse, world_pos, shape->mass);
    }
    if (rm->get_move_type() == RFGM_FIXED)
    {
        rm->create_explosion_event(explosion_pos, e_info, 0);
    }
}

// 0x00713020 (:3803)
static void apply_impulse_direct_damage(unsigned int damage, rfg_subpiece* sp, rfg_subpiece_base* sp_base, const vector& impulse_pos, const vector& impulse_dir)
{
    const float mass = sp_base->mass - 1.0f < 0.0f ? 1.0f : sp_base->mass;
    sp->add_impulse(impulse_dir * (mass * sqrtf((float)damage) * Destruction_direct_damage_impulse_scalar), impulse_pos);
}

// 0x00733800 (:3812)
// LTCG: ev in EDI. Each listed piece takes its (unpacked) damage at the hit point, pushed along the hit normal.
static void rfg_dyn_apply_direct_damage(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(event_owner_player(ev), rm);
    multi_game_destruction_record_last_frame(rm);
    for (int i = 0; i < ev->collision_base.data_count; i++)
    {
        const comp_sp_combined_direct_damage& entry = ev->collision_base.get_combined_direct_damage(i);
        const unsigned int damage = int_unpack16(entry.compressed_damage);
        const unsigned int key = shapekey_unpack16(entry.compressed_shape_key);
        rfg_dyn_apply_stress_to_subpiece(rm, key, damage, ev->collision_base.base_a.collision_point_local, ev->collision_base.destruction_type, false, false, false, apply_impulse_direct_damage, &ev->collision_base.base_a.collision_normal_local);
    }
}

// 0x007338F0 (:3836)
// The listed links are removed (both pieces queued for shardify), or with flag 0x20 the listed pieces' dynamic links.
// A busy multiplayer server (450 events in the last 360 frames) delays the event (flag 0x4).
static void rfg_dyn_apply_link_broken(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    if (box_acts_as_server_for_destruction() && !rfg_dyn_allow_low_priority_events()) // inlined
    {
        ev->collision_base.flags |= 4;
        return;
    }
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    for (int i = 0; i < ev->collision_base.data_count; i++)
    {
        const unsigned int index = ev->collision_base.get_link_index(i);
        if ((ev->collision_base.flags & 0x20) != 0)
        {
            rfg_mover_delete_dynamic_link(rm, &shape->get_object_buffer()[index], true);
            continue;
        }
        const rfg_link_base* link_base = shape->get_link_base(index);
        shape->remove_link(shape->get_link(index));
        rfg_subpiece* objects = shape->get_object_buffer();
        objects[link_base->obj[0]].flags |= 0x800;
        objects[link_base->obj[1]].flags |= 0x800;
        shape->add_subpiece_to_shardify_q(&objects[link_base->obj[0]]);
        shape->add_subpiece_to_shardify_q(&objects[link_base->obj[1]]);
    }
}

// 0x00733A60 (:3874)
// The listed pieces still in the mover are deleted.
static void rfg_dyn_apply_sp_array(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(event_owner_player(ev), rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    for (int i = 0; i < ev->collision_base.data_count; i++)
    {
        rfg_subpiece* sp = &shape->get_object_buffer()[shapekey_unpack16(ev->collision_base.get_direct_sps(i))];
        if (sp != nullptr && shape->get_shape_state(sp))
        {
            shape->delete_subpiece(sp);
        }
    }
}

// 0x00733B50 (:3905)
static void rfg_dyn_apply_shatter(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    player* pp = event_owner_player(ev);
    if (!Multiplayer && pp == nullptr && rm->player_damage_only())
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(pp, rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape != nullptr)
    {
        rfg_dyn_shatter(rm, shape, (float)ev->collision_base.damage, ev->collision_base.base_a.collision_point_local, ev->collision_base.base_a.collision_normal_local);
    }
}

// 0x007245B0 (:3931)
// LTCG: sp_base in EDI, local_pos in ESI. Pushes a piece cut by an explosion's shape cutter away from the explosion
// (once per frame: only without impulse info yet).
static void apply_impulse(const explosion_info* exp_info, rfg_subpiece* sp, const rfg_subpiece_base* sp_base, const vector& local_pos)
{
    if (exp_info == nullptr || rfg_shape_get_shard_impulse_info(sp) != nullptr) // inlined
    {
        return;
    }
    vector closest;
    bbox_find_closest_point(closest, sp_base->prel_bmin, sp_base->prel_bmax, local_pos);
    vector dir = closest - local_pos;
    const float dist = dir.mag_and_normalize_safe();
    const float falloff = fl_parameter(dist, exp_info->m_radius, 0.0f, false);
    if (0.0f < falloff)
    {
        sp->add_impulse(dir * (exp_info->m_impulse_magnitude * falloff * rfg_dyn_get_impulse_scalar(sp_base->mass, sp_base->physical_material_index)), closest);
    }
}

// 0x00733C40 (:3956)
// Every piece within the radius that is not anchored (flag 0x1) and not indestructible (base flag 0x20) is cut out:
// queued for shardify, its breakable links and dynamic link removed (0x200000 once it has no link left). Without an
// explosion the pieces are deleted (0x80); with one they are pushed and marked exploded (0x100) or, for explosions
// breaking over time (nano), 0x8000. A fixed mover gets an explosion event (jitter) unless a large shape cutting
// explosion covers it.
static void rfg_dyn_apply_shape_cutter(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    player* pp = event_owner_player(ev);
    if (!Multiplayer && pp == nullptr && rm->player_damage_only())
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(pp, rm);
    activity_register_damage_mark_objects(pp, rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    const explosion_info* exp_info = explosion_lookup_by_unique_id(ev->collision_base.exp_unique_id);
    bool large_cut = false;
    if (exp_info != nullptr && Large_explosion_radius <= exp_info->m_secondary_radius)
    {
        rm->flags.large_explosion = 1;
        large_cut = (exp_info->flags & 0x8000) != 0 && ev->collision_base.radius < exp_info->m_radius;
    }
    float radius = ev->collision_base.radius;
    const vector& pos = ev->collision_base.base_a.collision_point_local;
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, pos, radius, hits);
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        rfg_subpiece* sp = &shape->get_object_buffer()[key];
        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(key);
        if ((sp_base->flags & 0x20) != 0 || (sp->flags & 1) != 0)
        {
            continue;
        }
        shape->add_subpiece_to_shardify_q(sp);
        if (exp_info == nullptr)
        {
            sp->flags |= 0x80;
        }
        else
        {
            apply_impulse(exp_info, sp, sp_base, pos);
            sp->flags |= exp_info->time_between_breaks == 0 ? 0x100 : 0x8000;
        }
        int l = 0;
        while (l < sp->num_links)
        {
            const unsigned short link_index = sp->links.to_pointer()[l];
            const rfg_link_base* link_base = shape->get_link_base(link_index);
            if ((link_base->flags & 2) != 0)
            {
                l++; // unbreakable
                continue;
            }
            shape->add_subpiece_to_shardify_q(linked_subpiece(shape, key, link_index));
            shape->remove_link(shape->get_link(link_index));
        }
        if (sp->num_links == 0)
        {
            sp->flags |= 0x200000;
        }
        rfg_mover_delete_dynamic_link(rm, sp, true);
    }
    if (rm->get_move_type() == RFGM_FIXED && !large_cut)
    {
        if (exp_info == nullptr || exp_info->time_between_breaks == 0)
        {
            rm->create_explosion_event(pos, Shape_cutter_jitter_radius + radius, radius, 0);
        }
        else
        {
            radius = exp_info->m_radius;
            rm->create_explosion_event(pos, radius * Nano_jitter_radius_mult, radius, 1);
        }
    }
}

// 0x0071C100 (:4070)
// Collector callback of a repair: objects in the regrown pieces' space go away. Debris pieces of movable rfg_movers are
// deleted through a piece array event (authority only), unsynced general movers (or ones flagged 0x4) and other movers
// are destroyed. Never stops the collection.
static bool fading_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp->get_obj_type() != OT_MOVER || hit_objp == user_data)
    {
        return false;
    }
    if (hit_objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover* rm = (rfg_mover*)hit_objp;
        if (rm->get_move_type() != RFGM_FIXED && rm->rdd.is_loaded() && rm->rdd.get_data()->shape != nullptr && box_is_single_player_or_acts_as_server_for_destruction() && !hit_objp->obj_flags.destroyed) // inlined
        {
            rfg_collision_event* ev = get_sp_array_event(hit_objp->get_handle(), shape_key);
            if (ev != nullptr)
            {
                ev->collision_base.set_direct_sps(ev->collision_base.data_count, shapekey_pack16(shape_key));
                ev->collision_base.data_count++;
                multi_game_destruction_set_sp_array_working_data(ev->collision_base);
            }
        }
        return false;
    }
    if (hit_objp->get_sub_type() != OT_SUB_MOVER_GENERAL || !((object_mover*)hit_objp)->mover_should_sync() || (((object_mover*)hit_objp)->mover_flags & 4) != 0)
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// Restores a link's strength by up to amount (all of it when amount is negative).
static void repair_link(rfg_link* link, const rfg_link_base* link_base, int amount)
{
    if (link_base->yield_max <= link->yield_limit)
    {
        return;
    }
    const int yield_max = link_base->yield_max;
    int yield = link->yield_limit;
    if (yield < 0)
    {
        yield = 0;
    }
    else if (yield_max < yield)
    {
        yield = yield_max;
    }
    if (amount < 0)
    {
        amount = yield_max;
    }
    link->yield_limit = yield;
    link->yield_limit = (amount <= yield_max - yield ? amount : yield_max - yield) + yield;
}

// 0x00733FE0 (:4096)
// The pieces within the radius regain link strength (duration x Repair_rate per event, everything when negative):
// first their current links, then their removed ones (not shatter links, base flag 0x4); a removed link back at full
// strength is restored when the other piece is still in this mover, otherwise that piece regrows here (fade in) and
// whatever occupies its space is cleared (fading_callback, authority only).
//
// Kept: restore_link moves a link into the active part of the list while the second loop walks the removed part
// with the same counter.
static void rfg_dyn_apply_repair(rfg_collision_event* ev)
{
    multi_game_destruction_update_jip_status(true);
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    multi_game_player_repaired_rfg_mover(event_owner_player(ev), rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    const float radius = ev->collision_base.radius;
    const vector& pos = ev->collision_base.base_a.collision_point_local;
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, pos, radius, hits);
    rm->record_repair_event(pos, radius);
    const int amount = (int)(ev->collision_base.repair_duration * Repair_rate);
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        rfg_subpiece* sp = &shape->get_object_buffer()[key];
        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(key);
        int l = 0;
        for (; l < sp->num_links; l++)
        {
            const unsigned short link_index = sp->links.to_pointer()[l];
            repair_link(shape->get_link(link_index), shape->get_link_base(link_index), amount);
        }
        vector bmin(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f); // FLT_MAX
        vector bmax(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
        bool regrew = false;
        if (sp_base->num_links <= l)
        {
            continue;
        }
        for (; l < sp_base->num_links; l++)
        {
            const unsigned short link_index = sp->links.to_pointer()[l];
            rfg_link* link = shape->get_link(link_index);
            const rfg_link_base* link_base = shape->get_link_base(link_index);
            if ((link_base->flags & 4) != 0)
            {
                continue;
            }
            repair_link(link, link_base, amount);
            if (link->yield_limit < link_base->yield_max)
            {
                continue;
            }
            rfg_subpiece* osp = linked_subpiece(shape, key, link_index);
            if (osp->get_destroyable() == shape)
            {
                shape->restore_link(link);
            }
            else if (shape->fade_subpiece(osp, true))
            {
                const rfg_subpiece_base* other_base = shape->get_subpiece_base((unsigned int)(osp - shape->get_object_buffer()));
                regrew = true;
                bbox_expand(bmin, bmax, other_base->prel_bmin, other_base->prel_bmax);
            }
        }
        if (regrew && box_is_single_player_or_acts_as_server_for_destruction()) // inlined
        {
            unsigned int collected;
            havok_collect_objects(bmin, bmax, rm->pos, rm->orient, &collected, 1, COLLIDE_RAY_RFG_DESTROYABLE, fading_callback, rm);
        }
    }
}

// 0x00734400 (:4210)
// LTCG: ev in ESI. The pieces inside a vehicle's box take its damage (multiplayer vehicles against movers).
//
// Kept: rfg_dyn_add_pre_collide_mp_vehicle never stores the box (half extents, position, angles), so the query uses
// whatever an earlier event left in the slot; the position goes to world and back to local coordinates.
static void rfg_dyn_apply_mp_veh(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr)
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(event_owner_player(ev), rm);
    multi_game_destruction_record_last_frame(rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return;
    }
    const vector& angles = ev->collision_base.base_b.collision_point_local;
    const matrix box_orient(angles.x, angles.y, angles.z);
    vector box_pos = ev->collision_base.base_a.collision_normal_local;
    vec_change_to_global_coords(box_pos, rm->pos, rm->orient);
    vec_change_to_local_coords(box_pos, rm->pos, rm->orient);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_obb(shape, box_pos, box_orient.copy_transpose(), ev->collision_base.base_a.collision_point_local, hits);
    for (int i = 0; i < hits.getSize(); i++)
    {
        rfg_dyn_apply_stress_to_subpiece(rm, hits[i], ev->collision_base.damage, ZERO_VECTOR, ev->collision_base.destruction_type, false, false, false, nullptr, nullptr);
    }
}

// Port addition (test seam, see rfg_dynamics.h).
bool rfg_dyn_openrfg_is_pregame_destruction()
{
    return Destruction_mode == DM_PREGAME;
}

// 0x00724720 (:4246)
// LTCG: ev in EDI. Ends the pregame destruction mode: the listed movers (and their children) toggle invulnerability.
static void rfg_dyn_apply_destruction_mode(rfg_collision_event* ev)
{
    Destruction_mode = DM_NORMAL;
    const unsigned int* handles = ev->collision_base.get_invulnerable_mover_handles();
    for (int i = 0; i < ev->collision_base.data_count; i++)
    {
        rfg_mover* rm = World.object_from_handle<rfg_mover>(handles[i]);
        if (rm != nullptr)
        {
            rfg_mover_toggle_self_and_children_invulnerable(rm);
        }
    }
}

// 0x00713100 (:4262)
static void apply_impulse_collision(unsigned int damage, rfg_subpiece* sp, rfg_subpiece_base* sp_base, const vector& impulse_pos, const vector& impulse_dir)
{
    const float mass = sp_base->mass - 1.0f < 0.0f ? 1.0f : sp_base->mass;
    sp->add_impulse(impulse_dir * (mass * sqrtf((float)damage) * Destruction_impact_impulse_scalar), impulse_pos);
}

// 0x00739DF0 (:4271)
// LTCG: rfgm in ESI. A collision hit on one piece: shakes the camera, shatters shatterable movers on hard hits
// (Shatter_min_damage), slices big heavy buildings hit fast enough (single player, Slice_min_damage, over 100 pieces;
// the hit counts when the closing speed along the hit direction beats a random 1-10 m/s, a third or two thirds of it
// in (uber) heavy destruction), and in uber heavy destruction may blow the piece up (explosion effect, piece deleted).
// The piece then takes the damage (pushed along the hit when apply_impulse) and records a damaged piece event.
static rfg_subpiece* rfg_dyn_apply_single_object_collision(rfg_mover* rfgm, int subpiece_index, const vector& stress_pos, const vector& stress_dir, unsigned int damage, destruction_event_type det, bool apply_impulse, bool force_break_links)
{
    vector world_pos = stress_pos;
    world_pos.unrotate(rfgm->orient);
    world_pos += rfgm->pos;
    camera_shake_start_from_impact((float)damage, vec_dist_squared(world_pos, Rfg_camera.real_pos));
    rfg_destroyable_shape* shape = rfgm->rdd.is_loaded() ? rfgm->rdd.get_data()->shape : nullptr;
    const rfg_subpiece_base* sp_base = shape->get_subpiece_base(subpiece_index); // not checked
    if ((sp_base->flags & 0x40) == 0 && Shatter_min_damage < damage && (shape->flags & 0x1000) != 0)
    {
        rfg_dyn_shatter(rfgm, shape, sqrtf((float)damage), stress_pos, stress_dir);
    }
    if (!Multiplayer && Slice_min_damage < damage && 100 < shape->num_existing_child_shapes && rfg_dyn_can_slice())
    {
        float min_speed = fl_rand_range(1.0f, 10.0f);
        if ((shape->flags & 8) != 0)
        {
            min_speed *= 0.33f;
        }
        else if ((shape->flags & 4) != 0)
        {
            min_speed *= 0.66f;
        }
        vector velocity;
        havok_body_get_linear_velocity(rfgm->havok_handle, velocity);
        const vector world_dir = stress_dir.copy_unrotate(rfgm->orient);
        if (min_speed < -(velocity.y * world_dir.y + world_dir.x * velocity.x + velocity.z * world_dir.z) && rfg_dyn_slice_shape(rfgm, shape, sp_base->pos, true))
        {
            return &shape->get_object_buffer()[subpiece_index];
        }
    }
    bool delete_piece = false;
    if (!Multiplayer && (shape->flags & 8) != 0 && rfgm->get_move_type() != RFGM_FIXED && Explode_min_damage < damage && RFG_uber_heavy_destruction_explosion_min_delay < Framecount - RFG_uber_heavy_destruction_explosion_framecount && int_rand_chance(RFG_uber_heavy_destruction_explosion_chance))
    {
        RFG_uber_heavy_destruction_explosion_framecount = Framecount;
        const vector effect_pos = sp_base->pos.copy_unrotate(rfgm->orient) + rfgm->pos;
        const material_event_size size = get_rand_explosion_size(); // inlined
        rfg_effect_candidate_create(rfg_get_effect_material_index(sp_base->physical_material_index), effect_pos, MATERIAL_EVENT_EXPLODE, size, INVALID_MATERIAL_EVENT_SHAPE_TYPE, true, 0, nullptr, nullptr, 0xFFFFFFFF, 0xFF, 0xFF);
        delete_piece = true;
    }
    rfg_subpiece* sp = rfg_dyn_apply_stress_to_subpiece(rfgm, subpiece_index, damage, stress_pos, det, delete_piece, false, force_break_links, apply_impulse ? apply_impulse_collision : nullptr, &stress_dir);
    if (sp != nullptr)
    {
        rfgm->add_damaged_subpiece_event((unsigned int)(sp - shape->get_object_buffer()));
    }
    return sp;
}

// 0x0073A210 (:4367)
// LTCG: ev in EDI. An object hit an rfg_mover (flags: 0x8 apply impulse, 0x10 force break links).
static void rfg_dyn_apply_object_simple(rfg_collision_event* ev)
{
    rfg_mover* rm = event_target_mover(ev);
    if (rm == nullptr || rm->player_damage_only())
    {
        return;
    }
    multi_game_player_damaged_rfg_mover(event_owner_player(ev), rm);
    multi_game_destruction_record_last_frame(rm);
    const bool apply_impulse = ((ev->collision_base.flags >> 3) & 1) != 0;
    const bool force_break_links = ((ev->collision_base.flags >> 4) & 1) != 0;
    ev->havok_base.affected_subpiece_a = rfg_dyn_apply_single_object_collision(rm, ev->collision_base.base_a.shape_key, ev->collision_base.base_a.collision_point_local, ev->collision_base.base_a.collision_normal_local, ev->collision_base.damage, DET_OBJECT_SIMPLE, apply_impulse, force_break_links);
}

// 0x0073A2E0 (:4390)
// Two rfg_movers collided: both take the damage (impulses only for fixed movers). Hard hits (1e6) between two large
// movers moving faster than 1 m/s play their collapse effects.
static void rfg_dyn_apply_object_complex(rfg_collision_event* ev)
{
    rfg_mover* rm_a = World.object_from_handle<rfg_mover>(ev->collision_base.base_a.obj_handle);
    rfg_mover* rm_b = World.object_from_handle<rfg_mover>(ev->collision_base.base_b.obj_handle);
    if (rm_a == nullptr || rm_b == nullptr || rm_a->obj_flags.destroyed || rm_b->obj_flags.destroyed)
    {
        return;
    }
    player* pp = event_owner_player(ev);
    multi_game_player_damaged_rfg_mover(pp, rm_a);
    multi_game_player_damaged_rfg_mover(pp, rm_b);
    multi_game_destruction_record_last_frame(rm_a);
    multi_game_destruction_record_last_frame(rm_b);
    const bool apply_impulse = ((ev->collision_base.flags >> 3) & 1) != 0;
    const bool force_break_links = ((ev->collision_base.flags >> 4) & 1) != 0;
    ev->havok_base.affected_subpiece_a = rfg_dyn_apply_single_object_collision(rm_a, ev->collision_base.base_a.shape_key, ev->collision_base.base_a.collision_point_local, ev->collision_base.base_a.collision_normal_local, ev->collision_base.damage, DET_OBJECT_COMPLEX, apply_impulse && rm_a->get_move_type() == RFGM_FIXED, force_break_links);
    ev->havok_base.affected_subpiece_b = rfg_dyn_apply_single_object_collision(rm_b, ev->collision_base.base_b.shape_key, ev->collision_base.base_b.collision_point_local, ev->collision_base.base_b.collision_normal_local, ev->collision_base.damage, DET_OBJECT_COMPLEX, apply_impulse && rm_b->get_move_type() == RFGM_FIXED, force_break_links);
    const rfg_subpiece* sp_a = ev->havok_base.affected_subpiece_a;
    const rfg_subpiece* sp_b = ev->havok_base.affected_subpiece_b;
    if ((sp_a != nullptr && (sp_a->flags & 0x10) != 0) || (sp_b != nullptr && (sp_b->flags & 0x10) != 0))
    {
        activity_register_damage(rm_a, rm_b, (float)ev->collision_base.damage, -1);
    }
    if (999999 < ev->collision_base.damage && (rm_a->mover_flags & 2) != 0 && (rm_b->mover_flags & 2) != 0)
    {
        vector vel_a;
        vector vel_b;
        havok_body_get_linear_velocity(rm_a->havok_handle, vel_a);
        havok_body_get_linear_velocity(rm_b->havok_handle, vel_b);
        const float speed_a = vel_a.mag_squared();
        const float speed_b = vel_b.mag_squared();
        if (1.0f < (0.0f <= speed_a - speed_b ? speed_a : speed_b))
        {
            rm_a->play_collapse(rm_b);
            rm_b->play_collapse(rm_a);
        }
    }
}

// 0x0071C1F0 (:4450)
// The server destroyed a mover: destroy it here without sending it back.
static void rfg_dyn_apply_mp_destroy(rfg_collision_event* ev)
{
    rfg_mover* rm = World.object_from_handle<rfg_mover>(ev->collision_base.base_a.obj_handle);
    if (rm == nullptr)
    {
        return;
    }
    multi_game_destruction_record_last_frame(rm);
    rm->remote_obj_flags.client_destroy_permission = true;
    rm->remote_obj_flags.no_destroy_packet = true;
    rm->mp_info->destroy_causes.mp_destroy = true; // not checked
    World.destroy_object(rm, false);
}

// 0x007345F0 (:4466)
// Cuts the mover with a 2 cm thick box through the event's point, across the shape, tilted by the event's angles about
// the other two axes. More than 10 pieces in it: they lose all their links (all queued for shardify). Returns whether
// it cut.
static bool rfg_dyn_apply_slice(rfg_collision_event* ev)
{
    rfg_mover* rm = World.object_from_handle<rfg_mover>(ev->collision_base.base_a.obj_handle);
    if (rm == nullptr)
    {
        return false;
    }
    multi_game_destruction_record_last_frame(rm);
    rm->slice_delay.set_rand(500, 1500);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr; // not checked
    const unsigned int axis = ev->collision_base.get_slice_index();
    float bmin[3] = {shape->local_bmin.x, shape->local_bmin.y, shape->local_bmin.z};
    float bmax[3] = {shape->local_bmax.x, shape->local_bmax.y, shape->local_bmax.z};
    const float* base_pos = &ev->collision_base.base_a.collision_point_local.x;
    bmax[axis] = base_pos[axis] + 0.01f;
    bmin[axis] = base_pos[axis] - 0.01f;
    const vector extents(bmax[0] - bmin[0], bmax[1] - bmin[1], bmax[2] - bmin[2]);
    const vector center(extents.x * 0.5f + bmin[0], extents.y * 0.5f + bmin[1], extents.z * 0.5f + bmin[2]);
    float angles[3];
    angles[axis] = ev->collision_base.base_b.collision_point_local.x; // angle0
    angles[(axis + 1) % 3] = 0.0f;
    angles[(axis + 2) % 3] = ev->collision_base.base_b.collision_point_local.y; // angle1
    const matrix orient(angles[0], angles[1], angles[2]);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_obb(shape, center, orient, extents, hits); // kept: full extents passed as the half extents
    if (hits.getSize() <= 10)
    {
        return false;
    }
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        rfg_subpiece* sp = &shape->get_object_buffer()[key];
        shape->add_subpiece_to_shardify_q(sp);
        while (sp->num_links != 0)
        {
            const unsigned short link_index = sp->links.to_pointer()[0];
            shape->remove_link(shape->get_link(link_index));
            shape->add_subpiece_to_shardify_q(linked_subpiece(shape, key, link_index));
        }
    }
    return true;
}

// 0x0072D720 (:4536)
// Destruction trails: a new one starts its effect on the shard (10-20 s for nano trails, 0.5-2 s otherwise) sized by
// the shard and tinted by its material; running ones follow the shard's speed and stop emitting when their time is up
// (or right away once the shard or the effect is gone).
static void desruction_trail_do_frame()
{
    csspinlock_scope lock(Destruction_trail_pool.thread_access);
    rfg_destruction_trail* trail = Destruction_trail_pool.get_first(0);
    while (trail != nullptr)
    {
        rfg_destruction_trail* next = Destruction_trail_pool.get_next(trail);
        if (trail->lifetime.valid())
        {
            if (trail->lifetime.elapsed())
            {
                object_effect* effect = World.object_from_handle<object_effect>(trail->trail_handle);
                if (effect != nullptr)
                {
                    effect->stop(object_effect::SS_STOP_EMITTING);
                }
                Destruction_trail_pool.free(trail);
            }
            else
            {
                object_effect* effect = World.object_from_handle<object_effect>(trail->trail_handle);
                object* owner = World.object_from_handle(trail->owner_handle);
                if (effect == nullptr || owner == nullptr || owner->havok_handle == 0xFFFFFFFF)
                {
                    trail->lifetime.set(0);
                }
                else
                {
                    vector velocity;
                    havok_body_get_linear_velocity(owner->havok_handle, velocity);
                    effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z));
                }
            }
        }
        else
        {
            object* owner = World.object_from_handle(trail->owner_handle);
            if (owner == nullptr)
            {
                trail->trail_handle = 0xFFFFFFFF;
                Destruction_trail_pool.free(trail);
            }
            else
            {
                trail->effect_handle = effects_play(trail->trail_handle, owner->pos, owner->orient, true, owner->get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                if (trail->effect_handle == 0xFFFFFFFF)
                {
                    Destruction_trail_pool.free(trail);
                }
                else
                {
                    if ((trail->flags & 2) == 0)
                    {
                        trail->lifetime.set_rand(500, 2000);
                    }
                    else
                    {
                        trail->lifetime.set_rand(10000, 20000);
                    }
                    object_effect* effect = World.object_from_handle<object_effect>(trail->effect_handle);
                    if (effect != nullptr)
                    {
                        vector bmin;
                        vector bmax;
                        owner->get_local_bbox(bmin, bmax);
                        effect->set_expression_variable(EFFECTS_VAR_OBJECT_SIZE, vec_dist(bmin, bmax));
                        if (trail->rfg_effect_material_p->effect_tint_enabled)
                        {
                            effect->set_material_tint(trail->rfg_effect_material_p->effect_tint);
                        }
                    }
                }
            }
        }
        trail = next;
    }
}

// ---- the event queue (RFG_dyn_collisions, guarded by Collision_event_wrapper)
// 0x007131E0 (:4635)
// LTCG: shape_key_a in EAX, flags in CL, ev in ESI. Whether new data can join an existing event of the same type and
// objects (same flags 0x3): direct damage to a listed piece, or close to the first hit (50 cm, within 30 degrees);
// link breaks (up to 7, same flag 0x20); destruction mode handles (up to 7 in all); contacts of the same pieces (up to
// 10 contact ids).
static bool can_append_collision_event(destruction_event_type det, unsigned int shape_key_a, unsigned int shape_key_b, unsigned char flags, const vector* local_pos, const vector* local_normal, rfg_collision_event* ev, hkpDynamicsContactMgr* mgr, unsigned short id, const void* append_param)
{
    if (((ev->collision_base.flags ^ flags) & 3) != 0)
    {
        return false;
    }
    if (det == DET_DIRECT_DAMAGE)
    {
        const unsigned short packed_key = shapekey_pack16(shape_key_a);
        for (int i = 0; i < ev->collision_base.data_count; i++)
        {
            if (ev->collision_base.get_combined_direct_damage(i).compressed_shape_key == packed_key)
            {
                return true;
            }
        }
        if (ev->collision_base.data_count < MAX_EVENT_DATA && vec_dist_squared(ev->collision_base.base_a.collision_point_local, *local_pos) < 0.25f)
        {
            const vector& normal = ev->collision_base.base_a.collision_normal_local;
            return 0.8660254f < normal.y * local_normal->y + local_normal->x * normal.x + normal.z * local_normal->z; // cos 30
        }
        return false;
    }
    if (det == DET_LINK_BROKEN)
    {
        return ev->collision_base.data_count < MAX_EVENT_DATA && ((ev->collision_base.flags ^ flags) & 0x20) == 0;
    }
    if (det == DET_DESTRUCTION_MODE)
    {
        return (int)ev->collision_base.data_count + *(const int*)append_param <= MAX_EVENT_DATA;
    }
    if (ev->collision_base.base_a.shape_key == shape_key_a && ev->collision_base.base_b.shape_key == shape_key_b)
    {
        return ev->add_contact_info(mgr, id);
    }
    return false;
}

// 0x00713350 (:4687)
static bool destruction_type_can_append(destruction_event_type det)
{
    switch (det)
    {
    case DET_DIRECT_DAMAGE:
    case DET_OBJECT_SIMPLE:
    case DET_OBJECT_COMPLEX:
    case DET_DESTRUCTION_MODE:
    case DET_LINK_BROKEN:
        return true;
    default:
        return false;
    }
}

// 0x00713390 (:4703)
// An event for this frame: with authority, appendable types join the latest run of events of the same type and objects
// when they can; otherwise a new one (nullptr when all 1024 are taken).
static rfg_collision_event* next_free_collision_event(destruction_event_type det, unsigned int handle_a, unsigned int handle_b, unsigned int shape_key_a, unsigned int shape_key_b, unsigned char flags, hkpDynamicsContactMgr* mgr, unsigned short id, const vector* local_pos, const vector* local_normal, const void* append_param)
{
    cs_scope lock(Collision_event_wrapper);
    if (box_is_single_player_or_acts_as_server_for_destruction() && destruction_type_can_append(det)) // inlined
    {
        for (int i = Num_rfg_dyn_collisions - 1; 0 <= i; i--)
        {
            rfg_collision_event* ev = &RFG_dyn_collisions[i];
            if (ev->collision_base.destruction_type != det || ev->collision_base.base_a.obj_handle != handle_a || ev->collision_base.base_b.obj_handle != handle_b)
            {
                break;
            }
            if (can_append_collision_event(det, shape_key_a, shape_key_b, flags, local_pos, local_normal, ev, mgr, id, append_param))
            {
                return ev;
            }
        }
    }
    if (MAX_RFG_DYN_COLLISIONS <= Num_rfg_dyn_collisions)
    {
        return nullptr;
    }
    rfg_collision_event* ev = &RFG_dyn_collisions[Num_rfg_dyn_collisions++];
    ev->collision_base.base_a.obj_handle = handle_a;
    ev->collision_base.base_b.obj_handle = handle_b;
    ev->collision_base.base_b.shape_key = shape_key_b;
    ev->collision_base.flags = flags;
    ev->collision_base.owner = 0xFFFFFFFF;
    ev->havok_base.count = 0;
    ev->collision_base.destruction_type = det;
    ev->collision_base.base_a.shape_key = shape_key_a;
    ev->collision_base.data_count = 0;
    ev->collision_base.base_a.misc = 0;
    ev->collision_base.base_b.misc = 0;
    ev->collision_base.damage = 0;
    ev->collision_base.exp_unique_id = 0xFF;
    ev->collision_base.exp_radius_multiplier = 0.0f;
    ev->collision_base.compressed_exp_radius_multiplier = 0;
    ev->havok_base.affected_subpiece_a = nullptr;
    ev->havok_base.affected_subpiece_b = nullptr;
    ev->havok_base.mgr = mgr;
    for (int i = 0; i < MAX_CONTACT_IDS; i++)
    {
        ev->havok_base.id[i] = 0xFFFF;
    }
    ev->add_contact_info(mgr, id); // inlined
    return ev;
}

// 0x007135A0 (:4793)
// A piece array event of this havok step for the target that does not list the piece yet and has room (15), or a new
// one. nullptr when the piece is listed already or the queue is full.
//
// Kept: the lock is entered twice (it is recursive), the multiplayer authority check's result is ignored, and the
// listed (packed) keys are compared with the unpacked key.
static rfg_collision_event* get_sp_array_event(unsigned int target_handle, unsigned int target_shape_key)
{
    cs_scope lock(Collision_event_wrapper);
    for (int i = Num_rfg_dyn_collisions - 1; Num_events_to_process_this_havok_step <= i; i--)
    {
        rfg_collision_event* ev = &RFG_dyn_collisions[i];
        if (ev->collision_base.destruction_type != DET_SP_ARRAY || (ev->collision_base.flags & 3) != 0 || ev->collision_base.base_a.obj_handle != target_handle)
        {
            continue;
        }
        for (int k = 0; k < ev->collision_base.data_count; k++)
        {
            if (ev->collision_base.get_direct_sps(k) == target_shape_key)
            {
                return nullptr;
            }
        }
        if (ev->collision_base.data_count < MAX_SP_ARRAY_KEYS)
        {
            return ev;
        }
    }
    cs_scope lock2(Collision_event_wrapper);
    if (Multiplayer && (Multiplayer != true || multi_is_server()))
    {
        multi_game_is_migrating();
    }
    if (MAX_RFG_DYN_COLLISIONS <= Num_rfg_dyn_collisions)
    {
        return nullptr;
    }
    rfg_collision_event* ev = &RFG_dyn_collisions[Num_rfg_dyn_collisions++];
    ev->collision_base.base_a.obj_handle = target_handle;
    ev->collision_base.destruction_type = DET_SP_ARRAY;
    ev->collision_base.flags = 0;
    ev->collision_base.data_count = 0;
    ev->collision_base.base_a.misc = 0;
    ev->collision_base.base_b.misc = 0;
    ev->collision_base.damage = 0;
    ev->collision_base.exp_unique_id = 0xFF;
    ev->collision_base.exp_radius_multiplier = 0.0f;
    ev->collision_base.compressed_exp_radius_multiplier = 0;
    ev->havok_base.affected_subpiece_a = nullptr;
    ev->havok_base.affected_subpiece_b = nullptr;
    ev->havok_base.count = 0;
    ev->havok_base.mgr = nullptr;
    ev->collision_base.base_b.obj_handle = 0xFFFFFFFF;
    ev->collision_base.base_a.shape_key = 0xFFFFFFFF;
    ev->collision_base.base_b.shape_key = 0xFFFFFFFF;
    ev->collision_base.owner = 0xFFFFFFFF;
    for (int i = 0; i < MAX_CONTACT_IDS; i++)
    {
        ev->havok_base.id[i] = 0xFFFF;
    }
    return ev;
}

// ---- system
// 0x00724780 (:4968)
void rfg_dyn_system_init()
{
    if (!Rfg_dyn_system_init)
    {
        Rfg_dyn_system_init = true;
        hkBool use_it = {0};
        hkpBvTreeAgent::setUseAabbCaching(use_it);
        stress_evaluation_load_table();
    }
}

// 0x00713750 (:4984)
void rfg_dyn_system_shutdown()
{
    if (Rfg_dyn_system_init)
    {
        Rfg_dyn_system_init = false;
    }
}

// 0x0072DA00 (:4993)
void rfg_dyn_level_init_pre()
{
    Rfg_mover_destruction_uid = 0;
    Destruction_mode = Multiplayer ? DM_PREGAME : DM_NORMAL;
    General_mover_destruction_uid = 0;
    Num_rfg_dyn_collisions = 0;
    RFG_uber_heavy_destruction_slice_framecount = -1;
    RFG_uber_heavy_destruction_explosion_framecount = -1;
    Last_regrow_framecount = -1;
    rfg_destruction_event* devp = RFG_destruction_events.get_first(0);
    while (devp != nullptr)
    {
        devp->flags.in_use = 0;
        remove_dynamic_hole_request(devp);
        RFG_destruction_events.free(devp);
        devp = RFG_destruction_events.get_first(0);
    }
    stress_evaluation_level_init_pre();
}

// 0x00713770 (:5023)
void rfg_dyn_level_shutdown()
{
}

// 0x0073A550 (:5028)
// Per rfg_mover: clears the per-frame piece flag 0x4 when the shape asks (0x4000), queues a stress test whose delay
// is up (stress_evaluation_start), ends heavy destruction mode when its timer is up (restoring the collision type),
// slices uber heavy movable movers when their slice timer is up (a random point in the middle half of the shape,
// over 100 pieces), regrows one mover per frame and rebuilds movers that asked for it. Then the stress evaluation runs.
void rfg_dyn_do_frame_pre()
{
    const int num_movers = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        rfg_mover* rm = World.get_object<rfg_mover>(i);
        rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
        if (shape == nullptr)
        {
            continue;
        }
        if ((shape->flags & 0x4000) != 0)
        {
            shape->flags &= ~0x4000;
            for (int k = 0; k < shape->parent->num_objects; k++)
            {
                shape->get_object_buffer()[k].flags &= ~4u;
            }
        }
        if (rm->flags.under_stress && rm->stress_test_delay.elapsed())
        {
            rm->stress_test_count--;
            rm->stress_test_delay.invalidate();
            stress_evaluation_start(rm, false, false); // inlined
        }
        if ((shape->flags & 4) != 0 && shape->destruction_mode_timer.elapsed())
        {
            shape->flags &= ~0xC;
            shape->destruction_mode_timer.invalidate();
            havok_body_set_collision_type(rm->havok_handle, rfg_dyn_get_filter_by_move_type(rm->get_move_type(), shape->flags), false, false); // inlined
        }
        if ((shape->flags & 8) != 0 && rm->get_move_type() != RFGM_FIXED && rm->slice_delay.elapsed())
        {
            if (shape->num_existing_child_shapes < 101)
            {
                rm->slice_delay.invalidate();
            }
            else
            {
                const float quarter_x = (shape->local_bmax.x - shape->local_bmin.x) * 0.25f;
                const float quarter_z = (shape->local_bmax.z - shape->local_bmin.z) * 0.25f;
                vector hit_pos;
                hit_pos.x = fl_rand_range(shape->local_bmin.x + quarter_x, shape->local_bmax.x - quarter_x);
                hit_pos.y = 0.0f;
                hit_pos.z = fl_rand_range(shape->local_bmin.z + quarter_z, shape->local_bmax.z - quarter_z);
                rfg_dyn_slice_shape(rm, shape, hit_pos, false);
            }
        }
        if (Last_regrow_framecount != Framecount)
        {
            if (rm->flags.regrow_on_stream_finish)
            {
                rfg_mover_regrow_finish(rm);
                Last_regrow_framecount = Framecount;
            }
            else if (rm->flags.regrow_on_stream_begin && rfg_mover_regrow_begin(rm))
            {
                Last_regrow_framecount = Framecount;
            }
        }
        if (rm->flags.rebuild)
        {
            rm->flags.rebuild = 0;
            World.restore_object(rm->get_handle());
        }
    }
    stress_evaluation_do_frame(); // inlined
}

// 0x00734960 (:5107)
void rfg_dyn_do_frame_post()
{
    desruction_trail_do_frame();
    shape_cutter_do_frame();
}

// ---- adding events (rfg_dyn_add_pre_collide_*): without authority only the server's events are taken (from_server).
// The event's raw data is turned into its working data by multi_game_destruction_set_*_working_data (copied in single
// player, compressed and decompressed on a server).

// Multiplayer clients (and a migrating server) only take events from the server.
static bool rfg_dyn_skip_local_event(bool from_server)
{
    return Multiplayer && (!multi_is_server() || multi_game_is_migrating()) && !from_server;
}

// 0x00713780 (:5121)
bool rfg_dyn_add_pre_collide_explosion(rfg_mover* mover, const object* owner, const explosion_info* e_info, const vector& explosion_pos_local, const vector& explosion_dir_local, float radius_mult, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return true;
    }
    if (mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_EXPLOSION, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr);
    if (ev != nullptr)
    {
        ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
        ev->collision_base.exp_unique_id = explosion_get_explosion_unique_id(e_info);
        ev->collision_base.exp_radius_multiplier = radius_mult;
        ev->collision_base.base_a.raw.rcollision_point_local = explosion_pos_local;
        ev->collision_base.base_a.raw.rcollision_normal_local = explosion_dir_local;
        multi_game_destruction_set_explosion_working_data(ev->collision_base);
    }
    return true;
}

// 0x00713860 (:5159)
// Kept: half_extents, world_pos and world_orient are not stored (see rfg_dyn_apply_mp_veh).
bool rfg_dyn_add_pre_collide_mp_vehicle(rfg_mover* mover, const object* owner, unsigned int damage, const vector& half_extents, const vector& world_pos, const matrix& world_orient, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return true;
    }
    if (mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_MP_VEHICLE, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev != nullptr)
    {
        ev->collision_base.damage += damage;
        ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
        multi_game_destruction_set_mp_veh_working_data(ev->collision_base);
    }
    return true;
}

// 0x007139D0 (:5228)
// One piece adds its damage to an event already listing it (or close to its hit), several pieces fill a new event.
bool rfg_dyn_add_pre_collide_direct_damage(rfg_mover* mover, const object* owner, int sp_count, const sp_combined_direct_damage_raw* combined_sps, const vector& pos_local, const vector& normal_local, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server) || mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    const unsigned int shape_key = combined_sps->shape_key;
    const unsigned int damage = combined_sps->damage;
    rfg_collision_event* ev = next_free_collision_event(DET_DIRECT_DAMAGE, mover->get_handle(), 0xFFFFFFFF, shape_key, 0xFFFFFFFF, 0, nullptr, 0xFFFF, &pos_local, &normal_local, nullptr);
    if (ev == nullptr)
    {
        return true;
    }
    rfg_collision_event_base& base = ev->collision_base;
    base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
    if (base.data_count == 0)
    {
        base.base_a.raw.rcollision_point_local = pos_local;
        base.base_a.raw.rcollision_normal_local = normal_local;
    }
    if (sp_count == 1)
    {
        const unsigned short packed_key = shapekey_pack16(shape_key);
        bool found = false;
        for (int i = 0; i < base.data_count; i++)
        {
            const comp_sp_combined_direct_damage& entry = base.get_combined_direct_damage(i);
            if (entry.compressed_shape_key == packed_key)
            {
                base.set_combined_direct_damage(i, int_pack16(int_unpack16(entry.compressed_damage) + damage), entry.compressed_shape_key);
                found = true;
            }
        }
        if (!found)
        {
            base.set_combined_direct_damage(base.data_count, int_pack16(damage), packed_key);
            base.data_count++;
        }
    }
    else
    {
        for (int i = 0; i < sp_count; i++)
        {
            base.set_combined_direct_damage(i, int_pack16(combined_sps[i].damage), shapekey_pack16(combined_sps[i].shape_key));
        }
        base.data_count = (unsigned char)sp_count;
    }
    multi_game_destruction_set_direct_damage_working_data(base);
    return true;
}

// 0x00713BB0 (:5311)
// Kept: the indices are appended without checking the room left (7).
bool rfg_dyn_add_pre_collide_link_broken(rfg_mover* mover, unsigned int num_broken_links, const unsigned int* link_indices, unsigned char flags, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return true;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_LINK_BROKEN, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, flags, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev == nullptr)
    {
        return false;
    }
    ev->collision_base.set_link_indices(num_broken_links, link_indices); // inlined
    multi_game_destruction_set_link_broken_working_data(ev->collision_base);
    return true;
}

// 0x00713DB0 (:5330)
// From the server a new event; locally the pieces join a piece array event of this step (get_sp_array_event looks up
// the first key only).
bool rfg_dyn_add_pre_collide_sp_array(rfg_mover* mover, const object* owner, unsigned int* shape_keys, int num_keys, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server) || mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_collision_event* ev;
    if (from_server)
    {
        ev = next_free_collision_event(DET_SP_ARRAY, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
        if (ev == nullptr)
        {
            return true;
        }
    }
    else
    {
        ev = get_sp_array_event(mover->get_handle(), *shape_keys);
    }
    if (ev != nullptr)
    {
        for (int i = 0; i < num_keys; i++)
        {
            ev->collision_base.set_direct_sps(ev->collision_base.data_count + i, shapekey_pack16(shape_keys[i]));
        }
        ev->collision_base.data_count += (unsigned char)num_keys;
        multi_game_destruction_set_sp_array_working_data(ev->collision_base);
    }
    return true;
}

// 0x00713F60 (:5376)
bool rfg_dyn_add_pre_collide_shatter(rfg_mover* mover, const object* owner, unsigned int shape_key, unsigned int damage, const vector& local_pos, const vector& local_dir, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server) || mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_SHATTER, mover->get_handle(), 0xFFFFFFFF, shape_key, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev != nullptr)
    {
        ev->collision_base.damage += damage;
        ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
        ev->collision_base.base_a.raw.rcollision_point_local = local_pos;
        ev->collision_base.base_a.raw.rcollision_normal_local = local_dir;
        multi_game_destruction_set_shatter_working_data(ev->collision_base);
    }
    return true;
}

// 0x0071C240 (:5410)
bool rfg_dyn_add_pre_collide_shape_cutter(rfg_mover* mover, const object* owner, float radius, const vector& local_pos, unsigned char exp_info_unique_id, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server) || mover == nullptr || mover->obj_flags.destroyed)
    {
        return false;
    }
    if (!mover->rdd.is_loaded() || mover->rdd.get_data()->shape == nullptr)
    {
        sprintf(Rfg_dynamics_message, "uh... trying to cut a rfg_mover that isn't streamed in: %d", mover->get_handle()); // never shown
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_SHAPE_CUTTER, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev != nullptr)
    {
        ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
        ev->collision_base.radius = radius;
        ev->collision_base.exp_unique_id = exp_info_unique_id;
        ev->collision_base.base_a.raw.rcollision_point_local = local_pos;
        multi_game_destruction_set_shape_cutter_working_data(ev->collision_base);
    }
    return true;
}

// 0x007140E0 (:5452)
// Only fixed movers are repaired.
bool rfg_dyn_add_pre_collide_repair(rfg_mover* mover, const object* owner, const vector& local_pos, const float radius, const int duration, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return true;
    }
    if (mover->get_move_type() != RFGM_FIXED)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_REPAIR, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev == nullptr)
    {
        return false;
    }
    ev->collision_base.repair_duration = duration;
    ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
    ev->collision_base.radius = radius;
    ev->collision_base.base_a.raw.rcollision_point_local = local_pos;
    multi_game_destruction_set_repair_working_data(ev->collision_base);
    return true;
}

// 0x00714270 (:5484)
bool rfg_dyn_add_pre_collide_mp_destroy(rfg_mover* mover, const object* owner, bool from_server)
{
    if (mover == nullptr)
    {
        return false;
    }
    if (rfg_dyn_skip_local_event(from_server))
    {
        return true;
    }
    if (mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_MP_DESTROY, mover->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev == nullptr)
    {
        return false;
    }
    ev->collision_base.owner = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
    multi_game_destruction_set_mp_destroy_working_data(ev->collision_base);
    return true;
}

// 0x007143D0 (:5509)
// A shardify marker event (multiplayer frames that only shardified).
bool rfg_dyn_add_pre_collide_shardify(bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return false;
    }
    next_free_collision_event(DET_SHARDIFY, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    return true;
}

// 0x007144F0 (:5520)
bool rfg_dyn_add_pre_collide_destruction_mode(const unsigned int* invulnerable_mover_handles, int num_handles, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_DESTRUCTION_MODE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, &num_handles); // inlined
    if (ev == nullptr)
    {
        return false;
    }
    if (invulnerable_mover_handles != nullptr)
    {
        for (int i = 0; i < num_handles; i++)
        {
            ev->collision_base.add_invulnerable_mover_handle(invulnerable_mover_handles[i]);
        }
    }
    return true;
}

// 0x007146B0 (:5541)
bool rfg_dyn_add_pre_collide_slice(rfg_mover* rmp, const vector& base_pos, int slice_index, float angle0, float angle1, bool from_server)
{
    if (rfg_dyn_skip_local_event(from_server))
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_SLICE, rmp->get_handle(), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev == nullptr)
    {
        return false;
    }
    add_pre_collide_slice_common(ev->collision_base, base_pos, slice_index, angle0, angle1); // inlined
    multi_game_destruction_set_slice_working_data(ev->collision_base);
    return true;
}

// 0x00714820 (:5563)
// Repairs the fixed rfg_movers around pos (up to 128 objects collected; authority only).
void rfg_dyn_repair_sphere(const vector& pos, const float radius, const int duration, human* hp)
{
    unsigned int handles[128];
    const int num_handles = havok_collect_objects_fast(pos, radius, handles, 128, COLLIDE_OBJECT_COLLECTOR, nullptr, nullptr);
    for (int i = 0; i < num_handles; i++)
    {
        object* objp = World.object_from_handle(handles[i]);
        if (objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG) // not checked for nullptr
        {
            continue;
        }
        vector local_pos = pos;
        vec_change_to_local_coords(local_pos, objp->pos, objp->orient);
        if (box_is_single_player_or_acts_as_server_for_destruction() && ((rfg_mover*)objp)->get_move_type() == RFGM_FIXED) // inlined
        {
            rfg_dyn_add_pre_collide_repair((rfg_mover*)objp, hp, local_pos, radius, duration, false); // inlined
        }
    }
}

// 0x00714A80 (:5591)
static void object_event_set_working_data(rfg_collision_event* ev)
{
    if (ev->collision_base.destruction_type == DET_OBJECT_SIMPLE)
    {
        multi_game_destruction_set_object_simple_working_data(ev->collision_base);
    }
    else if (ev->collision_base.destruction_type == DET_OBJECT_COMPLEX)
    {
        multi_game_destruction_set_object_complex_working_data(ev->collision_base);
    }
}

// 0x0072DAA0 (:5605)
// LTCG: obj_a in EAX, obj_b in EDI. Heavy destruction of either rfg_mover spreads to both.
static void rfg_dyn_add_pre_collide_contact_common(rfg_collision_event* ev, object* obj_a, object* obj_b)
{
    if (obj_a == nullptr)
    {
        return;
    }
    const int heavy_flags = rfg_dyn_is_heavy_destruction(obj_a) | (obj_b != nullptr ? rfg_dyn_is_heavy_destruction(obj_b) : 0); // inlined
    if (heavy_flags != 0)
    {
        if (obj_a->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            set_heavy_destruction_flags((rfg_mover*)obj_a, heavy_flags);
        }
        if (obj_b != nullptr && obj_b->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            set_heavy_destruction_flags((rfg_mover*)obj_b, heavy_flags);
        }
    }
    object_event_set_working_data(ev); // inlined
}

// 0x0072DBA0 (:5641)
// LTCG: obj_a in EBP, obj_b in EDI (the network's version of a contact).
bool rfg_dyn_multi_add_pre_collide_contact(object* obj_a, object* obj_b, unsigned int damage, const vector& collision_point_local_a, const vector& collision_normal_local_a, const vector& collision_point_local_b, const vector& collision_normal_local_b, unsigned int shape_key_a, unsigned int shape_key_b, bool apply_impulse)
{
    if (obj_a == nullptr)
    {
        return false;
    }
    rfg_collision_event* ev = next_free_collision_event(DET_INVALID, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, nullptr, 0xFFFF, nullptr, nullptr, nullptr); // inlined
    if (ev == nullptr)
    {
        return true;
    }
    rfg_collision_event_base& base = ev->collision_base;
    base.damage += damage;
    base.flags |= apply_impulse ? 8 : 0;
    base.destruction_type = obj_b != nullptr ? DET_OBJECT_COMPLEX : DET_OBJECT_SIMPLE;
    base.base_a.obj_handle = obj_a->get_handle();
    base.base_a.shape_key = shape_key_a;
    if (obj_b != nullptr)
    {
        base.base_b.obj_handle = obj_b->get_handle();
        base.base_b.shape_key = shape_key_b;
    }
    base.base_a.raw.rcollision_point_local = collision_point_local_a;
    base.base_a.raw.rcollision_normal_local = collision_normal_local_a;
    base.base_b.raw.rcollision_point_local = collision_point_local_b;
    base.base_b.raw.rcollision_normal_local = collision_normal_local_b;
    rfg_dyn_add_pre_collide_contact_common(ev, obj_a, obj_b);
    return true;
}

// 0x00714AA0 (:5776)
static bool can_damage_destroyable(object_type obj_type)
{
    switch (obj_type)
    {
    case OT_HUMAN:
    case OT_MOVER:
    case OT_VEHICLE:
    case OT_TURRET:
    case OT_ZONE:
        return true;
    default:
        return false;
    }
}

// 0x0071C400 (:5790)
// LTCG: obj_a in EAX, obj_b in ECX. In multiplayer general movers do not destroy.
static bool can_simple_object_cause_destruction(object* obj_a, bool simple_object_a, object* obj_b, bool simple_object_b)
{
    if (!Multiplayer)
    {
        return true;
    }
    if (simple_object_a && obj_a != nullptr && obj_a->get_obj_type() == OT_MOVER && obj_a->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return false;
    }
    return !(simple_object_b && obj_b != nullptr && obj_b->get_obj_type() == OT_MOVER && obj_b->get_sub_type() == OT_SUB_MOVER_GENERAL);
}

// 0x0072DD60 (:5798)
// LTCG: shape_key in EAX, objp in EDX. General movers: gm_flags 0x40; rfg_movers: base flag 0x40 of the piece.
static bool get_subpiece_base_is_shatterable(object_mover* objp, unsigned int shape_key)
{
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return ((((general_mover*)objp)->gm_flags >> 6) & 1) != 0;
    }
    if (objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    rfg_mover* rm = (rfg_mover*)objp;
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return false;
    }
    const rfg_subpiece_base* sp_base = shape->get_subpiece_base(shape_key);
    return sp_base != nullptr && (sp_base->flags & 0x40) != 0;
}

// 0x00714AE0 (:5803)
rfg_dyn_collision_energy_info::rfg_dyn_collision_energy_info(object* _obj_a, object* _obj_b, const vector& _collision_point_world)
    : obj_a(_obj_a)
    , obj_b(_obj_b)
    , collision_point_world(_collision_point_world)
{
}

// 0x00714B00 (:5810)
rfg_dyn_point_collision_energy_info::rfg_dyn_point_collision_energy_info(object* _obj_a, object* _obj_b, const vector& _collision_point_world)
    : rfg_dyn_collision_energy_info(_obj_a, _obj_b, _collision_point_world)
    , event_flags(0)
{
}

// 0x0071C440 (:5817)
// A vehicle a walker threw recently.
static bool thrown_by_walker(object* objp)
{
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !((vehicle*)objp)->m_walker_thrown_energy_scaled.elapsed();
}

// 0x0071C470 (:5826)
// LTCG: energy_info in ESI. The kinetic energy of the bodies in their centre of mass frame at the contact (masses and
// velocities from get_contact_point_properties), scaled by the energy scale of the non-mover side (of a, when b is a
// mover) and x100 for vehicles thrown by a walker. Slow (relative speed below 1 m/s) or weak (< 200) contacts get event
// flag 0x2 (no destruction). False when the velocities are not finite.
static bool rfg_dyn_compute_point_collision_energy(rfg_dyn_point_collision_energy_info& energy_info, const hkpContactPointAddedEvent* contact_event)
{
    object* obj_a = energy_info.obj_a;
    object* obj_b = energy_info.obj_b;
    get_contact_point_properties(obj_a, energy_info.collision_point_world, energy_info.mass_a, energy_info.vel_a, contact_event != nullptr ? contact_event->m_bodyA : nullptr);
    if (!fl_is_finite(energy_info.vel_a.x * energy_info.vel_a.x + energy_info.vel_a.y * energy_info.vel_a.y + energy_info.vel_a.z * energy_info.vel_a.z))
    {
        return false;
    }
    get_contact_point_properties(obj_b, energy_info.collision_point_world, energy_info.mass_b, energy_info.vel_b, contact_event != nullptr ? contact_event->m_bodyB : nullptr);
    if (!fl_is_finite(energy_info.vel_b.x * energy_info.vel_b.x + energy_info.vel_b.y * energy_info.vel_b.y + energy_info.vel_b.z * energy_info.vel_b.z))
    {
        return false;
    }
    float v_cm_a;
    float v_cm_b;
    rfg_dyn_calc_vel_sqr_cm(energy_info.mass_a, energy_info.vel_a, energy_info.mass_b, energy_info.vel_b, v_cm_a, v_cm_b); // inlined
    if (v_cm_a + v_cm_b < 1.0f)
    {
        energy_info.event_flags |= 2;
    }
    float scale = obj_b->get_obj_type() == OT_MOVER ? obj_a->get_energy_scale() : obj_b->get_energy_scale();
    if (thrown_by_walker(obj_a) || thrown_by_walker(obj_b)) // inlined
    {
        scale *= 100.0f;
    }
    energy_info.energy = rfg_dyn_calc_energy(energy_info.mass_a, v_cm_a, energy_info.mass_b, v_cm_b) * scale; // inlined
    if (energy_info.energy < 200.0f)
    {
        energy_info.event_flags |= 2;
    }
    return true;
}

// 0x0071C850 (:5863)
bool rfg_dyn_compute_collision_energy(rfg_dyn_collision_energy_info& energy_info)
{
    rfg_dyn_point_collision_energy_info point_info(energy_info.obj_a, energy_info.obj_b, energy_info.collision_point_world);
    const bool result = rfg_dyn_compute_point_collision_energy(point_info, nullptr);
    energy_info.energy = point_info.energy;
    return result;
}

// 0x00714B20 (:5872)
// Whether a walker's body part is an arm (body property 5 in 4..24).
static bool is_multi_walker_arm_collision(const hkpCdBody* walker_body)
{
    const hkpCollidable* root = walker_body->getRootCollidable(); // inlined
    const unsigned short index = (unsigned short)(unsigned int)havok_get_property(hkpGetRigidBody(root), 5, (void*)0xFFFF, true);
    return (unsigned short)(index - 4) < 21;
}

// 0x0072DDB0 (:5884)
// A Havok contact between two destroying objects (humans, movers, vehicles, turrets, zones) becomes an object event:
// simple when one side is not an rfg_mover (shatterable pieces count as simple against non-shatterable ones; two
// simple sides are ignored), complex between two rfg_movers. The rfg_mover goes to side a (between two, the lower
// address). The energy of the contact is the damage. Fixed movers and vehicles push the pieces (0x8); walkers may
// force links to break (0x10). On bridges, walker feet and vehicle wheels (collision filter info 0x2F) do nothing (flag
// 0x1). The first contact of the event records the local hit points and normals.
bool rfg_dyn_add_pre_collide_contact(hkpContactPointAddedEvent& add_event, object* obj_a, object* obj_b)
{
    if (add_event.m_type.m_storage == hkpContactPointAddedEvent::TYPE_TOI)
    {
        return true;
    }
    if (add_event.m_type.m_storage != hkpContactPointAddedEvent::TYPE_MANIFOLD)
    {
        return false;
    }
    const unsigned short contact_id = add_event.asManifoldEvent().m_contactPointId; // inlined
    if (contact_id == 0xFFFF)
    {
        return true;
    }
    if (obj_a == nullptr || obj_b == nullptr || obj_a->obj_flags.destroyed || obj_b->obj_flags.destroyed)
    {
        return false;
    }
    if (!can_damage_destroyable((object_type)obj_a->get_obj_type()) || !can_damage_destroyable((object_type)obj_b->get_obj_type()))
    {
        return true;
    }
    unsigned int key_a = add_event.m_bodyA->m_shapeKey;
    unsigned int key_b = add_event.m_bodyB->m_shapeKey;
    if (rfg_dyn_filter_destruction(obj_a, obj_b, key_a, key_b))
    {
        return false;
    }
    bool simple_a = obj_a->get_sub_type() != OT_SUB_MOVER_RFG;
    bool simple_b = obj_b->get_sub_type() != OT_SUB_MOVER_RFG;
    if (!can_simple_object_cause_destruction(obj_a, simple_a, obj_b, simple_b))
    {
        return true;
    }
    if (obj_a->get_obj_type() == OT_MOVER && obj_b->get_obj_type() == OT_MOVER)
    {
        const bool shatterable_a = get_subpiece_base_is_shatterable((object_mover*)obj_a, key_a);
        const bool shatterable_b = get_subpiece_base_is_shatterable((object_mover*)obj_b, key_b);
        if (shatterable_a != shatterable_b)
        {
            simple_b |= shatterable_a;
            simple_a |= shatterable_b;
            if (simple_a && simple_b)
            {
                return true;
            }
        }
    }
    vector contact_pos;
    vector contact_normal;
    havok_to_game_vector(contact_pos, add_event.m_contactPoint->m_position);
    havok_to_game_vector(contact_normal, add_event.m_contactPoint->m_separatingNormal);
    if (!fl_is_finite(contact_pos.mag_squared()) || !fl_is_finite(contact_normal.mag_squared()))
    {
        return false;
    }
    rfg_dyn_point_collision_energy_info energy_info(obj_a, obj_b, contact_pos);
    if (!rfg_dyn_compute_point_collision_energy(energy_info, &add_event))
    {
        return false;
    }
    destruction_event_type det;
    bool swap;
    if (simple_a || simple_b)
    {
        swap = simple_a;
        det = DET_OBJECT_SIMPLE;
    }
    else
    {
        swap = obj_b < obj_a;
        det = DET_OBJECT_COMPLEX;
    }
    if (swap)
    {
        swap_dynamics(obj_a, obj_b);
        swap_dynamics(key_a, key_b);
        swap_dynamics(energy_info.mass_a, energy_info.mass_b);
        swap_dynamics(energy_info.vel_a, energy_info.vel_b);
        contact_normal = -contact_normal;
    }
    const hkpCdBody* body_b = swap ? add_event.m_bodyA : add_event.m_bodyB;
    if (Multiplayer)
    {
        if (box_acts_as_client_for_destruction())
        {
            return true;
        }
        if (obj_b != nullptr && obj_b->get_obj_type() == OT_VEHICLE && !is_multi_walker_arm_collision(body_b) && !Allow_mp_vehicle_mover_collision)
        {
            return true;
        }
    }
    if (det == DET_OBJECT_SIMPLE)
    {
        key_b = 0xFFFFFFFF;
    }
    rfg_collision_event* ev = next_free_collision_event(det, obj_a->get_handle(), obj_b->get_handle(), key_a, key_b, energy_info.event_flags, add_event.m_internalContactMgr, contact_id, nullptr, nullptr, nullptr);
    if (ev == nullptr)
    {
        return true;
    }
    ev->collision_base.damage += (int)(long long)energy_info.energy;
    const bool fixed_a = obj_a->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)obj_a)->get_move_type() == RFGM_FIXED;
    const bool fixed_b = obj_b->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)obj_b)->get_move_type() == RFGM_FIXED;
    if (fixed_a || fixed_b || obj_b->get_obj_type() == OT_VEHICLE)
    {
        ev->collision_base.flags |= 8;
        if (obj_b->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = (vehicle*)obj_b;
            if (vp->info->flags.force_break_links)
            {
                ev->collision_base.flags |= 0x10;
            }
            rfg_mover* rm = World.object_cast<rfg_mover>(obj_a);
            if (rm->mover_is_type(MOVER_TYPE_BRIDGE) != 0)
            {
                const hkpCdBody* root = body_b;
                while (root->m_parent != nullptr)
                {
                    root = root->m_parent;
                }
                bool ignore = false;
                if (obj_b->get_sub_type() == OT_SUB_VEHICLE_WALKER)
                {
                    const unsigned short index = (unsigned short)(unsigned int)havok_get_property(hkpGetRigidBody((const hkpCollidable*)root), 5, (void*)0xFFFF, true);
                    ignore = index == 0xFFFF || walker_idx_is_foot((walker*)obj_b, index);
                }
                else
                {
                    const hkpShapeContainer* container = root->m_shape->getContainer();
                    ignore = container != nullptr && (container->getCollisionFilterInfo(body_b->m_shapeKey) & 0xFFFF) == 0x2F;
                }
                if (ignore)
                {
                    ev->collision_base.flags |= 1;
                    return true;
                }
            }
        }
    }
    if (ev->is_first_event(add_event.m_internalContactMgr, contact_id))
    {
        convert_point_direction_from_world_to_local(ev->collision_base.base_a.raw.rcollision_point_local, ev->collision_base.base_a.raw.rcollision_normal_local, contact_pos, contact_normal, obj_a->pos, obj_a->orient);
        if (det == DET_OBJECT_COMPLEX)
        {
            const vector normal_b = -contact_normal;
            convert_point_direction_from_world_to_local(ev->collision_base.base_b.raw.rcollision_point_local, ev->collision_base.base_b.raw.rcollision_normal_local, contact_pos, normal_b, obj_b->pos, obj_b->orient);
        }
    }
    rfg_dyn_add_pre_collide_contact_common(ev, obj_a, obj_b);
    return true;
}

// 0x00714B60 (:6130)
// A removed contact leaves its object event; the event dies (flag 0x1) with its last contact.
void rfg_dyn_remove_pre_collide_contact(hkpContactPointRemovedEvent& remove_event)
{
    if (remove_event.m_contactPointId == 0xFFFF)
    {
        return;
    }
    cs_scope lock(Collision_event_wrapper);
    for (int i = 0; i < Num_rfg_dyn_collisions; i++)
    {
        rfg_collision_event* ev = &RFG_dyn_collisions[i];
        if ((ev->collision_base.destruction_type != DET_OBJECT_COMPLEX && ev->collision_base.destruction_type != DET_OBJECT_SIMPLE) || ev->havok_base.mgr != remove_event.m_internalContactMgr)
        {
            continue;
        }
        for (int k = 0; k < ev->havok_base.count; k++)
        {
            if (ev->havok_base.id[k] != remove_event.m_contactPointId)
            {
                continue;
            }
            ev->havok_base.count--;
            if (ev->havok_base.count == 0)
            {
                ev->collision_base.flags |= 1;
                return;
            }
            if (k != ev->havok_base.count)
            {
                ev->havok_base.id[k] = ev->havok_base.id[ev->havok_base.count];
            }
            return;
        }
    }
}

// 0x00714C30 (:6160)
void rfg_dyn_reset_destruction_frame(unsigned short frame)
{
    Destruction_frame = frame;
}

// 0x00714C40 (:6169)
unsigned short rfg_dyn_get_active_destruction_frame()
{
    return Active_destruction_frame;
}

// ---- processing the events
// 0x0071C8A0 (:6181)
// LTCG: ev in ESI. An event dies (flag 0x1) when its movers are gone, destroyed or invulnerable, or its pieces left
// them. Returns whether it is live.
static bool rfg_dyn_mp_collision_set_event_is_live(rfg_collision_event* ev)
{
    rfg_collision_event_base& base = ev->collision_base;
    if ((base.flags & 3) != 0)
    {
        return false;
    }
    rfg_mover* rm_a = nullptr;
    rfg_mover* rm_b = nullptr;
    bool live = false;
    switch (base.destruction_type)
    {
    case DET_EXPLOSION:
    case DET_DIRECT_DAMAGE:
    case DET_SP_ARRAY:
    case DET_OBJECT_SIMPLE:
    case DET_SHATTER:
    case DET_SHAPE_CUTTER:
    case DET_LINK_BROKEN:
    case DET_SLICE:
        rm_a = World.object_from_handle<rfg_mover>(base.base_a.obj_handle);
        live = rm_a != nullptr && !rm_a->obj_flags.destroyed && !rm_a->flags.invulnerable;
        break;
    case DET_OBJECT_COMPLEX:
        rm_a = World.object_from_handle<rfg_mover>(base.base_a.obj_handle);
        rm_b = World.object_from_handle<rfg_mover>(base.base_b.obj_handle);
        live = rm_a != nullptr && !rm_a->obj_flags.destroyed && !rm_a->flags.invulnerable && rm_b != nullptr && !rm_b->obj_flags.destroyed && !rm_b->flags.invulnerable;
        break;
    case DET_REPAIR:
        rm_a = World.object_from_handle<rfg_mover>(base.base_a.obj_handle);
        live = rm_a != nullptr && !rm_a->obj_flags.destroyed;
        break;
    case DET_MP_DESTROY:
        rm_a = World.object_from_handle<rfg_mover>(base.base_a.obj_handle);
        live = true;
        break;
    case DET_MP_VEHICLE:
        rm_a = World.object_from_handle<rfg_mover>(base.base_a.obj_handle);
        live = rm_a != nullptr && !rm_a->obj_flags.destroyed && !rm_a->flags.invulnerable;
        break;
    case DET_SHARDIFY:
    case DET_DESTRUCTION_MODE:
        live = true;
        break;
    default:
        base.flags |= 1; // Int3 critical function
        live = true;
        break;
    }
    if (!live)
    {
        base.flags |= 1;
    }
    switch (base.destruction_type)
    {
    case DET_DIRECT_DAMAGE:
        if (rm_a != nullptr)
        {
            rfg_destroyable_shape* shape = rm_a->get_shape();
            for (int i = 0; i < base.data_count; i++)
            {
                if (!shape->get_shape_state(shapekey_unpack16(base.get_combined_direct_damage(i).compressed_shape_key)))
                {
                    base.flags |= 1;
                }
            }
        }
        break;
    case DET_SP_ARRAY:
        if (rm_a != nullptr)
        {
            rfg_destroyable_shape* shape = rm_a->get_shape();
            for (int i = 0; i < base.data_count; i++)
            {
                if (!shape->get_shape_state(shapekey_unpack16(base.get_direct_sps(i))))
                {
                    base.flags |= 1;
                }
            }
        }
        break;
    case DET_OBJECT_COMPLEX:
        if (rm_b != nullptr && !rm_b->get_shape()->get_shape_state(base.base_b.shape_key))
        {
            base.flags |= 1;
        }
        // fall through: side a as a simple event
    case DET_OBJECT_SIMPLE:
        if (rm_a != nullptr && !rm_a->get_shape()->get_shape_state(base.base_a.shape_key))
        {
            base.flags |= 1;
        }
        break;
    case DET_LINK_BROKEN:
        if (rm_a != nullptr)
        {
            rfg_destroyable_shape* shape = rm_a->get_shape();
            for (int i = 0; i < base.data_count; i++)
            {
                // kept: only checks the address of the piece (0x20, dynamic links) or link for nullptr
                const void* target = (base.flags & 0x20) != 0 ? (const void*)&shape->get_object_buffer()[base.get_link_index(i)] : (const void*)shape->get_link(base.get_link_index(i));
                if (target == nullptr)
                {
                    base.flags |= 1;
                }
            }
        }
        break;
    default:
        break;
    }
    return (base.flags & 1) == 0;
}

// 0x00714C50 (:6274)
// Higher first: multiplayer destroys, destruction mode, slices, the others, link breaks; repairs come last except on
// the terminal Havok step.
static int get_destruction_sort_priority(destruction_event_type type)
{
    switch (type)
    {
    case DET_REPAIR:
        return Terminal_havok_step ? 4 : -1;
    case DET_MP_DESTROY:
        return 10;
    case DET_DESTRUCTION_MODE:
        return 8;
    case DET_LINK_BROKEN:
        return 0;
    case DET_SLICE:
        return 6;
    default:
        return 1;
    }
}

// 0x00714CB0 (:6302)
// Dead or delayed events (flags 0x3) first, then by priority.
static int destruction_compare(const void* arg1, const void* arg2)
{
    const rfg_collision_event* ev1 = (const rfg_collision_event*)arg1;
    const rfg_collision_event* ev2 = (const rfg_collision_event*)arg2;
    const int flags1 = ev1->collision_base.flags & 3;
    const int flags2 = ev2->collision_base.flags & 3;
    if (flags1 != 0 || flags2 != 0)
    {
        return flags2 - flags1;
    }
    return get_destruction_sort_priority(ev2->collision_base.destruction_type) - get_destruction_sort_priority(ev1->collision_base.destruction_type); // inlined
}

// 0x0073AAD0 (:6324)
// LTCG: multi_client_must_process in EAX. Starts the destruction frame: a client takes the server's events; the
// events are validated and (with authority) sorted. Up to 5 live events are processed this step after the dead ones,
// stopping at a repair unless this is the terminal Havok step (where repairs move behind the others). Beyond 15
// pending events (30 in the countdown) the rest is dropped, keeping the multiplayer destroys and (outside the terminal
// step) some trailing repairs.
static void rfg_dyn_mp_do_collide_post_setup(int& num_events_to_process, bool& multi_client_must_process)
{
    Active_destruction_frame = Destruction_frame;
    Mover_destruction_results.num_synced_general_movers_created_by_destruction = 0;
    Mover_destruction_results.num_rfg_movers_created_by_destruction = 0;
    Mover_destruction_results.num_rfg_movers_destroyed_by_destruction = 0;
    Mover_destruction_results.num_shardify_delete_counts = 0;
    const bool client = box_acts_as_client_for_destruction(); // inlined
    const bool authority = !client;
    if (authority && multi_game_mode_is_pregame())
    {
        return;
    }
    multi_client_must_process = false;
    if (client)
    {
        multi_client_must_process = multi_game_destruction_frame_client_get_destruction_events();
        if (multi_game_is_migrating())
        {
            get_destruction_next_frame_id(multi_game_destruction_get_last_applied_destruction_frame()); // result unused
        }
    }
    for (int i = 0; i < Num_rfg_dyn_collisions; i++)
    {
        rfg_dyn_mp_collision_set_event_is_live(&RFG_dyn_collisions[i]);
    }
    Terminal_havok_step = havok_get_is_terminal_havok_step() || multi_game_is_migrating();
    if (authority)
    {
        qsort(RFG_dyn_collisions, Num_rfg_dyn_collisions, sizeof(rfg_collision_event), destruction_compare);
    }
    int num_dead = 0;
    int next = 0;
    while (next < Num_rfg_dyn_collisions && (RFG_dyn_collisions[next].collision_base.flags & 3) != 0)
    {
        next++;
        num_dead++;
    }
    bool has_repair = false;
    int num_live = 0;
    while (next < Num_rfg_dyn_collisions)
    {
        if (RFG_dyn_collisions[next].collision_base.destruction_type == DET_REPAIR)
        {
            if (!Terminal_havok_step)
            {
                break;
            }
            has_repair = true;
        }
        if (4 < num_live)
        {
            break;
        }
        next++;
        num_live++;
    }
    const int num_to_process = num_dead + num_live;
    num_events_to_process = num_to_process;
    rfg_dyn_record_events_processed(num_to_process); // inlined
    if (authority && Terminal_havok_step && has_repair)
    {
        int last = num_to_process - 1;
        for (int i = num_dead; i < last; i++)
        {
            if (RFG_dyn_collisions[i].collision_base.destruction_type == DET_REPAIR)
            {
                swap_dynamics(RFG_dyn_collisions[i], RFG_dyn_collisions[last]);
                last--;
            }
        }
    }
    const int num_pending = Num_rfg_dyn_collisions - num_live - num_dead;
    const int max_pending = multi_game_mode_is_countdown() ? 30 : 15;
    if (max_pending < num_pending)
    {
        int num_mp_destroys = 0;
        for (int i = next; i < Num_rfg_dyn_collisions && RFG_dyn_collisions[i].collision_base.destruction_type == DET_MP_DESTROY; i++)
        {
            num_mp_destroys++;
        }
        int keep = num_pending;
        if (Terminal_havok_step)
        {
            keep = max_pending < num_mp_destroys ? num_mp_destroys : max_pending;
        }
        else if (num_mp_destroys + max_pending < num_pending)
        {
            keep = num_mp_destroys + max_pending;
            int num_repairs = 0;
            for (int i = Num_rfg_dyn_collisions - 1; next <= i && RFG_dyn_collisions[i].collision_base.destruction_type == DET_REPAIR; i--)
            {
                num_repairs++;
            }
            if (0 < num_repairs)
            {
                const int count = max_pending <= num_repairs ? max_pending : num_repairs;
                // kept: copies the first trailing repairs (not the last ones) over the end of the kept range
                memmove(&RFG_dyn_collisions[num_mp_destroys - count + max_pending + num_live + num_dead], &RFG_dyn_collisions[Num_rfg_dyn_collisions - num_repairs], count * sizeof(rfg_collision_event));
            }
        }
        if (num_dead < Num_rfg_dyn_collisions - keep - num_live)
        {
            Num_rfg_dyn_collisions = keep + num_live + num_dead;
        }
    }
    if (authority)
    {
        multi_game_destruction_frame_clear();
    }
}

// 0x00714DB0 (:6513)
// Ends the destruction frame: the server sends the movers created and destroyed (and validates the frame), drops the
// processed events (keeping the delayed ones, flag 0x4, for the next step); a client applies the server's results.
static void rfg_dyn_do_mp_collide_post_finalize(int num_events_processed, bool multi_client_must_process)
{
    for (int i = 0; i < Num_rfg_dyn_collisions; i++)
    {
        RFG_dyn_collisions[i].havok_base.mgr = nullptr;
    }
    const bool client = box_acts_as_client_for_destruction(); // inlined
    bool has_results = 0 < Mover_destruction_results.num_synced_general_movers_created_by_destruction || 0 < Mover_destruction_results.num_rfg_movers_created_by_destruction || 0 < Mover_destruction_results.num_rfg_movers_destroyed_by_destruction || 0 < Mover_destruction_results.num_shardify_delete_counts;
    if (!client)
    {
        unsigned short last_frame;
        if (has_results || multi_game_destruction_frame_in_use())
        {
            has_results = true;
            multi_game_destruction_frame_server_add_create_general_movers(Mover_destruction_results.synced_general_movers_created_by_destruction, Mover_destruction_results.num_synced_general_movers_created_by_destruction);
            multi_game_destruction_frame_server_add_create_rfg_movers(Mover_destruction_results.rfg_movers_created_by_destruction, Mover_destruction_results.num_rfg_movers_created_by_destruction);
            multi_game_destruction_frame_server_add_destroyed_rfg_movers(Mover_destruction_results.rfg_movers_destroyed_by_destruction, Mover_destruction_results.num_rfg_movers_destroyed_by_destruction);
            multi_game_destruction_frame_server_add_shardify_delete_counts(Mover_destruction_results.shardify_delete_count_records, Mover_destruction_results.num_shardify_delete_counts);
            multi_game_destruction_frame_server_finalize_packet();
            multi_game_destruction_send_validate_destruction_frame(Active_destruction_frame);
            last_frame = Active_destruction_frame;
        }
        else
        {
            has_results = false;
            last_frame = Active_destruction_frame - 1;
        }
        multi_game_destruction_sync_jip(0, last_frame);
        int num_delayed = 0;
        for (int i = num_events_processed - 1; 0 <= i; i--)
        {
            rfg_collision_event* ev = &RFG_dyn_collisions[i];
            if ((ev->collision_base.flags & 4) == 0)
            {
                continue;
            }
            num_delayed++;
            num_events_processed--;
            ev->collision_base.flags &= ~4;
            if (i != num_events_processed)
            {
                memmove(&RFG_dyn_collisions[num_events_processed], ev, sizeof(rfg_collision_event));
            }
        }
        if (Multiplayer == true && multi_is_server() && num_delayed != 0 && Destruction_event_history != nullptr) // rfg_dyn_record_events_delayed inlined
        {
            Destruction_event_history_running_total -= num_delayed;
            Destruction_event_history[Destruction_event_history_index] -= num_delayed;
        }
        if (num_events_processed == Num_rfg_dyn_collisions)
        {
            Num_rfg_dyn_collisions = 0;
        }
        else
        {
            if (num_events_processed < Num_rfg_dyn_collisions && 0 < num_events_processed)
            {
                memmove(RFG_dyn_collisions, &RFG_dyn_collisions[num_events_processed], (Num_rfg_dyn_collisions - num_events_processed) * sizeof(rfg_collision_event));
            }
            Num_rfg_dyn_collisions -= num_events_processed;
        }
    }
    if (client)
    {
        if (has_results || multi_client_must_process)
        {
            multi_game_destruction_frame_client_process_synced_general_mover_remote_objects(Mover_destruction_results.synced_general_movers_created_by_destruction, Mover_destruction_results.num_synced_general_movers_created_by_destruction);
            multi_game_destruction_frame_client_process_new_rfg_mover_remote_objects(Mover_destruction_results.rfg_movers_created_by_destruction, Mover_destruction_results.num_rfg_movers_created_by_destruction);
            multi_game_destruction_frame_client_process_destroyed_rfg_mover_remote_objects(Mover_destruction_results.rfg_movers_destroyed_by_destruction, Mover_destruction_results.num_rfg_movers_destroyed_by_destruction);
            multi_game_destruction_frame_client_inc_next_destruction_frame();
            if (!multi_game_is_migrating())
            {
                multi_game_destruction_process_validate_destruction_frame(multi_game_destruction_get_last_applied_destruction_frame());
            }
        }
        Num_rfg_dyn_collisions = 0;
    }
}

// 0x00715040 (:6609)
static void server_add_destruction_event(rfg_collision_event* ev)
{
    if (box_acts_as_server_for_destruction()) // inlined
    {
        multi_game_destruction_frame_server_add_destruction_event(ev->collision_base);
    }
}

// 0x00715070 (:6620)
// By destruction uid, the movers without one (-1) first, by handle.
static int rfg_mover_list_compare(const void* arg1, const void* arg2)
{
    const rfg_mover_process_info* info1 = (const rfg_mover_process_info*)arg1;
    const rfg_mover_process_info* info2 = (const rfg_mover_process_info*)arg2;
    if (info1->destruction_uid != 0xFFFFFFFF)
    {
        if (info2->destruction_uid == 0xFFFFFFFF)
        {
            return 1;
        }
        return (int)(info1->destruction_uid - info2->destruction_uid);
    }
    if (info2->destruction_uid == 0xFFFFFFFF)
    {
        return (int)(info1->handle - info2->handle);
    }
    return -1;
}

// 0x007247B0 (:6643)
// Whether a mover has pieces to shardify. A shape whose dynamic links changed (flag 0x1) queues the linked pieces
// whose link went away; a fixed mover left without dynamic links starts moving (no longer held, flag 0x20 cleared):
// its move type follows its size unless anchored (0x10).
static bool rfg_mover_should_shardify(rfg_mover* rm)
{
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return false;
    }
    if ((shape->flags & 1) != 0)
    {
        shape->flags &= ~1;
        bool linked = false;
        if (shape->child_obj_list != 0xFFFF)
        {
            for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
            {
                if ((sp->flags & 2) != 0)
                {
                    rfg_dynamic_link* rdl = sp->dynamic_link_index != 0xFFFF ? RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index) : nullptr;
                    if (rdl == nullptr)
                    {
                        shape->add_subpiece_to_shardify_q(sp);
                        sp->flags &= ~2u;
                    }
                    else
                    {
                        linked = true;
                    }
                }
                if (sp->next == shape->child_obj_list)
                {
                    break;
                }
            }
        }
        if (rm->get_move_type() == RFGM_FIXED && !linked)
        {
            rm->mover_flags &= ~0x8000000u;
            shape->flags &= ~0x20;
            if ((shape->flags & 0x10) == 0)
            {
                const rfg_move_type move_type = get_move_type_for_new_shard(shape->num_existing_child_shapes, (rm->mover_flags & 2) << 3); // inlined; large 0x10
                rm->set_move_type(move_type, false); // inlined
                rm->mover_flags |= 8;
                rm->flags.pristine = 0;
                havok_body_force_activate(rm->havok_handle);
                havok_body_update_cached_shape_data(rm->havok_handle);
            }
        }
    }
    return shape->shardify_q != 0xFFFF;
}

// 0x007249B0 (:6698)
// The candidates with pieces to shardify (the candidate list is locked by swapping its count with -1); in multiplayer
// sorted by destruction uid. Returns the count.
static int alloc_rfg_mover_process_shardify_list(rfg_mover_process_info* Rfg_mover_process_shardify_list)
{
    unsigned int count;
    do
    {
        count = _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, -1);
    } while (count == 0xFFFFFFFF);
    int num = 0;
    for (int i = 0; i < (int)count; i++)
    {
        rfg_mover* rm = Rfg_mover_process_shardify_candidate_list[i];
        if (rfg_mover_should_shardify(rm))
        {
            rfg_mover_process_info& info = Rfg_mover_process_shardify_list[num++];
            info.rmp = rm;
            info.handle = rm->get_handle();
            info.destruction_uid = rm->destruction_uid;
        }
        rm->process_shardify_candidate_index = 0xFFFFFFFF;
    }
    _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, 0);
    if (Multiplayer && 1 < num)
    {
        qsort(Rfg_mover_process_shardify_list, num, sizeof(rfg_mover_process_info), rfg_mover_list_compare);
    }
    return num;
}

// 0x0072E2D0 (:6727)
// A server frame that only shardified (no movers created or destroyed) still sends a shardify event.
static void maybe_add_shardify_event(int shardify_count)
{
    if (0 < shardify_count && Multiplayer && multi_is_server() && !multi_game_is_migrating() && !multi_game_destruction_frame_in_use() && Mover_destruction_results.num_synced_general_movers_created_by_destruction == 0 && Mover_destruction_results.num_rfg_movers_created_by_destruction == 0 && Mover_destruction_results.num_rfg_movers_destroyed_by_destruction == 0)
    {
        rfg_collision_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.collision_base.destruction_type = DET_SHARDIFY;
        server_add_destruction_event(&ev); // inlined
    }
}

// 0x0073AE70 (:6861)
void process_setup(lf_node<rfg_shardify_info>* this_node)
{
    if (rfg_dyn_shardify_setup(&this_node->value))
    {
        Work_queue->push(&this_node->base_node);
        return;
    }
    _InterlockedDecrement((volatile long*)&Num_to_work_on);
}

// 0x0073D510 (:6873)
void process_work(lf_node<rfg_shardify_info>* this_node)
{
    rfg_dyn_shardify_work(&this_node->value);
    Cleanup_queue->push(&this_node->base_node);
}

// 0x007406B0 (:6882)
void process_cleanup(lf_node<rfg_shardify_info>* this_node)
{
    _InterlockedDecrement((volatile long*)&Num_to_work_on);
    rfg_dyn_shardify_cleanup(&this_node->value);
}

// 0x007406D0 (:6897)
// Runs the queued shardify jobs on this thread until none is left (work first, then setup, then cleanup).
void swc_process_all()
{
    while (Num_to_work_on != 0)
    {
        t_base_node* node = Work_queue->pop();
        if (node != nullptr)
        {
            process_work((lf_node<rfg_shardify_info>*)node); // inlined
            continue;
        }
        node = Setup_queue->pop();
        if (node != nullptr)
        {
            process_setup((lf_node<rfg_shardify_info>*)node); // inlined
            continue;
        }
        node = Cleanup_queue->pop();
        if (node != nullptr)
        {
            process_cleanup((lf_node<rfg_shardify_info>*)node); // inlined
        }
    }
}

// 0x007150B0 (:6974)
// The shardify allocations (Common_mempool and the sim threads' pools) are released after each pass.
void mark_shardify_pools()
{
    Common_mempool_mark = Common_mempool.mark();
    for (int hw_thread_num = 0; hw_thread_num < 16; hw_thread_num++)
    {
        cpu_controller* controller = scheduler_get_controller(hw_thread_num);
        if (controller != nullptr && controller->job_type == ccjt_sim)
        {
            Sim_thread_marks[hw_thread_num] = controller->get_cpu_virtual_mempool().mark();
        }
    }
}

// 0x007150F0 (:6988)
void restore_to_mark_shardify_pools()
{
    Common_mempool.restore_to_mark(Common_mempool_mark);
    for (int hw_thread_num = 0; hw_thread_num < 16; hw_thread_num++)
    {
        cpu_controller* controller = scheduler_get_controller(hw_thread_num);
        if (controller != nullptr && controller->job_type == ccjt_sim)
        {
            controller->get_cpu_virtual_mempool().set_thread_ownership();
            controller->get_cpu_virtual_mempool().restore_to_mark(Sim_thread_marks[hw_thread_num]);
        }
    }
}

// 0x00740790 (:7002)
// After the Havok step: applies this step's events (all of them in single player; the server records them for the
// clients), shardifies the movers whose pieces broke (setup, work and cleanup in turn on this thread, on a list sized
// by the number of rfg_movers), disables the contacts of object events whose pieces changed this frame (contact
// distance 1000) and ends the destruction frame.
void rfg_dyn_do_collide_post()
{
    rfg_shape_shard_impulse_info_clear(); // inlined
    int num_events = 0;
    bool multi_client_must_process = false;
    if (Multiplayer)
    {
        rfg_dyn_mp_do_collide_post_setup(num_events, multi_client_must_process);
        if (box_is_single_player_or_acts_as_server_for_destruction() && multi_game_mode_is_pregame()) // inlined
        {
            return;
        }
    }
    else
    {
        num_events = Num_rfg_dyn_collisions;
    }
    Num_events_to_process_this_havok_step = num_events;
    for (int i = 0; i < num_events; i++)
    {
        rfg_collision_event* ev = &RFG_dyn_collisions[i];
        if ((ev->collision_base.flags & 3) != 0)
        {
            continue;
        }
        switch (ev->collision_base.destruction_type)
        {
        case DET_EXPLOSION:
            rfg_dyn_apply_explosion(ev);
            break;
        case DET_DIRECT_DAMAGE:
            rfg_dyn_apply_direct_damage(ev);
            break;
        case DET_SP_ARRAY:
            rfg_dyn_apply_sp_array(ev);
            break;
        case DET_OBJECT_SIMPLE:
            rfg_dyn_apply_object_simple(ev);
            break;
        case DET_OBJECT_COMPLEX:
            rfg_dyn_apply_object_complex(ev);
            break;
        case DET_SHATTER:
            rfg_dyn_apply_shatter(ev);
            break;
        case DET_SHAPE_CUTTER:
            rfg_dyn_apply_shape_cutter(ev);
            break;
        case DET_REPAIR:
            rfg_dyn_apply_repair(ev);
            break;
        case DET_MP_DESTROY:
            rfg_dyn_apply_mp_destroy(ev);
            break;
        case DET_MP_VEHICLE:
            rfg_dyn_apply_mp_veh(ev);
            break;
        case DET_SHARDIFY:
            break;
        case DET_DESTRUCTION_MODE:
            rfg_dyn_apply_destruction_mode(ev);
            break;
        case DET_LINK_BROKEN:
            rfg_dyn_apply_link_broken(ev);
            break;
        case DET_SLICE:
            rfg_dyn_apply_slice(ev);
            break;
        default:
            break; // Int3 critical function
        }
        if ((ev->collision_base.flags & 4) == 0)
        {
            server_add_destruction_event(ev); // inlined
        }
    }
    if (box_is_single_player_or_acts_as_server_for_destruction() || multi_client_must_process) // inlined
    {
        rfg_mover_process_info* process_list = (rfg_mover_process_info*)_alloca(World.get_num_objects<rfg_mover>() * sizeof(rfg_mover_process_info));
        const int num_shardify = alloc_rfg_mover_process_shardify_list(process_list);
        mark_shardify_pools();
        for (int i = 0; i < num_shardify; i++)
        {
            rfg_shardify_info info;
            info.rm = process_list[i].rmp;
            info.q_shard = nullptr;
            info.num_q_shards = 0;
            info.destruction_flags = 0;
            if (rfg_dyn_shardify_setup(&info))
            {
                rfg_dyn_shardify_work(&info);
                rfg_dyn_shardify_cleanup(&info);
            }
        }
        maybe_add_shardify_event(num_shardify);
        restore_to_mark_shardify_pools();
    }
    const unsigned char frame = (unsigned char)Framecount;
    for (int i = 0; i < Num_rfg_dyn_collisions; i++)
    {
        rfg_collision_event* ev = &RFG_dyn_collisions[i];
        if (ev->havok_base.mgr == nullptr || (ev->collision_base.flags & 1) != 0 || (ev->collision_base.destruction_type != DET_OBJECT_COMPLEX && ev->collision_base.destruction_type != DET_OBJECT_SIMPLE))
        {
            continue;
        }
        const bool modified = (ev->havok_base.affected_subpiece_a != nullptr && ev->havok_base.affected_subpiece_a->modified_framecount == frame) || (ev->havok_base.affected_subpiece_b != nullptr && ev->havok_base.affected_subpiece_b->modified_framecount == frame);
        if (!modified)
        {
            continue;
        }
        for (int k = 0; k < ev->havok_base.count; k++)
        {
            if (ev->havok_base.id[k] != 0xFFFF)
            {
                ev->havok_base.mgr->getContactPoint(ev->havok_base.id[k])->setDistance(1000.0f);
            }
        }
    }
    if (!Multiplayer)
    {
        Num_rfg_dyn_collisions = 0;
        return;
    }
    rfg_dyn_do_mp_collide_post_finalize(num_events, multi_client_must_process);
    if (box_acts_as_client_for_destruction()) // inlined
    {
        Num_rfg_dyn_collisions = 0;
    }
}

// ---- small helpers
// 0x00715140 (:7244)
int rfg_dyn_get_linked_index(const rfg_destroyable_shape* shape, int sp_index, const rfg_link_base* fl)
{
    return fl->obj[0] == sp_index ? fl->obj[1] : fl->obj[0];
}

// 0x00715160 (:7255)
rfg_subpiece* rfg_dyn_get_linked_subpiece(const rfg_destroyable_shape* shape, const rfg_subpiece* sp, const rfg_link_base* fl)
{
    rfg_subpiece* objects = const_cast<rfg_destroyable_shape*>(shape)->get_object_buffer();
    if (fl->obj[0] == (int)(sp - objects))
    {
        return &objects[fl->obj[1]];
    }
    return &objects[fl->obj[0]];
}

// 0x007151B0 (:7262)
// Over Small_shard_length_limit along an axis or Small_shard_volume_limit.
bool rfg_dyn_is_shard_large(const vector& bmin, const vector& bmax)
{
    const float ex = bmax.x - bmin.x;
    const float ey = bmax.y - bmin.y;
    const float ez = bmax.z - bmin.z;
    float largest = 0.0f <= ex - ey ? ex : ey;
    if (0.0f <= ez - largest)
    {
        largest = ez;
    }
    return Small_shard_length_limit < largest || Small_shard_volume_limit < ex * ey * ez;
}

// 0x0071CC50 (:7273)
// Under Too_small_shard_length_limit along every axis, Too_small_shard_volume_limit or Too_small_mass.
bool rfg_dyn_is_shard_small(const rfg_mover* rmp)
{
    const rfg_destroyable_shape* shape = rmp->rdd.is_loaded() ? rmp->rdd.get_data()->shape : nullptr; // not checked
    const float ex = shape->local_bmax.x - shape->local_bmin.x;
    const float ey = shape->local_bmax.y - shape->local_bmin.y;
    const float ez = shape->local_bmax.z - shape->local_bmin.z;
    float largest = 0.0f <= ex - ey ? ex : ey;
    if (largest - ez < 0.0f)
    {
        largest = ez;
    }
    return largest < Too_small_shard_length_limit || ex * ey * ez < Too_small_shard_volume_limit || shape->mass < Too_small_mass;
}

// 0x0071CD60 (:7288)
// Lighter pieces than their material's density (per m3) are pushed less (scaled by mass / density, at most 1).
float rfg_dyn_get_impulse_scalar(float mass, unsigned char material_index)
{
    const rfg_physical_material* material = rfg_get_physical_material_by_index(material_index); // inlined
    if (mass - 1.0f < 0.0f)
    {
        mass = 1.0f;
    }
    float scale = mass / material->density;
    if (scale < 0.0f)
    {
        return Destruction_explosion_impulse_scalar * 0.0f;
    }
    if (1.0f < scale)
    {
        scale = 1.0f;
    }
    return Destruction_explosion_impulse_scalar * scale;
}

// 0x00715290 (:7336)
// A removable (0x4) small (not 0x2) mover pushed harder than max_impulse_mag per kg.
bool rfg_dyn_maybe_delete(object_mover* obj_mover, const float mass, const vector& impulse, const float max_impulse_mag)
{
    return (obj_mover->mover_flags & 4) != 0 && (obj_mover->mover_flags & 2) == 0 && mass * max_impulse_mag < sqrtf(impulse.x * impulse.x + impulse.y * impulse.y + impulse.z * impulse.z);
}

// 0x00715320 (:7345)
unsigned int rfg_dyn_get_filter_by_move_type(int mt, int shape_flags)
{
    switch (mt)
    {
    case RFGM_FIXED:
        return COLLIDE_RFG_INITIAL_POSITION;
    case RFGM_NORMAL:
        return (shape_flags & 4) != 0 ? COLLIDE_RFG_NORMAL_FAST : COLLIDE_RFG_NORMAL;
    case RFGM_LITE:
        return COLLIDE_RFG_LITE;
    case RFGM_ULTRA_LITE:
        return COLLIDE_RFG_ULTRA_LITE;
    case RFGM_WORLD_ONLY:
        return COLLIDE_RFG_WORLD_ONLY;
    default:
        return COLLIDE_NO_COLLISION;
    }
}

// 0x00715380 (:7385)
// Removable movers (0x4): normal ones at level 1, lighter ones at level 5; others never.
unsigned char rfg_dyn_get_auto_remove_level_by_move_type(int mt, int mover_flags)
{
    if ((mover_flags & 4) != 0)
    {
        switch (mt)
        {
        case RFGM_NORMAL:
            return 1;
        case RFGM_LITE:
        case RFGM_ULTRA_LITE:
        case RFGM_WORLD_ONLY:
        case RFGM_NO_COLLISION:
            return 5;
        default:
            break;
        }
    }
    return 0;
}

// 0x007153C0 (:7412)
// The candidate list is locked by swapping its count with -1.
void rfg_dyn_add_to_process_shardify_list(rfg_mover* rmp)
{
    if (rmp == nullptr || rmp->process_shardify_candidate_index != 0xFFFFFFFF)
    {
        return;
    }
    unsigned int count;
    do
    {
        count = _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, -1);
    } while (count == 0xFFFFFFFF);
    rmp->process_shardify_candidate_index = count;
    Rfg_mover_process_shardify_candidate_list[count] = rmp;
    _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, count + 1);
}

// 0x00715400 (:7430)
void rfg_dyn_remove_from_process_shardify_list(rfg_mover* rmp)
{
    if (rmp == nullptr || rmp->process_shardify_candidate_index == 0xFFFFFFFF)
    {
        return;
    }
    unsigned int count;
    do
    {
        count = _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, -1);
    } while (count == 0xFFFFFFFF);
    const unsigned int index = rmp->process_shardify_candidate_index;
    if (index < count - 1)
    {
        rfg_mover* last = Rfg_mover_process_shardify_candidate_list[count - 1];
        Rfg_mover_process_shardify_candidate_list[index] = last;
        last->process_shardify_candidate_index = index;
    }
    rmp->process_shardify_candidate_index = 0xFFFFFFFF;
    _InterlockedExchange((volatile long*)&Rfg_mover_process_shardify_candidate_count, count - 1);
}

// 0x0071CE10 (:7460)
rfg_destruction_event::rfg_destruction_event()
{
    flags.in_use = 0;
    last_modified_timestamp.invalidate();
    request = nullptr;
    memset(&flags, 0, sizeof(flags));
}

// 0x00715460 (:7469)
void rfg_destruction_event::reset()
{
    last_modified_timestamp.invalidate();
    request = nullptr;
    memset(&flags, 0, sizeof(flags));
}

// 0x00719780
rfg_destruction_trail::rfg_destruction_trail()
{
    // timestamp() invalidates lifetime
}

// ---- rfg_collision_event_base accessors (rfg_dynamics.h; the data share one union)
// 0x007110A0 (.h:205)
unsigned short rfg_collision_event_base::get_direct_sps(int idx) const
{
    return direct_sps[idx];
}

// 0x007110B0 (.h:210)
void rfg_collision_event_base::set_direct_sps(int idx, unsigned short val)
{
    direct_sps[idx] = val;
}

// 0x007110D0 (.h:216)
const comp_sp_combined_direct_damage& rfg_collision_event_base::get_combined_direct_damage(int idx) const
{
    return combined_direct_damage[idx];
}

// 0x007110E0 (.h:221)
void rfg_collision_event_base::set_combined_direct_damage(int idx, unsigned short compressed_damage, unsigned short compressed_shape_key)
{
    combined_direct_damage[idx].compressed_damage = compressed_damage;
    combined_direct_damage[idx].compressed_shape_key = compressed_shape_key;
}

// 0x00711110 (.h:228)
unsigned int rfg_collision_event_base::get_link_index(int idx) const
{
    return link_indices[idx];
}

// 0x0094CFE0 (.h:233)
void rfg_collision_event_base::set_link_index(int idx, unsigned int val)
{
    link_indices[idx] = val;
}

// 0x0094D000 (.h:238)
const unsigned int* rfg_collision_event_base::get_link_indices() const
{
    return link_indices;
}

// 0x00711120 (.h:242)
void rfg_collision_event_base::set_link_indices(unsigned int num_indices, const unsigned int* indices)
{
    memcpy(&link_indices[data_count], indices, num_indices * sizeof(unsigned int));
    data_count += (unsigned char)num_indices;
}

// 0x00711160 (.h:249)
void rfg_collision_event_base::set_slice_index(unsigned int slice_index)
{
    link_indices[0] = slice_index;
}

// 0x00711170 (.h:254)
unsigned int rfg_collision_event_base::get_slice_index() const
{
    return link_indices[0];
}

// 0x00711180 (.h:260)
const unsigned int* rfg_collision_event_base::get_invulnerable_mover_handles() const
{
    return invulnerable_mover_handles;
}

// 0x00711190 (.h:264)
void rfg_collision_event_base::add_invulnerable_mover_handle(unsigned int handle)
{
    invulnerable_mover_handles[data_count] = handle;
    data_count++;
}
