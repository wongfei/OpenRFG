// Original: volition/rfg/code/objects/object_cover_node.cpp
//
// Whole (NPC.md NP2). Zones place cover nodes; vehicles and barricades create dynamic ones through the same serialize.

#include "volition/rfg/code/objects/object_cover_node.h"

#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/memory/object_pool.h"

#include <math.h>
#include <string.h>

// size 0x6: what older zone files hold in CN_PROP_COMBINED_OLD.
struct object_cover_node_properties_old
{
    unsigned char def_angle_left; // 0x00
    unsigned char def_angle_right; // 0x01
    unsigned char off_angle_left; // 0x02
    unsigned char off_angle_right; // 0x03
    unsigned short flags; // 0x04
};

// size 0x10: what zone files hold in CN_PROP_COMBINED.
struct object_cover_node_properties
{
    int version; // 0x00
    float heading; // 0x04
    unsigned char def_angle_left; // 0x08
    unsigned char def_angle_right; // 0x09
    unsigned char off_angle_left; // 0x0A
    unsigned char off_angle_right; // 0x0B
    unsigned short flags; // 0x0C
};

// The bits of both properties' flags.
static const unsigned short CN_FILE_FLAG_CROUCH = 1;
static const unsigned short CN_FILE_FLAG_FIRE_LEFT = 2;
static const unsigned short CN_FILE_FLAG_FIRE_RIGHT = 4;
static const unsigned short CN_FILE_FLAG_FIRE_POPUP = 8; // only read on a crouching node

static const int MAX_COVER_NODES = 5120;
static const int DESTRUCTION_EVENT_MAX_NODES = 64; // 0x40 handles on the stack

static object_pool<cover_node, MAX_COVER_NODES, 1> cover_nodepool; // 0x032A0680

// Where a destruction event looks at a node, in the node's space, and how far it reaches past the event's box.
static const vector DESTRUCTION_EVENT_CHECK_OFFSETS[2] = { vector(0.0f, 0.0f, 0.0f), vector(0.0f, 0.0f, 0.25f) }; // 0x016AD218
static vector DESTRUCTION_EVENT_EXPAND_BMAX_OFFSET_COVER_NODE(0.75f, 0.25f, 0.75f); // 0x016AD230
static vector DESTRUCTION_EVENT_EXPAND_BMIN_OFFSET_COVER_NODE(-0.75f, -0.75f, -0.75f); // 0x016AD23C

// 0x00A16750 (:476)
void types_cover_node(signed char& type, signed char& subtype)
{
    type = OT_COVER_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A16760 (:476)
int bpo_index_cover_node()
{
    return 3;
}

// 0x00A16770 (:476)
const char* classname_cover_node()
{
    return "cover_node";
}

// 0x00A62500 (:476)
object* alloc_cover_node()
{
    return cover_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01076E90), the pool's bytes and capacity.
static obj_class_instantiator cover_node_instantiator("cover_node", alloc_cover_node, types_cover_node, bpo_index_cover_node, classname_cover_node, cover_node::pool_remaining, 0xF6440, MAX_COVER_NODES); // 0x033AFEDC

// 0x00A16780 (:476)
unsigned int cover_node::get_classname_hash()
{
    return cover_node_instantiator.this_hash;
}

// 0x00A3A4C0 (:476)
int cover_node::pool_remaining()
{
    return cover_nodepool.get_free();
}

// 0x00A8F7D0 (:476)
void cover_node::pool_free()
{
    cover_nodepool.free(this);
}

// 0x00A16790 (:477)
cover_node::cover_node()
{
}

// 0x00A167D0
cover_node::~cover_node()
{
}

// 0x00A05A50 (.h:59)
const char* cover_node::get_classname_static()
{
    return "cover_node";
}

// 0x00A167C0 (.h:61)
const char* cover_node::get_classname() const
{
    return "cover_node";
}

// 0x005DB9F0 (.h:60): World.collect_objects<cover_node> walks this bpo list.
int cover_node::get_bpo_index_static()
{
    return 3;
}

// 0x00A167E0 (:483): a node disabled for good is not saved.
obj_serialize_mode cover_node::serialize_mode()
{
    return disabled_timer.valid() ? OSM_RESTORE : OSM_NONE;
}

// Rounds an angle in degrees to the byte a node stores, within 0 to 180.
static unsigned char cover_angle_to_byte(float degrees)
{
    if (degrees - 0.0f < 0.0f)
    {
        degrees = 0.0f;
    }
    if (0.0f <= degrees - 180.0f)
    {
        degrees = 180.0f;
    }
    return (unsigned char)fl_round(degrees);
}

// 0x00AA6000 (:496)
bool cover_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        // rfg.exe prints "Cover nodes should never be deserialized! They use OSM_RESTORE." here, though this is the saving
        // side (original wording).
        return true;
    }
    World.set_object_name(this, "");
    memset(&flags, 0, sizeof(flags));
    death_count = 0;
    death_count_timer.set(0);
    recorded_hits = 0;
    disabled_timer.set(0);
    flags.off_navmesh = 1;
    closest_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;

    bool from_zone_file = true;
    unsigned char* combined;
    if (pblock.get_buf_optional(CN_PROP_COMBINED, combined) > 0)
    {
        const object_cover_node_properties* props = reinterpret_cast<const object_cover_node_properties*>(combined);
        def_left_deg = props->def_angle_left;
        def_right_deg = props->def_angle_right;
        off_left_deg = props->off_angle_left < def_left_deg ? props->off_angle_left : def_left_deg;
        off_right_deg = props->off_angle_right < def_right_deg ? props->off_angle_right : def_right_deg;
        flags.zone = 1;
        if ((props->flags & CN_FILE_FLAG_CROUCH) != 0)
        {
            flags.crouch = 1;
            if ((props->flags & CN_FILE_FLAG_FIRE_POPUP) != 0)
            {
                flags.fire_popup = 1;
            }
        }
        if ((props->flags & CN_FILE_FLAG_FIRE_LEFT) != 0)
        {
            flags.fire_left = 1;
        }
        if ((props->flags & CN_FILE_FLAG_FIRE_RIGHT) != 0)
        {
            flags.fire_right = 1;
        }
        // Only this format replaces the orientation.
        orient.make_ry(-props->heading);
    }
    else
    {
        object_cover_node_properties_old old_props;
        if (pblock.get_optional(CN_PROP_COMBINED_OLD, old_props))
        {
            def_left_deg = old_props.def_angle_left;
            def_right_deg = old_props.def_angle_right;
            off_left_deg = old_props.off_angle_left < old_props.def_angle_left ? old_props.off_angle_left : old_props.def_angle_left;
            off_right_deg = old_props.off_angle_right < old_props.def_angle_right ? old_props.off_angle_right : old_props.def_angle_right;
            flags.zone = 1;
            if ((old_props.flags & CN_FILE_FLAG_CROUCH) != 0)
            {
                flags.crouch = 1;
                if ((old_props.flags & CN_FILE_FLAG_FIRE_POPUP) != 0)
                {
                    flags.fire_popup = 1;
                }
            }
            if ((old_props.flags & CN_FILE_FLAG_FIRE_LEFT) != 0)
            {
                flags.fire_left = 1;
            }
            if ((old_props.flags & CN_FILE_FLAG_FIRE_RIGHT) != 0)
            {
                flags.fire_right = 1;
            }
        }
        else
        {
            // Magic number: a defensive arc defaults to 90 degrees each way.
            float def_left = 90.0f;
            float def_right = 90.0f;
            pblock.get_optional(CN_PROP_DEFENSIVE_ANGLE_LEFT, def_left);
            pblock.get_optional(CN_PROP_DEFENSIVE_ANGLE_RIGHT, def_right);
            float off_left;
            float off_right;
            if (!pblock.get_optional(CN_PROP_OFFENSIVE_ANGLE_LEFT, off_left))
            {
                pblock.get(CN_PROP_ANGLE_LEFT, off_left);
            }
            if (!pblock.get_optional(CN_PROP_OFFENSIVE_ANGLE_RIGHT, off_right))
            {
                pblock.get(CN_PROP_ANGLE_RIGHT, off_right);
            }
            off_left_deg = cover_angle_to_byte(off_left);
            off_right_deg = cover_angle_to_byte(off_right);
            def_left_deg = cover_angle_to_byte(def_left);
            def_right_deg = cover_angle_to_byte(def_right);
            if (def_left_deg < off_left_deg)
            {
                off_left_deg = def_left_deg;
            }
            if (def_right_deg < off_right_deg)
            {
                off_right_deg = def_right_deg;
            }
            if (pblock.get_optional(CN_PROP_ALL_FLAGS, flags))
            {
                // Vehicles and barricades: their position is exact.
                from_zone_file = false;
            }
            else
            {
                char str[512]; // Magic number: rfg.exe's stack buffer.
                pblock.get(CN_PROP_STANCE, str);
                if (strstr(str, "crouching") != nullptr)
                {
                    flags.crouch = 1;
                }
                pblock.get(CN_PROP_FIRING_FLAGS, str);
                if (flags.crouch != 0 && strstr(str, "standing") != nullptr)
                {
                    flags.fire_popup = 1;
                }
                if (strstr(str, "left") != nullptr)
                {
                    flags.fire_left = 1;
                }
                if (strstr(str, "right") != nullptr)
                {
                    flags.fire_right = 1;
                }
                flags.zone = 1;
            }
        }
    }
    if (from_zone_file)
    {
        // Magic number: placed nodes move 0.17 forward.
        pos += orient.fvec * 0.17f;
    }
    if (flags.dynamic == 0 && flags.vehicle == 0)
    {
        flags.zone = 1;
    }
    return true;
}

// 0x00A62510 (:754)
bool cover_node::stream(bool load)
{
    if (load)
    {
        locate_node();
    }
    else if (obj_flags.destroy_on_stream != 0)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00AA6560 (:797): the first human that reserved this node gives it up. Only one is told, and the handle is read after
// object::free_this (both original).
void cover_node::free_this()
{
    object::free_this();
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->combat.reserved_cover_node == get_handle())
        {
            human_combat_cover_abandon_node(hp);
            return;
        }
    }
}

// 0x00A16800 (:815)
void cover_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A16DB0 (:1209)
void cover_node::reenable()
{
    flags.disabled_reason = COVER_DISABLED_NONE;
    disabled_timer.set(0);
}

// 0x00A16DD0 (:1217)
void cover_node::increment_death_count()
{
    if (death_count_timer.elapsed())
    {
        death_count = 1;
    }
    else
    {
        death_count = death_count + 1;
    }
    // Magic numbers: deaths count for 5 minutes; 3 of them disable the node for good, fewer for 15 seconds.
    death_count_timer.set(300000);
    if (death_count < 3)
    {
        disable(COVER_DISABLED_DEATHS, 15000);
    }
    else
    {
        death_count = 3;
        disable(COVER_DISABLED_DEATHS, -1);
    }
}

// 0x00A16E90 (:1292): true when this hit disabled the node. attacker is not used.
bool cover_node::register_hit(const human*)
{
    recorded_hits = recorded_hits + 1;
    // Magic number: 5 hits disable the node for 10 seconds.
    if (recorded_hits < 5)
    {
        return false;
    }
    recorded_hits = 5;
    disable(COVER_DISABLED_HITS, 10000);
    return true;
}

// 0x00A16F10 (:1317)
bool cover_node::is_inside(const vector& point) const
{
    return vec_dist_squared(point, pos) <= 1.0f;
}

// 0x00A16F40 (:1325)
void cover_node::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin += pos;
    bmax += pos;
}

// 0x00A17010 (:1333)
void cover_node::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = vector(-0.5f, -0.5f, -0.5f);
    bmax = vector(0.5f, 0.5f, 0.5f);
}

// 0x00A05A60 (.h:110)
bool cover_node::is_permanently_disabled() const
{
    return !disabled_timer.valid();
}

// 0x00A3A4F0 (:1339): finds what the node stands on, the navmesh or the floors of its parent building.
void cover_node::locate_node()
{
    float cell_dist_sq = -1.0f;
    nav_cell* closest_cell = nullptr;
    // Uninitialized in rfg.exe, but only stored when a cell was found (0x00A3A683).
    vector adjusted_pos(0.0f, 0.0f, 0.0f);
    flags.off_navmesh = 1;
    flags.on_mover = 0;

    float terrain_y;
    // Magic number: a node up to 0.65 above the terrain stands on it.
    const bool near_terrain = havok_terrain_height(terrain_y, pos.x, pos.z, false, nullptr) && !(0.65f < pos.y - terrain_y);
    closest_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    if (near_terrain)
    {
        bool in_cell = false;
        // Magic number: the search radius.
        if (bb_pf_find_closest_point_on_nav_mesh_fast(adjusted_pos, closest_cell, pos, &cell_dist_sq, &in_cell, false, 0.7f))
        {
            if (in_cell)
            {
                flags.off_navmesh = 0;
                adjusted_pos.y = terrain_y;
                closest_point = adjusted_pos;
                closest_nav_cell_handle = nav_cell_get_handle(closest_cell);
                // 0x00A3A625: a node inside a cell is done, whatever its parent is.
                return;
            }
            // Magic number: 0.7 squared in single precision.
            if (0.48999998f < cell_dist_sq)
            {
                cell_dist_sq = -1.0f;
                closest_cell = nullptr;
            }
        }
    }

    bool on_mover = closest_cell == nullptr;
    const unsigned int parent_handle = get_parent();
    object* parent_obj = parent_handle != 0xFFFFFFFF ? World.object_from_handle(parent_handle) : nullptr;
    if (parent_obj != nullptr && parent_obj->get_obj_type() == OT_MOVER && parent_obj->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        const navpoint* closest_navpoint = nullptr;
        float closest_navpoint_dist_sq = -1.0f;
        object* const first = parent_obj->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            const navpoint* npp = child->get_obj_type() == OT_NAVPOINT ? static_cast<const navpoint*>(child) : nullptr;
            // Magic numbers: types 10 and 14 are path and stairs, seen within 1.2 up or down.
            if (npp != nullptr && (npp->type == 10 || npp->type == 14) && fabsf(pos.y - npp->pos.y) <= 1.2f)
            {
                float dist = vec_xz_dist(npp->pos, pos) - npp->radius;
                for (int i = 0; i < npp->num_links; i++)
                {
                    const navpoint* other = npp->links[i];
                    // The address order takes each link inside one parent once.
                    if ((Navpoint_treat_all_as_enabled || npp->link_disabled_by[i] == 0) &&
                        (npp < other || other->get_parent() != npp->get_parent()) &&
                        (other->type == 10 || other->type == 14))
                    {
                        vector on_link;
                        ix_closest_point_on_line_segment(on_link, pos, npp->pos, other->pos);
                        const float radius = other->radius <= npp->radius ? other->radius : npp->radius;
                        const float link_dist = vec_xz_dist(on_link, pos) - radius;
                        if (0.0f <= dist - link_dist)
                        {
                            dist = link_dist;
                        }
                    }
                }
                const float dist_sq = dist * dist;
                // Magic number: only the first candidate may be further than 5 metres (25 squared), and it stays until a closer one.
                const float limit = 25.0f <= closest_navpoint_dist_sq ? 25.0f : closest_navpoint_dist_sq;
                if (closest_navpoint == nullptr || dist_sq < limit)
                {
                    closest_navpoint_dist_sq = dist_sq;
                    closest_navpoint = npp;
                }
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
        if (closest_navpoint != nullptr && closest_navpoint_dist_sq < cell_dist_sq)
        {
            on_mover = true;
        }
    }
    if (on_mover)
    {
        // 0x00A3A8DF: no cell, or a closer navpoint, stores neither the point nor the handle.
        flags.on_mover = 1;
        return;
    }
    closest_point = adjusted_pos;
    closest_nav_cell_handle = nav_cell_get_handle(closest_cell);
}

// 0x00A3A8F0 (:1548)
void disable_all_cover_nodes_in_a_building(rfg_mover* mover)
{
    if (mover == nullptr)
    {
        return;
    }
    object* const first = mover->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_COVER_NODE)
        {
            static_cast<cover_node*>(child)->disable(COVER_DISABLED_DESTRUCTION, -1);
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
}

// 0x00A3A950 (:1567)
void enable_all_cover_nodes_in_a_building(rfg_mover* mover)
{
    if (mover == nullptr)
    {
        return;
    }
    object* const first = mover->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_COVER_NODE)
        {
            static_cast<cover_node*>(child)->reenable();
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
}

// 0x00A62640 (:1590): a destroyed region takes the nodes it reaches with it, for good. Glass does not count.
void cover_dyn_process_destruction_event(const rfg_destruction_event* eventp)
{
    if (eventp->flags.is_glass != 0)
    {
        return;
    }
    const vector bmax = eventp->bmax + DESTRUCTION_EVENT_EXPAND_BMAX_OFFSET_COVER_NODE;
    const vector bmin = eventp->bmin + DESTRUCTION_EVENT_EXPAND_BMIN_OFFSET_COVER_NODE;
    vector world_bmin;
    vector world_bmax;
    bbox_instance(world_bmin, world_bmax, eventp->orient, eventp->pos, bmin, bmax);
    unsigned int handles[DESTRUCTION_EVENT_MAX_NODES];
    const int count = World.collect_objects<cover_node>(world_bmin, world_bmax, handles, DESTRUCTION_EVENT_MAX_NODES, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_COVER_NODE)
        {
            continue;
        }
        cover_node* node = static_cast<cover_node*>(objp);
        if (node->is_permanently_disabled())
        {
            continue;
        }
        for (int check = 0; check < 2; check++)
        {
            vector point = DESTRUCTION_EVENT_CHECK_OFFSETS[check];
            vec_change_to_global_coords(point, node->pos, node->orient);
            if (ix_point_in_box_oriented(point, eventp->pos, eventp->orient, bmin, bmax))
            {
                node->disable(COVER_DISABLED_DESTRUCTION, -1);
            }
        }
    }
}

// cos(i degrees) for 0..90. rfg.exe has it as data, identical except that [90] is exactly 0.
static const int COMPRESSED_COSINE_ENTRIES = 91;

struct compressed_cosine_table
{
    compressed_cosine_table()
    {
        for (int i = 0; i < COMPRESSED_COSINE_ENTRIES - 1; i++)
        {
            values[i] = (float)cos(i * 3.14159265358979323846 / 180.0);
        }
        values[COMPRESSED_COSINE_ENTRIES - 1] = 0.0f;
    }

    float values[COMPRESSED_COSINE_ENTRIES];
};

static const compressed_cosine_table Compressed_cosine_table;

// 0x00A16600 (:433): a node's angles are bytes in degrees. Past 180 the index runs off the front of the table (original).
float get_compressed_cosine_ubyte(unsigned char degrees)
{
    if (degrees <= 90)
    {
        return Compressed_cosine_table.values[degrees];
    }
    return -Compressed_cosine_table.values[180 - degrees];
}

// 0x00A16630 (:448)
float get_compressed_sine_ubyte(unsigned char degrees)
{
    return degrees <= 90 ? Compressed_cosine_table.values[90 - degrees] : Compressed_cosine_table.values[degrees - 90];
}

// 0x00A16660 (:463)
float get_compressed_cosine_int(int degrees)
{
    // Only the low byte counts, so +-256 and beyond wrap (original).
    const unsigned char magnitude = static_cast<unsigned char>(degrees < 0 ? -degrees : degrees);
    if (magnitude <= 90)
    {
        return Compressed_cosine_table.values[magnitude];
    }
    return -Compressed_cosine_table.values[180 - magnitude];
}

// 0x00A166B0 (:471)
float get_compressed_sine_int(int degrees)
{
    const unsigned char magnitude = static_cast<unsigned char>(degrees < 0 ? -degrees : degrees);
    const float sine = magnitude <= 90 ? Compressed_cosine_table.values[90 - magnitude] : Compressed_cosine_table.values[magnitude - 90];
    return degrees < 0 ? -sine : sine;
}

// 0x005DBA80 (:109): disabled unless a valid timer has run out, so with no timer at all it counts as disabled.
bool cover_node::is_disabled() const
{
    return !(disabled_timer.valid() && disabled_timer.elapsed());
}

// 0x005DBA00 (.h:91)
bool cover_node::is_defense_only() const
{
    return flags.fire_popup == 0 && flags.fire_left == 0 && flags.fire_right == 0;
}

// 0x00A16B40 (:921)
bool cover_node::in_firing_arc(const vector& target_xz_dir) const
{
    const unsigned char degrees = orient.rvec.y * target_xz_dir.y + target_xz_dir.x * orient.rvec.x + orient.rvec.z * target_xz_dir.z < 0.0f ? off_left_deg : off_right_deg;
    return !(orient.fvec.y * target_xz_dir.y + target_xz_dir.x * orient.fvec.x + orient.fvec.z * target_xz_dir.z < get_compressed_cosine_ubyte(degrees));
}

// 0x00A16C30 (:945)
bool cover_node::in_defensive_arc(const vector& target_xz_dir) const
{
    const unsigned char degrees = orient.rvec.y * target_xz_dir.y + target_xz_dir.x * orient.rvec.x + orient.rvec.z * target_xz_dir.z < 0.0f ? def_left_deg : def_right_deg;
    return !(orient.fvec.y * target_xz_dir.y + target_xz_dir.x * orient.fvec.x + orient.fvec.z * target_xz_dir.z < get_compressed_cosine_ubyte(degrees));
}

// 0x00A62490: the ray ignores humans and the vehicle or turret either side is using. data is the attacker, then the target.
static bool cover_line_of_fire_callback(object* hit_obj, void* data_ptr)
{
    if (hit_obj->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    const human* const* humans = (const human* const*)data_ptr;
    for (int i = 0; i < 2; ++i)
    {
        if (humans[i] != nullptr && (hit_obj->get_handle() == humans[i]->vehicle_handle || (object*)human_combat_get_manned_turret(humans[i]) == hit_obj))
        {
            return false;
        }
    }
    return true;
}

// 0x00A8F7E0 (:977)
bool cover_node::has_line_of_fire(const vector& target_pos, const human* attacker_hp, const human* target_hp) const
{
    // Magic numbers: where a gun is held at the node, 1.32 up from a popup, otherwise 1.2 up standing or 0.9 crouching and
    // 0.7 out to the side that is open.
    vector fire_pos = pos;
    if (flags.fire_popup != 0)
    {
        fire_pos.y = pos.y + 1.32f;
    }
    else
    {
        fire_pos.y = pos.y + (flags.crouch == 0 ? 1.2f : 0.9f);
        if (flags.fire_right == 0 || (flags.fire_left != 0 && orient.rvec.x * (target_pos.x - pos.x) + orient.rvec.y * (target_pos.y - pos.y) + orient.rvec.z * (target_pos.z - pos.z) < 0.0f))
        {
            if (flags.fire_left == 0)
            {
                return false;
            }
            fire_pos.x = fire_pos.x - orient.rvec.x * 0.7f;
            fire_pos.y = fire_pos.y - orient.rvec.y * 0.7f;
            fire_pos.z = fire_pos.z - orient.rvec.z * 0.7f;
        }
        else
        {
            fire_pos.x = orient.rvec.x * 0.7f + fire_pos.x;
            fire_pos.y = orient.rvec.y * 0.7f + fire_pos.y;
            fire_pos.z = orient.rvec.z * 0.7f + fire_pos.z;
        }
    }
    const human* humans[2] = { attacker_hp, target_hp };
    raycast_input in;
    in.start = fire_pos;
    in.end = target_pos;
    in.radius = 0.0f;
    in.filter_params = humans;
    in.filter_callback = cover_line_of_fire_callback;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    // Magic number: raycast flags, not recovered as names.
    in.m_flags = 0x12;
    raycast_output out;
    if (havok_raycast(in, out))
    {
        // Magic number: a hit within half a metre of the target counts as reaching it.
        return !(0.25f < vec_dist_squared(out.m_hits[0].hit_pos, target_pos));
    }
    return true;
}

// 0x00A8FB10 (:1068)
cover_check_results cover_node::check_cover_type(const human* hp, const vector& target_pos, bool check_occupied, bool check_los, bool check_range, const human* target_hp) const
{
    if (is_disabled())
    {
        return CCR_NO_COVER;
    }
    if (check_occupied && is_occupied(hp) != nullptr)
    {
        return CCR_NO_COVER;
    }
    vector target_dir(target_pos.x - pos.x, target_pos.y - pos.y, target_pos.z - pos.z);
    const float target_dist = target_dir.flatten_mag_and_normalize();
    if (!in_defensive_arc(target_dir))
    {
        return CCR_NO_COVER;
    }
    if (is_defense_only())
    {
        return CCR_DEFENSIVE_ONLY;
    }
    // A target in cover, or off this level, right next to the node (magic numbers: 1 metre up or down, under 2 metres away).
    if (target_hp != nullptr && target_dist < 2.0f &&
        ((target_hp->get_sub_type() == OT_SUB_HUMAN_NPC && target_hp->cur_state == HS_COVER) ||
         (target_hp->movement_mode != HMM_JUMP && target_hp->movement_mode != HMM_FALL && 1.0f < fabsf(pos.y - target_hp->pos.y))))
    {
        return CCR_DEFENSIVE_ONLY;
    }
    // Magic number: the range when no human's weapon says otherwise.
    float range = 75.0f;
    if (check_range && hp != nullptr)
    {
        const weapon_info* info = human_weapons_get_weapon_info(hp);
        if (info == nullptr || !info->is_firearm())
        {
            return CCR_DEFENSIVE_ONLY;
        }
        range = info->max_range;
    }
    if (range < target_dist || !in_firing_arc(target_dir))
    {
        return CCR_DEFENSIVE_ONLY;
    }
    if (check_los && !has_line_of_fire(target_pos, hp, target_hp))
    {
        return CCR_DEFENSIVE_ONLY;
    }
    return CCR_OFFENSIVE;
}

// 0x00AB3D60 (:1049): aims at where the target's body or gun will be, at its own cover node if it has one.
cover_check_results cover_node::check_cover_type(const human* hp, const object* target, bool check_occupied, bool check_los, bool check_range) const
{
    vector aim_pos = target->pos;
    const human* target_hp = nullptr;
    if (target->get_obj_type() == OT_HUMAN)
    {
        target_hp = (const human*)target;
        const cover_node* target_node = human_combat_cover_get_node(target_hp);
        if (target_node == nullptr)
        {
            // Magic number: chest height.
            aim_pos.y = aim_pos.y + 1.6f;
        }
        else if (target_node->flags.fire_popup != 0)
        {
            aim_pos = target_node->pos;
            aim_pos.y = aim_pos.y + 1.32f;
        }
        else
        {
            aim_pos = target_node->pos;
            aim_pos.y = aim_pos.y + (target_node->flags.crouch == 0 ? 1.2f : 0.9f);
            const vector& rvec = target_node->orient.rvec;
            if (target_node->flags.fire_right == 0 ||
                (target_node->flags.fire_left != 0 && rvec.y * (hp->pos.y - target_node->pos.y) + rvec.x * (hp->pos.x - target_node->pos.x) + rvec.z * (hp->pos.z - target_node->pos.z) < 0.0f))
            {
                // A target that cannot fire out from its node cannot hit this one, so only the defense counts.
                if (target_node->flags.fire_left == 0)
                {
                    return will_provide_defensive_cover(hp, target->pos, check_occupied) ? CCR_DEFENSIVE_ONLY : CCR_NO_COVER;
                }
                aim_pos.x = aim_pos.x - rvec.x * 0.7f;
                aim_pos.y = aim_pos.y - rvec.y * 0.7f;
                aim_pos.z = aim_pos.z - rvec.z * 0.7f;
            }
            else
            {
                aim_pos.x = rvec.x * 0.7f + aim_pos.x;
                aim_pos.y = rvec.y * 0.7f + aim_pos.y;
                aim_pos.z = rvec.z * 0.7f + aim_pos.z;
            }
        }
    }
    return check_cover_type(hp, aim_pos, check_occupied, check_los, check_range, target_hp);
}

// 0x00A8FE00 (:1164)
bool cover_node::will_provide_defensive_cover(const human* hp, const vector& target_pos, bool check_occupied) const
{
    if (is_disabled())
    {
        return false;
    }
    if (check_occupied && is_occupied(hp) != nullptr)
    {
        return false;
    }
    vector target_dir(target_pos.x - pos.x, target_pos.y - pos.y, target_pos.z - pos.z);
    target_dir.flatten_normalize_safe();
    return in_defensive_arc(target_dir);
}

// 0x00AA65C0 (:1160)
bool cover_node::will_provide_defensive_cover(const human* hp, const object* target, bool check_occupied) const
{
    return will_provide_defensive_cover(hp, target->pos, check_occupied);
}

// 0x00A16D20 (:1182): a time of -1 disables for good, otherwise the longer of the two times wins.
void cover_node::disable(cover_node_disabled_reasons reason, int time)
{
    if (reason == COVER_DISABLED_NONE || !disabled_timer.valid())
    {
        return;
    }
    if (time == -1)
    {
        disabled_timer.invalidate();
        flags.disabled_reason = (unsigned char)reason;
    }
    else if (disabled_timer.elapsed() || disabled_timer.time_until() < time)
    {
        disabled_timer.set(time);
        flags.disabled_reason = (unsigned char)reason;
    }
}

// 0x00A62540 (:1246): the human that has reserved the node, or stands closer to it than the one asking (within a metre).
const human* cover_node::is_occupied(const human* checking_human) const
{
    const unsigned int node_handle = get_handle();
    if (checking_human != nullptr && checking_human->combat.reserved_cover_node == node_handle)
    {
        return nullptr;
    }
    float radius_sq = 1.0f;
    if (checking_human != nullptr)
    {
        const float dist_sq = vec_dist_squared(checking_human->pos, pos);
        if (0.0f <= 1.0f - dist_sq)
        {
            radius_sq = dist_sq;
        }
    }
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; ++i)
    {
        const human* hp = World.get_object<human>(i);
        if (hp != checking_human && (vec_dist_squared(hp->pos, pos) <= radius_sq || hp->combat.reserved_cover_node == node_handle))
        {
            return hp;
        }
    }
    return nullptr;
}

// 0x00A17050 (:1642)
cover_node_stances cover_stance_from_state(const human* hp)
{
    if (hp->cur_state == HS_COVER)
    {
        switch (hp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_RIGHT_LEANUP:
            return hp->cur_stance != HST_STAND ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_STAND_RIGHT;
        case HMSM_COVER_LEFT:
        case HMSM_COVER_LEFT_LEAN:
        case HMSM_COVER_LEFT_LEANUP:
            return hp->cur_stance != HST_STAND ? COVER_STANCE_CROUCH_LEFT : COVER_STANCE_STAND_LEFT;
        default:
            break;
        }
    }
    return COVER_STANCE_UNDEFINED;
}

// 0x00A170D0 (:1671)
cover_node_firing_positions cover_firing_position_from_state(const human* hp)
{
    if (hp->cur_state == HS_COVER)
    {
        switch (hp->movement_submode)
        {
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_LEFT_LEAN:
            return COVER_FIRE_LEANOUT;
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_LEFT_LEANUP:
            return COVER_FIRE_POPUP;
        default:
            break;
        }
    }
    return COVER_FIRE_UNDEFINED;
}

// 0x00A16B10 (:908)
bool cover_node::has_standing_stepout() const
{
    if (flags.fire_popup != 0)
    {
        return true;
    }
    return flags.crouch == 0 && (flags.fire_left != 0 || flags.fire_right != 0);
}

// 0x00A16820 (:834)
bool cover_node::get_approximate_firing_position(vector& fire_pos, const vector& target_pos, cover_node_stances* stance, cover_node_firing_positions* firing_position) const
{
    fire_pos = pos;
    const bool target_on_right = 0.0f <= orient.rvec * (target_pos - pos);
    const bool standing = flags.crouch == 0;
    if (flags.fire_popup != 0)
    {
        if (stance != nullptr)
        {
            if (flags.fire_left == 0 && flags.fire_right != 0)
            {
                *stance = COVER_STANCE_CROUCH_RIGHT;
            }
            else if (flags.fire_left != 0 && flags.fire_right == 0)
            {
                *stance = COVER_STANCE_CROUCH_LEFT;
            }
            else
            {
                *stance = target_on_right ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_CROUCH_LEFT;
            }
        }
        if (firing_position != nullptr)
        {
            *firing_position = COVER_FIRE_POPUP;
        }
        fire_pos.y += 1.32f; // popup height
        return true;
    }
    if (firing_position != nullptr)
    {
        *firing_position = COVER_FIRE_LEANOUT;
    }
    fire_pos.y += standing ? 1.2f : 0.9f; // lean out heights
    if (flags.fire_right != 0 && (flags.fire_left == 0 || target_on_right))
    {
        if (stance != nullptr)
        {
            *stance = standing ? COVER_STANCE_STAND_RIGHT : COVER_STANCE_CROUCH_RIGHT;
        }
        fire_pos += orient.rvec * 0.7f;
        return true;
    }
    if (flags.fire_left != 0)
    {
        if (stance != nullptr)
        {
            *stance = standing ? COVER_STANCE_STAND_LEFT : COVER_STANCE_CROUCH_LEFT;
        }
        fire_pos -= orient.rvec * 0.7f;
        return true;
    }
    return false;
}

// 0x005DBA50 (.h:97)
bool cover_node::will_provide_offensive_cover(const human* hp, const vector& target_pos, bool check_occupied, bool check_los, bool check_range, const human* target_hp) const
{
    return check_cover_type(hp, target_pos, check_occupied, check_los, check_range, target_hp) == CCR_OFFENSIVE;
}
