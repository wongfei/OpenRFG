// Original: volition/rfg/code/objects/humans/npc.h
// Implementation: volition/rfg/code/objects/humans/npc.cpp
//
// npc, a computer controlled human (subtype of OT_HUMAN): AI (ai_system, see ai/ai_system.h), squad membership,
// spawn data, script orders (npc_script_info), weapon drift and the npc manager that owns it (npc_manager_systems).
// npc_process_all (decompiled) runs the pathfinding checks, sends queued sound infos and alert level changes, runs
// ai_system::do_frame_pre for streamed npcs (destroying those whose zone streamed out), then ai_system::do_frame, weapon
// drift and human_do_frame for each npc processed this frame, then visibility and corpse despawning.
// npc_process_post_transform kills npcs spawned as corpses and destroys npcs that finished fading out.
// Placement of npc_script_info and npc_manager_systems inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class ai_system;
class navpoint;
enum npc_despawn_type
{
    INVALID_NPC_DESPAWN_PRIORITY = -1,
    NPC_DESPAWN_TYPE_LOW = 0,
    NPC_DESPAWN_TYPE_MEDUIM = 1,
    NPC_DESPAWN_TYPE_HIGH = 2,
    NPC_DESPAWN_TYPE_WITHDRAW = 3,
    NPC_DESPAWN_TYPE_RETREAT = 4,
    NUM_NPC_DESPAWN_PRIORITIES = 5,
};
class obj_prop_block;
class object;

enum npc_manager_systems
{
    NPC_MANAGER_INVALID = -1,
    NPC_MANAGER_SQUAD = 0,
    NPC_MANAGER_AMBIENT = 1,
    NPC_MANAGER_SPAWN_NODE = 2,
    NPC_MANAGER_CUT_SCENE = 3,
    NPC_MANAGER_ACTIVITY_ASSASSIN = 4,
    NPC_MANAGER_ACTIVITY_HOUSE_ARREST = 5,
    NPC_MANAGER_DESPAWN = 6,
    NPC_MANAGER_DEBUG = 7,
    NUM_NPC_MANAGERS = 8,
};

// size 0xC
struct npc_flags_struct
{
    npc_flags_struct(); // 0x005FB670 (.h:100)
    void reset(); // 0x005DCAA0 (.h:243)

    unsigned int had_squad : 1; // 0x00:0
    unsigned int always_show_on_minimap : 1; // 0x00:1
    unsigned int deaf : 1; // 0x00:2
    unsigned int force_targetable : 1; // 0x00:3
    unsigned int in_cutscene : 1; // 0x00:4
    unsigned int not_recruitable : 1; // 0x00:5
    unsigned int recruitable_escort : 1; // 0x00:6
    unsigned int turning_around : 1; // 0x00:7
    unsigned int use_default_team : 1; // 0x00:8
    unsigned int reequipping_weapon : 1; // 0x00:9
    unsigned int going_left : 1; // 0x00:10
    unsigned int going_left_last_time : 1; // 0x00:11
    unsigned int passenger_wait : 1; // 0x00:12
    unsigned int player_exit_wait : 1; // 0x00:13
    unsigned int chase_teleport_disable : 1; // 0x00:14
    unsigned int honked_at : 1; // 0x00:15
    unsigned int honked_at_friendly : 1; // 0x00:16
    unsigned int honked_at_hostile : 1; // 0x00:17
    unsigned int activity_related : 1; // 0x00:18
    unsigned int mission_related : 1; // 0x00:19
    unsigned int ambush_related : 1; // 0x00:20
    unsigned int convoy_squad : 1; // 0x00:21
    unsigned int civilian_has_been_oppressed : 1; // 0x00:22
    unsigned int entered_raid : 1; // 0x00:23
    unsigned int raid_cheer : 1; // 0x00:24
    unsigned int raid_watch : 1; // 0x00:25
    unsigned int return_fire_aim_bonus : 1; // 0x00:26
    unsigned int reduced_npc_damage : 1; // 0x00:27
    unsigned int maneuver_required : 1; // 0x00:28
    unsigned int activity_driver_vr : 1; // 0x00:29
    unsigned int activity_pursuit_vr : 1; // 0x00:30
    unsigned int activity_pursuit_ca : 1; // 0x00:31
    unsigned int activity_ha : 1; // 0x04:0
    unsigned int prevent_weapon_draw : 1; // 0x04:1
    unsigned int tied_up : 1; // 0x04:2
    unsigned int freed_from_bonds : 1; // 0x04:3
    unsigned int run_to_safehouse : 1; // 0x04:4
    unsigned int bureaucrat : 1; // 0x04:5
    unsigned int ambient_spawn : 1; // 0x04:6
    unsigned int ambient_flee : 1; // 0x04:7
    unsigned int ambient_near_player : 1; // 0x04:8
    unsigned int ambient_vehicle_spawn : 1; // 0x04:9
    unsigned int ambient_edf_flyer : 1; // 0x04:10
    unsigned int roadblock_guard : 1; // 0x04:11
    unsigned int thanked_player : 1; // 0x04:12
    unsigned int converted_guerrilla : 1; // 0x04:13
    unsigned int attempted_spontaneous_conversion : 1; // 0x04:14
    unsigned int dont_collide_with_humans : 1; // 0x04:15
    unsigned int disable_grenades : 1; // 0x04:16
    unsigned int no_turret_attacks : 1; // 0x04:17
    unsigned int force_out_of_vehicle : 1; // 0x04:18
    unsigned int scripted_move : 1; // 0x04:19
    unsigned int scripted_drive : 1; // 0x04:20
    unsigned int only_ride_with_player : 1; // 0x04:21
    unsigned int played_added_message : 1; // 0x04:22
    unsigned int has_reached_player : 1; // 0x04:23
    unsigned int too_far_from_player : 1; // 0x04:24
    unsigned int spawned_as_corpse : 1; // 0x04:25
    unsigned int use_corpse_vehicle_exit : 1; // 0x04:26
    unsigned int man_flyer : 1; // 0x04:27
    unsigned int ignore_player : 1; // 0x04:28
    unsigned int squad_leader_ignore_player : 1; // 0x04:29
    unsigned int fire_on_vehicles : 1; // 0x04:30
    unsigned int ignore_vehicles : 1; // 0x04:31
    unsigned int orders_active : 1; // 0x08:0
    unsigned int fodder_on_object : 1; // 0x08:1
    unsigned int remove_icon_on_death : 1; // 0x08:2
    unsigned int courier_run_to_destination : 1; // 0x08:3
    unsigned int can_use_special_roads : 1; // 0x08:4
    unsigned int bomb : 1; // 0x08:5
    unsigned int drive_disabled : 1; // 0x08:6
    unsigned int used_action_node : 1; // 0x08:7
    unsigned int immediate_spawn : 1; // 0x08:8
    unsigned int console_created : 1; // 0x08:9
    unsigned int amb_watch_player : 1; // 0x08:10
    unsigned int despawn_after_fade : 1; // 0x08:11
    unsigned int allow_idle_despawn : 1; // 0x08:12
    unsigned int disallow_idle_despawn : 1; // 0x08:13
    unsigned int despawn_npc : 1; // 0x08:14
    unsigned int disallow_despawn_engagement : 1; // 0x08:15
    unsigned int high_priority_despawn : 1; // 0x08:16
    unsigned int despawn_on_green_alert : 1; // 0x08:17
};

// size 0x8
struct npc_script_info
{
    unsigned int location_object; // 0x00
    struct
    {
        bool run : 1; // 0x00:0
        bool fire_at_target : 1; // 0x00:1
        bool use_vehicle : 1; // 0x00:2
        bool drive_directly : 1; // 0x00:3
    } flags; // 0x04
};

// size 0x30
struct npc_weapon_drift_info
{
    void clear(); // 0x00A05FE0 (.h:257)
    npc_weapon_drift_info(); // 0x00A0F1D0

    float desired_error_mag; // 0x00
    float actual_error_mag; // 0x04
    timestamp_percent acquire_time; // 0x08
    timestamp_percent change_time; // 0x10
    vector2 next_error_pos; // 0x18
    vector2 prev_error_pos; // 0x20
    vector2 cur_error_pos; // 0x28
};

// size 0xD90
class npc : public human
{
public:
    npc(); // 0x00A9DD10 (:856)
    static const object_type get_type_static() { return OT_HUMAN; } // 0x005DCAB0 (.h:277)
    static const object_sub_type get_subtype_static() { return OT_SUB_HUMAN_NPC; } // 0x005DCAC0 (.h:277)
    static const char* get_classname_static(); // 0x00A06030 (.h:277)
    static int get_bpo_index_static(); // 0x005DCAD0 (.h:277)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A9DD60 (.h:277)
    static unsigned int get_classname_hash(); // 0x00A0F1C0 (:855)
    static int pool_remaining(); // 0x00A333A0 (:855)
    virtual void pool_free(); // vtbl+0x80 0x00A83A90 (:855)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A333B0 (:861)
private:
    void set_despawn_flags(npc_despawn_type desired_despawn_type); // 0x00A0F460 (:1213)
public:
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00ADD9C0 (:942)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AC1D30 (:1088)
protected:
    virtual void free_this(); // vtbl+0x28 0x00ACCDC0 (:1138)
public:
    object* get_despawn_object() const; // 0x00A0F540 (:1296)
    void get_display_name(char* output); // 0x00A0F390 (:1177)
    void clear(); // 0x00ADDC20 (:1334)
    void load_resources(); // 0x00A0F5A0 (:1407)
    void unload_resources();
    virtual bool set_team(human_teams new_team); // vtbl+0x88 0x00A0F5C0 (:1412)
    virtual bool is_in_combat() const; // vtbl+0x8C 0x00A33400 (:1321)
    bool despawn_overdue() const; // 0x00A0F560 (:1302)
    void start_active_despawn(npc_despawn_type desired_despawn_type, const object* despawn_objp); // 0x00A0F4D0 (:1260)
    bool should_active_despawn() const; // 0x00A0F410 (:1197)
    bool should_despawn() const; // 0x00A0F450 (:1207)
    virtual bool is_offscreen() const; // vtbl+0x84 0x00A333E0 (:888)
    void update_offscreen() const; // 0x00A0F300 (:909)
    int get_offscreen_period() const; // 0x00A0F2E0 (:897)
    bool vehicle_exit_banned() const; // 0x005DCAE0 (.h:373)
    bool is_wounded() const;
    float get_wounded_percentage() const;
    void fade_out_and_despawn(int _fade_time, bool fading_from_nano); // 0x00A0F660 (:1449)
    void play_random_cower_animation(bool crouch_only); // 0x00A83CB0 (:1468)
    void change_manager(npc_manager_systems new_manager); // 0x00A06040 (.h:388)
    npc_manager_systems get_manager() const; // 0x005DCB00 (.h:389)
    npc_manager_systems get_creator() const; // 0x00A06050 (.h:390)
    void collide_with_other_humans_by_default(bool);
    const ai_system* get_ai() const; // 0x005DCB20 (.h:403)
    ai_system* get_ai() { return ai; } // 0x005DCB10 (.h:402)
    // Inlined in rfg.exe (no instance): human_combat.cpp restarts the drift when the npc is hit.
    npc_weapon_drift_info& get_drift_info() { return drift_info; }
    void weapon_drift_start_acquire(); // 0x00A5AF50 (:1511)
    void weapon_drift_restart_acquire();
    void weapon_drift_process(); // 0x00AB82B0 (:1540)
    virtual ~npc(); // vtbl+0x00 0x00A9DD70

    char name[32]; // 0xCE0
    npc_flags_struct npc_flags; // 0xD00
    navpoint* start_navpoint; // 0xD0C
    object_spawn_priority spawn_priority; // 0xD10
    float m_stream_load_squared_dist; // 0xD14
    float m_stream_unload_squared_dist; // 0xD18
    vector dest_point; // 0xD1C
    uint64_t dest_cell_hint_handle; // 0xD28
    npc_script_info scripting_info; // 0xD30
    unsigned int spawn_node_handle; // 0xD38
private:
    npc_manager_systems management_system; // 0xD3C
    npc_manager_systems creation_system; // 0xD40
    timestamp time_offscreen; // 0xD44
    unsigned int despawn_obj_handle; // 0xD48
    timestamp final_despawn_timer; // 0xD4C
protected:
    npc_weapon_drift_info drift_info; // 0xD50
    ai_system* ai; // 0xD80
};

void unused_dcf_add_passenger(); // 0x00ACCD50 (:195)
void unused_dcf_npc_disarm_all(); // 0x00AD58A0 (:212)
void unused_dcf_drop_ped(); // 0x00A331F0 (:264)
bool Marauder_retreat_collect_callback(object* obj, unsigned int shape_key, const void* cb_data); // 0x00A33200 (:428)
void types_npc(signed char& type, signed char& subtype); // 0x00A0F190 (:855)
const char* classname_npc(); // 0x00A0F1B0 (:855)
int bpo_index_npc(); // 0x00A0F1A0 (:855)
object* alloc_npc(); // 0x00A5AF00 (:855)
void npc_level_init_post(); // 0x00A0F7C0 (:1792)
void npc_level_shutdown(); // 0x00A83DD0 (:1800)
void npc_process_all(); // 0x00AE6560 (:1807)
void npc_process_all_no_animation(); // 0x00AB2F10 (:1925)
void npc_process_post_transform(npc* np); // 0x00AD5900 (:1937)
void npc_kill_all(); // 0x00A5AF80 (:1968)
void npc_free_all(); // 0x00A5AFD0 (:1979)
void npc_reset_all(); // 0x00ADDEB0 (:1991)
float npc_voice_cost_player_facing(const npc* np, const void* selection_data); // 0x00A33420 (:2121)
float npc_voice_cost_player_distance(const npc* np, const void* selection_data); // 0x00A33500 (:2144)
void npc_flags_bitwise_or(const npc_flags_struct& s1, const npc_flags_struct& s2, npc_flags_struct& return_value); // 0x00A0F7F0 (:2152)
void npc_flags_set_flags(npc_flags_struct& flags, const npc_flags_struct& flags_mask); // 0x00A0F820 (:2169)
void npc_flags_bitwise_clear(const npc_flags_struct& s1, const npc_flags_struct& s2, npc_flags_struct& return_value); // 0x00A0F840 (:2183)
void npc_set_load_distance(npc* p_npc, float distance); // 0x00A0F870 (:2275)
bool npc_marauders_should_retreat(); // 0x00A0F8B0 (:2282)
void npc_maybe_set_male_guerrilla_anim_set(npc* np); // 0x00A0F8C0 (:2288)
bool npc_is_defending_player(const human* const hp); // 0x00A33520 (:2306)
npc_despawn_type npc_get_despawn_type(const char* despawn_type); // 0x00A0F910 (:2324)

// 0x005DCB00 (.h:389)
inline npc_manager_systems npc::get_manager() const
{
    return management_system;
}

// 0x005DCB20 (.h:403)
inline const ai_system* npc::get_ai() const
{
    return ai;
}

// 0x005DCAD0 (.h:277): npcs are in the human broadphase layer (World.collect_objects<npc>).
inline int npc::get_bpo_index_static()
{
    return 0;
}
