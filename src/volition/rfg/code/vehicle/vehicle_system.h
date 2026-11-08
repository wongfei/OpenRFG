// Original: volition/rfg/code/vehicle/vehicle_system.h
// Implementation: volition/rfg/code/vehicle/vehicle_system.cpp
//
// The vehicle type table and the per-frame passes (decompiled). vehicle_process_all_presim processes the nav mesh path
// requests, then vehicle_do_frame_presim for every vehicle that is not a corpse. vehicle_process_all_postsim first
// handles the automobiles (subtype OT_SUB_VEHICLE_AUTO), then the others: fading vehicles are drawn with render flags
// and despawned when faded, dead ones update their corpse (more than 6 corpses forces removal), live ones run
// vehicle_do_frame_postsim; then cover, component damage and tire tracks.
#pragma once

#include "volition/vlib/code/util/varray.h"

class item;
struct nearby_vehicle_info;
class object;
class vector;
class vehicle;
class vehicle_info;

// size 0xC
struct nearby_vehicle_info
{
    unsigned int handle; // 0x00
    float dist_sqr; // 0x04
    float fdist; // 0x08
};

void build_resource_information(); // 0x00B84820 (:262)
void vehicle_system_init(); // 0x00BB35D0 (:386)
void vehicle_system_shutdown(); // 0x00B848D0 (:406)
void vehicle_level_init_pre(); // 0x00B848F0 (:419)
void vehicle_level_shutdown(); // 0x00B68D10 (:441)
void vehicle_level_reset(); // 0x00B91E40 (:450)
void vehicle_create_world_vehicles(); // 0x00B68D20 (:462)
void vehicle_make_occupied(unsigned int v_handle); // 0x00B84910 (:498)
void vehicle_make_abandoned(unsigned int v_handle); // 0x00B84950 (:516)
float vehicle_type_get_value(int type_index); // 0x00B849A0 (:536)
void vehicle_free_all(); // 0x00B91EC0 (:582)
unsigned int vehicle_get_closest_vehicle(const vector& pos, float max_dist, vehicle* last_vehicle, bool raycast_test); // 0x00B91F10 (:624)
void vehicle_get_potentially_collidable_list(const vector& pos, const vector& fvec, int list_size, float max_dist, int& num_vehicles, nearby_vehicle_info* vehicle_list); // 0x00B92110 (:686)
void vehicle_process_all_presim(); // 0x00BBFA20 (:704)
void vehicle_process_all_postsim_internal(vehicle* vp, int& corpse_count); // 0x00BC2530 (:723)
void vehicle_process_all_postsim(); // 0x00BC25A0 (:760)
const char* vehicle_get_name(int type_index); // 0x00B849C0 (:907)
const char* vehicle_type_get_display_name(int type_index); // 0x00B849E0 (:924)
unsigned int vehicle_type_get_chop_shop_props(int type_index); // 0x00B84A00 (:941)
void vehicle_get_chassis_bbox_from_vehicle_type(vector* bmin, vector* bmax, int vehicle_type); // 0x00B68D30 (:970)
void vehicle_reset_mp_honked_horns(); // 0x00B92290 (:987)
int vehicle_get_type(const char* name); // 0x00B84A20 (:1005)
const vehicle_info* vehicle_get_info_from_type(int vehicle_type); // 0x00B84A70 (:1022)
const vehicle_info* vehicle_get_info_from_name(const char* name); // 0x00B84A90 (:1031)
bool vehicle_collect_family(farray<vehicle_info const*,163>& vehicle_family_list, const char* variant_family, bool collect_loaded_only); // 0x00B92300 (:1048)
void vehicle_init_exhaust_tag(int vehicle_type, int tag_index, int tag); // 0x00B84AC0 (:1070)
int vehicle_get_type_by_display_name(const char* name); // 0x00B84AF0 (:1089)
int vehicle_info_get_type(const vehicle_info* vinfo); // 0x00B84B40 (:1111)
unsigned int vehicle_get_num_object_item_spots(unsigned int vehicle_handle); // 0x00B84B70 (:1130)
unsigned int vehicle_place_object_items(unsigned int vehicle_handle, item** objects, unsigned int num_objects); // 0x00B84BD0 (:1162)
bool vehicle_raycast_callback(object* hit_objp, void* vp); // 0x00B68D40 (:1200)
bool vehicle_is_tank(const vehicle* vp); // 0x00B68D90 (:1225)
bool vehicle_is_bomber(const vehicle* vp); // 0x00B68DB0 (:1231)
bool vehicle_is_ambulance(const vehicle* vp); // 0x00B68DD0 (:1238)
bool vehicle_is_minelayer(const vehicle* vp); // 0x00B68DF0 (:1244)
bool vehicle_is_bus(const vehicle* vp); // 0x00B990D0 (:1257)
bool vehicle_is_apc(const vehicle* vp); // 0x00B99190 (:1272)
bool vehicle_is_min_jetter(const vehicle* vp); // 0x00B99250 (:1287)
bool vehicle_is_mar_raider(const vehicle* vp); // 0x00B992E0 (:1301)
const farray<vehicle_info,163>& vehicle_info_get_master_list(); // 0x00B68E40 (:1317)
void vehicle_set_avoid_despawning_vehicles(bool avoid); // 0x00B68E50 (:1322)
bool vehicle_should_avoid_despawning(vehicle* vp); // 0x00B68E90 (:1339)
void vehicle_info_reload_table(); // 0x00BB0190 (:1346)
void vehicle_get_all_info(farray<vehicle_info const*,163>& all_vehicle_info); // 0x00B92380 (:1389)
const vehicle_info* vehicle_lookup_by_name_partial_match(const char* name, char* potential_match_names, int potential_match_names_array_size); // 0x00B923F0 (:1400)
const vehicle_info* vehicle_lookup_by_display_name_partial_match(const char* name, char* potential_match_names, int potential_match_names_array_size); // 0x00B92580 (:1450)
const vehicle_info* vehicle_find_best_match_vehicle(const char* name, char* potential_match_names, int potential_match_names_array_size); // 0x00B928A0 (:1537)
void vehicle_display_vehicle_types(); // 0x00B84C70 (:1555)
