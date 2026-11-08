// Original: volition/rfg/code/spawn/spawn_resource_manager.h
// Implementation: volition/rfg/code/spawn/spawn_resource_manager.cpp
//
// The spawn resource manager (srm): human and vehicle resource slots (one streamed info each) that the spawn users
// (activities, squads, reinforcements, ambient spawning, the safehouse VIP) reserve, register their objects in and
// release; object_spawn_do_spawn and object_spawn_vehicle spawn through it. gameplay_do_frame calls srm_do_frame.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/vlib/code/util/varray.h"

class human_info;
class human_spawn_group_info;
class npc;
class npc_spawn_param_list;
class spawn_resource_data;
class vehicle;
class vehicle_info;
class vehicle_spawn_group_info;
class vehicle_spawn_params;

enum resource_slot_user_id
{
    RESOURCE_SLOT_USER_ID_NONE = 0,
    END_OF_ACTIVITY_SLOT_USERS = 16,
    RESOURCE_SLOT_USER_ID_SQUAD = 17,
    RESOURCE_SLOT_USER_ID_REINFORCEMENT = 18,
    RESOURCE_SLOT_USER_ID_SPECIAL = 19,
    RESOURCE_SLOT_USER_ID_AMBIENT = 20,
    RESOURCE_SLOT_USER_ID_SAFEHOUSE_VIP = 21,
    NUM_RESOURCE_SLOT_USER_IDS = 22,
};

enum spawn_status_result
{
    INVALID_SPAWN_STATUS_RESULT = -1,
    SPAWN_STATUS_SUCCESS = 0,
    SPAWN_STATUS_STREAMING = 1,
    SPAWN_STATUS_FAILURE = 2,
    NUM_SPAWN_STATUS = 3,
};

extern farray<human_info const*, 16> Desired_guerrilla_list; // 0x016B7980
extern farray<vehicle_info const*, 128> Desired_vehicle_list; // 0x016B7C78
extern bool Mission_mode; // 0x0374246E

int get_spawn_slot_type_bit_flag(spawn_slot_types slot_type); // 0x00B43E40 (:233)
void srm_do_frame_special_slot(); // 0x00B5D360 (:1401)
spawn_status_result object_spawn_do_spawn(npc_spawn_param_list& spawn_param_list); // 0x00B5D610 (:3306)
spawn_status_result object_spawn_vehicle(vehicle_spawn_params& spawn_param); // 0x00B57F40 (:3363)
void srm_do_frame(); // 0x00B5D6F0 (:3425)
void srm_system_init(); // 0x00B5D770 (:3483)
int srm_find_human_resource_slot_id(unsigned int srid); // 0x00B493B0 (:3494)
int srm_find_vehicle_resource_slot_id(unsigned int srid); // 0x00B493F0 (:3508)
int srm_create_human_resource_slot_id(unsigned int srid); // 0x00B52900 (:3521)
int srm_create_vehicle_resource_slot_id(unsigned int srid); // 0x00B529A0 (:3551)
void srm_level_init_pre(); // 0x00B52A40 (:3581)
void srm_level_shutdown(); // 0x00B52AE0 (:3606)
bool srm_slots_streamed(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B49430 (:3759)
bool srm_slot_is_reserved(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info); // 0x00B58030 (:3913)
bool srm_slot_is_reserved(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info); // 0x00B58050 (:3919)
bool srm_slot_is_reserved(spawn_slot_types slot_type, const vehicle_info* veh_info); // 0x00B52B00 (:3925)
bool srm_swap_player_slot(const vehicle_info* v_info); // 0x00B5FD30 (:3940)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_spawn_group_info* hum_group); // 0x00B5D790 (:3959)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const base_array<vehicle_info const*>& veh_info_list); // 0x00B5FE10 (:3972)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_spawn_group_info* veh_group); // 0x00B5FEA0 (:3991)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info); // 0x00B5D800 (:4001)
bool srm_reserve_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info); // 0x00B5D870 (:4014)
bool srm_can_release_vehicle_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B52B50 (:4026)
bool srm_can_release_human_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B44180 (:4065)
bool srm_no_owner(spawn_slot_types slot_type); // 0x00B494B0 (:4075)
resource_slot_user_id srm_get_owner(spawn_slot_types slot_type); // 0x00B441A0 (:4082)
int srm_get_availible_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B52BF0 (:4091)
int srm_get_used_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B52C70 (:4111)
void srm_free_unused_reserved_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B58070 (:4127)
void srm_release_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const human_info* hum_info); // 0x00B5D8E0 (:4180)
void srm_release_slot(spawn_slot_types slot_type, resource_slot_user_id owner_id, const vehicle_info* veh_info); // 0x00B5D900 (:4186)
void srm_release_all_slots(spawn_slot_types slot_type, resource_slot_user_id owner_id); // 0x00B580F0 (:4192)
bool srm_all_reserved_slots_streamed(); // 0x00B58180 (:4227)
void srm_level_init_post(); // 0x00B5D920 (:4243)
void srm_register_npc(const npc* np); // 0x00B441E0 (:4276)
void srm_register_vehicle(const vehicle* vp); // 0x00B441F0 (:4292)
int srm_free_low_priority_slots_human(); // 0x00B49500 (:4316)
int srm_free_low_priority_slots_vehicle(); // 0x00B49510 (:4323)
void srm_remove_npc(const npc* np); // 0x00B44210 (:4330)
void srm_remove_vehicle(const vehicle* vp); // 0x00B44220 (:4335)
void srm_allocate_ambient_spawn_slots(); // 0x00B5D9E0 (:4341)
void srm_allocate_ambient_spawn_slots_human(); // 0x00B5DA00 (:4350)
void srm_allocate_ambient_spawn_slots_vehicle(); // 0x00B58210 (:4358)
const farray<human_info const*,165>& srm_loaded_human_info_list(); // 0x00B58220 (:4369)
const farray<vehicle_info const*,163>& srm_loaded_vehicle_info_list(); // 0x00B58300 (:4393)
const human_info* srm_pick_loaded_human_info(const base_array<human_info const*>& human_info_list); // 0x00B583E0 (:4417)
const human_info* srm_pick_loaded_human_info(const human_spawn_group_info* h_spawn_group); // 0x00B58500 (:4447)
const vehicle_info* srm_pick_loaded_vehicle_info(const base_array<vehicle_info const*>& vehicle_info_list); // 0x00B58510 (:4452)
const vehicle_info* srm_pick_loaded_vehicle_info(const vehicle_spawn_group_info* v_spawn_group); // 0x00B58630 (:4481)
void srm_reserve_vehicle_spawn_group_mp(const vehicle_spawn_group_info* v_spawn_group); // 0x00B58640 (:4487)
bool srm_human_info_loaded(const human_info* h_info); // 0x00B58680 (:4500)
bool srm_human_info_reserved(const human_info* h_info); // 0x00B52CD0 (:4506)
bool srm_vehicle_info_loaded(const vehicle_info* v_info); // 0x00B58690 (:4518)
bool srm_vehicle_info_reserved(const vehicle_info* v_info); // 0x00B52D00 (:4523)
bool srm_human_group_reserved(const human_spawn_group_info* h_group); // 0x00B52D30 (:4535)
bool srm_vehicle_group_reserved(const vehicle_spawn_group_info* v_group); // 0x00B52D90 (:4549)
bool allcoate_free_slot(const vehicle_info* v_info); // 0x00B586A0 (:4564)
bool allcoate_free_slot(const human_info* h_info); // 0x00B58700 (:4587)
npc* srm_spawn_single_npc_vehicle(const human_info* h_info, vehicle* vp, vehicle_seat_index vsi); // 0x00B5DA10 (:4608)
void srm_reserved_human_info_list(farray<human_info const*,165>& info_list, human_teams team, bool high_priority_only); // 0x00B52DF0 (:4635)
void srm_mission_reservation(const spawn_resource_data* srm_data); // 0x00B5E410 (:5232)
void srm_mission_release(); // 0x00B5E470 (:5258)
unsigned int srm_human_resource_dlc_mark(); // 0x00B49550 (:5278)
unsigned int srm_vehicle_resource_dlc_mark(); // 0x00B49560 (:5283)
void srm_human_resource_dlc_restore_to_mark(unsigned int mark); // 0x00B49570 (:5289)
void srm_vehicle_resource_dlc_restore_to_mark(unsigned int mark); // 0x00B49650 (:5298)
