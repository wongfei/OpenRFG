// Original: volition/rfg/code/pathfinding/nav_cell_detour.cpp
//
// Nav cell detours (stub_impl.md S9). Movers standing on the mesh file requests; each request gets a 2D convex hull, and
// hulls that touch merge into compound hulls registered on the cells they cover. All pools are static arrays threaded
// by indices (next/prev circular lists, 0xFFFF = none), like rfg.exe's vlist macros. rfg.exe's "Int3 critical function"
// prints are left out: they only log.

#include "volition/rfg/code/pathfinding/nav_cell_detour.h"

#include <math.h>
#include <string.h>

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

// size 0xC
struct detour_navpoint_block_ref
{
    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    navpoint* navp; // 0x04
    unsigned short hull_ref; // 0x08
    struct
    {
        unsigned short point_disabled : 1;
        unsigned short link_disabled : 10;
    } blocked_list; // 0x0A
};

// size 0xC
struct detour_action_node_block_ref
{
    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    object_action_node* node; // 0x04
    unsigned short hull_ref; // 0x08
};

// size 0xC
struct detour_guard_node_block_ref
{
    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    object_guard_node* node; // 0x04
    unsigned short hull_ref; // 0x08
};

static const unsigned short NCD_NONE = 0xFFFF;

// Magic numbers: rfg.exe's pool sizes.
static const int NCD_MAX_COMPOUND_HULL_REFERENCES = 768;
static const int NCD_MAX_NAVPOINT_BLOCK_REFS = 500;
static const int NCD_MAX_NODE_BLOCK_REFS = 75;
static const int NCD_MAX_COMPOUND_HULL_VERTS = 1024;
static const int NCD_MAX_REQUESTS = 4096;
static const int NCD_MAX_CONVEX_HULLS = 512;
static const int NCD_MAX_COMPOUND_HULLS = 256;

// Magic number: the navpoint_disabled_by bit of a detour hull.
static const unsigned char NCD_NAVPOINT_DISABLED_BIT = 4;

static float Ncd_mover_active_dist_from_camera = 60.0f; // 0x016B0190
static unsigned short Unincorporated_convex_hulls = NCD_NONE; // 0x016B0194
static unsigned short Free_convex_hulls = NCD_NONE; // 0x016B0198
static unsigned short Free_compound_hull_verts = NCD_NONE; // 0x016B019C
static unsigned short Active_compound_hulls = NCD_NONE; // 0x016B01A0
static unsigned short Free_compound_hulls = NCD_NONE; // 0x016B01A4
static unsigned short Active_compound_hull_references = NCD_NONE; // 0x016B01A8
static unsigned short Free_compound_hull_references = NCD_NONE; // 0x016B01AC
static unsigned short Active_nav_cell_detour_requests = NCD_NONE; // 0x016B01B0
static unsigned short Free_nav_cell_detour_requests = NCD_NONE; // 0x016B01B4
unsigned short Free_detour_navpoint_block_refs = NCD_NONE; // 0x016B01B8
unsigned short Active_detour_navpoint_block_refs = NCD_NONE; // 0x016B01BC
unsigned short Free_detour_action_node_block_refs = NCD_NONE; // 0x016B01C0
unsigned short Active_detour_action_node_block_refs = NCD_NONE; // 0x016B01C4
unsigned short Free_detour_guard_node_block_refs = NCD_NONE; // 0x016B01C8
unsigned short Active_detour_guard_node_block_refs = NCD_NONE; // 0x016B01CC
static nav_cell_detour_activation_listener Nav_cell_detour_activation_listener; // 0x016B01E4
static unsigned int Next_nav_cell_request_identifier;
// rfg_camera.cpp reads this one.
const vehicle_info* Nav_cell_detour_veh_info_mar_raider_1; // 0x035B6080
static const vehicle_info* Nav_cell_detour_veh_info_walker_flamer; // 0x035BB63C
static const vehicle_info* Nav_cell_detour_veh_info_walker_min_1; // 0x035B9694
static const vehicle_info* Nav_cell_detour_veh_info_walker_min_2; // 0x035B6050
static const vehicle_info* Nav_cell_detour_veh_info_walker_min_3; // 0x035BB9C4
static int Next_compound_hull_identifier; // 0x035B6054
static compound_hull_ref Compound_hull_references[NCD_MAX_COMPOUND_HULL_REFERENCES]; // 0x035B6088
static detour_navpoint_block_ref Detour_navpoint_block_refs[NCD_MAX_NAVPOINT_BLOCK_REFS]; // 0x035B9698
static detour_guard_node_block_ref Detour_guard_node_block_refs[NCD_MAX_NODE_BLOCK_REFS]; // 0x035BB640
static detour_action_node_block_ref Detour_action_node_block_refs[NCD_MAX_NODE_BLOCK_REFS]; // 0x035BB9D8
static compound_hull_vert Compound_hull_verts[NCD_MAX_COMPOUND_HULL_VERTS]; // 0x035BBD80
static nav_cell_detour_request Nav_cell_detour_requests[NCD_MAX_REQUESTS]; // 0x035D1D80
static convex_hull_2d Convex_hulls[NCD_MAX_CONVEX_HULLS]; // 0x035F1D80
static compound_hull Compound_hulls[NCD_MAX_COMPOUND_HULLS]; // 0x0360E320

// The circular index lists: T has next and prev.
template<typename T>
static void ncd_list_remove(T* pool, unsigned short& head, unsigned short index)
{
    T& entry = pool[index];
    if (head == index && (head = entry.next, head == index)) {
        head = NCD_NONE;
    } else {
        pool[entry.prev].next = entry.next;
        pool[entry.next].prev = entry.prev;
    }
    entry.next = NCD_NONE;
    entry.prev = NCD_NONE;
}

template<typename T>
static void ncd_list_add(T* pool, unsigned short& head, unsigned short index)
{
    T& entry = pool[index];
    if (head == NCD_NONE) {
        entry.next = index;
        entry.prev = index;
        head = index;
    } else {
        entry.prev = pool[head].prev;
        entry.next = head;
        pool[pool[head].prev].next = index;
        pool[head].prev = index;
    }
}

// Appends the circular list other to the circular list head.
template<typename T>
static void ncd_list_merge(T* pool, unsigned short& head, unsigned short other)
{
    if (other == NCD_NONE) {
        return;
    }
    if (head == NCD_NONE) {
        head = other;
        return;
    }
    unsigned short head_tail = pool[head].prev;
    unsigned short other_tail = pool[other].prev;
    pool[head_tail].next = other;
    pool[other_tail].next = head;
    pool[other].prev = head_tail;
    pool[head].prev = other_tail;
}

static unsigned short ncd_hull_index(const compound_hull* cmp_hull)
{
    return (unsigned short)(cmp_hull - Compound_hulls);
}

// The vertex after vert, the list wrapping to its start (the "next or head" every edge walk below uses).
static compound_hull_vert* ncd_next_vert(const compound_hull_vert* vert)
{
    return Compound_hull_verts + vert->next;
}

// 0x00AED2C0 (:173)
convex_hull_intersection::convex_hull_intersection()
{
    ix_valid = false;
    ix_pos = ZERO_VECTOR2;
    hull0 = nullptr;
    hull0_vert_indices[1] = 0;
    hull0_vert_indices[0] = 0;
    hull1 = nullptr;
    hull1_vert_indices[1] = 0;
    hull1_vert_indices[0] = 0;
}

// 0x00AED2F0 (:194)
compound_hull_intersection::compound_hull_intersection()
{
    ix_valid = false;
    ix_pos = ZERO_VECTOR2;
    hull0 = nullptr;
    hull0_verts[1] = nullptr;
    hull0_verts[0] = nullptr;
    hull1_verts[1] = nullptr;
    hull1_verts[0] = nullptr;
}

// 0x00AED320
convex_hull_2d::convex_hull_2d()
{
}

// 0x00AED330
compound_hull_vert::compound_hull_vert()
{
}

// 0x00AED340
compound_hull::compound_hull()
{
}

// 0x00AED350
nav_cell_detour_request::nav_cell_detour_request()
{
}

// 0x00AED360 (:418)
bool ncd_ix_edge_edge(const vector2& edge0_vert0, const vector2& edge0_vert1, const vector2& edge1_vert0, const vector2& edge1_vert1, vector2* ix_pos)
{
    vector2 edge1(edge1_vert1.x - edge1_vert0.x, edge1_vert1.y - edge1_vert0.y);
    float edge0_dx = edge0_vert1.x - edge0_vert0.x;
    float edge0_dy = edge0_vert1.y - edge0_vert0.y;
    float denominator = edge0_dx * edge1.y - edge0_dy * edge1.x;
    // Magic numbers: rfg.exe's tolerances; a pair running against each other (negative denominator) never counts.
    if (denominator < -0.0001f) {
        return false;
    }
    float offset_x = edge0_vert0.x - edge1_vert0.x;
    float offset_y = edge0_vert0.y - edge1_vert0.y;
    float numerator1 = offset_y * edge0_dx - edge0_dy * offset_x;
    if (0.0001f <= denominator) {
        float t1 = numerator1 / denominator;
        if (t1 < -0.005f || 0.995f <= t1) {
            return false;
        }
        float t0 = (offset_y * edge1.x - edge1.y * offset_x) / denominator;
        if (t0 < -0.005f || 0.995f <= t0) {
            return false;
        }
        if (ix_pos != nullptr) {
            ix_pos->x = edge0_vert0.x + edge0_dx * t0;
            ix_pos->y = edge0_vert0.y + edge0_dy * t0;
        }
        return true;
    }
    if (fabsf(numerator1) >= 0.0001f) {
        return false;
    }
    float length = edge1.mag_and_normalize_safe();
    if (length == 0.0f) {
        return false;
    }
    float along = edge1.y * offset_y + offset_x * edge1.x;
    if (along < 0.0f || length <= along) {
        return false;
    }
    if (ix_pos != nullptr) {
        ix_pos->x = edge1_vert0.x + edge1.x * along;
        ix_pos->y = edge1_vert0.y + edge1.y * along;
    }
    return true;
}

// 0x00AF9600 (:488)
static object_mover* nav_cell_detour_mover_from_entity(hkpEntity* entity)
{
    object* objp = object_from_world_obj(entity);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER) {
        return nullptr;
    }
    return (object_mover*)objp;
}

// 0x00AED6D0 (:514)
static bool ncd_object_is_flat(object* objp)
{
    vector bmin, bmax;
    objp->get_bbox(bmin, bmax);
    // Magic number: rfg.exe's height a human steps over.
    return bmax.y - bmin.y < 0.65f;
}

// 0x00AF6FB0 (:521) LTCG: om ESI
static void nav_cell_detour_mover_deactivate(object_mover* om)
{
    if (om->obj_flags.destroyed == 0 && om->nav_cell_detour_request_handle == 0xFFFFFFFF) {
        om->nav_cell_detour_request_handle = nav_cell_detour_add_request(om->get_handle(), -1);
    }
}

// 0x00AF9640 (:532)
static void nav_cell_detour_mover_activate(object_mover* om)
{
}

// 0x00AED720 (:544) LTCG: navp ESI, the rest on the stack
static void ncd_add_navpoint_block_ref(navpoint* navp, compound_hull* cmp_hull, bool blocks_point, short blocked_links)
{
    if (Free_detour_navpoint_block_refs == NCD_NONE) {
        return;
    }
    unsigned short index = Free_detour_navpoint_block_refs;
    ncd_list_remove(Detour_navpoint_block_refs, Free_detour_navpoint_block_refs, index);
    detour_navpoint_block_ref& ref = Detour_navpoint_block_refs[index];
    ref.navp = navp;
    ref.blocked_list.point_disabled = blocks_point ? 1 : 0;
    ref.blocked_list.link_disabled = (unsigned short)blocked_links & 0x3FF;
    ref.hull_ref = ncd_hull_index(cmp_hull);
    ncd_list_add(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, index);
    if (blocks_point) {
        navp->heap_data.disabled_by |= NCD_NAVPOINT_DISABLED_BIT;
    }
    for (int i = 0; i < 10; i++) {
        if ((blocked_links & (1 << i)) != 0) {
            navp->link_disabled_by[i] |= NCD_NAVPOINT_DISABLED_BIT;
        }
    }
}

// 0x00AED960 (:575)
static void ncd_remove_navpoint_block_refs(compound_hull* cmp_hull)
{
    unsigned short active_at_start = Active_detour_navpoint_block_refs;
    detour_navpoint_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_navpoint_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_navpoint_block_ref* current = ref;
        ref = current->next == active_at_start ? nullptr : Detour_navpoint_block_refs + current->next;
        if (Compound_hulls + current->hull_ref != cmp_hull) {
            continue;
        }
        unsigned short index = (unsigned short)(current - Detour_navpoint_block_refs);
        ncd_list_remove(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, index);
        ncd_list_add(Detour_navpoint_block_refs, Free_detour_navpoint_block_refs, index);
        active_at_start = Active_detour_navpoint_block_refs;

        // What the node's other hulls still block.
        bool point_blocked = false;
        unsigned short links_blocked = 0;
        if (active_at_start != NCD_NONE) {
            detour_navpoint_block_ref* other = Detour_navpoint_block_refs + active_at_start;
            for (;;) {
                if (other->navp == current->navp && other->hull_ref != current->hull_ref) {
                    if (other->blocked_list.point_disabled != 0) {
                        point_blocked = true;
                    }
                    links_blocked |= other->blocked_list.link_disabled;
                }
                if (other->next == active_at_start) {
                    break;
                }
                other = Detour_navpoint_block_refs + other->next;
            }
        }
        navpoint* navp = current->navp;
        if (point_blocked) {
            navp->heap_data.disabled_by |= NCD_NAVPOINT_DISABLED_BIT;
        } else {
            navp->heap_data.disabled_by &= ~NCD_NAVPOINT_DISABLED_BIT;
        }
        for (int i = 0; i < 10; i++) {
            if ((links_blocked & (1 << i)) != 0) {
                navp->link_disabled_by[i] |= NCD_NAVPOINT_DISABLED_BIT;
            } else {
                navp->link_disabled_by[i] &= ~NCD_NAVPOINT_DISABLED_BIT;
            }
        }
        current->navp = nullptr;
        current->hull_ref = NCD_NONE;
    }
}

// 0x00AEDC30 (:623)
static void ncd_remove_navpoint_block_refs(navpoint* navp)
{
    unsigned short active_at_start = Active_detour_navpoint_block_refs;
    detour_navpoint_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_navpoint_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_navpoint_block_ref* current = ref;
        ref = current->next == Active_detour_navpoint_block_refs ? nullptr : Detour_navpoint_block_refs + current->next;
        if (current->navp == navp) {
            unsigned short index = (unsigned short)(current - Detour_navpoint_block_refs);
            ncd_list_remove(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, index);
            ncd_list_add(Detour_navpoint_block_refs, Free_detour_navpoint_block_refs, index);
        }
    }
    navp->heap_data.disabled_by &= ~NCD_NAVPOINT_DISABLED_BIT;
    for (int i = 0; i < 10; i++) {
        navp->link_disabled_by[i] &= ~NCD_NAVPOINT_DISABLED_BIT;
    }
}

// 0x00AEDDA0 (:651) LTCG: node ECX
static void ncd_add_action_node_block_ref(object_action_node* node, compound_hull* cmp_hull)
{
    if (Free_detour_action_node_block_refs == NCD_NONE) {
        return;
    }
    unsigned short index = Free_detour_action_node_block_refs;
    ncd_list_remove(Detour_action_node_block_refs, Free_detour_action_node_block_refs, index);
    Detour_action_node_block_refs[index].hull_ref = ncd_hull_index(cmp_hull);
    Detour_action_node_block_refs[index].node = node;
    ncd_list_add(Detour_action_node_block_refs, Active_detour_action_node_block_refs, index);
    node->set_disabled_by(ANDR_OBSTRUCTION);
}

// 0x00AEDEC0 (:676) LTCG: cmp_hull EAX
static void ncd_add_guard_node_block_ref(object_guard_node* node, compound_hull* cmp_hull)
{
    if (Free_detour_guard_node_block_refs == NCD_NONE) {
        return;
    }
    unsigned short index = Free_detour_guard_node_block_refs;
    ncd_list_remove(Detour_guard_node_block_refs, Free_detour_guard_node_block_refs, index);
    Detour_guard_node_block_refs[index].hull_ref = ncd_hull_index(cmp_hull);
    Detour_guard_node_block_refs[index].node = node;
    ncd_list_add(Detour_guard_node_block_refs, Active_detour_guard_node_block_refs, index);
    node->disable_node_detour();
}

// 0x00AEDFE0 (:696)
static void ncd_remove_action_node_block_refs(compound_hull* cmp_hull)
{
    unsigned short active_at_start = Active_detour_action_node_block_refs;
    detour_action_node_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_action_node_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_action_node_block_ref* current = ref;
        ref = current->next == active_at_start ? nullptr : Detour_action_node_block_refs + current->next;
        if (Compound_hulls + current->hull_ref != cmp_hull) {
            continue;
        }
        unsigned short index = (unsigned short)(current - Detour_action_node_block_refs);
        ncd_list_remove(Detour_action_node_block_refs, Active_detour_action_node_block_refs, index);
        ncd_list_add(Detour_action_node_block_refs, Free_detour_action_node_block_refs, index);
        active_at_start = Active_detour_action_node_block_refs;
        bool still_blocked = false;
        if (active_at_start != NCD_NONE) {
            detour_action_node_block_ref* other = Detour_action_node_block_refs + active_at_start;
            for (;;) {
                if (other->node == current->node && other->hull_ref != current->hull_ref) {
                    still_blocked = true;
                }
                if (other->next == active_at_start) {
                    break;
                }
                other = Detour_action_node_block_refs + other->next;
            }
        }
        if (!still_blocked) {
            current->node->clear_disabled_by(ANDR_OBSTRUCTION);
            active_at_start = Active_detour_action_node_block_refs;
        }
        current->node = nullptr;
        current->hull_ref = NCD_NONE;
    }
}

// 0x00AEE190 (:731)
static void ncd_remove_guard_node_block_refs(compound_hull* cmp_hull)
{
    unsigned short active_at_start = Active_detour_guard_node_block_refs;
    detour_guard_node_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_guard_node_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_guard_node_block_ref* current = ref;
        ref = current->next == active_at_start ? nullptr : Detour_guard_node_block_refs + current->next;
        if (Compound_hulls + current->hull_ref != cmp_hull) {
            continue;
        }
        unsigned short index = (unsigned short)(current - Detour_guard_node_block_refs);
        ncd_list_remove(Detour_guard_node_block_refs, Active_detour_guard_node_block_refs, index);
        ncd_list_add(Detour_guard_node_block_refs, Free_detour_guard_node_block_refs, index);
        active_at_start = Active_detour_guard_node_block_refs;
        bool still_blocked = false;
        if (active_at_start != NCD_NONE) {
            detour_guard_node_block_ref* other = Detour_guard_node_block_refs + active_at_start;
            for (;;) {
                if (other->node == current->node && other->hull_ref != current->hull_ref) {
                    still_blocked = true;
                }
                if (other->next == active_at_start) {
                    break;
                }
                other = Detour_guard_node_block_refs + other->next;
            }
        }
        if (!still_blocked) {
            current->node->reenable_node_detour();
        }
        current->node = nullptr;
        current->hull_ref = NCD_NONE;
    }
}

// 0x00AEE340 (:766)
static void ncd_remove_action_node_block_refs(object_action_node* node)
{
    unsigned short active_at_start = Active_detour_action_node_block_refs;
    detour_action_node_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_action_node_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_action_node_block_ref* current = ref;
        ref = current->next == Active_detour_action_node_block_refs ? nullptr : Detour_action_node_block_refs + current->next;
        if (current->node == node) {
            unsigned short index = (unsigned short)(current - Detour_action_node_block_refs);
            ncd_list_remove(Detour_action_node_block_refs, Active_detour_action_node_block_refs, index);
            ncd_list_add(Detour_action_node_block_refs, Free_detour_action_node_block_refs, index);
        }
    }
    node->clear_disabled_by(ANDR_OBSTRUCTION);
}

// 0x00AEE470 (:788)
static void ncd_remove_guard_node_block_refs(object_guard_node* node)
{
    unsigned short active_at_start = Active_detour_guard_node_block_refs;
    detour_guard_node_block_ref* ref = active_at_start == NCD_NONE ? nullptr : Detour_guard_node_block_refs + active_at_start;
    while (ref != nullptr) {
        detour_guard_node_block_ref* current = ref;
        ref = current->next == Active_detour_guard_node_block_refs ? nullptr : Detour_guard_node_block_refs + current->next;
        if (current->node == node) {
            unsigned short index = (unsigned short)(current - Detour_guard_node_block_refs);
            ncd_list_remove(Detour_guard_node_block_refs, Active_detour_guard_node_block_refs, index);
            ncd_list_add(Detour_guard_node_block_refs, Free_detour_guard_node_block_refs, index);
        }
    }
    node->reenable_node_detour();
}

// compound_hull_ref threads two lists through itself: the allocation lists and the references of one nav cell.
static void ncd_ref_alloc_remove(unsigned short& head, unsigned short index)
{
    compound_hull_ref& ref = Compound_hull_references[index];
    if (head == index && (head = ref.allocation_next, head == index)) {
        head = NCD_NONE;
    } else {
        Compound_hull_references[ref.allocation_prev].allocation_next = ref.allocation_next;
        Compound_hull_references[ref.allocation_next].allocation_prev = ref.allocation_prev;
    }
    ref.allocation_next = NCD_NONE;
    ref.allocation_prev = NCD_NONE;
}

static void ncd_ref_alloc_add(unsigned short& head, unsigned short index)
{
    compound_hull_ref& ref = Compound_hull_references[index];
    if (head == NCD_NONE) {
        ref.allocation_next = index;
        ref.allocation_prev = index;
        head = index;
    } else {
        ref.allocation_prev = Compound_hull_references[head].allocation_prev;
        ref.allocation_next = head;
        Compound_hull_references[Compound_hull_references[head].allocation_prev].allocation_next = index;
        Compound_hull_references[head].allocation_prev = index;
    }
}

// 0x00AEE5A0 (:815) LTCG: ncp EDX, cmp_hull ECX
static void add_compound_hull_reference(nav_cell* ncp, compound_hull* cmp_hull)
{
    unsigned short index = Free_compound_hull_references;
    if (index == NCD_NONE) {
        return;
    }
    compound_hull_ref& ref = Compound_hull_references[index];
    ref.ncp = ncp;
    ref.cmp_hull = cmp_hull;
    cmp_hull->reference_count++;
    if (ncp->compound_hull_reference == NCD_NONE) {
        ref.references_next = index;
        ref.references_prev = index;
        ncp->compound_hull_reference = index;
    } else {
        compound_hull_ref& head = Compound_hull_references[ncp->compound_hull_reference];
        ref.references_prev = head.references_prev;
        ref.references_next = ncp->compound_hull_reference;
        Compound_hull_references[head.references_prev].references_next = index;
        head.references_prev = index;
    }
    ncd_ref_alloc_remove(Free_compound_hull_references, index);
    ncd_ref_alloc_add(Active_compound_hull_references, index);
}

// 0x00B06D70 (:841) LTCG: reference ESI
static void release_compound_hull_reference(compound_hull_ref* reference)
{
    reference->cmp_hull->reference_count--;
    uint64_t handle = nav_cell_get_handle(reference->ncp);
    unsigned short first_vert = reference->cmp_hull->points;
    if (first_vert != NCD_NONE) {
        for (compound_hull_vert* vert = Compound_hull_verts + first_vert;; vert = ncd_next_vert(vert)) {
            if (vert->nav_cell_handle == handle) {
                vert->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
            }
            if (vert->next == first_vert) {
                break;
            }
        }
    }
    nav_cell* ncp = reference->ncp;
    unsigned short index = (unsigned short)(reference - Compound_hull_references);
    if (ncp->compound_hull_reference == index && reference->references_next == index) {
        reference->references_next = NCD_NONE;
        reference->references_prev = NCD_NONE;
        ncp->compound_hull_reference = NCD_NONE;
    } else {
        if (ncp->compound_hull_reference == index) {
            ncp->compound_hull_reference = reference->references_next;
        }
        Compound_hull_references[reference->references_prev].references_next = reference->references_next;
        Compound_hull_references[reference->references_next].references_prev = reference->references_prev;
        reference->references_next = NCD_NONE;
        reference->references_prev = NCD_NONE;
    }
    ncd_ref_alloc_remove(Active_compound_hull_references, index);
    ncd_ref_alloc_add(Free_compound_hull_references, index);
}

// 0x00AEE6E0 (:869)
static compound_hull* allocate_compound_hull()
{
    unsigned short index = Free_compound_hulls;
    if (index == NCD_NONE) {
        return nullptr;
    }
    ncd_list_remove(Compound_hulls, Free_compound_hulls, index);
    compound_hull* cmp_hull = Compound_hulls + index;
    memset(cmp_hull, 0, sizeof(*cmp_hull));
    ncd_list_add(Compound_hulls, Active_compound_hulls, index);
    cmp_hull->points = NCD_NONE;
    cmp_hull->convex_hulls = NCD_NONE;
    cmp_hull->handle = ((unsigned int)Next_compound_hull_identifier << 16) | index;
    Next_compound_hull_identifier = (Next_compound_hull_identifier + 1) & 0xFFFF;
    return cmp_hull;
}

// 0x00B06EE0 (:914) LTCG: cmp_hull EDI; detatched_convex_hull_list on the stack
static void release_compound_hull(compound_hull* cmp_hull, unsigned short* detatched_convex_hull_list)
{
    if (cmp_hull->reference_count != 0) {
        unsigned short active = Active_compound_hull_references;
        compound_hull_ref* ref = active == NCD_NONE ? nullptr : Compound_hull_references + active;
        while (ref != nullptr) {
            compound_hull_ref* current = ref;
            ref = current->allocation_next == active ? nullptr : Compound_hull_references + current->allocation_next;
            if (current->cmp_hull == cmp_hull) {
                release_compound_hull_reference(current);
                active = Active_compound_hull_references;
            }
        }
    }
    ncd_remove_navpoint_block_refs(cmp_hull);
    ncd_remove_action_node_block_refs(cmp_hull);
    ncd_remove_guard_node_block_refs(cmp_hull);
    if (cmp_hull->convex_hulls != NCD_NONE) {
        unsigned short hull = cmp_hull->convex_hulls;
        do {
            Convex_hulls[hull].cmp_hull = nullptr;
            hull = Convex_hulls[hull].next;
        } while (hull != cmp_hull->convex_hulls);
        if (detatched_convex_hull_list != nullptr) {
            *detatched_convex_hull_list = cmp_hull->convex_hulls;
        } else {
            ncd_list_merge(Convex_hulls, Unincorporated_convex_hulls, cmp_hull->convex_hulls);
        }
        // Original quirk kept: 0, not NCD_NONE; the hull goes back on the free list and is reset when reallocated.
        cmp_hull->convex_hulls = 0;
    }
    cmp_hull->convex_hull_count = 0;
    if (cmp_hull->points != NCD_NONE) {
        ncd_list_merge(Compound_hull_verts, Free_compound_hull_verts, cmp_hull->points);
        cmp_hull->points = NCD_NONE;
    }
    unsigned short index = ncd_hull_index(cmp_hull);
    cmp_hull->handle = 0xFFFFFFFF;
    ncd_list_remove(Compound_hulls, Active_compound_hulls, index);
    ncd_list_add(Compound_hulls, Free_compound_hulls, index);
}

// 0x00AEE810 (:1004) LTCG: expanded_hull EDX, num_points CL, expansion_size XMM0
static void expand_convex_hull(convex_hull_2d* expanded_hull, const vector2* base_points, unsigned char num_points, float expansion_size)
{
    int count = num_points;
    if (count == 0) {
        expanded_hull->num_points = 0;
        return;
    }
    vector2* out = expanded_hull->points;
    float e = expansion_size;
    if (count == 1) {
        expanded_hull->num_points = 4;
        out[0] = vector2(base_points[0].x - e, base_points[0].y - e);
        out[1] = vector2(base_points[0].x - e, base_points[0].y + e);
        out[2] = vector2(base_points[0].x + e, base_points[0].y + e);
        out[3] = vector2(base_points[0].x + e, base_points[0].y - e);
        return;
    }
    if (count == 2) {
        expanded_hull->num_points = 4;
        vector2 dir(base_points[1].x - base_points[0].x, base_points[1].y - base_points[0].y);
        dir.normalize();
        out[0] = vector2(base_points[0].x + e * (-dir.x - dir.y), base_points[0].y + (-dir.y + dir.x) * e);
        out[1] = vector2(base_points[1].x + e * (dir.x - dir.y), base_points[1].y + (dir.y + dir.x) * e);
        out[2] = vector2(base_points[1].x + e * (dir.y + dir.x), base_points[1].y + (-dir.x + dir.y) * e);
        out[3] = vector2(base_points[0].x + e * (-dir.x + dir.y), base_points[0].y + (-dir.y - dir.x) * e);
        return;
    }
    // Each vertex moves out along both of its edges.
    expanded_hull->num_points = num_points;
    vector2 edges[6];
    for (int i = 0; i < count; i++) {
        edges[i] = vector2(base_points[(i + 1) % count].x - base_points[i].x, base_points[(i + 1) % count].y - base_points[i].y);
        edges[i].normalize();
        edges[i].x *= e;
        edges[i].y *= e;
    }
    for (int i = 0; i < count; i++) {
        const vector2& previous = edges[(count - 1 + i) % count];
        out[i].x = base_points[i].x + previous.x - edges[i].x;
        out[i].y = base_points[i].y + previous.y - edges[i].y;
    }
}

// 0x00AEF080 (:1071) LTCG: bbox_max ECX, expansion_size XMM0 from the stack
static void compute_convex_hull_from_oriented_bbox(convex_hull_2d* simple_hull, const vector& pos, const matrix& orient, const vector& bbox_min, const vector& bbox_max, float expansion_size)
{
    vector corners[8];
    corners[0] = vector(bbox_min.x, bbox_min.y, bbox_min.z);
    corners[1] = vector(bbox_max.x, bbox_min.y, bbox_min.z);
    corners[2] = vector(bbox_min.x, bbox_max.y, bbox_min.z);
    corners[3] = vector(bbox_max.x, bbox_max.y, bbox_min.z);
    corners[4] = vector(bbox_min.x, bbox_min.y, bbox_max.z);
    corners[5] = vector(bbox_max.x, bbox_min.y, bbox_max.z);
    corners[6] = vector(bbox_min.x, bbox_max.y, bbox_max.z);
    corners[7] = vector(bbox_max.x, bbox_max.y, bbox_max.z);
    vector2 projected[8];
    for (int i = 0; i < 8; i++) {
        corners[i].unrotate(orient);
        corners[i].x += pos.x;
        corners[i].y += pos.y;
        corners[i].z += pos.z;
        projected[i] = vector2(corners[i].x, corners[i].z);
    }
    // Magic numbers: rfg.exe's table of the box faces, in opposite pairs (0/1, 2/3, 4/5).
    static const unsigned char faces[6][4] = {
        { 2, 6, 7, 3 }, { 0, 1, 5, 4 }, { 4, 6, 2, 0 }, { 1, 3, 7, 5 }, { 5, 7, 6, 4 }, { 0, 2, 3, 1 },
    };
    // The faces looking up: their outline is the silhouette.
    unsigned char up_faces[3];
    unsigned char num_up_faces = 0;
    for (int face = 0; face < 6; face += 2) {
        vector normal;
        pltind_vec_perp(normal, corners[faces[face][0]], corners[faces[face][1]], corners[faces[face][2]]);
        normal.normalize();
        // Magic number: a face within about 6 degrees of vertical is ignored.
        if (0.1f <= fabsf(normal.y)) {
            up_faces[num_up_faces++] = 0.0f <= normal.y ? (unsigned char)face : (unsigned char)(face + 1);
        }
    }
    // One past the 6 points of the largest silhouette: the removals below move one entry too many, as in rfg.exe.
    vector2 points[7];
    unsigned char num_points = 0;
    if (num_up_faces == 1) {
        for (int i = 0; i < 4; i++) {
            points[i] = projected[faces[up_faces[0]][i]];
        }
        num_points = 4;
    } else if (num_up_faces == 2) {
        const unsigned char* a = faces[up_faces[0]];
        const unsigned char* b = faces[up_faces[1]];
        bool found = false;
        for (int u = 1; u <= 4 && !found; u++) {
            for (int j = 0; j < 4; j++) {
                if (a[u - 1] != b[j]) {
                    continue;
                }
                num_points = 4;
                unsigned char last = b[(j + 3) % 4];
                if (a[u % 4] == last) {
                    points[0] = projected[a[(u + 1) % 4]];
                    points[1] = projected[a[(u + 2) % 4]];
                    points[2] = projected[b[(j + 1) % 4]];
                    last = b[(j + 2) % 4];
                } else {
                    points[0] = projected[a[u % 4]];
                    points[1] = projected[a[(u + 1) % 4]];
                    points[2] = projected[b[(j + 2) % 4]];
                }
                points[3] = projected[last];
                found = true;
                break;
            }
        }
    } else if (num_up_faces == 3) {
        const unsigned char* a = faces[up_faces[0]];
        const unsigned char* b = faces[up_faces[1]];
        const unsigned char* c = faces[up_faces[2]];
        bool found = false;
        for (int u = 0; u < 4 && !found; u++) {
            for (int j = 0; j < 4 && !found; j++) {
                if (a[u] != b[j]) {
                    continue;
                }
                for (int k = 2; k <= 5 && !found; k++) {
                    if (c[k - 2] != b[j]) {
                        continue;
                    }
                    found = true;
                    num_points = 6;
                    points[0] = projected[a[(u + 1) % 4]];
                    points[1] = projected[a[(u + 2) % 4]];
                    unsigned char third = a[(u + 3) % 4];
                    points[2] = projected[third];
                    if (third == b[(j + 1) % 4]) {
                        points[3] = projected[b[(j + 2) % 4]];
                        points[4] = projected[b[(j + 3) % 4]];
                        points[5] = projected[c[k % 4]];
                    } else {
                        points[3] = projected[c[k % 4]];
                        points[4] = projected[c[(k + 1) % 4]];
                        points[5] = projected[b[(j + 2) % 4]];
                    }
                }
            }
        }
    } else {
        return;
    }
    // Magic number: points closer than 5 cm merge.
    for (int i = 0; i < num_points && num_points >= 2; i++) {
        if (vec2_dist_squared(points[i], points[(i + 1) % num_points]) < 0.0025000002f) {
            if (num_points - i > 0) {
                memmove(&points[i], &points[i + 1], (num_points - i) * sizeof(vector2));
            }
            i--;
            num_points--;
        }
    }
    // Magic number: a corner flatter than about 2.5 degrees goes.
    for (int i = 0; i < num_points && num_points >= 3; i++) {
        const vector2& previous = points[(num_points - 1 + i) % num_points];
        vector2 in_dir(points[i].x - previous.x, points[i].y - previous.y);
        in_dir.normalize();
        const vector2& next = points[(i + 1) % num_points];
        vector2 out_dir(next.x - points[i].x, next.y - points[i].y);
        out_dir.normalize();
        if (0.999f <= out_dir.y * in_dir.y + out_dir.x * in_dir.x) {
            if (num_points - i > 0) {
                memmove(&points[i], &points[i + 1], (num_points - i) * sizeof(vector2));
            }
            i--;
            num_points--;
        }
    }
    expand_convex_hull(simple_hull, points, num_points, expansion_size);
    simple_hull->bmin.x = simple_hull->points[0].x;
    simple_hull->bmin.y = 0.0f;
    simple_hull->bmin.z = simple_hull->points[0].y;
    simple_hull->bmax = simple_hull->bmin;
    for (int i = 1; i < simple_hull->num_points; i++) {
        const vector2& point = simple_hull->points[i];
        simple_hull->bmin.x = 0.0f <= simple_hull->bmin.x - point.x ? point.x : simple_hull->bmin.x;
        simple_hull->bmin.z = 0.0f <= simple_hull->bmin.z - point.y ? point.y : simple_hull->bmin.z;
        simple_hull->bmax.x = simple_hull->bmax.x - point.x < 0.0f ? point.x : simple_hull->bmax.x;
        simple_hull->bmax.z = simple_hull->bmax.z - point.y < 0.0f ? point.y : simple_hull->bmax.z;
    }
    simple_hull->bmin.y = corners[0].y;
    simple_hull->bmax.y = corners[0].y;
    for (int i = 1; i < 8; i++) {
        simple_hull->bmin.y = 0.0f <= simple_hull->bmin.y - corners[i].y ? corners[i].y : simple_hull->bmin.y;
        simple_hull->bmax.y = simple_hull->bmax.y - corners[i].y < 0.0f ? corners[i].y : simple_hull->bmax.y;
    }
}

// 0x00AEFB30 (:1294) LTCG: edge_vert1 ECX, pt EAX
static point_classification classify_point_vs_segment(const vector2& edge_vert0, const vector2& edge_vert1, const vector2& pt)
{
    float side = (pt.x - edge_vert0.x) * (edge_vert1.y - edge_vert0.y) + (edge_vert0.x - edge_vert1.x) * (pt.y - edge_vert0.y);
    if (0.0f < side) {
        return POINT_CLASS_RIGHT;
    }
    if (side < 0.0f) {
        return POINT_CLASS_LEFT;
    }
    return POINT_CLASS_ON;
}

// 0x00AEFBD0 (:1322) LTCG: start_point EDX, end_point EAX
static bool can_point_on_hull_see_point_on_hull(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, const compound_hull* cmp_hull)
{
    const compound_hull_vert* start_vert = start_point.vert;
    const compound_hull_vert* end_vert = end_point.vert;
    if (start_vert == end_vert) {
        return true;
    }
    float start_x = start_point.pos.x;
    float start_z = start_point.pos.z;
    bool start_on_vert = start_x == start_vert->pos.x && start_z == start_vert->pos.y;
    bool end_on_vert = end_point.pos.x == end_vert->pos.x && end_point.pos.z == end_vert->pos.y;
    if (start_on_vert && end_on_vert && (start_vert == Compound_hull_verts + end_vert->next || start_vert == Compound_hull_verts + end_vert->prev)) {
        return true;
    }
    // x87 doubles as in rfg.exe, the hulls' edge tests are exact on shared vertices.
    double delta_x = (double)end_point.pos.x - (double)start_x;
    double delta_z = (double)end_point.pos.z - (double)start_z;
    vector2 delta((float)delta_x, (float)delta_z);
    const compound_hull_vert* ignored_start[2];
    int num_ignored_start;
    if (start_on_vert) {
        const compound_hull_vert* previous = Compound_hull_verts + start_vert->prev;
        const compound_hull_vert* next = ncd_next_vert(start_vert);
        vector to_previous(previous->pos.x - start_vert->pos.x, 0.0f, previous->pos.y - start_vert->pos.y);
        vector to_next(next->pos.x - start_vert->pos.x, 0.0f, next->pos.y - start_vert->pos.y);
        ignored_start[0] = previous;
        ignored_start[1] = start_vert;
        num_ignored_start = 2;
        // Magic number: 2 pi; the ray must leave through the outside of the hull's corner.
        float corner_angle = vec_delta_ang(to_previous, to_next, UP_VECTOR);
        if (corner_angle < 0.0f) {
            corner_angle += 6.2831855f;
        }
        vector ray(delta.x, 0.0f, delta.y);
        float ray_angle = vec_delta_ang(to_previous, ray, UP_VECTOR);
        if (ray_angle < 0.0f) {
            ray_angle += 6.2831855f;
        }
        if (corner_angle < ray_angle) {
            return false;
        }
    } else {
        ignored_start[0] = start_vert;
        ignored_start[1] = nullptr;
        num_ignored_start = 1;
        const compound_hull_vert* next = ncd_next_vert(start_vert);
        vector2 edge(next->pos.x - start_vert->pos.x, next->pos.y - start_vert->pos.y);
        if (vec2_which_side_of_2d_line(delta, ZERO_VECTOR2, edge) < 0.0f) {
            return false;
        }
    }
    const compound_hull_vert* ignored_end[2];
    int num_ignored_end;
    if (end_on_vert) {
        ignored_end[0] = Compound_hull_verts + end_vert->prev;
        ignored_end[1] = end_vert;
        num_ignored_end = 2;
    } else {
        ignored_end[0] = end_vert;
        ignored_end[1] = nullptr;
        num_ignored_end = 1;
    }
    unsigned short first = cmp_hull->points;
    if (first == NCD_NONE) {
        return true;
    }
    for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
        bool ignored = false;
        for (int i = 0; i < num_ignored_start && !ignored; i++) {
            ignored = ignored_start[i] == vert;
        }
        for (int i = 0; i < num_ignored_end && !ignored; i++) {
            ignored = ignored_end[i] == vert;
        }
        if (!ignored) {
            const compound_hull_vert* next = ncd_next_vert(vert);
            double edge_dz = (double)next->pos.y - (double)vert->pos.y;
            double edge_dx = (double)next->pos.x - (double)vert->pos.x;
            float denominator = (float)(edge_dz * delta_x - edge_dx * delta_z);
            if (denominator != 0.0f) {
                double offset_z = (double)start_z - (double)vert->pos.y;
                double offset_x = (double)start_x - (double)vert->pos.x;
                float t = (float)(offset_z * edge_dx - offset_x * edge_dz) / denominator;
                if (0.0f <= t && t <= 1.0f) {
                    float u = (float)(offset_z * delta_x - offset_x * delta_z) / denominator;
                    if (0.0f <= u && u <= 1.0f) {
                        return false;
                    }
                }
            }
        }
        if (vert->next == first) {
            return true;
        }
    }
}

// 0x00AF6FE0 (:1454) LTCG: start_point EAX; the stack holds end_point and cmp_hull
static bool does_linesegment_enter_compound_hull(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point, const compound_hull* cmp_hull)
{
    const compound_hull* end_hull = end_point.cmp_hull;
    vector2 end_2d(end_point.pos.x, end_point.pos.z);
    if (end_hull != cmp_hull && nav_cell_detour_is_point_in_compound_hull(end_2d, cmp_hull)) {
        return true;
    }
    float start_x = start_point.pos.x;
    float start_z = start_point.pos.z;
    float delta_x = end_2d.x - start_x;
    float delta_z = end_2d.y - start_z;
    unsigned short first = cmp_hull->points;
    if (first == NCD_NONE) {
        return false;
    }
    if (start_point.cmp_hull == cmp_hull) {
        if (end_hull == cmp_hull) {
            return !can_point_on_hull_see_point_on_hull(start_point, end_point, cmp_hull);
        }
        const compound_hull_vert* start_vert = start_point.vert;
        bool start_on_vert = start_x == start_vert->pos.x && start_z == start_vert->pos.y;
        for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
            const compound_hull_vert* next = ncd_next_vert(vert);
            if (vert != start_vert && (!start_on_vert || next != start_vert)) {
                float edge_dz = next->pos.y - vert->pos.y;
                float edge_dx = next->pos.x - vert->pos.x;
                float denominator = delta_x * edge_dz - edge_dx * delta_z;
                if (denominator != 0.0f) {
                    float offset_z = start_z - vert->pos.y;
                    float offset_x = start_x - vert->pos.x;
                    float t = (offset_z * edge_dx - offset_x * edge_dz) / denominator;
                    if (0.0f <= t && t <= 1.0f) {
                        float u = (offset_z * delta_x - offset_x * delta_z) / denominator;
                        if (0.0f <= u && u <= 1.0f) {
                            return true;
                        }
                    }
                }
            }
            if (vert->next == first) {
                return false;
            }
        }
    }
    // Only edges the segment crosses from outside to inside count.
    for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
        const compound_hull_vert* next = ncd_next_vert(vert);
        float edge_dz = next->pos.y - vert->pos.y;
        float back_dx = vert->pos.x - next->pos.x;
        float offset_x = start_x - vert->pos.x;
        float offset_z = start_z - vert->pos.y;
        if (offset_z * back_dx + offset_x * edge_dz <= 0.0f
            && (end_point.cmp_hull != cmp_hull || end_point.vert != vert)
            && 0.0f < (end_2d.y - vert->pos.y) * back_dx + (end_2d.x - vert->pos.x) * edge_dz) {
            float edge_dx = next->pos.x - vert->pos.x;
            float denominator = delta_x * edge_dz - delta_z * edge_dx;
            if (denominator != 0.0f) {
                float t = (edge_dx * offset_z - offset_x * edge_dz) / denominator;
                if (0.0f <= t && t <= 1.0f) {
                    float u = (delta_x * offset_z - delta_z * offset_x) / denominator;
                    if (0.0f <= u && u <= 1.0f) {
                        return true;
                    }
                }
            }
        }
        if (vert->next == first) {
            return false;
        }
    }
}

// 0x00AF0090 (:1540) LTCG: start_pos EDI
static bool get_first_linesegment_entry_point_into_compound_hull(vector2* closest_entry_pos, int* entry_edge_index, const vector2& start_pos, const vector2& end_pos, const compound_hull* cmp_hull)
{
    int edge = -1;
    float best_dist_sq = -1.0f;
    unsigned short first = cmp_hull->points;
    if (first == NCD_NONE) {
        return false;
    }
    for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
        const compound_hull_vert* next = ncd_next_vert(vert);
        edge++;
        float back_dx = vert->pos.x - next->pos.x;
        double offset_x = (double)start_pos.x - (double)vert->pos.x;
        double offset_z = (double)start_pos.y - (double)vert->pos.y;
        double edge_dz = (double)next->pos.y - (double)vert->pos.y;
        if ((float)offset_x * (float)edge_dz + (float)offset_z * back_dx <= 0.0f
            && 0.0f < (end_pos.x - vert->pos.x) * (float)edge_dz + back_dx * (end_pos.y - vert->pos.y)) {
            double delta_x = (double)end_pos.x - (double)start_pos.x;
            double delta_z = (double)end_pos.y - (double)start_pos.y;
            double edge_dx = (double)next->pos.x - (double)vert->pos.x;
            float denominator = (float)(delta_x * edge_dz - delta_z * edge_dx);
            if (denominator != 0.0f) {
                float t = (float)(edge_dx * offset_z - offset_x * edge_dz) / denominator;
                if (0.0f <= t && t <= 1.0f) {
                    float u = (float)(offset_z * delta_x - delta_z * offset_x) / denominator;
                    if (0.0f <= u && u <= 1.0f) {
                        vector2 ix(start_pos.x + (float)delta_x * t, start_pos.y + (end_pos.y - start_pos.y) * t);
                        float dist_sq = vec2_dist_squared(ix, start_pos);
                        if (0.0f < dist_sq && (dist_sq < best_dist_sq || best_dist_sq < 0.0f)) {
                            *closest_entry_pos = ix;
                            *entry_edge_index = edge;
                            best_dist_sq = dist_sq;
                        }
                    }
                }
            }
        }
        if (vert->next == first) {
            break;
        }
    }
    return best_dist_sq >= 0.0f;
}

// 0x00AF74A0 (:1601) LTCG: next_node EAX
static bool get_first_linesegment_exit_point_from_compound_hull(vector2* closest_exit_pos, int* exit_edge_index, const path_node* cur_node, const path_node* next_node, const compound_hull* cmp_hull)
{
    vector2 start(cur_node->pos.x, cur_node->pos.z);
    vector2 end(next_node->pos.x, next_node->pos.z);
    int edge = -1;
    float best_dist_sq = -1.0f;
    unsigned short first = cmp_hull->points;
    if (first == NCD_NONE) {
        return false;
    }
    for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
        const compound_hull_vert* next = ncd_next_vert(vert);
        edge++;
        // The edge the node sits on (when it is on this hull) cannot be the exit.
        bool own_edge = cur_node->compound_hull_vert_index == edge && nav_cell_detour_get_compound_hull(cur_node->compound_hull_handle) == cmp_hull;
        if (!own_edge) {
            float edge_dz = next->pos.y - vert->pos.y;
            float back_dx = vert->pos.x - next->pos.x;
            float offset_x = start.x - vert->pos.x;
            float offset_z = start.y - vert->pos.y;
            if (0.0f <= back_dx * offset_z + offset_x * edge_dz && back_dx * (end.y - vert->pos.y) + (end.x - vert->pos.x) * edge_dz < 0.0f) {
                float delta_x = end.x - start.x;
                float delta_z = end.y - start.y;
                float edge_dx = next->pos.x - vert->pos.x;
                float denominator = delta_x * edge_dz - edge_dx * delta_z;
                if (denominator != 0.0f) {
                    float t = (edge_dx * offset_z - offset_x * edge_dz) / denominator;
                    if (0.0f <= t && t <= 1.0f) {
                        float u = (delta_x * offset_z - delta_z * offset_x) / denominator;
                        if (0.0f <= u && u <= 1.0f) {
                            vector2 ix(delta_x * t + start.x, delta_z * t + start.y);
                            float dist_sq = vec2_dist_squared(ix, start);
                            if (0.0f < dist_sq && (dist_sq < best_dist_sq || best_dist_sq < 0.0f)) {
                                *closest_exit_pos = ix;
                                *exit_edge_index = edge;
                                best_dist_sq = dist_sq;
                            }
                        }
                    }
                }
            }
        }
        if (vert->next == first) {
            break;
        }
    }
    return best_dist_sq >= 0.0f;
}

// 0x00AF78B0 (:1667) LTCG: convex_hull EAX
static bool is_convex_hull_contained_in_compound_hull(const convex_hull_2d* convex_hull, const compound_hull* cmp_hull)
{
    unsigned short first = cmp_hull->points;
    for (int i = 0; i < convex_hull->num_points; i++) {
        const vector2& point = convex_hull->points[i];
        bool inside = false;
        if (first != NCD_NONE) {
            // Even-odd crossing test.
            for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
                const compound_hull_vert* next = ncd_next_vert(vert);
                float y0 = vert->pos.y;
                float y1 = next->pos.y;
                if (((y0 < point.y && point.y <= y1) || (y1 < point.y && point.y <= y0))
                    && ((point.y - y0) / (y1 - y0)) * (next->pos.x - vert->pos.x) + vert->pos.x < point.x) {
                    inside = !inside;
                }
                if (vert->next == first) {
                    break;
                }
            }
        }
        if (!inside) {
            return false;
        }
    }
    return true;
}

static vector2 ncd_jittered_point(const convex_hull_2d* hull, int vert_index)
{
    return vector2(hull->jitter.x + hull->points[vert_index].x, hull->jitter.y + hull->points[vert_index].y);
}

// 0x00AF0450 (:1688) LTCG: ix_info ECX
static bool check_for_convex_hull_intersection(vector2* ix_pos, const convex_hull_intersection& ix_info)
{
    // A previous hit on this pair starts the test from that point.
    vector2 start = ix_info.ix_valid ? ix_info.ix_pos : ncd_jittered_point(ix_info.hull0, ix_info.hull0_vert_indices[0]);
    vector2 end0 = ncd_jittered_point(ix_info.hull0, ix_info.hull0_vert_indices[1]);
    vector2 start1 = ncd_jittered_point(ix_info.hull1, ix_info.hull1_vert_indices[0]);
    vector2 end1 = ncd_jittered_point(ix_info.hull1, ix_info.hull1_vert_indices[1]);
    return ncd_ix_edge_edge(start, end0, start1, end1, ix_pos);
}

// 0x00AF05B0 (:1705) LTCG: ix_info EAX, other_hull_jitter ECX
static bool check_for_compound_hull_intersection(vector2* ix_pos, const compound_hull_intersection& ix_info, compound_hull* cmp_hull0, compound_hull* other_hull, const vector2& other_hull_jitter)
{
    // The stack holds other_hull first and the second hull: the hull that is other_hull's gets the jitter.
    vector2 edge1_vert0, edge1_vert1;
    if (other_hull == cmp_hull0) {
        edge1_vert1 = ix_info.hull1_verts[1]->pos;
        edge1_vert0 = ix_info.hull1_verts[0]->pos;
    } else {
        edge1_vert1 = vector2(ix_info.hull1_verts[1]->pos.x + other_hull_jitter.x, ix_info.hull1_verts[1]->pos.y + other_hull_jitter.y);
        edge1_vert0 = vector2(other_hull_jitter.x + ix_info.hull1_verts[0]->pos.x, ix_info.hull1_verts[0]->pos.y + other_hull_jitter.y);
    }
    bool hull0_is_other = ix_info.hull0 == cmp_hull0;
    vector2 edge0_vert1 = hull0_is_other ? ix_info.hull0_verts[1]->pos
                                         : vector2(ix_info.hull0_verts[1]->pos.x + other_hull_jitter.x, ix_info.hull0_verts[1]->pos.y + other_hull_jitter.y);
    vector2 edge0_vert0;
    if (ix_info.ix_valid) {
        // Original quirk kept: the kept intersection point takes no jitter.
        edge0_vert0 = ix_info.ix_pos;
    } else if (hull0_is_other) {
        edge0_vert0 = ix_info.hull0_verts[0]->pos;
    } else {
        edge0_vert0 = vector2(ix_info.hull0_verts[0]->pos.x + other_hull_jitter.x, ix_info.hull0_verts[0]->pos.y + other_hull_jitter.y);
    }
    return ncd_ix_edge_edge(edge0_vert0, edge0_vert1, edge1_vert0, edge1_vert1, ix_pos);
}

// 0x00AF0720 (:1715) LTCG: convex_hull ECX, cmp_hull EDI; current_hull_list on the stack
static void attach_convex_hull_to_compound_hull(compound_hull* cmp_hull, convex_hull_2d* convex_hull, unsigned short& current_hull_list)
{
    unsigned short index = (unsigned short)(convex_hull - Convex_hulls);
    convex_hull->cmp_hull = cmp_hull;
    ncd_list_remove(Convex_hulls, current_hull_list, index);
    ncd_list_add(Convex_hulls, cmp_hull->convex_hulls, index);
    // Magic number: OT_VEHICLE's value in request_obj_type.
    if (convex_hull->request_obj_type == 3) {
        cmp_hull->flags.contains_vehicle = 1;
    }
    if (convex_hull->flags.from_destruction != 0) {
        cmp_hull->flags.contains_destruction = 1;
    }
    if (convex_hull->flags.dislodged_mover != 0) {
        cmp_hull->flags.contains_dislodged_mover = 1;
    }
    cmp_hull->convex_hull_count++;
}

// 0x00AF0810 (:1772)
static int eliminate_worthless_points(compound_hull* cmp_hull, compound_hull_vert** unassociated_points, int max_unassociated_points)
{
    // Magic number: vertices closer than 5 cm merge.
    if (cmp_hull->points != NCD_NONE) {
        compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;
        for (;;) {
            unsigned short next_index = vert->next;
            compound_hull_vert* next = Compound_hull_verts + next_index;
            if (next == vert || next == Compound_hull_verts + vert->prev) {
                break;
            }
            if (vec2_dist_squared(vert->pos, next->pos) < 0.0025000002f) {
                ncd_list_remove(Compound_hull_verts, cmp_hull->points, next_index);
                ncd_list_add(Compound_hull_verts, Free_compound_hull_verts, next_index);
            }
            if (vert->next == cmp_hull->points) {
                break;
            }
            vert = Compound_hull_verts + vert->next;
        }
    }
    int num_unassociated = 0;
    compound_hull_vert* walk = cmp_hull->points == NCD_NONE ? nullptr : Compound_hull_verts + cmp_hull->points;
    while (walk != nullptr) {
        compound_hull_vert* vert = walk;
        unsigned short next_index = vert->next;
        unsigned short prev_index = vert->prev;
        walk = next_index == cmp_hull->points ? nullptr : Compound_hull_verts + next_index;
        if (Compound_hull_verts + next_index == vert || next_index == prev_index) {
            return num_unassociated;
        }
        vector2 in_dir(vert->pos.x - Compound_hull_verts[prev_index].pos.x, vert->pos.y - Compound_hull_verts[prev_index].pos.y);
        if (in_dir.is_null()) {
            return num_unassociated;
        }
        vector2 out_dir(Compound_hull_verts[next_index].pos.x - vert->pos.x, Compound_hull_verts[next_index].pos.y - vert->pos.y);
        if (out_dir.is_null()) {
            return num_unassociated;
        }
        in_dir.normalize();
        out_dir.normalize();
        // Magic number: a corner flatter than about 2.5 degrees goes.
        if (0.999f <= out_dir.x * in_dir.x + in_dir.y * out_dir.y) {
            unsigned short index = (unsigned short)(vert - Compound_hull_verts);
            ncd_list_remove(Compound_hull_verts, cmp_hull->points, index);
            ncd_list_add(Compound_hull_verts, Free_compound_hull_verts, index);
        } else if (num_unassociated < max_unassociated_points && vert->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull) {
            unassociated_points[num_unassociated++] = vert;
        }
    }
    return num_unassociated;
}

// Even-odd crossing test of a point against a hull's outline.
static bool ncd_point_in_outline(const vector2& point, unsigned short first)
{
    bool inside = false;
    if (first != NCD_NONE) {
        for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
            const compound_hull_vert* next = ncd_next_vert(vert);
            float y0 = vert->pos.y;
            float y1 = next->pos.y;
            if (((y0 < point.y && point.y <= y1) || (y1 < point.y && point.y <= y0))
                && ((point.y - y0) / (y1 - y0)) * (next->pos.x - vert->pos.x) + vert->pos.x < point.x) {
                inside = !inside;
            }
            if (vert->next == first) {
                break;
            }
        }
    }
    return inside;
}

// 0x00AF7A00 (:1834) LTCG: cmp_hull0 EDX (the container), cmp_hull1 EAX
static bool does_compound_hull_contain_hull(compound_hull* cmp_hull0, compound_hull* cmp_hull1)
{
    unsigned short first = cmp_hull1->points;
    if (first == NCD_NONE) {
        return true;
    }
    for (const compound_hull_vert* vert = Compound_hull_verts + first;; vert = ncd_next_vert(vert)) {
        if (!ncd_point_in_outline(vert->pos, cmp_hull0->points)) {
            return false;
        }
        if (vert->next == first) {
            return true;
        }
    }
}

// Renames the block references of hull old_ref to new_ref. Original bug kept where a caller skips the guard nodes.
template<typename T>
static void ncd_rename_block_refs(T* pool, unsigned short head, unsigned short old_ref, unsigned short new_ref)
{
    if (head == NCD_NONE) {
        return;
    }
    unsigned short index = head;
    do {
        if (pool[index].hull_ref == old_ref) {
            pool[index].hull_ref = new_ref;
        }
        index = pool[index].next;
    } while (index != head);
}

// 0x00B07170 (:1851) LTCG: cmp_hull0 ESI (keeps everything), cmp_hull1 EAX and EDI (is released)
static void combine_contained_hull_2d_internal(compound_hull* cmp_hull0, compound_hull* cmp_hull1)
{
    if (cmp_hull1->convex_hulls != NCD_NONE) {
        unsigned short hull = cmp_hull1->convex_hulls;
        do {
            Convex_hulls[hull].cmp_hull = cmp_hull0;
            hull = Convex_hulls[hull].next;
        } while (hull != cmp_hull1->convex_hulls);
    }
    ncd_list_merge(Convex_hulls, cmp_hull0->convex_hulls, cmp_hull1->convex_hulls);
    cmp_hull0->convex_hull_count += cmp_hull1->convex_hull_count;
    cmp_hull1->convex_hulls = NCD_NONE;
    unsigned short ref0 = ncd_hull_index(cmp_hull0);
    unsigned short ref1 = ncd_hull_index(cmp_hull1);
    ncd_rename_block_refs(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, ref1, ref0);
    ncd_rename_block_refs(Detour_action_node_block_refs, Active_detour_action_node_block_refs, ref1, ref0);
    ncd_rename_block_refs(Detour_guard_node_block_refs, Active_detour_guard_node_block_refs, ref1, ref0);
    cmp_hull0->flags.contains_vehicle = cmp_hull0->flags.contains_vehicle != 0 || cmp_hull1->flags.contains_vehicle != 0;
    cmp_hull0->flags.contains_destruction = cmp_hull0->flags.contains_destruction != 0 || cmp_hull1->flags.contains_destruction != 0;
    cmp_hull0->flags.contains_dislodged_mover = cmp_hull0->flags.contains_dislodged_mover != 0 || cmp_hull1->flags.contains_dislodged_mover != 0;
    release_compound_hull(cmp_hull1, nullptr);
}

// The jitter moves the second hull's points so no vertex of one lies exactly on the other's edge.
static vector2 ncd_hull_point(const compound_hull_vert* vert, bool jittered, float jitter_x, float jitter_y)
{
    if (jittered) {
        return vector2(jitter_x + vert->pos.x, vert->pos.y + jitter_y);
    }
    return vert->pos;
}

// 0x00B0A980 (:1908)
static bool combine_compound_hull_2d_internal(compound_hull* cmp_hull_output, compound_hull* cmp_hull0, compound_hull* cmp_hull1, compound_hull_vert* starting_vert)
{
    // Magic number: the outline walk's safety limit.
    const int MAX_COMBINE_STEPS = 1000;
    int num_intersections = 0;
    compound_hull_intersection current;
    // Magic number: 2.5 cm.
    float jitter_x = fl_rand_range(-0.025f, 0.025f);
    float jitter_y = fl_rand_range(-0.025f, 0.025f);
    vector2 jitter(jitter_x, jitter_y);
    bool switched_hulls = false;
    int steps = 0;
    compound_hull_vert* cur_vert = nullptr;
    compound_hull* cur_hull = nullptr;
    for (;;) {
        steps++;
        bool had_intersection = current.ix_valid;
        compound_hull* prev_hull0 = current.hull0;
        compound_hull_vert* prev_vert0 = current.hull0_verts[0];
        compound_hull* other = cmp_hull1;
        if (cur_hull == nullptr) {
            current.hull0 = cmp_hull0;
            current.hull0_verts[0] = starting_vert;
            current.hull0_verts[1] = ncd_next_vert(starting_vert);
            if (starting_vert == current.hull0_verts[1]) {
                return false;
            }
        } else {
            if (!had_intersection) {
                current.hull0_verts[0] = cur_vert;
                current.hull0_verts[1] = ncd_next_vert(cur_vert);
                if (cur_vert == starting_vert) {
                    break;
                }
            } else {
                current.hull0 = cur_hull == cmp_hull0 ? cmp_hull1 : cmp_hull0;
                current.hull0_verts[0] = current.hull1_verts[0];
                current.hull0_verts[1] = current.hull1_verts[1];
                switched_hulls = true;
            }
            if (current.hull0 != cmp_hull0) {
                other = cmp_hull0;
            }
        }

        compound_hull_intersection best;
        compound_hull_intersection test = current;
        bool have_dir = false;
        vector2 edge_dir;
        float best_dist_sq = 3.4028235e+38f;
        float best_dot = 3.4028235e+38f;
        bool other_jittered = other != cmp_hull0;
        bool hull0_jittered = test.hull0 != cmp_hull0;
        if (other->points != NCD_NONE) {
            for (compound_hull_vert* vert = Compound_hull_verts + other->points;; vert = ncd_next_vert(vert)) {
                if (!(had_intersection && other == prev_hull0 && vert == prev_vert0)) {
                    test.hull1_verts[0] = vert;
                    test.hull1_verts[1] = ncd_next_vert(vert);
                    if (test.hull1_verts[1] == vert) {
                        return false;
                    }
                    vector2 ix;
                    if (check_for_compound_hull_intersection(&ix, test, cmp_hull0, other, jitter)) {
                        vector2 edge_start = ncd_hull_point(test.hull0_verts[0], hull0_jittered, jitter_x, jitter_y);
                        float dist_sq = vec2_dist_squared(edge_start, ix);
                        if (dist_sq < best_dist_sq) {
                            best = test;
                            best_dist_sq = dist_sq;
                            best.ix_valid = true;
                            best.ix_pos = ix;
                            best_dot = 3.4028235e+38f;
                        } else {
                            float excess = dist_sq - best_dist_sq;
                            // Magic number: the same intersection point: take the sharper turn.
                            if (excess < 0.0001f && -excess < 0.0001f) {
                                if (!have_dir) {
                                    vector2 edge_end = ncd_hull_point(test.hull0_verts[1], hull0_jittered, jitter_x, jitter_y);
                                    edge_dir = vector2(edge_end.x - edge_start.x, edge_end.y - edge_start.y);
                                    edge_dir.normalize_safe();
                                    have_dir = true;
                                }
                                if (best_dot == 3.4028235e+38f) {
                                    vector2 best0 = ncd_hull_point(best.hull1_verts[0], other_jittered, jitter_x, jitter_y);
                                    vector2 best1 = ncd_hull_point(best.hull1_verts[1], other_jittered, jitter_x, jitter_y);
                                    vector2 best_dir(best1.x - best0.x, best1.y - best0.y);
                                    best_dir.normalize_safe();
                                    best_dot = edge_dir.y * best_dir.y + best_dir.x * edge_dir.x;
                                }
                                // Original quirk kept: this start point takes its jitter from hull0, not from other.
                                vector2 test0 = ncd_hull_point(test.hull1_verts[0], hull0_jittered, jitter_x, jitter_y);
                                vector2 test1 = ncd_hull_point(test.hull1_verts[1], other_jittered, jitter_x, jitter_y);
                                vector2 test_dir(test1.x - test0.x, test1.y - test0.y);
                                test_dir.normalize_safe();
                                float dot = test_dir.y * edge_dir.y + test_dir.x * edge_dir.x;
                                if (dot < best_dot) {
                                    best = test;
                                    best.ix_valid = true;
                                    best.ix_pos = ix;
                                    if (excess < 0.0f) {
                                        best_dist_sq = dist_sq;
                                    }
                                    best_dot = dot;
                                }
                            }
                        }
                    }
                }
                if (vert->next == other->points) {
                    break;
                }
            }
        }

        vector2 out_pos;
        uint64_t out_handle = 0xFFFFFFFFFFFFFFFFull;
        if (!best.ix_valid) {
            out_pos = ncd_hull_point(current.hull0_verts[1], current.hull0 != cmp_hull0, jitter_x, jitter_y);
            out_handle = current.hull0_verts[1]->nav_cell_handle;
            if (out_handle != 0xFFFFFFFFFFFFFFFFull && current.hull0 != cmp_hull0) {
                const nav_cell* cell = nav_cell_from_handle(current.hull0_verts[1]->nav_cell_handle);
                if (cell != nullptr && !nav_cell_is_point_in_cell_column(cell, out_pos)) {
                    out_handle = 0xFFFFFFFFFFFFFFFFull;
                }
            }
            current.ix_valid = false;
        } else {
            num_intersections++;
            current = best;
            out_pos = best.ix_pos;
        }
        unsigned short vert_index = Free_compound_hull_verts;
        if (vert_index == NCD_NONE) {
            return false;
        }
        Compound_hull_verts[vert_index].pos = out_pos;
        Compound_hull_verts[vert_index].nav_cell_handle = out_handle;
        ncd_list_remove(Compound_hull_verts, Free_compound_hull_verts, vert_index);
        ncd_list_add(Compound_hull_verts, cmp_hull_output->points, vert_index);
        if (steps >= MAX_COMBINE_STEPS) {
            break;
        }
        cur_vert = current.hull0_verts[1];
        cur_hull = current.hull0;
    }
    if (!switched_hulls || steps >= MAX_COMBINE_STEPS) {
        return false;
    }

    // rfg.exe allocas this list.
    compound_hull_vert** unassociated = (compound_hull_vert**)_alloca(sizeof(compound_hull_vert*) * (num_intersections + 1));
    int num_unassociated = eliminate_worthless_points(cmp_hull_output, unassociated, num_intersections);

    if (cmp_hull0->convex_hulls != NCD_NONE) {
        unsigned short hull = cmp_hull0->convex_hulls;
        do {
            Convex_hulls[hull].cmp_hull = cmp_hull_output;
            hull = Convex_hulls[hull].next;
        } while (hull != cmp_hull0->convex_hulls);
    }
    if (cmp_hull1->convex_hulls != NCD_NONE) {
        unsigned short hull = cmp_hull1->convex_hulls;
        do {
            Convex_hulls[hull].cmp_hull = cmp_hull_output;
            Convex_hulls[hull].jitter.x += jitter_x;
            Convex_hulls[hull].jitter.y += jitter_y;
            hull = Convex_hulls[hull].next;
        } while (hull != cmp_hull1->convex_hulls);
    }
    cmp_hull_output->convex_hulls = cmp_hull0->convex_hulls;
    ncd_list_merge(Convex_hulls, cmp_hull_output->convex_hulls, cmp_hull1->convex_hulls);
    cmp_hull_output->convex_hull_count = cmp_hull0->convex_hull_count + cmp_hull1->convex_hull_count;
    cmp_hull1->convex_hulls = NCD_NONE;
    cmp_hull0->convex_hulls = NCD_NONE;
    cmp_hull1->convex_hull_count = 0;
    cmp_hull0->convex_hull_count = 0;

    unsigned short ref0 = ncd_hull_index(cmp_hull0);
    unsigned short ref1 = ncd_hull_index(cmp_hull1);
    unsigned short ref_output = ncd_hull_index(cmp_hull_output);
    // Original bug kept: rfg.exe renames the navpoint and action node references here, not the guard nodes'.
    ncd_rename_block_refs(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, ref0, ref_output);
    ncd_rename_block_refs(Detour_navpoint_block_refs, Active_detour_navpoint_block_refs, ref1, ref_output);
    ncd_rename_block_refs(Detour_action_node_block_refs, Active_detour_action_node_block_refs, ref0, ref_output);
    ncd_rename_block_refs(Detour_action_node_block_refs, Active_detour_action_node_block_refs, ref1, ref_output);

    cmp_hull_output->reference_count = 0;
    unsigned short active = Active_compound_hull_references;
    compound_hull_ref* ref = active == NCD_NONE ? nullptr : Compound_hull_references + active;
    while (ref != nullptr) {
        compound_hull_ref* current_ref = ref;
        ref = current_ref->allocation_next == active ? nullptr : Compound_hull_references + current_ref->allocation_next;
        if (current_ref->cmp_hull != cmp_hull0 && current_ref->cmp_hull != cmp_hull1) {
            continue;
        }
        current_ref->cmp_hull = cmp_hull_output;
        cmp_hull_output->reference_count++;
        for (int i = 0; i < num_unassociated; i++) {
            if (nav_cell_is_point_in_cell_column(current_ref->ncp, unassociated[i]->pos)) {
                unassociated[i]->nav_cell_handle = nav_cell_get_handle(current_ref->ncp);
                unassociated[i] = unassociated[num_unassociated - 1];
                num_unassociated--;
                i--;
            }
        }
        // The cell may also reference the other hull: keep one reference.
        unsigned short first_ref = current_ref->ncp->compound_hull_reference;
        if (first_ref != NCD_NONE) {
            compound_hull_ref* duplicate = Compound_hull_references + first_ref;
            while (duplicate->cmp_hull != cmp_hull0 && duplicate->cmp_hull != cmp_hull1) {
                if (duplicate->references_next == first_ref) {
                    duplicate = nullptr;
                    break;
                }
                duplicate = Compound_hull_references + duplicate->references_next;
            }
            if (duplicate != nullptr) {
                if (duplicate == ref) {
                    ref = duplicate->allocation_next == active ? nullptr : Compound_hull_references + duplicate->allocation_next;
                }
                release_compound_hull_reference(duplicate);
                active = Active_compound_hull_references;
            }
        }
    }
    cmp_hull1->reference_count = 0;
    cmp_hull0->reference_count = 0;
    cmp_hull_output->flags.nav_cells_computed = cmp_hull0->flags.nav_cells_computed != 0 && cmp_hull1->flags.nav_cells_computed != 0;
    cmp_hull_output->flags.contains_vehicle = cmp_hull0->flags.contains_vehicle != 0 || cmp_hull1->flags.contains_vehicle != 0;
    cmp_hull_output->flags.contains_destruction = cmp_hull0->flags.contains_destruction != 0 || cmp_hull1->flags.contains_destruction != 0;
    cmp_hull_output->flags.contains_dislodged_mover = cmp_hull0->flags.contains_dislodged_mover != 0 || cmp_hull1->flags.contains_dislodged_mover != 0;
    cmp_hull_output->bmin = cmp_hull0->bmin;
    cmp_hull_output->bmax = cmp_hull0->bmax;
    bbox_expand(cmp_hull_output->bmin, cmp_hull_output->bmax, cmp_hull1->bmin, cmp_hull1->bmax);
    release_compound_hull(cmp_hull0, nullptr);
    release_compound_hull(cmp_hull1, nullptr);
    return true;
}

// 0x00AF0BD0 (:2205)
static bool compound_hulls_can_be_combined(compound_hull* cmp_hull0, compound_hull* cmp_hull1, int& start_hull, compound_hull_vert*& start_vert)
{
    if (!ix_box_box_aligned(cmp_hull0->bmin, cmp_hull0->bmax, cmp_hull1->bmin, cmp_hull1->bmax)) {
        return false;
    }
    float min_x = 3.4028235e+38f;
    start_hull = 0;
    start_vert = nullptr;
    bool touching = false;
    if (cmp_hull0->points == NCD_NONE) {
        return false;
    }
    for (compound_hull_vert* vert = Compound_hull_verts + cmp_hull0->points;; vert = ncd_next_vert(vert)) {
        if (vert->pos.x < min_x) {
            start_vert = vert;
            min_x = vert->pos.x;
        }
        if (!touching && cmp_hull1->points != NCD_NONE) {
            for (compound_hull_vert* other = Compound_hull_verts + cmp_hull1->points;; other = ncd_next_vert(other)) {
                if (ncd_ix_edge_edge(vert->pos, ncd_next_vert(vert)->pos, other->pos, ncd_next_vert(other)->pos, nullptr)) {
                    touching = true;
                    break;
                }
                if (other->next == cmp_hull1->points) {
                    break;
                }
            }
        }
        if (vert->next == cmp_hull0->points) {
            break;
        }
    }
    if (!touching) {
        return false;
    }
    if (cmp_hull1->points != NCD_NONE) {
        for (compound_hull_vert* vert = Compound_hull_verts + cmp_hull1->points;; vert = ncd_next_vert(vert)) {
            if (vert->pos.x < min_x) {
                start_vert = vert;
                start_hull = 1;
                min_x = vert->pos.x;
            }
            if (vert->next == cmp_hull1->points) {
                break;
            }
        }
    }
    return true;
}

// 0x00AF7B70 (:2260) LTCG: convex_hull_consideration_list ECX
static bool compute_compound_hull_2d_internal(compound_hull* cmp_hull, unsigned short& convex_hull_consideration_list, int start_hull_vert_index)
{
    // Magic numbers: the outline walk's safety limit, and a marker on the hulls the walk has passed through.
    const int MAX_HULL_STEPS = 1000;
    compound_hull* const HULL_VISITED = (compound_hull*)0xFFFFFFFF;
    convex_hull_intersection current;
    int steps = 0;
    int cur_vert = 0;
    convex_hull_2d* cur_hull = nullptr;
    for (;;) {
        steps++;
        convex_hull_intersection previous = current;
        if (cur_hull == nullptr) {
            convex_hull_2d* first = Convex_hulls + convex_hull_consideration_list;
            current.hull0 = first;
            current.hull0_vert_indices[0] = start_hull_vert_index;
            current.hull0_vert_indices[1] = (start_hull_vert_index + 1) % first->num_points;
            cur_hull = first;
        } else if (current.ix_valid) {
            current.hull0 = current.hull1;
            current.hull0_vert_indices[0] = current.hull1_vert_indices[0];
            current.hull0_vert_indices[1] = current.hull1_vert_indices[1];
            cur_hull = current.hull0;
        } else {
            current.hull0_vert_indices[0] = cur_vert;
            current.hull0_vert_indices[1] = (cur_vert + 1) % cur_hull->num_points;
            if (cur_hull == Convex_hulls + convex_hull_consideration_list && cur_vert == start_hull_vert_index) {
                // Back at the start: the outline is closed. Every hull it touches or holds joins the compound hull.
                if (steps >= MAX_HULL_STEPS) {
                    return false;
                }
                eliminate_worthless_points(cmp_hull, nullptr, 0);
                unsigned short head = convex_hull_consideration_list;
                convex_hull_2d* hull = head == NCD_NONE ? nullptr : Convex_hulls + head;
                while (hull != nullptr) {
                    convex_hull_2d* next = hull->next == convex_hull_consideration_list ? nullptr : Convex_hulls + hull->next;
                    if (hull->cmp_hull == HULL_VISITED || is_convex_hull_contained_in_compound_hull(hull, cmp_hull)) {
                        attach_convex_hull_to_compound_hull(cmp_hull, hull, convex_hull_consideration_list);
                    }
                    hull = next;
                }
                return true;
            }
        }
        if (cur_hull->cmp_hull == nullptr) {
            cur_hull->cmp_hull = HULL_VISITED;
        }
        const convex_hull_2d* bounds_hull = cur_hull;

        convex_hull_intersection best;
        float best_dist_sq = 3.4028235e+38f;
        convex_hull_2d* hull0 = current.hull0;
        if (convex_hull_consideration_list != NCD_NONE) {
            for (convex_hull_2d* hull = Convex_hulls + convex_hull_consideration_list;; hull = Convex_hulls + hull->next) {
                if (hull != hull0) {
                    bool have_dir = false;
                    vector2 edge_dir;
                    float best_dot = 3.4028235e+38f;
                    for (int vert = 0; vert < hull->num_points; vert++) {
                        if (previous.ix_valid && hull == previous.hull0 && vert == previous.hull0_vert_indices[0]) {
                            continue;
                        }
                        int next_vert = (vert + 1) % hull->num_points;
                        vector2 edge_start = ncd_jittered_point(hull0, current.hull0_vert_indices[0]);
                        vector2 from = current.ix_valid ? current.ix_pos : edge_start;
                        vector2 ix;
                        if (!ncd_ix_edge_edge(from, ncd_jittered_point(hull0, current.hull0_vert_indices[1]), ncd_jittered_point(hull, vert), ncd_jittered_point(hull, next_vert), &ix)) {
                            continue;
                        }
                        float dist_sq = vec2_dist_squared(edge_start, ix);
                        bool take = false;
                        if (dist_sq < best_dist_sq) {
                            best_dot = 3.4028235e+38f;
                            best_dist_sq = dist_sq;
                            take = true;
                        } else {
                            float excess = dist_sq - best_dist_sq;
                            // Magic number: the same intersection point: take the sharper turn.
                            if (excess < 0.0001f && -excess < 0.0001f) {
                                if (!have_dir) {
                                    edge_dir = vector2(hull0->points[current.hull0_vert_indices[1]].x - hull0->points[current.hull0_vert_indices[0]].x,
                                        hull0->points[current.hull0_vert_indices[1]].y - hull0->points[current.hull0_vert_indices[0]].y);
                                    edge_dir.normalize_safe();
                                    have_dir = true;
                                }
                                if (best_dot == 3.4028235e+38f) {
                                    vector2 best_dir(best.hull1->points[best.hull1_vert_indices[1]].x - best.hull1->points[best.hull1_vert_indices[0]].x,
                                        best.hull1->points[best.hull1_vert_indices[1]].y - best.hull1->points[best.hull1_vert_indices[0]].y);
                                    best_dir.normalize_safe();
                                    best_dot = edge_dir.x * best_dir.x + edge_dir.y * best_dir.y;
                                }
                                vector2 test_dir(hull->points[next_vert].x - hull->points[vert].x, hull->points[next_vert].y - hull->points[vert].y);
                                test_dir.normalize_safe();
                                float dot = test_dir.x * edge_dir.x + test_dir.y * edge_dir.y;
                                if (dot < best_dot) {
                                    best_dot = dot;
                                    if (excess < 0.0f) {
                                        best_dist_sq = dist_sq;
                                    }
                                    take = true;
                                }
                            }
                        }
                        if (take) {
                            best = current;
                            best.ix_valid = true;
                            best.ix_pos = ix;
                            best.hull1 = hull;
                            best.hull1_vert_indices[0] = vert;
                            best.hull1_vert_indices[1] = next_vert;
                        }
                    }
                }
                if (hull->next == convex_hull_consideration_list) {
                    break;
                }
            }
        }

        vector2 point;
        if (best.ix_valid) {
            current = best;
            point = best.ix_pos;
        } else {
            point = ncd_jittered_point(current.hull0, current.hull0_vert_indices[1]);
            current.ix_valid = false;
        }
        unsigned short vert_index = Free_compound_hull_verts;
        if (vert_index == NCD_NONE) {
            return false;
        }
        bool add_vert = false;
        if (cmp_hull->points == NCD_NONE) {
            cmp_hull->bmin.x = point.x;
            cmp_hull->bmin.y = bounds_hull->bmin.y;
            cmp_hull->bmin.z = point.y;
            cmp_hull->bmax.x = point.x;
            cmp_hull->bmax.y = bounds_hull->bmax.y;
            cmp_hull->bmax.z = point.y;
            add_vert = true;
        } else {
            const compound_hull_vert& last = Compound_hull_verts[Compound_hull_verts[cmp_hull->points].prev];
            if (point.x != last.pos.x || point.y != last.pos.y) {
                cmp_hull->bmin.x = 0.0f <= cmp_hull->bmin.x - point.x ? point.x : cmp_hull->bmin.x;
                cmp_hull->bmin.y = 0.0f <= cmp_hull->bmin.y - bounds_hull->bmin.y ? bounds_hull->bmin.y : cmp_hull->bmin.y;
                cmp_hull->bmin.z = 0.0f <= cmp_hull->bmin.z - point.y ? point.y : cmp_hull->bmin.z;
                cmp_hull->bmax.x = cmp_hull->bmax.x - point.x < 0.0f ? point.x : cmp_hull->bmax.x;
                cmp_hull->bmax.y = cmp_hull->bmax.y - bounds_hull->bmax.y < 0.0f ? bounds_hull->bmax.y : cmp_hull->bmax.y;
                cmp_hull->bmax.z = cmp_hull->bmax.z - point.y < 0.0f ? point.y : cmp_hull->bmax.z;
                add_vert = true;
            }
        }
        if (add_vert) {
            Compound_hull_verts[vert_index].pos = point;
            Compound_hull_verts[vert_index].nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
            ncd_list_remove(Compound_hull_verts, Free_compound_hull_verts, vert_index);
            ncd_list_add(Compound_hull_verts, cmp_hull->points, vert_index);
        }
        if (steps >= MAX_HULL_STEPS) {
            return false;
        }
        cur_vert = current.hull0_vert_indices[1];
        cur_hull = current.hull0;
    }
}

// 0x00B073B0 (:2526)
static compound_hull* compute_compound_hull_2d(unsigned short& convex_hull_consideration_list)
{
    compound_hull* cmp_hull = allocate_compound_hull();
    if (cmp_hull == nullptr) {
        return nullptr;
    }
    // Magic number: rfg.exe tries 10 jitters.
    const int MAX_TRIES = 10;
    int tries = 0;
    for (;;) {
        convex_hull_2d* start_hull = nullptr;
        int start_vert = -1;
        if (convex_hull_consideration_list != NCD_NONE) {
            for (convex_hull_2d* hull = Convex_hulls + convex_hull_consideration_list;; hull = Convex_hulls + hull->next) {
                // Magic number: 2.5 cm.
                float jitter_x = fl_rand_range(-0.025f, 0.025f);
                float jitter_y = fl_rand_range(-0.025f, 0.025f);
                hull->jitter.y = jitter_y;
                hull->jitter.x = jitter_x;
                for (int i = 0; i < hull->num_points; i++) {
                    if (start_vert < 0 || hull->points[i].x < start_hull->points[start_vert].x) {
                        start_hull = hull;
                        start_vert = i;
                    }
                }
                if (hull->next == convex_hull_consideration_list) {
                    break;
                }
            }
        }
        bool done = start_hull == nullptr || start_vert < 0;
        if (!done) {
            // The walk starts from the head: the westmost hull goes there.
            unsigned short start_index = (unsigned short)(start_hull - Convex_hulls);
            if (start_index != convex_hull_consideration_list) {
                ncd_list_remove(Convex_hulls, convex_hull_consideration_list, start_index);
                ncd_list_add(Convex_hulls, convex_hull_consideration_list, start_index);
                convex_hull_consideration_list = start_index;
            }
            done = compute_compound_hull_2d_internal(cmp_hull, convex_hull_consideration_list, start_vert);
        }
        if (done) {
            if (tries >= MAX_TRIES) {
                release_compound_hull(cmp_hull, nullptr);
                return nullptr;
            }
            return cmp_hull;
        }
        ncd_list_merge(Compound_hull_verts, Free_compound_hull_verts, cmp_hull->points);
        cmp_hull->points = NCD_NONE;
        if (convex_hull_consideration_list != NCD_NONE) {
            unsigned short hull = convex_hull_consideration_list;
            do {
                if (Convex_hulls[hull].cmp_hull == (compound_hull*)0xFFFFFFFF) {
                    Convex_hulls[hull].cmp_hull = nullptr;
                }
                hull = Convex_hulls[hull].next;
            } while (hull != convex_hull_consideration_list);
        }
        tries++;
        if (tries >= MAX_TRIES) {
            release_compound_hull(cmp_hull, nullptr);
            return nullptr;
        }
    }
}

// 0x00AF0FB0 (:2661) LTCG: request_handle ECX
static nav_cell_detour_request* get_detour_request(unsigned int request_handle)
{
    if (request_handle == 0xFFFFFFFF) {
        return nullptr;
    }
    nav_cell_detour_request* request = Nav_cell_detour_requests + (request_handle & 0xFFF);
    return request->request_handle == request_handle ? request : nullptr;
}

// 0x00AF8780 (:2710) LTCG: objp ESI, component_index CX
static bool ncd_request_disabled_inherent(object* objp, short component_index)
{
    switch (objp->get_obj_type()) {
    case OT_HUMAN: {
        human* hp = (human*)objp;
        if (!human_is_ragdolled(hp) && !human_in_vehicle_interaction(hp, nullptr) && (int)hp->movement_mode != 2 && (int)hp->movement_mode != 3) {
            return false;
        }
        return true;
    }
    case OT_ITEM:
    case OT_TURRET:
        return false;
    case OT_MOVER: {
        object_mover* om = (object_mover*)objp;
        // Magic numbers: rfg.exe's offsets of the sub type 1 mover's dynamic body and the motion type of a fixed body.
        if (objp->get_sub_type() == 1) {
            rfg_mover* rm = (rfg_mover*)om;
            if ((int)rm->get_move_type() == 0 && (om->mover_flags & 0x20) == 0) {
                return true;
            }
        } else {
            level_object_inst_properties* props = om->m_level_obj_properties;
            if ((props->m_base_properties->m_max_dislodge_hitpoints == 0 || props->m_dislodge_hitpoints > 0) && havok_body_get_motion_type(objp->havok_handle) == 5) {
                return true;
            }
        }
        return ncd_object_is_flat(objp);
    }
    case OT_VEHICLE: {
        vehicle* vp = (vehicle*)objp;
        if (vp->rdd.is_loaded() && (component_index < 0 || vehicle_component_is_attached(vp, component_index))) {
            // Magic number: 6 = walker; a walker in its stand state 1 or 0x6A blocks only the entering player.
            if (objp->get_sub_type() != 6) {
                return false;
            }
            int state = vp->vp->desired_state->index;
            if (state == 1 || state == 0x6A) {
                player* hp = player_get_local_player();
                if (hp == nullptr || !human_in_vehicle_transition(hp, vp, VI_INTERACTION_NONE)) {
                    return false;
                }
            }
        }
        return true;
    }
    default:
        return true;
    }
}

// 0x00AF0FE0 (:2818) LTCG: objp EAX
static float ncd_get_exist_distance(object* objp)
{
    switch (objp->get_obj_type()) {
    case OT_HUMAN:
    case OT_VEHICLE:
    case OT_TURRET:
        return -1.0f;
    case OT_ITEM:
    case OT_MOVER:
        return Ncd_mover_active_dist_from_camera;
    default:
        return 0.0f;
    }
}

// 0x00AF1030 (:2840) LTCG: objp EAX
static bool ncd_request_disabled_velocity(object* objp)
{
    // Magic numbers: a request with a hull keeps it until the object moves faster than the lower speed.
    float max_speed;
    vector velocity;
    switch (objp->get_obj_type()) {
    case OT_HUMAN:
        return !(((human*)objp)->actual_velocity_mag <= 0.01f);
    case OT_ITEM:
        max_speed = Nav_cell_detour_requests[((item*)objp)->m_nav_cell_detour_request_handle & 0xFFF].convex_hull_idx == NCD_NONE ? 0.4f : 0.05f;
        break;
    case OT_MOVER:
        max_speed = Nav_cell_detour_requests[((object_mover*)objp)->nav_cell_detour_request_handle & 0xFFF].convex_hull_idx == NCD_NONE ? 0.4f : 0.05f;
        break;
    case OT_VEHICLE: {
        max_speed = Nav_cell_detour_requests[((vehicle*)objp)->m_nav_cell_detour_request_handle & 0xFFF].convex_hull_idx == NCD_NONE ? 0.3f : 0.2f;
        vehicle_get_velocity((vehicle*)objp, velocity);
        return max_speed * max_speed < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    }
    case OT_TURRET:
        return false;
    default:
        return true;
    }
    havok_body_get_linear_velocity(objp->havok_handle, velocity);
    return !(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z <= max_speed * max_speed);
}

// 0x00AF1200 (:2933)
static bool ncd_request_disabled_position(object* objp)
{
    switch (objp->get_obj_type()) {
    case OT_HUMAN:
    case OT_VEHICLE:
        return false;
    case OT_ITEM:
    case OT_MOVER:
    case OT_TURRET: {
        vector bmin, bmax;
        objp->get_local_bbox(bmin, bmax);
        // Magic number: a human's height below the object.
        vector grow(0.0f, -1.9f, 0.0f);
        bbox_grow(bmin, bmax, grow);
        nav_cell* cell;
        return havok_collect_nav_cells(bmin, bmax, objp->pos, objp->orient, &cell, 1, false, nullptr) == 0;
    }
    default:
        return true;
    }
}

// 0x00AF88A0 (:2976) LTCG: request ESI
static bool ncd_update_request_disabled_flags(nav_cell_detour_request* request, bool& clean_up)
{
    // disabled_flags (the names are not recovered): 1 set by nav_cell_detour_set_dont_update, then what the code below sets.
    const unsigned char DISABLED_DONT_UPDATE = 0x01;
    const unsigned char DISABLED_INHERENT = 0x04;
    const unsigned char DISABLED_DISTANCE = 0x08;
    const unsigned char DISABLED_VELOCITY = 0x10;
    const unsigned char DISABLED_POSITION = 0x20;
    clean_up = false;
    if ((request->disabled_flags & DISABLED_DONT_UPDATE) != 0) {
        return false;
    }
    object* objp = World.object_from_handle(request->object_handle);
    if (objp == nullptr) {
        clean_up = true;
        return false;
    }
    if (ncd_request_disabled_inherent(objp, request->component_index)) {
        request->disabled_flags |= DISABLED_INHERENT;
        return false;
    }
    request->disabled_flags &= ~DISABLED_INHERENT;
    float exist_distance = ncd_get_exist_distance(objp);
    if (exist_distance >= 0.0f && exist_distance * exist_distance < vec_dist_squared(Rfg_camera.real_pos, objp->pos)) {
        request->disabled_flags |= DISABLED_DISTANCE;
        return false;
    }
    request->disabled_flags &= ~DISABLED_DISTANCE;
    if (ncd_request_disabled_velocity(objp)) {
        request->disabled_flags |= DISABLED_VELOCITY;
        return false;
    }
    request->disabled_flags &= ~DISABLED_VELOCITY;
    // Magic number: half a metre moved.
    if (0.25f <= vec_dist_squared(objp->pos, request->last_update_pos)) {
        if (ncd_request_disabled_position(objp)) {
            request->disabled_flags |= DISABLED_POSITION;
        } else {
            request->disabled_flags &= ~DISABLED_POSITION;
        }
        request->last_update_pos = objp->pos;
        return true;
    }
    return false;
}

// 0x00AF89F0 (:3257)
static void create_convex_hull_for_request(nav_cell_detour_request* request)
{
    object* objp = World.object_from_handle(request->object_handle);
    if (objp == nullptr) {
        return;
    }
    unsigned short hull_index = Free_convex_hulls;
    if (hull_index == NCD_NONE) {
        return;
    }
    convex_hull_2d* hull = Convex_hulls + hull_index;
    bool high_priority = false;
    bool from_destruction = false;
    bool dislodged_mover = false;
    vector pos, bmin, bmax;
    matrix orient;
    // Magic numbers below: rfg.exe's clearance around each kind of object, in metres.
    float expansion = 0.6f;
    switch (objp->get_obj_type()) {
    case OT_HUMAN:
        pos = objp->pos;
        orient = objp->orient;
        bmin = vector(-0.25f, 0.0f, -0.25f);
        bmax = vector(0.25f, 1.9f, 0.25f);
        expansion = 0.3125f;
        break;
    case OT_MOVER:
        from_destruction = (((object_mover*)objp)->mover_flags & 4) != 0;
        dislodged_mover = ((object_mover*)objp)->m_level_obj_properties->m_dislodge_hitpoints < 1;
        // fall through
    case OT_ITEM:
    case OT_TURRET:
        pos = objp->pos;
        orient = objp->orient;
        objp->get_local_bbox(bmin, bmax);
        break;
    case OT_VEHICLE: {
        vehicle* vp = (vehicle*)objp;
        high_priority = true;
        if (request->component_index < 0) {
            pos = objp->pos;
            orient = objp->orient;
            if (!havok_body_get_aabb(objp->havok_handle, ZERO_VECTOR, IDENTITY_MATRIX, bmin, bmax)) {
                objp->get_local_bbox(bmin, bmax);
                // Magic numbers: sub type 5's box, whose physics can be missing.
                if (objp->get_sub_type() == 5) {
                    bmin.x = -3.95f;
                    bmin.z = -9.105f;
                    bmax.x = 3.95f;
                    bmax.z = 3.105f;
                }
            }
            if (vp->m_flags.m_corpse != 0) {
                expansion = 0.6f;
                break;
            }
            if (objp->get_sub_type() == 6) {
                player* local_player = player_get_local_player();
                bool player_inside = local_player != nullptr && human_in_vehicle(local_player, vp);
                const vehicle_info* info = vp->info;
                if (info == Nav_cell_detour_veh_info_walker_flamer) {
                    bmin.x -= 1.0f;
                    bmax.x += 1.0f;
                    expansion = player_inside ? 1.6f : 1.3f;
                } else if (info == Nav_cell_detour_veh_info_walker_min_1 || info == Nav_cell_detour_veh_info_walker_min_2 || info == Nav_cell_detour_veh_info_walker_min_3) {
                    bmin.z += 0.75f;
                    bmax.z += 0.75f;
                    expansion = player_inside ? 2.1f : 1.6f;
                } else {
                    bmin.z += 0.75f;
                    bmax.z += 0.75f;
                    if (player_inside) {
                        expansion = 3.35f;
                    } else {
                        expansion = 2.6f;
                        bmin.z -= 0.75f;
                        bmax.z += 0.75f;
                    }
                }
            } else {
                expansion = 0.6f;
            }
            if (vp->info == Nav_cell_detour_veh_info_mar_raider_1 && vp->m_flags.m_corpse == 0) {
                bmin.z += 4.0f;
            }
        } else {
            vehicle_component* component = &vp->rdd.get_data()->m_components[request->component_index];
            pos = component->get_world_space_position();
            orient = component->get_world_space_rotation();
            component->get_local_bbox(vp, bmin, bmax);
            bmin.z -= 0.5f;
            bmax.z += 0.5f;
            // Original quirk kept: VCT_DOOR is a hash global (vehicle_component.cpp), compared by value.
            expansion = component->get_data()->m_type == VCT_DOOR ? 0.0f : 0.6f;
        }
        break;
    }
    default:
        return;
    }
    compute_convex_hull_from_oriented_bbox(hull, pos, orient, bmin, bmax, expansion);
    hull->jitter.zero();
    hull->cmp_hull = nullptr;
    hull->request_obj_type = (signed char)objp->get_obj_type();
    hull->request_obj_subtype = (signed char)objp->get_sub_type();
    hull->flags.high_priority = high_priority ? 1 : 0;
    hull->flags.from_destruction = from_destruction ? 1 : 0;
    hull->flags.dislodged_mover = dislodged_mover ? 1 : 0;
    ncd_list_remove(Convex_hulls, Free_convex_hulls, hull_index);
    ncd_list_add(Convex_hulls, Unincorporated_convex_hulls, hull_index);
    request->convex_hull_idx = hull_index;
    request->last_update_pos = objp->pos;
    request->disabled_flags = 0;
    if (ncd_request_disabled_position(objp)) {
        request->disabled_flags |= 0x20;
    }
}

// 0x00AF12C0 (:3472) LTCG: convex_hull EDI
static bool potentially_remove_convex_hull_quickly(const convex_hull_2d* convex_hull)
{
    const compound_hull* cmp_hull = convex_hull->cmp_hull;
    if (cmp_hull->convex_hull_count == 1) {
        return false;
    }
    // The hull can leave without rebuilding only if the outline uses none of its points.
    for (int i = 0; i < convex_hull->num_points; i++) {
        vector2 point(convex_hull->points[i].x + convex_hull->jitter.x, convex_hull->points[i].y + convex_hull->jitter.y);
        if (cmp_hull->points != NCD_NONE) {
            for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
                if (vec2_dist_squared(point, vert->pos) < 0.0001f) {
                    return false;
                }
                if (vert->next == cmp_hull->points) {
                    break;
                }
            }
        }
    }
    return true;
}

static void add_nav_cell_references_to_compound_hull(compound_hull* cmp_hull); // 0x00B13130 (:3836), below

// 0x00B17A60 (:3514) LTCG: convex_hull in EDI is the request's hull
static void destroy_convex_hull_for_request(nav_cell_detour_request* request, bool recreate_detour)
{
    unsigned short hull_index = request->convex_hull_idx;
    if (hull_index == NCD_NONE) {
        return;
    }
    convex_hull_2d* hull = Convex_hulls + hull_index;
    unsigned short detached = NCD_NONE;
    bool rebuild = false;
    if (hull->cmp_hull == nullptr) {
        ncd_list_remove(Convex_hulls, Unincorporated_convex_hulls, hull_index);
    } else if (potentially_remove_convex_hull_quickly(hull)) {
        ncd_list_remove(Convex_hulls, hull->cmp_hull->convex_hulls, hull_index);
        hull->cmp_hull = nullptr;
    } else if (!recreate_detour) {
        release_compound_hull(hull->cmp_hull, nullptr);
        ncd_list_remove(Convex_hulls, Unincorporated_convex_hulls, hull_index);
    } else {
        release_compound_hull(hull->cmp_hull, &detached);
        ncd_list_remove(Convex_hulls, detached, hull_index);
        rebuild = detached != NCD_NONE;
    }
    ncd_list_add(Convex_hulls, Free_convex_hulls, hull_index);
    request->convex_hull_idx = NCD_NONE;
    if (rebuild) {
        compound_hull* cmp_hull = compute_compound_hull_2d(detached);
        if (cmp_hull != nullptr) {
            cmp_hull->flags.combinations_considered = 0;
            cmp_hull->flags.nav_cells_computed = 0;
            add_nav_cell_references_to_compound_hull(cmp_hull);
        }
        if (detached != NCD_NONE) {
            ncd_list_merge(Convex_hulls, detached, Unincorporated_convex_hulls);
            Unincorporated_convex_hulls = detached;
        }
    }
}

static int Max_detour_requests_processed_per_frame = 200; // 0x016B01D8
static int Max_detour_convex_hulls_created_per_frame = 10; // 0x016B01DC
static int Max_detour_convex_hulls_destroyed_per_frame = 2; // 0x016B01E0

// 0x00B1E710 (:3589)
static void process_convex_hulls_for_requests()
{
    int num_processed = 0;
    int num_created = 0;
    int num_destroyed = 0;
    nav_cell_detour_request* next = Active_nav_cell_detour_requests == NCD_NONE ? nullptr : Nav_cell_detour_requests + Active_nav_cell_detour_requests;
    while (next != nullptr) {
        nav_cell_detour_request* request = next;
        next = request->next == Active_nav_cell_detour_requests ? nullptr : Nav_cell_detour_requests + request->next;
        if (Max_detour_requests_processed_per_frame <= num_processed || Max_detour_convex_hulls_created_per_frame < num_created || Max_detour_convex_hulls_destroyed_per_frame < num_destroyed) {
            // Out of budget: the next frame resumes from this request. rfg.exe splits and rejoins the circular list, which
            // leaves it as it was with a new head.
            if (Active_nav_cell_detour_requests != NCD_NONE) {
                Active_nav_cell_detour_requests = (unsigned short)(request - Nav_cell_detour_requests);
            }
            return;
        }
        num_processed++;
        bool clean_up = false;
        bool moved = ncd_update_request_disabled_flags(request, clean_up);
        if (clean_up) {
            nav_cell_detour_remove_request(request->request_handle);
        } else if (request->disabled_flags == 0) {
            if (request->convex_hull_idx == NCD_NONE) {
                create_convex_hull_for_request(request);
                num_created++;
            } else if (moved) {
                destroy_convex_hull_for_request(request, false);
                create_convex_hull_for_request(request);
                num_created++;
            }
        } else if (request->convex_hull_idx != NCD_NONE) {
            if (Convex_hulls[request->convex_hull_idx].request_obj_type == OT_HUMAN) {
                destroy_convex_hull_for_request(request, true);
                num_destroyed++;
            } else {
                destroy_convex_hull_for_request(request, false);
            }
        }
    }
}

// 0x00B076E0 (:3690) LTCG: compound_hull_pos EAX, nav_cell_pos ECX
static bool nav_cell_corresponds_to_higher_level_geometry(const vector& compound_hull_pos, const vector& nav_cell_pos)
{
    if (compound_hull_pos.y <= nav_cell_pos.y) {
        return false;
    }
    raycast_input in;
    in.radius = 0.0f;
    in.start = nav_cell_pos;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.end = compound_hull_pos;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_flags = 0;
    in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    raycast_output out;
    if (!havok_raycast_against_static_level_only(in, out)) {
        return false;
    }
    // A hit from below means the hull stands on a floor above this cell.
    for (int i = 0; i < out.m_hits.getSize(); i++) {
        if (out.m_hits[i].hit_normal.y < 0.0f) {
            return true;
        }
    }
    return false;
}

// 0x00B0B820 (:3726)
static void compute_vertex_nav_cells(compound_hull* cmp_hull, int num_cells_to_search, const nav_cell* const* cells_to_search)
{
    if (cmp_hull->points == NCD_NONE) {
        return;
    }
    for (compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
        if (vert->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull || nav_cell_from_handle(vert->nav_cell_handle) == nullptr) {
            vert->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
            for (int i = 0; i < num_cells_to_search; i++) {
                if (nav_cell_is_point_in_cell_column(cells_to_search[i], vert->pos)) {
                    vert->nav_cell_handle = nav_cell_get_handle(cells_to_search[i]);
                    break;
                }
            }
        }
        if (vert->next == cmp_hull->points) {
            return;
        }
    }
}

// 0x00B022E0 (:3756)
static bool ncd_navpoint_collector(object* hit_navp, unsigned int shape_key, const void* hull_ptr)
{
    const compound_hull* cmp_hull = (const compound_hull*)hull_ptr;
    navpoint* navp = nullptr;
    if (hit_navp->get_obj_type() == OT_NAVPOINT) {
        // Magic numbers: only the node types 10 to 16 (path, transition and jump nodes) can be blocked.
        navpoint* candidate = (navpoint*)hit_navp;
        if (candidate->type <= 9 || candidate->type >= 17) {
            return false;
        }
        navp = candidate;
    }
    // Magic number: half a human's height under the hull.
    vector grown_min = cmp_hull->bmin;
    grown_min.y -= 0.95f;
    bool blocks_point = false;
    if (ix_point_in_box(hit_navp->pos, grown_min, cmp_hull->bmax) && nav_cell_detour_is_point_in_compound_hull(vector2(hit_navp->pos.x, hit_navp->pos.z), cmp_hull)) {
        blocks_point = true;
    }
    if (navp == nullptr) {
        if (blocks_point) {
            if (hit_navp->get_obj_type() == OT_GUARD_NODE) {
                ncd_add_guard_node_block_ref((object_guard_node*)hit_navp, const_cast<compound_hull*>(cmp_hull));
            } else if (hit_navp->get_obj_type() == OT_ACTION_NODE && !((object_action_node*)hit_navp)->allow_player()) {
                ncd_add_action_node_block_ref((object_action_node*)hit_navp, const_cast<compound_hull*>(cmp_hull));
            }
        }
        return false;
    }
    unsigned short blocked_links = 0;
    for (int i = 0; i < navp->num_links; i++) {
        const navpoint* other = navp->links[i];
        vector start = navp->pos;
        vector end = other->pos;
        vector dir = end - start;
        dir.mag_and_normalize_safe();
        start.x += dir.x * navp->radius;
        start.y += dir.y * navp->radius;
        start.z += dir.z * navp->radius;
        end.x -= dir.x * other->radius;
        end.y -= dir.y * other->radius;
        end.z -= dir.z * other->radius;
        vector ix;
        if (ix_linesegment_boundingbox(ix, grown_min, cmp_hull->bmax, start, end) && nav_cell_detour_is_line_in_compound_hull(start, end, cmp_hull)) {
            blocked_links |= (unsigned short)(1 << i);
        }
    }
    if (blocks_point || blocked_links != 0) {
        ncd_add_navpoint_block_ref(navp, const_cast<compound_hull*>(cmp_hull), blocks_point, (short)blocked_links);
    }
    return false;
}

// 0x00B13130 (:3836)
static void add_nav_cell_references_to_compound_hull(compound_hull* cmp_hull)
{
    // Magic numbers: the height window (metres) in which a cell counts as under the hull, and rfg.exe's cell list size.
    const float MAX_CELL_HEIGHT_DELTA = 3.8f;
    const int MAX_CELLS = 256;

    // The two furthest points give the centre and radius of the search.
    const compound_hull_vert* far0 = nullptr;
    const compound_hull_vert* far1 = nullptr;
    float far_dist_sq = -1.0f;
    if (cmp_hull->points != NCD_NONE) {
        far0 = far1 = Compound_hull_verts + cmp_hull->points;
        const compound_hull_vert* last = Compound_hull_verts + Compound_hull_verts[cmp_hull->points].prev;
        for (const compound_hull_vert* vert = far0; vert != last; vert = ncd_next_vert(vert)) {
            for (const compound_hull_vert* other = ncd_next_vert(vert);; other = ncd_next_vert(other)) {
                float dist_sq = vec2_dist_squared(vert->pos, other->pos);
                if (far_dist_sq < dist_sq) {
                    far0 = vert;
                    far1 = other;
                    far_dist_sq = dist_sq;
                }
                if (other->next == cmp_hull->points) {
                    break;
                }
            }
        }
    }
    vector center((far1->pos.x + far0->pos.x) * 0.5f, cmp_hull->bmin.y, (far1->pos.y + far0->pos.y) * 0.5f);
    // Magic number: 14.44 = 3.8 squared, the smallest search radius.
    float radius = far_dist_sq <= 14.44f ? 3.8f : sqrtf(far_dist_sq);
    if (cmp_hull->flags.contains_vehicle != 0 || cmp_hull->flags.contains_destruction != 0 || cmp_hull->flags.contains_dislodged_mover != 0) {
        unsigned int unused_handle;
        World.collect_objects<navpoint>(center, radius * 2.0f, &unused_handle, 1, ncd_navpoint_collector, cmp_hull);
        World.collect_objects<object_action_node>(center, radius, &unused_handle, 1, ncd_navpoint_collector, cmp_hull);
        World.collect_objects<object_guard_node>(center, radius, &unused_handle, 1, ncd_navpoint_collector, cmp_hull);
    }
    nav_cell* cells[MAX_CELLS];
    bool referenced[MAX_CELLS];
    int num_cells = havok_collect_nav_cells(center, radius, cells, MAX_CELLS, false);
    if (num_cells < 1) {
        cmp_hull->flags.nav_cells_computed = 1;
        return;
    }
    if (Active_compound_hull_references != NCD_NONE) {
        for (const compound_hull_ref* ref = Compound_hull_references + Active_compound_hull_references;; ref = Compound_hull_references + ref->allocation_next) {
            if (ref->cmp_hull == cmp_hull) {
                for (int i = 0; i < num_cells; i++) {
                    if (cells[i] == ref->ncp) {
                        num_cells--;
                        cells[i] = cells[num_cells];
                        break;
                    }
                }
            }
            if (ref->allocation_next == Active_compound_hull_references) {
                break;
            }
        }
    }
    bbox2d hull_box;
    hull_box.min = vector2(cmp_hull->bmin.x, cmp_hull->bmin.z);
    hull_box.max = vector2(cmp_hull->bmax.x, cmp_hull->bmax.z);
    for (int i = 0; i < num_cells; i++) {
        bbox2d cell_box;
        nav_cell_get_xz_bounding_box(&cell_box.min, &cell_box.max, cells[i]);
        if (!ix2d_bbox_bbox(cell_box, hull_box)) {
            num_cells--;
            cells[i] = cells[num_cells];
            i--;
        }
    }
    for (int i = 0; i < num_cells; i++) {
        bool found = false;
        unsigned short first = cells[i]->compound_hull_reference;
        if (first != NCD_NONE) {
            for (const compound_hull_ref* ref = Compound_hull_references + first;; ref = Compound_hull_references + ref->references_next) {
                if (ref->cmp_hull == cmp_hull) {
                    found = true;
                    break;
                }
                if (ref->references_next == first) {
                    break;
                }
            }
        }
        referenced[i] = found;
    }
    auto erase_cell = [&](int index) {
        int count = num_cells - index - 1;
        if (count > 0) {
            memmove(&cells[index], &cells[index + 1], count * sizeof(nav_cell*));
            memmove(&referenced[index], &referenced[index + 1], count);
        }
        num_cells--;
    };

    // Each hull vertex references the cell under it, nearest in height; a cell on another floor is dropped.
    if (cmp_hull->points != NCD_NONE) {
        for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
            int best = -1;
            float best_delta = -1.0f;
            for (int i = 0; i < num_cells; i++) {
                if (!nav_cell_is_point_in_cell_column(cells[i], vert->pos)) {
                    continue;
                }
                vector on_hull(vert->pos.x, cmp_hull->bmin.y, vert->pos.y);
                vector on_cell = nav_cell_force_point_to_cell(cells[i], on_hull);
                float delta = fabsf(on_hull.y - on_cell.y);
                if (delta < MAX_CELL_HEIGHT_DELTA) {
                    vector hull_top(vert->pos.x, cmp_hull->bmax.y, vert->pos.y);
                    vector cell_pos(vert->pos.x, cmp_hull->bmin.y - (on_hull.y - on_cell.y), vert->pos.y);
                    if (nav_cell_corresponds_to_higher_level_geometry(hull_top, cell_pos)) {
                        erase_cell(i);
                        i--;
                    } else if (best_delta < 0.0f || delta < best_delta) {
                        best_delta = delta;
                        best = i;
                    }
                }
            }
            if (best >= 0 && !referenced[best]) {
                add_compound_hull_reference(cells[best], cmp_hull);
                referenced[best] = true;
            }
            if (vert->next == cmp_hull->points) {
                break;
            }
        }
    }
    // Cells with a vertex inside the hull.
    for (int i = 0; i < num_cells; i++) {
        if (referenced[i]) {
            continue;
        }
        for (int v = 0; v < 3; v++) {
            const vector* cell_vert = nav_cell_get_vertex(cells[i], v, nullptr);
            vector on_hull(cell_vert->x, cmp_hull->bmin.y, cell_vert->z);
            vector on_cell = nav_cell_force_point_to_cell(cells[i], on_hull);
            if (fabsf(on_hull.y - on_cell.y) < MAX_CELL_HEIGHT_DELTA && nav_cell_detour_is_point_in_compound_hull(vector2(cell_vert->x, cell_vert->z), cmp_hull)) {
                vector hull_top(cell_vert->x, cmp_hull->bmax.y, cell_vert->z);
                if (nav_cell_corresponds_to_higher_level_geometry(hull_top, *cell_vert)) {
                    erase_cell(i);
                    i--;
                } else {
                    add_compound_hull_reference(cells[i], cmp_hull);
                    referenced[i] = true;
                }
                break;
            }
        }
    }
    // Cells an outline edge crosses.
    for (int i = 0; i < num_cells; i++) {
        if (referenced[i]) {
            continue;
        }
        bool handled = false;
        for (int edge = 0; edge < 3 && !handled && cmp_hull->points != NCD_NONE; edge++) {
            for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
                vector2 ix;
                if (nav_cell_classify_segment_vs_edge(cells[i], edge, vert->pos, ncd_next_vert(vert)->pos, &ix) == LINE_CLASS_SEGMENTS_INTERSECT) {
                    vector on_hull(ix.x, cmp_hull->bmin.y, ix.y);
                    vector on_cell = nav_cell_force_point_to_cell(cells[i], on_hull);
                    if (fabsf(on_hull.y - on_cell.y) < MAX_CELL_HEIGHT_DELTA) {
                        vector hull_top(ix.x, cmp_hull->bmax.y, ix.y);
                        vector cell_pos(ix.x, cmp_hull->bmin.y - (on_hull.y - on_cell.y), ix.y);
                        if (nav_cell_corresponds_to_higher_level_geometry(hull_top, cell_pos)) {
                            erase_cell(i);
                            i--;
                        } else {
                            add_compound_hull_reference(cells[i], cmp_hull);
                            referenced[i] = true;
                        }
                        handled = true;
                        break;
                    }
                }
                if (vert->next == cmp_hull->points) {
                    break;
                }
            }
        }
    }
    for (int i = 0; i < num_cells; i++) {
        if (!referenced[i]) {
            erase_cell(i);
            i--;
        }
    }
    compute_vertex_nav_cells(cmp_hull, num_cells, cells);
    cmp_hull->flags.nav_cells_computed = 1;
}

// 0x00B07830 (:4123)
static compound_hull* create_compound_hull(unsigned short& convex_hull_consideration_list)
{
    compound_hull* cmp_hull = compute_compound_hull_2d(convex_hull_consideration_list);
    if (cmp_hull != nullptr) {
        cmp_hull->flags.combinations_considered = 0;
        cmp_hull->flags.nav_cells_computed = 0;
    }
    return cmp_hull;
}

// Gathers the unincorporated hulls overlapping the group into list, starting from the group's first hull: both
// create_compound_hull_from_convex_hulls and create_compound_hulls inline this.
static void ncd_gather_overlapping_hulls(unsigned short& list, int& num_hulls, bbox& box)
{
    // Magic numbers: at most 15 hulls, 100 square metres.
    const int MAX_GATHERED_HULLS = 15;
    const float MAX_GATHERED_AREA = 100.0f;
    for (;;) {
        unsigned short head = Unincorporated_convex_hulls;
        convex_hull_2d* next = head == NCD_NONE ? nullptr : Convex_hulls + head;
        convex_hull_2d* found = nullptr;
        while (next != nullptr) {
            convex_hull_2d* hull = next;
            next = hull->next == head ? nullptr : Convex_hulls + hull->next;
            if (hull->cmp_hull != nullptr) {
                continue;
            }
            if (num_hulls == MAX_GATHERED_HULLS) {
                return;
            }
            if (num_hulls < 1) {
                box.min = hull->bmin;
                box.max = hull->bmax;
                found = hull;
                break;
            }
            if (box.max.x <= hull->bmin.x || box.max.y <= hull->bmin.y || box.max.z <= hull->bmin.z
                || hull->bmax.x <= box.min.x || hull->bmax.y <= box.min.y || hull->bmax.z <= box.min.z) {
                continue;
            }
            bbox_expand(box, hull->bmin, hull->bmax);
            found = hull;
            break;
        }
        if (found == nullptr) {
            return;
        }
        unsigned short index = (unsigned short)(found - Convex_hulls);
        num_hulls++;
        ncd_list_remove(Convex_hulls, Unincorporated_convex_hulls, index);
        ncd_list_add(Convex_hulls, list, index);
        list = index;
        if (MAX_GATHERED_AREA < (box.max.x - box.min.x) * (box.max.z - box.min.z)) {
            return;
        }
    }
}

// 0x00B07850 (:4150)
compound_hull* create_compound_hull_from_convex_hulls(bool& consumed_cpu, convex_hull_2d* start_hull)
{
    consumed_cpu = false;
    unsigned short list = NCD_NONE;
    int num_hulls = 0;
    bbox box;
    if (start_hull != nullptr) {
        box.min = start_hull->bmin;
        box.max = start_hull->bmax;
        unsigned short index = (unsigned short)(start_hull - Convex_hulls);
        num_hulls = 1;
        ncd_list_remove(Convex_hulls, Unincorporated_convex_hulls, index);
        ncd_list_add(Convex_hulls, list, index);
    }
    ncd_gather_overlapping_hulls(list, num_hulls, box);
    if (list == NCD_NONE) {
        return nullptr;
    }
    consumed_cpu = true;
    compound_hull* cmp_hull = create_compound_hull(list);
    if (Unincorporated_convex_hulls == NCD_NONE) {
        Unincorporated_convex_hulls = list;
    } else {
        ncd_list_merge(Convex_hulls, Unincorporated_convex_hulls, list);
    }
    return cmp_hull;
}

// 0x00B13D30 (:4259)
static bool associate_compound_hulls_with_nav_cells(int num_to_associate)
{
    int num_associated = 0;
    if (Active_compound_hulls != NCD_NONE) {
        compound_hull* cmp_hull = Compound_hulls + Active_compound_hulls;
        for (;;) {
            if (cmp_hull->flags.nav_cells_computed == 0) {
                add_nav_cell_references_to_compound_hull(cmp_hull);
                num_associated++;
                if (num_to_associate <= num_associated) {
                    break;
                }
            }
            if (cmp_hull->next == Active_compound_hulls) {
                break;
            }
            cmp_hull = Compound_hulls + cmp_hull->next;
        }
    }
    return num_associated > 0;
}

// 0x00B07C10 (:4286)
static bool create_compound_hulls(int num_to_create)
{
    int num_created = 0;
    for (;;) {
        unsigned short list = NCD_NONE;
        int num_hulls = 0;
        bbox box;
        ncd_gather_overlapping_hulls(list, num_hulls, box);
        if (list == NCD_NONE) {
            break;
        }
        create_compound_hull(list);
        if (Unincorporated_convex_hulls == NCD_NONE) {
            Unincorporated_convex_hulls = list;
        } else {
            ncd_list_merge(Convex_hulls, Unincorporated_convex_hulls, list);
        }
        num_created++;
        if (num_created >= num_to_create) {
            break;
        }
    }
    return num_created > 0;
}

// 0x00AF13B0 (:4310)
static convex_hull_2d* check_for_unincorportated_high_priority_convex_hull()
{
    if (Unincorporated_convex_hulls != NCD_NONE) {
        for (convex_hull_2d* hull = Convex_hulls + Unincorporated_convex_hulls;; hull = Convex_hulls + hull->next) {
            if (hull->flags.high_priority != 0) {
                return hull;
            }
            if (hull->next == Unincorporated_convex_hulls) {
                break;
            }
        }
    }
    return nullptr;
}

// 0x00B0B970 (:4326)
static bool process_compound_hull_combination()
{
    if (Free_compound_hulls == NCD_NONE || Active_compound_hulls == NCD_NONE) {
        return false;
    }
    compound_hull* cmp_hull0 = Compound_hulls + Active_compound_hulls;
    while (cmp_hull0->flags.nav_cells_computed == 0 || cmp_hull0->flags.combinations_considered != 0) {
        if (cmp_hull0->next == Active_compound_hulls) {
            return false;
        }
        cmp_hull0 = Compound_hulls + cmp_hull0->next;
    }
    for (compound_hull* other = Compound_hulls + Active_compound_hulls;; other = Compound_hulls + other->next) {
        if (other->flags.combinations_considered != 0 && ix_box_box_aligned(cmp_hull0->bmin, cmp_hull0->bmax, other->bmin, other->bmax)) {
            if (does_compound_hull_contain_hull(cmp_hull0, other)) {
                combine_contained_hull_2d_internal(cmp_hull0, other);
                return true;
            }
            if (does_compound_hull_contain_hull(other, cmp_hull0)) {
                combine_contained_hull_2d_internal(other, cmp_hull0);
                other->flags.combinations_considered = 0;
                return true;
            }
            int start_hull;
            compound_hull_vert* start_vert;
            if (compound_hulls_can_be_combined(cmp_hull0, other, start_hull, start_vert)) {
                compound_hull* output = allocate_compound_hull();
                if (output == nullptr) {
                    return false;
                }
                compound_hull* first = start_hull == 0 ? cmp_hull0 : other;
                compound_hull* second = start_hull == 0 ? other : cmp_hull0;
                if (!combine_compound_hull_2d_internal(output, first, second, start_vert)) {
                    ncd_list_merge(Compound_hull_verts, Free_compound_hull_verts, output->points);
                    output->points = NCD_NONE;
                    release_compound_hull(output, nullptr);
                    // Failed: this hull waits at the end of the list.
                    unsigned short index = ncd_hull_index(cmp_hull0);
                    ncd_list_remove(Compound_hulls, Active_compound_hulls, index);
                    ncd_list_add(Compound_hulls, Active_compound_hulls, index);
                }
                return true;
            }
        }
        if (other->next == Active_compound_hulls) {
            cmp_hull0->flags.combinations_considered = 1;
            return true;
        }
    }
}

static int Compound_hull_creations_per_nav_cell_association = 3; // 0x016B01D0
static int Compound_hull_combinations_per_frame = 5; // 0x016B01D4
static int Nav_cell_association_counter;
static bool Combination_attempted_tracker;

static void ncd_run_combinations()
{
    for (int i = 0; i < Compound_hull_combinations_per_frame; i++) {
        if (!process_compound_hull_combination()) {
            break;
        }
    }
    Combination_attempted_tracker = true;
}

// 0x00B13DA0 (:4415)
static void process_compound_hulls()
{
    convex_hull_2d* start_hull = check_for_unincorportated_high_priority_convex_hull();
    if (start_hull != nullptr) {
        bool consumed_cpu;
        compound_hull* cmp_hull = create_compound_hull_from_convex_hulls(consumed_cpu, start_hull);
        if (cmp_hull != nullptr) {
            add_nav_cell_references_to_compound_hull(cmp_hull);
            return;
        }
        start_hull->flags.high_priority = 0;
        if (consumed_cpu) {
            create_compound_hulls(Compound_hull_creations_per_nav_cell_association - 1);
            Nav_cell_association_counter = 0;
            Combination_attempted_tracker = false;
            return;
        }
    }
    bool create_tried = false;
    if (Compound_hull_creations_per_nav_cell_association <= Nav_cell_association_counter) {
        if (!Combination_attempted_tracker) {
            ncd_run_combinations();
            return;
        }
        Nav_cell_association_counter = 0;
        create_tried = true;
        if (create_compound_hulls(Compound_hull_creations_per_nav_cell_association)) {
            return;
        }
        Combination_attempted_tracker = false;
    }
    if (associate_compound_hulls_with_nav_cells(1)) {
        Nav_cell_association_counter++;
        return;
    }
    if (!create_tried) {
        if (!Combination_attempted_tracker) {
            ncd_run_combinations();
            return;
        }
        if (create_compound_hulls(Compound_hull_creations_per_nav_cell_association)) {
            return;
        }
        Combination_attempted_tracker = false;
    }
    ncd_run_combinations();
}

// 0x00B07F40 (:4510)
static void ncd_add_impassable_nav_cells_for_compound_hull(const compound_hull* cmp_hull, uint64_t start_navcell_handle, uint64_t stop_navcell_handle)
{
    if (Active_compound_hull_references == NCD_NONE) {
        return;
    }
    for (compound_hull_ref* ref = Compound_hull_references + Active_compound_hull_references;; ref = Compound_hull_references + ref->allocation_next) {
        if (ref->cmp_hull == cmp_hull && (ref->ncp->flags & NAV_CELL_FLAG_BLOCKED) == 0) {
            uint64_t handle = nav_cell_get_handle(ref->ncp);
            if (handle != start_navcell_handle && handle != stop_navcell_handle) {
                vector verts[3];
                nav_cell_get_all_verts(ref->ncp, verts[0], verts[1], verts[2]);
                // Original bug kept: the source loops over three vertices, but only the first is tested.
                bool impassable = nav_cell_detour_is_point_in_compound_hull(vector2(verts[0].x, verts[0].z), cmp_hull);
                if (!impassable) {
                    // A cell with two open edges running into the hull is cut off too.
                    int num_entering = 0;
                    for (int edge = 0; edge < 3 && num_entering <= 1; edge++) {
                        const nav_cell* adjacent = ref->ncp->adjacent_cells[edge].to_pointer();
                        if (adjacent != nullptr && (adjacent->flags & NAV_CELL_FLAG_DISABLED) == 0) {
                            nav_cell_los_test_point start_point;
                            start_point.pos = verts[edge];
                            start_point.nav_cell_handle = handle;
                            nav_cell_los_test_point end_point;
                            end_point.pos = verts[(edge + 1) % 3];
                            end_point.nav_cell_handle = handle;
                            if (does_linesegment_enter_compound_hull(start_point, end_point, cmp_hull)) {
                                num_entering++;
                            }
                        }
                    }
                    impassable = num_entering > 1;
                }
                if (impassable && !rfg_pf_add_impassable_nav_cell(ref->ncp)) {
                    return;
                }
            }
        }
        if (ref->allocation_next == Active_compound_hull_references) {
            return;
        }
    }
}

// 0x00AF9080 (:4579) LTCG: start_node EAX
static bool get_path_exit_point_from_compound_hull(vector2* exit_pos_2d, int* exit_edge_index, path_node** exit_node, const compound_hull* cmp_hull, path_node* start_node, path_node* path)
{
    *exit_node = nullptr;
    path_node* node = start_node;
    for (;;) {
        path_node* next = node->next;
        if (next == path || next == nullptr) {
            return false;
        }
        if (get_first_linesegment_exit_point_from_compound_hull(exit_pos_2d, exit_edge_index, node, next, cmp_hull)) {
            *exit_node = next;
            return true;
        }
        node = next;
    }
}

// 0x00B13EF0 (:4669) LTCG: start_pos and end_pos's handle part in EAX
static bool get_shortest_detour_path_los_check(uint64_t start_nav_cell_handle, const vector& start_pos, uint64_t end_nav_cell_handle, const vector& end_pos, unsigned int flags)
{
    return nav_cell_test_line_of_sight(start_nav_cell_handle, start_pos, end_nav_cell_handle, end_pos, NAV_CELL_LOS_IGNORE_DETOUR);
}

// 0x00B17D70 (:4731) LTCG: exit_pos_2d EDX
static detour_path_types get_shortest_detour_path(compound_hull* cmp_hull, int entry_edge_index, const vector2& entry_pos_2d, uint64_t entry_nav_cell_handle, const vector2& exit_pos_2d, int exit_edge_index, unsigned int flags, bool ignore_cells)
{
    const uint64_t NO_HANDLE = 0xFFFFFFFFFFFFFFFFull;
    compound_hull_vert* entry_vert = nullptr;
    compound_hull_vert* exit_vert = nullptr;
    if (cmp_hull->points != NCD_NONE) {
        int edge = 0;
        for (compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert), edge++) {
            if (edge == entry_edge_index) {
                entry_vert = vert;
            }
            if (edge == exit_edge_index) {
                exit_vert = vert;
            }
            if ((entry_vert != nullptr && exit_vert != nullptr) || vert->next == cmp_hull->points) {
                break;
            }
        }
    }
    compound_hull_vert* after_entry = ncd_next_vert(entry_vert);
    compound_hull_vert* after_exit = ncd_next_vert(exit_vert);
    float height = cmp_hull->bmin.y;
    vector start_pos(entry_pos_2d.x, height, entry_pos_2d.y);
    vector end_pos(exit_pos_2d.x, height, exit_pos_2d.y);
    vector cell_end;
    uint64_t cell_end_handle;

    if (entry_edge_index == exit_edge_index) {
        nav_cell* entry_cell = nav_cell_from_handle(entry_nav_cell_handle);
        if (bb_pf_get_nav_cell_from_hint(&cell_end, entry_cell, end_pos, false, &cell_end_handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) != nullptr
            && get_shortest_detour_path_los_check(entry_nav_cell_handle, start_pos, cell_end_handle, cell_end, flags)) {
            vector2 adjusted_exit(cell_end.x, cell_end.z);
            return vec2_dist_squared(entry_vert->pos, entry_pos_2d) < vec2_dist_squared(entry_vert->pos, adjusted_exit) ? DETOUR_PATH_CLOCKWISE : DETOUR_PATH_COUNTER_CLOCKWISE;
        }
    }

    // Clockwise: along next from the vertex after the entry edge to the exit edge's vertex.
    float clockwise_length = -1.0f;
    if (after_entry->nav_cell_handle != NO_HANDLE
        && bb_pf_get_nav_cell_from_hint(&cell_end, nav_cell_from_handle(exit_vert->nav_cell_handle), end_pos, false, &cell_end_handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) != nullptr) {
        vector point(after_entry->pos.x, height, after_entry->pos.y);
        if (get_shortest_detour_path_los_check(entry_nav_cell_handle, start_pos, after_entry->nav_cell_handle, point, flags)) {
            clockwise_length = vec_xz_dist(start_pos, point) + 0.0f;
            compound_hull_vert* vert = after_entry;
            vector vert_pos = point;
            while (vert != exit_vert) {
                compound_hull_vert* next = ncd_next_vert(vert);
                if (next->nav_cell_handle == NO_HANDLE) {
                    clockwise_length = -1.0f;
                    break;
                }
                nav_cell_los_test_point from;
                from.pos = vert_pos;
                from.nav_cell_handle = vert->nav_cell_handle;
                nav_cell_los_test_point to;
                to.pos = vector(next->pos.x, height, next->pos.y);
                to.nav_cell_handle = next->nav_cell_handle;
                if (!nav_cell_test_line_of_sight(from, to, NAV_CELL_LOS_IGNORE_DETOUR)) {
                    clockwise_length = -1.0f;
                    break;
                }
                clockwise_length += vec2_dist(vert->pos, next->pos);
                vert = next;
                vert_pos = to.pos;
            }
            if (0.0f <= clockwise_length && exit_vert->nav_cell_handle != NO_HANDLE) {
                vector exit_point(exit_vert->pos.x, height, exit_vert->pos.y);
                if (get_shortest_detour_path_los_check(exit_vert->nav_cell_handle, exit_point, cell_end_handle, cell_end, flags)) {
                    clockwise_length += vec_xz_dist(exit_point, cell_end);
                } else {
                    clockwise_length = -1.0f;
                }
            }
        }
    }

    // Counter clockwise: along prev from the entry edge's vertex to the vertex after the exit edge.
    float counter_length = -1.0f;
    if (entry_vert->nav_cell_handle != NO_HANDLE
        && bb_pf_get_nav_cell_from_hint(&cell_end, nav_cell_from_handle(after_exit->nav_cell_handle), end_pos, false, &cell_end_handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) != nullptr) {
        vector point(entry_vert->pos.x, height, entry_vert->pos.y);
        if (get_shortest_detour_path_los_check(entry_nav_cell_handle, start_pos, entry_vert->nav_cell_handle, point, flags)) {
            counter_length = vec_xz_dist(start_pos, point) + 0.0f;
            compound_hull_vert* vert = entry_vert;
            vector vert_pos = point;
            while (vert != after_exit) {
                compound_hull_vert* previous = Compound_hull_verts + vert->prev;
                if (previous->nav_cell_handle == NO_HANDLE) {
                    counter_length = -1.0f;
                    break;
                }
                nav_cell_los_test_point from;
                from.pos = vert_pos;
                from.nav_cell_handle = vert->nav_cell_handle;
                nav_cell_los_test_point to;
                to.pos = vector(previous->pos.x, height, previous->pos.y);
                to.nav_cell_handle = previous->nav_cell_handle;
                if (!nav_cell_test_line_of_sight(from, to, NAV_CELL_LOS_IGNORE_DETOUR)) {
                    counter_length = -1.0f;
                    break;
                }
                counter_length += vec2_dist(vert->pos, previous->pos);
                vert = previous;
                vert_pos = to.pos;
            }
            if (0.0f <= counter_length && after_exit->nav_cell_handle != NO_HANDLE) {
                vector exit_point(after_exit->pos.x, height, after_exit->pos.y);
                if (get_shortest_detour_path_los_check(after_exit->nav_cell_handle, exit_point, cell_end_handle, cell_end, flags)) {
                    counter_length += vec_xz_dist(exit_point, cell_end);
                } else {
                    counter_length = -1.0f;
                }
            }
        }
    }

    if (ignore_cells && clockwise_length < 0.0f && counter_length < 0.0f) {
        // Neither way is walkable: compare the plain lengths.
        if (bb_pf_get_nav_cell_from_hint(&cell_end, nav_cell_from_handle(exit_vert->nav_cell_handle), end_pos, false, &cell_end_handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) == nullptr) {
            clockwise_length = -1.0f;
        } else {
            clockwise_length = vec_xz_dist(start_pos, vector(after_entry->pos.x, height, after_entry->pos.y)) + 0.0f;
            for (compound_hull_vert* vert = after_entry; vert != exit_vert; vert = ncd_next_vert(vert)) {
                clockwise_length += vec2_dist(vert->pos, ncd_next_vert(vert)->pos);
            }
            if (0.0f <= clockwise_length) {
                clockwise_length += vec_xz_dist(vector(exit_vert->pos.x, height, exit_vert->pos.y), cell_end);
            }
        }
        if (bb_pf_get_nav_cell_from_hint(&cell_end, nav_cell_from_handle(after_exit->nav_cell_handle), end_pos, false, &cell_end_handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) == nullptr) {
            counter_length = -1.0f;
        } else {
            counter_length = vec_xz_dist(start_pos, vector(entry_vert->pos.x, height, entry_vert->pos.y)) + 0.0f;
            for (compound_hull_vert* vert = entry_vert; vert != after_exit; vert = Compound_hull_verts + vert->prev) {
                counter_length += vec2_dist(vert->pos, Compound_hull_verts[vert->prev].pos);
            }
            if (0.0f <= counter_length && after_exit->nav_cell_handle != NO_HANDLE) {
                counter_length += vec_xz_dist(vector(after_exit->pos.x, height, after_exit->pos.y), cell_end);
            }
        }
    }

    if (clockwise_length < 0.0f) {
        return 0.0f <= counter_length ? DETOUR_PATH_COUNTER_CLOCKWISE : DETOUR_PATH_NONE;
    }
    if (0.0f <= counter_length) {
        // Magic number: flag 2 asks for the longer way round.
        if ((flags & 2) == 0) {
            if (counter_length < clockwise_length) {
                return DETOUR_PATH_COUNTER_CLOCKWISE;
            }
        } else if (clockwise_length <= counter_length) {
            return DETOUR_PATH_COUNTER_CLOCKWISE;
        }
    }
    return DETOUR_PATH_CLOCKWISE;
}

// 0x00B0BC30 (:5015) LTCG: pos EAX, path ECX, cur_node EDI
static bool insert_compound_hull_point_in_path(path_node** cur_node, path_node** path, const vector2& pos, uint64_t nav_cell_handle, const compound_hull* cmp_hull, const compound_hull_vert* vert)
{
    if (*path == nullptr) {
        return false;
    }
    vector point(pos.x, cmp_hull->bmin.y, pos.y);
    nav_cell* cell = nav_cell_from_handle(nav_cell_handle);
    if (cell != nullptr) {
        point = nav_cell_force_point_to_cell(cell, point);
    }
    path_node* node = bb_pf_get_free_path_node(point, nav_cell_handle, 0xFFFFFFFF);
    if (node == nullptr) {
        return false;
    }
    node->compound_hull_handle = cmp_hull->handle;
    node->compound_hull_vert_index = nav_cell_detour_get_compound_hull_vert_index(cmp_hull, vert);
    node->next = (*cur_node)->next;
    node->prev = *cur_node;
    (*cur_node)->next->prev = node;
    (*cur_node)->next = node;
    *cur_node = node;
    return true;
}

// 0x00B0BD20 (:5103) LTCG: cur_node and path in registers, cmp_hull first on the stack
static bool insert_detour_path_nodes(path_node** cur_node, path_node** path, const compound_hull* cmp_hull, int entry_edge_index, const vector2& entry_pos, uint64_t entry_nav_cell_handle, int exit_edge_index, const vector2& exit_pos, uint64_t exit_nav_cell_handle, bool clockwise)
{
    const compound_hull_vert* entry_vert = nullptr;
    const compound_hull_vert* exit_vert = nullptr;
    int num_verts = 0;
    if (cmp_hull->points != NCD_NONE) {
        for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
            if (num_verts == entry_edge_index) {
                entry_vert = vert;
            }
            if (num_verts == exit_edge_index) {
                exit_vert = vert;
            }
            num_verts++;
            if (vert->next == cmp_hull->points) {
                break;
            }
        }
    }
    if (!insert_compound_hull_point_in_path(cur_node, path, entry_pos, entry_nav_cell_handle, cmp_hull, entry_vert)) {
        return false;
    }
    bool same_edge_skip = false;
    if (entry_edge_index == exit_edge_index) {
        float entry_dist = vec2_dist_squared(entry_vert->pos, entry_pos);
        float exit_dist = vec2_dist_squared(entry_vert->pos, exit_pos);
        same_edge_skip = clockwise ? entry_dist <= exit_dist : exit_dist <= entry_dist;
    }
    if (!same_edge_skip) {
        bool started = false;
        if (!clockwise) {
            int index = num_verts - 1;
            const compound_hull_vert* vert = cmp_hull->points == NCD_NONE ? nullptr : Compound_hull_verts + Compound_hull_verts[cmp_hull->points].prev;
            for (;;) {
                if (started) {
                    if (index == exit_edge_index) {
                        break;
                    }
                    if (!insert_compound_hull_point_in_path(cur_node, path, vert->pos, vert->nav_cell_handle, cmp_hull, vert)) {
                        return false;
                    }
                } else if (index == entry_edge_index) {
                    started = true;
                    if (!insert_compound_hull_point_in_path(cur_node, path, vert->pos, vert->nav_cell_handle, cmp_hull, vert)) {
                        return false;
                    }
                }
                index = (index + num_verts - 1) % num_verts;
                vert = Compound_hull_verts + vert->prev;
            }
        } else {
            int index = -1;
            const compound_hull_vert* vert = cmp_hull->points == NCD_NONE ? nullptr : Compound_hull_verts + cmp_hull->points;
            while (!started || index != exit_edge_index) {
                index = (index + 1) % num_verts;
                if (started) {
                    if (!insert_compound_hull_point_in_path(cur_node, path, vert->pos, vert->nav_cell_handle, cmp_hull, vert)) {
                        return false;
                    }
                } else if (index == entry_edge_index) {
                    started = true;
                }
                vert = ncd_next_vert(vert);
            }
        }
    }
    return insert_compound_hull_point_in_path(cur_node, path, exit_pos, exit_nav_cell_handle, cmp_hull, exit_vert);
}

// 0x00B187E0 (:5227) LTCG: entry_pos EAX
static bool replace_rest_of_path_with_detour_hull(path_node** cur_node, path_node** path, const vector& entry_pos, const nav_cell* entry_nav_cell, compound_hull* cmp_hull, int entry_edge_index, unsigned int flags)
{
    bool result = true;
    path_node* last = *path == nullptr ? nullptr : (*path)->prev;
    if (last == *cur_node) {
        return false;
    }
    // The path's end stays: it is taken out, everything after the current node freed, and it goes back at the end.
    if (*path == last && last->next == last) {
        last->next = nullptr;
        last->prev = nullptr;
        *path = nullptr;
    } else {
        if (*path == last) {
            *path = last->next;
        }
        if (last->prev != nullptr) {
            last->prev->next = last->next;
        }
        if (last->next != nullptr) {
            last->next->prev = last->prev;
        }
        last->next = nullptr;
        last->prev = nullptr;
    }
    path_node* node = (*cur_node)->next;
    if (node != *path) {
        while (node != nullptr) {
            path_node* next = node->next;
            path_node* head = *path;
            bb_pf_free_path_node(path, node);
            node = next != head ? next : nullptr;
        }
    }
    vector2 closest;
    int closest_edge;
    nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_edge, last->pos, cmp_hull);
    vector closest_pos(closest.x, cmp_hull->bmin.y, closest.y);
    uint64_t closest_handle;
    bool done = false;
    if (bb_pf_get_nav_cell(closest_pos, false, &closest_handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr) {
        vector2 entry_2d(entry_pos.x, entry_pos.z);
        uint64_t entry_handle = nav_cell_get_handle(entry_nav_cell);
        if (entry_edge_index == closest_edge) {
            const compound_hull_vert* vert = nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, entry_edge_index);
            insert_compound_hull_point_in_path(cur_node, path, entry_2d, entry_handle, cmp_hull, vert);
            insert_compound_hull_point_in_path(cur_node, path, closest, closest_handle, cmp_hull, vert);
            (*cur_node)->flags.do_not_remove = true;
            done = true;
        } else {
            detour_path_types direction = get_shortest_detour_path(cmp_hull, entry_edge_index, entry_2d, entry_handle, closest, closest_edge, flags, false);
            if ((direction == DETOUR_PATH_CLOCKWISE || direction == DETOUR_PATH_COUNTER_CLOCKWISE)
                && insert_detour_path_nodes(cur_node, path, cmp_hull, entry_edge_index, entry_2d, entry_handle, closest_edge, closest, closest_handle, direction == DETOUR_PATH_CLOCKWISE)) {
                (*cur_node)->flags.do_not_remove = true;
                done = true;
            }
        }
    }
    if (!done) {
        result = false;
    }
    if (*path == nullptr) {
        last->next = last;
        last->prev = last;
        *path = last;
    } else {
        last->prev = (*path)->prev;
        last->next = *path;
        (*path)->prev->next = last;
        (*path)->prev = last;
    }
    *cur_node = last;
    last->compound_hull_handle = cmp_hull->handle;
    return result;
}

static void ncd_mark_do_not_remove_after(path_node* node, path_node* path)
{
    path_node* next = node->next;
    if (next != path && next != nullptr) {
        next->flags.do_not_remove = true;
    }
}

// 0x00B18A30 (:5314)
static bool add_detour_points_for_path_starting_in_compound_hull(path_node** cur_node, path_node** path, compound_hull* cmp_hull, unsigned int flags)
{
    vector2 closest;
    int closest_edge;
    vector2 cur_2d((*cur_node)->pos.x, (*cur_node)->pos.z);
    nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_edge, cur_2d, cmp_hull);
    vector closest_pos(closest.x, cmp_hull->bmin.y, closest.y);
    uint64_t closest_handle;
    nav_cell* closest_cell = bb_pf_get_nav_cell(closest_pos, false, &closest_handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    const compound_hull_vert* edge_vert = nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, closest_edge);
    const compound_hull_vert* next_vert = ncd_next_vert(edge_vert);
    if (closest_cell == nullptr) {
        // The closest point is off the mesh: walk along the edge from both ends until a mesh border stops it.
        nav_cell* vert_cell = nav_cell_from_handle(edge_vert->nav_cell_handle);
        nav_cell* next_cell = nav_cell_from_handle(next_vert->nav_cell_handle);
        if (vert_cell == nullptr && next_cell == nullptr) {
            return false;
        }
        nav_cell_longest_los_input input0;
        nav_cell_longest_los_output output0;
        if (vert_cell != nullptr) {
            input0.start_pos = vector(edge_vert->pos.x, cmp_hull->bmin.y, edge_vert->pos.y);
            input0.flags.ignore_detour_hulls = 1;
            input0.delta_2d = vector2(next_vert->pos.x - edge_vert->pos.x, next_vert->pos.y - edge_vert->pos.y);
            input0.start_nav_cell = vert_cell;
            input0.flags.use_disabled_nav_cells = 0;
            nav_cell_find_longest_line_of_sight_point(output0, input0);
        }
        nav_cell_longest_los_input input1;
        nav_cell_longest_los_output output1;
        if (next_cell != nullptr) {
            input1.start_pos = vector(next_vert->pos.x, cmp_hull->bmin.y, next_vert->pos.y);
            input1.flags.ignore_detour_hulls = 1;
            input1.delta_2d = vector2(edge_vert->pos.x - next_vert->pos.x, edge_vert->pos.y - next_vert->pos.y);
            input1.start_nav_cell = next_cell;
            input1.flags.use_disabled_nav_cells = 0;
            nav_cell_find_longest_line_of_sight_point(output1, input1);
        }
        const nav_cell_longest_los_output* chosen = &output0;
        if (vert_cell == nullptr || (next_cell != nullptr && vec_dist_squared((*cur_node)->pos, output1.end_pos) <= vec_dist_squared((*cur_node)->pos, output0.end_pos))) {
            chosen = &output1;
        }
        closest.x = chosen->end_pos.x;
        closest.y = chosen->end_pos.z;
        closest_pos = chosen->end_pos;
        closest_cell = chosen->end_nav_cell;
        closest_handle = nav_cell_get_handle(closest_cell);
    }
    vector2 exit_2d;
    int exit_edge;
    path_node* exit_node;
    if (!get_path_exit_point_from_compound_hull(&exit_2d, &exit_edge, &exit_node, cmp_hull, *cur_node, *path)) {
        path_node* before = *cur_node;
        if (replace_rest_of_path_with_detour_hull(cur_node, path, closest_pos, closest_cell, cmp_hull, closest_edge, flags)) {
            ncd_mark_do_not_remove_after(before, *path);
            return true;
        }
        return false;
    }
    vector exit_pos(exit_2d.x, cmp_hull->bmin.y, exit_2d.y);
    const compound_hull_vert* exit_vert = nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, exit_edge);
    vector adjusted;
    uint64_t exit_handle;
    if (bb_pf_get_nav_cell_from_hint(&adjusted, nav_cell_from_handle(exit_vert->nav_cell_handle), exit_pos, false, &exit_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED) == nullptr) {
        return false;
    }
    if (closest_edge == exit_edge) {
        path_node* node = (*cur_node)->next == *path ? nullptr : (*cur_node)->next;
        while (node != exit_node) {
            bb_pf_free_path_node(path, node);
            node = (*cur_node)->next == *path ? nullptr : (*cur_node)->next;
        }
        insert_compound_hull_point_in_path(cur_node, path, closest, closest_handle, cmp_hull, exit_vert);
        (*cur_node)->flags.do_not_remove = true;
        insert_compound_hull_point_in_path(cur_node, path, exit_2d, exit_handle, cmp_hull, nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, exit_edge));
        return true;
    }
    detour_path_types direction = get_shortest_detour_path(cmp_hull, closest_edge, closest, closest_handle, exit_2d, exit_edge, flags, false);
    if (direction != DETOUR_PATH_CLOCKWISE && direction != DETOUR_PATH_COUNTER_CLOCKWISE) {
        return false;
    }
    path_node* node = (*cur_node)->next == *path ? nullptr : (*cur_node)->next;
    while (node != exit_node) {
        path_node* next = node->next != *path ? node->next : nullptr;
        bb_pf_free_path_node(path, node);
        node = next;
    }
    path_node* before = *cur_node;
    if (!insert_detour_path_nodes(cur_node, path, cmp_hull, closest_edge, closest, closest_handle, exit_edge, exit_2d, exit_handle, direction == DETOUR_PATH_CLOCKWISE)) {
        return false;
    }
    ncd_mark_do_not_remove_after(before, *path);
    return true;
}

// 0x00AF9510 (:5489) LTCG: path EAX
static bool can_skip_adding_detour_points_for_simple_path(const path_node* const* path, const compound_hull* cmp_hull)
{
    const path_node* head = *path;
    const path_node* last = head == nullptr ? nullptr : head->prev;
    const path_node* second = head->next != head ? head->next : nullptr;
    if (second != last) {
        return false;
    }
    vector2 end_2d(last->pos.x, last->pos.z);
    if (!nav_cell_detour_is_point_in_compound_hull(end_2d, cmp_hull)) {
        return false;
    }
    vector2 start_2d(head->pos.x, head->pos.z);
    vector2 closest;
    int closest_edge;
    nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_edge, end_2d, cmp_hull);
    vector2 on_segment;
    vec2_find_closest_point_on_linesegment(on_segment, start_2d, closest, end_2d);
    // Magic number: half a metre.
    return vec2_dist_squared(start_2d, on_segment) <= 0.25f;
}

// 0x00AF9650 (:5546)
void nav_cell_detour_activation_listener::entityDeactivatedCallback(hkpEntity* entity)
{
    nav_cell_detour_mover_deactivate(nav_cell_detour_mover_from_entity(entity));
}

// 0x00AF96A0 (:5551)
void nav_cell_detour_activation_listener::entityActivatedCallback(hkpEntity* entity)
{
    nav_cell_detour_mover_activate(nav_cell_detour_mover_from_entity(entity));
}

// 0x00AF95F0
nav_cell_detour_activation_listener::nav_cell_detour_activation_listener()
{
}

// 0x00AF13F0
nav_cell_detour_activation_listener::~nav_cell_detour_activation_listener()
{
}

// 0x00AF1410 (:5565)
void nav_cell_detour_level_init_pre()
{
    Nav_cell_detour_veh_info_mar_raider_1 = vehicle_get_info_from_name("mar_raider_1");
    Nav_cell_detour_veh_info_walker_flamer = vehicle_get_info_from_name("walker_flamer");
    Nav_cell_detour_veh_info_walker_min_1 = vehicle_get_info_from_name("walker_min");
    Nav_cell_detour_veh_info_walker_min_2 = vehicle_get_info_from_name("walker_min2");
    Nav_cell_detour_veh_info_walker_min_3 = vehicle_get_info_from_name("walker_min3");
    Free_convex_hulls = NCD_NONE;
    Free_compound_hulls = NCD_NONE;
    Active_compound_hull_references = NCD_NONE;
    Active_detour_navpoint_block_refs = NCD_NONE;
    Unincorporated_convex_hulls = NCD_NONE;
    Active_compound_hulls = NCD_NONE;
    Active_nav_cell_detour_requests = NCD_NONE;
    Free_compound_hull_verts = NCD_NONE;
    Next_compound_hull_identifier = 0;
    Free_compound_hull_references = NCD_NONE;
    Free_nav_cell_detour_requests = NCD_NONE;
    Next_nav_cell_request_identifier = 0;
    Free_detour_navpoint_block_refs = NCD_NONE;
    for (int i = 0; i < NCD_MAX_NAVPOINT_BLOCK_REFS; i++) {
        ncd_list_add(Detour_navpoint_block_refs, Free_detour_navpoint_block_refs, (unsigned short)i);
        Detour_navpoint_block_refs[i].navp = nullptr;
        Detour_navpoint_block_refs[i].hull_ref = NCD_NONE;
    }
    Active_detour_action_node_block_refs = NCD_NONE;
    Free_detour_action_node_block_refs = NCD_NONE;
    for (int i = 0; i < NCD_MAX_NODE_BLOCK_REFS; i++) {
        ncd_list_add(Detour_action_node_block_refs, Free_detour_action_node_block_refs, (unsigned short)i);
        Detour_action_node_block_refs[i].node = nullptr;
        Detour_action_node_block_refs[i].hull_ref = NCD_NONE;
    }
    Active_detour_guard_node_block_refs = NCD_NONE;
    Free_detour_guard_node_block_refs = NCD_NONE;
    for (int i = 0; i < NCD_MAX_NODE_BLOCK_REFS; i++) {
        ncd_list_add(Detour_guard_node_block_refs, Free_detour_guard_node_block_refs, (unsigned short)i);
        Detour_guard_node_block_refs[i].node = nullptr;
        Detour_guard_node_block_refs[i].hull_ref = NCD_NONE;
    }
    for (int i = 0; i < NCD_MAX_CONVEX_HULLS; i++) {
        ncd_list_add(Convex_hulls, Free_convex_hulls, (unsigned short)i);
    }
    for (int i = 0; i < NCD_MAX_COMPOUND_HULL_VERTS; i++) {
        ncd_list_add(Compound_hull_verts, Free_compound_hull_verts, (unsigned short)i);
    }
    for (int i = 0; i < NCD_MAX_COMPOUND_HULLS; i++) {
        ncd_list_add(Compound_hulls, Free_compound_hulls, (unsigned short)i);
    }
    for (int i = 0; i < NCD_MAX_COMPOUND_HULL_REFERENCES; i++) {
        ncd_ref_alloc_add(Free_compound_hull_references, (unsigned short)i);
    }
    for (int i = 0; i < NCD_MAX_REQUESTS; i++) {
        ncd_list_add(Nav_cell_detour_requests, Free_nav_cell_detour_requests, (unsigned short)i);
    }
}

// 0x00B1E940 (:5684)
void nav_cell_detour_do_frame()
{
    if (!Multiplayer) {
        process_convex_hulls_for_requests();
        process_compound_hulls();
    }
}

// 0x00AF2920 (:5733)
void nav_cell_detour_load_zone(const obj_zone* zone)
{
    if (Active_compound_hulls == NCD_NONE) {
        return;
    }
    for (compound_hull* cmp_hull = Compound_hulls + Active_compound_hulls;; cmp_hull = Compound_hulls + cmp_hull->next) {
        vector bmin, bmax;
        zone->get_bbox(bmin, bmax);
        if (bmin.x < cmp_hull->bmax.x && bmin.y < cmp_hull->bmax.y && bmin.z < cmp_hull->bmax.z
            && cmp_hull->bmin.x < bmax.x && cmp_hull->bmin.y < bmax.y && cmp_hull->bmin.z < bmax.z) {
            cmp_hull->flags.nav_cells_computed = 0;
        }
        if (cmp_hull->next == Active_compound_hulls) {
            return;
        }
    }
}

// 0x00B081C0 (:5751)
void nav_cell_detour_unload_zone(const obj_zone* zone)
{
    compound_hull_ref* ref = Active_compound_hull_references == NCD_NONE ? nullptr : Compound_hull_references + Active_compound_hull_references;
    while (ref != nullptr) {
        compound_hull_ref* current = ref;
        ref = current->allocation_next == Active_compound_hull_references ? nullptr : Compound_hull_references + current->allocation_next;
        if (nav_cell_does_zone_contain_nav_cell(current->ncp, zone, nullptr) != NAV_CELL_NOT_IN_ZONE) {
            release_compound_hull_reference(current);
        }
    }
    unsigned short active = Active_compound_hulls;
    compound_hull* hull = active == NCD_NONE ? nullptr : Compound_hulls + active;
    while (hull != nullptr) {
        compound_hull* current = hull;
        hull = current->next == active ? nullptr : Compound_hulls + current->next;
        if (current->reference_count == 0) {
            release_compound_hull(current, nullptr);
            active = Active_compound_hulls;
        }
    }
}

// 0x00AF2A20 (:5792)
unsigned int nav_cell_detour_add_request(unsigned int obj_handle, short component_index)
{
    if (Multiplayer) {
        return 0xFFFFFFFF;
    }
    object* objp = World.object_from_handle(obj_handle);
    if (objp == nullptr) {
        return 0xFFFFFFFF;
    }
    switch (objp->get_obj_type()) {
    case OT_HUMAN:
    case OT_ITEM:
    case OT_MOVER:
    case OT_TURRET:
        // Only a vehicle's request can name a component.
        if (component_index != -1) {
            return 0xFFFFFFFF;
        }
        break;
    case OT_VEHICLE:
        break;
    default:
        return 0xFFFFFFFF;
    }
    unsigned short index = Free_nav_cell_detour_requests;
    if (index == NCD_NONE) {
        return 0xFFFFFFFF;
    }
    nav_cell_detour_request& request = Nav_cell_detour_requests[index];
    request.component_index = component_index;
    request.object_handle = obj_handle;
    request.convex_hull_idx = NCD_NONE;
    request.request_handle = (Next_nav_cell_request_identifier << 12) | index;
    Next_nav_cell_request_identifier++;
    // Magic number: the identifier wraps within the 20 bits above the index.
    if (Next_nav_cell_request_identifier > 0xFFFFE) {
        Next_nav_cell_request_identifier = 0;
    }
    request.disabled_flags = 0;
    if (ncd_request_disabled_position(objp)) {
        request.disabled_flags |= 0x20;
    }
    ncd_list_remove(Nav_cell_detour_requests, Free_nav_cell_detour_requests, index);
    ncd_list_add(Nav_cell_detour_requests, Active_nav_cell_detour_requests, index);
    return request.request_handle;
}

// 0x00AF2BB0 (:5873)
void nav_cell_detour_set_dont_update(unsigned int request_handle)
{
    // Empty in rfg.exe.
}

// 0x00AF2BC0 (:5884)
void nav_cell_detour_clear_dont_update(unsigned int request_handle)
{
    // Empty in rfg.exe.
}

// 0x00B190F0 (:5899)
void nav_cell_detour_remove_request(unsigned int request_handle)
{
    if (request_handle == 0xFFFFFFFF) {
        return;
    }
    unsigned short index = (unsigned short)(request_handle & 0xFFF);
    nav_cell_detour_request& request = Nav_cell_detour_requests[index];
    if (request.request_handle != request_handle) {
        return;
    }
    if (request.convex_hull_idx != NCD_NONE) {
        destroy_convex_hull_for_request(&request, false);
    }
    ncd_list_remove(Nav_cell_detour_requests, Active_nav_cell_detour_requests, index);
    ncd_list_add(Nav_cell_detour_requests, Free_nav_cell_detour_requests, index);
}

// 0x00AF2BD0 (:5927)
bool nav_cell_detour_has_valid_convex_hull(unsigned int request_handle)
{
    if (request_handle != 0xFFFFFFFF && Nav_cell_detour_requests[request_handle & 0xFFF].request_handle == request_handle) {
        return Nav_cell_detour_requests[request_handle & 0xFFF].convex_hull_idx != NCD_NONE;
    }
    return false;
}

// 0x00B191E0 (:5942)
void nav_cell_detour_remove_convex_hull_for_request(unsigned int request_handle)
{
    if (request_handle != 0xFFFFFFFF && Nav_cell_detour_requests[request_handle & 0xFFF].request_handle == request_handle) {
        destroy_convex_hull_for_request(Nav_cell_detour_requests + (request_handle & 0xFFF), true);
    }
}

// 0x00AF2C00 (:5962)
const compound_hull* nav_cell_detour_get_compound_hull(unsigned int handle)
{
    if (handle == 0xFFFFFFFF) {
        return nullptr;
    }
    const compound_hull* cmp_hull = Compound_hulls + (handle & 0xFFFF);
    return cmp_hull->handle == handle ? cmp_hull : nullptr;
}

// 0x00AF2C30 (:5988)
const compound_hull_vert* nav_cell_detour_get_compound_hull_vert_from_index(const compound_hull* cmp_hull, int index)
{
    if (index > -1 && cmp_hull->points != NCD_NONE) {
        int i = 0;
        for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
            if (i == index) {
                return vert;
            }
            i++;
            if (vert->next == cmp_hull->points) {
                break;
            }
        }
    }
    return nullptr;
}

// 0x00AF2C80 (:6013)
int nav_cell_detour_get_compound_hull_vert_index(const compound_hull* cmp_hull, const compound_hull_vert* vert)
{
    if (cmp_hull->points != NCD_NONE) {
        int i = 0;
        for (const compound_hull_vert* walk = Compound_hull_verts + cmp_hull->points;; walk = ncd_next_vert(walk)) {
            if (walk == vert) {
                return i;
            }
            i++;
            if (walk->next == cmp_hull->points) {
                break;
            }
        }
    }
    return -1;
}

// 0x00AF2CD0 (:6035)
int nav_cell_detour_get_num_compound_hull_verts(const compound_hull* cmp_hull)
{
    int count = 0;
    if (cmp_hull->points != NCD_NONE) {
        unsigned short index = cmp_hull->points;
        do {
            index = Compound_hull_verts[index].next;
            count++;
        } while (index != cmp_hull->points);
    }
    return count;
}

// 0x00AF2D10 (:6055)
bool nav_cell_detour_is_point_in_compound_hull(const vector2& pt, const compound_hull* cmp_hull)
{
    return ncd_point_in_outline(pt, cmp_hull->points);
}

// 0x00AF96B0 (:6091)
bool nav_cell_detour_is_line_in_compound_hull(const vector& start_pt, const vector& end_pt, const compound_hull* cmp_hull)
{
    if (nav_cell_detour_is_point_in_compound_hull(vector2(start_pt.x, start_pt.z), cmp_hull)) {
        return true;
    }
    nav_cell_los_test_point start_point;
    start_point.pos = start_pt;
    nav_cell_los_test_point end_point;
    end_point.pos = end_pt;
    return does_linesegment_enter_compound_hull(start_point, end_point, cmp_hull);
}

// 0x00AF2E30 (:6112)
bool nav_cell_detour_is_point_in_compound_hull(const vector& pt, const compound_hull* cmp_hull)
{
    return nav_cell_detour_is_point_in_compound_hull(vector2(pt.x, pt.z), cmp_hull);
}

// 0x00AF2E60 (:6125)
compound_hull* nav_cell_detour_is_point_in_compound_hull(const vector& pt, const nav_cell* ncp)
{
    unsigned short first = ncp->compound_hull_reference;
    if (first != NCD_NONE) {
        vector2 pt_2d(pt.x, pt.z);
        for (const compound_hull_ref* ref = Compound_hull_references + first;; ref = Compound_hull_references + ref->references_next) {
            if (nav_cell_detour_is_point_in_compound_hull(pt_2d, ref->cmp_hull)) {
                return ref->cmp_hull;
            }
            if (ref->references_next == first) {
                break;
            }
        }
    }
    return nullptr;
}

// 0x00AF2EE0 (:6146)
void nav_cell_detour_get_closest_point_on_compound_hull(vector2* closest_pt, int* closest_edge_index, const vector2& pos, const compound_hull* cmp_hull)
{
    if (cmp_hull->points == NCD_NONE) {
        return;
    }
    int edge = 0;
    float best_dist_sq = -1.0f;
    for (const compound_hull_vert* vert = Compound_hull_verts + cmp_hull->points;; vert = ncd_next_vert(vert)) {
        vector2 closest;
        vec2_find_closest_point_on_linesegment(closest, pos, vert->pos, ncd_next_vert(vert)->pos);
        float dist_sq = vec2_dist_squared(pos, closest);
        if (dist_sq < best_dist_sq || best_dist_sq < 0.0f) {
            *closest_pt = closest;
            *closest_edge_index = edge;
            best_dist_sq = dist_sq;
        }
        edge++;
        if (vert->next == cmp_hull->points) {
            return;
        }
    }
}

// 0x00AF2FC0 (:6177)
void nav_cell_detour_get_closest_point_on_compound_hull(vector2* closest_pt, int* closest_edge_index, const vector& pos, const compound_hull* cmp_hull)
{
    nav_cell_detour_get_closest_point_on_compound_hull(closest_pt, closest_edge_index, vector2(pos.x, pos.z), cmp_hull);
}

// 0x00B0BF80 (:6196)
bool nav_cell_detour_get_closest_compound_hull_intersection(vector* ix_pos, compound_hull** closest_cmp_hull, int* entry_edge_index, nav_cell* ncp, const vector& start_pos, const vector& end_pos)
{
    vector2 start_2d(start_pos.x, start_pos.z);
    vector2 end_2d(end_pos.x, end_pos.z);
    float best_dist_sq = -1.0f;
    vector2 best;
    unsigned short first = ncp->compound_hull_reference;
    if (first == NCD_NONE) {
        return false;
    }
    for (const compound_hull_ref* ref = Compound_hull_references + first;; ref = Compound_hull_references + ref->references_next) {
        vector2 entry;
        int edge;
        if (get_first_linesegment_entry_point_into_compound_hull(&entry, &edge, start_2d, end_2d, ref->cmp_hull) && nav_cell_is_point_in_cell_column(ncp, entry)) {
            float dist_sq = vec2_dist_squared(entry, start_2d);
            if (0.0f < dist_sq && (dist_sq < best_dist_sq || best_dist_sq < 0.0f)) {
                best = entry;
                *closest_cmp_hull = ref->cmp_hull;
                *entry_edge_index = edge;
                best_dist_sq = dist_sq;
            }
        }
        if (ref->references_next == first) {
            break;
        }
    }
    if (best_dist_sq < 0.0f) {
        return false;
    }
    ix_pos->x = best.x;
    ix_pos->z = best.y;
    nav_cell_force_point_to_cell_plane(*ix_pos, ncp);
    return true;
}

// 0x00B1E960 (:6253)
bool nav_cell_detour_add_detour_points_to_path(human* hp, unsigned int flags, float check_dist)
{
    path_node** path = &hp->pf.path;
    if (*path == nullptr) {
        return false;
    }
    // Forget hull references that have gone; the walk starts at the first node on no hull.
    path_node* cur = nullptr;
    path_node* node = *path;
    do {
        unsigned int hull_handle = node->compound_hull_handle;
        if (hull_handle != 0xFFFFFFFF && Compound_hulls[hull_handle & 0xFFFF].handle != hull_handle) {
            node->compound_hull_handle = 0xFFFFFFFF;
        }
        if (cur == nullptr && node->compound_hull_handle == 0xFFFFFFFF) {
            cur = node;
        }
        node = node->next;
    } while (node != *path && node != nullptr);
    if (cur == nullptr) {
        return false;
    }

    float dist = 0.0f;
    if (cur == *path) {
        if (cur->next == *path || cur->next == nullptr) {
            return false;
        }
        path_node* second = cur->next;
        compound_hull* start_hull = nav_cell_detour_is_point_in_compound_hull(cur->pos, nav_cell_from_handle(cur->nav_cell_handle));
        if (start_hull != nullptr && cur->compound_hull_handle != start_hull->handle) {
            if (second->compound_hull_handle == start_hull->handle) {
                // The path already goes along this hull: go on from where it does.
                vector* prev_pos = &cur->pos;
                do {
                    cur = cur->next;
                    if (cur == *path || cur == nullptr) {
                        return false;
                    }
                    dist += vec_dist(*prev_pos, cur->pos);
                    if (check_dist <= dist) {
                        return false;
                    }
                    prev_pos = &cur->pos;
                } while (cur->compound_hull_handle != start_hull->handle);
            } else {
                dist = vec_dist(cur->pos, second->pos) + 0.0f;
                if ((flags & 1) == 0 || !can_skip_adding_detour_points_for_simple_path(path, start_hull)) {
                    path_node* walker = cur;
                    if (add_detour_points_for_path_starting_in_compound_hull(&walker, path, start_hull, flags)) {
                        return false;
                    }
                    // No way out: every node from the start that is inside gets the hull, and the path counts as failed.
                    cur = nullptr;
                    if (*path != nullptr) {
                        path_node* inside = *path;
                        while (nav_cell_detour_is_point_in_compound_hull(vector2(inside->pos.x, inside->pos.z), start_hull)) {
                            inside->compound_hull_handle = start_hull->handle;
                            cur = inside;
                            inside = inside->next;
                            if (inside == *path || inside == nullptr) {
                                break;
                            }
                        }
                    }
                    hp->hflags.last_pf_failed = 1;
                }
            }
        }
    } else {
        path_node* walk = *path;
        for (;;) {
            path_node* next = walk->next != *path ? walk->next : nullptr;
            if (walk == cur || next == nullptr) {
                break;
            }
            dist += vec_dist(walk->pos, next->pos);
            walk = next;
        }
    }
    if ((double)check_dist <= (double)dist) {
        return false;
    }

    for (;;) {
        path_node* next = cur->next;
        if (next == hp->pf.path || next == nullptr) {
            return false;
        }
        if (cur->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull || cur->compound_hull_handle != 0xFFFFFFFF) {
            return false;
        }
        dist += vec_dist(cur->pos, next->pos);
        nav_cell_longest_los_input input;
        nav_cell_longest_los_output output;
        input.start_pos = cur->pos;
        input.start_nav_cell = nav_cell_from_handle(cur->nav_cell_handle);
        input.delta_2d = vector2(next->pos.x - cur->pos.x, next->pos.z - cur->pos.z);
        input.flags.use_disabled_nav_cells = 0;
        nav_cell_find_longest_line_of_sight_point(output, input);
        path_node* following = next;
        if (output.result == NAV_CELL_LONGEST_LOS_RESULT_EXITED_NAVMESH) {
            return false;
        }
        if (output.result == NAV_CELL_LONGEST_LOS_RESULT_CONTACTED_DETOUR_HULL && output.obstruction.detour.cmp_hull->handle != next->compound_hull_handle) {
            compound_hull* hull = output.obstruction.detour.cmp_hull;
            int entry_edge = output.obstruction.detour.entry_edge_index;
            vector2 exit_2d;
            int exit_edge;
            path_node* exit_node;
            if (!get_path_exit_point_from_compound_hull(&exit_2d, &exit_edge, &exit_node, hull, cur, *path)) {
                path_node* walker = cur;
                replace_rest_of_path_with_detour_hull(&walker, path, output.end_pos, output.end_nav_cell, hull, entry_edge, flags);
                return false;
            }
            path_node* remove = cur->next == *path ? nullptr : cur->next;
            while (remove != exit_node) {
                bb_pf_free_path_node(path, remove);
                remove = cur->next == *path ? nullptr : cur->next;
            }
            vector2 entry_2d(output.end_pos.x, output.end_pos.z);
            uint64_t entry_handle = nav_cell_get_handle(output.end_nav_cell);
            detour_path_types direction = get_shortest_detour_path(hull, entry_edge, entry_2d, entry_handle, exit_2d, exit_edge, flags, false);
            if (direction == DETOUR_PATH_NONE) {
                // No way round: remember the hull, and search again with its cells impassable once per hull.
                bool already_problem = false;
                for (int i = 0; i < hp->pf.num_problem_detours; i++) {
                    if (hp->pf.problem_detour_handles[i] == hull->handle) {
                        already_problem = true;
                        break;
                    }
                    if (nav_cell_detour_get_compound_hull(hp->pf.problem_detour_handles[i]) == nullptr) {
                        hp->pf.problem_detour_handles[i] = hp->pf.problem_detour_handles[hp->pf.num_problem_detours - 1];
                        hp->pf.num_problem_detours--;
                        i--;
                    }
                }
                // Magic number: the 4 entries of problem_detour_handles.
                if (!already_problem && hp->pf.num_problem_detours < 4) {
                    hp->pf.problem_detour_handles[hp->pf.num_problem_detours] = hull->handle;
                    hp->pf.num_problem_detours++;
                    path_node* head = hp->pf.path;
                    path_node* tail = head == nullptr ? nullptr : head->prev;
                    hp->pf.problem_detours_dest_pos = tail->pos;
                    nav_cell_detour_add_impassable_nav_cells(hp, head->nav_cell_handle, tail->nav_cell_handle);
                    path_node* new_path = rfg_pf_pathfind(hp, head->pos, tail->pos, head->nav_cell_handle, tail->nav_cell_handle, false);
                    rfg_pf_clear_impassable_nav_cells();
                    if (new_path != nullptr) {
                        bb_pf_free_path(&hp->pf.path);
                        human_move_path_set_direct(hp, new_path);
                        return true;
                    }
                }
                // The path ends here.
                path_node* head = hp->pf.path;
                path_node* cut = cur->next;
                if (cut != head && cut != nullptr) {
                    if (head == nullptr) {
                        cut = nullptr;
                    } else if (head == cut) {
                        hp->pf.path = nullptr;
                        cut = head;
                    } else {
                        path_node* tail = head->prev;
                        cut->prev->next = head;
                        hp->pf.path->prev = cut->prev;
                        cut->prev = tail;
                        tail->next = cut;
                    }
                    bb_pf_free_path(&cut);
                }
                hp->hflags.last_pf_failed = 1;
                return false;
            }
            if (direction != DETOUR_PATH_CLOCKWISE && direction != DETOUR_PATH_COUNTER_CLOCKWISE) {
                return false;
            }
            vector exit_pos(exit_2d.x, hull->bmin.y, exit_2d.y);
            const compound_hull_vert* exit_vert = nav_cell_detour_get_compound_hull_vert_from_index(hull, exit_edge);
            vector adjusted;
            uint64_t exit_handle;
            if (bb_pf_get_nav_cell_from_hint(&adjusted, exit_vert->nav_cell_handle, exit_pos, false, &exit_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED) == nullptr) {
                return false;
            }
            path_node* walker = cur;
            if (!insert_detour_path_nodes(&walker, path, hull, entry_edge, entry_2d, entry_handle, exit_edge, exit_2d, exit_handle, direction == DETOUR_PATH_CLOCKWISE)) {
                return false;
            }
            following = walker;
        }
        cur = following;
        if ((double)check_dist <= (double)dist) {
            return false;
        }
    }
}

// 0x00B0C230 (:6637)
bool nav_cell_detour_test_line_of_sight(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point)
{
    nav_cell* cell = nav_cell_from_handle(start_point.nav_cell_handle);
    unsigned short first = cell->compound_hull_reference;
    if (first != NCD_NONE) {
        unsigned short index = first;
        do {
            if (does_linesegment_enter_compound_hull(start_point, end_point, Compound_hull_references[index].cmp_hull)) {
                return false;
            }
            index = Compound_hull_references[index].references_next;
        } while (index != first);
    }
    if (nav_cell_is_point_in_cell_column(cell, end_point.pos)) {
        return true;
    }
    for (int edge = 0; edge < 3; edge++) {
        nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
        vector ix;
        if (adjacent != nullptr && nav_cell_classify_segment_vs_edge(cell, edge, start_point.pos, end_point.pos, &ix) == LINE_CLASS_SEGMENTS_INTERSECT) {
            nav_cell_los_test_point crossing;
            crossing.pos = nav_cell_force_point_to_cell(adjacent, ix);
            crossing.nav_cell_handle = nav_cell_get_handle(adjacent);
            unsigned short first_adjacent = adjacent->compound_hull_reference;
            if (first_adjacent != NCD_NONE) {
                unsigned short index = first_adjacent;
                do {
                    if (does_linesegment_enter_compound_hull(crossing, end_point, Compound_hull_references[index].cmp_hull)) {
                        return false;
                    }
                    index = Compound_hull_references[index].references_next;
                } while (index != first_adjacent);
            }
            return true;
        }
    }
    return true;
}

// 0x00AF3000 (:6692) LTCG: om ESI
static bool nav_cell_detour_mover_ignore(object_mover* om)
{
    signed char sub_type = (signed char)om->get_sub_type();
    if (sub_type != 0 && (sub_type != 1 || rfg_mover_is_world_anchor((rfg_mover*)om))) {
        return true;
    }
    return !rfg_pf_should_avoid_mover(om);
}

// 0x00AF9760 (:6725)
void nav_cell_detour_mover_created(object_mover* om)
{
    if (!nav_cell_detour_mover_ignore(om) && om->obj_flags.destroyed == 0 && om->nav_cell_detour_request_handle == 0xFFFFFFFF) {
        om->nav_cell_detour_request_handle = nav_cell_detour_add_request(om->get_handle(), -1);
    }
}

// 0x00B19210 (:6735)
void nav_cell_detour_mover_destroyed(object_mover* om)
{
    if (!nav_cell_detour_mover_ignore(om)) {
        nav_cell_detour_remove_request(om->nav_cell_detour_request_handle);
        om->nav_cell_detour_request_handle = 0xFFFFFFFF;
    }
}

// 0x00B1F090 (:6747)
void nav_cell_detour_check_hull_against_current_path(human* hp, object* repulsor, unsigned int request_handle)
{
    path_node* first = hp->pf.path;
    if (hp->pf.flags.smoothing != 0 || first == nullptr || hp->hflags.on_mover != 0 || request_handle == 0xFFFFFFFF) {
        return;
    }
    nav_cell_detour_request* request = get_detour_request(request_handle);
    if (request == nullptr || request->convex_hull_idx == NCD_NONE) {
        return;
    }
    compound_hull* cmp_hull = Convex_hulls[request->convex_hull_idx].cmp_hull;
    if (cmp_hull == nullptr || cmp_hull->handle == hp->pf.last_cmp_hull) {
        return;
    }
    path_node* node = first;
    do {
        if (node->compound_hull_handle == cmp_hull->handle) {
            return;
        }
        node = node->next;
    } while (node != first && node != nullptr);

    nav_cell_los_test_point start_point;
    nav_cell_los_test_point end_point;
    start_point.pos = hp->pos;
    start_point.nav_cell_handle = hp->pf.last_visited_nav_cell_handle;
    end_point.pos = first->pos;
    end_point.nav_cell_handle = first->nav_cell_handle;
    vector2 pos_2d(hp->pos.x, hp->pos.z);
    if (nav_cell_detour_is_point_in_compound_hull(pos_2d, cmp_hull)) {
        vector2 closest;
        int closest_edge;
        nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_edge, start_point.pos, cmp_hull);
        start_point.cmp_hull = cmp_hull;
        start_point.vert = nav_cell_detour_get_compound_hull_vert_from_index(cmp_hull, closest_edge);
    }
    if (!does_linesegment_enter_compound_hull(start_point, end_point, cmp_hull)) {
        return;
    }
    vector adjusted;
    uint64_t handle;
    if (bb_pf_get_nav_cell_from_hint(&adjusted, hp->pf.last_visited_nav_cell_handle, hp->pos, false, &handle, false, true, NAV_CELL_TRAVERSAL_ENABLED) == nullptr) {
        return;
    }
    hp->pf.last_visited_nav_cell_handle = handle;
    path_node* start_node = bb_pf_get_free_path_node(adjusted, handle, 0xFFFFFFFF);
    if (start_node == nullptr) {
        return;
    }
    path_node** path = &hp->pf.path;
    if (*path == nullptr) {
        start_node->next = start_node;
        start_node->prev = start_node;
    } else {
        (*path)->prev->next = start_node;
        start_node->prev = (*path)->prev;
        (*path)->prev = start_node;
        start_node->next = *path;
    }
    *path = start_node;
    // Magic number: FLT_MAX: no limit on how far ahead to look.
    nav_cell_detour_add_detour_points_to_path(hp, hp->hflags.stuck != 0 ? 2 : 0, 3.4028235e+38f);
    bb_pf_free_path_node(path, *path);
}

// 0x00AF3030 (:6855)
bool nav_cell_detour_object_is_flat(object* obj)
{
    return ncd_object_is_flat(obj);
}

// 0x00AF3080 (:6860)
unsigned int nav_cell_detour_get_handle_from_object(const object* objp)
{
    if (objp != nullptr) {
        switch (objp->get_obj_type()) {
        case OT_HUMAN:
            return ((const human*)objp)->nav_cell_detour_request_handle;
        case OT_MOVER:
            return ((const object_mover*)objp)->nav_cell_detour_request_handle;
        case OT_VEHICLE:
            return ((const vehicle*)objp)->m_nav_cell_detour_request_handle;
        case OT_TURRET:
            return ((const turret*)objp)->get_detour_request_handle();
        default:
            break;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00B082C0 (:6882)
void nav_cell_detour_add_impassable_nav_cells(human* hp, uint64_t ignore_handle_one, uint64_t ignore_handle_two)
{
    for (int i = 0; i < hp->pf.num_problem_detours; i++) {
        unsigned int handle = hp->pf.problem_detour_handles[i];
        if (handle == 0xFFFFFFFF || Compound_hulls[handle & 0xFFFF].handle != handle) {
            hp->pf.problem_detour_handles[i] = hp->pf.problem_detour_handles[hp->pf.num_problem_detours - 1];
            hp->pf.num_problem_detours--;
            i--;
        } else {
            ncd_add_impassable_nav_cells_for_compound_hull(Compound_hulls + (handle & 0xFFFF), ignore_handle_one, ignore_handle_two);
        }
    }
}

// 0x00AF30E0 (:6899)
void nav_cell_detour_navpoint_removed(navpoint* navp)
{
    ncd_remove_navpoint_block_refs(navp);
}

// 0x00AF30F0 (:6907)
void nav_cell_detour_action_node_removed(object_action_node* node)
{
    ncd_remove_action_node_block_refs(node);
}

// 0x00AF3100 (:6915)
void nav_cell_detour_guard_node_removed(object_guard_node* node)
{
    ncd_remove_guard_node_block_refs(node);
}
