// Original: volition/rfg/code/objects/humans/player.h
// Implementation: volition/rfg/code/objects/humans/player.cpp
//
// player, the human controlled by a local or remote player (subtype of OT_HUMAN): controls and the command history
// (c_command), cover, ladders, usable objects (t_usable_object: the best object to use and its action callback),
// script modes, zoom, the meta data (progress), the decals it placed and the multiplayer state history (mp_sim_state).
// player_process_all (decompiled) calls player_do_frame for every player whose character instance is loaded.
// player_get_local_player returns the local player. Placement of the small player types inferred.
#pragma once

#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include <stdint.h>

struct cfile;
class flyer;
struct inventory_item;
class item;
class ladder;
struct local_player_info;
class material_effect_parameters;
class multi_object_backpack;
class npc;
class obj_prop_block;
class object;
class object_action_node;
class object_squad;
class player;
struct squad_definition;
enum upgrade_definition_names;
class vehicle;

// Placement inferred (rfg.pdb records the enum, not its header).
enum weapon_lock_on_status
{
    WLOS_NONE = 0,
    WLOS_LOCKING = 1,
    WLOS_LOCKED = 2,
    WLOS_LAST = 3,
};

// size 0x8
struct player_flags
{
    unsigned int action_node_anim_started : 1; // 0x00:0
    unsigned int custom_mesh : 1; // 0x00:1
    unsigned int is_loaded : 1; // 0x00:2
    unsigned int script_controlled : 1; // 0x00:3
    unsigned int never_die : 1; // 0x00:4
    unsigned int mission_never_die : 1; // 0x00:5
    unsigned int do_sprint : 1; // 0x00:6
    unsigned int disable_controls : 1; // 0x00:7
    unsigned int jetpack_started : 1; // 0x00:8
    unsigned int jetpack_ready : 1; // 0x00:9
    unsigned int moving_to_a_stop : 1; // 0x00:10
    unsigned int quick_turn : 1; // 0x00:11
    unsigned int crouching_state : 1; // 0x00:12
    unsigned int weapon_hidden : 1; // 0x00:13
    unsigned int is_stuck : 1; // 0x00:14
    unsigned int cover_firing : 1; // 0x00:15
    unsigned int cover_leanup : 1; // 0x00:16
    unsigned int use_cover : 1; // 0x00:17
    unsigned int no_reticule : 1; // 0x00:18
    unsigned int no_fineaim : 1; // 0x00:19
    unsigned int cover_search_for_edge : 1; // 0x00:20
    unsigned int cover_against_vehicle : 1; // 0x00:21
    unsigned int in_comm_tower_range : 1; // 0x00:22
    unsigned int comm_range_checked : 1; // 0x00:23
    unsigned int passenger_brake : 1; // 0x00:24
    unsigned int local_player_hit_someone_this_frame : 1; // 0x00:25
    unsigned int weapon_swap_lock : 1; // 0x00:26
    unsigned int unlimited_ammo : 1; // 0x00:27
    unsigned int in_radiation : 1; // 0x00:28
    unsigned int ragdoll_on_next_explosion : 1; // 0x00:29
    unsigned int leaving_squad_member_behind : 1; // 0x00:30
    unsigned int is_waiting_for_hostage_vehicle_exit : 1; // 0x00:31
    unsigned int backpack_fade_out : 1; // 0x04:0
    unsigned int backpack_fade_in : 1; // 0x04:1
    unsigned int mp_display_respawn_msg : 1; // 0x04:2
    unsigned int mp_remote_player_fired_this_frame : 1; // 0x04:3
    unsigned int mp_ignore_player_for_respawn_collisions : 1; // 0x04:4
    unsigned int mp_lost_hp : 1; // 0x04:5
    unsigned int mp_hit_someone_this_network_frame : 1; // 0x04:6
    unsigned int mp_remote_player_charged_this_frame : 1; // 0x04:7
    unsigned int mp_local_player_can_see_me : 1; // 0x04:8
    unsigned int mp_has_reloaded : 1; // 0x04:9
    unsigned int mp_removing_player : 1; // 0x04:10
    unsigned int waiting_for_locker_exit : 1; // 0x04:11
};

// size 0xC (multiplayer.md MP10: the kaboom's camera shake falls off from the local player)
struct local_player_info
{
    human_teams team; // 0x00
    vector* pos; // 0x04
    matrix* orient; // 0x08
};

// size 0xC
struct player_ladder_state
{
    player_ladder_state(); // 0x00A104F0

    union
    {
        struct
        {
            unsigned char l_state; // 0x00
            unsigned char l_rung; // 0x01
        };
        unsigned short data; // 0x00
    };
    timestamp remote_ladder_mount_delay; // 0x04
    timestamp local_ladder_mount_delay; // 0x08
};

// size 0x10
struct player_positional_record
{
    player_positional_record(); // 0x00A10510

    vector pos; // 0x00
    int period_ms; // 0x0C
};

// size 0x8
struct vehicle_enter_struct
{
    unsigned int vehicle_handle; // 0x00
    bool run; // 0x04
};

// size 0x8
union script_specific_data
{
    vehicle_enter_struct vehicle_enter_data; // 0x00
};

enum general_action_types
{
    GENERAL_ACTION_TYPE_INVALID = -1,
    GENERAL_ACTION_MP_USE_DISARM = 0,
    GENERAL_ACTION_MP_SWAP_BACKPACK = 1,
    GENERAL_ACTION_MP_MODE_SPECIFIC = 2,
    GENERAL_ACTION_DIALOGUE = 3,
    GENERAL_ACTION_USE_TURRET = 4,
    GENERAL_ACTION_USE_EQUIPPED_ITEM = 5,
    GENERAL_ACTION_USE_NEAREST_OBJECT = 6,
    GENERAL_ACTION_USE_VEHICLE = 7,
    GENERAL_ACTION_USE_LADDER = 8,
    GENERAL_ACTION_USE_AMMO_BOX = 9,
    GENERAL_ACTION_RELEASE_HOSTAGE = 10,
    GENERAL_ACTION_USE_BASE = 11,
    GENERAL_ACTION_START_MISSION = 12,
    GENERAL_ACTION_START_DEMOLITIONS_MASTER = 13,
    GENERAL_ACTION_START_RIDING_SHOTGUN = 14,
    GENERAL_ACTION_START_HOUSE_ARREST = 15,
    GENERAL_ACTION_START_OFFENSIVE_RAID = 16,
    GENERAL_ACTION_UPGRADE = 17,
    GENERAL_ACTION_START_AREA_DEFENSE = 18,
    NUM_GENERAL_ACTION_TYPES = 19,
};

enum use_object_icon
{
    USE_OBJECT_ICON_INVALID = -1,
    USE_OBJECT_ICON_SWAP = 0,
    USE_OBJECT_ICON_DEFAULT = 1,
    USE_OBJECT_ICON_HYDROGEN_TANK = 2,
    USE_OBJECT_ICON_LADDER = 3,
    USE_OBJECT_ICON_VEHICLE = 4,
    USE_OBJECT_ICON_MISSION = 5,
    USE_OBJECT_ICON_DEMOLITION_GUERRILLA = 6,
    USE_OBJECT_ICON_DEMOLITION_EDF = 7,
    USE_OBJECT_ICON_BAGMAN_GUERRILLA = 8,
    USE_OBJECT_ICON_BAGMAN_EDF = 9,
    USE_OBJECT_ICON_UPGRADE = 10,
    NUM_PAOI_TYPES = 11,
};

// size 0x18
struct t_usable_object
{
    void reset(); // 0x006F6990 (.h:167)

    unsigned int handle; // 0x00
    general_action_types action_type; // 0x04
    use_object_icon action_icon; // 0x08
    float dot_prod; // 0x0C
    float dist_squared; // 0x10
    bool (*action_callback)(player*, bool); // 0x14
};

enum player_script_mode
{
    PSM_NONE = 0,
    PSM_VEHICLE_ENTER = 1,
    NUM_PLAYER_SCRIPT_MODES = 2,
};

enum player_zoom_state
{
    PZS_OFF = 0,
    PZS_SPRINT = 1,
    PZS_FINE_AIM = 2,
    PZS_SCOPE = 3,
};

enum player_cover_test
{
    PCT_CHECK_COVER_NORMAL = 0,
    PCT_THREE_FRAME_DELAY = 1,
    PCT_TWO_FRAME_DELAY = 2,
    PCT_ONE_FRAME_DELAY = 3,
    PCT_CROUCH = 4,
    PCT_VERIFY_COVER = 5,
    PCT_DONE = 6,
};

enum tagging_sequence
{
    TAGGING_SEQUENCE_NONE = -1,
    TAGGING_SEQUENCE_START = 0,
    TAGGING_SEQUENCE_TAG = 1,
    TAGGING_SEQUENCE_DONE = 2,
    NUM_TAGGING_SEQUENCES = 3,
};

// size 0x10
struct custom_vehicle
{
    // size 0x4
    struct flags
    {
        unsigned int is_totaled : 1; // 0x00:0
        unsigned int is_in_garage : 1; // 0x00:1
    };

    custom_vehicle* prev; // 0x00
    custom_vehicle* next; // 0x04
    int m_type_index; // 0x08
    flags m_flags; // 0x0C
};

// size 0x4
struct multi_player_blocked_info
{
    int raycast_blocked_count; // 0x00
};

// size 0x8
struct multi_player_edge_correction_info
{
    int increasing_velocity_count; // 0x00
    float vertical_velocity; // 0x04
};

// size 0x2E50
class player : public human
{
public:
    player(); // 0x00ADECA0 (:2735)
    static const object_type get_type_static() { return OT_HUMAN; } // 0x005DC200 (.h:329)
    static const object_sub_type get_subtype_static() { return OT_SUB_HUMAN_PLAYER; } // 0x005DC210 (.h:329)
    static const char* get_classname_static(); // 0x00A057A0 (.h:329)
    static int get_bpo_index_static(); // 0x00943020 (.h:329)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00ADEE80 (.h:329)
    static unsigned int get_classname_hash(); // 0x00A104D0 (:2734)
    static int pool_remaining(); // 0x00A33DB0 (:2734)
    virtual void pool_free(); // vtbl+0x80 0x00A842E0 (:2734)
    bool mp_should_render_on_minimap() const; // 0x00A34A90 (:9065)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00ADEEF0 (:2739)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A10520 (:3050)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AC2070 (:3076)
protected:
    virtual void free_this(); // vtbl+0x28 0x00ACD440 (:3137)
public:
    void open_ammo_box(item* abp); // 0x00A846E0 (:4000)
    void hide_weapon(); // 0x00A84420 (:3260)
    void unhide_weapon(); // 0x00AC21B0 (:3286)
    bool in_comm_tower_range() const; // 0x00A107F0 (:4046)
    void do_frame(); // 0x00AD5A80 (:3865)
    void do_frame_comm_tower(); // 0x00A84610 (:3929)
    void do_frame_track_pos(); // 0x00A106E0 (:3734)
    void do_frame_open_ammo_box(); // 0x00ACD590 (:3487)
    void do_frame_action_node(); // 0x00AC2300 (:3596)
    void do_frame_path_timeout(); // 0x00A10590 (:3336)
    void do_frame_radiation(); // 0x00AD59E0 (:3362)
    void do_frame_blood_decals(); // 0x00A5C7D0 (:3381)
    void remove_blood_decals(); // 0x00A5C920 (:3405)
    void do_frame_blow_undercover(); // 0x00A5C980 (:3414)
    void update_backpack_fade(); // 0x00A84490 (:3452)
    void ammo_box_reset(); // 0x00ACD560 (:3252)
    void action_node_reset(object_action_node* anp); // 0x00AC2230 (:3313)
    const player_positional_record& get_positional_record(int index) const; // 0x00A10760 (:3763)
    bool in_range_of_tower(const object* objp) const; // 0x00A107C0 (:3992)
    void ally_squad_add(npc* np); // 0x00A84910 (:4072)
    void ally_squad_remove(npc* np); // 0x00A84A50 (:4123)
    void ally_squad_fall_back(); // 0x00A5CB80 (:4151)
    void ally_squad_assign_orders(); // 0x00A84B30 (:4194)
    void ally_squad_call_specialist(); // 0x00A10800 (:4296)
    void ally_squad_report_member_death(const npc* np); // 0x00A5CC00 (:4301)
    void ally_squad_reassign_icons(const object_squad* squad) const; // 0x00A33FC0 (:4327)
    bool ally_squad_can_add() const; // 0x00A33F40 (:4055)
    void escort_squad_add(npc* np); // 0x00A84EE0 (:4405)
    void escort_squad_remove(npc* np, bool activity_success); // 0x00A85010 (:4469)
    void escort_squad_report_member_death(const npc* np); // 0x00A5CC90 (:4495)
    void escort_squad_reassign_icons(const object_squad* squad) const; // 0x00A34040 (:4506)
    bool escort_squad_can_add() const; // 0x00A34000 (:4336)
    void disband_ally_and_escort_squads(); // 0x00A9DDC0 (:4355)
    void check_squads(); // 0x00A9DFA0 (:4518)
    void request_all_followers_replan() const; // 0x00A5CCE0 (:4560)
    void save_inventory(cfile* save_file); // 0x00A34AD0 (:9114)
    bool load_inventory(cfile* save_file, bool demolitions_master_override); // 0x00AC3370 (:9149)
    void create_player_squads(); // 0x00A842F0 (:3194)
    unsigned int create_a_player_squad(const squad_definition* squad_def); // 0x00A33DD0 (:3209)
    weapon_lock_on_status heat_seeking_target_locking_status(); // 0x00A10550 (:3240)
    bool target_is_locking_or_locked();
    bool target_is_locking();
    bool target_is_locked(); // 0x00A2C600 (.h:677)
private:
    void squad_dismiss_member(npc* np) const; // 0x00A10830 (:4689)
    bool check_squad(object_squad* squad, bool is_squadmate); // 0x00A850D0 (:4581)
    void save_inventory_from_activity_backup(cfile* save_file); // 0x00A11300 (:9082)
public:
    virtual bool is_undercover() const; // vtbl+0x90 0x00A10850 (:4707)
private:
    bool check_comm_tower_range() const; // 0x00A5CAA0 (:3967)
public:
    virtual ~player(); // vtbl+0x00 0x00ADEE90

    float frametime_multiplier; // 0xCE0
    t_usable_object action_object; // 0xCE4
    timestamp action_object_timestamp; // 0xCFC
    timestamp last_fire_time; // 0xD00
    unsigned int mp_action_object_handle; // 0xD04
    int radio_id; // 0xD08
    int tag_trigger; // 0xD0C
    tagging_sequence tag_seq; // 0xD10
    timestamp tag_timer; // 0xD14
    int tag_effect; // 0xD18
    hud_message_handle hud_message; // 0xD1C
    unsigned int havok_filter_group; // 0xD20
    unsigned int aim_target; // 0xD24
    unsigned int penetrating_aim_target; // 0xD28
    vector penetrating_aim_pos; // 0xD2C
    timestamp aim_target_duration; // 0xD38
    unsigned int heat_seeking_target; // 0xD3C
    vector aim_pos; // 0xD40
    timestamp combat_target_update_time; // 0xD4C
    float remote_charge_time; // 0xD50
    bool doing_remote_charge; // 0xD54
    player_zoom_state zoom_state; // 0xD58
    player_zoom_state previous_zoom_state; // 0xD5C
    vector cover_vector; // 0xD60
    vector cover_vector_right; // 0xD6C
    timestamp entering_cover; // 0xD78
    timestamp exiting_cover; // 0xD7C
    float cover_move_vel; // 0xD80
    bool cover_approaching_edge; // 0xD84
    timestamp cover_approaching_edge_ts; // 0xD88
    vector cover_edge_hit_pos; // 0xD8C
    vector cover_edge_miss_pos; // 0xD98
    vector thrown_weapon_check_pos; // 0xDA4
    int combat_move_queue; // 0xDB0
    timestamp_percent previous_bullet_hit; // 0xDB4
    float jetpack_fuel_percent; // 0xDBC
    int jetpack_foley; // 0xDC0
    unsigned int jetpack_effect; // 0xDC4
    bool allow_ragdoll; // 0xDC8
    player_flags pflags; // 0xDCC
    bool hold_object_available; // 0xDD4
    unsigned int hold_object_handle; // 0xDD8
    unsigned int abandoned_vehicles[3]; // 0xDDC
    int num_abandoned_vehicles; // 0xDE8
    custom_vehicle custom_vehicles[50]; // 0xDEC
    custom_vehicle* custom_vehicle_list; // 0x110C
    custom_vehicle* custom_vehicle_free_list; // 0x1110
    int num_custom_vehicles; // 0x1114
    timestamp_percent dead_timestamp; // 0x1118
    unsigned int mat_fx_handle[16]; // 0x1120
    unsigned int current_mat_fx; // 0x1160
    player_script_mode script_mode; // 0x1164
    script_specific_data script_data; // 0x1168
    multi_object_backpack* backpack; // 0x1170
    int mp_respawn_backpack_type; // 0x1174
    c_command mp_current_command; // 0x1178
    bool mp_remote_player_fired_this_frame; // 0x1228
    vector mp_camera_pos; // 0x122C
    matrix mp_camera_orient; // 0x1238
    vector2 mp_camera_angular_velocity; // 0x125C
    int mp_desired_weapon_slot; // 0x1264
    unsigned int mp_last_respawn_time; // 0x1268
    unsigned int mp_data; // 0x126C
    unsigned int mp_disarm_bomb_handle; // 0x1270
    multi_sim_state<multi_player_state> mp_sim_state; // 0x1274
    unsigned int mp_last_spawn_point_handle; // 0x2348
    multi_timestamp mp_time_until_player_can_respawn; // 0x234C
    multi_timestamp_percent mp_time_until_player_leaves_invulnerable; // 0x2350
    multi_object_backpack* mp_swap_backpack; // 0x2358
    float mp_player_damage_history[16]; // 0x235C
    player_ladder_state mp_ladder_state; // 0x239C
    vector mp_ragdoll_force; // 0x23A8
    vector mp_ragdoll_pos; // 0x23B4
    timestamp mp_ragdoll_force_timeout; // 0x23C0
    multi_player_edge_correction_info mp_edge_correction_info; // 0x23C4
    multi_player_blocked_info mp_blocked_info; // 0x23CC
    vector mp_ragdoll_linear_velocity_aggregate; // 0x23D0
    vector mp_ragdoll_angular_velocity_aggregate; // 0x23DC
    float mp_simulation_time_used; // 0x23E8
    float mp_simulation_time_accumulated; // 0x23EC
    float mp_simulation_time_extrapolated; // 0x23F0
    int jetpack_use_log_handle; // 0x23F4
    int backpack_equip_log_handle; // 0x23F8
    int district_log_handle; // 0x23FC
    matrix43 camera_bone_transform; // 0x2400
    vector movement_velocity; // 0x2430
    vector2 direct_control_dir; // 0x243C
    vector direct_control_vel; // 0x2444
    vector cover_collision_normal; // 0x2450
    vector last_frame_cover_collision_normal; // 0x245C
    player_cover_test cover_test_queue; // 0x2468
    timestamp cover_delay_ts; // 0x246C
    timestamp cover_enter_ts; // 0x2470
    timestamp cover_exit_delay_ts; // 0x2474
    timestamp cover_dive_capsule; // 0x2478
    timestamp cover_lean_delay; // 0x247C
    float cover_root_offset; // 0x2480
    float spinebend_root_offset; // 0x2484
    timestamp facial_idle; // 0x2488
    timestamp footstep_propagate_time; // 0x248C
    timestamp bump_into_propagate_time; // 0x2490
    unsigned int ammo_box_handle; // 0x2494
    timestamp pathfind_time_out; // 0x2498
    interface_modes current_interface_mode; // 0x249C
    unsigned int common_controls_allowed; // 0x24A0
    unsigned int controls_general_allowed; // 0x24A4
    unsigned int controls_on_foot_allowed; // 0x24A8
    unsigned int controls_driving_allowed; // 0x24AC
    unsigned int general_actions_allowed; // 0x24B0
    timestamp weapon_swap_timestamp; // 0x24B4
    timestamp sprint_delay_timestamp; // 0x24B8
    timestamp sprint_start_timestamp; // 0x24BC
    timestamp jump_refresh_timestamp; // 0x24C0
    unsigned int ally_squad_handle; // 0x24C4
    unsigned int escort_squad_handle; // 0x24C8
    timestamp check_squads_timer; // 0x24CC
    struct
    {

        uint64_t last_visited_veh_nav_cell_handle; // 0x00
        timestamp last_visited_veh_nav_cell_update_timer; // 0x08
    } pathfind_info; // 0x24D0
    decal_circ_array_queue<10> blood_decals; // 0x24E0
    int blood_decals_fade_index; // 0x253C
    cfile* activity_inventory_save_file; // 0x2540
    unsigned char activity_inventory_buffer[1024]; // 0x2544
    timestamp zoom_cancel_timestamp; // 0x2944
    unsigned int non_inventory_item_handle; // 0x2948
    unsigned int last_vehicle_driven; // 0x294C
    matrix quick_turn_orient; // 0x2950
    float quick_turn_speed; // 0x2974
    player_meta_data meta_data; // 0x2978
    timestamp radiation_timestamp; // 0x2D98
    float radiation_damage; // 0x2D9C
    int radiation_foley; // 0x2DA0
    timestamp is_stuck_timer; // 0x2DA4
    vector last_stuck_pos; // 0x2DA8
    int ragdoll_override_get_up_time; // 0x2DB4
    timestamp_percent fade_backpack_time; // 0x2DB8
private:
    timestamp comm_tower_check_period; // 0x2DC0
    int next_record; // 0x2DC4
    player_positional_record positional_records[8]; // 0x2DC8
    timestamp tracking_period; // 0x2E48
};

// size 0x4
struct action_message_flags_type
{
    action_message_flags_type(); // 0x00A33CB0 (:315)
    void reset(); // 0x00A10170 (:320)

    unsigned int was_near_vehicle : 1; // 0x00:0
    unsigned int was_near_ladder : 1; // 0x00:1
};

extern player* Player; // 0x03023874, the local player

bool player_is_near_vehicle(const player* pp, t_usable_object& usable_object); // 0x00AB2F70 (:1103)
bool player_is_near_turret(player* pp, t_usable_object& usable_object); // 0x00ACCE20 (:1167)
ladder* player_is_near_ladder(const player* pp); // 0x00A5B5C0 (:1287)
void get_ammo_box_anim_position(const item* abp, vector& open_pos); // 0x00A10200 (:1411)
bool player_is_near_mission_start(const player* pp, t_usable_object& usable_object); // 0x00A5B9A0 (:1417)
bool player_is_near_upgrade_node(const player* pp, t_usable_object& usable_object); // 0x00A5BC70 (:1511)
bool demolitions_master_can_be_started(); // 0x00A10230 (:1562)
bool player_is_near_offensive_raid_start(const player* pp, t_usable_object& usable_object); // 0x00A5BDD0 (:1593)
bool player_is_near_demolitions_master_node(const player* pp, t_usable_object& usable_object); // 0x00A5BF40 (:1637)
bool player_is_near_riding_shotgun_node(const player* pp, t_usable_object& usable_object); // 0x00A5C2E0 (:1702)
bool player_is_near_area_defense_node(const player* pp, t_usable_object& usable_object); // 0x00A83F20 (:1787)
bool player_is_near_house_arrest_node(const player* pp, t_usable_object& usable_object); // 0x00A5C490 (:1877)
bool ammo_box_is_usable_by_player(const item* const abp, const player* const pp); // 0x00A10280 (:1950)
bool player_process_usable_items(player* pp, t_usable_object& usable_object); // 0x00ADDF30 (:2002)
bool is_player_near_wmr_flatbed(player* pp, t_usable_object& usable_object); // 0x00A84100 (:2171)
bool player_is_near_action_node(player* pp, t_usable_object& usable_object); // 0x00AC1DF0 (:2243)
bool player_is_usable_object_allowed(const player* pp, const t_usable_object* usable_object); // 0x00A103A0 (:2312)
bool player_is_usable_object_better(const player* pp, const t_usable_object* usable_object); // 0x00A103D0 (:2327)
bool player_is_near_mission_start(player* pp, t_usable_object* usable_object); // 0x00A5B9A0
void player_process_object_interaction(player* pp); // 0x00ADE470 (:2347)
const char* classname_player(); // 0x00A104C0 (:2734)
object* alloc_player(); // 0x00A5C780 (:2734)
void types_player(signed char& type, signed char& subtype); // 0x00A104A0 (:2734)
int bpo_index_player(); // 0x00A104B0 (:2734)
int get_backpack_index(upgrade_definition_names backpackType); // 0x00A10530 (:3057)
void player_system_init(); // 0x00A10880 (:4729)
void player_level_init_pre(); // 0x00A108C0 (:4753)
bool player_level_init_post(); // 0x00AD5CD0 (:4758)
void player_level_shutdown(player* pp); // 0x00A9E080 (:4963)
void player_do_frame(player* pp); // 0x00AE66B0 (:4982)
player* player_get_local_player(); // 0x00A108D0 (:5507)
player_meta_data* player_get_meta_data(); // 0x00A108E0 (:5512)
void player_delete_all(); // 0x00A108F0 (:5521)
void player_process_all(); // 0x00AE7750 (:5527)
void player_process_all_no_animation(); // 0x00AB3260 (:5538)
void player_level_shutdown_all(); // 0x00A9E0D0 (:5553)
bool player_use_mp_disarm(player* pp); // 0x00A10900 (:5574)
bool player_start_dialogue(player* pp); // 0x00A10910 (:5584)
bool player_use_vehicle(player* pp, bool do_action); // 0x00A9E160 (:5750)
bool player_flip_vehicle(player* pp, bool do_action); // 0x00A34080 (:5873)
void player_switch_seat(player* pp); // 0x00A34200 (:5910)
bool player_use_turret(player* pp, bool do_action); // 0x00AC2620 (:5994)
bool player_use_ladder(player* pp, bool do_action); // 0x00ABE560 (:6057)
bool player_use_ammo_box(player* pp, bool do_action); // 0x00A853D0 (:6089)
bool player_start_mission(player* pp, bool do_action); // 0x00A342D0 (:6174)
bool player_start_upgrades(player* pp, bool do_action); // 0x00A34370 (:6210)
bool player_start_offensive_raid(player* pp, bool do_action); // 0x00A34430 (:6252)
bool player_start_demolitions_master(player* pp, bool do_action); // 0x00A34460 (:6270)
bool player_start_riding_shotgun(player* pp, bool do_action); // 0x00A344D0 (:6294)
bool player_start_area_defense(player* pp, bool do_action); // 0x00A34540 (:6319)
bool player_start_house_arrest(player* pp, bool do_action); // 0x00A345B0 (:6343)
bool player_pickup_weapon(player* pp, bool do_action); // 0x00AD6420 (:6373)
bool player_pickup_non_inventory(player* pp, bool do_action); // 0x00AC2720 (:6458)
bool player_mount_walker_in_vehicle(player* pp, bool do_action); // 0x00A5CD40 (:6559)
bool player_use_action_node(player* pp, bool do_action); // 0x00ABE5D0 (:6590)
bool player_set_down_non_inventory(player* pp); // 0x00ABE780 (:6673)
void player_remove_item_from_hands(player* pp); // 0x00AB8860 (:6710)
bool player_has_non_inventory(const player* pp); // 0x00A10920 (:6735)
bool player_use_equipped_item(player* pp); // 0x00A10960 (:6751)
bool player_holster_unholster_weapon(player* pp); // 0x00ACD950 (:6765)
bool player_firearm_attack_start(bool secondary_weapon); // 0x00ACD990 (:6778)
bool player_firearm_attack_stop(bool secondary_weapon); // 0x00A5CDC0 (:6832)
void player_update_jetpack(player* pp, bool boost_active); // 0x00A9E430 (:6847)
void player_radiation_start(player* pp, float damage_per_second); // 0x00A10980 (:7018)
void player_radiation_stop(player* pp); // 0x00A109F0 (:7032)
bool player_is_sabotaging_flyer(player* pp, flyer* fp); // 0x00AB32D0 (:7045)
void player_set_zoom_state(player* pp, player_zoom_state zoom, bool instant_change); // 0x00A85570 (:7077)
bool player_is_zoomed(const player* pp); // 0x00A10A50 (:7187)
bool player_is_fine_aimed(const player* pp); // 0x00A10A70 (:7193)
bool player_is_scoped(const player* pp); // 0x00A10A90 (:7199)
void player_make_last_equipped_weapon_valid(player* pp); // 0x00A10AB0 (:7205)
bool player_has_room_in_inventory(const player* pp); // 0x00A10B20 (:7240)
bool player_equip_item(player* pp, inventory_item* ip, item** unequipped_item, bool server_setting); // 0x00AC2B60 (:7274)
void player_adjust_cash(player* pp, float amount); // 0x00A10B80 (:7342)
void player_reequip(player* pp); // 0x00ADF870 (:7348)
void player_lock_local_player_controls(); // 0x00A10BC0 (:7403)
void player_unlock_local_player_controls(); // 0x00A10BE0 (:7411)
bool player_is_local_player_controls_locked(); // 0x00A10C00 (:7421)
void player_respawn(player* pp, vector& respawn_pos, matrix& respawn_orient, bool do_teleport); // 0x00ADFA10 (:7437)
void player_respawn(player* pp, int player_start_index, bool do_teleport); // 0x00ADFED0 (:7603)
void player_set_next_active_wrecking_crew_player(); // 0x00A10C20 (:7621)
void player_is_killed(player* pp); // 0x00A346F0 (:7682)
bool player_occupied_vehicle(const vehicle* vp); // 0x00A5CE20 (:7699)
int player_get_max_squad_size(); // 0x00A10C40 (:7714)
void player_register_abandoned_vehicle(player* pp, unsigned int v_handle); // 0x00A34740 (:7729)
void player_unregister_abandoned_vehicle(player* pp, unsigned int v_handle); // 0x00A10C50 (:7760)
bool player_can_vehicle_be_deleted(player* pp, unsigned int v_handle); // 0x00A10CD0 (:7797)
custom_vehicle* player_add_custom_vehicle(player* pp, int type_index, bool start_in_garage); // 0x00A10D00 (:7822)
void player_delete_custom_vehicle(player* pp, custom_vehicle* cvp); // 0x00A10DE0 (:7843)
void player_set_crouch_camera(bool crouch); // 0x00A10E80 (:7856)
void player_refresh(player* pp); // 0x00A10F00 (:7886)
bool player_ragdoll_start(); // 0x00ACDA90 (:8144)
bool player_ragdoll_stop(bool hold_pos); // 0x00AC2EC0 (:8156)
void player_get_max_movement_speeds(player* pp, float& max_movement_speed, float& max_movement_speed_base); // 0x00A857A0 (:8165)
void player_process_normal_movement(player* pp, vector new_dest_point, float speed); // 0x00A85D10 (:8267)
const player* player_find_nearest_player(const vector& point); // 0x00A5CE80 (:8433)
const player* player_find_nearest_player_xz(const vector& point); // 0x00A5CF30 (:8459)
float player_find_shortest_player_dist_sqrd(const vector& point); // 0x00A5CFE0 (:8486)
float player_find_shortest_player_xz_dist_sqrd(const vector& point); // 0x00A5D0A0 (:8507)
bool local_player_is_friendly(const human* hp); // 0x00A347B0 (:8529)
bool local_player_is_in_vehicle(unsigned int vehicle_handle); // 0x00A5D160 (:8537)
void local_player_fill_ammo(); // 0x00A34800 (:8551)
void local_player_fade_out_backpack(int fade_time_ms); // 0x00A10F20 (:8579)
void local_player_fade_in_backpack(int fade_time_ms); // 0x00A11060 (:8605)
bool player_is_player_squad(unsigned int squad_handle); // 0x00A5D1C0 (:8632)
void validate_player_destruction(const object* obj); // 0x00A34880 (:8645)
bool player_can_swap_inv_item(const player* pp, const inventory_item* inv_item); // 0x00A111B0 (:8667)
inventory_item* player_get_swap_weapon(const player* pp); // 0x00A111F0 (:8687)
bool player_can_swap_weapons(player* pp); // 0x00A11290 (:8710)
bool player_can_pickup_weapon(player* pp, item* itemp); // 0x00A348E0 (:8719)
void player_do_weapon_swap(player* pp, unsigned int weapon_to_pickup, int swap_slot); // 0x00ACDAB0 (:8764)
bool player_weapon_autoswap(player* pp); // 0x00AC2EE0 (:8908)
void player_equip_sledgehammer(player* pp); // 0x00AC3000 (:8958)
const human* player_get_enemy_target_from_object(const player* pp, const object* target); // 0x00A34980 (:8981)
bool player_update_combat_target(player* pp, const object* hit_objp); // 0x00A34A50 (:9024)
void player_get_camera_pos_and_orient(const player* pp, const vector** pos, const matrix** orient); // 0x00A112B0 (:9040)
bool is_player_my_target_or_is_my_target_with_player_in_stuck_vehicle(const npc* np); // 0x00A5D220 (:9232)
void player_set_player_invulnerable(player* pp, bool is_invulnerable); // 0x00A5D2D0 (:9258)
void player_start_material_effect(player* pp, unsigned int effect, const material_effect_parameters& mat_fx_params); // 0x00A34BB0 (:9285)
bool player_stop_material_effect(player* pp); // 0x00A11400 (:9355)
void player_refresh_head_material_effect(player* pp); // 0x00A34D10 (:9378)
void player_get_local_player_info(local_player_info* info); // 0x00A11480 (:9409)
void player_record_single_player_stats(player* pp); // 0x00A5D360 (:9424)
void player_hide_jetpack_straps(human* hp); // 0x00A9EBA0 (:9491)
void player_create_armor_prop(int armor_level, player* pp); // 0x00AB3350 (:9505)

extern bool Player_died; // 0x0302F0A3
extern bool Player_instant_unequip; // 0x01694314
extern bool Allow_player_flying_equips; // 0x01694315
extern bool Player_recruit_enabled; // 0x01694422
extern bool Player_dismiss_enabled; // 0x01694423
extern bool Wants_to_start_game_with_golden_hammer; // 0x0302F044
extern bool Look_at_player; // 0x0302F045
extern bool Player_instant_weapon_change; // 0x0302F048

// 0x00A104F0: the timestamps construct themselves.
// 0x00A2C600 (.h:677): the target was aimed at for more than 0.5 s (the 2 s test before it is redundant)
inline bool player::target_is_locked()
{
    if (aim_target_duration.time_since() > 2000) // Magic numbers: 2 s and 0.5 s
    {
        return true;
    }
    return aim_target_duration.time_since() > 500;
}

// 0x006F6990 (.h:167)
inline void t_usable_object::reset()
{
    handle = 0xFFFFFFFF;
    action_type = GENERAL_ACTION_TYPE_INVALID;
    action_icon = USE_OBJECT_ICON_INVALID;
    action_callback = nullptr;
    dot_prod = -1.0f;
    dist_squared = -1.0f;
}

inline player_ladder_state::player_ladder_state()
{
}

// 0x00A10510: nothing to construct.
inline player_positional_record::player_positional_record()
{
}
