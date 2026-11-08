// Original: volition/rfg/code/vehicle/vehicle_system.cpp
//
// The vehicle type table and the per-frame passes over every vehicle.

#include "volition/rfg/code/vehicle/vehicle_system.h"

#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_cover.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser_shared.h"
#include "volition/rfg/code/vehicle_ai/vehicle_path_request.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include <float.h>
#include <string.h>

// ---- file-static globals
bool Vehicle_system_avoid_despawn; // 0x03770045
static bool Vehicle_system_init; // 0x03770046
farray<vehicle_info,163> Vehicle_info_pool; // 0x016BA5F8

static const int INVALID_VEHICLE_INFO_INDEX = -1;

// More corpses than this are faded out, the far and unseen ones first.
static const int VEHICLE_MAX_CORPSES = 6;
static const int VEHICLE_MAX_CORPSES_WHILE_AVOIDING_DESPAWN = 12;

// 0x00B91970 (:139)
static void vehicle_force_corpse_removal(const int count)
{
    if (Vehicle_system_avoid_despawn && count <= VEHICLE_MAX_CORPSES_WHILE_AVOIDING_DESPAWN) {
        return;
    }
    // Original bug kept: 32 entries, filled without a bound.
    vehicle* corpses[32];
    float distances[32];
    bool protected_corpse[32];
    bool visible[32];
    int found = 0;
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->m_flags.m_corpse == 0 || vp->m_flags.m_despawn_after_fade != 0 || vp->m_flags.m_fading_out != 0) {
            continue;
        }
        vehicle_update_orient_pos_from_havok(vp);
        vector bmin;
        vector bmax;
        vp->get_bbox(bmin, bmax);
        const float distance = vp->render_distance.get_apparent_distance(vp->pos);
        distances[found] = distance < 0.0f ? -distance : distance;
        corpses[found] = vp;
        visible[found] = game_render_is_object_visible(bmin, bmax);
        protected_corpse[found] = mission_is_active() && Vehicle_system_avoid_despawn && object_is_local_player(vp->most_recent_driver);
        found++;
    }

    float farthest = -1.0f;
    int choice = -1;
    for (int i = 0; i < count; i++) {
        if (!visible[i] && !protected_corpse[i] && farthest < distances[i]) {
            choice = i;
            farthest = distances[i];
        }
    }
    if (choice == -1) {
        for (int i = 0; i < count; i++) {
            if (!protected_corpse[i] && farthest < distances[i]) {
                choice = i;
                farthest = distances[i];
            }
        }
    }
    if (choice == -1) {
        for (int i = 0; i < count; i++) {
            if (farthest < distances[i]) {
                choice = i;
                farthest = distances[i];
            }
        }
    }
    vehicle_fade_out(corpses[choice], 1000, true, false);
}

// 0x00B68BC0 (:246) (LTCG: handle in ESI, num_elements in EAX, list in EDX)
static bool add_collidable_vehicle_handle_to_list(unsigned int handle, float dist_sqr, float fdist, int max_size, int& num_elements, nearby_vehicle_info* list)
{
    if (max_size <= num_elements) {
        return false;
    }
    list[num_elements].handle = handle;
    list[num_elements].dist_sqr = dist_sqr;
    list[num_elements].fdist = fdist;
    num_elements++;
    return true;
}

// 0x00B84820 (:262)
void build_resource_information()
{
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        vehicle_info& info = Vehicle_info_pool[i];
        info.slot_id = info.srid == 0 ? -1 : srm_create_vehicle_resource_slot_id(info.srid);
    }
}

// 0x00B84870 (:279): empty in rfg.exe.
static void check_vehicle_variants()
{
}

// 0x00B68C00 (:323) (LTCG: pos in ECX, fvec in EDX, num_vehicles in ESI, vehicle_list in EDI). rfg.exe inlines it.
static bool add_ll_to_potentially_collidable_list(vehicle* vp, const vector& pos, const vector& fvec, int list_size, float max_dist, int& num_vehicles, nearby_vehicle_info* vehicle_list)
{
    if (vp->obj_flags._dont_use_me == 0) {
        return true;
    }
    const float fdist = fvec.y * (vp->pos.y - pos.y) + fvec.x * (vp->pos.x - pos.x) + fvec.z * (vp->pos.z - pos.z);
    if (0.0f <= fdist) {
        const float dist_sqr = vec_dist_squared(vp->pos, pos);
        if (dist_sqr <= max_dist * max_dist) {
            if (!add_collidable_vehicle_handle_to_list(vp->get_handle(), dist_sqr, fdist, list_size, num_vehicles, vehicle_list)) {
                return false;
            }
        }
    }
    return true;
}

// 0x00B84880 (:359)
static void vehicle_info_release_preloaded()
{
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        const vehicle_info& info = Vehicle_info_pool[i];
        if (info.flags.preload) {
            const unsigned int srid = stream2_container_find(info.name);
            if (srid != 0) {
                stream2_release(srid);
            }
        }
    }
}

// 0x00BB35D0 (:386)
void vehicle_system_init()
{
    if (Vehicle_system_init) {
        return;
    }
    Vehicle_system_init = true;
    Walker_footprint_decal_id = decal_info_find("civ_walker_footstep");
    Walker_jetpack_effect_info = effects_get_handle("veh_walker_thruster_a");
    Large_walker_explosion_info = explosion_lookup("large_walker_smash");
    Small_walker_explosion_info = explosion_lookup("small_walker_smash");
    vehicle_interaction_system_init();
    vehicle_info_reload_table();
    Vehicle_physical_material_index = rfg_get_physical_material_index("Steel-Normal");
    Vehicle_effect_material_index = rfg_get_effect_material_index(Vehicle_physical_material_index);
    automobile_tire_track_infos_read_table();
    Vehicle_shatter_glass_effect = effects_get_handle("veh_shatter_glass_a");
}

// 0x00B848D0 (:406)
void vehicle_system_shutdown()
{
    if (Vehicle_system_init) {
        Vehicle_system_init = false;
        vehicle_info_release_preloaded();
    }
}

// 0x00B848F0 (:419)
void vehicle_level_init_pre()
{
    Num_damaged_components_this_frame = 0;
    vehicle_ai_level_init_pre();
    vehicle_effects_level_init_pre();
}

// 0x00B68D10 (:441): empty in rfg.exe.
void vehicle_level_shutdown()
{
}

// 0x00B91E40 (:450)
void vehicle_level_reset()
{
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->m_flags.m_occupied == 0 || vp->m_flags.m_corpse != 0) {
            World.destroy_object(vp, false);
        }
    }
}

// 0x00B68D20 (:462): empty in rfg.exe.
void vehicle_create_world_vehicles()
{
}

// 0x00B84910 (:498)
void vehicle_make_occupied(unsigned int v_handle)
{
    vehicle* vp = v_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(v_handle) : NULL;
    if (vp != NULL && vp->m_flags.m_occupied == 0 && vp->m_flags.m_corpse == 0) {
        vp->m_flags.m_occupied = 1;
    }
}

// 0x00B84950 (:516)
void vehicle_make_abandoned(unsigned int v_handle)
{
    vehicle* vp = v_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(v_handle) : NULL;
    if (vp != NULL && vp->m_flags.m_occupied != 0 && vp->m_flags.m_corpse == 0) {
        vp->m_flags.m_occupied = 0;
    }
}

// 0x00B849A0 (:536)
float vehicle_type_get_value(int type_index)
{
    return Vehicle_info_pool[type_index].m_value;
}

// 0x00B91EC0 (:582)
void vehicle_free_all()
{
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        World.destroy_object(World.get_object<vehicle>(i), false);
    }
}

// 0x00B91F10 (:624)
unsigned int vehicle_get_closest_vehicle(const vector& pos, float max_dist, vehicle* last_vehicle, bool raycast_test)
{
    // with a last vehicle, the next one farther than it
    const float min_dist_sqr = last_vehicle == NULL ? -0.01f : vec_dist_squared(pos, last_vehicle->pos);
    float best_dist_sqr = max_dist < 0.0f ? FLT_MAX : max_dist * max_dist;
    vehicle* best = NULL;
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        vector local_pos = pos;
        vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
        const float dist_sqr = bbox_find_distance_squared(bmin, bmax, local_pos);
        if (min_dist_sqr < dist_sqr && dist_sqr < best_dist_sqr) {
            if (raycast_test) {
                vector target = vp->pos;
                if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
                    target.y = (bmax.y - bmin.y) * 0.5f + target.y;
                }
                else {
                    target.y = target.y + 0.1f;
                }
                if (havok_raycast(pos, target, COLLIDE_HUMAN_MINIMUM, 0, NULL, NULL)) {
                    continue;
                }
            }
            best_dist_sqr = dist_sqr;
            best = vp;
        }
    }
    return best != NULL ? best->get_handle() : 0xFFFFFFFF;
}

// 0x00B92110 (:686)
void vehicle_get_potentially_collidable_list(const vector& pos, const vector& fvec, int list_size, float max_dist, int& num_vehicles, nearby_vehicle_info* vehicle_list)
{
    num_vehicles = 0;
    const int total = World.get_num_objects<vehicle>();
    for (int i = 0; i < total; i++) {
        if (!add_ll_to_potentially_collidable_list(World.get_object<vehicle>(i), pos, fvec, list_size, max_dist, num_vehicles, vehicle_list)) {
            return;
        }
    }
}

// 0x00BBFA20 (:704)
void vehicle_process_all_presim()
{
    vehicle_navmesh_path_requests_process();
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->m_flags.m_corpse == 0) {
            vehicle_do_frame_presim(vp);
        }
    }
}

// 0x00BC2530 (:723)
void vehicle_process_all_postsim_internal(vehicle* vp, int& corpse_count)
{
    if (!vp->is_killed()) {
        vehicle_do_frame_postsim(vp);
    }
    else {
        vehicle_post_sim_update_corpse(vp);
        if (vp->m_flags.m_fading_out == 0) {
            corpse_count++;
        }
        else {
            vp->set_render_flags(vp->m_flags.m_fading_from_nano == 0 ? 2 : 8);
        }
    }
    vp->vehicle_cover_process();
}

// 0x00BC25A0 (:760)
void vehicle_process_all_postsim()
{
    vehicle_cover_process_start();
    int corpse_count = 0;
    // automobiles first
    int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->get_sub_type() != OT_SUB_VEHICLE_AUTO) {
            continue;
        }
        if (vp->m_flags.m_fading_out != 0 && vp->m_flags.m_despawn_after_fade != 0 && vp->m_flags.m_fading_from_nano == 0 && vp->is_offscreen()) {
            World.destroy_object(vp, false);
            continue;
        }
        if (vp->m_flags.m_fading_in != 0 || (vp->m_flags.m_fading_out != 0 && vp->m_flags.m_fading_from_nano == 0)) {
            vp->set_render_flags(2);
        }
        if (!vehicle_post_sim_fade_despawn(vp)) {
            vehicle_process_all_postsim_internal(vp, corpse_count);
        }
    }
    num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
            continue;
        }
        if (vp->m_flags.m_fading_in != 0 || (vp->m_flags.m_fading_out != 0 && vp->m_flags.m_fading_from_nano == 0)) {
            vp->set_render_flags(2);
        }
        if (!vehicle_post_sim_fade_despawn(vp)) {
            vehicle_process_all_postsim_internal(vp, corpse_count);
        }
    }
    vehicle_cover_clear_registrations();
    if (VEHICLE_MAX_CORPSES < corpse_count) {
        vehicle_force_corpse_removal(corpse_count);
    }
    const vehicle_component_accident_info* accident_info = NULL;
    unsigned int num_accidents = 0;
    vehicle_components_process_damage(accident_info, num_accidents);
    automobile_effects_tire_tracks_do_frame();
}

// 0x00B849C0 (:907)
const char* vehicle_get_name(int type_index)
{
    return Vehicle_info_pool[type_index].name;
}

// 0x00B849E0 (:924)
const char* vehicle_type_get_display_name(int type_index)
{
    return Vehicle_info_pool[type_index].display_name;
}

// 0x00B84A00 (:941)
unsigned int vehicle_type_get_chop_shop_props(int type_index)
{
    return Vehicle_info_pool[type_index].m_chop_shop_props;
}

// 0x00B68D30 (:970): empty in rfg.exe.
void vehicle_get_chassis_bbox_from_vehicle_type(vector* bmin, vector* bmax, int vehicle_type)
{
}

// 0x00B92290 (:987)
void vehicle_reset_mp_honked_horns()
{
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->m_flags.m_occupied != 0) {
            vp->m_flags.m_mp_honked_horn = 0;
        }
    }
}

// 0x00B84A20 (:1005)
int vehicle_get_type(const char* name)
{
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        if (v_stricmp(name, Vehicle_info_pool[i].name) == 0) {
            return i;
        }
    }
    return INVALID_VEHICLE_INFO_INDEX;
}

// 0x00B84A70 (:1022)
const vehicle_info* vehicle_get_info_from_type(int vehicle_type)
{
    return &Vehicle_info_pool[vehicle_type];
}

// 0x00B84A90 (:1031)
const vehicle_info* vehicle_get_info_from_name(const char* name)
{
    const int type = vehicle_get_type(name);
    return type >= 0 ? &Vehicle_info_pool[type] : NULL;
}

// 0x00B92300 (:1048)
bool vehicle_collect_family(farray<vehicle_info const*,163>& vehicle_family_list, const char* variant_family, bool collect_loaded_only)
{
    vehicle_family_list.clear();
    if (variant_family == INVALID_VEHICLE_FAMILY_ID) {
        return false;
    }
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        const vehicle_info* info = &Vehicle_info_pool[i];
        // the family ids come from one string pool: the pointers compare
        if (variant_family == info->variant_family && (!collect_loaded_only || srm_vehicle_info_loaded(info))) {
            vehicle_family_list.add(info);
        }
    }
    return 0 < vehicle_family_list.size();
}

// 0x00B84AC0 (:1070)
void vehicle_init_exhaust_tag(int vehicle_type, int tag_index, int tag)
{
    Vehicle_info_pool[vehicle_type].m_exhaust_tag_id[tag_index] = tag;
}

// 0x00B84AF0 (:1089)
int vehicle_get_type_by_display_name(const char* name)
{
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        if (v_stricmp(name, Vehicle_info_pool[i].display_name) == 0) {
            return i;
        }
    }
    return INVALID_VEHICLE_INFO_INDEX;
}

// 0x00B84B40 (:1111)
int vehicle_info_get_type(const vehicle_info* vinfo)
{
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        if (vinfo == &Vehicle_info_pool[i]) {
            return i;
        }
    }
    return INVALID_VEHICLE_INFO_INDEX;
}

// 0x00B84B70 (:1130)
unsigned int vehicle_get_num_object_item_spots(unsigned int vehicle_handle)
{
    // Original bug kept: a bad handle calls through null.
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : NULL;
    unsigned int count = 0;
    while (vp->get_nth_tag_index_from_name("smuggling_item", count + 1) != -1) {
        count++;
    }
    return count;
}

// 0x00B84BD0 (:1162)
unsigned int vehicle_place_object_items(unsigned int vehicle_handle, item** objects, unsigned int num_objects)
{
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : NULL;
    unsigned int placed = 0;
    int tag_index = vp->get_nth_tag_index_from_name("smuggling_item", 1);
    while (tag_index != -1 && placed < num_objects) {
        vector pos;
        matrix orient;
        vp->get_tag_world_coords(tag_index, pos, &orient, NULL, NULL);
        havok_body_set_pos_orient(((object*)objects[placed])->havok_handle, pos, orient);
        placed++;
        tag_index = vp->get_nth_tag_index_from_name("smuggling_item", placed + 1);
    }
    return placed;
}

// 0x00B68D40 (:1200)
bool vehicle_raycast_callback(object* hit_objp, void* vp)
{
    if (hit_objp == NULL) {
        return true;
    }
    if (hit_objp->get_obj_type() == OT_HUMAN || character_controller_should_push_away(hit_objp, false)) {
        return false;
    }
    while (hit_objp != NULL) {
        if (hit_objp == vp) {
            return false;
        }
        hit_objp = World.object_from_handle(hit_objp->get_parent());
    }
    return true;
}

// 0x00B68D90 (:1225)
bool vehicle_is_tank(const vehicle* vp)
{
    return vp->info->vehicle_classification == BVT_TANK || vp->info->vehicle_classification == BVT_ART_TANK;
}

// 0x00B68DB0 (:1231)
bool vehicle_is_bomber(const vehicle* vp)
{
    return vp->info->vehicle_classification == BVT_BOMBER;
}

// 0x00B68DD0 (:1238)
bool vehicle_is_ambulance(const vehicle* vp)
{
    return vp->info->vehicle_classification == BVT_AMBULANCE;
}

// 0x00B68DF0 (:1244)
bool vehicle_is_minelayer(const vehicle* vp)
{
    for (int i = 0; i < vp->info->num_turret_mounts; i++) {
        const turret_info* turret = vp->info->turret_mounts[i].attached_turret_info;
        if (turret != NULL && turret->w_info != NULL && turret->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE) {
            return true;
        }
    }
    return false;
}

// The display name lookup, then the name lookup when it gave no candidates.
static const vehicle_info* vehicle_lookup_by_either_name(const char* name)
{
    char matches[50];
    const vehicle_info* info = vehicle_lookup_by_display_name_partial_match(name, matches, 0x32);
    if (info == NULL && matches[0] == '\0') {
        info = vehicle_lookup_by_name_partial_match(name, matches, 0x32);
    }
    return info;
}

// 0x00B990D0 (:1257)
bool vehicle_is_bus(const vehicle* vp)
{
    static const char* Bus_family_id = INVALID_VEHICLE_FAMILY_ID; // 0x03870C80
    if (Bus_family_id == INVALID_VEHICLE_FAMILY_ID) {
        const vehicle_info* info = vehicle_lookup_by_either_name("CaravanBus");
        if (info == NULL) {
            return false;
        }
        Bus_family_id = info->variant_family;
    }
    return vp->info->variant_family == Bus_family_id;
}

// 0x00B99190 (:1272)
bool vehicle_is_apc(const vehicle* vp)
{
    static const char* APC_family_id = INVALID_VEHICLE_FAMILY_ID; // 0x03870C88
    if (APC_family_id == INVALID_VEHICLE_FAMILY_ID) {
        const vehicle_info* info = vehicle_lookup_by_either_name("APC");
        if (info == NULL) {
            return false;
        }
        APC_family_id = info->variant_family;
    }
    return vp->info->variant_family == APC_family_id;
}

// 0x00B99250 (:1287)
bool vehicle_is_min_jetter(const vehicle* vp)
{
    static const vehicle_info* min_jetter_info = NULL; // 0x03870C90
    if (min_jetter_info == NULL) {
        min_jetter_info = vehicle_lookup_by_either_name("min_jetter");
        if (min_jetter_info == NULL) {
            return false;
        }
    }
    return vp->info == min_jetter_info;
}

// 0x00B992E0 (:1301)
bool vehicle_is_mar_raider(const vehicle* vp)
{
    static const vehicle_info* min_raider_info = NULL; // 0x03870C94
    if (min_raider_info == NULL) {
        min_raider_info = vehicle_lookup_by_either_name("raider");
        if (min_raider_info == NULL) {
            return false;
        }
    }
    return vp->info == min_raider_info;
}

// 0x00B68E40 (:1317)
const farray<vehicle_info,163>& vehicle_info_get_master_list()
{
    return Vehicle_info_pool;
}

// 0x00B68E50 (:1322)
void vehicle_set_avoid_despawning_vehicles(bool avoid)
{
    if (avoid) {
        if (!mission_is_active()) {
            amb_set_ambient_vehicle_spawn_disabled(true);
            Vehicle_system_avoid_despawn = true;
        }
    }
    else {
        amb_set_ambient_vehicle_spawn_disabled(false);
        Vehicle_system_avoid_despawn = false;
    }
}

// 0x00B68E90 (:1339)
bool vehicle_should_avoid_despawning(vehicle* vp)
{
    return mission_is_active() && Vehicle_system_avoid_despawn && vp != NULL && object_is_local_player(vp->most_recent_driver);
}

// 0x00BB0190 (:1346)
void vehicle_info_reload_table()
{
    const int common_mark = Common_mempool.mark();
    // Live vehicles keep a copy of their display name in the info pointer while the table is rebuilt.
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        char* name_copy = (char*)Common_mempool.alloc(0x21, 0);
        strcpy(name_copy, vp->info->display_name);
        vp->info = (const vehicle_info*)name_copy;
    }
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        if (Vehicle_info_pool[i].rig_name != NULL) {
            animlib_unregister_entity_base_bone_set(Vehicle_info_pool[i].bones_used_info.unique_id);
        }
    }

    if (Vehicle_info_string_pool_data == NULL) {
        Vehicle_info_string_pool_data = (char*)Permanent_mempool.alloc(0x2000, 0);
        if (Vehicle_info_string_pool_data == NULL) {
            keen::debug::nativePrintArgs("Unable to allocate memory for the vehicle string pool");
        }
        Vehicle_info_string_pool.init(Vehicle_info_string_pool_data, 0x2000, "Vehicle info strings", 0, '\0');
    }
    build_vehicle_containers_shared(&Common_mempool, &Permanent_mempool, PLATFORM_PC, false, true, -1);
    vehicle_cover_info_free_all();
    vehicle_parse_cover_table(Vehicle_cover_info_pool, -1);
    if (parse_vehicle_defaults() && parse_vehicle_groups() && parse_single_vehicle_info(Vehicle_info_pool, -1)) {
        find_variant_cover_info(Vehicle_info_pool);
        parse_wheel_effect_table();
    }
    const int dlc_id = dlc_get_current_id();
    if (dlc_id != -1) {
        vehicle_parse_table(Vehicle_info_pool, &Level_mempool, dlc_id);
    }
    build_resource_information();

    for (int i = 0; i < num_vehicles; i++) {
        vehicle* vp = World.get_object<vehicle>(i);
        // Original bug kept: a display name no longer in the table gives entry -1.
        vp->info = &Vehicle_info_pool[vehicle_get_type_by_display_name((const char*)vp->info)];
    }
    Common_mempool.restore_to_mark(common_mark);
}

// 0x00B92380 (:1389)
void vehicle_get_all_info(farray<vehicle_info const*,163>& all_vehicle_info)
{
    all_vehicle_info.clear();
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        const vehicle_info* info = &Vehicle_info_pool[i];
        all_vehicle_info.add(info);
    }
}

// 0x00B923F0 (:1400)
const vehicle_info* vehicle_lookup_by_name_partial_match(const char* name, char* potential_match_names, int potential_match_names_array_size)
{
    farray<vehicle_info const*,163> matches;
    potential_match_names[0] = '\0';
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        const vehicle_info* info = &Vehicle_info_pool[i];
        if (v_stricmp(name, info->name) == 0) {
            return info;
        }
        // Original bug kept: the lowered copy of the name is made and never used.
        char lower_name[256];
        strncpy_safe(lower_name, name, 0x100);
        str_make_lower(lower_name);
        char lower_candidate[256];
        strncpy_safe(lower_candidate, info->name, 0x100);
        str_make_lower(lower_candidate);
        if (strstr(lower_candidate, name) != NULL) {
            matches.add(info);
        }
    }
    if (matches.size() == 1) {
        return matches[0];
    }
    if (1 < matches.size()) {
        for (int i = 0; i < matches.size(); i++) {
            if (matches[i] != NULL) {
                _snprintf_safe(potential_match_names, potential_match_names_array_size, "%s\n%s", potential_match_names, matches[i]->name);
            }
        }
    }
    return NULL;
}

// 0x00B92580 (:1450)
const vehicle_info* vehicle_lookup_by_display_name_partial_match(const char* name, char* potential_match_names, int potential_match_names_array_size)
{
    farray<vehicle_info const*,163> matches;
    potential_match_names[0] = '\0';
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        const vehicle_info* info = &Vehicle_info_pool[i];
        if (info->variant_family != NULL && v_stricmp(name, info->variant_family) == 0) {
            if (info->variant_family == INVALID_VEHICLE_FAMILY_ID) {
                return info;
            }
            farray<vehicle_info const*,163> family;
            return vehicle_collect_family(family, info->variant_family, true) ? family[0] : info;
        }
        if (v_stricmp(name, info->display_name) == 0) {
            return info;
        }
        // Original bug kept: the searched name is not lowered, only the candidate.
        const char* found;
        if (info->variant_family == NULL) {
            char lower_display[256];
            strncpy_safe(lower_display, info->display_name, 0x100);
            str_make_lower(lower_display);
            found = strstr(lower_display, name);
        }
        else {
            char lower_family[256];
            strncpy_safe(lower_family, info->variant_family, 0x100);
            str_make_lower(lower_family);
            found = strstr(lower_family, name);
        }
        if (found != NULL) {
            matches.add(info);
        }
    }
    if (matches.size() == 0) {
        return NULL;
    }
    farray<vehicle_info const*,163> family;
    if (matches.size() == 1) {
        if (matches[0]->variant_family != INVALID_VEHICLE_FAMILY_ID && vehicle_collect_family(family, matches[0]->variant_family, true)) {
            return family[0];
        }
        return matches[0];
    }
    for (int i = 0; i < matches.size(); i++) {
        _snprintf_safe(potential_match_names, potential_match_names_array_size, "%s\n%s", potential_match_names, matches[i]->display_name);
        if (family.size() == 0 && matches[i]->variant_family != INVALID_VEHICLE_FAMILY_ID) {
            vehicle_collect_family(family, matches[i]->variant_family, true);
        }
    }
    return 0 < family.size() ? family[0] : matches[0];
}

// 0x00B928A0 (:1537)
const vehicle_info* vehicle_find_best_match_vehicle(const char* name, char* potential_match_names, int potential_match_names_array_size)
{
    if (name == NULL) {
        return NULL;
    }
    const vehicle_info* info = vehicle_lookup_by_display_name_partial_match(name, potential_match_names, potential_match_names_array_size);
    if (info == NULL && potential_match_names[0] == '\0') {
        info = vehicle_lookup_by_name_partial_match(name, potential_match_names, potential_match_names_array_size);
    }
    return info;
}

// 0x00B84C70 (:1555): retail stripped the line's formatting: it prints the thread's empty scratch text once per type.
void vehicle_display_vehicle_types()
{
    static char scratch[1] = {'\0'};
    for (int i = 0; i < Vehicle_info_pool.size(); i++) {
        console_output(scratch, NULL, false);
    }
}
