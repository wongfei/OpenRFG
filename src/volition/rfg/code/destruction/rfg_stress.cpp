// Original: volition/rfg/code/destruction/rfg_stress.cpp
//
// The structural stress evaluation (decompiled; header comment in rfg_stress.h). Compiled in
// rfg_destruction_unity.cpp (last of the destruction sources), so it shares the file statics of rfg_dynamics.cpp
// (the collision event queue) and rfg_materials.cpp.
//
// One fixed rfg_mover is evaluated at a time, a few pieces per frame (Stress_controls objs_per_frame):
// - idle: the next mover of the high priority queue (heavy destruction movers wait in the low priority queue);
// - init stage 1: movers supporting it through dynamic links join the evaluation (evaluated first); the pieces that
//   rest on the ground (anchored, the root, or supported by a dynamic link) start the init queue;
// - init stage 2: a breadth first walk through the links gives every piece its layer (depth in the base extra data,
//   parity in flag 0x400) and builds the evaluation list (deepest layer first);
// - collect objects: objects lying on the mover add their mass to the pieces under them;
// - process object: each piece adds its mass and centre of mass to its parents (the linked pieces one layer closer to
//   the ground) and the force its weight puts on each parent link (weighted by stress_calc_dist_scalar); a piece whose
//   parent links are weaker than its mass (parent_yield_scalar) loses them;
// - process siblings: the pieces of one layer connected to each other are a group; when the group's force exceeds
//   its links' strength every piece of the group breaks its parent links (link broken events, the group then falls
//   through the shardify); one random piece of the group plays a stress effect sized by force / strength, starts
//   repeated stress tests (under_stress, stress_test_count), creaks to the npcs and shakes the camera;
// - finish: the next mover of the evaluation, or idle; the mover is tested again later (stress_test_delay).
// Shape flags used: 0x40 evaluate again, 0x80 in this evaluation, 0x100 sort the links only, 0x200 initialised.
// Piece flags: 0x200 in the evaluation, 0x400 layer parity.
//
// Notes (decompiled behaviour, kept as is):
// - Without a "Default" entry in rfg_stress_controls.xtbl, Current_stress_control stays -1 and Stress_controls[-1] is
//   read.
// - A piece supported by a dynamic link hands its centre of mass to the other mover's piece in this mover's local
//   coordinates.

#include "volition/rfg/code/destruction/rfg_stress.h"

#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_rbb_tree.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdlib.h>

static const int MAX_STRESS_CONTROLS = 4;
static const int MAX_STRESS_INFOS = 2800; // Stress_buffer
static const int MAX_STRESS_PARENTS = 256; // parent links of one piece in stress_evaluation_process_object
static const float GRAVITY = 9.8f;

// size 0x34
struct stress_evaluation_controls
{
    char name[32]; // 0x00
    int objs_per_frame; // 0x20
    int repeat_delay_min; // 0x24
    int repeat_delay_max; // 0x28
    float force_scalar; // 0x2C
    float parent_yield_scalar; // 0x30
};

enum stress_evaluation_state
{
    STRESS_EVALUATE_INVALID = 0,
    STRESS_EVALUATE_IDLE = 1,
    STRESS_EVALUATE_INIT_STAGE_1 = 2,
    STRESS_EVALUATE_INIT_STAGE_2 = 3,
    STRESS_EVALUATE_COLLECT_OBJECTS = 4,
    STRESS_EVALUATE_PROCESS_OBJECT = 5,
    STRESS_EVALUATE_PROCESS_SIBLINGS = 6,
    STRESS_EVALUATE_FINISH = 7,
    NUM_STRESS_EVALUATE_STATES = 8,
};

// One parent link of the piece in stress_evaluation_process_object.
struct stress_parent_entry
{
    int link; // index into the piece's links
    stress_evaluation_info* info;
    rfg_subpiece* sp;
};

// ---- globals
cs_wrapper Stress_mover_queue_wrapper(0); // 0x01F13368

// ---- file-static globals
static bool Stress_sibling_evaluating = true; // 0x016503EE
static unsigned short Stress_evaluation_list = 0xFFFF; // 0x01650484
static unsigned short Stress_sibling_list = 0xFFFF; // 0x01650488
static unsigned short Stress_sibling_local_list = 0xFFFF; // 0x0165048C
static unsigned short Stress_init_queue = 0xFFFF; // 0x01650490
static int Current_stress_control = -1; // 0x01650494
static int Default_stress_control = -1; // 0x01650498
static float Stress_event_large_force = 3000000.0f; // 0x0165049C
static float Stress_event_medium_force = 900000.0f; // 0x016504A0
static float Stress_event_small_force = 50000.0f; // 0x016504A4
static float Stress_min_percent_destroyed = 0.1f; // 0x016504A8
static float Stress_min_dot = 0.25f; // 0x016504AC
static stress_evaluation_controls Stress_controls[MAX_STRESS_CONTROLS]; // 0x01E38250
static stress_evaluation_state Current_state; // 0x01E3832C
static unsigned short Stress_info_free_list; // 0x01E38338
static unsigned short Stress_info_used_list; // 0x01E8408C
static rfg_subpiece* Stress_sibling_local_last_sp; // 0x01EF82E8
static rfg_mover* Current_mover; // 0x01EF82EC
static rfg_mover* Current_mover_list; // 0x01EF82F0, the movers of this evaluation (stress_next/prev ring)
static rfg_mover* Stress_evaluation_mover_queue; // 0x01EF82F4
static rfg_mover* Stress_evaluation_low_priority_queue; // 0x01EF82F8
static int Num_stress_control_configs; // 0x01EF82FC
static float Stress_sibling_total_force; // 0x01EF8300
static unsigned int Stress_sibling_total_yield; // 0x01EF8304
static int Stress_sibling_event_count; // 0x01EF8308
static int Stress_sibling_count; // 0x01EF830C
static stress_evaluation_info Stress_buffer[MAX_STRESS_INFOS]; // 0x01F2B3B0

static void stress_evaluation_finish_object(bool can_evaluate_again);

// ---- mover rings (stress_next / stress_prev)
static bool stress_mover_ring_contains(rfg_mover* list, const rfg_mover* rm)
{
    for (rfg_mover* it = list; it != nullptr;)
    {
        if (it == rm)
        {
            return true;
        }
        it = it->stress_next;
        if (it == list)
        {
            break;
        }
    }
    return false;
}

static void stress_mover_ring_remove(rfg_mover*& list, rfg_mover* rm)
{
    if (list == rm && (list = rm->stress_next) == rm)
    {
        list = nullptr;
    }
    else
    {
        rm->stress_prev->stress_next = rm->stress_next;
        rm->stress_next->stress_prev = rm->stress_prev;
    }
    rm->stress_prev = nullptr;
    rm->stress_next = nullptr;
}

static void stress_mover_ring_append(rfg_mover*& list, rfg_mover* rm)
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

// ---- piece rings (rfg_subpiece::stress_next / stress_prev, indices)
static void stress_piece_list_remove(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (list == index && (list = sp->stress_next) == index)
    {
        list = 0xFFFF;
    }
    else
    {
        objects[sp->stress_prev].stress_next = sp->stress_next;
        objects[sp->stress_next].stress_prev = sp->stress_prev;
    }
    sp->stress_next = 0xFFFF;
    sp->stress_prev = 0xFFFF;
}

// Inserts before the head (at the end of the ring); the head stays.
static void stress_piece_list_append(rfg_destroyable_shape* shape, unsigned short& list, rfg_subpiece* sp)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const unsigned short index = (unsigned short)(sp - objects);
    if (list == 0xFFFF)
    {
        sp->stress_next = index;
        sp->stress_prev = index;
        list = index;
        return;
    }
    sp->stress_prev = objects[list].stress_prev;
    sp->stress_next = list;
    objects[objects[list].stress_prev].stress_next = index;
    objects[list].stress_prev = index;
}

static rfg_subpiece* stress_linked_subpiece(rfg_destroyable_shape* shape, const rfg_subpiece* sp, unsigned short link_index)
{
    rfg_subpiece* objects = shape->get_object_buffer();
    const rfg_link_base* link_base = shape->get_link_base(link_index);
    int other = link_base->obj[0];
    if (other == (int)(sp - objects))
    {
        other = link_base->obj[1];
    }
    return &objects[other];
}

// 0x00718090
stress_evaluation_info::stress_evaluation_info()
{
}

// 0x007180A0 (:211)
static stress_evaluation_info* get_stress_eval_info(rfg_subpiece* sp)
{
    if (sp->eval_info_index == 0xFFFF)
    {
        return nullptr;
    }
    return &Stress_buffer[sp->eval_info_index];
}

// 0x007180D0 (:216)
// LTCG: info in EAX.
static void set_stress_eval_info(rfg_subpiece* sp, stress_evaluation_info* info)
{
    sp->eval_info_index = (unsigned short)(info - Stress_buffer);
}

// 0x00718100 (:225)
// The piece's accumulator, taken from the free list (cleared) when it has none; nullptr when all are taken.
static stress_evaluation_info* stress_evaluation_info_create(rfg_subpiece* sp)
{
    if (sp->eval_info_index == 0xFFFF)
    {
        if (Stress_info_free_list == 0xFFFF)
        {
            return nullptr;
        }
        const unsigned short index = Stress_info_free_list;
        stress_evaluation_info* info = &Stress_buffer[index];
        if (Stress_info_free_list == index && (Stress_info_free_list = info->next) == index)
        {
            Stress_info_free_list = 0xFFFF;
        }
        else
        {
            Stress_buffer[info->prev].next = info->next;
            Stress_buffer[info->next].prev = info->prev;
        }
        info->next = 0xFFFF;
        info->prev = 0xFFFF;
        if (Stress_info_used_list == 0xFFFF)
        {
            info->next = index;
            info->prev = index;
        }
        else
        {
            unsigned short& head_prev = Stress_buffer[Stress_info_used_list].prev;
            Stress_buffer[head_prev].next = index;
            info->prev = head_prev;
            head_prev = index;
            info->next = Stress_info_used_list;
        }
        Stress_info_used_list = index;
        info->total_com.zero();
        info->total_mass = 0.0f;
        info->total_force = 0.0f;
        info->total_yield = 0;
        set_stress_eval_info(sp, info); // inlined
    }
    return get_stress_eval_info(sp); // inlined
}

// 0x00718250 (:263)
// LTCG: sp in EAX, info in ECX. The piece leaves the evaluation (0x200) and its accumulator goes back to the free list.
static void stress_evaluation_info_release(stress_evaluation_info* info, rfg_subpiece* sp)
{
    if (sp != nullptr)
    {
        sp->flags &= ~0x200u;
        sp->eval_info_index = 0xFFFF;
    }
    if (info == nullptr)
    {
        return;
    }
    const unsigned short index = (unsigned short)(info - Stress_buffer);
    if (Stress_info_used_list == index && (Stress_info_used_list = info->next) == index)
    {
        Stress_info_used_list = 0xFFFF;
    }
    else
    {
        Stress_buffer[info->prev].next = info->next;
        Stress_buffer[info->next].prev = info->prev;
    }
    info->prev = 0xFFFF;
    info->next = 0xFFFF;
    if (Stress_info_free_list == 0xFFFF)
    {
        info->next = index;
        info->prev = index;
        Stress_info_free_list = index;
        return;
    }
    unsigned short& head_prev = Stress_buffer[Stress_info_free_list].prev;
    info->next = Stress_info_free_list;
    info->prev = head_prev;
    Stress_buffer[head_prev].next = index;
    head_prev = index;
}

// 0x00718350 (:291)
// LTCG: rm in ESI.
static bool stress_evaluation_find_mover(rfg_mover* rm, bool search_high_priority, bool search_low_priority, bool remove_from_q)
{
    cs_scope lock(Stress_mover_queue_wrapper);
    if (search_high_priority && stress_mover_ring_contains(Stress_evaluation_mover_queue, rm))
    {
        if (remove_from_q)
        {
            stress_mover_ring_remove(Stress_evaluation_mover_queue, rm);
        }
        return true;
    }
    if (search_low_priority && stress_mover_ring_contains(Stress_evaluation_low_priority_queue, rm))
    {
        if (remove_from_q)
        {
            stress_mover_ring_remove(Stress_evaluation_low_priority_queue, rm);
        }
        return true;
    }
    return false;
}

// 0x00720FE0 (:330)
// Starts the next mover of the high priority queue (or moves the first low priority one there). Movers in heavy
// destruction (shape flag 0x4) that are not destroyed yet go to the low priority queue instead.
static void stress_evaluation_process_idle()
{
    if (Stress_evaluation_mover_queue == nullptr)
    {
        rfg_mover* rm = Stress_evaluation_low_priority_queue;
        if (rm != nullptr)
        {
            stress_mover_ring_remove(Stress_evaluation_low_priority_queue, rm);
            rm->stress_next = rm;
            rm->stress_prev = rm;
            Stress_evaluation_mover_queue = rm;
        }
        return;
    }
    cs_scope lock(Stress_mover_queue_wrapper);
    rfg_mover* rm = Stress_evaluation_mover_queue;
    Current_mover = rm;
    stress_mover_ring_remove(Stress_evaluation_mover_queue, rm);
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        Current_mover = nullptr;
        return;
    }
    if ((shape->flags & 4) != 0 && !rm->flags.game_destroyed)
    {
        stress_mover_ring_append(Stress_evaluation_low_priority_queue, rm);
        shape->flags |= 0x40;
        Current_mover = nullptr;
        return;
    }
    rm->stress_prev = rm;
    rm->stress_next = rm;
    Current_state = STRESS_EVALUATE_INIT_STAGE_1;
    Stress_sibling_evaluating = true;
    Stress_init_queue = 0xFFFF;
    Current_mover_list = rm;
    while (Stress_info_used_list != 0xFFFF)
    {
        stress_evaluation_info_release(&Stress_buffer[Stress_info_used_list], nullptr);
    }
}

// 0x00728AF0 (:410)
// LTCG: shape in EAX, sp in ECX. Queues a piece for the layer walk: one layer above its parent (depth in the base
// extra data, parity in flag 0x400).
static void stress_evaluation_add_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp, rfg_subpiece* parent_sp)
{
    sp->flags |= 0x200;
    if (parent_sp == nullptr || (parent_sp->flags & 0x400) != 0)
    {
        sp->flags &= ~0x400u;
    }
    else
    {
        sp->flags |= 0x400;
    }
    shape->get_subpiece_base_extra_data(sp)->h = parent_sp != nullptr ? shape->get_subpiece_base_extra_data(parent_sp)->h + 1 : 0; // inlined
    stress_piece_list_append(shape, Stress_init_queue, sp);
}

// 0x00730380 (:439)
// The head of the evaluation: movers supporting it through dynamic links (and not evaluated yet) join the evaluation
// in front of it and are evaluated first (this stage repeats for them). The ground pieces start the layer walk.
static void stress_evaluation_init_object_stage_1()
{
    rfg_mover* rm = Current_mover_list;
    Current_mover = rm;
    rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr; // not checked
    const unsigned short head = shape->child_obj_list;
    rfg_subpiece* root = head != 0xFFFF ? &shape->get_object_buffer()[head] : nullptr;
    if ((shape->flags & 0x30) != 0 || root == nullptr || root->num_links == 0)
    {
        root = nullptr;
    }
    bool added_movers = false;
    shape->flags |= 0x80;
    const bool single_mover = rm == (rm != nullptr ? rm->stress_prev : nullptr);
    if (head != 0xFFFF)
    {
        for (rfg_subpiece* sp = &shape->get_object_buffer()[head]; sp != nullptr; sp = &shape->get_object_buffer()[sp->next])
        {
            sp->flags &= ~0x200u;
            sp->eval_info_index = 0xFFFF;
            object_mover* other_mp;
            rfg_subpiece* other_sp;
            rfg_subpiece_base* other_base;
            if ((shape->flags & 0x300) == 0 && rfg_dyn_link_get_state(Current_mover, sp) == DLS_SUPPORTING && rfg_dyn_link_get_linked_data(Current_mover, sp, other_mp, other_sp, other_base) && other_base != nullptr && (other_base->flags & 0x20) == 0)
            {
                rfg_mover* other = World.object_cast<rfg_mover>(other_mp);
                rfg_destroyable_shape* other_shape = other->rdd.is_loaded() ? other->rdd.get_data()->shape : nullptr; // other not checked
                if (other_shape != nullptr)
                {
                    if (single_mover && !stress_mover_ring_contains(Current_mover_list, other))
                    {
                        other_shape->flags &= ~0x80;
                    }
                    stress_evaluation_find_mover(other, true, true, true); // inlined
                    if ((other_shape->flags & 0x80) == 0)
                    {
                        // in front of the evaluation
                        stress_mover_ring_append(Current_mover_list, other);
                        Current_mover_list = other;
                    }
                    other_shape->flags |= 0x80;
                    added_movers = true;
                    stress_evaluation_info_create(sp);
                }
            }
            if ((sp->flags & 1) != 0 || sp == root || rfg_dyn_link_get_state(Current_mover, sp) == DLS_SUPPORTED)
            {
                stress_evaluation_add_subpiece(shape, sp, nullptr);
            }
            if (sp->next == shape->child_obj_list)
            {
                break;
            }
        }
    }
    shape->flags |= 0x200;
    if (Stress_init_queue == 0xFFFF)
    {
        stress_evaluation_finish_object(true);
        return;
    }
    if (added_movers)
    {
        Stress_init_queue = 0xFFFF; // the new head first
        return;
    }
    Current_state = STRESS_EVALUATE_INIT_STAGE_2;
    Stress_evaluation_list = 0xFFFF;
    Stress_sibling_list = 0xFFFF;
    Stress_sibling_local_list = 0xFFFF;
}

// 0x00728C40 (:553)
// The layer walk: pieces leave the init queue, queue their unvisited linked pieces (not through shatter links, 0x4)
// one layer up and go in front of the evaluation list (so the top layers are processed first).
static void stress_evaluation_init_object_stage_2()
{
    rfg_destroyable_shape* shape = Current_mover->rdd.is_loaded() ? Current_mover->rdd.get_data()->shape : nullptr;
    int count = 0;
    while (Stress_init_queue != 0xFFFF)
    {
        rfg_subpiece* sp = &shape->get_object_buffer()[Stress_init_queue];
        stress_piece_list_remove(shape, Stress_init_queue, sp);
        for (int l = 0; l < sp->num_links; l++)
        {
            const unsigned short link_index = sp->links.to_pointer()[l];
            if ((shape->get_link_base(link_index)->flags & 4) != 0)
            {
                continue;
            }
            rfg_subpiece* osp = stress_linked_subpiece(shape, sp, link_index);
            if ((osp->flags & 0x200) == 0)
            {
                stress_evaluation_add_subpiece(shape, osp, sp);
            }
        }
        stress_piece_list_append(shape, Stress_evaluation_list, sp);
        Stress_evaluation_list = (unsigned short)(sp - shape->get_object_buffer());
        if (Stress_init_queue == 0xFFFF)
        {
            break;
        }
        if (Stress_controls[Current_stress_control].objs_per_frame <= ++count)
        {
            return;
        }
    }
    // sorting the links only (0x100) skips the objects lying on the mover
    Current_state = (stress_evaluation_state)(((shape->flags & 0x100) | 0x400) >> 8);
}

// 0x00718440 (:614)
static bool stress_collect_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    return hit_objp != user_data;
}

// 0x00737910 (:622)
// Up to 100 objects lying on the mover (its bounding box raised by 25 cm) spread their mass over the pieces under
// them.
static void stress_evaluation_collect_objects()
{
    vector bmin;
    vector bmax;
    Current_mover->get_local_bbox(bmin, bmax);
    bmax.y += 0.25f;
    unsigned int handles[100];
    const int num_handles = havok_collect_objects_fast(bmin, bmax, Current_mover->pos, Current_mover->orient, handles, 100, COLLIDE_RFG_STRESS, stress_collect_callback, Current_mover);
    rfg_destroyable_shape* shape = Current_mover->rdd.is_loaded() ? Current_mover->rdd.get_data()->shape : nullptr;
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    for (int i = 0; i < num_handles; i++)
    {
        object* objp = World.object_from_handle(handles[i]);
        if (objp == nullptr)
        {
            continue;
        }
        objp->get_local_bbox(bmin, bmax);
        vector extents;
        vector center;
        bbox_extract_obb_info(bmin, bmax, objp->pos, objp->orient, extents, center);
        center -= Current_mover->pos;
        center.rotate(Current_mover->orient);
        const matrix orient = Current_mover->orient * objp->orient.copy_transpose();
        hits.setSize(0);
        rfg_rbb_node::query_obb(shape, center, orient, extents, hits);
        const int num_hits = hits.getSize();
        if (num_hits == 0)
        {
            continue;
        }
        const float mass_per_piece = objp->get_mass() / (float)num_hits;
        for (int k = 0; k < num_hits; k++)
        {
            stress_evaluation_info* info = stress_evaluation_info_create(&shape->get_object_buffer()[hits[k]]);
            if (info == nullptr)
            {
                break;
            }
            info->total_mass += mass_per_piece;
        }
    }
    Current_state = STRESS_EVALUATE_PROCESS_OBJECT;
}

// 0x00718450 (:692)
// LTCG: sp_bmax in EDX, other_sp_bmin in EDI, other_sp_bmax in ESI. The lever of a load: the distance from the load's
// centre (base_pos) to the contact of the two boxes (the centre of their overlap, or where the line from the other
// box's centre to base_pos leaves it, else default_pos), scaled by how horizontal it is (1 - y^2 of the direction,
// downward directions count as horizontal, at least Stress_min_dot).
static float stress_calc_dist_scalar(const vector& sp_bmin, const vector& sp_bmax, const vector& other_sp_bmin, const vector& other_sp_bmax, const vector& base_pos, const vector& default_pos)
{
    vector contact;
    vector overlap_min;
    vector overlap_max;
    if (ix_box_box_intersect(sp_bmin, sp_bmax, other_sp_bmin, other_sp_bmax, overlap_min, overlap_max))
    {
        contact.set((overlap_min.x + overlap_max.x) * 0.5f, (overlap_min.y + overlap_max.y) * 0.5f, (overlap_min.z + overlap_max.z) * 0.5f);
    }
    else
    {
        const vector center((other_sp_bmin.x + other_sp_bmax.x) * 0.5f, (other_sp_bmin.y + other_sp_bmax.y) * 0.5f, (other_sp_bmin.z + other_sp_bmax.z) * 0.5f);
        if (!ix_linesegment_boundingbox(contact, other_sp_bmin, other_sp_bmax, center, base_pos))
        {
            contact = default_pos;
        }
    }
    vector dir = base_pos - contact;
    const float dist = dir.mag_and_normalize_safe();
    const float up = dir.y - 0.0f < 0.0f ? 0.0f : dir.y;
    float scale = 1.0f - up * up;
    if (0.0f <= Stress_min_dot - scale)
    {
        scale = Stress_min_dot;
    }
    return scale * dist;
}

// 0x00728EA0 (:723)
// The pieces of the evaluation list, top layers first: anchored pieces and pieces no longer in this mover leave; the
// others join the sibling list. A piece gathers its parents (linked pieces of the previous layer, up to 256); when
// their links together are weaker than parent_yield_scalar x its mass, its breakable parent links break. Then its
// mass (with what rests on it) and centre of mass go to its parents, and the force its weight puts on each parent
// link (scaled by the lever, stress_calc_dist_scalar) and the links' strength add up in its accumulator. A piece
// held by a dynamic link loads the other mover's piece the same way. A layer change hands over to the siblings.
// With shape flag 0x100 the pieces only sort their links (up to 100 per frame).
static void stress_evaluation_process_object()
{
    rfg_destroyable_shape* shape = Current_mover->rdd.is_loaded() ? Current_mover->rdd.get_data()->shape : nullptr;
    stress_parent_entry parents[MAX_STRESS_PARENTS];
    int count = 0;
    while (Stress_evaluation_list != 0xFFFF)
    {
        rfg_subpiece* sp = &shape->get_object_buffer()[Stress_evaluation_list];
        stress_piece_list_remove(shape, Stress_evaluation_list, sp);
        if ((sp->flags & 1) != 0)
        {
            sp->flags &= ~0x200u;
            stress_evaluation_info_release(get_stress_eval_info(sp), sp); // inlined
            continue;
        }
        if (sp->get_destroyable() != shape)
        {
            stress_evaluation_info_release(get_stress_eval_info(sp), sp); // inlined
            continue;
        }
        stress_piece_list_append(shape, Stress_sibling_list, sp);
        if ((shape->flags & 0x100) != 0)
        {
            sp->sort_links();
            if (99 < ++count)
            {
                return;
            }
            continue;
        }
        stress_evaluation_info* info = stress_evaluation_info_create(sp);
        int num_parents = 0;
        int parent_yield = 0;
        for (int l = 0; l < sp->num_links; l++)
        {
            const unsigned short link_index = sp->links.to_pointer()[l];
            rfg_subpiece* osp = stress_linked_subpiece(shape, sp, link_index);
            if ((osp->flags & 0x200) == 0 || ((sp->flags ^ osp->flags) & 0x400) == 0)
            {
                continue;
            }
            if (MAX_STRESS_PARENTS - 1 < num_parents)
            {
                break;
            }
            stress_parent_entry& entry = parents[num_parents];
            entry.info = stress_evaluation_info_create(osp);
            entry.sp = osp;
            if (entry.info != nullptr)
            {
                entry.link = l;
                num_parents++;
            }
            int yield = shape->get_link(link_index)->yield_limit;
            if (0x7FFFFFFF - parent_yield <= yield)
            {
                yield = 0x7FFFFFFF - parent_yield; // saturates
            }
            parent_yield += yield;
        }
        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(sp);
        if (0 < num_parents && parent_yield < (int)(Stress_controls[Current_stress_control].parent_yield_scalar * sp_base->mass))
        {
            // too weak to carry the piece: the breakable parent links break
            for (int i = num_parents - 1; 0 <= i; i--)
            {
                unsigned int link_index = sp->links.to_pointer()[parents[i].link];
                if ((shape->get_link_base(link_index)->flags & 2) != 0)
                {
                    continue;
                }
                rfg_dyn_add_pre_collide_link_broken(Current_mover, 1, &link_index, 0, false); // inlined
                num_parents--;
                if (i != num_parents)
                {
                    parents[i] = parents[num_parents];
                }
            }
        }
        const dynamic_link_state link_state = rfg_dyn_link_get_state(Current_mover, sp);
        if (info != nullptr && (sp->num_links != 0 || link_state == DLS_SUPPORTED))
        {
            const float total_mass = info->total_mass + sp_base->mass;
            if (0.0f < total_mass)
            {
                info->total_com.set((sp_base->pos.x * sp_base->mass + info->total_mass * info->total_com.x) / total_mass, (sp_base->pos.y * sp_base->mass + info->total_com.y * info->total_mass) / total_mass, (sp_base->pos.z * sp_base->mass + info->total_com.z * info->total_mass) / total_mass);
                info->total_mass = total_mass / (float)(num_parents < 2 ? 1 : num_parents); // shared by the parents
            }
            const float weight = Stress_controls[Current_stress_control].force_scalar * info->total_mass * GRAVITY;
            if (num_parents < 1)
            {
                object_mover* other_mp;
                rfg_subpiece* other_sp;
                rfg_subpiece_base* other_base;
                if (link_state == DLS_SUPPORTED && rfg_dyn_link_get_linked_data(Current_mover, sp, other_mp, other_sp, other_base))
                {
                    rfg_dynamic_link* rdl = rfg_mover_get_dynamic_link(sp);
                    if (rdl != nullptr)
                    {
                        vector a_min;
                        vector a_max;
                        bbox_instance(a_min, a_max, Current_mover->orient, Current_mover->pos, sp_base->prel_bmin, sp_base->prel_bmax);
                        vector b_min;
                        vector b_max;
                        bbox_instance(b_min, b_max, other_mp->orient, other_mp->pos, other_base->prel_bmin, other_base->prel_bmax);
                        const vector world_com = info->total_com.copy_unrotate(Current_mover->orient) + Current_mover->pos;
                        const vector world_other_pos = other_base->pos.copy_unrotate(other_mp->orient) + other_mp->pos;
                        info->total_force += stress_calc_dist_scalar(a_min, a_max, b_min, b_max, world_com, world_other_pos) * weight;
                        info->total_yield += rdl->yield_limit;
                    }
                    stress_evaluation_info* other_info = get_stress_eval_info(other_sp); // inlined
                    if (other_info != nullptr && 0.0f < total_mass)
                    {
                        other_info->total_com = info->total_com;
                        other_info->total_mass += total_mass;
                    }
                }
            }
            else
            {
                for (int i = 0; i < num_parents; i++)
                {
                    const unsigned short link_index = sp->links.to_pointer()[parents[i].link];
                    const rfg_link* link = shape->get_link(link_index);
                    if (link->yield_limit < 0)
                    {
                        continue;
                    }
                    const rfg_subpiece_base* other_base = shape->get_subpiece_base(parents[i].sp);
                    info->total_force += stress_calc_dist_scalar(sp_base->prel_bmin, sp_base->prel_bmax, other_base->prel_bmin, other_base->prel_bmax, info->total_com, other_base->pos) * weight;
                    info->total_yield += link->yield_limit;
                    stress_evaluation_info* parent_info = parents[i].info;
                    const float mass = parent_info->total_mass + info->total_mass;
                    if (0.0f < mass)
                    {
                        parent_info->total_com.set((parent_info->total_mass * parent_info->total_com.x + info->total_mass * info->total_com.x) / mass, (parent_info->total_com.y * parent_info->total_mass + info->total_com.y * info->total_mass) / mass, (parent_info->total_com.z * parent_info->total_mass + info->total_com.z * info->total_mass) / mass);
                        parent_info->total_mass = mass;
                    }
                }
            }
        }
        if (Stress_evaluation_list == 0xFFFF)
        {
            break;
        }
        if (((shape->get_object_buffer()[Stress_evaluation_list].flags ^ sp->flags) & 0x400) != 0)
        {
            Current_state = STRESS_EVALUATE_PROCESS_SIBLINGS; // end of the layer
            return;
        }
        if (Stress_controls[Current_stress_control].objs_per_frame <= ++count)
        {
            return;
        }
    }
    Current_state = (stress_evaluation_state)(((shape->flags & 0x100) | 0x600) >> 8); // siblings, or finish when sorting
}

// Takes a broken child's load back out of a parent's accumulator.
static void stress_remove_child_load(stress_evaluation_info* parent_info, const stress_evaluation_info* info)
{
    const float mass = parent_info->total_mass - info->total_mass;
    parent_info->total_mass = mass;
    if (mass <= 0.0f)
    {
        parent_info->total_com.zero();
        return;
    }
    const float old_mass = mass + info->total_mass;
    parent_info->total_com.set((old_mass * parent_info->total_com.x - info->total_mass * info->total_com.x) / mass, (parent_info->total_com.y * old_mass - info->total_com.y * info->total_mass) / mass, (parent_info->total_com.z * old_mass - info->total_com.z * info->total_mass) / mass);
}

// 0x00730700 (:965)
// Alternates between gathering a group (the pieces of the layer connected to the first piece of the sibling list,
// summing their force and strength; pieces flagged indestructible, base flag 0x20, add nothing) and breaking it:
// when the group is overloaded (force over strength) each piece breaks its breakable parent links and its dynamic
// link (event flag 0x20) and its load leaves its parents. One random piece of the group plays the stress effects
// (size from min(force, strength) x force / strength, also limited by the material's stress sizes and, for a mover
// under stress already, by its remaining stress tests), once the mover is destroyed enough (Stress_min_percent_destroyed;
// near its fully destroyed percentage overloaded groups also restart the stress tests). Destroyed shatterable movers
// (0x1000) shatter there.
static void stress_evaluation_process_siblings()
{
    rfg_destroyable_shape* shape = Current_mover->rdd.is_loaded() ? Current_mover->rdd.get_data()->shape : nullptr;
    int count = 0;
    float ratio = 0.0f;
    const float pct_destroyed = Current_mover->percent_destroyed();
    float fully_destroyed_pct = Current_mover->gameplay_props->fully_destroyed_pct;
    if (fully_destroyed_pct == -1.0f)
    {
        fully_destroyed_pct = 0.5f; // default
    }
    if (0.0f <= Current_mover->game_destroyed_pct - fully_destroyed_pct)
    {
        fully_destroyed_pct = Current_mover->game_destroyed_pct;
    }
    bool can_play_effects = false;
    bool near_fully_destroyed = false;
    if (fully_destroyed_pct - Stress_min_percent_destroyed < pct_destroyed)
    {
        near_fully_destroyed = true;
        can_play_effects = true;
    }
    else if (Stress_min_percent_destroyed < pct_destroyed)
    {
        can_play_effects = true;
    }
    for (;;)
    {
        if (Stress_sibling_list == 0xFFFF && Stress_sibling_local_list == 0xFFFF)
        {
            Current_state = Stress_evaluation_list == 0xFFFF ? STRESS_EVALUATE_FINISH : STRESS_EVALUATE_PROCESS_OBJECT;
            return;
        }
        if (!Stress_sibling_evaluating)
        {
            // break the group
            while (Stress_sibling_local_list != 0xFFFF)
            {
                rfg_subpiece* sp = &shape->get_object_buffer()[Stress_sibling_local_list];
                stress_piece_list_remove(shape, Stress_sibling_local_list, sp);
                stress_evaluation_info* info = get_stress_eval_info(sp);
                if (info != nullptr)
                {
                    if (0 < Stress_sibling_event_count && --Stress_sibling_event_count == 0)
                    {
                        // this piece plays the group's stress effects
                        float yield = (float)Stress_sibling_total_yield;
                        ratio = Stress_sibling_total_force / yield;
                        if (ratio - 0.0f < 0.0f)
                        {
                            ratio = 0.0f;
                        }
                        if (0.0f <= ratio - 1.0f)
                        {
                            ratio = 1.0f;
                        }
                        if (Stress_sibling_total_force - yield < 0.0f)
                        {
                            yield = Stress_sibling_total_force;
                        }
                        const float force = yield * ratio;
                        material_event_size size;
                        if (Stress_event_large_force < force)
                        {
                            size = MATERIAL_EVENT_SIZE_LARGE;
                        }
                        else if (Stress_event_medium_force < force)
                        {
                            size = MATERIAL_EVENT_SIZE_MEDIUM;
                        }
                        else if (Stress_event_small_force < force)
                        {
                            size = MATERIAL_EVENT_SIZE_SMALL;
                        }
                        else
                        {
                            size = INVALID_MATERIAL_EVENT_SIZE;
                        }
                        const unsigned int key = (unsigned int)(sp - shape->get_object_buffer());
                        const rfg_subpiece_base* sp_base = shape->get_subpiece_base(key);
                        const unsigned char effect_material = rfg_get_effect_material_index(sp_base->physical_material_index); // inlined
                        const material_event_size material_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_STRESS, ratio); // inlined
                        if (material_size <= size)
                        {
                            size = material_size;
                        }
                        if (size != INVALID_MATERIAL_EVENT_SIZE)
                        {
                            if (!Current_mover->flags.under_stress)
                            {
                                Current_mover->flags.under_stress = 1;
                                Current_mover->stress_test_count += (short)int_rand_range(size * 3 + 1, size * 3 + 5);
                                if (Current_mover->stress_test_count < 1)
                                {
                                    Current_mover->stress_test_count = 1;
                                }
                                else if (9 < Current_mover->stress_test_count)
                                {
                                    Current_mover->stress_test_count = 9;
                                }
                            }
                            else
                            {
                                const float remaining = (float)Current_mover->stress_test_count / 9.0f;
                                const material_event_size remaining_size = 0.66f < remaining ? MATERIAL_EVENT_SIZE_LARGE : (0.33f < remaining ? MATERIAL_EVENT_SIZE_MEDIUM : MATERIAL_EVENT_SIZE_SMALL);
                                if (remaining_size <= size)
                                {
                                    size = remaining_size;
                                }
                            }
                            if (!Current_mover->flags.is_ai_subpiece && 0.0f < ratio)
                            {
                                Current_mover->accumulated_load_amount += ratio;
                                if (Current_mover->stress_sound_propagate_timer.elapsed())
                                {
                                    npc_sound_info_stress sound_info(Current_mover, Current_mover->accumulated_load_amount);
                                    npc_sound_info::queue<npc_sound_info_stress>(sound_info);
                                    Current_mover->accumulated_load_amount = 0.0f;
                                    Current_mover->stress_sound_propagate_timer.set_rand(1000, 1200);
                                }
                            }
                            vector pos;
                            matrix orient;
                            shape->get_subpiece_pos_orient_uncached(sp, pos, orient);
                            camera_shake_start_from_stress(force, vec_dist_squared(pos, Rfg_camera.real_pos));
                            if (Current_mover->flags.game_destroyed && (shape->flags & 0x1000) != 0)
                            {
                                // kept: the event names no piece (shape key -1)
                                rfg_dyn_add_pre_collide_shatter(Current_mover, nullptr, 0xFFFFFFFF, (unsigned int)(long long)sqrtf(force), sp_base->pos, ZERO_VECTOR, false); // inlined
                            }
                            const unsigned int delay = rand() % 10000;
                            rfg_effect_candidate_create(effect_material, pos, MATERIAL_EVENT_STRESS, size, rfg_material_get_shape_type(Current_mover, key), false, delay, nullptr, Current_mover, key, 0xFF, 0xFF);
                        }
                    }
                    if (Stress_sibling_total_yield < (unsigned int)(long long)Stress_sibling_total_force)
                    {
                        // overloaded: the parent links break
                        for (int l = sp->num_links - 1; 0 <= l; l--)
                        {
                            unsigned int link_index = sp->links.to_pointer()[l];
                            if ((shape->get_link_base(link_index)->flags & 2) != 0)
                            {
                                continue;
                            }
                            rfg_subpiece* parent = stress_linked_subpiece(shape, sp, (unsigned short)link_index);
                            if ((parent->flags & 0x200) == 0 || ((sp->flags ^ parent->flags) & 0x400) == 0)
                            {
                                continue;
                            }
                            rfg_dyn_add_pre_collide_link_broken(Current_mover, 1, &link_index, 0, false); // inlined
                            if (near_fully_destroyed)
                            {
                                Current_mover->flags.under_stress = 1;
                                if (Current_mover->stress_test_count < 2)
                                {
                                    Current_mover->stress_test_count = 1;
                                }
                            }
                            stress_evaluation_info* parent_info = get_stress_eval_info(parent); // inlined
                            if (parent_info != nullptr)
                            {
                                stress_remove_child_load(parent_info, info);
                            }
                        }
                        object_mover* other_mp;
                        rfg_subpiece* other_sp;
                        rfg_subpiece_base* other_base;
                        if (rfg_dyn_link_get_linked_data(Current_mover, sp, other_mp, other_sp, other_base))
                        {
                            stress_evaluation_info* other_info = other_sp != nullptr ? get_stress_eval_info(other_sp) : nullptr; // inlined
                            if (other_info != nullptr)
                            {
                                stress_remove_child_load(other_info, info);
                            }
                            unsigned int key = (unsigned int)(sp - shape->get_object_buffer());
                            rfg_dyn_add_pre_collide_link_broken(Current_mover, 1, &key, 0x20, false); // inlined, the dynamic link
                        }
                    }
                    stress_evaluation_info_release(info, sp);
                    if (Stress_sibling_local_list == 0xFFFF)
                    {
                        break;
                    }
                    if (Stress_controls[Current_stress_control].objs_per_frame <= ++count)
                    {
                        return;
                    }
                }
            }
            Stress_sibling_evaluating = true;
            continue;
        }
        // gather the next group
        rfg_subpiece* sp = Stress_sibling_local_last_sp;
        if (Stress_sibling_local_list == 0xFFFF)
        {
            sp = &shape->get_object_buffer()[Stress_sibling_list];
            stress_piece_list_remove(shape, Stress_sibling_list, sp);
            stress_piece_list_append(shape, Stress_sibling_local_list, sp);
            sp->flags &= ~0x200u;
            Stress_sibling_total_force = 0.0f;
            Stress_sibling_total_yield = 0;
            Stress_sibling_count = 0;
            Stress_sibling_event_count = 0;
            Stress_sibling_local_last_sp = sp;
        }
        for (; sp != nullptr; sp = &shape->get_object_buffer()[sp->stress_next])
        {
            stress_evaluation_info* info = get_stress_eval_info(sp);
            if (info != nullptr)
            {
                if ((shape->get_subpiece_base(sp)->flags & 0x20) == 0)
                {
                    Stress_sibling_total_yield += info->total_yield;
                    Stress_sibling_total_force += info->total_force;
                }
                Stress_sibling_count++;
                for (int l = 0; l < sp->num_links; l++)
                {
                    rfg_subpiece* osp = stress_linked_subpiece(shape, sp, sp->links.to_pointer()[l]);
                    if ((osp->flags & 0x200) == 0 || osp->get_destroyable() != shape || ((sp->flags ^ osp->flags) & 0x400) != 0)
                    {
                        continue;
                    }
                    osp->flags &= ~0x200u;
                    stress_piece_list_remove(shape, Stress_sibling_list, osp);
                    stress_piece_list_append(shape, Stress_sibling_local_list, osp);
                }
                if (Stress_sibling_local_list != 0xFFFF && Stress_controls[Current_stress_control].objs_per_frame <= ++count)
                {
                    Stress_sibling_local_last_sp = sp->stress_next != Stress_sibling_local_list ? &shape->get_object_buffer()[sp->stress_next] : nullptr;
                    return;
                }
            }
            if (sp->stress_next == Stress_sibling_local_list)
            {
                break;
            }
        }
        if (Stress_sibling_total_yield != 0 && can_play_effects)
        {
            Stress_sibling_event_count = int_rand_range(1, Stress_sibling_count);
        }
        Stress_sibling_evaluating = false;
    }
}

// 0x00729B20 (:1294)
// The mover leaves the evaluation and is tested again later: after repeat_delay_max, or sooner the more stress tests
// it has left (randomized by +-50%); a mover out of stress tests is no longer under stress. A mover asked to be
// evaluated again (0x40) is queued again. Then the next mover of the evaluation, or idle.
static void stress_evaluation_finish_object(bool can_evaluate_again)
{
    if (Current_mover != nullptr)
    {
        cs_scope lock(Stress_mover_queue_wrapper);
        rfg_mover* rm = Current_mover;
        stress_mover_ring_remove(Current_mover_list, rm);
        const stress_evaluation_controls& controls = Stress_controls[Current_stress_control];
        if (!rm->flags.under_stress)
        {
            rm->stress_test_delay.set(controls.repeat_delay_max);
        }
        else if (rm->stress_test_count < 1)
        {
            rm->flags.under_stress = 0;
        }
        else
        {
            const int delay = (int)((1.0f - (float)rm->stress_test_count / 9.0f) * (float)(controls.repeat_delay_max - controls.repeat_delay_min)) + controls.repeat_delay_min;
            rm->stress_test_delay.set_rand(delay - delay / 2, delay / 2 + delay);
        }
        rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
        if (shape != nullptr)
        {
            shape->flags &= ~0x300;
            if (can_evaluate_again && (shape->flags & 0x40) != 0)
            {
                shape->flags &= ~0x40;
                stress_evaluation_start(rm, false, false); // inlined
            }
        }
    }
    if (Current_mover_list != nullptr)
    {
        Current_mover = Current_mover_list;
        Current_state = STRESS_EVALUATE_INIT_STAGE_1;
        Stress_init_queue = 0xFFFF;
        return;
    }
    Current_mover = nullptr;
    Current_state = STRESS_EVALUATE_IDLE;
}

// 0x007186D0 (:1356)
static int stress_evaluation_find_configuration_by_name(const char* name)
{
    for (int i = 0; i < Num_stress_control_configs; i++)
    {
        if (v_stricmp(name, Stress_controls[i].name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x007211C0 (:1369)
// rfg_stress_controls.xtbl: up to 4 Stress_Control entries; "Default" is the current one.
static void stress_evaluation_load_table()
{
    xml_element* root = xml_table_parse("rfg_stress_controls.xtbl");
    for (xml_element* element = xml_table_find_opt(root, "Stress_Control"); element != nullptr && Num_stress_control_configs < MAX_STRESS_CONTROLS; element = xml_table_find_next(root, element, "Stress_Control"))
    {
        stress_evaluation_controls& controls = Stress_controls[Num_stress_control_configs++];
        xml_table_get_req_string(controls.name, sizeof(controls.name), element, "Name");
        xml_table_get_req_int(controls.objs_per_frame, element, "Objects_Per_Frame");
        xml_table_get_req_int(controls.repeat_delay_min, element, "Repeat_Delay_Min");
        xml_table_get_req_int(controls.repeat_delay_max, element, "Repeat_Delay_Max");
        xml_table_get_req_float(controls.force_scalar, element, "Force_Scalar");
        xml_table_get_req_float(controls.parent_yield_scalar, element, "Parent_Yield_Scalar");
    }
    xml_table_close(root);
    Default_stress_control = stress_evaluation_find_configuration_by_name("Default"); // inlined
    Current_stress_control = Default_stress_control;
}

// 0x007212E0 (:1539)
void stress_evaluation_system_init()
{
    stress_evaluation_load_table(); // inlined
}

// 0x00729E90 (:1554)
// Every accumulator goes to the free list.
void stress_evaluation_level_init_pre()
{
    Current_mover = nullptr;
    Current_mover_list = nullptr;
    stress_evaluation_finish_object(true);
    Stress_info_used_list = 0xFFFF;
    Stress_evaluation_mover_queue = nullptr;
    Stress_evaluation_low_priority_queue = nullptr;
    unsigned short free_list = 0xFFFF;
    for (int i = 0; i < MAX_STRESS_INFOS; i++)
    {
        stress_evaluation_info& info = Stress_buffer[i];
        if (free_list == 0xFFFF)
        {
            free_list = (unsigned short)i;
            info.next = free_list;
            info.prev = free_list;
        }
        else
        {
            info.prev = Stress_buffer[free_list].prev;
            info.next = free_list;
            Stress_buffer[Stress_buffer[free_list].prev].next = (unsigned short)i;
            Stress_buffer[free_list].prev = (unsigned short)i;
        }
    }
    Stress_info_free_list = free_list;
}

// 0x007212F0 (:1578)
// Queues a fixed mover for evaluation (not when queued already; one in the current evaluation is asked to evaluate
// again, 0x40). sort_links: single player only, to the low priority queue to sort its links (0x100). Otherwise to the
// high priority queue (out of the low priority one), by distance to the camera when priority.
bool stress_evaluation_start(rfg_mover* mover, bool sort_links, bool priority)
{
    if (!gameplay_should_run_stress() || mover == nullptr || (mover->get_move_type() != RFGM_FIXED && !sort_links) || mover->obj_flags.destroyed)
    {
        return false;
    }
    rfg_destroyable_shape* shape = mover->rdd.is_loaded() ? mover->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return false;
    }
    cs_scope lock(Stress_mover_queue_wrapper);
    if (stress_evaluation_find_mover(mover, true, sort_links, false)) // inlined
    {
        return false;
    }
    if (stress_mover_ring_contains(Current_mover_list, mover))
    {
        shape->flags |= 0x40;
        return false;
    }
    if (sort_links)
    {
        if (Multiplayer)
        {
            return false;
        }
        shape->flags |= 0x100;
        stress_mover_ring_append(Stress_evaluation_low_priority_queue, mover);
        return true;
    }
    if (stress_evaluation_find_mover(mover, false, true, true) && (shape->flags & 0x100) != 0) // inlined
    {
        shape->flags |= 0x40;
    }
    if (priority && Stress_evaluation_mover_queue != nullptr)
    {
        // before the first mover farther from the camera
        const float dist = vec_dist_squared(mover->pos, Rfg_camera.real_pos);
        rfg_mover* it = Stress_evaluation_mover_queue;
        while (vec_dist_squared(it->pos, Rfg_camera.real_pos) <= dist)
        {
            it = it->stress_next;
            if (it == Stress_evaluation_mover_queue || it == nullptr)
            {
                it = nullptr;
                break;
            }
        }
        if (it != nullptr)
        {
            it->stress_prev->stress_next = mover;
            mover->stress_prev = it->stress_prev;
            it->stress_prev = mover;
            mover->stress_next = it;
            if (Stress_evaluation_mover_queue == it)
            {
                Stress_evaluation_mover_queue = mover;
            }
            return true;
        }
    }
    stress_mover_ring_append(Stress_evaluation_mover_queue, mover);
    return true;
}

// 0x0072A200 (:1675)
// Ends the evaluation when the mover is in it (the whole evaluation, without evaluating again), or takes it out of
// the queues. From the low priority queue it returns whether it was asked to evaluate again.
bool stress_evaluation_stop(rfg_mover* mover)
{
    cs_scope lock(Stress_mover_queue_wrapper);
    if (stress_mover_ring_contains(Current_mover_list, mover))
    {
        while (Current_mover != nullptr)
        {
            stress_evaluation_finish_object(false);
        }
        return true;
    }
    if (stress_mover_ring_contains(Stress_evaluation_mover_queue, mover))
    {
        stress_mover_ring_remove(Stress_evaluation_mover_queue, mover);
        return true;
    }
    if (stress_mover_ring_contains(Stress_evaluation_low_priority_queue, mover))
    {
        stress_mover_ring_remove(Stress_evaluation_low_priority_queue, mover);
        rfg_destroyable_shape* shape = mover->rdd.is_loaded() ? mover->rdd.get_data()->shape : nullptr;
        return shape != nullptr && (shape->flags & 0x40) != 0;
    }
    return false;
}

// 0x00721600 (:1707)
// A fixed mover not under stress whose test delay is up (or unset) is tested now (the delay restarts).
bool stress_evaluation_poke(rfg_mover* mover)
{
    if (!gameplay_should_run_stress())
    {
        return false;
    }
    if (mover == nullptr || mover->get_move_type() != RFGM_FIXED || mover->flags.under_stress)
    {
        return false;
    }
    if (mover->stress_test_delay.valid() && !mover->stress_test_delay.elapsed())
    {
        return false;
    }
    mover->stress_test_delay.set(Stress_controls[Current_stress_control].repeat_delay_max);
    stress_evaluation_start(mover, false, false); // inlined
    return true;
}

// 0x0072A3D0 (:1738)
bool stress_evaluation_is_active(unsigned int handle)
{
    return stress_evaluation_is_active(World.object_from_handle<rfg_mover>(handle));
}

// 0x00721860 (:1743)
// The mover (or one of its attached rfg_movers) is evaluated or queued (low priority only when asked to evaluate
// again).
bool stress_evaluation_is_active(rfg_mover* mover)
{
    if (mover == nullptr)
    {
        return false;
    }
    object* child = mover->child_ptr;
    while (child != nullptr)
    {
        if (stress_evaluation_is_active(World.object_cast<rfg_mover>(child)))
        {
            return true;
        }
        child = child->child_next;
        if (child == mover->child_ptr)
        {
            break;
        }
    }
    if (stress_mover_ring_contains(Current_mover_list, mover))
    {
        return true;
    }
    rfg_destroyable_shape* shape = mover->rdd.is_loaded() ? mover->rdd.get_data()->shape : nullptr;
    const bool evaluate_again = shape != nullptr && (shape->flags & 0x40) != 0;
    return stress_evaluation_find_mover(mover, true, evaluate_again, false); // inlined
}

// 0x00737BB0 (:1775)
void stress_evaluation_do_frame()
{
    if (!gameplay_should_run_stress())
    {
        return;
    }
    switch (Current_state)
    {
    case STRESS_EVALUATE_IDLE:
        stress_evaluation_process_idle();
        break;
    case STRESS_EVALUATE_INIT_STAGE_1:
        stress_evaluation_init_object_stage_1();
        break;
    case STRESS_EVALUATE_INIT_STAGE_2:
        stress_evaluation_init_object_stage_2();
        break;
    case STRESS_EVALUATE_COLLECT_OBJECTS:
        stress_evaluation_collect_objects();
        break;
    case STRESS_EVALUATE_PROCESS_OBJECT:
        stress_evaluation_process_object();
        break;
    case STRESS_EVALUATE_PROCESS_SIBLINGS:
        stress_evaluation_process_siblings();
        break;
    case STRESS_EVALUATE_FINISH:
        stress_evaluation_finish_object(true);
        break;
    default:
        break;
    }
}

// 0x00718710 (:1858)
// "" selects the default controls.
bool stress_evaluation_set_mode(const char* mode_name)
{
    int index = Default_stress_control;
    if (*mode_name != '\0' && (index = stress_evaluation_find_configuration_by_name(mode_name)) == -1)
    {
        return false;
    }
    Current_stress_control = index;
    return true;
}
