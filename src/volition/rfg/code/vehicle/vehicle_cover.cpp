// Original: volition/rfg/code/vehicle/vehicle_cover.cpp
//
// Temporary cover nodes along stopped vehicles, from the cover node definitions of the mesh.

#include "volition/rfg/code/vehicle/vehicle_cover.h"

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser.h"
#include "volition/vlib/code/util/string_comp.h"

#include <math.h>

// ---- file-static globals
static const vector VEHICLE_COVER_BMIN(-0.5f, 0.1f, -0.5f); // 0x016BA5C8
static const vector VEHICLE_COVER_BMAX(0.5f, 1.9f, 0.5f); // 0x016BA5D4
object_pool<vehicle_cover_info,163,1> Vehicle_cover_info_pool; // 0x0383D3A0

static const float VEHICLE_COVER_MAX_GROUND_OFFSET = 0.3f; // a node this far off the terrain is not used

// 0x00B662D0 (:78)
static bool vehicle_cover_cb(object* objp, unsigned int shape_key, const void* data)
{
    if (objp != NULL) {
        const signed char type = (signed char)objp->get_obj_type();
        // object types 0, 1 and 0x13 do not block a node
        if (type < 0 || (1 < type && type != 0x13)) {
            return objp != data;
        }
    }
    return false;
}

// 0x00B66300 (:105)
void vehicle_cover_process_start()
{
    Processed_this_frame = false;
    Checked_this_frame = false;
}

// 0x00B66310 (:110)
bool vehicle_cover_can_process()
{
    return !Processed_this_frame;
}

// 0x00B66320 (:114)
bool vehicle_cover_can_check()
{
    return !Checked_this_frame;
}

// 0x00B66330 (:119)
void vehicle_cover_register_check()
{
    Num_vehicles_checking_cover++;
}

// 0x00B66340 (:124)
void vehicle_cover_clear_registrations()
{
    Num_vehicles_checking_cover_last_frame = Num_vehicles_checking_cover;
    Num_vehicles_checking_cover = 0;
}

// 0x00B66360 (:130)
int vehicle_cover_num_vehicles_checking()
{
    return Num_vehicles_checking_cover_last_frame;
}

// 0x00B8EA70 (:137)
void vehicle_cover_regenerate_all()
{
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        for (int n = 0; n < vp->vehicle_cover_nodes.size(); n++) {
            World.destroy_object(vp->vehicle_cover_nodes[n], false);
        }
        vp->vehicle_cover_nodes.clear();
    }
}

// Frees every entry (inlined in rfg.exe): free returns the next one.
static void vehicle_cover_info_free_all()
{
    Vehicle_cover_info_pool.thread_access.enter();
    for (vehicle_cover_info* info = Vehicle_cover_info_pool.get_first(0); info != NULL; info = Vehicle_cover_info_pool.free(info)) {
    }
    Vehicle_cover_info_pool.thread_access.leave();
}

// 0x00BAAA60 (:152)
void vehicle_cover_info_init(int dlc_id)
{
    if (dlc_id == -1) {
        vehicle_cover_info_free_all();
    }
    vehicle_parse_cover_table(Vehicle_cover_info_pool, dlc_id);
}

// 0x00B8EB00 (:166)
const vehicle_cover_info* vehicle_cover_get_info_for_mesh(const char* mesh_name)
{
    Vehicle_cover_info_pool.thread_access.enter();
    for (vehicle_cover_info* info = Vehicle_cover_info_pool.get_first(0); info != NULL; info = Vehicle_cover_info_pool.get_next(info)) {
        if (v_stricmp(mesh_name, info->mesh_name) == 0) {
            Vehicle_cover_info_pool.thread_access.leave();
            return info;
        }
    }
    Vehicle_cover_info_pool.thread_access.leave();
    return NULL;
}

// The node's world position and orientation (flattened); false when it is too far off the terrain.
static bool vehicle_cover_node_placement(const vehicle* vp, const vehicle_cover_node_definition& def, vector& pos, matrix& orient)
{
    pos = def.offset;
    vec_change_to_global_coords(pos, vp->pos, vp->orient);
    vector fvec = def.relative_fvec;
    vec_change_to_global_coords(fvec, ZERO_VECTOR, vp->orient);
    fvec.flatten_normalize_safe();
    orient.make_quick(fvec);
    float height;
    havok_terrain_height(height, pos.x, pos.z, false, NULL);
    if (fabsf(height - pos.y) > VEHICLE_COVER_MAX_GROUND_OFFSET) {
        return false;
    }
    pos.y = height;
    return true;
}

static unsigned int vehicle_cover_spawn_node(const vehicle* vp, const vehicle_cover_node_definition& def, const vector& pos, const matrix& orient)
{
    obj_prop_block props(pos, orient, NULL);
    props.set<float>(CN_PROP_DEFENSIVE_ANGLE_LEFT, def.def_left_arc);
    props.set<float>(CN_PROP_DEFENSIVE_ANGLE_RIGHT, def.def_right_arc);
    props.set<float>(CN_PROP_OFFENSIVE_ANGLE_LEFT, def.off_left_arc);
    props.set<float>(CN_PROP_OFFENSIVE_ANGLE_RIGHT, def.off_right_arc);
    props.set<cover_node_flags>(CN_PROP_ALL_FLAGS, def.cover_flags);
    cover_node* node = World.create_object<cover_node>(props, vp->get_handle());
    return node != NULL ? node->get_handle() : 0xFFFFFFFF;
}

// 0x00B7C050 (:187)
unsigned int vehicle_cover_create_node(const vehicle* vp, const vehicle_cover_node_definition& def)
{
    if (vp == NULL) {
        return 0xFFFFFFFF;
    }
    Processed_this_frame = true;
    vector pos;
    matrix orient;
    if (!vehicle_cover_node_placement(vp, def, pos, orient)) {
        return 0xFFFFFFFF;
    }
    unsigned int blocker;
    if (havok_collect_objects(VEHICLE_COVER_BMIN, VEHICLE_COVER_BMAX, pos, orient, &blocker, 1, COLLIDE_OBJECT_COLLECTOR, vehicle_cover_cb, vp) != 0) {
        return 0xFFFFFFFF;
    }
    return vehicle_cover_spawn_node(vp, def, pos, orient);
}

// 0x00B8EB70 (:251)
void vehicle_cover_check_node(vehicle* vp, const vehicle_cover_node_definition& def)
{
    if (vp == NULL) {
        return;
    }
    Checked_this_frame = true;
    vector pos;
    matrix orient;
    if (!vehicle_cover_node_placement(vp, def, pos, orient)) {
        return;
    }
    unsigned int blocker;
    int i = 0;
    for (; i < vp->vehicle_cover_nodes.size(); i++) {
        cover_node* node = World.object_from_handle<cover_node>(vp->vehicle_cover_nodes[i]);
        if (node != NULL && vec_xz_dist_squared(node->pos, pos) < 1.0f) {
            // a node exists here: it goes when something now blocks it
            if (havok_collect_objects(VEHICLE_COVER_BMIN, VEHICLE_COVER_BMAX, pos, orient, &blocker, 1, COLLIDE_OBJECT_COLLECTOR, vehicle_cover_cb, vp) != 0) {
                World.destroy_object(node, false);
                vp->vehicle_cover_nodes.remove_unsorted(i);
            }
            return;
        }
    }
    const int num_definitions = (vp->info != NULL && vp->info->cover_info != NULL) ? vp->info->cover_info->node_info.size() : 0;
    if (i < num_definitions && vp->vehicle_cover_nodes.size() < 10) {
        if (havok_collect_objects(VEHICLE_COVER_BMIN, VEHICLE_COVER_BMAX, pos, orient, &blocker, 1, COLLIDE_OBJECT_COLLECTOR, vehicle_cover_cb, vp) == 0) {
            const unsigned int handle = vehicle_cover_spawn_node(vp, def, pos, orient);
            if (handle != 0xFFFFFFFF) {
                vp->vehicle_cover_nodes.add(handle);
            }
        }
    }
    else {
        vp->vehicle_cover_timestamp.set(1500);
        if (0 < vp->vehicle_cover_nodes.size()) {
            vp->vehicle_cover_destroy_nodes();
        }
    }
}
