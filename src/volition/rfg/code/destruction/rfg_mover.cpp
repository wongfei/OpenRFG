// Original: volition/rfg/code/destruction/rfg_mover.cpp
//
// rfg_mover, a destructible building (decompiled; header comment in rfg_mover.h). Compiled in rfg_destruction_unity.cpp.
//
// Streaming: stream(true) creates the rfg_destroyable_shape from the chunk's base destroyable (rfg_shape_new), restores
// the saved shape state (decompress_state: which pieces still exist, world anchors and dynamic links), builds the
// destroyable model instance (build_new_rl_instance_from_shape) and adds a Havok body (add_to_world); stream(false)
// saves the state again (compress_state) and frees everything. A mover split off another one (shardify) receives its
// shape through RFGM_PROP_NEW_SHAPE and splits its render instance off its creator's later (create_deferred_ri, from
// the pre-stream render task).
//
// Damage: apply_damage turns bullets, melee and level objects into direct damage or shatter events for the destruction
// dynamics (rfg_dynamics.cpp); do_frame_report recomputes the damage percent from the structural mass and reports
// game-destroyed / fully-destroyed transitions (activities, HUD, gameflow, wrecking crew).
//
// rfg_subpiece::flags, from the decompiled uses: 0x1 world anchor, 0x2 dynamic link, 0x1000 always damageable,
// 0x2000/0x4000 pending create/delete in transfer_child_obj_list, 0x8000 nano (deleted with the nano material effect),
// 0x10000 fading in (mover fader), 0x20000 jittered, 0x80000 regrow visited, 0x400000 script point effect host,
// 0x10000000 queued for jitter, 0x40000000 queued for transfer, 0x80000000 no fade on delete.
// rfg_dlod::flags: 0x2 the dlod's whole-mesh sub instance is claimed by this mover's instance, 0x8 whole (no piece
// fading, see rfg_dlod::should_render), 0x10 recheck whole, 0x20 the whole-mesh sub instance is shown instead of the
// pieces. rfg_destroyable_shape::flags: 0x1 links changed, 0x4 fast, 0x10/0x20 anchored to the world / by a dynamic
// link, 0x40 re-run the stress test, 0x100 queued for the low priority stress test, 0x400 freshly streamed.
//
// Partial:
// - unused_dcf_rfg_dynamic_link_dump_info and unused_dcf_regrow_test (console commands) are not recovered.
// - Damage sounds and the collapse sound are queued for NPCs (npc_sound_info), whose processing comes with the AI.
//
// Notes (decompiled behaviour, kept as is):
// - maybe_post_damage_sound compares percent_destroyed() (0..1) against 10, so only damage_amt >= 10 posts a sound.
// - rfg_mover_regrow_begin requires percent_destroyed() >= REGROW_MIN_PERCENT_DESTROYED (2.0), which a damage percent
//   (0..1) never reaches, so movers never regrow on stream unless the value is tweaked.
// - create_deferred_ri resets creator_handle before passing it as the donor handle, so faders of the transferred pieces
//   are looked up with owner 0xFFFFFFFF.
// - rfg_mover_regrow_setup restores a link when the visited piece's own shape state is set, not the neighbour's.
// - rfg_mover_regrow_begin clears rfg_subpiece_base_extra_data::h of anchored pieces in the shared base data.
// - attach_index_is_valid accepts index == num_objects.
// - set_move_type through find_anchor_points and add_to_world keeps the move type when the Havok handle is invalid.
// - get_tag_local_coords returns false without touching the outputs when the chunk is missing.
// - script_point_destroy dereferences the shape without checking it.

#include "volition/rfg/code/destruction/rfg_mover.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/code_effects/glass.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_rbb_tree.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/activity_tanks_a_lot.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_damage_report.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/custom_object.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/lod_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/movers/mover_fader.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/util/color.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

#include <stdio.h>
#include <string.h>

static const int RFG_MOVER_POOL_SIZE = 4000; // rfg_moverpool
static const unsigned int RFG_MOVER_POOL_USAGE = 0x1685E0; // memory footprint registered with the instantiator
static const unsigned int RFG_MOVER_STATE_BUFFER_SIZE = 0x74000; // Rfg_mover_state_buffer
static const unsigned int RFG_MOVER_STATE_PAGE_SIZE = 0x10;
static const float RFG_MOVER_DEFAULT_GAME_DESTROYED_PCT = 0.3f; // when the gameplay properties say -1
static const float RFG_MOVER_DEFAULT_FULLY_DESTROYED_PCT = 0.5f; // when the gameplay properties say -1
static const unsigned int RFG_MOVER_STREAM_OUT_RESTORE_MS = 300000; // 5 minutes of play time before a restore
static const int RFG_MOVER_DAMAGE_SOUND_DELAY_MS = 5000;
static const float RFG_MOVER_DAMAGE_SOUND_MIN = 10.0f;
static const float RFG_MOVER_BUILDING_MIN_SIZE = 3.0f; // metres on each axis
static const float RFG_MOVER_BUILDING_MIN_MASS = 2000.0f;
static const float RFG_MOVER_PLAYER_ONLY_DIST_SQR = 400.0f; // 20 m
static const float LINK_STRENGTH_SCALE = 1.0f / 1024.0f; // dynamic link yields are fixed point (x1024)
static const float LINK_MIN_OVERLAP_AREA = 0.01f;
static const int NUM_STATE_DATA_TYPES = rfg_mover_temporary_data::NUM_STATE_DATA_TYPES;
// MAX_SCRIPT_POINT_EFFECT_INSTS: rfg_dynamics.cpp (earlier in rfg_destruction_unity.cpp)

// ---- file-static globals
static unsigned char Default_collapse_mat_index = 0xFF; // 0x016503EC, the "Concrete" effect material
static bool Building_smoke_plumes = true; // 0x016503ED
static float Bullet_shatter_damage_scalar = 7.5f; // 0x016503F0
static float Bullet_damage_scalar = 200.0f; // 0x016503F4
static float Melee_damage_scalar = 100.0f; // 0x016503F8
static float Level_object_damage_scalar = 100.0f; // 0x016503FC
static float Melee_jitter_radius = 1.5f; // 0x01650400
static float Shard_delete_fade_time_min = 0.3f; // 0x01650404, seconds
static float Shard_delete_fade_time_max = 0.6f; // 0x01650408
float REGROW_MIN_PERCENT_DESTROYED = 2.0f; // 0x0165040C, see the notes
static bool Rfg_mover_damage_applied; // 0x01E38321
static unsigned char Rfg_mover_state_buffer[RFG_MOVER_STATE_BUFFER_SIZE]; // 0x01E84230
static rfg_mover* Pre_stream_mover_list; // 0x01EF82D4
static game_render_task Pre_stream_render_task_mover; // 0x01F13354
object_pool<rfg_dynamic_link, 2400, 2> RFG_dynamic_link_pool; // 0x01F1C900 (0x01069C00), category 1 live, 0 pending
static page_mempool Rfg_mover_state_pool; // 0x020C1B60 (0x01069C20)
static object_pool<explosion_event, 20, 1> Explosion_event_pool; // 0x01F14100 (0x01069C40)
static object_pool<damaged_subpiece_event, 256, 1> Damaged_sp_event_pool; // 0x01F13400 (0x01069C60)
static object_pool<repair_event, 32, 1> Repair_event_pool; // 0x01F13D90 (0x01069C80)
static object_pool<rfg_mover, RFG_MOVER_POOL_SIZE, 1> rfg_moverpool; // 0x01F59580 (0x01069CA0)
static obj_class_instantiator rfg_mover_instantiator("rfg_mover", alloc_rfg_mover, types_rfg_mover, bpo_index_rfg_mover, classname_rfg_mover, rfg_mover::pool_remaining, RFG_MOVER_POOL_USAGE, RFG_MOVER_POOL_SIZE); // 0x01F140FC (0x01069CC0)

// The thread's message buffer (TLS +0x38 in rfg.exe): the stream failures are formatted into it and never shown.
static __declspec(thread) char Rfg_mover_message[1024];

// ---- internal functions
static void rfg_mover_pre_stream_process_all(void*, rl_renderer*);

// 0x00719830
explosion_event::explosion_event()
{
}

// 0x00719840
repair_event::repair_event()
{
}

// 0x00716580 (:338)
// LTCG: rmp in EAX.
static void rfg_mover_stream_fail_mp_cleanup(rfg_mover* rmp)
{
    if (Multiplayer)
    {
        rmp->remote_obj_flags.client_destroy_permission = true;
        if (rmp->mp_info != nullptr)
        {
            rmp->mp_info->destroy_causes.stream_fail = true;
        }
    }
}

// 0x007165A0 (:348)
// LTCG: rm_p in EAX.
static void rfg_mover_add_to_pre_stream_list(rfg_mover* rm_p)
{
    if (rm_p->pre_stream_next != nullptr && rm_p->pre_stream_prev != nullptr)
    {
        return; // already listed
    }
    if (Pre_stream_mover_list == nullptr)
    {
        rm_p->pre_stream_next = rm_p;
        rm_p->pre_stream_prev = rm_p;
        Pre_stream_mover_list = rm_p;
        return;
    }
    rm_p->pre_stream_prev = Pre_stream_mover_list->pre_stream_prev;
    rm_p->pre_stream_next = Pre_stream_mover_list;
    Pre_stream_mover_list->pre_stream_prev->pre_stream_next = rm_p;
    Pre_stream_mover_list->pre_stream_prev = rm_p;
}

// 0x00716600 (:357)
static hkpMotion::MotionType havok_motion_type_from_move_type(rfg_move_type mt)
{
    return mt == RFGM_FIXED ? hkpMotion::MOTION_FIXED : hkpMotion::MOTION_DYNAMIC;
}

// 0x00716620 (:362)
// LTCG: mt in EAX.
static hkpCollidableQualityType collidable_quality_from_move_type(rfg_move_type mt)
{
    return mt != RFGM_FIXED ? HK_COLLIDABLE_QUALITY_DEBRIS : HK_COLLIDABLE_QUALITY_FIXED;
}

// The local bounding box of a render instance from a shape (rl_scene_entity::set_local_aabb, inlined).
static void set_instance_bounds(rl_destroyable_model_instance* ri, const vector& bmin, const vector& bmax)
{
    ri->set_local_aabb(rl_aabb(bmin, bmax));
}

// 0x0073AEB0 (:377)
// LTCG: shape in EDI. Splits the sub instances of shape's pieces (and of the dlods the shape owns) off the donor's
// instance into a new instance: dlods whose pieces are all whole show their whole-mesh sub instance, the others their
// pieces. Faders of the transferred pieces are destroyed. Returns nullptr when the shape has no pieces or the split
// fails; the donor then gets its sub instances back.
static rl_destroyable_model_instance* build_new_rl_instance_from_shapes(rfg_destroyable_shape* shape, rfg_destroyable_shape* donor_shape, rl_destroyable_model_instance* donor_ri, unsigned int donor_handle)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rfg_base_destroyable_shape* base = donor_shape->parent;
    const int max_indices = (base->num_dlods + base->num_objects) * 2;
    rl_split_info split;
    split.m_sub_insts_to_split = (short*)pool->alloc(max_indices, 0);
    split.m_num_sub_insts_to_split = 0;
    split.m_sub_insts_to_disable = (short*)pool->alloc(max_indices, 0);
    split.m_num_sub_insts_to_disable = 0;
    split.m_sub_insts_to_enable = (short*)pool->alloc(max_indices, 0);
    split.m_num_sub_insts_to_enable = 0;
    unsigned char* dlod_used = (unsigned char*)pool->alloc(base->num_dlods, 0);
    memset(dlod_used, 0, base->num_dlods);
    int num_unclaim = 0;
    short* unclaim = (short*)pool->alloc(max_indices, 0);

    // the pieces of both piece lists move to the new instance
    const unsigned short lists[2] = {shape->child_obj_list, shape->transfer_child_obj_list};
    for (int l = 0; l < 2; l++)
    {
        if (lists[l] == 0xFFFF)
        {
            continue;
        }
        for (rfg_subpiece* sp = &shape->get_object_buffer()[lists[l]]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
        {
            const unsigned int index = shape->get_subpiece_index(sp);
            split.m_sub_insts_to_split[split.m_num_sub_insts_to_split++] = (short)index;
            const unsigned int dlod_index = shape->parent->base_objects.to_pointer()[index].dlod_key & 0xFFFF;
            dlod_used[dlod_index] = 1;
            if ((sp->flags & 0x10000) != 0)
            {
                mover_fader_destroy(donor_handle, index);
                sp->flags &= ~0x10000u;
                shape->get_dlod(dlod_index)->flags |= 0x10;
            }
            if (sp->next == lists[l])
            {
                break;
            }
        }
    }
    if (split.m_num_sub_insts_to_split == 0)
    {
        return nullptr;
    }

    for (int d = 0; d < base->num_dlods; d++)
    {
        if (!dlod_used[d])
        {
            continue;
        }
        rfg_dlod* dlod = shape->get_dlod(d);
        const short dlod_sub_inst = (short)(base->num_objects + d);
        if (dlod->get_owner() == shape)
        {
            split.m_sub_insts_to_split[split.m_num_sub_insts_to_split++] = dlod_sub_inst;
            if (dlod->should_render())
            {
                if ((dlod->flags & 0x20) == 0)
                {
                    split.m_sub_insts_to_enable[split.m_num_sub_insts_to_enable++] = dlod_sub_inst;
                    dlod->get_subpiece_indices(split.m_num_sub_insts_to_disable, split.m_sub_insts_to_disable);
                    dlod->flags |= 0x20;
                    continue;
                }
            }
            if (!dlod->should_render() && (dlod->flags & 0x20) != 0)
            {
                split.m_sub_insts_to_disable[split.m_num_sub_insts_to_disable++] = dlod_sub_inst;
                dlod->get_subpiece_indices(split.m_num_sub_insts_to_enable, split.m_sub_insts_to_enable);
                dlod->flags &= ~0x20;
            }
        }
        else
        {
            if ((dlod->flags & 2) != 0)
            {
                unclaim[num_unclaim++] = dlod_sub_inst;
                dlod->flags &= ~2;
            }
            if ((dlod->flags & 0x20) != 0)
            {
                split.m_sub_insts_to_disable[split.m_num_sub_insts_to_disable++] = dlod_sub_inst;
                dlod->get_subpiece_indices(split.m_num_sub_insts_to_enable, split.m_sub_insts_to_enable);
                dlod->flags &= ~0x20;
            }
        }
    }

    rl_destroyable_model_instance* new_ri = donor_ri->split(split);
    donor_ri->unclaim_sub_instances(unclaim, num_unclaim);
    if (new_ri == nullptr)
    {
        // give the donor its sub instances back
        split.m_num_sub_insts_to_disable = 0;
        split.m_num_sub_insts_to_enable = 0;
        for (int d = 0; d < base->num_dlods; d++)
        {
            if (!dlod_used[d])
            {
                continue;
            }
            rfg_dlod* dlod = shape->get_dlod(d);
            if (dlod->get_owner() == shape)
            {
                if ((dlod->flags & 0x20) != 0)
                {
                    split.m_sub_insts_to_disable[split.m_num_sub_insts_to_disable++] = (short)(base->num_objects + d);
                    dlod->get_subpiece_indices(split.m_num_sub_insts_to_enable, split.m_sub_insts_to_enable);
                }
                dlod->flags &= ~0x22;
            }
        }
        donor_ri->set_sub_instance_is_active(split.m_sub_insts_to_disable, split.m_num_sub_insts_to_disable, false, true);
        donor_ri->set_sub_instance_is_active(split.m_sub_insts_to_enable, split.m_num_sub_insts_to_enable, true, true);
        donor_ri->unclaim_sub_instances(split.m_sub_insts_to_split, split.m_num_sub_insts_to_split);
        return nullptr;
    }
    new_ri->set_material_map(material_effects_get_original_material_map(donor_ri));
    set_instance_bounds(donor_ri, donor_shape->local_bmin, donor_shape->local_bmax); // inlined
    set_instance_bounds(new_ri, shape->local_bmin, shape->local_bmax); // inlined
    return new_ri;
}

// 0x0073B750 (:581)
// LTCG: shape in EAX. The destroyable model instance of a streamed shape. A shape with siblings (split off the same
// base, sib_next) and no full instance request shares the sibling mover's instance data (init from the sibling: the
// pieces in child_obj_list plus the dlods the shape owns; owned dlods show their whole-mesh sub instance). Otherwise
// one sub instance per piece and per dlod is built from the base data and the chunk's mesh: pieces of owned dlods
// start hidden, dlods with a shatterable piece (base flag 0x40) skip deferred rendering, and without create_whole_mover
// only the existing pieces and owned dlods are claimed. Returns nullptr when the instance cannot be created.
static rl_destroyable_model_instance* build_new_rl_instance_from_shape(rfg_destroyable_shape* shape, c_chunk_base* base_chunk, bool create_whole_mover)
{
    rfg_base_destroyable_shape* base = shape->parent;
    const int max_sub_insts = base->num_dlods + base->num_objects;
    rl_destroyable_model_instance* ri = rl_destroyable_model_instance::create();
    if (ri == nullptr)
    {
        return nullptr;
    }
    const unsigned int mark = Common_mempool.mark();
    if (!create_whole_mover && shape->sib_next != shape)
    {
        rfg_mover* sibling = shape->sib_next->get_mover();
        if (sibling == nullptr)
        {
            ri->release();
            ri = nullptr;
        }
        else
        {
            rl_destroyable_model_instance* sibling_ri = sibling->rdd.get_data()->model_instance_p; // not checked
            short* indices = (short*)Common_mempool.alloc(max_sub_insts * 2, 0);
            short* enable = (short*)Common_mempool.alloc(max_sub_insts * 2, 0);
            short* disable = (short*)Common_mempool.alloc(max_sub_insts * 2, 0);
            unsigned char* dlod_used = (unsigned char*)Common_mempool.alloc(base->num_dlods, 0);
            memset(dlod_used, 0, base->num_dlods);
            int num_indices = 0;
            int num_enable = 0;
            int num_disable = 0;
            if (shape->child_obj_list != 0xFFFF)
            {
                for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
                {
                    const unsigned int index = shape->get_subpiece_index(sp);
                    indices[num_indices++] = (short)index;
                    const unsigned int dlod_index = base->base_objects.to_pointer()[index].dlod_key & 0xFFFF;
                    dlod_used[dlod_index] = 1;
                    sp->flags &= ~0x30000u;
                    shape->get_dlod(dlod_index)->flags |= 0x10;
                    if (sp->next == shape->child_obj_list)
                    {
                        break;
                    }
                }
            }
            for (int d = 0; d < base->num_dlods; d++)
            {
                if (!dlod_used[d])
                {
                    continue;
                }
                rfg_dlod* dlod = shape->get_dlod(d);
                const short dlod_sub_inst = (short)(base->num_objects + d);
                if (dlod->get_owner() == shape)
                {
                    indices[num_indices++] = dlod_sub_inst;
                    if ((dlod->flags & 0x20) == 0)
                    {
                        enable[num_enable++] = dlod_sub_inst;
                        dlod->get_subpiece_indices(num_disable, disable);
                        dlod->flags |= 0x20;
                    }
                    dlod->flags |= 2;
                }
            }
            ri->init(sibling_ri, indices, num_indices);
            ri->set_sub_instance_is_active(enable, num_enable, true, true);
            ri->set_sub_instance_is_active(disable, num_disable, false, false);
        }
        Common_mempool.restore_to_mark(mark);
        return ri;
    }

    // the kd tree of this base destroyable (the last match)
    rl_fp_kdtree_base_data* kdtree = nullptr;
    if (base_chunk->base_destroyable_kdtrees.to_pointer() != nullptr)
    {
        for (int i = 0; i < base_chunk->num_base_destroyables; i++)
        {
            if (base_chunk->base_destroyables.to_pointer()[i].to_pointer() == base)
            {
                kdtree = base_chunk->base_destroyable_kdtrees.to_pointer()[i].to_pointer();
            }
        }
    }
    rl_sub_instance_info* infos = (rl_sub_instance_info*)Common_mempool.alloc(max_sub_insts * sizeof(rl_sub_instance_info), 0);
    int num_infos = 0;
    for (; num_infos < base->num_objects; num_infos++)
    {
        rfg_subpiece_base* sp_base = &base->base_objects.to_pointer()[num_infos];
        rl_sub_instance_info& info = infos[num_infos];
        matrix orient;
        base->get_subpiece_orient_local(sp_base, orient); // inlined
        info.m_is_active = shape->get_dlod(sp_base->dlod_key & 0xFFFF)->get_owner() != shape;
        info.m_disable_deferred = false;
        info.m_submesh_id = base->base_extra_data.to_pointer()[num_infos].render_subpiece;
        info.m_transform.set_orientation(orient);
        info.m_transform.set_translation(sp_base->pos);
    }
    for (int d = 0; d < base->num_dlods; d++, num_infos++)
    {
        rfg_dlod* dlod = shape->get_dlod(d);
        rl_sub_instance_info& info = infos[num_infos];
        if (dlod->get_owner() == shape)
        {
            info.m_is_active = true;
            dlod->flags |= 0x20;
            dlod->flags |= 2;
        }
        else
        {
            info.m_is_active = false;
            dlod->flags &= ~0x20;
            dlod->flags &= ~2;
        }
        const rfg_dlod_base* dlod_base = &base->base_dlods.to_pointer()[d];
        bool shatterable = false;
        const int first = dlod_base->first_piece;
        const int last = first + dlod_base->max_pieces;
        for (int k = first; k < last; k++)
        {
            if (k < base->num_objects && (base->base_objects.to_pointer()[k].flags & 0x40) != 0)
            {
                shatterable = true;
                break;
            }
        }
        info.m_disable_deferred = shatterable;
        info.m_submesh_id = dlod_base->render_subpiece;
        info.m_transform.set_orientation(dlod_base->orient);
        info.m_transform.set_translation(dlod_base->pos);
    }

    bool ok;
    if (!create_whole_mover)
    {
        short* indices = (short*)Common_mempool.alloc(num_infos * 2, 0);
        unsigned char* dlod_used = (unsigned char*)Common_mempool.alloc(base->num_dlods, 0);
        memset(dlod_used, 0, base->num_dlods);
        int num_indices = 0;
        if (shape->child_obj_list != 0xFFFF)
        {
            for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
            {
                const unsigned int index = shape->get_subpiece_index(sp);
                indices[num_indices++] = (short)index;
                const unsigned int dlod_index = base->base_objects.to_pointer()[index].dlod_key & 0xFFFF;
                rfg_dlod* dlod = shape->get_dlod(dlod_index);
                if (!dlod_used[dlod_index])
                {
                    if (dlod->get_owner() == shape)
                    {
                        indices[num_indices++] = (short)(base->num_objects + dlod_index);
                        dlod->flags |= 0x22;
                    }
                    else
                    {
                        dlod->flags &= ~0x22;
                    }
                    dlod_used[dlod_index] = 1;
                }
                if (sp->next == shape->child_obj_list)
                {
                    break;
                }
            }
        }
        ok = ri->init(base_chunk->mesh_p.to_pointer(), base_chunk->material_map_p.to_pointer(), kdtree, infos, num_infos, indices, num_indices);
    }
    else
    {
        ok = ri->init(base_chunk->mesh_p.to_pointer(), base_chunk->material_map_p.to_pointer(), kdtree, infos, num_infos, nullptr, 0);
    }
    if (!ok)
    {
        ri->release();
        Common_mempool.restore_to_mark(mark);
        return nullptr;
    }
    Common_mempool.restore_to_mark(mark);
    return ri;
}

// 0x00734970 (:847)
// LTCG: shape in EDI. One step of the regrow flood fill: the linked neighbours of sp not visited yet (0x80000) that
// are made of a regrowing material (physical flag 0x800). Neighbours still in the shape get their link restored;
// detached ones fade back in with regrow_chance percent (unless deleted, 0x2000, or owned by another shape). Both are
// queued in child_list (a circular list through shard_next/shard_prev) for the next step.
static void rfg_mover_regrow_setup(rfg_destroyable_shape* shape, rfg_subpiece* sp, unsigned int regrow_chance, unsigned short& child_list)
{
    const unsigned int index = shape->get_subpiece_index(sp);
    const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[index];
    const unsigned short* links = sp->links.to_pointer();
    for (int i = 0; i < sp_base->num_links; i++)
    {
        const rfg_link_base* link_base = shape->get_link_base(links[i]);
        int other = link_base->obj[0];
        if (other == (int)index)
        {
            other = link_base->obj[1];
        }
        rfg_subpiece* osp = &shape->get_object_buffer()[other];
        const unsigned int flags = osp->flags;
        if ((flags & 0x80000) != 0)
        {
            continue;
        }
        osp->flags = flags | 0x80000;
        const rfg_physical_material* material = rfg_get_physical_material_by_index(shape->parent->base_objects.to_pointer()[other].physical_material_index); // inlined
        if ((material->flags & 0x800) == 0)
        {
            continue;
        }
        if (osp->get_destroyable() == shape)
        {
            if (shape->get_shape_state(sp)) // see the notes
            {
                shape->restore_link(shape->get_link(links[i]));
            }
        }
        else
        {
            if ((flags & 0x2000) != 0 || osp->get_destroyable() != nullptr || regrow_chance < vrand() % 100)
            {
                continue;
            }
            shape->fade_subpiece(osp, true);
        }
        const unsigned short osp_index = (unsigned short)shape->get_subpiece_index(osp);
        if (child_list == 0xFFFF)
        {
            osp->shard_next = osp_index;
            osp->shard_prev = osp_index;
            child_list = osp_index;
        }
        else
        {
            osp->shard_prev = shape->get_object_buffer()[child_list].shard_prev;
            osp->shard_next = child_list;
            shape->get_object_buffer()[shape->get_object_buffer()[child_list].shard_prev].shard_next = osp_index;
            shape->get_object_buffer()[child_list].shard_prev = osp_index;
        }
    }
}

// 0x00734C00 (:905)
// Clears the visited marks of the flood fill; deleted pieces (0x2000) fade in, and the walk continues through the
// neighbours that left the shape.
static void rfg_mover_regrow_cleanup(rfg_destroyable_shape* shape, rfg_subpiece* sp)
{
    const unsigned int flags = sp->flags;
    if ((flags & 0x80000) == 0)
    {
        return;
    }
    sp->flags = flags & ~0x80000u;
    rfg_destroyable_shape* owner = sp->get_destroyable();
    if ((flags & 0x2000) != 0)
    {
        shape->fade_subpiece(sp, true);
    }
    else if (owner != shape)
    {
        return;
    }
    const unsigned int index = shape->get_subpiece_index(sp);
    const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[index];
    const unsigned short* links = sp->links.to_pointer();
    for (int i = 0; i < sp_base->num_links; i++)
    {
        const rfg_link_base* link_base = shape->get_link_base(links[i]);
        int other = link_base->obj[0];
        if (other == (int)index)
        {
            other = link_base->obj[1];
        }
        rfg_subpiece* osp = &shape->get_object_buffer()[other];
        if (osp->get_destroyable() != shape)
        {
            rfg_mover_regrow_cleanup(shape, osp);
        }
    }
}

// 0x0072E3A0 (:935)
// LTCG: rmp in EDI. Starts the script point effects of the level object properties on an intact building: each
// effect whose host tag names a script point of the chunk, on a dlod "<sub piece name>_parent" with all its pieces,
// gets a script_point_effect_inst on the first piece of that dlod (flag 0x400000) and its effect (night-only effects,
// flag 0x80, wait for the night).
static void rfg_mover_setup_script_point_inst(rfg_mover* rmp)
{
    if (!rmp->rdd.is_loaded())
    {
        return;
    }
    rfg_destroyable_shape* shape = rmp->rdd.get_data()->shape;
    if (shape == nullptr)
    {
        return;
    }
    c_chunk_base* chunk = chunk_get_base_by_uid(rmp->chunk_uid);
    if (chunk == nullptr)
    {
        return;
    }
    short* indices = (short*)_alloca(shape->parent->num_objects * sizeof(short)); // sized for any dlod
    for (int i = 0; i < MAX_SCRIPT_POINT_EFFECT_INSTS; i++)
    {
        const script_point_effect_info* info = rmp->m_level_obj_properties->m_base_properties->m_script_point_effect_infos[i];
        if (info == nullptr)
        {
            continue;
        }
        const int host_tag = rmp->get_tag_index_from_name(info->script_point_name);
        if (host_tag == -1)
        {
            continue;
        }
        char parent_name[104];
        sprintf(parent_name, "%s_parent", chunk->script_points.to_pointer()[host_tag].sub_piece_name);
        const unsigned int dlod_index = shape->get_dlod_index_from_name(parent_name);
        if (dlod_index == 0xFFFFFFFF)
        {
            continue;
        }
        rfg_dlod* dlod = shape->get_dlod(dlod_index);
        if (dlod->active_pieces != dlod->max_pieces)
        {
            continue;
        }
        int num_indices = 0;
        dlod->get_subpiece_indices(num_indices, indices);
        if (num_indices == 0)
        {
            continue;
        }
        const unsigned int shape_key = (unsigned int)indices[0];
        script_point_effect_inst* inst = lopm_new_script_point_effect_inst();
        rmp->m_level_obj_properties->m_script_point_effect_insts[i] = inst;
        if (inst == nullptr)
        {
            return;
        }
        inst->script_point_idx = host_tag;
        inst->shape_key = shape_key;
        if (info->custom_effect != 0xFFFFFFFF && ((info->spe_flags & 0x80) == 0 || game_clock_get_time_of_day() == GC_NIGHT)) // 0x80: night only
        {
            inst->current_effect = effects_play(info->custom_effect, rmp->get_handle(), host_tag, true, 0xFFFFFFFF, 1.0f, false);
            object_effect* effect = World.object_from_handle<object_effect>(inst->current_effect);
            if (effect != nullptr)
            {
                effect->obj_flags.destroy_on_stream = false;
            }
        }
        inst->hitpoints = info->max_hitpoints;
        inst->state = SPES_NORMAL;
        inst->host_obj = rmp;
        inst->host_idx = i;
        shape->get_object_buffer()[shape_key].flags |= 0x400000;
    }
}

// ---- subpiece_collector
// 0x00716630 (:1041)
subpiece_collector::subpiece_collector()
{
    new_anchor_list = nullptr;
}

// 0x00716650
subpiece_collector::~subpiece_collector()
{
}

// 0x00740B10 (:1050)
// Anchor candidates from create_anchors: the root bodies of both sides; when neither rfg_mover is movable (mover_flags
// 0xC), the piece of the first rfg_mover found is anchored to the other side (the world when it is not an rfg_mover).
void subpiece_collector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
    rfg_mover* rfgm_a = World.object_cast<rfg_mover>(object_from_world_obj((const hkpWorldObject*)bodyA.getRootCollidable()->getOwner())); // inlined
    rfg_mover* rfgm_b = World.object_cast<rfg_mover>(object_from_world_obj((const hkpWorldObject*)bodyB.getRootCollidable()->getOwner())); // inlined
    if ((rfgm_a != nullptr && (rfgm_a->mover_flags & 0xC) != 0) || (rfgm_b != nullptr && (rfgm_b->mover_flags & 0xC) != 0))
    {
        return;
    }
    const short key_a = (short)bodyA.getShapeKey();
    const short key_b = (short)bodyB.getShapeKey();
    if (rfgm_a != nullptr)
    {
        set_as_anchor(rfgm_a, key_a, rfgm_b, key_b);
    }
    else if (rfgm_b != nullptr)
    {
        set_as_anchor(rfgm_b, key_b, nullptr, key_a);
    }
}

// Appends a mover to the collector's new anchor list (through the stress links).
static void anchor_list_append(rfg_mover*& list, rfg_mover* rm)
{
    if (list == nullptr)
    {
        rm->stress_next = rm;
        rm->stress_prev = rm;
        list = rm;
        return;
    }
    rm->stress_prev = list->stress_prev;
    rm->stress_next = list;
    list->stress_prev->stress_next = rm;
    list->stress_prev = rm;
}

// 0x0073D540 (:1082)
// Anchors a piece to the world (other_rfgm == nullptr; not for pieces with base flags 0x44 or already anchored) or
// links it to another mover's piece. Movers that were not anchored before stop their stress test and are queued in
// new_anchor_list (find_anchor_points freezes them). A piece both world-anchored and dynamically linked to an
// anchored piece loses the dynamic link.
void subpiece_collector::set_as_anchor(rfg_mover* rfgm, short subpiece_index, rfg_mover* other_rfgm, short other_subpiece_index)
{
    rfg_destroyable_shape* shape = rfgm->get_shape();
    if (shape == nullptr)
    {
        return;
    }
    rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_index];
    const bool was_anchored = (shape->flags & 0x30) != 0;
    bool other_was_anchored = true;
    if (other_rfgm == nullptr)
    {
        if ((shape->parent->base_objects.to_pointer()[subpiece_index].flags & 0x44) != 0 || (sp->flags & 1) != 0)
        {
            return;
        }
        shape->create_world_anchor(rfgm, sp);
    }
    else
    {
        rfg_destroyable_shape* other_shape = other_rfgm->get_shape();
        if (other_shape == nullptr)
        {
            return;
        }
        other_was_anchored = (other_shape->flags & 0x30) != 0;
        if (!rfg_mover_create_dynamic_link(rfgm, subpiece_index, other_rfgm, other_subpiece_index))
        {
            return;
        }
    }
    if (!was_anchored)
    {
        stress_evaluation_stop(rfgm);
        if (rfgm->stress_next != nullptr || rfgm->stress_prev != nullptr)
        {
            return;
        }
        anchor_list_append(new_anchor_list, rfgm);
    }
    if (!other_was_anchored)
    {
        stress_evaluation_stop(other_rfgm);
        if (other_rfgm == nullptr || other_rfgm->stress_next != nullptr || other_rfgm->stress_prev != nullptr)
        {
            return;
        }
        anchor_list_append(new_anchor_list, other_rfgm);
    }
    object_mover* linked_mover;
    rfg_subpiece* linked_sp = nullptr;
    rfg_subpiece_base* linked_base = nullptr;
    if ((sp->flags & 1) != 0 && rfg_dyn_link_get_linked_data(rfgm, sp, linked_mover, linked_sp, linked_base) && linked_sp != nullptr && (linked_sp->flags & 1) != 0)
    {
        rfg_mover_delete_dynamic_link(rfgm, sp, false);
    }
}

// ---- dynamic links
// RFG_dynamic_link_pool: category 1 holds the live links, category 0 the ones waiting for their movers (decompress
// before the other side streamed in, or saved after a delete with the yield negated, 0x80000000).

// 0x0071E700 (:1137)
rfg_dynamic_link* rfg_mover_get_dynamic_link(rfg_subpiece* sp)
{
    if (sp == nullptr || sp->dynamic_link_index == 0xFFFF)
    {
        return nullptr;
    }
    return RFG_dynamic_link_pool.get_entry(sp->dynamic_link_index);
}

// 0x00725C50 (:1143)
// The first link (any category) with movep on either side.
rfg_dynamic_link* rfg_mover_get_dynamic_link(object_mover* movep)
{
    if (movep == nullptr)
    {
        return nullptr;
    }
    const unsigned int handle = movep->get_handle();
    for (int category = 0; category < 2; category++)
    {
        csspinlock_scope lock(RFG_dynamic_link_pool.thread_access);
        for (rfg_dynamic_link* rdl = RFG_dynamic_link_pool.get_first(category); rdl != nullptr; rdl = RFG_dynamic_link_pool.get_next(rdl))
        {
            if (rdl->mover_handle[0] == handle || rdl->mover_handle[1] == handle)
            {
                return rdl;
            }
        }
    }
    return nullptr;
}

// 0x00725D10 (:1164)
// The live link of a mover's piece.
static rfg_dynamic_link* rfg_mover_get_dynamic_link(unsigned int handle, unsigned short shape_key)
{
    csspinlock_scope lock(RFG_dynamic_link_pool.thread_access);
    for (rfg_dynamic_link* rdl = RFG_dynamic_link_pool.get_first(1); rdl != nullptr; rdl = RFG_dynamic_link_pool.get_next(rdl))
    {
        if ((rdl->mover_handle[0] == handle && rdl->shape_key[0] == shape_key) || (rdl->mover_handle[1] == handle && rdl->shape_key[1] == shape_key))
        {
            return rdl;
        }
    }
    return nullptr;
}

// 0x0072E5A0 (:1178)
// Activates a link once both movers are streamed (the second may be a general mover): when both pieces still exist it
// becomes live (category 1), flags the pieces (0x2) and shapes (0x20), turns a saved yield (negative) back into its
// strength or computes the strength from the two materials and the overlap area (yield_limit holds the area x1024),
// and propagates the anchor depths. Otherwise it is freed and the shapes are flagged for a link update (0x1).
static void rfg_mover_process_dynamic_link(rfg_dynamic_link* rdl)
{
    rfg_mover* rm0 = World.object_from_handle<rfg_mover>(rdl->mover_handle[0]);
    object_mover* other = World.object_from_handle<object_mover>(rdl->mover_handle[1]);
    if (rm0 == nullptr || other == nullptr)
    {
        RFG_dynamic_link_pool.free(rdl);
        return;
    }
    rfg_mover* rm1 = World.object_cast<rfg_mover>(other);
    general_mover* gm1 = World.object_cast<general_mover>(other);
    rfg_destroyable_shape* shape0 = rm0->get_shape();
    rfg_destroyable_shape* shape1 = rm1 != nullptr ? rm1->get_shape() : nullptr;
    if (shape0 == nullptr || (shape1 == nullptr && gm1 == nullptr))
    {
        return;
    }
    RFG_dynamic_link_pool.move(rdl, 0);
    rfg_subpiece* sp0 = &shape0->get_object_buffer()[rdl->shape_key[0]];
    rfg_subpiece* sp1 = shape1 != nullptr ? &shape1->get_object_buffer()[rdl->shape_key[1]] : nullptr;
    rfg_subpiece_base* sp_base0 = shape0->get_subpiece_base(rdl->shape_key[0]);
    rfg_subpiece_base* sp_base1 = nullptr;
    if (!shape0->get_shape_state(sp0) || (shape1 != nullptr && !shape1->get_shape_state(sp1)))
    {
        RFG_dynamic_link_pool.free(rdl);
        shape0->flags |= 1;
        if (shape1 != nullptr)
        {
            shape1->flags |= 1;
        }
        return;
    }
    const unsigned short link_index = (unsigned short)RFG_dynamic_link_pool.get_index(rdl);
    sp0->flags |= 2;
    shape0->flags |= 0x20;
    sp0->dynamic_link_index = link_index;
    if (shape1 == nullptr)
    {
        if (gm1 != nullptr)
        {
            gm1->mover_flags |= 0x8000000;
            gm1->gm_flags &= ~8u; // not dislodged while linked
        }
    }
    else
    {
        sp_base1 = shape1->get_subpiece_base(rdl->shape_key[1]);
        sp1->flags |= 2;
        shape1->flags |= 0x20;
        sp1->dynamic_link_index = link_index;
    }
    unsigned int yield;
    if (rdl->yield_limit < 0)
    {
        yield = rdl->yield_limit & 0x7FFFFFFF;
    }
    else
    {
        const unsigned char material1 = sp_base1 != nullptr ? sp_base1->physical_material_index : other->get_physical_material(rdl->shape_key[1], nullptr, nullptr);
        yield = rfg_shape_calc_link_strength(sp_base0->physical_material_index, sp_base0, material1, sp_base1, (float)rdl->yield_limit * LINK_STRENGTH_SCALE);
    }
    rdl->yield_limit = (int)yield;
    rfg_dyn_link_update_depth(rm0, shape0, rdl->anchor_depth[0]);
    rfg_dyn_link_update_depth(rm1, shape1, rdl->anchor_depth[1]);
}

// 0x0072E850 (:1253)
// LTCG: dynamic_link in ESI.
static bool rfg_mover_create_dynamic_link(const rfg_dynamic_link& dynamic_link)
{
    rfg_dynamic_link* rdl = RFG_dynamic_link_pool.alloc(1);
    if (rdl == nullptr)
    {
        return false;
    }
    *rdl = dynamic_link;
    rfg_mover_process_dynamic_link(rdl);
    return true;
}

// 0x0073BE40 (:1273)
// Links a piece of rm to a piece of other_mp (an rfg_mover) or to other_mp itself (a general mover) when neither
// piece is marked unlinkable (base flags 0xC0000000), at least one side is anchored and the two pieces are not both
// world anchors. Movers already constrained to each other, pieces linked to a third mover, and links already present
// are rejected. The strength is the overlap area of the two boxes (x1024, at least 0.01); a stronger existing link on
// either piece is kept, a weaker one replaced. The anchor depth of each side is at most the other's plus one.
bool rfg_mover_create_dynamic_link(rfg_mover* rm, unsigned short subpiece_index, object_mover* other_mp, unsigned short other_subpiece_index)
{
    rfg_destroyable_shape* shape = rm->get_shape();
    rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_index];
    rfg_subpiece_base* sp_base = shape->get_subpiece_base(subpiece_index);
    rfg_mover* other_rm = World.object_cast<rfg_mover>(other_mp);
    rfg_destroyable_shape* other_shape = nullptr;
    rfg_subpiece* other_sp = nullptr;
    rfg_subpiece_base* other_base = nullptr;
    if (other_rm != nullptr)
    {
        other_shape = other_rm->get_shape();
        other_sp = &other_shape->get_object_buffer()[other_subpiece_index];
        other_base = other_shape->get_subpiece_base(other_subpiece_index);
    }
    if ((sp_base->flags & 0xC0) != 0 || (other_base != nullptr && (other_base->flags & 0xC0) != 0))
    {
        return false;
    }
    if ((shape->flags & 0x30) == 0 && (other_shape == nullptr || (other_shape->flags & 0x30) == 0))
    {
        return false;
    }
    if (other_sp != nullptr && (other_sp->flags & 1) != 0 && (sp->flags & 1) != 0)
    {
        return false;
    }
    const unsigned int other_handle = other_mp->get_handle();
    constraint* c = rm->child_constraint_ptr;
    while (c != nullptr)
    {
        if (c->host_handle == other_handle)
        {
            return false;
        }
        c = c->child_constraint_next;
        if (c == rm->child_constraint_ptr)
        {
            break;
        }
    }
    c = rm->host_constraint_ptr;
    while (c != nullptr)
    {
        if (c->child_handle == other_handle)
        {
            return false;
        }
        c = c->host_constraint_next;
        if (c == rm->host_constraint_ptr)
        {
            break;
        }
    }
    object_mover* linked_mover;
    rfg_subpiece* linked_sp;
    rfg_subpiece_base* linked_base;
    if (rfg_dyn_link_get_linked_data(rm, sp, linked_mover, linked_sp, linked_base) && linked_mover != other_mp)
    {
        return false;
    }
    if (rfg_dyn_link_get_linked_data(other_rm, other_sp, linked_mover, linked_sp, linked_base) && linked_mover != rm)
    {
        return false;
    }
    vector bmin, bmax;
    bbox_instance(bmin, bmax, rm->orient, rm->pos, sp_base->prel_bmin, sp_base->prel_bmax);
    vector other_bmin, other_bmax;
    if (other_rm == nullptr)
    {
        other_mp->get_bbox(other_bmin, other_bmax);
    }
    else
    {
        bbox_instance(other_bmin, other_bmax, other_rm->orient, other_rm->pos, other_base->prel_bmin, other_base->prel_bmax);
    }
    const vector center((bmax.x + bmin.x) * 0.5f, (bmax.y + bmin.y) * 0.5f, (bmax.z + bmin.z) * 0.5f);
    const vector other_center((other_bmax.x + other_bmin.x) * 0.5f, (other_bmax.y + other_bmin.y) * 0.5f, (other_bmax.z + other_bmin.z) * 0.5f);
    float area = rfg_shape_calc_overlap_area(center, bmin, bmax, other_center, other_bmin, other_bmax);
    if (LINK_MIN_OVERLAP_AREA - area >= 0.0f)
    {
        area = LINK_MIN_OVERLAP_AREA;
    }
    const unsigned int strength = (unsigned int)(int)(area * 1024.0f); // x1024, see LINK_STRENGTH_SCALE
    rfg_dynamic_link* existing = rfg_mover_get_dynamic_link(sp);
    rfg_dynamic_link* other_existing = other_sp == nullptr ? rfg_mover_get_dynamic_link(other_mp) : rfg_mover_get_dynamic_link(other_sp);
    if (existing == nullptr && rfg_mover_get_dynamic_link(rm->get_handle(), subpiece_index) != nullptr)
    {
        return false;
    }
    if (other_existing == nullptr && rfg_mover_get_dynamic_link(other_mp->get_handle(), other_subpiece_index) != nullptr)
    {
        return false;
    }
    const int existing_yield = existing != nullptr ? existing->yield_limit : 0;
    const int other_existing_yield = other_existing != nullptr ? other_existing->yield_limit : 0;
    if (strength <= (unsigned int)(other_existing_yield + existing_yield))
    {
        return false;
    }
    if (existing != nullptr)
    {
        rfg_mover_delete_dynamic_link(rm, sp, false);
    }
    if (other_existing != nullptr)
    {
        if (other_rm == nullptr)
        {
            rfg_mover_delete_dynamic_link(other_mp);
        }
        else
        {
            rfg_mover_delete_dynamic_link(other_rm, other_sp, false);
        }
    }
    unsigned short depth = shape->anchor_depth;
    unsigned short other_depth = other_shape != nullptr ? other_shape->anchor_depth : 0xFFFF;
    if (depth != 0xFFFF && (unsigned short)(depth + 1) <= other_depth)
    {
        other_depth = depth + 1;
    }
    if (other_depth != 0xFFFF && (unsigned short)(other_depth + 1) <= depth)
    {
        depth = other_depth + 1;
    }
    rfg_dynamic_link* rdl = RFG_dynamic_link_pool.alloc(1);
    if (rdl == nullptr)
    {
        return false;
    }
    rdl->shape_key[0] = subpiece_index;
    rdl->shape_key[1] = other_subpiece_index;
    rdl->anchor_depth[0] = depth;
    rdl->anchor_depth[1] = other_depth;
    rdl->mover_handle[0] = rm->get_handle();
    rdl->mover_handle[1] = other_handle;
    rdl->yield_limit = (int)strength;
    rfg_mover_process_dynamic_link(rdl);
    return true;
}

// 0x00734D40 (:1402)
// Deletes the first link of a general mover (from the rfg_mover's side).
void rfg_mover_delete_dynamic_link(object_mover* movep)
{
    rfg_dynamic_link* rdl = rfg_mover_get_dynamic_link(movep);
    if (rdl == nullptr)
    {
        return;
    }
    rfg_mover* rm = World.object_from_handle<rfg_mover>(rdl->mover_handle[0]);
    if (rm == nullptr || !rm->rdd.is_loaded())
    {
        return;
    }
    rfg_destroyable_shape* shape = rm->rdd.get_data()->shape;
    if (shape != nullptr)
    {
        rfg_mover_delete_dynamic_link(rm, &shape->get_object_buffer()[rdl->shape_key[0]], false);
    }
}

// 0x0072E890 (:1416)
// Frees every live link of a mover (when it is freed).
void rfg_mover_delete_dynamic_link(unsigned int handle)
{
    csspinlock_scope lock(RFG_dynamic_link_pool.thread_access);
    rfg_dynamic_link* rdl = RFG_dynamic_link_pool.get_first(1);
    while (rdl != nullptr)
    {
        rfg_dynamic_link* next = RFG_dynamic_link_pool.get_next(rdl);
        if (rdl->mover_handle[0] == handle || rdl->mover_handle[1] == handle)
        {
            RFG_dynamic_link_pool.free(rdl);
        }
        rdl = next;
    }
}

// 0x0072E940 (:1432)
// Removes a piece's link. While the mover is alive (not destroyed, not forced) a link that should be saved is
// kept as pending (category 0, yield negated) with both pieces unflagged; otherwise the link is freed, both shapes are
// queued for shardify with their links changed (0x1), and a linked general mover becomes movable (gm_flags 0x8, only
// once streamed) and stops its link-dependent custom effects. Forced or destroyed deletes clear pristine.
void rfg_mover_delete_dynamic_link(rfg_mover* rm, rfg_subpiece* sp, bool force_delete)
{
    rfg_dynamic_link* rdl = rfg_mover_get_dynamic_link(sp);
    if (rdl == nullptr)
    {
        return;
    }
    const bool destroyed = rm->obj_flags.destroyed != 0;
    object_mover* linked_mover;
    rfg_subpiece* linked_sp;
    rfg_subpiece_base* linked_base;
    if (!force_delete && !destroyed && !rdl->should_save(rm->get_handle()))
    {
        if (rfg_dyn_link_get_linked_data(rm, sp, linked_mover, linked_sp, linked_base))
        {
            if (linked_mover->get_sub_type() == OT_SUB_MOVER_GENERAL)
            {
                World.object_cast<general_mover>(linked_mover)->mover_flags &= ~0x8000000u;
            }
            else if (linked_mover->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                linked_sp->flags &= ~2u;
                linked_sp->dynamic_link_index = 0xFFFF;
            }
        }
        sp->flags &= ~2u;
        sp->dynamic_link_index = 0xFFFF;
        rdl->yield_limit |= 0x80000000;
        RFG_dynamic_link_pool.move(rdl, 1);
        return;
    }
    rdl->yield_limit = 0;
    rfg_destroyable_shape* shape = rm->get_shape();
    if (shape == nullptr)
    {
        return;
    }
    shape->flags |= 1;
    rfg_dyn_add_to_process_shardify_list(rm);
    if (force_delete || destroyed)
    {
        rm->flags.pristine = 0;
    }
    if (rfg_dyn_link_get_linked_data(rm, sp, linked_mover, linked_sp, linked_base))
    {
        if (linked_mover->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            if (linked_mover->obj_flags.streamed)
            {
                general_mover* gm = World.object_cast<general_mover>(linked_mover);
                gm->mover_flags &= ~0x8000000u;
                gm->gm_flags |= 8;
            }
            if ((linked_mover->mover_flags & 0x1000000) != 0 && linked_mover->m_level_obj_properties->m_base_properties->m_flags.m_link_affects_effects)
            {
                custom_object_stop_custom_effects(linked_mover);
            }
        }
        else if (linked_mover->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* other = World.object_cast<rfg_mover>(linked_mover);
            rfg_destroyable_shape* other_shape = other->get_shape();
            if (other_shape != nullptr)
            {
                other_shape->flags |= 1;
                rfg_dyn_add_to_process_shardify_list(other);
                if (force_delete || destroyed)
                {
                    other->flags.pristine = 0;
                }
                linked_sp->dynamic_link_index = 0xFFFF;
            }
        }
    }
    RFG_dynamic_link_pool.free(rdl);
    sp->dynamic_link_index = 0xFFFF;
}

// ---- rfg_mover (header inlines)
// 0x005DBEB0 (.h:131)
const object_type rfg_mover::get_type_static()
{
    return OT_MOVER;
}

// 0x005DBEC0 (.h:131)
const object_sub_type rfg_mover::get_subtype_static()
{
    return OT_SUB_MOVER_RFG;
}

// 0x00711080 (.h:131)
const char* rfg_mover::get_classname_static()
{
    return "rfg_mover";
}

// 0x005DBED0 (.h:131)
int rfg_mover::get_bpo_index_static()
{
    return 0;
}

// 0x0071E7A0 (.h:131)
const char* rfg_mover::get_classname() const
{
    return "rfg_mover";
}

// 0x005DBEE0 (.h:269)
rfg_move_type rfg_mover::get_move_type() const
{
    return move_type;
}

// 0x005FB400 (.h:352)
rfg_destroyable_shape* rfg_mover::get_shape() const
{
    return rdd.is_loaded() ? rdd.get_data()->shape : nullptr;
}

// 0x00711090 (.h:357)
bool rfg_mover::is_movable() const
{
    return move_type != RFGM_FIXED;
}

// ---- rfg_mover class registration (:1593)
// 0x00716670
void types_rfg_mover(signed char& type, signed char& subtype)
{
    type = OT_MOVER;
    subtype = OT_SUB_MOVER_RFG;
}

// 0x007166A0
unsigned int rfg_mover::get_classname_hash()
{
    return rfg_mover_instantiator.this_hash;
}

// 0x0072EB60
void rfg_mover::pool_free()
{
    rfg_moverpool.free(this);
}

// 0x00725DD0
object* alloc_rfg_mover()
{
    return rfg_moverpool.alloc(0);
}

// 0x00716690
const char* classname_rfg_mover()
{
    return "rfg_mover";
}

// 0x0071E740
int rfg_mover::pool_remaining()
{
    return rfg_moverpool.get_free();
}

// 0x00716680
int bpo_index_rfg_mover()
{
    return 0;
}

// ---- rfg_mover
// 0x0071E750 (:1594)
rfg_mover::rfg_mover()
{
}

// 0x0071E7B0
rfg_mover::~rfg_mover()
{
}

// 0x0071E7E0 (:1599)
// The current structural mass of this mover and the children that inherit its damage percent; the base masses are
// added to base_structural_mass.
void rfg_mover::calculate_structural_mass(float& base_structural_mass)
{
    current_structural_mass = 0.0f;
    rfg_destroyable_shape* shape = get_shape();
    if (shape != nullptr)
    {
        current_structural_mass = shape->structural_mass + 0.0f;
        base_structural_mass = shape->base_structural_mass + base_structural_mass;
    }
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)child)->flags.inherit_damage_pct)
        {
            rfg_mover* rm = (rfg_mover*)child;
            rm->calculate_structural_mass(base_structural_mass);
            current_structural_mass = rm->current_structural_mass + current_structural_mass;
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            break;
        }
    }
}

// 0x0071E8A0 (:1623)
// damage_pct = 1 - current / base structural mass (clamped to [0, 1]) at the top of an inherit_damage_pct hierarchy;
// the children that inherit take the parent's value.
void rfg_mover::update_damage_percent()
{
    rfg_mover* parent_mover = World.object_from_handle<rfg_mover>(get_parent());
    if (parent_mover == nullptr || !flags.inherit_damage_pct)
    {
        float base_mass = 0.0f;
        calculate_structural_mass(base_mass);
        if (0.0f < base_mass)
        {
            float pct = 1.0f - current_structural_mass / base_mass;
            damage_pct = pct;
            if (pct - 0.0f < 0.0f)
            {
                pct = 0.0f;
            }
            if (0.0f <= pct - 1.0f)
            {
                pct = 1.0f;
            }
            damage_pct = pct;
        }
    }
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)child)->flags.inherit_damage_pct)
        {
            rfg_mover* rm = (rfg_mover*)child;
            rm->damage_pct = damage_pct;
            rm->update_damage_percent();
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            break;
        }
    }
}

// 0x00716730 (:1653)
bool rfg_mover::alloc_state_data()
{
    if (state_data == nullptr)
    {
        if (Rfg_mover_state_pool.can_alloc(sizeof(rfg_mover_temporary_data), 0))
        {
            state_data = (rfg_mover_temporary_data*)Rfg_mover_state_pool.alloc(sizeof(rfg_mover_temporary_data), 0);
        }
        if (state_data != nullptr)
        {
            for (int i = 0; i < NUM_STATE_DATA_TYPES; i++)
            {
                state_data->data_buf[i] = nullptr;
                state_data->data_size[i] = 0;
            }
        }
    }
    return state_data != nullptr;
}

// 0x00716790 (:1676)
// Frees the state data once all its buffers are gone.
bool rfg_mover::dealloc_state_data()
{
    if (state_data == nullptr)
    {
        return false;
    }
    for (int i = 0; i < NUM_STATE_DATA_TYPES; i++)
    {
        if (state_data->data_buf[i] != nullptr)
        {
            return false;
        }
    }
    Rfg_mover_state_pool.release_bytes(state_data);
    state_data = nullptr;
    return true;
}

// 0x007167D0 (:1698)
// Copies data_buf (or zeroes) into a new buffer of the type; an existing buffer is kept as is.
void rfg_mover::alloc_data_buf(rfg_mover_temporary_data::data_type type, unsigned char* data_buf, int buf_size)
{
    if (!alloc_state_data() || state_data->data_buf[type] != nullptr)
    {
        return;
    }
    if (Rfg_mover_state_pool.can_alloc(buf_size, 0))
    {
        state_data->data_buf[type] = (unsigned char*)Rfg_mover_state_pool.alloc(buf_size, 0);
    }
    if (state_data->data_buf[type] == nullptr)
    {
        return;
    }
    if (data_buf != nullptr)
    {
        memcpy(state_data->data_buf[type], data_buf, buf_size);
    }
    else
    {
        memset(state_data->data_buf[type], 0, buf_size);
    }
    state_data->data_size[type] = (unsigned short)buf_size;
}

// 0x00716870 (:1730)
void rfg_mover::dealloc_data_buf(rfg_mover_temporary_data::data_type type)
{
    if (state_data == nullptr || state_data->data_buf[type] == nullptr)
    {
        return;
    }
    Rfg_mover_state_pool.release_bytes(state_data->data_buf[type]);
    state_data->data_buf[type] = nullptr;
    state_data->data_size[type] = 0;
    dealloc_state_data();
}

// 0x007168C0 (:1743)
unsigned char* rfg_mover::get_data_buf(rfg_mover_temporary_data::data_type type)
{
    return state_data != nullptr ? state_data->data_buf[type] : nullptr;
}

// 0x007168E0 (:1750)
unsigned int rfg_mover::get_data_buf_size(rfg_mover_temporary_data::data_type type)
{
    return state_data != nullptr ? state_data->data_size[type] : 0;
}

// 0x00725DE0 (:1764)
// Load: move type, rfg mover flags and damage percent (saved state), the chunk flags of a level mover (collapse
// effect 0x10, inherit damage percent 0x80, regrow 0x40, invulnerable 0x100, smoke plume on death 0x200, one of many
// 0x400, mining 0x800, supply crate 0x1000, propaganda 0x8000, building 0x80000), salvage and destroyed percentages,
// a damage report for important EDF and guerrilla-held structures, and the saved world anchors, dynamic links, shape
// state and decals; a mover split off another one takes its destruction uid and new shape instead. Movers lighter
// than RFGM_LITE are not streaming dependent. Save: the compressed state, the flags (run_stress_on_stream set while
// a stress test is pending), the move type and the percentages; EDF buildings that were damaged remember the play
// time they streamed out at, for the restore check (maybe_flag_restore).
bool rfg_mover::serialize(obj_prop_block& pblock, bool load)
{
    if (!object_mover::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        damage_pct = 0.0f;
        current_structural_mass = -1.0f; // not computed yet (do_frame_report)
        game_destroyed_pct = gameplay_props->game_destroyed_pct;
        if (game_destroyed_pct == -1.0f)
        {
            game_destroyed_pct = RFG_MOVER_DEFAULT_GAME_DESTROYED_PCT;
        }
        repair_event_list = 0xFFFF;
        mp_info = nullptr;
        explosion_event_index = 0xFFFF;
        damaged_subpiece_event_first_index = 0xFFFF;
        stress_test_delay.invalidate();
        stress_test_count = 0;
        slice_delay.invalidate();
        stress_prev = nullptr;
        stress_next = nullptr;
        pre_stream_prev = nullptr;
        pre_stream_next = nullptr;
        salvage_num_pieces = gameplay_props->salvage_num_pieces;
        destruction_uid = 0xFFFFFFFF;
        damage_report_handle = -1;
        stream_out_time = 0;
        stress_sound_propagate_timer.set(0);
        accumulated_load_amount = 0.0f;
        damage_sound_propagate_timer.set(0);
        state_data = nullptr;
        m_glass_obj_list = nullptr;
        restricted_area = 0xFFFFFFFF;
        dead_smoke_plume_effect = 0xFFFFFFFF;
        process_shardify_candidate_index = 0xFFFFFFFF;
        rdd.set_loaded(true);
        rdd.get_data()->shape = nullptr;
        rdd.get_data()->model_instance_p = nullptr;
        rdd.set_loaded(false);
        remote_obj_flags.client_sync = true;
        if (checking_reset < 1)
        {
            reference_position_index = object_mover_get_reference_pos_index(pos);
        }
        obj_prop* prop;
        if (pblock.get_internal(prop, RFGM_PROP_MOVE_TYPE, 5) && prop->size == sizeof(rfg_move_type))
        {
            move_type = *(rfg_move_type*)prop->data;
        }
        if (checking_reset < 1 && move_type > RFGM_LITE)
        {
            set_streaming_dependent(false);
        }
        memset(&flags, 0, sizeof(flags));
        if ((mover_flags & 0x4) == 0)
        {
            flags.pristine = 1;
        }
        if (pblock.get_internal(prop, RFGM_PROP_FLAGS, 5) && prop->size == sizeof(rfg_mover_flags))
        {
            flags = *(rfg_mover_flags*)prop->data;
        }
        if (pblock.get_internal(prop, RFGM_PROP_DAMAGE_PERCENT, 5) && prop->size == sizeof(float))
        {
            damage_pct = *(float*)prop->data;
        }
        if ((mover_flags & 0x4) == 0)
        {
            int chunk_flags = 0;
            pblock.get_str_as_flags_optional(OM_PROP_CHUNK_FLAGS, chunk_flags, Chunk_flags_table);
            chunk_flags |= gameplay_props->chunk_flags;
            if ((chunk_flags & 0x10) != 0)
            {
                flags.collapse_effect = 1;
            }
            if ((chunk_flags & 0x80) != 0)
            {
                flags.inherit_damage_pct = 1;
            }
            if ((chunk_flags & 0x40) != 0) // regrow
            {
                mover_flags |= 0x1;
            }
            if ((chunk_flags & 0x100) != 0)
            {
                flags.invulnerable = 1;
            }
            if ((chunk_flags & 0x200) != 0)
            {
                flags.plume_on_death = 1;
            }
            if ((chunk_flags & 0x400) != 0)
            {
                flags.one_of_many = 1;
            }
            if ((chunk_flags & 0x800) != 0)
            {
                flags.mining = 1;
            }
            if ((chunk_flags & 0x1000) != 0)
            {
                flags.supply_crate = 1;
            }
            if ((chunk_flags & 0x8000) != 0)
            {
                flags.propaganda = 1;
            }
            if ((chunk_flags & 0x80000) != 0)
            {
                flags.building = 1;
            }
            pblock.get_optional(RFGM_PROP_SALVAGE_NUM_PIECES, salvage_num_pieces);
            pblock.get_optional(RFGM_PROP_GAME_DESTROYED_PCT, game_destroyed_pct);
            if (checking_reset < 1 && (int)team >= 0 && team != HUMAN_TEAM_CIVILIAN && (int)team < 4 && get_importance_level() > OBJECT_IMPORTANCE_LEVEL_NONE)
            {
                damage_report_handle = sdc_create_damage_tracking_report(this, get_importance_level(), team);
            }
            pblock.get_optional(RFGM_PROP_STREAM_OUT_PLAY_TIME, stream_out_time);
            if (checking_reset > 0)
            {
                return true;
            }
            unsigned char* buf;
            const int anchors_size = pblock.get_buf_optional(RFGM_PROP_WORLD_ANCHORS, buf);
            if (anchors_size < 1)
            {
                mover_flags &= ~0x4000000u;
            }
            else
            {
                alloc_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR, buf, anchors_size);
                mover_flags |= 0x4000000;
            }
            const int links_size = pblock.get_buf_optional(RFGM_PROP_DYNAMIC_LINKS, buf);
            if (links_size > 0)
            {
                alloc_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK, buf, links_size);
                mover_flags |= 0x8000000;
            }
        }
        else
        {
            pblock.get_optional(RFGM_PROP_DESTRUCT_UID, destruction_uid);
            if (checking_reset > 0)
            {
                return true;
            }
            rdd.set_loaded(true); // the shape pointer is read into the resource data
            rdd.get_data()->shape = nullptr;
            pblock.get_optional(RFGM_PROP_NEW_SHAPE, rdd.get_data()->shape);
            rdd.set_loaded(false);
        }
        if (checking_reset > 0)
        {
            return true;
        }
        unsigned char* buf;
        const int state_size = pblock.get_buf_optional(RFGM_PROP_SHAPE_STATE, buf);
        if (state_size > 0)
        {
            alloc_data_buf(rfg_mover_temporary_data::SHAPE_STATE, buf, state_size);
        }
        const int decals_size = pblock.get_buf_optional(OM_PROP_DECAL_LIST, buf);
        if (decals_size > 0)
        {
            alloc_data_buf(rfg_mover_temporary_data::DECAL_LIST, buf, decals_size);
        }
        if (Multiplayer)
        {
            multi_game_object_rfg_mover_created(this);
        }
        return true;
    }

    compress_state(get_shape(), 0xDEADBEEF); // no checksum
    if (state_data != nullptr && state_data->data_buf[rfg_mover_temporary_data::WORLD_ANCHOR] != nullptr)
    {
        pblock.set_buf(RFGM_PROP_WORLD_ANCHORS, state_data->data_buf[rfg_mover_temporary_data::WORLD_ANCHOR], state_data->data_size[rfg_mover_temporary_data::WORLD_ANCHOR]);
    }
    if (state_data != nullptr && state_data->data_buf[rfg_mover_temporary_data::DYNAMIC_LINK] != nullptr)
    {
        pblock.set_buf(RFGM_PROP_DYNAMIC_LINKS, state_data->data_buf[rfg_mover_temporary_data::DYNAMIC_LINK], state_data->data_size[rfg_mover_temporary_data::DYNAMIC_LINK]);
    }
    if (state_data != nullptr && state_data->data_buf[rfg_mover_temporary_data::SHAPE_STATE] != nullptr)
    {
        pblock.set_buf(RFGM_PROP_SHAPE_STATE, state_data->data_buf[rfg_mover_temporary_data::SHAPE_STATE], state_data->data_size[rfg_mover_temporary_data::SHAPE_STATE]);
    }
    if (state_data != nullptr && state_data->data_buf[rfg_mover_temporary_data::DECAL_LIST] != nullptr)
    {
        pblock.set_buf(OM_PROP_DECAL_LIST, state_data->data_buf[rfg_mover_temporary_data::DECAL_LIST], state_data->data_size[rfg_mover_temporary_data::DECAL_LIST]);
    }
    if (obj_flags.streamed)
    {
        dealloc_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR);
        dealloc_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK);
        dealloc_data_buf(rfg_mover_temporary_data::SHAPE_STATE);
    }
    flags.run_stress_on_stream = stress_evaluation_is_active(this);
    pblock.set(RFGM_PROP_FLAGS, flags);
    pblock.set(RFGM_PROP_MOVE_TYPE, move_type);
    if (0.001f < damage_pct)
    {
        pblock.set(RFGM_PROP_DAMAGE_PERCENT, damage_pct);
    }
    pblock.set(RFGM_PROP_SALVAGE_NUM_PIECES, salvage_num_pieces);
    if (0.01f < fabsf(game_destroyed_pct - RFG_MOVER_DEFAULT_GAME_DESTROYED_PCT))
    {
        pblock.set(RFGM_PROP_GAME_DESTROYED_PCT, game_destroyed_pct);
    }
    if (0 < stream_out_time)
    {
        pblock.set(RFGM_PROP_STREAM_OUT_PLAY_TIME, stream_out_time);
        return true;
    }
    if (is_killed() || flags.pristine || team != HUMAN_TEAM_EDF)
    {
        return true;
    }
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        player* local = player_get_local_player();
        const int play_time = (local == nullptr || !local->is_killed()) ? meta->get_play_time() : 1; // dead: restore on respawn
        pblock.set(RFGM_PROP_STREAM_OUT_PLAY_TIME, play_time);
    }
    return true;
}

// 0x00716900 (:2108)
// Movers outside their original zone are not saved; untouched fixed movers are restored from the level; zone movers
// (handle byte 2 == 0xFF) are dropped once almost destroyed when movable, or when a movable one hosts a script point
// effect.
obj_serialize_mode rfg_mover::serialize_mode()
{
    if (!World.object_physically_in_original_zone(this))
    {
        return OSM_NONE;
    }
    if ((mover_flags & 0x20000) == 0)
    {
        if (flags.pristine && move_type == RFGM_FIXED)
        {
            return OSM_RESTORE;
        }
        if (((get_handle() >> 16) & 0xFF) == 0xFF)
        {
            if (move_type != RFGM_FIXED && 0.999f < damage_pct)
            {
                return OSM_NONE;
            }
            if ((mover_flags & 0x4) != 0 && has_script_point_effect(0xFFFFFFFF, nullptr))
            {
                return OSM_NONE;
            }
        }
    }
    return OSM_FULL;
}

// 0x00740BF0 (:2178)
// See the file header. Load also skips movers whose chunk or shape cannot be created (destroyed, multiplayer stream
// fail) or whose base has no mass, queues a regrow for regrowing level movers (not in multiplayer or wrecking crew),
// sets up script point effects of level movers, posts the stream-in event and restarts a stress test saved as
// pending. Unload releases the glass, stops the smoke plume, saves the state and stops a running stress test
// (remembered in run_stress_on_stream unless destroyed), and releases the body, the instance, the shape and the
// pending events.
bool rfg_mover::stream(bool load)
{
    if (!object_mover::stream(load))
    {
        return false;
    }
    if (load)
    {
        c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
        if (chunk == nullptr)
        {
            sprintf(Rfg_mover_message, "*** Base chunk failed to load ***");
            rfg_mover_stream_fail_mp_cleanup(this); // inlined
            World.destroy_object(this, false);
            return false;
        }
        rdd.set_loaded(true);
        resource_dependent_data* data = rdd.get_data();
        rfg_destroyable_shape* shape = data->shape;
        if (shape == nullptr)
        {
            rfg_base_destroyable_shape* base = chunk_get_destroyable_object(chunk, chunk_obj_uid, this);
            if (base == nullptr)
            {
                rfg_mover_stream_fail_mp_cleanup(this);
                World.destroy_object(this, false);
                return false;
            }
            shape = rfg_shape_new(base, chunk_uid, pos, orient, shape_uid);
            data->shape = shape;
            if (shape == nullptr)
            {
                sprintf(Rfg_mover_message, "*** Warning: Failed to create destroyable shape: %s <%.1f, %.1f, %.1f> ***\n", chunk->name, pos.x, pos.y, pos.z);
                rfg_mover_stream_fail_mp_cleanup(this);
                World.destroy_object(this, false);
                return false;
            }
            shape->flags |= 0x400;
            if (!decompress_state(shape, chunk->destruction_checksum))
            {
                sprintf(Rfg_mover_message, "*** Warning: Failed to decompress destroyable shape: %s <%.1f, %.1f, %.1f> ***\n", chunk->name, pos.x, pos.y, pos.z);
                rfg_mover_stream_fail_mp_cleanup(this);
                World.destroy_object(this, false);
                rfg_shape_delete(shape);
                data->shape = nullptr;
                return false;
            }
            shape->recalc_shape_properties();
            shape->recalc_rbb_tree();
            set_instance_bounds(data->model_instance_p, shape->local_bmin, shape->local_bmax); // inlined, not checked
        }
        if (shape->get_object_buffer() == nullptr)
        {
            sprintf(Rfg_mover_message, "*** Warning: Destroyable shape is invalid: %s <%.1f, %.1f, %.1f> ***\n", chunk->name, pos.x, pos.y, pos.z);
            rfg_mover_stream_fail_mp_cleanup(this);
            World.destroy_object(this, false);
            shape->delete_me();
            shape->removeReference();
            data->shape = nullptr;
            return false;
        }
        if (data->model_instance_p != nullptr)
        {
            data->model_instance_p->set_casts_shadows((mover_flags & 0x10) != 0);
            data->model_instance_p->set_casts_drop_shadows((mover_flags & 0x4000) != 0);
            if (gameplay_props != nullptr && gameplay_props->lod_properties != nullptr)
            {
                data->model_instance_p->set_lod_off_distance(gameplay_props->lod_properties->m_lod_off_dist); // inlined
            }
        }
        shape->set_mover(this);
        shape_uid = shape->shape_uid;
        bbox_instance(last_known_bmin, last_known_bmax, orient, pos, shape->local_bmin, shape->local_bmax);
        if (rfg_dyn_is_shard_large(last_known_bmin, last_known_bmax))
        {
            mover_flags |= 0x2;
        }
        if ((mover_flags & 0x1) != 0 && !Multiplayer && !wrecking_crew_is_active())
        {
            flags.regrow_on_stream_begin = 1;
        }
        const unsigned int old_mover_flags = mover_flags;
        update_pos_and_orient(pos, orient, false);
        if ((old_mover_flags & 0x40000) == 0)
        {
            mover_flags &= ~0x40000u; // keep the movement flag only when it was set before
        }
        if (shape->mass <= 0.0f)
        {
            sprintf(Rfg_mover_message, "*** Warning: Invalid rfg_mover - base chunk has 0 mass: %s (%s) ***\n", chunk->filename, chunk_get_object_name(chunk_uid, chunk_obj_uid));
            World.destroy_object(this, false);
            rfg_shape_delete(shape);
            return false;
        }
        add_to_world(move_type);
        rfgl_object_created(this);
        if (move_type != RFGM_FIXED)
        {
            nav_cell_detour_mover_created(this);
        }
        if (data->model_instance_p != nullptr)
        {
            add_decals();
        }
        if ((mover_flags & 0x4) == 0)
        {
            rfg_mover_setup_script_point_inst(this);
        }
        mover_stream_in_event stream_event; // EVENT_MOVER_STREAM_IN
        stream_event.handle = get_handle();
        gem_post_event(stream_event);
        maybe_flag_restore();
        if (flags.run_stress_on_stream)
        {
            flags.run_stress_on_stream = 0;
            stress_evaluation_start(this, false, false); // inlined
        }
    }
    else
    {
        glass_obj* glass = m_glass_obj_list;
        while (glass != nullptr)
        {
            glass_obj_release(glass);
            glass = glass->mover_next;
            if (glass == m_glass_obj_list)
            {
                break;
            }
        }
        if (!is_killed() && !flags.pristine && team == HUMAN_TEAM_EDF)
        {
            player_meta_data* meta = player_get_meta_data();
            if (meta != nullptr)
            {
                stream_out_time = meta->get_play_time(); // inlined
            }
        }
        rfg_destroyable_shape* shape = get_shape();
        compress_state(shape, 0xDEADBEEF); // no checksum
        flags.run_stress_on_stream = stress_evaluation_stop(this) && !obj_flags.destroyed;
        if (dead_smoke_plume_effect != 0xFFFFFFFF)
        {
            stop_smoke_plume_effect(); // inlined
        }
        if (havok_handle != 0xFFFFFFFF)
        {
            nav_cell_detour_mover_destroyed(this);
            havok_body_free(havok_handle, true, true, false);
            havok_handle = 0xFFFFFFFF;
        }
        if (rdd.is_loaded())
        {
            resource_dependent_data* data = rdd.get_data();
            if (data->model_instance_p != nullptr)
            {
                data->model_instance_p->release();
            }
            data->model_instance_p = nullptr;
            if (data->shape != nullptr && shape->child_obj_list != 0xFFFF)
            {
                // the whole-mesh sub instances are no longer claimed
                for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
                {
                    rfg_dlod* dlod = shape->get_dlod(shape->parent->base_objects.to_pointer()[shape->get_subpiece_index(sp)].dlod_key & 0xFFFF);
                    if (dlod->get_owner() == shape && (dlod->flags & 2) != 0)
                    {
                        dlod->flags &= ~0x22;
                    }
                    if (sp->next == shape->child_obj_list)
                    {
                        break;
                    }
                }
            }
            if (shape != nullptr)
            {
                shape->delete_me();
                shape->removeReference();
            }
            data->shape = nullptr;
        }
        if (process_shardify_candidate_index != 0xFFFFFFFF)
        {
            rfg_dyn_remove_from_process_shardify_list(this);
        }
        release_damaged_subpiece_events();
        release_explosion_event(); // inlined
        rdd.set_loaded(false);
    }
    ai_building_process_streaming(this, load);
    return true;
}

// 0x00716980 (:2493)
// Mining targets are always restored; supply crates, propaganda and invulnerable movers never.
bool rfg_mover::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    checking_reset++;
    serialize(pblock, true);
    checking_reset--;
    if (flags.mining)
    {
        return true;
    }
    if (!flags.supply_crate && !flags.propaganda && !flags.invulnerable)
    {
        return object_mover::serialize_restore_if_missing_in_save_file(pblock);
    }
    return false;
}

// 0x007169D0 (:2523)
bool rfg_mover::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    checking_reset++;
    serialize(pblock, true);
    checking_reset--;
    if (!flags.mining && !flags.supply_crate && !flags.propaganda)
    {
        return !flags.invulnerable;
    }
    return false;
}

// 0x0071E9B0 (:2544)
// Saves which pieces still exist (SHAPE_STATE, unless one is pending): a mode word then the indices of the missing
// pieces (0), of the existing ones (1) or a bit field (2), whichever is smallest. A mover anchored by its own anchors
// or links (shape flags 0x30) with a destruction checksum also saves the indices of its world-anchored pieces
// (WORLD_ANCHOR) and the links it owns (DYNAMIC_LINK, yields negated), unless already pending. chunk_destruction_crc
// is not stored here.
bool rfg_mover::compress_state(rfg_destroyable_shape* shape, unsigned int chunk_destruction_crc)
{
    if (shape == nullptr || shape->get_object_buffer() == nullptr || obj_flags.destroyed)
    {
        return false;
    }
    const int num_existing = shape->num_existing_child_shapes;
    const int num_objects = shape->parent->num_objects;
    if (0 < num_existing && num_existing < num_objects && (state_data == nullptr || state_data->data_buf[rfg_mover_temporary_data::SHAPE_STATE] == nullptr))
    {
        const int missing_size = (num_objects - num_existing) * 2;
        const int existing_size = num_existing * 2;
        int size = num_objects / 8 + (num_objects % 8 > 0 ? 1 : 0); // bit field
        if (missing_size < size)
        {
            size = missing_size;
        }
        unsigned short mode;
        if (existing_size < size)
        {
            mode = 1;
            size = existing_size;
        }
        else if (size == existing_size)
        {
            mode = 1;
        }
        else if (size == missing_size)
        {
            mode = 0;
        }
        else
        {
            mode = 2;
            size += (size & 1) != 0 ? 2 - (size & 1) : 0; // whole words
        }
        alloc_data_buf(rfg_mover_temporary_data::SHAPE_STATE, nullptr, size + 2);
        unsigned short* buf = (unsigned short*)get_data_buf(rfg_mover_temporary_data::SHAPE_STATE);
        if (buf != nullptr)
        {
            buf[0] = mode;
            unsigned short* out = buf + 1;
            for (int i = 0; i < num_objects; i++)
            {
                const bool state = shape->get_shape_state(i);
                if (mode == (unsigned short)state)
                {
                    *out++ = (unsigned short)i;
                }
                else if (mode == 2 && state)
                {
                    buf[1 + i / 16] |= (unsigned short)(1 << (i % 16));
                }
            }
        }
    }
    if ((shape->flags & 0x30) == 0 || destruction_checksum == 0xFFFFFFFF)
    {
        return true;
    }
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    unsigned short* anchors = (unsigned short*)pool->alloc(num_existing * 2, 0);
    rfg_dynamic_link* links = (rfg_dynamic_link*)pool->alloc(num_existing * sizeof(rfg_dynamic_link), 0);
    if (anchors == nullptr || links == nullptr)
    {
        return false;
    }
    const unsigned int mover_handle = get_handle();
    int num_anchors = 0;
    int num_links = 0;
    if (shape->child_obj_list != 0xFFFF)
    {
        for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
        {
            if ((sp->flags & 1) != 0)
            {
                anchors[num_anchors++] = (unsigned short)shape->get_subpiece_index(sp);
            }
            rfg_dynamic_link* rdl = rfg_mover_get_dynamic_link(sp);
            if (rdl != nullptr && rdl->should_save(mover_handle))
            {
                links[num_links] = *rdl;
                links[num_links].yield_limit |= 0x80000000; // restored as is (rfg_mover_process_dynamic_link)
                num_links++;
            }
            if (sp->next == shape->child_obj_list)
            {
                break;
            }
        }
    }
    if (get_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR) == nullptr && 0 < num_anchors)
    {
        alloc_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR, (unsigned char*)anchors, num_anchors * 2);
    }
    if (get_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK) == nullptr && 0 < num_links)
    {
        alloc_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK, (unsigned char*)links, num_links * sizeof(rfg_dynamic_link));
    }
    return true;
}

// The record of a saved decal (DECAL_LIST: a count, then the records). Placement inferred.
// size 0x24
struct rfg_mover_saved_decal
{
    unsigned int decal_info_hash; // 0x00
    vector pos; // 0x04
    quaternion orient; // 0x10
    unsigned short width; // 0x20, of 100 m in 1/65535
    unsigned short depth; // 0x22
};

// 0x0071ECC0 (:2681)
// Re-adds the saved decals (permanent) to the streamed-in instance. The depth is decoded with 65525 instead of 65535
// (kept).
void rfg_mover::add_decals()
{
    if (state_data == nullptr || state_data->data_size[rfg_mover_temporary_data::DECAL_LIST] == 0 || state_data->data_buf[rfg_mover_temporary_data::DECAL_LIST] == nullptr)
    {
        return;
    }
    const unsigned char* buf = state_data->data_buf[rfg_mover_temporary_data::DECAL_LIST];
    update_render(nullptr);
    resource_dependent_data* data = rdd.get_data();
    if (data->model_instance_p == nullptr)
    {
        return;
    }
    const int count = *(const int*)buf;
    const rfg_mover_saved_decal* saved = (const rfg_mover_saved_decal*)(buf + 4);
    for (int i = 0; i < count; i++, saved++)
    {
        rl_decal_parameters params;
        decal_info_get_by_hash(&params, saved->decal_info_hash);
        params.m_pos = saved->pos;
        params.m_permanent = true;
        saved->orient.extract_matrix(params.m_transform);
        params.m_planar_params.m_width = ((float)saved->width / 65535.0f) * 100.0f;
        params.m_planar_params.m_depth = ((float)saved->depth / 65525.0f) * 100.0f; // see above
        params.m_planar_params.m_length = params.m_planar_params.m_width;
        data->model_instance_p->add_decal(&params, 0xFFFFFFFF);
    }
}

// 0x0073C3F0 (:2725)
// Restores the saved shape state (modes 0..2 as written by compress_state, 3..5 the same with the chunk's destruction
// checksum after the mode word; a checksum mismatch drops the state and fails). Without state every piece exists.
// With a state, a shape without siblings first removes all links, then the links between two pieces of the shape are
// restored. Then the instance is built (whole when no state was saved) and, for a mover with anchors found, the saved
// world anchors and dynamic links are re-created (links to movers not streamed yet stay pending).
bool rfg_mover::decompress_state(rfg_destroyable_shape* shape, unsigned int chunk_destruction_crc)
{
    unsigned short* state = (unsigned short*)get_data_buf(rfg_mover_temporary_data::SHAPE_STATE);
    unsigned int mode = 0xFFFFFFFF;
    unsigned short* entries = nullptr;
    if (state != nullptr)
    {
        mode = state[0];
        entries = state + 1;
        if (mode == 3 || mode == 4 || mode == 5)
        {
            const unsigned int crc = *(const unsigned int*)entries;
            entries += 2;
            if (crc != chunk_destruction_crc)
            {
                dealloc_data_buf(rfg_mover_temporary_data::SHAPE_STATE);
                return false;
            }
        }
    }
    rfg_base_destroyable_shape* base = shape->parent;
    int num_entries = (int)(get_data_buf_size(rfg_mover_temporary_data::SHAPE_STATE) >> 1) - 1;
    if (2 < (int)mode && (int)mode < 6)
    {
        num_entries = (int)(get_data_buf_size(rfg_mover_temporary_data::SHAPE_STATE) >> 1) - 3;
    }
    switch (mode)
    {
    case 0:
    case 3:
    {
        // the missing pieces are listed
        int k = 0;
        for (int i = 0; i < base->num_objects; i++)
        {
            if (k < num_entries && i == entries[k])
            {
                k++;
            }
            else
            {
                shape->set_shape_state(&shape->get_object_buffer()[i], true);
            }
        }
        break;
    }
    case 1:
    case 4:
        for (int k = 0; k < num_entries; k++)
        {
            shape->set_shape_state(&shape->get_object_buffer()[entries[k]], true);
        }
        break;
    case 2:
    case 5:
        for (int k = 0; k < num_entries; k++)
        {
            for (int bit = 0; bit < 16; bit++)
            {
                if ((entries[k] & (1 << bit)) != 0)
                {
                    shape->set_shape_state(&shape->get_object_buffer()[k * 16 + bit], true); // not bounded by num_objects
                }
            }
        }
        break;
    default:
        for (int i = 0; i < base->num_objects; i++)
        {
            shape->set_shape_state(&shape->get_object_buffer()[i], true);
        }
        break;
    }
    if (entries != nullptr)
    {
        if (shape->sib_next == shape)
        {
            for (int i = 0; i < base->num_links; i++)
            {
                shape->remove_link(shape->get_link(i));
            }
        }
        for (int i = 0; i < base->num_links; i++)
        {
            const rfg_link_base* link_base = shape->get_link_base(i);
            if (shape->get_object_buffer()[link_base->obj[0]].get_destroyable() == shape && shape->get_object_buffer()[link_base->obj[1]].get_destroyable() == shape)
            {
                shape->restore_link(shape->get_link(i));
            }
        }
    }
    dealloc_data_buf(rfg_mover_temporary_data::SHAPE_STATE); // inlined
    rl_destroyable_model_instance* ri = build_new_rl_instance_from_shape(shape, chunk_get_base_by_uid(chunk_uid), entries == nullptr);
    rdd.get_data()->model_instance_p = ri; // not checked
    if (ri == nullptr)
    {
        return false;
    }
    mover_flags &= ~0x80u;
    if ((mover_flags & 0x1000000) != 0)
    {
        const unsigned short* anchors = (const unsigned short*)get_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR);
        if (anchors != nullptr)
        {
            const int num_anchors = get_data_buf_size(rfg_mover_temporary_data::WORLD_ANCHOR) >> 1;
            for (int i = 0; i < num_anchors; i++)
            {
                shape->create_world_anchor(this, &shape->get_object_buffer()[anchors[i]]);
            }
        }
        const rfg_dynamic_link* links = (const rfg_dynamic_link*)get_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK);
        if (links != nullptr)
        {
            const unsigned int num_links = get_data_buf_size(rfg_mover_temporary_data::DYNAMIC_LINK) / sizeof(rfg_dynamic_link);
            for (unsigned int i = 0; i < num_links; i++)
            {
                unsigned int other = links[i].mover_handle[0];
                if (other == get_handle())
                {
                    other = links[i].mover_handle[1];
                }
                // only to level movers (with a destruction checksum)
                object_mover* other_mover = World.object_from_handle<object_mover>(other);
                if (other_mover == nullptr || other_mover->destruction_checksum == 0xFFFFFFFF)
                {
                    continue;
                }
                rfg_mover_create_dynamic_link(links[i]); // inlined
            }
        }
    }
    dealloc_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR); // inlined
    dealloc_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK); // inlined
    return true;
}

// 0x0072EB70 (:2906)
void rfg_mover::free_this()
{
    stress_evaluation_stop(this);
    rfg_mover_delete_dynamic_link(get_handle());
    for (int i = 0; i < NUM_STATE_DATA_TYPES; i++)
    {
        dealloc_data_buf((rfg_mover_temporary_data::data_type)i); // inlined
    }
    if (Multiplayer)
    {
        multi_game_object_rfg_mover_destroyed(this);
    }
    sdc_destroy_report(this);
    object_mover::free_this();
}

// 0x00716A10 (:2940)
bool rfg_mover::is_killed() const
{
    return flags.game_destroyed != 0;
}

// 0x00734DB0 (:2948)
// Damage to one piece (hit_shape_key; none: nothing happens). Riding-shotgun damage counts for the vehicle rail
// activity; script point effects take the damage first. Mining and supply crate targets far from the player only take
// the player's damage. Bullets damage pieces of bullet-damageable materials (physical flag 0x8) or always damageable
// pieces (0x1000), more for shatterable pieces (base flag 0x40); level objects shatter shatterable pieces and damage
// the others; melee needs a melee-damageable material (0x10), an always damageable piece or a structural damage move
// (or a remote player on the server), and jitters the building around the hit for humans. The damage becomes a direct
// damage (or shatter) event of the destruction dynamics in local space. Melee returns false when it cannot break the
// piece's anchor or any of its links. Damage sounds are posted for cars, bullets, level objects and melee.
bool rfg_mover::apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server)
{
    if (hit_shape_key == 0xFFFFFFFF)
    {
        return true;
    }
    if (activity_vrail_is_riding_shotgun_running())
    {
        activity_register_damage((object*)source, this, damage, -1);
    }
    if (script_point_apply_damage(source, damage, from_type, hit_shape_key, from_server))
    {
        return true;
    }
    if (player_damage_only() && !object_is_local_player(source))
    {
        return true;
    }
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return true;
    }
    rfg_subpiece* sp = &shape->get_object_buffer()[hit_shape_key];
    const rfg_subpiece_base* sp_base = shape->get_subpiece_base(hit_shape_key);
    bool result = true;
    const rfg_physical_material* material = rfg_get_physical_material_by_index(sp_base->physical_material_index); // inlined
    if (material == nullptr)
    {
        return true;
    }
    const vector reversed_normal = -hit_normal;
    vector local_pos;
    vector local_dir;
    switch (from_type)
    {
    case DBT_CAR:
        break;
    case DBT_BULLET:
    {
        if ((material->flags & 0x8) == 0 && (sp->flags & 0x1000) == 0)
        {
            return true;
        }
        const float scalar = (sp_base->flags & 0x40) != 0 ? Bullet_shatter_damage_scalar : Bullet_damage_scalar;
        convert_point_direction_from_world_to_local(local_pos, local_dir, hit_pos, reversed_normal, pos, orient);
        sp_combined_direct_damage_raw combined;
        combined.shape_key = hit_shape_key;
        combined.damage = (unsigned int)(int)(scalar * damage);
        rfg_dyn_add_pre_collide_direct_damage(this, source, 1, &combined, local_pos, local_dir, false); // inlined
        Rfg_mover_damage_applied = true;
        break;
    }
    case DBT_LEVEL_OBJECT:
        convert_point_direction_from_world_to_local(local_pos, local_dir, hit_pos, reversed_normal, pos, orient);
        if ((sp_base->flags & 0x40) == 0)
        {
            sp_combined_direct_damage_raw combined;
            combined.shape_key = hit_shape_key;
            combined.damage = (unsigned int)(int)(Level_object_damage_scalar * damage);
            rfg_dyn_add_pre_collide_direct_damage(this, source, 1, &combined, local_pos, local_dir, false); // inlined
        }
        else
        {
            rfg_dyn_add_pre_collide_shatter(this, source, hit_shape_key, 0, local_pos, local_dir, false); // inlined
        }
        Rfg_mover_damage_applied = true;
        break;
    case DBT_MELEE:
    {
        bool structural = false;
        if (source->get_obj_type() == OT_HUMAN)
        {
            const human_melee_combat_move_data* move = ((const human*)source)->combat.cur_melee_combat_move;
            structural = move != nullptr && move->flags.structural_damage;
            if (Multiplayer == true && multi_is_server() && object_is_player(source) && !object_is_local_player(source))
            {
                structural = true;
            }
        }
        if ((material->flags & 0x10) == 0 && (sp->flags & 0x1000) == 0 && !structural)
        {
            return false;
        }
        const unsigned int melee_damage = (unsigned int)(Melee_damage_scalar * damage);
        convert_point_direction_from_world_to_local(local_pos, local_dir, hit_pos, reversed_normal, pos, orient);
        sp_combined_direct_damage_raw combined;
        combined.shape_key = hit_shape_key;
        combined.damage = melee_damage;
        rfg_dyn_add_pre_collide_direct_damage(this, source, 1, &combined, local_pos, local_dir, false); // inlined
        Rfg_mover_damage_applied = true;
        if (structural && source->get_obj_type() == OT_HUMAN)
        {
            create_explosion_event(local_pos, Melee_jitter_radius, 0.0f, 0);
        }
        // the hit breaks the piece only when it beats its anchor and every link
        if ((sp->flags & 1) == 0 || sp->anchor_yield <= (int)melee_damage)
        {
            const unsigned short* links = sp->links.to_pointer();
            for (int i = 0; i < sp->num_links; i++)
            {
                if ((int)melee_damage < shape->get_link(links[i])->yield_limit)
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            result = false;
        }
        break;
    }
    default:
        return true;
    }
    maybe_post_damage_sound(source, hit_pos, damage);
    return result;
}

// 0x00726540 (:3111)
// Once the body is in the world: lets the parent rfg_mover find its anchors first, then collects the pieces touching
// the world (world anchors) and other movers (dynamic links). A mover without anchors falls (at least RFGM_NORMAL);
// movers that just got anchored are frozen and stress tested. Dynamic link anchors are searched from each new anchored
// mover in turn.
bool rfg_mover::find_anchor_points()
{
    if ((mover_flags & 0x1000000) != 0)
    {
        return true;
    }
    hkpRigidBody* body = havok_body_get_pointer(havok_handle);
    if (body == nullptr || body->getWorld() == nullptr)
    {
        return false;
    }
    rfg_mover* parent_mover = World.object_from_handle<rfg_mover>(get_parent());
    if (parent_mover != nullptr)
    {
        parent_mover->find_anchor_points();
    }
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return false;
    }
    subpiece_collector collector;
    create_anchors(COLLIDE_RAY_WORLD_ONLY, collector);
    mover_flags |= 0x1000000;
    if ((shape->flags & 0x30) == 0)
    {
        set_move_type(move_type < RFGM_LITE ? RFGM_NORMAL : move_type, false); // inlined
    }
    else
    {
        rfg_mover* rm = collector.new_anchor_list;
        while (rm != nullptr)
        {
            rfg_mover* next = rm->stress_next;
            if (next == rm)
            {
                rm->stress_next = nullptr;
                rm->stress_prev = nullptr;
                next = nullptr;
            }
            else
            {
                rm->stress_prev->stress_next = rm->stress_next;
                rm->stress_next->stress_prev = rm->stress_prev;
                rm->stress_next = nullptr;
                rm->stress_prev = nullptr;
            }
            collector.new_anchor_list = next;
            rm->create_anchors(COLLIDE_DYNAMIC_LINK, collector);
            rm->set_move_type(RFGM_FIXED, true); // inlined
            stress_evaluation_start(rm, true, false);
            rm = collector.new_anchor_list;
        }
    }
    if ((shape->flags & 0x10) == 0)
    {
        nav_cell_detour_mover_created(this);
    }
    if (!flags.pristine)
    {
        update_damage_percent();
    }
    return true;
}

// 0x0071EDF0 (:3184)
// Drops the saved world anchors and pending dynamic links; general movers that were linked release their anchors too.
void rfg_mover::release_anchor_points()
{
    object_mover::release_anchor_points();
    dealloc_data_buf(rfg_mover_temporary_data::WORLD_ANCHOR); // inlined
    const rfg_dynamic_link* links = (const rfg_dynamic_link*)get_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK);
    if (links != nullptr)
    {
        const unsigned int num_links = get_data_buf_size(rfg_mover_temporary_data::DYNAMIC_LINK) / sizeof(rfg_dynamic_link);
        for (unsigned int i = 0; i < num_links; i++)
        {
            unsigned int other = links[i].mover_handle[0];
            if (other == get_handle())
            {
                other = links[i].mover_handle[1];
            }
            general_mover* gm = World.object_from_handle<general_mover>(other);
            if (gm != nullptr)
            {
                gm->release_anchor_points();
            }
        }
    }
    dealloc_data_buf(rfg_mover_temporary_data::DYNAMIC_LINK); // inlined
}

// 0x007267E0 (:3210)
// Creates the destroyable Havok body (shape type 5) from the shape's mass, centre of mass and tensor. Level movers
// (flags 0x8 always set, 0x4 when unconstrained) start at least RFGM_NORMAL when force_dynamic; pieces broken off
// (movable, mover_flags 0x4) get flags 0x4 and an auto remove level (1 normal, 5 lighter). World anchors (mover_flags
// 0x4000000) start fixed. Then the move type is applied and the stress test queued.
void rfg_mover::add_to_world(rfg_move_type initial_move_type)
{
    physics_create_info pci;
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr || shape->getReferenceCount() == 0)
    {
        return;
    }
    pci.pos = pos;
    pci.orient = orient;
    pci.shape_type = (havok_shape_type)5; // destroyable
    pci.mass = shape->mass;
    pci.friction = 1.0f;
    pci.restitution = 0.0f;
    pci.precalc_center_of_mass = shape->center_of_mass;
    pci.obj_ptr = this;
    pci.shape = shape;
    shape->get_tensor(pci.precalc_tensor);
    const bool movable = (mover_flags & 0x4) != 0;
    pci.flags |= 0x1;
    if (movable)
    {
        pci.flags |= 0x4;
    }
    else
    {
        pci.flags |= 0x8;
        if (child_constraint_ptr == nullptr && host_constraint_ptr == nullptr)
        {
            pci.flags |= 0x4;
        }
        if ((mover_flags & 0x8) != 0 && initial_move_type < RFGM_LITE) // force_dynamic
        {
            initial_move_type = RFGM_NORMAL;
        }
    }
    pci.collision_type = rfg_dyn_get_filter_by_move_type(initial_move_type, shape->flags);
    pci.auto_remove_level = 0;
    if (movable)
    {
        switch (initial_move_type)
        {
        case RFGM_NORMAL:
            pci.auto_remove_level = 1;
            break;
        case RFGM_LITE:
        case RFGM_ULTRA_LITE:
        case RFGM_WORLD_ONLY:
        case RFGM_NO_COLLISION:
            pci.auto_remove_level = 5;
            break;
        default:
            break;
        }
    }
    if ((mover_flags & 0x4000000) == 0)
    {
        pci.solver_deactivation = (hkpRigidBodyCinfo::SolverDeactivation)(((mover_flags & 0x4) | 0x8) >> 2); // 2, or 3 when movable
        pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_DEBRIS;
        pci.motion_type = hkpMotion::MOTION_DYNAMIC;
    }
    else
    {
        pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_FIXED;
        pci.motion_type = hkpMotion::MOTION_FIXED;
    }
    havok_handle = havok_body_create(pci);
    if (initial_move_type == move_type)
    {
        havok_body_set_motion_type(havok_handle, havok_motion_type_from_move_type(initial_move_type)); // inlined
    }
    else
    {
        set_move_type(initial_move_type, false); // inlined
    }
    stress_evaluation_start(this, true, false);
}

// 0x0071EF10 (:3287)
// Switches the body's collision filter, motion type and quality (RFGM_NORMAL uses the fast filter for shapes with
// flag 0x4); nothing happens without a body.
void rfg_mover::set_move_type(rfg_move_type mt, bool force)
{
    if ((move_type == mt && !force) || havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    rfg_destroyable_shape* shape = get_shape();
    const int shape_flags = shape != nullptr ? shape->flags : 0;
    unsigned int collision_type;
    switch (mt)
    {
    case RFGM_FIXED:
        collision_type = COLLIDE_RFG_INITIAL_POSITION;
        break;
    case RFGM_NORMAL:
        collision_type = (shape_flags & 4) != 0 ? COLLIDE_RFG_NORMAL_FAST : COLLIDE_RFG_NORMAL;
        break;
    case RFGM_LITE:
        collision_type = COLLIDE_RFG_LITE;
        break;
    case RFGM_ULTRA_LITE:
        collision_type = COLLIDE_RFG_ULTRA_LITE;
        break;
    case RFGM_WORLD_ONLY:
        collision_type = COLLIDE_RFG_WORLD_ONLY;
        break;
    default:
        collision_type = COLLIDE_NO_COLLISION;
        break;
    }
    havok_body_set_collision_type(havok_handle, collision_type, false, false);
    havok_body_set_motion_type(havok_handle, havok_motion_type_from_move_type(mt)); // inlined
    havok_body_set_collision_quality(havok_handle, collidable_quality_from_move_type(mt)); // inlined
    move_type = mt;
}

// 0x00716A20 (:3308)
float rfg_mover::percent_destroyed() const
{
    return damage_pct;
}

// 0x0072ECA0 (:3315)
// A fixed mover sees other_mover collapse (once per mover): the concrete collapse effect sized by the footprint, at the
// footprint centre and this mover's height, and the collapse sound for NPCs.
void rfg_mover::play_collapse(rfg_mover* other_mover)
{
    if (move_type != RFGM_FIXED || other_mover == nullptr || other_mover->flags.collapse_effect)
    {
        return;
    }
    other_mover->flags.collapse_effect = 1;
    vector bmin;
    vector bmax;
    other_mover->get_bbox(bmin, bmax);
    const material_event_size size = rfg_effect_candidate_get_size(Default_collapse_mat_index, MATERIAL_EVENT_COLLAPSE, (bmax.z - bmin.z) * (bmax.x - bmin.x));
    const vector effect_pos((bmin.x + bmax.x) * 0.5f, pos.y, (bmax.z + bmin.z) * 0.5f);
    if (rfg_effect_candidate_create(Default_collapse_mat_index, effect_pos, MATERIAL_EVENT_COLLAPSE, size, INVALID_MATERIAL_EVENT_SHAPE_TYPE, false, 0, nullptr, nullptr, 0xFFFFFFFF, 0xFF, 0xFF))
    {
        npc_sound_info_building_collapse sound(this);
        npc_sound_info::queue(sound);
    }
}

// 0x0072EDF0 (:3348)
// The fraction of missing pieces of the dlod "<sub_piece_name>_parent" (only while damaged this frame, dlod flag 0x1).
bool rfg_mover::get_sub_piece_percent_destroyed(float& percent_destroyed, const char* sub_piece_name) const
{
    rfg_destroyable_shape* shape = get_shape();
    char name[1024];
    sprintf(name, "%s_parent", sub_piece_name);
    const unsigned int dlod_index = shape->get_dlod_index_from_name(name); // inlined, shape not checked
    if (dlod_index == 0xFFFFFFFF)
    {
        return false;
    }
    const rfg_dlod* dlod = shape->get_dlod(dlod_index);
    if ((dlod->flags & 1) == 0)
    {
        return false;
    }
    percent_destroyed = (float)((int)dlod->max_pieces - (int)dlod->active_pieces) / (float)dlod->max_pieces;
    return true;
}

// 0x0072EEF0 (:3371)
bool rfg_mover::get_sub_piece_percent_destroyed(float& percent_destroyed, int sub_piece_index) const
{
    c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr || sub_piece_index < 0 || sub_piece_index >= chunk->num_script_points)
    {
        return false;
    }
    return get_sub_piece_percent_destroyed(percent_destroyed, chunk->script_points.to_pointer()[sub_piece_index].sub_piece_name);
}

// 0x0072EF40 (:3394)
// The building damage sound for NPCs (not for mining targets), at most every 5 s, for big hits (see the notes); the
// attacker is the human or the driver of the vehicle.
void rfg_mover::maybe_post_damage_sound(const object* source, vector hit_pos, float damage_amt)
{
    if (flags.mining || source == nullptr || !damage_sound_propagate_timer.elapsed())
    {
        return;
    }
    if (percent_destroyed() < RFG_MOVER_DAMAGE_SOUND_MIN && damage_amt < RFG_MOVER_DAMAGE_SOUND_MIN)
    {
        return;
    }
    const human* attacker;
    if (source->get_obj_type() == OT_HUMAN)
    {
        attacker = (const human*)source;
    }
    else if (source->get_obj_type() == OT_VEHICLE)
    {
        attacker = vehicle_most_recent_driver((const vehicle*)source);
    }
    else
    {
        return;
    }
    if (attacker == nullptr)
    {
        return;
    }
    npc_sound_info_building_damage sound(this, attacker, hit_pos, damage_amt);
    npc_sound_info::queue(sound);
    damage_sound_propagate_timer.set(RFG_MOVER_DAMAGE_SOUND_DELAY_MS);
}

// 0x0071EFF0 (:3438)
explosion_event* rfg_mover::get_explosion_event()
{
    if (explosion_event_index == 0xFFFF)
    {
        return nullptr;
    }
    return Explosion_event_pool.get_entry(explosion_event_index);
}

// 0x00726AD0 (:3449)
// One pending explosion per mover (for the jitter of the render update).
bool rfg_mover::create_explosion_event(const vector& posp, const explosion_info* info, unsigned int flagsp)
{
    if (explosion_event_index != 0xFFFF || Explosion_event_pool.get_free() <= 0)
    {
        return false;
    }
    explosion_event* ev = Explosion_event_pool.alloc(0);
    explosion_event_index = (unsigned short)Explosion_event_pool.get_index(ev);
    ev->pos = posp;
    ev->info = info;
    ev->fake_radius = 0.0f;
    ev->flags = flagsp;
    set_render_flags(8);
    return true;
}

// 0x00726B60 (:3472)
// A pending explosion with explicit radii (melee hits).
bool rfg_mover::create_explosion_event(const vector& posp, float outer_radius, float inner_radius, unsigned int flagsp)
{
    if (explosion_event_index != 0xFFFF || Explosion_event_pool.get_free() <= 0)
    {
        return false;
    }
    explosion_event* ev = Explosion_event_pool.alloc(0);
    explosion_event_index = (unsigned short)Explosion_event_pool.get_index(ev);
    ev->pos = posp;
    ev->fake_radius = outer_radius;
    ev->fake_inner_radius = inner_radius;
    ev->info = nullptr;
    ev->flags = flagsp;
    set_render_flags(8);
    return true;
}

// 0x0072F000 (:3496)
void rfg_mover::release_explosion_event()
{
    explosion_event* ev = get_explosion_event(); // inlined
    if (ev != nullptr)
    {
        Explosion_event_pool.free(ev);
        explosion_event_index = 0xFFFF;
    }
}

// 0x0071F030 (:3506)
damaged_subpiece_event* rfg_mover::get_damaged_subpiece_events()
{
    if (damaged_subpiece_event_first_index == 0xFFFF)
    {
        return nullptr;
    }
    return Damaged_sp_event_pool.get_entry(damaged_subpiece_event_first_index);
}

// 0x0071F060 (:3516)
damaged_subpiece_event* rfg_mover::get_next_damaged_subpiece_event(damaged_subpiece_event* cur_event)
{
    if (cur_event == nullptr || cur_event->next_event_index == 0xFFFF)
    {
        return nullptr;
    }
    return Damaged_sp_event_pool.get_entry(cur_event->next_event_index);
}

// 0x00726C00 (:3526)
// Pushes a damaged piece at the front of the mover's list (for the jitter of the render update).
bool rfg_mover::add_damaged_subpiece_event(unsigned int sp_key)
{
    if (Damaged_sp_event_pool.get_free() < 1)
    {
        return false;
    }
    damaged_subpiece_event* ev = Damaged_sp_event_pool.alloc(0);
    ev->subpiece_index = (unsigned short)sp_key;
    ev->next_event_index = damaged_subpiece_event_first_index;
    damaged_subpiece_event_first_index = (unsigned short)Damaged_sp_event_pool.get_index(ev);
    set_render_flags(8);
    return true;
}

// 0x0072F050 (:3542)
void rfg_mover::release_damaged_subpiece_events()
{
    unsigned short index = damaged_subpiece_event_first_index;
    while (index != 0xFFFF)
    {
        damaged_subpiece_event* ev = Damaged_sp_event_pool.get_entry(index); // not checked
        index = ev->next_event_index;
        Damaged_sp_event_pool.free(ev);
    }
    damaged_subpiece_event_first_index = 0xFFFF;
}

// The base of Repair_event_pool (the decompile takes it only when the pool has a capacity).
static repair_event* repair_event_at(unsigned short index)
{
    return Repair_event_pool.get_capacity() != 0 ? Repair_event_pool.get_entry(0) + index : nullptr;
}

// 0x00726C50 (:3556)
// Appends a repair sphere (local space) to the mover's circular list.
bool rfg_mover::record_repair_event(const vector& posp, float radius)
{
    if (Repair_event_pool.get_free() < 1)
    {
        return false;
    }
    repair_event* ev = Repair_event_pool.alloc(0);
    const unsigned short index = (unsigned short)(ev - repair_event_at(0));
    if (repair_event_list == 0xFFFF)
    {
        ev->next = index;
        ev->prev = index;
        repair_event_list = index;
    }
    else
    {
        repair_event* head = repair_event_at(repair_event_list);
        ev->prev = head->prev;
        ev->next = repair_event_list;
        repair_event_at(head->prev)->next = index;
        head->prev = index;
    }
    ev->pos = posp;
    ev->radius = radius;
    set_render_flags(8);
    return true;
}

// 0x00716A50 (:3577)
// The smoke plume of a game-destroyed building (single player, not in wrecking crew), only for plume_on_death buildings
// that are important EDF structures or mission objectives.
void rfg_mover::start_smoke_plume_effect()
{
    static unsigned int dest_effect; // 0x021062FC, guard $S6 0x021062F8
    static bool dest_effect_init;
    if (Multiplayer || wrecking_crew_is_active() || !Building_smoke_plumes || dead_smoke_plume_effect != 0xFFFFFFFF)
    {
        return;
    }
    const bool important = get_importance_level() >= 2 && team == HUMAN_TEAM_EDF;
    const bool objective = mission_is_active() && is_objective_handle(get_handle());
    if (is_killed() && flags.plume_on_death && (important || objective))
    {
        if (!dest_effect_init)
        {
            dest_effect_init = true;
            dest_effect = effects_get_handle("env_building_smoke_a");
        }
        dead_smoke_plume_effect = effects_play(dest_effect, pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x0071F090 (:3602)
void rfg_mover::stop_smoke_plume_effect()
{
    if (dead_smoke_plume_effect == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = World.object_from_handle<object_effect>(dead_smoke_plume_effect);
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
    dead_smoke_plume_effect = 0xFFFFFFFF;
}

// 0x00716B50 (:3620)
// Mining and supply crate targets more than 20 m from the player take only the player's damage (single player).
bool rfg_mover::player_damage_only() const
{
    if (Multiplayer || wrecking_crew_is_active() || (!flags.mining && !flags.supply_crate))
    {
        return false;
    }
    player* local = player_get_local_player();
    if (local == nullptr)
    {
        return false;
    }
    const float dx = pos.x - local->pos.x;
    const float dy = pos.y - local->pos.y;
    const float dz = pos.z - local->pos.z;
    return RFG_MOVER_PLAYER_ONLY_DIST_SQR < dz * dz + dy * dy + dx * dx;
}

// 0x00716C30 (:3636)
bool rfg_mover::allow_auto_restore() const
{
    return !flags.one_of_many && !flags.mining && !flags.supply_crate && !flags.propaganda && (mover_flags & 0x4) == 0;
}

// 0x00716C50 (:3647)
bool rfg_mover::is_tracked_mover() const
{
    return flags.mining || flags.supply_crate || flags.propaganda;
}

// 0x0071F0D0 (:3659)
void rfg_mover::update_physics_properties()
{
    rfg_destroyable_shape* shape = get_shape();
    if (havok_handle == 0xFFFFFFFF || shape == nullptr)
    {
        return;
    }
    matrix tensor;
    shape->get_tensor(tensor);
    havok_body_set_mass_props(havok_handle, shape->mass, shape->center_of_mass, tensor);
}

// 0x0071F130 (:3673)
void rfg_mover::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object_mover::update_pos_and_orient(update_pos, update_orient, set_havok_data);
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return;
    }
    bbox_instance(last_known_bmin, last_known_bmax, orient, pos, shape->local_bmin, shape->local_bmax);
    flags.render_xform_dirty = 1;
    check_for_movement_delete();
}

// 0x00726E00 (:3692)
// A script point of the chunk (see the notes); unloaded movers return the origin.
bool rfg_mover::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    if (!rdd.is_loaded())
    {
        if (tag_orient != nullptr)
        {
            tag_orient->make_identity();
        }
        tag_pos.zero();
        return false;
    }
    c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr)
    {
        return false;
    }
    if (tag_index < 0 || tag_index >= chunk->num_script_points)
    {
        return false;
    }
    const c_script_point& point = chunk->script_points.to_pointer()[tag_index];
    point.transform.get_translation(tag_pos);
    if (tag_orient != nullptr)
    {
        point.transform.get_orientation(*tag_orient);
    }
    return true;
}

// 0x00716C70 (:3725)
void rfg_mover::get_bbox(vector& _bmin, vector& _bmax) const
{
    _bmin = last_known_bmin;
    _bmax = last_known_bmax;
}

// 0x0071F1A0 (:3732)
void rfg_mover::get_local_bbox(vector& _bmin, vector& _bmax) const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape != nullptr)
    {
        _bmin = shape->local_bmin;
        _bmax = shape->local_bmax;
        return;
    }
    _bmin = last_known_bmin - pos;
    _bmax = last_known_bmax - pos;
}

// 0x00716CB0 (:3746)
// The gameplay properties' fully destroyed percent (0.5 by default), at least game_destroyed_pct.
float rfg_mover::get_fully_destroyed_pct()
{
    float pct = gameplay_props->fully_destroyed_pct;
    if (pct == -1.0f)
    {
        pct = RFG_MOVER_DEFAULT_FULLY_DESTROYED_PCT;
    }
    if (0.0f <= game_destroyed_pct - pct)
    {
        pct = game_destroyed_pct;
    }
    return pct;
}

// 0x0073C8F0 (:3757)
int rfg_mover::attach_index(const vector& posp, unsigned int parent_objp_body_handle)
{
    return query_closest_subpiece_sphere(posp, 1.0f);
}

// 0x0071F2E0 (:3764)
bool rfg_mover::attach_index_is_valid(int index)
{
    rfg_destroyable_shape* shape = get_shape();
    return shape != nullptr && index >= 0 && index <= shape->parent->num_objects; // see the notes
}

// 0x0071F310 (:3771)
float rfg_mover::get_mass() const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape != nullptr)
    {
        return shape->mass;
    }
    return object::get_mass();
}

// 0x00726E90 (:3782)
unsigned char rfg_mover::get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape_key != 0xFFFFFFFF && shape != nullptr)
    {
        return shape->parent->base_objects.to_pointer()[shape_key].physical_material_index;
    }
    return object::get_physical_material(shape_key, contact_point, hk_objp);
}

// 0x00726ED0 (:3800)
unsigned char rfg_mover::get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape_key != 0xFFFFFFFF && shape != nullptr)
    {
        return rfg_get_effect_material_index(shape->parent->base_objects.to_pointer()[shape_key].physical_material_index);
    }
    return object::get_effect_material(shape_key, contact_point, hk_objp);
}

// 0x00726F20 (:3820)
// The hit piece whose box is closest to pos (the first one containing it).
static int find_closest_subpiece(rfg_destroyable_shape* shape, const hkArray<unsigned int, hkContainerHeapAllocator>& hits, const vector& pos)
{
    int closest = -1;
    float closest_dist = 0.0f;
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[key];
        vector point;
        const float dist = bbox_find_closest_point(point, sp_base->prel_bmin, sp_base->prel_bmax, pos);
        if (dist < closest_dist || closest == -1)
        {
            closest = (int)key;
            closest_dist = dist;
            if (dist <= 0.0f)
            {
                break;
            }
        }
    }
    return closest;
}

// 0x00735790 (:3859)
// The piece in a world box closest to test_pos (the points are moved into the mover's frame with copy_rotate).
int rfg_mover::query_closest_subpiece_aabb(const vector& bb_min, const vector& bb_max, const vector& test_pos)
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return -1;
    }
    const vector local_pos = (test_pos - pos).copy_rotate(orient);
    const vector local_min = (bb_min - pos).copy_rotate(orient);
    const vector local_max = (bb_max - pos).copy_rotate(orient);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_aabb(shape, local_min, local_max, hits);
    return find_closest_subpiece(shape, hits, local_pos);
}

// 0x00735980 (:3879)
// The pieces in an oriented box, up to max_objects.
int rfg_mover::query_obb_for_obstacles(const vector& test_pos, const matrix& test_orient, const vector& bb_min, const vector& bb_max, rfg_subpiece** havok_objects, int max_objects)
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return 0;
    }
    vector extents;
    vector center;
    bbox_extract_obb_info(bb_min, bb_max, test_pos, test_orient, extents, center);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_obb(shape, center, test_orient, extents, hits);
    int count = 0;
    for (int i = 0; i < hits.getSize() && count < max_objects; i++)
    {
        havok_objects[count++] = &shape->get_object_buffer()[hits[i]];
    }
    return count;
}

// 0x00735A70 (:3911)
int rfg_mover::query_closest_subpiece_sphere(const vector& test_pos, const float radius)
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return -1;
    }
    const vector local_pos = (test_pos - pos).copy_rotate(orient);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, local_pos, radius, hits);
    return find_closest_subpiece(shape, hits, local_pos);
}

// 0x00735B80 (:3929)
// Steep pieces (the average of |dy| / horizontal distance to their linked pieces at least 1) whose centre lies 0..2 m
// above test_pos, inside the sphere; up to max_objects.
int rfg_mover::query_sphere_for_obstacles(const vector& test_pos, const float radius, rfg_subpiece** havok_objects, int max_objects)
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return 0;
    }
    const vector local_pos = (test_pos - pos).copy_rotate(orient);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    rfg_rbb_node::query_sphere(shape, local_pos, radius, hits);
    int count = 0;
    for (int i = 0; i < hits.getSize(); i++)
    {
        const unsigned int key = hits[i];
        rfg_subpiece* sp = &shape->get_object_buffer()[key];
        const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[key];
        const float y = sp_base->pos.y;
        if (local_pos.y - y > 0.0f || 2.0f < y - local_pos.y)
        {
            continue;
        }
        float slope = 0.0f;
        int num_links = 0;
        const unsigned short* links = sp->links.to_pointer();
        if ((float)sp->num_links != 0.0f)
        {
            for (int l = 0; l < sp->num_links; l++)
            {
                const rfg_link_base* link_base = shape->get_link_base(links[l]);
                int other = link_base->obj[0];
                if (other == (int)key)
                {
                    other = link_base->obj[1];
                }
                const rfg_subpiece_base* other_base = &shape->parent->base_objects.to_pointer()[other];
                const float dz = other_base->pos.z - sp_base->pos.z;
                const float dx = other_base->pos.x - sp_base->pos.x;
                slope = fabsf(other_base->pos.y - y) / sqrtf(dx * dx + dz * dz) + slope;
                num_links = sp->num_links;
            }
        }
        if (1.0f <= slope / (float)num_links) // no links: 0 / 0 fails the test
        {
            havok_objects[count++] = sp;
            if (count == max_objects)
            {
                break;
            }
        }
    }
    return count;
}

// 0x00726FE0 (:4004)
bool rfg_mover::is_shatterable(unsigned int shape_key) const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return false;
    }
    const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[shape_key];
    return sp_base != nullptr && (sp_base->flags & 0x40) != 0;
}

// 0x00727020 (:4016)
bool rfg_mover::can_be_damaged_by_bullets(unsigned int shape_key) const
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return false;
    }
    const rfg_subpiece_base* sp_base = &shape->parent->base_objects.to_pointer()[shape_key];
    if (sp_base == nullptr)
    {
        return false;
    }
    const rfg_physical_material* material = rfg_get_physical_material_by_index(sp_base->physical_material_index); // inlined
    return material != nullptr && (material->flags & 0x8) != 0;
}

// 0x00716D10 (:4031)
bool rfg_mover::debug_render_havok(const vector& center, const matrix& orientp, color c)
{
    return true;
}

static void set_game_destroyed(rfg_mover* rm, bool destroyed);

// 0x00716D20 (:4072)
// LTCG: rm in EDI.
static void set_game_destroyed_common(rfg_mover* rm, bool destroyed)
{
    rm->flags.game_destroyed = destroyed;
    if (destroyed)
    {
        object_mover_maybe_add_to_destroyed_event_queue(rm);
        activity_register_building_destroyed(rm);
    }
    object* child = rm->child_ptr;
    while (child != nullptr)
    {
        if (child->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            set_game_destroyed((rfg_mover*)child, destroyed);
        }
        child = child->child_next;
        if (child == rm->child_ptr)
        {
            break;
        }
    }
    rm->start_smoke_plume_effect(); // inlined
}

// 0x00716E70 (:4099)
// Game-destroyed level buildings (not riding shotgun, except mining targets): a HUD notice for EDF buildings with
// control (single player, not wrecking crew, once per building), the mining / supply crate counters, and for
// propaganda the morale change, the destroyed table, the statistic and the game log.
static void set_game_destroyed(rfg_mover* rm, bool destroyed)
{
    const bool riding = activity_vrail_is_riding_shotgun_running();
    set_game_destroyed_common(rm, destroyed);
    if (!destroyed || (riding && !rm->flags.mining))
    {
        return;
    }
    if (rm->get_parent() == 0xFFFFFFFF && !rm->flags.one_of_many && !Multiplayer && !wrecking_crew_is_active() && rm->team == HUMAN_TEAM_EDF && 0.0f < rm->control && !World.object_was_destroyed(rm->get_handle()))
    {
        hud_ui_mission_set_building_destroyed(rm->get_name(), rm->get_importance_level(), true);
    }
    if (rm->flags.mining)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            meta->mining_destroyed(rm->get_handle());
        }
    }
    if (rm->flags.supply_crate)
    {
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            meta->supply_crate_destroyed(rm->get_handle());
        }
    }
    if (rm->flags.propaganda && !World.object_was_destroyed(rm->get_handle()))
    {
        gfm_propaganda_morale_change(nullptr);
        World.object_add_to_destroyed_table(rm->get_handle());
        stats_increment_by_one(STATS_PROPAGANDA_DESTROYED);
        const vector item_pos = rm->pos;
        const int duration = gamelog_get_time_played();
        const char difficulty = gamelog_get_difficulty();
        const int locale_id = gamelog_get_locale();
        const char pct_game_complete = gamelog_get_pct_complete();
        const int time_played = gamelog_get_time_played();
        const int item_type = 4; // propaganda
        const int time = gamelog_get_logging_time();
        const int player_checksum = gamelog_get_player_checksum();
        log_event_player_find_collectable(player_checksum, time, item_type, time_played, pct_game_complete, locale_id, difficulty, duration, item_pos);
    }
}

// 0x00716FF0 (:4152)
// A fully destroyed building drops its navpoints, cover and action nodes and dynamic hole requests (with its
// children).
static void set_fully_destroyed(rfg_mover* rm, bool fully_destroyed)
{
    rm->flags.fully_destroyed = fully_destroyed;
    if (fully_destroyed)
    {
        delete_all_navpoints_in_building(rm);
        disable_all_cover_nodes_in_a_building(rm);
        disable_all_action_nodes_in_a_building(rm);
        dynamic_hole_requests_remove_for_building(rm);
    }
    object* child = rm->child_ptr;
    while (child != nullptr)
    {
        if (child->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            set_fully_destroyed((rfg_mover*)child, fully_destroyed);
        }
        child = child->child_next;
        if (child == rm->child_ptr)
        {
            break;
        }
    }
}

// 0x0071F350 (:4174)
// Recomputes the damage percent (from the top of an inherit_damage_pct hierarchy) and reports the structural mass lost
// (multiplayer mode, wrecking crew). For level movers: the damage report, the game-destroyed transition at
// game_destroyed_pct (mining targets at 99% or right away without salvage) with the control drop and the target
// destroyed log of "one of many" targets, the fully-destroyed transition, the damage queues and activities; the dlods'
// damaged-this-frame flags are cleared.
void rfg_mover::do_frame_report()
{
    rfg_mover* parent_mover = World.object_from_handle<rfg_mover>(get_parent());
    const float old_damage_pct = damage_pct;
    float mass_delta = current_structural_mass;
    if (parent_mover == nullptr || !flags.inherit_damage_pct)
    {
        update_damage_percent();
    }
    else
    {
        parent_mover->do_frame_report();
    }
    const float damage_percent_delta = damage_pct - old_damage_pct;
    const bool first_report = mass_delta == -1.0f;
    mass_delta = current_structural_mass - mass_delta;
    if (first_report)
    {
        if ((mover_flags & 0x4) != 0)
        {
            return;
        }
        float base_mass = 0.0f;
        calculate_structural_mass(base_mass);
        mass_delta = current_structural_mass - base_mass;
    }
    if (mass_delta != 0.0f)
    {
        if (Multiplayer == true)
        {
            multi_gameplay_mode_building_damage_changed(this, mass_delta);
        }
        wrecking_crew_register_building_damage(this, mass_delta);
    }
    if ((mover_flags & 0x4) != 0)
    {
        return;
    }
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return;
    }
    if (damage_report_handle != -1)
    {
        if (get_importance_level() < 1)
        {
            sdc_destroy_report(this);
            damage_report_handle = -1;
        }
        else if (!flags.game_destroyed && 0.0f < damage_percent_delta)
        {
            sdc_report_new_damage(this, damage_pct);
        }
    }
    if (!flags.mining)
    {
        if (!flags.game_destroyed && game_destroyed_pct <= damage_pct)
        {
            set_game_destroyed(this, true);
            drop_control();
            if (control <= 0.0f && flags.one_of_many)
            {
                player* local = player_get_local_player();
                const vector player_pos = gamelog_get_human_pos(local);
                const int duration = gamelog_get_game_duration();
                const char difficulty = gamelog_get_difficulty();
                const int locale_id = gamelog_get_locale();
                const char pct_game_complete = gamelog_get_pct_complete();
                const int time_played = gamelog_get_time_played();
                const int target_crc = gamelog_get_mover_crc(this);
                const int time = gamelog_get_logging_time();
                const int player_checksum = gamelog_get_player_checksum();
                log_event_target_destroyed(player_checksum, time, target_crc, time_played, pct_game_complete, locale_id, difficulty, duration, player_pos);
            }
        }
        else if (flags.game_destroyed && damage_pct < game_destroyed_pct)
        {
            set_game_destroyed(this, false); // inlined
        }
    }
    else if (!flags.game_destroyed && (salvage_num_pieces == 0 || fl_greater_than_equal_robust(damage_pct, 0.99f, 0.001f)))
    {
        set_game_destroyed(this, true);
    }
    if (!flags.fully_destroyed)
    {
        if (get_fully_destroyed_pct() <= damage_pct) // inlined
        {
            set_fully_destroyed(this, true);
        }
    }
    else if (damage_pct < get_fully_destroyed_pct()) // inlined
    {
        set_fully_destroyed(this, false);
    }
    object_mover_maybe_add_to_damaged_event_queue(this);
    activity_register_building_damage(this, damage_percent_delta, old_damage_pct);
    activity_tal_building_damaged(get_handle(), damage_percent_delta);
    if (shape->parent != nullptr)
    {
        for (int i = 0; i < shape->parent->num_dlods; i++)
        {
            shape->get_dlod(i)->flags &= ~1;
        }
    }
}

// 0x0071F7A0 (:4289)
bool rfg_mover::can_check_building() const
{
    return rdd.get_data() != nullptr;
}

// 0x00717060 (:4295)
bool rfg_mover::has_building_size() const
{
    if (move_type != RFGM_FIXED)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    get_bbox(bmin, bmax);
    return RFG_MOVER_BUILDING_MIN_SIZE <= bmax.x - bmin.x && RFG_MOVER_BUILDING_MIN_SIZE <= bmax.y - bmin.y && RFG_MOVER_BUILDING_MIN_SIZE <= bmax.z - bmin.z;
}

// 0x00717110 (:4320)
bool rfg_mover::has_building_mass() const
{
    if (move_type != RFGM_FIXED)
    {
        return false;
    }
    return RFG_MOVER_BUILDING_MIN_MASS < get_mass();
}

// 0x0071F7C0 (:4333)
// The time since the structure (the top rfg_mover of the hierarchy) was last damaged, -1 without a damage report.
int rfg_mover::time_since_last_damage() const
{
    const rfg_mover* top = this;
    rfg_mover* parent_mover = World.object_from_handle<rfg_mover>(get_parent());
    while (parent_mover != nullptr)
    {
        top = parent_mover;
        parent_mover = World.object_from_handle<rfg_mover>(top->get_parent());
    }
    gfm_damage_report* report = sdc_get_structure_damage_report(top);
    if (report != nullptr)
    {
        return report->get_time_since_damage();
    }
    return -1;
}

// 0x0073C910 (:4344)
// A mover split off another one takes its render instance from its creator's (once both have one); without it the
// mover is destroyed in single player.
void rfg_mover::create_deferred_ri()
{
    resource_dependent_data* data = rdd.get_data(); // not checked
    if (data->model_instance_p != nullptr || data->shape == nullptr)
    {
        return;
    }
    rfg_mover* creator = World.object_from_handle<rfg_mover>(creator_handle);
    creator_handle = 0xFFFFFFFF;
    if (creator == nullptr || !creator->rdd.is_loaded())
    {
        return;
    }
    resource_dependent_data* creator_data = creator->rdd.get_data();
    if (creator_data->shape == nullptr || creator_data->model_instance_p == nullptr)
    {
        return;
    }
    data->model_instance_p = build_new_rl_instance_from_shapes(data->shape, creator_data->shape, creator_data->model_instance_p, creator_handle); // see the notes
    if (data->model_instance_p == nullptr)
    {
        if (!Multiplayer)
        {
            World.destroy_object(this, false);
        }
        return;
    }
    data->model_instance_p->set_casts_shadows((mover_flags & 0x10) != 0);
    data->model_instance_p->set_casts_drop_shadows((mover_flags & 0x4000) != 0);
    if (gameplay_props != nullptr && gameplay_props->lod_properties != nullptr)
    {
        data->model_instance_p->set_lod_off_distance(gameplay_props->lod_properties->m_lod_off_dist); // inlined
    }
}

// Unlinks sp from a circular piece list through next/prev (head updated).
static void subpiece_list_remove(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    const unsigned short index = (unsigned short)shape->get_subpiece_index(sp);
    if (list == index)
    {
        if (sp->next == index)
        {
            sp->next = 0xFFFF;
            sp->prev = 0xFFFF;
            list = 0xFFFF;
            return;
        }
        list = sp->next;
    }
    shape->get_object_buffer()[sp->prev].next = sp->next;
    shape->get_object_buffer()[sp->next].prev = sp->prev;
    sp->next = 0xFFFF;
    sp->prev = 0xFFFF;
}

// Appends sp to a circular piece list through next/prev.
static void subpiece_list_append(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    const unsigned short index = (unsigned short)shape->get_subpiece_index(sp);
    if (list == 0xFFFF)
    {
        sp->next = index;
        sp->prev = index;
        list = index;
        return;
    }
    sp->next = list;
    sp->prev = shape->get_object_buffer()[list].prev;
    shape->get_object_buffer()[shape->get_object_buffer()[list].prev].next = index;
    shape->get_object_buffer()[list].prev = index;
}

// 0x00741680 (:4381)
// Applies the pieces queued by the destruction (transfer_child_obj_list, 0x40000000): pieces to delete (0x4000) fade
// out of the instance, pieces to create (0x2000) come back in; new pieces change the shape's properties and tree,
// and changes refresh the instance bounds.
void rfg_mover::update_render_subpieces()
{
    rfg_destroyable_shape* shape = get_shape();
    if (shape == nullptr)
    {
        return;
    }
    unsigned short delete_list = 0xFFFF;
    unsigned short create_list = 0xFFFF;
    int num_delete = 0;
    int num_create = 0;
    bool created = false;
    bool changed = false;
    rfg_subpiece* sp = shape->transfer_child_obj_list != 0xFFFF ? &shape->get_object_buffer()[shape->transfer_child_obj_list] : nullptr;
    while (sp != nullptr)
    {
        rfg_subpiece* next = sp->next != shape->transfer_child_obj_list ? &shape->get_object_buffer()[sp->next] : nullptr;
        subpiece_list_remove(shape, shape->transfer_child_obj_list, sp);
        sp->flags &= ~0x40000000u;
        unsigned int handled = 0;
        if ((sp->flags & 0x4000) != 0)
        {
            subpiece_list_append(shape, delete_list, sp);
            num_delete++;
            handled = 0x4000;
            changed = true;
        }
        else if ((sp->flags & 0x2000) != 0)
        {
            subpiece_list_append(shape, create_list, sp);
            num_create++;
            created = true;
            handled = 0x2000;
            changed = true;
        }
        sp->flags &= ~handled;
        sp = next;
    }
    delete_subpieces(delete_list, num_delete);
    create_subpieces(create_list, num_create);
    if (created)
    {
        shape->flags |= 0xC00;
        shape->recalc_shape_properties();
        shape->recalc_rbb_tree();
    }
    resource_dependent_data* data = rdd.get_data();
    if (changed && data != nullptr && data->model_instance_p != nullptr && data->shape != nullptr)
    {
        set_instance_bounds(data->model_instance_p, shape->local_bmin, shape->local_bmax); // inlined
    }
}

// 0x0072F0B0 (:4456)
// Removes deleted pieces from the instance: they fade out through a mover fader (unless no-fade, 0x80000000, or
// script point hosts, 0x400000; nano pieces use the nano effect), their dlods are re-checked, and owned whole-mesh sub
// instances give way to the remaining pieces.
void rfg_mover::delete_subpieces(unsigned short subpieces, int num_subpieces)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    if (num_subpieces == 0)
    {
        return;
    }
    rfg_destroyable_shape* shape = get_shape();
    resource_dependent_data* data = rdd.get_data();
    if (shape == nullptr || data == nullptr || data->model_instance_p == nullptr)
    {
        return;
    }
    rl_destroyable_model_instance* ri = data->model_instance_p;
    short* unclaim = (short*)pool->alloc(num_subpieces * 4, 0);
    int num_unclaim = 0;
    unsigned short list = subpieces;
    rfg_subpiece* sp = list != 0xFFFF ? &shape->get_object_buffer()[list] : nullptr;
    while (sp != nullptr)
    {
        rfg_subpiece* next = sp->next != list ? &shape->get_object_buffer()[sp->next] : nullptr;
        subpiece_list_remove(shape, list, sp);
        const short index = (short)shape->get_subpiece_index(sp);
        unclaim[num_unclaim] = index;
        ri->set_sub_instance_is_active(&unclaim[num_unclaim], 1, false, false);
        const unsigned int dlod_index = shape->parent->base_objects.to_pointer()[index].dlod_key & 0xFFFF;
        rfg_dlod* dlod = shape->get_dlod(dlod_index);
        short dlod_sub_inst = (short)(shape->parent->num_objects + dlod_index);
        if ((sp->flags & 0x10000) != 0)
        {
            mover_fader_destroy(get_handle(), index);
            sp->flags &= ~0x10000u;
            dlod->flags |= 0x10;
        }
        if ((sp->flags & 0x80000000) == 0 && (sp->flags & 0x400000) == 0)
        {
            rl_sub_instance_info sub_info;
            ri->get_sub_instance_info(sub_info, index);
            mover_fader_info info;
            info.fade_duration = fl_rand_range(Shard_delete_fade_time_min, Shard_delete_fade_time_max);
            info.mesh_p = (rl_mesh*)ri->get_renderable();
            info.material_map_p = ri->get_material_map();
            info.flags = 0x25;
            if ((sp->flags & 0x8000) != 0) // nano
            {
                info.flags = 0x2D;
                bool invert_render;
                info.material_effect_id = weapons_get_nano_material_effect(invert_render);
                if (invert_render)
                {
                    info.mat_fx_flags = 1;
                }
            }
            info.pos = sub_info.m_transform.m_translation;
            info.orient = sub_info.m_transform.m_rotation;
            info.submesh_index = sub_info.m_submesh_id;
            mover_fader_create(get_handle(), index, &info);
        }
        sp->flags &= ~0x30000u;
        dlod->flags |= 0x10;
        num_unclaim++;
        if ((dlod->flags & 2) != 0)
        {
            unclaim[num_unclaim++] = dlod_sub_inst;
            dlod->flags &= ~2;
        }
        if ((dlod->flags & 0x20) != 0)
        {
            mempool_mark_scope inner(pool);
            short* indices = (short*)pool->alloc(dlod->max_pieces * 2, 0);
            int num_indices = 0;
            dlod->get_subpiece_indices(num_indices, indices);
            ri->set_sub_instance_is_active(&dlod_sub_inst, 1, false, true);
            ri->set_sub_instance_is_active(indices, num_indices, true, false);
            dlod->flags &= ~0x20;
        }
        sp = next;
    }
    ri->unclaim_sub_instances(unclaim, num_unclaim);
}

// 0x0073D740 (:4579)
// The fade-in of a piece ended: its dlod is re-checked and may show its whole mesh again, otherwise the piece's own
// sub instance becomes active.
void rfg_mover_fade_in_finished(unsigned int owner, unsigned int uid)
{
    rfg_mover* rm = World.object_from_handle<rfg_mover>(owner);
    if (rm == nullptr || !rm->rdd.is_loaded())
    {
        return;
    }
    rfg_mover::resource_dependent_data* data = rm->rdd.get_data();
    if (data->model_instance_p == nullptr || data->shape == nullptr)
    {
        return;
    }
    rfg_destroyable_shape* shape = data->shape;
    rfg_subpiece* sp = &shape->get_object_buffer()[uid];
    if ((sp->flags & 0x10000) == 0)
    {
        return;
    }
    const unsigned int dlod_index = shape->parent->base_objects.to_pointer()[uid].dlod_key & 0xFFFF;
    sp->flags &= ~0x10000u;
    shape->get_dlod(dlod_index)->flags |= 0x10;
    rm->maybe_repair_dlod(dlod_index);
    if ((shape->get_dlod(dlod_index)->flags & 0x20) == 0)
    {
        short index = (short)uid;
        data->model_instance_p->set_sub_instance_is_active(&index, 1, true, false);
    }
}

// 0x0073D830 (:4612)
// Puts pieces back (regrow, repair, cancelled deletes): each regains its shape state and the links to existing
// neighbours (a piece left without links is dropped again), is claimed by the instance and fades in at its base
// transform (or its dlod's, for single-piece dlods); owned dlods are claimed and checked for their whole mesh.
void rfg_mover::create_subpieces(unsigned short subpieces, int num_subpieces)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rfg_destroyable_shape* shape = get_shape();
    resource_dependent_data* data = rdd.get_data();
    if (shape == nullptr || data == nullptr || data->model_instance_p == nullptr || num_subpieces == 0)
    {
        return;
    }
    rl_destroyable_model_instance* ri = data->model_instance_p;
    rfg_base_destroyable_shape* base = shape->parent;
    short* claim = (short*)pool->alloc(num_subpieces * 4, 0);
    int num_claim = 0;
    unsigned char* dlod_claimed = (unsigned char*)pool->alloc(base->num_dlods, 0);
    memset(dlod_claimed, 0, base->num_dlods);
    short* deactivate = (short*)pool->alloc(base->num_dlods * 2, 0);
    int num_deactivate = 0;
    rl_sub_instance_update* updates = (rl_sub_instance_update*)pool->alloc(num_subpieces * sizeof(rl_sub_instance_update), 0);
    int num_updates = 0;
    unsigned short list = subpieces;
    rfg_subpiece* sp = list != 0xFFFF ? &shape->get_object_buffer()[list] : nullptr;
    while (sp != nullptr)
    {
        rfg_subpiece* next = sp->next != list ? &shape->get_object_buffer()[sp->next] : nullptr;
        subpiece_list_remove(shape, list, sp);
        shape->set_shape_state(sp, true);
        sp->flags &= 0x60030003;
        const unsigned int index = shape->get_subpiece_index(sp);
        rfg_subpiece_base* sp_base = &base->base_objects.to_pointer()[index];
        const unsigned short* links = sp->links.to_pointer();
        for (int l = 0; l < sp_base->num_links; l++)
        {
            rfg_link* link = shape->get_link(links[l]);
            if (shape->get_shape_state(rfg_dyn_get_linked_subpiece(shape, sp, shape->get_link_base(links[l]))))
            {
                shape->restore_link(link);
            }
        }
        for (int l = sp->num_links - 1; l >= 0; l--)
        {
            const unsigned short link_index = links[l];
            if (!shape->get_shape_state(rfg_dyn_get_linked_subpiece(shape, sp, shape->get_link_base(link_index))))
            {
                shape->remove_link(shape->get_link(link_index));
            }
        }
        if (sp->num_links == 0)
        {
            shape->set_shape_state(sp, false);
            sp = next;
            continue;
        }
        claim[num_claim++] = (short)index;
        const unsigned int dlod_index = sp_base->dlod_key & 0xFFFF;
        rfg_dlod* dlod = shape->get_dlod(dlod_index);
        const rfg_subpiece_base_extra_data* extra = &base->base_extra_data.to_pointer()[index];
        matrix sp_orient;
        base->get_subpiece_orient_local(sp_base, sp_orient);
        rl_sub_instance_update& update = updates[num_updates];
        update.m_info.m_disable_deferred = false;
        update.m_base_index = (int)index;
        update.m_info.m_submesh_id = extra->render_subpiece;
        update.m_info.m_transform.set_orientation(sp_orient);
        update.m_info.m_transform.set_translation(sp_base->pos);
        mover_fader_info info;
        info.fade_duration = 1.0f;
        info.mesh_p = (rl_mesh*)ri->get_renderable();
        info.material_map_p = ri->get_material_map();
        info.flags = 0xB;
        info.material_effect_id = 0;
        if (dlod->max_pieces == 1)
        {
            const rfg_dlod_base* dlod_base = &base->base_dlods.to_pointer()[dlod_index];
            info.pos = dlod_base->pos;
            info.orient = dlod_base->orient;
            info.submesh_index = dlod_base->render_subpiece;
        }
        else
        {
            info.pos = sp_base->pos;
            info.orient = sp_orient;
            info.submesh_index = extra->render_subpiece;
        }
        info.update_render_done_cb = rfg_mover_fade_in_finished;
        if (mover_fader_create(get_handle(), index, &info) == 0xFFFFFFFF)
        {
            update.m_info.m_is_active = true;
        }
        else
        {
            update.m_info.m_is_active = false;
            sp->flags |= 0x10000;
            dlod->flags |= 0x10;
        }
        num_updates++;
        if (!dlod_claimed[dlod_index] && dlod->get_owner() == shape && (dlod->flags & 2) == 0)
        {
            dlod_claimed[dlod_index] = 1;
            const short dlod_sub_inst = (short)(base->num_objects + dlod_index);
            claim[num_claim++] = dlod_sub_inst;
            deactivate[num_deactivate++] = dlod_sub_inst;
            dlod->flags |= 2;
        }
        sp = next;
    }
    ri->claim_sub_instances(claim, num_claim);
    ri->set_sub_instance_is_active(deactivate, num_deactivate, false, false);
    ri->update_instances(updates, num_updates);
    for (int d = 0; d < base->num_dlods; d++)
    {
        if (dlod_claimed[d])
        {
            maybe_repair_dlod(d);
        }
    }
    do_frame_report();
}

// 0x00717140 (:4792)
void rfg_mover::process_fading_subpieces()
{
}

// 0x00717150 (:4864)
void rfg_mover::repair_jittered_shards(int dlod_index)
{
}

// 0x0073CA20 (:4998)
// A claimed, owned dlod whose pieces are all whole shows its whole mesh again instead of the pieces.
void rfg_mover::maybe_repair_dlod(int dlod_index)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rfg_destroyable_shape* shape = get_shape();
    resource_dependent_data* data = rdd.get_data();
    if (shape == nullptr || data == nullptr || data->model_instance_p == nullptr)
    {
        return;
    }
    rfg_dlod* dlod = shape->get_dlod(dlod_index);
    if (dlod->get_owner() != shape || (dlod->flags & 2) == 0)
    {
        return;
    }
    if (!dlod->should_render() || (dlod->flags & 0x20) != 0)
    {
        return;
    }
    short* indices = (short*)pool->alloc(dlod->max_pieces * 2, 0);
    short dlod_sub_inst = (short)(shape->parent->num_objects + dlod_index);
    int num_indices = 0;
    dlod->get_subpiece_indices(num_indices, indices);
    data->model_instance_p->set_sub_instance_is_active(&dlod_sub_inst, 1, true, true);
    data->model_instance_p->set_sub_instance_is_active(indices, num_indices, false, false);
    dlod->flags |= 0x20;
}

// 0x0071F830 (:5041)
void rfg_mover::transfer_decals_to_fader(rl_renderable_instance* fader_ri, unsigned int fader_uid)
{
    resource_dependent_data* data = rdd.get_data();
    if (data == nullptr || data->shape == nullptr || data->model_instance_p == nullptr)
    {
        return;
    }
    data->model_instance_p->transfer_decals(fader_ri, data->model_instance_p->get_decal_ri_handle(fader_uid));
}

// 0x0073DE80 (:5058)
// Repairs the jittered pieces (0x20000) inside each pending repair sphere: their decals fade, a fader moves each from
// its jittered transform back to its base transform (flags 0x31), and the touched dlods are checked for their whole
// mesh. The events are consumed.
void rfg_mover::process_repair_events()
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rfg_destroyable_shape* shape = get_shape();
    if (repair_event_list == 0xFFFF || shape == nullptr || rdd.get_data()->model_instance_p == nullptr)
    {
        return;
    }
    rl_destroyable_model_instance* ri = rdd.get_data()->model_instance_p;
    rfg_base_destroyable_shape* base = shape->parent;
    unsigned char* dlod_touched = (unsigned char*)pool->alloc(base->num_dlods, 0);
    memset(dlod_touched, 0, base->num_dlods);
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    repair_event* ev = repair_event_at(repair_event_list);
    while (ev != nullptr)
    {
        repair_event* next = ev->next != repair_event_list ? repair_event_at(ev->next) : nullptr;
        mempool_mark_scope inner(pool);
        hits.clear();
        rfg_rbb_node::query_sphere(shape, ev->pos, ev->radius, hits);
        rl_sub_instance_update* updates = (rl_sub_instance_update*)pool->alloc(hits.getSize() * sizeof(rl_sub_instance_update), 0);
        int num_updates = 0;
        for (int i = 0; i < hits.getSize(); i++)
        {
            const unsigned int index = hits[i];
            rfg_subpiece* sp = &shape->get_object_buffer()[index];
            rfg_subpiece_base* sp_base = &base->base_objects.to_pointer()[index];
            const rfg_subpiece_base_extra_data* extra = &base->base_extra_data.to_pointer()[index];
            ri->fade_decals_on_sub_instance(index, 1000);
            if ((sp->flags & 0x20000) == 0)
            {
                continue;
            }
            const unsigned int dlod_index = sp_base->dlod_key & 0xFFFF;
            rfg_dlod* dlod = shape->get_dlod(dlod_index);
            dlod_touched[dlod_index] = 1;
            if ((sp->flags & 0x10000) != 0)
            {
                mover_fader_destroy(get_handle(), index);
                sp->flags &= ~0x10000u;
                dlod->flags |= 0x10;
            }
            matrix sp_orient;
            base->get_subpiece_orient_local(sp_base, sp_orient);
            rl_sub_instance_update& update = updates[num_updates];
            update.m_info.m_disable_deferred = false;
            update.m_base_index = (int)index;
            update.m_info.m_submesh_id = extra->render_subpiece;
            update.m_info.m_transform.set_orientation(sp_orient);
            update.m_info.m_transform.set_translation(sp_base->pos);
            rl_sub_instance_info sub_info;
            ri->get_sub_instance_info(sub_info, index);
            mover_fader_info info;
            info.fade_duration = 1.0f;
            info.mesh_p = (rl_mesh*)ri->get_renderable();
            info.material_map_p = ri->get_material_map();
            info.pos = sub_info.m_transform.m_translation;
            info.orient = sub_info.m_transform.m_rotation;
            info.target_pos = sp_base->pos;
            info.target_orient = sp_orient;
            info.flags = 0x31;
            info.submesh_index = extra->render_subpiece;
            info.update_render_done_cb = rfg_mover_fade_in_finished;
            if (mover_fader_create(get_handle(), index, &info) == 0xFFFFFFFF)
            {
                update.m_info.m_is_active = true;
            }
            else
            {
                update.m_info.m_is_active = false;
                sp->flags |= 0x10000;
                dlod->flags |= 0x10;
            }
            num_updates++;
            sp->flags &= ~0x20000u;
        }
        ri->update_instances(updates, num_updates);
        const unsigned short index = (unsigned short)(ev - repair_event_at(0));
        if (repair_event_list == index && ev->next == index)
        {
            ev->next = 0xFFFF;
            ev->prev = 0xFFFF;
            repair_event_list = 0xFFFF;
        }
        else
        {
            if (repair_event_list == index)
            {
                repair_event_list = ev->next;
            }
            repair_event_at(ev->prev)->next = ev->next;
            repair_event_at(ev->next)->prev = ev->prev;
            ev->next = 0xFFFF;
            ev->prev = 0xFFFF;
        }
        Repair_event_pool.free(ev);
        ev = next;
    }
    for (int d = 0; d < base->num_dlods; d++)
    {
        if (dlod_touched[d] == 1)
        {
            maybe_repair_dlod(d);
        }
    }
}

// 0x00727090 (:5174)
// A destroyed script point effect: effects attached at its piece get their host's death, the piece is destroyed (a
// destruction event on its rfg_mover, or the general mover it broke into is destroyed, hidden on clients), and the
// instance is freed.
void rfg_mover::script_point_destroy(int idx)
{
    if ((unsigned int)idx >= MAX_SCRIPT_POINT_EFFECT_INSTS || !rdd.is_loaded())
    {
        return;
    }
    script_point_effect_inst* speip = m_level_obj_properties->m_script_point_effect_insts[idx];
    if (speip == nullptr)
    {
        return;
    }
    constraint* c = host_constraint_ptr;
    while (c != nullptr)
    {
        constraint* next = c->host_constraint_next != host_constraint_ptr ? c->host_constraint_next : nullptr;
        if ((unsigned int)c->host_attach_index == speip->shape_key)
        {
            object_effect* effect = World.object_from_handle<object_effect>(c->child_handle);
            if (effect != nullptr)
            {
                effect->handle_host_death();
            }
        }
        c = next;
    }
    object_mover* owner = rdd.get_data()->shape->get_object_buffer()[speip->shape_key].get_owner(); // see the notes
    rfg_mover* rm = World.object_cast<rfg_mover>(owner);
    general_mover* gm = World.object_cast<general_mover>(owner);
    if (rm == nullptr)
    {
        if (gm != nullptr)
        {
            if (!Multiplayer || multi_is_server())
            {
                World.destroy_object(gm, false);
            }
            else
            {
                gm->set_render_opacity(0); // inlined
            }
        }
    }
    else
    {
        unsigned int shape_key = speip->shape_key;
        rfg_dyn_add_pre_collide_sp_array(rm, nullptr, &shape_key, 1, false); // inlined
    }
    lopm_free_script_point_effect_inst(speip);
    m_level_obj_properties->m_script_point_effect_insts[idx] = nullptr;
}

// 0x0071F880 (:5224)
rl_renderable_instance* rfg_mover::get_render_model_instance()
{
    if (!rdd.is_loaded())
    {
        return nullptr;
    }
    return rdd.get_data()->model_instance_p;
}

// 0x00717160 (:5234)
void rfg_mover::update_render_opacity()
{
    handle_fading(0.0f, 1.0f);
}

// 0x0071F8A0 (:5242)
bool rfg_mover::enable_object_outline(int layer)
{
    rl_renderable_instance* ri = get_render_model_instance();
    if (ri != nullptr && (unsigned int)layer < 4)
    {
        ri->set_outline_enabled(layer); // inlined
    }
    return true;
}

// 0x00717180 (:5252)
bool rfg_mover::disable_object_outline()
{
    rl_renderable_instance* ri = get_render_model_instance();
    if (ri != nullptr)
    {
        ri->set_outline_disabled(); // inlined
    }
    return true;
}

// 0x0071F8E0 (:5262)
static void rfg_mover_toggle_self_and_children_invulnerable(rfg_mover* mover)
{
    object* child = mover->child_ptr;
    mover->flags.invulnerable ^= 1;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_MOVER && child->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover_toggle_self_and_children_invulnerable((rfg_mover*)child);
        }
        child = child->child_next;
        if (child == mover->child_ptr)
        {
            break;
        }
    }
}

// 0x0071F930 (:5275)
// Immediately, or through a destruction mode event (synchronized with the network).
void rfg_mover::toggle_invulnerable(bool immediate)
{
    if (immediate)
    {
        rfg_mover_toggle_self_and_children_invulnerable(this);
        return;
    }
    unsigned int mover_handle = get_handle();
    rfg_dyn_add_pre_collide_destruction_mode(&mover_handle, 1, false); // inlined
}

// 0x007171A0 (:5285)
bool rfg_mover::mover_should_sync()
{
    return true;
}

// 0x007171B0 (:5290)
void rfg_mover::set_render_created()
{
    mover_flags |= 0x80;
    rfg_mover_add_to_pre_stream_list(this); // inlined
}

// 0x00717210 (:5296)
void rfg_mover::set_render_changed()
{
    mover_flags |= 0x40;
    rfg_mover_add_to_pre_stream_list(this); // inlined
}

// 0x00717270 (:5307)
// An EDF building damaged before it streamed out is rebuilt (rebuild flag) after 5 minutes of play time or once the
// player died since.
void rfg_mover::maybe_flag_restore()
{
    player_meta_data* meta = player_get_meta_data();
    if (meta == nullptr)
    {
        return;
    }
    if (!flags.pristine && team == HUMAN_TEAM_EDF && !is_killed() && !flags.one_of_many && !flags.mining && !flags.supply_crate && !flags.propaganda && (mover_flags & 0x4) == 0)
    {
        if ((0 < stream_out_time && RFG_MOVER_STREAM_OUT_RESTORE_MS < (unsigned int)(meta->get_play_time() - stream_out_time)) || stream_out_time < meta->last_death_time)
        {
            flags.rebuild = 1;
        }
    }
    stream_out_time = 0;
}

// 0x00741B90 (:5332)
// The pre-stream render task: deferred instances and piece updates of the listed movers; the list is emptied.
static void rfg_mover_pre_stream_process_all(void*, rl_renderer*)
{
    rfg_mover* rm = Pre_stream_mover_list;
    while (rm != nullptr)
    {
        rfg_mover* next = rm->pre_stream_next != Pre_stream_mover_list ? rm->pre_stream_next : nullptr;
        if ((rm->mover_flags & 0x80) != 0)
        {
            rm->create_deferred_ri();
            rm->mover_flags &= ~0x80u;
        }
        if ((rm->mover_flags & 0x40) != 0)
        {
            rm->update_render_subpieces();
            rm->mover_flags &= ~0x40u;
        }
        if (Pre_stream_mover_list == rm && rm->pre_stream_next == rm)
        {
            Pre_stream_mover_list = nullptr;
        }
        else
        {
            if (Pre_stream_mover_list == rm)
            {
                Pre_stream_mover_list = rm->pre_stream_next;
            }
            rm->pre_stream_prev->pre_stream_next = rm->pre_stream_next;
            rm->pre_stream_next->pre_stream_prev = rm->pre_stream_prev;
        }
        rm->pre_stream_prev = nullptr;
        rm->pre_stream_next = nullptr;
        rm = next;
    }
}

// 0x00741C60 (:5381)
void rfg_mover_system_init()
{
    RFG_dynamic_link_pool.clear();
    Default_collapse_mat_index = rfg_get_effect_material_index("Concrete");
    Rfg_mover_state_pool.init(Rfg_mover_state_buffer, RFG_MOVER_STATE_BUFFER_SIZE, RFG_MOVER_STATE_PAGE_SIZE, "rfg_mover state pool", 1, 0);
    Pre_stream_render_task_mover.m_callback = rfg_mover_pre_stream_process_all;
    game_render_pre_stream_render_task_register(&Pre_stream_render_task_mover);
}

// 0x007172F0 (:5403)
void rfg_mover_level_init_post()
{
    if (Multiplayer)
    {
        create_level_rfg_mover_remote_objects();
    }
}

// 0x00727200 (:5411)
// Destroys every rfg_mover (in multiplayer without destroy packets).
void rfg_mover_level_shutdown()
{
    const int num = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num; i++)
    {
        rfg_mover* rm = World.get_object<rfg_mover>(i);
        if (Multiplayer == true)
        {
            rm->remote_obj_flags.client_destroy_permission = true;
            rm->remote_obj_flags.no_destroy_packet = true;
            rm->mp_info->destroy_causes.level_shutdown = true;
        }
        World.destroy_object(rm, false);
    }
}

// 0x00717300 (:5424)
void rfg_mover_set_thread_ownership()
{
    Rfg_mover_state_pool.set_thread_ownership();
}

// 0x0072F520 (:5430)
// Retries the live links (category 1), for movers that streamed in since.
void rfg_mover_do_frame_pre_sim()
{
    csspinlock_scope lock(RFG_dynamic_link_pool.thread_access);
    rfg_dynamic_link* rdl = RFG_dynamic_link_pool.get_first(1);
    while (rdl != nullptr)
    {
        rfg_dynamic_link* next = RFG_dynamic_link_pool.get_next(rdl);
        rfg_mover_process_dynamic_link(rdl);
        rdl = next;
    }
}

// 0x0071FAF0 (:5458)
void validate_rfg_mover_destruction(const object* obj)
{
}

// 0x00717310 (:5474)
bool rfg_mover_is_world_anchor(const rfg_mover* rm)
{
    return (rm->mover_flags & 0x4000000) != 0;
}

// 0x00717330 (:5479)
void rfg_mover_set_no_fade_destroy(rfg_mover* rm)
{
    rm->flags.mp_no_fade_destroy = 1;
}

// 0x00735E40 (:5485)
// Starts regrowing a damaged fixed building that is out of view (see the notes): a flood fill from the anchored
// pieces (world anchors and base flag 0x20) through regrowing materials, with a chance of 90%, 60% then 10% per
// step. rfg_mover_regrow_finish clears the marks and fades the regrown pieces in.
bool rfg_mover_regrow_begin(rfg_mover* rm)
{
    if ((rm->mover_flags & 0x1000000) == 0)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    rm->get_bbox(bmin, bmax);
    if (game_render_is_object_visible(bmin, bmax))
    {
        return false;
    }
    rm->flags.regrow_on_stream_begin = 0;
    if (rm->get_move_type() != RFGM_FIXED || rm->percent_destroyed() < REGROW_MIN_PERCENT_DESTROYED)
    {
        return false;
    }
    rfg_destroyable_shape* shape = rm->get_shape();
    if (shape == nullptr)
    {
        return false;
    }
    unsigned short list = 0xFFFF;
    if (shape->child_obj_list != 0xFFFF)
    {
        for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
        {
            const unsigned int index = shape->get_subpiece_index(sp);
            if ((sp->flags & 1) != 0 || (shape->parent->base_objects.to_pointer()[index].flags & 0x20) != 0)
            {
                sp->flags |= 0x80000;
                shape->parent->base_extra_data.to_pointer()[index].h = 0; // see the notes
                const unsigned short sp_index = (unsigned short)index;
                if (list == 0xFFFF)
                {
                    sp->shard_next = sp_index;
                    sp->shard_prev = sp_index;
                    list = sp_index;
                }
                else
                {
                    sp->shard_prev = shape->get_object_buffer()[list].shard_prev;
                    sp->shard_next = list;
                    shape->get_object_buffer()[shape->get_object_buffer()[list].shard_prev].shard_next = sp_index;
                    shape->get_object_buffer()[list].shard_prev = sp_index;
                }
            }
            if (sp->next == shape->child_obj_list)
            {
                break;
            }
        }
    }
    for (int step = 1; list != 0xFFFF; step++)
    {
        unsigned short next_list = 0xFFFF;
        const int regrow_chance = (10 - step * step) * 10; // percent: 90, 60, 10, then none
        rfg_subpiece* sp = &shape->get_object_buffer()[list];
        while (sp != nullptr)
        {
            const unsigned short sp_index = (unsigned short)shape->get_subpiece_index(sp);
            rfg_subpiece* next = sp->shard_next != list ? &shape->get_object_buffer()[sp->shard_next] : nullptr;
            if (list == sp_index && sp->shard_next == sp_index)
            {
                list = 0xFFFF;
            }
            else
            {
                if (list == sp_index)
                {
                    list = sp->shard_next;
                }
                shape->get_object_buffer()[sp->shard_prev].shard_next = sp->shard_next;
                shape->get_object_buffer()[sp->shard_next].shard_prev = sp->shard_prev;
            }
            sp->shard_next = 0xFFFF;
            sp->shard_prev = 0xFFFF;
            if (0 < regrow_chance)
            {
                rfg_mover_regrow_setup(shape, sp, regrow_chance, next_list);
            }
            sp = next;
        }
        list = next_list;
    }
    rm->flags.regrow_on_stream_finish = 1;
    return true;
}

// 0x00736160 (:5557)
void rfg_mover_regrow_finish(rfg_mover* rm)
{
    rm->flags.regrow_on_stream_finish = 0;
    rfg_destroyable_shape* shape = rm->get_shape();
    if (shape == nullptr || shape->child_obj_list == 0xFFFF)
    {
        return;
    }
    for (rfg_subpiece* sp = &shape->get_object_buffer()[shape->child_obj_list]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
    {
        rfg_mover_regrow_cleanup(shape, sp);
        if (sp->next == shape->child_obj_list)
        {
            return;
        }
    }
}

// 0x00717340 (:5573)
void rfg_mover_init_damage_applied()
{
    Rfg_mover_damage_applied = false;
}

// 0x00717350 (:5578)
bool rfg_mover_get_damage_applied()
{
    return Rfg_mover_damage_applied;
}

// 0x00717360 (:5583)
void rfg_mover_rebuild(rfg_mover* rm)
{
    if (rm != nullptr && rm->flags.rebuild)
    {
        rm->flags.rebuild = 0;
        World.restore_object(rm->get_handle());
    }
}

// 0x00717390 (:5652)
void rfg_mover::add_glass_object(glass_obj* glass_obj_p)
{
    if (m_glass_obj_list == nullptr)
    {
        glass_obj_p->mover_next = glass_obj_p;
        glass_obj_p->mover_prev = glass_obj_p;
        m_glass_obj_list = glass_obj_p;
    }
    else
    {
        glass_obj_p->mover_prev = m_glass_obj_list->mover_prev;
        glass_obj_p->mover_next = m_glass_obj_list;
        m_glass_obj_list->mover_prev->mover_next = glass_obj_p;
        m_glass_obj_list->mover_prev = glass_obj_p;
    }
    glass_obj_p->mover_p = this;
}

// 0x00717400 (:5660)
void rfg_mover::remove_glass_object(glass_obj* glass_obj_p)
{
    if (m_glass_obj_list == glass_obj_p)
    {
        if (glass_obj_p->mover_next == glass_obj_p)
        {
            glass_obj_p->mover_next = nullptr;
            glass_obj_p->mover_prev = nullptr;
            m_glass_obj_list = nullptr;
            return;
        }
        m_glass_obj_list = glass_obj_p->mover_next;
    }
    glass_obj_p->mover_prev->mover_next = glass_obj_p->mover_next;
    glass_obj_p->mover_next->mover_prev = glass_obj_p->mover_prev;
    glass_obj_p->mover_next = nullptr;
    glass_obj_p->mover_prev = nullptr;
}

// ---- obj_rfgm_props
// 0x0071B0A0 (.h:446)
// The property block of a mover split off old_mover: same transform, chunk and level properties, the initial move
// type, old_mover's flags (when the block has them) marked movable (0x4), the chunk uids, the new shape and the
// destruction uid.
obj_rfgm_props::obj_rfgm_props(rfg_mover* old_mover, rfg_move_type initial_move_type, const char* chunk_name, rfg_destroyable_shape* new_shape, unsigned int destruction_uid)
    : obj_mover_props(old_mover->pos, old_mover->orient, chunk_name, old_mover->m_level_obj_properties->m_base_properties->m_name, (old_mover->mover_flags & 0x10) != 0, (old_mover->mover_flags & 0x4000) != 0, nullptr, 0, OBJECT_IMPORTANCE_LEVEL_NONE, HUMAN_TEAM_NONE, 0)
{
    set(RFGM_PROP_MOVE_TYPE, initial_move_type);
    unsigned int mover_flags = 0;
    obj_prop* prop;
    if (get_internal(prop, OM_PROP_FLAGS, 5) && prop->size == sizeof(unsigned int))
    {
        mover_flags = *(unsigned int*)prop->data;
    }
    set(OM_PROP_FLAGS, mover_flags | 0x4);
    set(OM_PROP_CHUNK_UID, old_mover->chunk_uid);
    set(OM_PROP_CHUNK_OBJ_UID, old_mover->chunk_obj_uid);
    set(RFGM_PROP_NEW_SHAPE, new_shape);
    set(RFGM_PROP_DESTRUCT_UID, destruction_uid);
}

// 0x00712CA0
obj_rfgm_props::~obj_rfgm_props()
{
}
