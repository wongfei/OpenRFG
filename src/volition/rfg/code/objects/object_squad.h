// Original: volition/rfg/code/objects/object_squad.h
// Implementation: volition/rfg/code/objects/object_squad.cpp
//
// object_squad, a group of npcs (and their vehicle) spawned and directed together (OT_SQUAD): members, leader, orders,
// patrol position, spawn state and minimap display. Placement of squad_spawn_status inferred.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_damage_area.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

class activity_courier_chase_data;
class gfm_orders;
class human_info;
enum map_icon_types
{
    MAP_ICON_TYPE_NORMAL = 0,
    MAP_ICON_TYPE_STICKY = 1,
    MAP_ICON_TYPE_CLOSEST_STICKY = 2,
    NUM_MAP_ICON_TYPES = 3,
};
class matrix;
struct navmesh_lane_spline_node;
enum npc_despawn_type;
class npc_spawn_param_list;
class object_convoy;
class object_spawn_region;
class player;
struct squad_definition;
class squad_progress_info;
class vehicle_spawn_group_info;

enum squad_spawn_status
{
    SQUAD_SPAWN_NONE = -1,
    SQUAD_SPAWN_OK = 0,
    SQUAD_SPAWN_PARTIAL_NPC_CREATE_FAILED = 1,
    SQUAD_SPAWN_FAILED_NPC_MAX_QUANTITY = 2,
    SQUAD_SPAWN_FAILED_NO_ORDERS = 3,
    SQUAD_SPAWN_FAILED_OUT_OF_SPACE = 4,
    SQUAD_SPAWN_FAILED_OBJECT_DOES_NOT_EXIST = 5,
    SQUAD_SPAWN_FAILED_NO_OPEN_SPACE = 6,
    SQUAD_SPAWN_GENERIC_FAILURE = 7,
    SQUAD_SPAWN_FAILED_NPC_CREATE_FAILED = 8,
    SQUAD_SPAWN_FAILED_NO_DEFINITION = 9,
    NUM_SQUAD_SPAWN_STATUS = 10,
};

// size 0x4
struct squad_flags_struct
{
    squad_flags_struct(); // 0x005FE650 (.h:140)
    void reset(); // 0x005DD5B0 (.h:188)

    unsigned int simulation : 1; // 0x00:0
    unsigned int inside_load_dist : 1; // 0x00:1
    unsigned int outside_unload_dist : 1; // 0x00:2
    unsigned int force_spawn : 1; // 0x00:3
    unsigned int leader_has_died : 1; // 0x00:4
    unsigned int activity : 1; // 0x00:5
    unsigned int old_activity : 1; // 0x00:6
    unsigned int ignore_road_spawn : 1; // 0x00:7
    unsigned int disallow_reassignment : 1; // 0x00:8
    unsigned int player_squad : 1; // 0x00:9
    unsigned int stay_in_vehicle : 1; // 0x00:10
    unsigned int high_priority_targets : 1; // 0x00:11
    unsigned int vehicle_damage_update : 1; // 0x00:12
    unsigned int flagged_for_despawn : 1; // 0x00:13
    unsigned int squad_icon_is_sticky : 1; // 0x00:14
    unsigned int squad_icon_is_closest : 1; // 0x00:15
    unsigned int just_spawned : 1; // 0x00:16
    unsigned int flier_squad : 1; // 0x00:17
    unsigned int reinforcement_squad : 1; // 0x00:18
    unsigned int vehicle_infinite_mass : 1; // 0x00:19
    unsigned int managed_layer_creation : 1; // 0x00:20
    unsigned int dynamic_creation : 1; // 0x00:21
    unsigned int mission_created : 1; // 0x00:22
    unsigned int hands_off_raid_squad : 1; // 0x00:23
    unsigned int special_despawn_rules : 1; // 0x00:24
};

// size 0x16C
class object_squad : public object
{
    // stub_impl.md S9: script actions set these directly (inlined accessors).
    friend bool formation_get_object_velocity(vector& vel, const object* obj); // reads simulated_velocity (inferred)
    friend void Action_callback_ai_ignore_npc(struct script_node** args);
    friend void Action_callback_ai_ignore_player(struct script_node** args);
    friend class player; // ally_squad_assign_orders swaps assigned_orders_handle (inlined)
    friend class marauder_ambush_region; // update_squads and update_squad_orders read the orders handles (inlined)
    friend class gfm_smash_building_coordinator; // reads the orders handles (inlined)
    friend void object_safehouse_handle_disable(class object_safehouse* osp); // reads the orders handles (inlined)
    friend void squad_mission_clean_up(); // reads definition and squad_flags (inlined)
    friend void activity_release_squad(unsigned int spawn_node_handle); // clears hands_off_raid_squad (inlined)
    friend class activity_raid_data; // accept_new_squad reads definition (inlined)
    friend class offensive_raid_data; // activity_initialize sets special_despawn_rules (inlined)
    friend class object_roadblock_node; // release_squads reads override_team and definition (inlined)
    friend class activity_convoy_data; // reads the members, their flags and squad_vehicle (inlined)
    friend class activity_courier_chase_data; // activity_start sets vflags, activity_kill reads squad_vehicle (inlined)
    friend class activity_vehicle_rail_data; // object_setup sets ignore_road_spawn (inlined)
    friend class shared_vrail_spawn_data; // do_spawn sets ignore_road_spawn (inlined)
public:
    // mission_clean_invlunerable_flags (inlined).
    void clear_mission_invulnerable()
    {
        if (hflags.mission_invulnerable)
        {
            hflags.invulnerable = 0;
            hflags.mission_invulnerable = 0;
        }
        if (vflags.m_mission_invulnerable)
        {
            vflags.m_invulnerable = 0;
            vflags.m_mission_invulnerable = 0;
        }
    }
public:
    object_squad(); // 0x00A6F590 (:2098)
    static const object_type get_type_static(); // 0x005DD5C0 (.h:216)
    static const object_sub_type get_subtype_static(); // 0x005DD5D0 (.h:216)
    static const char* get_classname_static(); // 0x00A06070 (.h:216)
    static int get_bpo_index_static(); // 0x00A06080 (.h:216)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A6F620 (.h:216)
    static unsigned int get_classname_hash(); // 0x00A1D920 (:2097)
    static int pool_remaining(); // 0x00A3FEB0 (:2097)
    virtual void pool_free(); // vtbl+0x80 0x00A94C10 (:2097)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAD030 (:2295)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1DA50 (:2488)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1D930 (:2104)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1DA00 (:2111)
    virtual bool is_killed() const; // vtbl+0x10 0x00A40000 (:2216)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A3FFD0 (:2185)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A94BB0 (:2069)
public:
    void assign_base_orders(unsigned int new_orders); // 0x00A94E60 (:3290)
    void assign_orders(unsigned int new_orders); // 0x00A704C0 (:3321)
    void clear_orders(unsigned int old_orders); // 0x00A70540 (:3403)
    void evaluate_orders(); // 0x00A704E0 (:3380)
    bool request_despawn_all_members(npc_despawn_type despawn_type, const object* despawn_objp, bool destroy_orders); // 0x00A405E0 (:3462)
    void request_ai_replan(unsigned int except_this_npc) const; // 0x00A40740 (:3532)
    bool should_be_destroyed() const; // 0x00A407A0 (:3546)
    bool clear_assigned_orders(unsigned int* old_orders_handle); // 0x00A408D0 (:3614)
    bool reassign_orders(unsigned int new_orders_handle, unsigned int* old_orders_handle, bool make_base_orders); // 0x00A40840 (:3580)
    bool should_attempt_spawn() const; // 0x00A708E0 (:3850)
    bool should_attempt_despawn() const; // 0x00A709E0 (:4057)
    const object* spawn_parent_object() const; // 0x00A1DB10 (:4027)
    bool should_spawn_with_vehicle() const; // 0x00A1DA40 (:2274)
    bool spawn_members(bool force_spawn, const object_spawn_region* osrp); // 0x00A94C20 (:2632)
    bool despawn_members(); // 0x00A6F800 (:2561)
    void despawn(); // 0x00A70890 (:3836)
    void update_position(); // 0x00A94EB0 (:3642)
    bool add_existing_npc(unsigned int npc_handle); // 0x00A705D0 (:3710)
    void remove_member(unsigned int npc_handle); // 0x00A70700 (:3750)
    bool is_member(const npc* np) const; // 0x00A40930 (:3822)
    float get_default_simulated_move_speed(float dist, bool ramp_speed, float vehicle_speed) const; // 0x00A3F9C0 (:1415)
    void simulate(); // 0x00AB47B0 (:1478)
    void simulate_default(const gfm_orders* orders); // 0x00A6EC50 (:1572)
    void simulate_attack(const gfm_orders* orders); // 0x00AACFB0 (:1615)
    void simulate_defend(const gfm_orders* orders); // 0x00AACF00 (:1585)
    void simulate_fodder(const gfm_orders* orders); // 0x00A6ECC0 (:1629)
    void simulate_patrol(const object_patrol* patrol); // 0x00A6F300 (:1822)
    void simulate_convoy(const gfm_orders* orders); // 0x00A94AC0 (:1885)
    void simulate_courier(const gfm_orders* orders); // 0x00A6F490 (:1926)
    void simulate_convoy_follower(object_convoy* convoy); // 0x00A6F500 (:1955)
    void simulate_pathfind_to(const gfm_orders* orders); // 0x00A94800 (:1726)
    void simulate_go_to_spawn_node(); // 0x00A6F270 (:1710)
    bool simulate_move_to(const vector& dest_pos, float min_dist, bool ramp_speed, float vehicle_speed); // 0x00A3FB50 (:1993)
    gfm_orders* get_squad_orders() const; // 0x00A1D7E0 (:929)
    gfm_orders_types get_squad_orders_type() const; // 0x00A1D850 (:1124)
    vehicle* get_vehicle() const; // 0x00A3F400 (:1134)
    const vehicle_spawn_group_info* get_veh_spawn_group() const { return veh_spawn_group; } // inlined in squad_progression.cpp
    void remove_vehicle(bool force_out, bool no_respawn); // 0x00A3F430 (:1147)
    void assign_vehicle(vehicle* vp); // 0x00A3F520 (:1186)
    bool can_be_reassigned() const; // 0x00A3F2A0 (:946)
    void report_member_death(const npc* np); // 0x00A946E0 (:1058)
    bool in_combat() const; // 0x00A3F380 (:1007)
    bool in_squad_vehicle() const; // 0x00A3F310 (:982)
    unsigned int get_orders_handle() const; // 0x00A1D750 (:758)
    unsigned int get_base_orders_handle() const; // 0x00A1D7A0 (:770)
    gfm_orders* get_orders() const; // 0x00A1D700 (:734)
    bool has_orders(unsigned int handlep) const; // 0x00753F30 (.h:393)
    bool followers_near_leader() const; // 0x00A3F1A0 (:865)
    bool followers_in_vehicle() const; // 0x00A6EAE0 (:898)
    const object_patrol* get_patrol() const; // 0x00A3F170 (:843)
    void set_no_save(); // 0x00A6EA60 (:807)
    bool is_high_priority() const; // 0x005DD5E0 (.h:411)
    void set_high_priority(bool is_high_priority); // 0x00A3F100 (:833)
    player* get_player() const; // 0x00A6EBD0 (:1033)
    object_convoy* get_convoy() const; // 0x00A3F5F0 (:1230)
    bool squad_assets_ready() const; // 0x00A3F5A0 (:1202)
    activity_courier_chase_data* get_courier_data() const; // 0x00A1D870 (:1244)
    void remove_icons_from_members(bool remove_from_squad); // 0x00A3F660 (:1257)
    void assign_icons_to_members(minimap_elements spawn_icon, map_icon_types map_icon_type); // 0x00A3F700 (:1281)
    void remove_objective_icons_from_members(bool remove_from_squad); // 0x00A3F830 (:1329)
    void assign_objective_icons_to_members(int objective, minimap_elements spawn_icon); // 0x00A3F8C0 (:1360)
    bool original_leader_dead() const;
    bool is_full() const;
    bool simulated() const; // 0x00A1D8D0 (:1353)
    const vector& get_velocity() const; // 0x005DD5F0 (.h:451)
    void set_load_dist_flag(); // 0x00753F60 (.h:454)
    void set_unload_dist_flag(); // 0x00753F80 (.h:455)
    void set_just_spawned_flag(bool value); // 0x00753FA0 (.h:457)
    void set_infinite_mass(bool value); // 0x00B1FC40 (.h:459)
    void set_team_override(human_teams new_team); // 0x00A3F070 (:776)
    human_teams get_squad_team() const; // 0x00A1D7C0 (:797)
    int get_max_member_count() const; // 0x00A1D7B0 (:789)
    bool activity_spawned() const; // 0x00A06090 (.h:473)
    bool old_activity_spawned() const;
    bool current_activity_spawned() const;
    void set_old_activity_spawned();
    bool is_player_squad() const; // 0x00753FC0 (.h:481)
    bool stay_in_vehicle() const; // 0x005DD600 (.h:484)
    bool is_hands_off_raid_squad() const; // 0x00753FD0 (.h:487)
    void clear_hands_off_flag(); // 0x00753FE0 (.h:490)
    void no_road_spawn(); // 0x00753FF0 (.h:493)
    void prevent_reassignment(); // 0x00A1D840 (:936)
    void clear_all_orders(); // 0x00A1DA60 (:3358)
    void assign_new_squad_flags(const squad_flags_struct& new_squad_flags); // 0x00A40080 (:2279)
    const squad_definition* get_squad_def() const; // 0x005DD610 (.h:505)
    bool managed_layer_created() const; // 0x00A060A0 (.h:508)
    bool delete_mission_squad() const; // 0x00A40B80 (:4837)
    bool delete_mission_squad_vehicle(const npc* np, const vehicle* vp) const; // 0x00A1DBD0 (:4872)
    void set_special_despawn_rule(); // 0x00754000 (.h:515)
private:
    void despawn_vehicle(); // 0x00A6F680 (:2494)
    void destroy_notify_destroy_vehicle(); // 0x00A3FEC0 (:2118)
    bool should_attempt_spawn_patrol(const gfm_orders* orders) const; // 0x00A40970 (:3961)
    bool should_attempt_spawn_defend(const gfm_orders* orders) const; // 0x00A409C0 (:3978)
    bool should_attempt_spawn_fodder(const gfm_orders* orders) const; // 0x00A1DAC0 (:4000)
    bool should_attempt_spawn_defalt() const; // 0x00A1DB00 (:4019)
    void release_vehicle_path(); // 0x00A1DA90 (:3370)
    bool get_vehicle_destination(vector& dest_pos) const; // 0x00A6F2C0 (:1793)
    void assign_orders_to_members(); // 0x00A40570 (:3336)
    bool spawn_squad_members(const object_spawn_region* osrp); // 0x00A6FEA0 (:3059)
    squad_spawn_status do_spawn(npc_spawn_param_list& spawn_param_list); // 0x00A6FCC0 (:2995)
    vehicle* spawn_vehicle(const object_spawn_region* osrp); // 0x00A6E620 (:555)
    bool is_inital_spawn() const; // 0x00A3FE70 (:2082)
    vehicle* should_spawn_in_vehicle(npc_spawn_param_list& spawn_param_list); // 0x00A400A0 (:2716)
    const object* should_spawn_in_object(npc_spawn_param_list& spawn_param_list); // 0x00A6FB70 (:2939)
    const object* should_spawn_in_object_defend(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp); // 0x00A6F9C0 (:2754)
    const object* should_spawn_in_object_patrol(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp); // 0x00A40170 (:2794)
    const object* should_spawn_in_object_ambient(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp); // 0x00A40500 (:2908)
    const object* should_spawn_in_object_wait(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp); // 0x00A40450 (:2880)
    const object* should_spawn_in_object_tied_up(npc_spawn_param_list& spawn_param_list, const gfm_orders* orders_object, const object* objp); // 0x00A403A0 (:2854)
public:
    npc* get_leader() const; // 0x00A70AD0 (:4131)
private:
    npc* pick_leader() const; // 0x00A40A20 (:4145)
public:
    virtual ~object_squad(); // vtbl+0x00 0x00A6F630

    squad_spawn_status spawn_status; // 0x9C
    farray<unsigned int,10> member_array; // 0xA0
    npc_flags_struct npc_flags; // 0xD8
    human_flags hflags; // 0xE4
    vehicle_flags vflags; // 0xF4
    npc_script_info scripting_info; // 0xFC
    const vehicle_spawn_group_info* veh_spawn_group; // 0x104
    unsigned int squad_vehicle; // 0x108
    timestamp last_member_removed; // 0x10C
    patrol_node_handle current_node_handle; // 0x110
    int squad_map_objective; // 0x11C
    minimap_elements squad_map_icon; // 0x120
    unsigned int spawn_node_handle; // 0x124
private:
    const squad_definition* definition; // 0x128
    unsigned int base_orders_handle; // 0x12C
    unsigned int assigned_orders_handle; // 0x130
    squad_flags_struct squad_flags; // 0x134
    timestamp squad_age; // 0x138
    vector simulated_velocity; // 0x13C
    vehicle_damage_area stored_damage_areas[1]; // 0x148
    navmesh_lane_spline_node* dest_spline_node; // 0x15C
    human_teams override_team; // 0x160
    float vehicle_hp_mult; // 0x164
    unsigned int squad_leader; // 0x168
};

// size 0xC
class report_parameters_class
{
public:
    report_parameters_class(float min_hostile_dist_sqrd, float min_high_threat, int max_report_ms); // 0x00A1D3A0 (:58)

    float minimum_distance_from_hostile_sqrd; // 0x00
    float high_priority_minimum; // 0x04
    int maximum_report_period_ms; // 0x08
};

// size 0x1C
struct squad_definition
{
    squad_definition(); // 0x00A2D370 (.h:113)
    void reset(); // 0x00A1D6E0 (:540)
    int get_member_count() const; // 0x00A06060 (.h:134)

    const char* name; // 0x00
    const human_info* special_npc_info; // 0x04
    human_teams squad_team; // 0x08
    int total_npc_count; // 0x0C
    const squad_progress_info* primary_info; // 0x10
    const squad_progress_info* secondary_info; // 0x14
    const squad_progress_info* reinforcement_info; // 0x18
};

// size 0xC
class object_squad_props : public obj_prop_block
{
public:
    object_squad_props(const vector& pos, const matrix& orient, const char* squad_name, int index, unsigned int orders); // 0x00768D70 (.h:203)
    ~object_squad_props(); // 0x0075F480

};

// stub_impl.md S5 (downloadable_content.cpp removes a DLC's squads; placement inferred; Dlc_npc_squad_def_list is
// defined in downloadable_content.cpp, next to its file statics).
extern str_data_hash_table_cached<const squad_definition*,76,unsigned char> Squad_hash_table; // 0x030C0408
extern object_pool<squad_definition,76,1> Npc_squad_def_pool; // 0x031A2040
extern string_pool Squad_name_string_pool; // 0x0328B670
extern farray<squad_definition*,22> Dlc_npc_squad_def_list; // 0x0165E220

human_teams squad_definition_get_team(const squad_definition* squad_def); // 0x00A1D6D0 (:530)
int bpo_index_object_squad(); // 0x00A1D900 (:2097)
const char* classname_object_squad(); // 0x00A1D910 (:2097)
void types_object_squad(signed char& type, signed char& subtype); // 0x00A1D8F0 (:2097)
object* alloc_object_squad(); // 0x00A6F580 (:2097)
void squad_invalidate_orders(unsigned int squad_handle, unsigned int order_handle); // 0x00A95010 (:4185)
void squad_invalidate_orders(object_squad* squad, unsigned int order_handle); // 0x00A70B20 (:4194)
void console_print_available_squads(); // 0x00A70B40 (:4204)
void squad_definitions_init(int dlc_id); // 0x00AAD470 (:4219)
const squad_definition* squad_definition_from_index(int squad_index); // 0x00A40AB0 (:4279)
int squad_definition_index_from_name(const char* name); // 0x00A95040 (:4285)
int squad_definition_index_from_definition(const squad_definition* squad_def); // 0x00A40AE0 (:4296)
const squad_definition* squad_definition_from_name(const char* name); // 0x00A70BA0 (:4302)
const squad_definition* squad_definition_from_type(gfm_orders_types orders_type, human_teams team); // 0x00A1DB60 (:4317)
bool player_squad_recruit(player* pp); // 0x00A950B0 (:4734)
bool squad_human_in_player_squad(human* hp, player* pp); // 0x00A70BF0 (:4814)
void squad_do_immediate_spawns(); // 0x00A95250 (:4888)
void destroy_squad(object_squad* squad); // 0x00A40BF0 (:4918)
void squad_pre_mission_clean_up(); // 0x00A70C40 (:4945)
void squad_mission_clean_up(); // 0x00A70D80 (:4997)
