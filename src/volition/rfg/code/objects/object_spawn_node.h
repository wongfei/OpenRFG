// Original: volition/rfg/code/objects/object_spawn_node.h
// Implementation: volition/rfg/code/objects/object_spawn_node.cpp
//
// Spawn nodes: placed nodes that spawn a vehicle, an NPC, a squad or a turret from a spawn group with day and night
// probabilities, respawning at a speed; squad nodes create the squad's orders from their parent (patrol, action node,
// nav point, region, building). Spawned objects are found again by spawn node handle.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/time/timestamp.h"

struct cfile;
class human_info;
class human_spawn_group_info;
enum map_icon_types;
class npc;
class player;
struct squad_definition;
struct turret_info;
class vector;
class vehicle;
class vehicle_info;
class vehicle_spawn_group_info;

enum spawn_node_respawn_speed
{
    INVALID_RESPAWN_SPEED = -1,
    RESPAWN_SPEED_NONE = 0,
    RESPAWN_SPEED_SLOW = 1,
    RESPAWN_SPEED_MEDIUM = 2,
    RESPAWN_SPEED_FAST = 3,
    NUM_RESPAWN_SPEEDS = 4,
};

// size 0xB0
class object_safehouse;

class object_vehicle_spawn_node : public object
{
    // The safehouse writes the disabled flag directly (friends are not recorded in rfg.pdb).
    friend void object_safehouse_handle_enable(object_safehouse* osp);
    friend void object_safehouse_handle_disable(object_safehouse* osp);
public:
    object_vehicle_spawn_node(); // 0x00A1C820 (:910)
    static const object_type get_type_static(); // 0x005DF900 (.h:61)
    static const object_sub_type get_subtype_static(); // 0x005DF910 (.h:61)
    static const char* get_classname_static(); // 0x00A0CFF0 (.h:61)
    static int get_bpo_index_static(); // 0x00A0D000 (.h:61)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1C840 (.h:61)
    static unsigned int get_classname_hash(); // 0x00A1C810 (:909)
    static int pool_remaining(); // 0x00A3E5F0 (:909)
    virtual void pool_free(); // vtbl+0x80 0x00A94100 (:909)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AABB30 (:915)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A3E6B0 (:1167)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A94190 (:1227)
public:
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A1C950 (:1214)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A1C940 (:1157)
    const vehicle_spawn_group_info* get_info() const; // 0x005DF920 (.h:86)
    unsigned int get_child_vehicle_handle() const; // 0x005DF930 (.h:89)
    const unsigned int get_mp_child() const; // 0x00A1C970 (:1233)
    void set_mp_child(unsigned int mp_child); // 0x00A1C990 (:1239)
    bool spawn_mp_child(); // 0x00A941A0 (:1245)
    unsigned int spawn_child_object(unsigned int parent_override); // 0x00A6CF20 (:1347)
    void evaluate_spawn(); // 0x00A3E620 (:1128)
    void evaluate_child_relation(); // 0x00A94110 (:1031)
    void evaluate_abandon_child(vehicle* vp); // 0x00A1C8E0 (:1054)
    void evaluate_adopt_child(); // 0x00A6CE30 (:1070)
    void force_abandon_child(vehicle* vp); // 0x00A1C920 (:1116)
    bool should_spawn() const; // 0x00754F00 (.h:113)
    bool should_evaluate_spawn() const; // 0x00A0D010 (.h:116)
    bool is_special_spawn_node() const { return node_flags.special_spawn_node != 0; } // inlined in spawn_resource_manager.cpp
    bool use_special_slot_vehicle() const; // 0x00B42EF0 (.h:119)
    bool in_spawn_range(const player* pp) const; // 0x00A1C860 (:1005)
    void reevaluate_spawn(); // 0x00A0D020 (.h:125)
    bool walker_spawn_node() const; // 0x00A0D030 (.h:128)
    void do_respawn(); // 0x00A1C8D0 (:1025)
    float get_spawn_prob() const; // 0x00A1C9B0 (:1328)
    float get_day_spawn_prob() const;
    float get_night_spawn_prob() const;
    void disable_spawn(); // 0x00754F10 (.h:139)
    void enable_spawn(); // 0x00754F20 (.h:140)
    void disable_mission_spawn(); // 0x00B1FCA0 (.h:142)
    void enable_mission_spawn(); // 0x00A0D040 (.h:143)
    bool is_disabled() const; // 0x00754F30 (.h:146)
    bool no_parking() const;
    bool is_player_vehicle_respawn() const; // 0x00A0D050 (.h:150)
    const vehicle_spawn_group_info* get_spawn_group() const { return v_spawn_group; } // rfg.exe reads the field directly
    bool allow_ambient_action() const; // 0x00A3E790 (:1302)
    bool can_spawn_child() const; // 0x00A1C9D0 (:1339)
    virtual ~object_vehicle_spawn_node(); // vtbl+0x00 0x00A1C850

private:
    const vehicle_spawn_group_info* v_spawn_group; // 0x9C
    float spawn_prob; // 0xA0
    float night_spawn_prob; // 0xA4
    unsigned int child_vehicle_handle; // 0xA8
    struct
    {
        unsigned int disabled : 1; // 0x00:0
        unsigned int mission_disabled : 1; // 0x00:1
        unsigned int evaluated_spawn : 1; // 0x00:2
        unsigned int spawn_next_frame : 1; // 0x00:3
        unsigned int special_spawn_node : 1; // 0x00:4
        unsigned int no_parking : 1; // 0x00:5
        unsigned int ge_node : 1; // 0x00:6
        unsigned int player_vehicle_respawn : 1; // 0x00:7
        unsigned int walker_spawn : 1; // 0x00:8
    } node_flags; // 0xAC
};

// size 0xB0
class object_npc_spawn_node : public object
{
    // human_teams_get_team reads the team directly (friends are not recorded in rfg.pdb)
    friend human_teams human_teams_get_team(const object* obj);
public:
    object_npc_spawn_node(); // 0x00A1CA50 (:1532)
    static const object_type get_type_static(); // 0x005DF940 (.h:193)
    static const object_sub_type get_subtype_static(); // 0x005DF950 (.h:193)
    static const char* get_classname_static(); // 0x00A0D060 (.h:193)
    static int get_bpo_index_static(); // 0x00A0D070 (.h:193)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1CA70 (.h:193)
    static unsigned int get_classname_hash(); // 0x00A1CA40 (:1531)
    static int pool_remaining(); // 0x00A3E7F0 (:1531)
    virtual void pool_free(); // vtbl+0x80 0x00A94320 (:1531)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AABDF0 (:1537)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A3E900 (:1694)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A94330 (:1744)
public:
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A1CB30 (:1733)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A1CB20 (:1684)
    const human_spawn_group_info* get_info() const; // 0x00754F50 (.h:218)
    bool has_human_info(const human_info* h_info) const; // 0x00A6D240 (:1758)
    unsigned int spawn_child_object(); // 0x00A94340 (:1772)
    human_teams get_team() const; // 0x00A1CB50 (:1751)
    unsigned int get_child_npc_handle() const; // 0x00A0D080 (.h:230)
    npc* get_child_npc() const; // 0x00A3E970 (:1727)
    void evaluate_spawn(); // 0x00A3E820 (:1644)
    bool can_clear_eval_flag() const;
    bool should_spawn() const; // 0x00A0D090 (.h:243)
    bool should_evaluate_spawn() const; // 0x00A0D0A0 (.h:246)
    bool is_special_spawn_node() const { return node_flags.special_spawn_node != 0; } // inlined in spawn_resource_manager.cpp
    bool in_spawn_range(const player* pp) const; // 0x00A1CA90 (:1607)
    void reevaluate_spawn(); // 0x00A0D0B0 (.h:252)
    bool use_special_slot_npc() const; // 0x00B42F00 (.h:255)
    bool is_hostage_node() const; // 0x00754F60 (.h:258)
    void do_respawn(); // 0x00A1CB10 (:1637)
    float get_spawn_prob() const; // 0x00A1CAF0 (:1625)
    float get_day_spawn_prob() const;
    float get_night_spawn_prob() const;
    void disable_spawn(); // 0x00B1FCB0 (.h:269)
    void enable_spawn();
    void disable_mission_spawn(); // 0x00B1FCC0 (.h:272)
    void enable_mission_spawn(); // 0x00A0D0C0 (.h:273)
    bool is_disabled() const; // 0x00A0D0D0 (.h:276)
    bool can_spawn_child() const; // 0x00A1CB70 (:1764)
    virtual ~object_npc_spawn_node(); // vtbl+0x00 0x00A1CA80

private:
    const human_spawn_group_info* h_spawn_group; // 0x9C
    float spawn_prob; // 0xA0
    float night_spawn_prob; // 0xA4
    unsigned int child_npc_handle; // 0xA8
    struct
    {
        unsigned int disabled : 1; // 0x00:0
        unsigned int mission_disabled : 1; // 0x00:1
        unsigned int evaluated_spawn : 1; // 0x00:2
        unsigned int spawn_next_frame : 1; // 0x00:3
        unsigned int special_spawn_node : 1; // 0x00:4
        unsigned int spawn_dead_body : 1; // 0x00:5
        unsigned int is_hostage : 1; // 0x00:6
    } node_flags; // 0xAC
};

// size 0xE0
class object_squad_spawn_node : public object
{
public:
    // size 0x4
    struct squad_spawn_node_flags
    {
        unsigned int disabled : 1; // 0x00:0
        unsigned int mission_disabled : 1; // 0x00:1
        unsigned int evaluated_spawn : 1; // 0x00:2
        unsigned int spawn_next_frame : 1; // 0x00:3
        unsigned int no_reassignment : 1; // 0x00:4
        unsigned int raid_spawn : 1; // 0x00:5
        unsigned int hands_off_raid_squad : 1; // 0x00:6
        unsigned int miner_persona : 1; // 0x00:7
        unsigned int safehouse_vip : 1; // 0x00:8
        unsigned int ignore_tod_spawning : 1; // 0x00:9
        unsigned int respawn : 1; // 0x00:10
        unsigned int special_spawn_npc : 1; // 0x00:11
        unsigned int special_spawn_vehicle : 1; // 0x00:12
        unsigned int disable_during_alert : 1; // 0x00:13
        unsigned int roadblock_spawn_node : 1; // 0x00:14
        unsigned int radio_operator : 1; // 0x00:15
    };

    object_squad_spawn_node(); // 0x00A1CBF0 (:1897)
    static const object_type get_type_static(); // 0x005DF960 (.h:317)
    static const object_sub_type get_subtype_static(); // 0x005DF970 (.h:317)
    static const char* get_classname_static(); // 0x00A0D0F0 (.h:317)
    static int get_bpo_index_static(); // 0x00A0D100 (.h:317)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1CC20 (.h:317)
    static unsigned int get_classname_hash(); // 0x00A1CBE0 (:1896)
    static int pool_remaining(); // 0x00A3E9B0 (:1896)
    virtual void pool_free(); // vtbl+0x80 0x00A945A0 (:1896)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAC060 (:1902)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A94630 (:2404)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A946A0 (:2494)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1CCB0 (:2439)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1CDB0 (:2447)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A6D490 (:2457)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A3EAA0 (:2317)
    const squad_definition* get_info() const; // 0x00754F70 (.h:347)
    const vehicle_spawn_group_info* get_veh_info() const; // 0x00754F80 (.h:348)
    unsigned int get_child_squad_handle() const; // 0x00754F90 (.h:351)
    unsigned int get_child_vehicle_handle() const; // 0x00A3EB60 (:2728)
    human_teams get_team() const; // 0x00A3EB40 (:2722)
    void assign_default_orders(gfm_orders_types new_orders_type); // 0x00A1CDF0 (:2501)
    unsigned int spawn_child_object(minimap_elements spawn_icon, map_icon_types map_icon_type); // 0x00A6D670 (:2519)
    void force_despawn_squad(); // 0x00A6DA60 (:2705)
    void evaluate_spawn(); // 0x00A6D370 (:2263)
    bool should_spawn() const; // 0x00A0D110 (.h:375)
    bool should_evaluate_spawn() const; // 0x00A0D120 (.h:378)
    bool is_special_spawn_npc() const { return node_flags.special_spawn_npc != 0; } // inlined in spawn_resource_manager.cpp
    bool is_special_spawn_vehicle() const { return node_flags.special_spawn_vehicle != 0; } // inlined in spawn_resource_manager.cpp
    float get_spawn_prob() const; // 0x00A1CC60 (:2134)
    float get_day_spawn_prob() const;
    float get_night_spawn_prob() const;
    void disable_spawn(); // 0x00A1CC40 (:2122)
    void enable_spawn(); // 0x00A1CC50 (:2127)
    void disable_mission_spawn(); // 0x00B1FCD0 (.h:389)
    void enable_mission_spawn(); // 0x00A0D130 (.h:390)
    bool use_special_slot_npc() const; // 0x00B42F10 (.h:393)
    bool use_special_slot_vehicle() const; // 0x00B42F20 (.h:394)
    bool is_hands_off_raid_squad() const; // 0x00754FA0 (.h:397)
    bool is_disabled() const; // 0x00A0D140 (.h:400)
    bool in_spawn_range(const player* pp) const; // 0x00A1CCA0 (:2255)
    void do_respawn(bool force_respawn); // 0x00A3E9E0 (:2207)
    void reevaluate_spawn(); // 0x00A0D160 (.h:409)
    void respawn_child(); // 0x00B1FCE0 (.h:412)
    void change_tod_status(); // 0x00A6D2F0 (:2176)
    bool has_respawn_speed() const; // 0x00A1CC80 (:2145)
    void evaluate_alert_level_spawn(); // 0x00A6D2A0 (:2152)
    void abandon_child_squad(); // 0x00A945B0 (:2364)
    void adopt_child_squad(unsigned int new_squad_handle); // 0x00A3EB00 (:2349)
    void update_vehicle_handle(unsigned int new_vehicle_handle); // 0x00A0D170 (.h:430)
    bool can_spawn_child() const; // 0x00A1CE10 (:2511)
    bool disabled_at_high_alert() const; // 0x00A0D180 (.h:436)
    bool is_roadblock_spawn_node() const; // 0x00754FB0 (.h:439)
    bool is_safehouse_vip() const; // 0x00754FC0 (.h:442)
    void set_mission_start_disabled(bool value) { node_flags.disabled = value ? 1 : 0; } // inlined in object_mission_start_node.cpp
    virtual ~object_squad_spawn_node(); // vtbl+0x00 0x00A1CC30

private:
    friend class marauder_ambush_region; // check_teams and update_squads read squad_def (inferred)
    friend class object_safehouse; // despawn_squad clears evaluated_spawn (inferred)
    float spawn_prob; // 0x9C
    float night_spawn_prob; // 0xA0
    const squad_definition* squad_def; // 0xA4
    const vehicle_spawn_group_info* veh_spawn_group; // 0xA8
    bbox local_bbox; // 0xAC
    spawn_node_respawn_speed respawn_speed; // 0xC4
    gfm_orders_types default_orders_type; // 0xC8
    unsigned int original_orders_handle; // 0xCC
    unsigned int child_squad_handle; // 0xD0
    unsigned int child_vehicle_handle; // 0xD4
    timestamp respawn_timer; // 0xD8
    squad_spawn_node_flags node_flags; // 0xDC
};

// size 0xB4
class object_turret_spawn_node : public object
{
public:
    object_turret_spawn_node(); // 0x00A1CE80 (:2748)
    static const object_type get_type_static(); // 0x00754FD0 (.h:504)
    static const object_sub_type get_subtype_static(); // 0x00754FE0 (.h:504)
    static const char* get_classname_static(); // 0x00A0D190 (.h:504)
    static int get_bpo_index_static(); // 0x00A0D1A0 (.h:504)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1CEA0 (.h:504)
    static unsigned int get_classname_hash(); // 0x00A1CE70 (:2747)
    static int pool_remaining(); // 0x00A3EBB0 (:2747)
    virtual void pool_free(); // vtbl+0x80 0x00A946B0 (:2747)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAC7C0 (:2752)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A3EC70 (:2878)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A1CF20 (:2869)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A946C0 (:2906)
public:
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A1CF30 (:2913)
    unsigned int spawn_child_object(unsigned int parent_override); // 0x00A6DAC0 (:2933)
    unsigned int get_child_turret_handle() const; // 0x00754FF0 (.h:527)
    void evaluate_spawn(); // 0x00A3EBE0 (:2841)
    bool should_spawn() const; // 0x00A0D1B0 (.h:533)
    bool should_evaluate_spawn() const; // 0x00A0D1C0 (.h:536)
    bool in_spawn_range(const player* pp) const; // 0x00A1CEC0 (:2802)
    void reevaluate_spawn(); // 0x00A0D1D0 (.h:542)
    float get_spawn_prob() const; // 0x00A1CF00 (:2829)
    float get_day_spawn_prob() const;
    float get_night_spawn_prob() const;
    void do_respawn(); // 0x00A1CED0 (:2809)
    void disable_spawn(); // 0x00A1CEE0 (:2815)
    void enable_spawn(); // 0x00A1CEF0 (:2820)
    void disable_mission_spawn(); // 0x00B1FCF0 (.h:557)
    void enable_mission_spawn(); // 0x00A0D1E0 (.h:558)
    bool is_disabled() const; // 0x00A0D1F0 (.h:561)
    bool can_spawn_child() const; // 0x00A1CF50 (:2924)
    virtual ~object_turret_spawn_node(); // vtbl+0x00 0x00A1CEB0

private:
    const turret_info* t_info; // 0x9C
    float rotation_limit; // 0xA0
    unsigned int child_turret_handle; // 0xA4
    float spawn_prob; // 0xA8
    float night_spawn_prob; // 0xAC
    struct
    {
        unsigned int disabled : 1; // 0x00:0
        unsigned int mission_disabled : 1; // 0x00:1
        unsigned int evaluated_spawn : 1; // 0x00:2
        unsigned int spawn_next_frame : 1; // 0x00:3
        unsigned int roadblock_node : 1; // 0x00:4
    } node_flags; // 0xB0
};

unsigned int object_spawn_node_get_player_vehicle_handle(); // 0x00A6CB20 (:307)
// template instance: bool obj_specific_should_not_clear<item>(const item* objp); // 0x00A1C7A0 (:743)
// template instance: bool obj_specific_should_not_clear<vehicle>(const vehicle* objp); // 0x00A1C7B0 (:753)
// template instance: unsigned int find_spawned_object<turret>(unsigned int spawn_node_handle, const vector& node_pos); // 0x00A4E180 (:785)
// template instance: unsigned int find_spawned_object<npc>(unsigned int spawn_node_handle, const vector& node_pos); // 0x00A4DD50 (:785)
// template instance: unsigned int find_spawned_object<vehicle>(unsigned int spawn_node_handle, const vector& node_pos); // 0x00A793B0 (:785)
// template instance: unsigned int find_spawned_object<object_squad>(unsigned int spawn_node_handle); // 0x00A4DF70 (:812)
const char* classname_object_vehicle_spawn_node(); // 0x00A1C800 (:909)
int bpo_index_object_vehicle_spawn_node(); // 0x00A1C7F0 (:909)
object* alloc_object_vehicle_spawn_node(); // 0x00A6CE20 (:909)
void types_object_vehicle_spawn_node(signed char& type, signed char& subtype); // 0x00A1C7E0 (:909)
const char* classname_object_npc_spawn_node(); // 0x00A1CA30 (:1531)
int bpo_index_object_npc_spawn_node(); // 0x00A1CA20 (:1531)
object* alloc_object_npc_spawn_node(); // 0x00A6D230 (:1531)
void types_object_npc_spawn_node(signed char& type, signed char& subtype); // 0x00A1CA10 (:1531)
const char* classname_object_squad_spawn_node(); // 0x00A1CBD0 (:1896)
int bpo_index_object_squad_spawn_node(); // 0x00A1CBC0 (:1896)
object* alloc_object_squad_spawn_node(); // 0x00A6D290 (:1896)
void types_object_squad_spawn_node(signed char& type, signed char& subtype); // 0x00A1CBB0 (:1896)
const char* classname_object_turret_spawn_node(); // 0x00A1CE60 (:2747)
object* alloc_object_turret_spawn_node(); // 0x00A6DAB0 (:2747)
int bpo_index_object_turret_spawn_node(); // 0x00A1CE50 (:2747)
void types_object_turret_spawn_node(signed char& type, signed char& subtype); // 0x00A1CE40 (:2747)
void object_spawn_node_level_init_post(); // 0x00A6DC70 (:3024)
void object_spawn_node_level_init(); // 0x00A1CF90 (:3080)
void object_spawn_do_spawn(); // 0x00AB46D0 (:3105)
void object_spawn_set_player_vehicle_info(const vehicle_info* player_vehicle_info); // 0x00A1CFD0 (:3138)
void object_spawn_node_do_frame(); // 0x00AAC9D0 (:3146)
void object_squad_spawn_node_set_disabled(bool disabled); // 0x00A1CFE0 (:3246)
void object_spawn_node_enable_mission_spawns(); // 0x00A6DD30 (:3264)
void object_squad_spawn_disable_for_team(human_teams team); // 0x00A1CFF0 (:3280)
void object_squad_spawn_enable_for_team(human_teams team); // 0x00A1D010 (:3289)
void object_spawn_set_walker_spawn_status(bool enabled); // 0x00A1D030 (:3302)
void object_spawn_node_system_save(cfile* save_file); // 0x00A3ECE0 (:3308)
void object_spawn_node_system_load(cfile* load_file); // 0x00A1D040 (:3335)

extern bool Allow_walker_spawn; // 0x01694700, saved by world::save_state_start

// 0x005DF930 (.h:89)
inline unsigned int object_vehicle_spawn_node::get_child_vehicle_handle() const
{
    return child_vehicle_handle;
}
