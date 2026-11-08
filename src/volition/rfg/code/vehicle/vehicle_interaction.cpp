// Original: volition/rfg/code/vehicle/vehicle_interaction.cpp
//
// Humans entering, riding and leaving vehicles. A vi_instance (vehicle::vii) holds up to 8 vi_human_managers, each a
// logic state (pathing, teleporting, exiting) and an animation state (running to the door, opening it, the transition
// animations). vi_human_pathfind_to_interior searches the table's waypoint graph and vi_register_paths decides who
// holds which waypoint each frame.

#include "volition/rfg/code/vehicle/vehicle_interaction.h"

#include "volition/rfg/code/control/control_map.h"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/hk_custom_character_proxy.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <math.h>

// Who holds a waypoint (waypoint_usage::block_type).
enum vvm_waypoint_blocked_type
{
    VVM_WP_BLOCKED_NULL = 0,
    VVM_WP_TRANS_RESERVED = 1,
    VVM_WP_SEAT_FREE = 2,
    VVM_WP_SEAT_RESERVED = 3,
    VVM_WP_SEAT_PF_FAILED = 4,
    VVM_WP_ENTRY_CHECK = 5,
    VVM_WP_ENTRY_CLEAR = 6,
    VVM_WP_ENTRY_BLOCKED = 7,
};

// vehicle_interaction_waypoint::node_type (names inferred from how the search uses them).
// (VCF_* component flags come from vehicle_class.cpp and vehicle_component.cpp, which share this translation unit.)
static const unsigned int VI_NODE_TYPE_DOOR_ENTRY = 1; // entering needs a close, level approach
static const unsigned int VI_NODE_TYPE_ENTRY = 2;
static const unsigned int VI_NODE_TYPE_EXIT_FIRST = 3; // 3 to 10 index vi_instance::exit_costs
static const unsigned int VI_NODE_TYPE_EXIT_GROUND = 6;
static const unsigned int VI_NODE_TYPE_EXIT_ROOF = 9;
static const unsigned int VI_NODE_TYPE_EXIT_CORPSE = 10;

static const int VI_MAX_HUMANS = 8;
static const int VI_MAX_DOORS = 6;
static const int VI_MAX_PATH_NODES = 6;
static const int VI_MAX_WAYPOINTS = 44;
static const int VI_MAX_SEARCH_NODES = 50;
static const float VI_WEIGHT_BLOCKED = 1000.0f; // a search weight at or above this is not walkable
static const float VI_WEIGHT_LAST_CHOICE = 150.0f;
static const int VI_RAYCAST_REFRESH_MS = 1500;

// ---- file-static globals
// const float LOW_SWERVE_DISTANCE (0x01336B5C) is used here too; placed in vehicle_class.cpp.
// const float DEFAULT_LOOKFROM_HEIGHT (0x01336E70) is used here too; a literal in vehicle_table_parser.cpp.
// const float vehicle_light_camera_fudge_distance (0x013385D0) is used here too; placed in vehicle_component.cpp.
static bool Player_vehicle_turret_override = true; // 0x016BA510 (0x101: this byte and vi_dmh_use_time_not_speed)
static bool vi_dmh_use_time_not_speed = true; // 0x016BA511
static int Vehicle_enter_seat_override = -1; // 0x016BA514
static float eject_offset = 0.2f; // 0x016BA51C
static float vi_auto_connect_ramp_out = 0.3f; // 0x016BA520
extern float Vi_impulse_ragdoll; // 0x016BA524 (human_combat.cpp, with its tweak table item)
static float Vi_impulse_ragdoll_vert_damp = 0.7f; // 0x016BA528
static tweak_table_item Tweak_table_item_Vi_impulse_ragdoll_vert_damp("Vi_impulse_ragdoll_vert_damp", &Vi_impulse_ragdoll_vert_damp); // 0x0383B600
static float Vi_impulse_ragdoll_turret = 300.0f; // 0x016BA52C
static tweak_table_item Tweak_table_item_Vi_impulse_ragdoll_turret("Vi_impulse_ragdoll_turret", &Vi_impulse_ragdoll_turret); // 0x0377CC20
static float Vi_impulse_ragdoll_vert_damp_turret = 0.4f; // 0x016BA530
static tweak_table_item Tweak_table_item_Vi_impulse_ragdoll_vert_damp_turret("Vi_impulse_ragdoll_vert_damp_turret", &Vi_impulse_ragdoll_vert_damp_turret); // 0x0381A52C
static float Vi_impulse_ragdoll_guerilla_bonus = 1.1f; // 0x016BA534
static tweak_table_item Tweak_table_item_Vi_impulse_ragdoll_guerilla_bonus("Vi_impulse_ragdoll_guerilla_bonus", &Vi_impulse_ragdoll_guerilla_bonus); // 0x0383B60C
static float Vi_vehicle_lean_force = 0.09f; // 0x016BA538
static float Vi_vehicle_lean_blend_time = 0.88f; // 0x016BA53C
static float vi_pf_stuck_threshold = 0.6f; // 0x016BA540
static float vi_pf_stuck_time = 0.5f; // 0x016BA544
static float vi_dmh_door_vel_offset = -2.0f; // 0x016BA54C
static float vi_dmh_speed_smoothing = 0.3f; // 0x016BA550
static bool Teleport_into_vehicles; // 0x03770033
static float vi_dmh_anim_frame_offset; // 0x03770048
static float turn_speed = 16.0f; // 0x016EB698, read here by process_enter_path_to_door; owning file not identified

// ---- internal functions
static int release_curr_wp_node(vi_instance* vii, int human_index); // 0x00BB00B0 (:596)
static void remove_human_from_seat(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat); // 0x00BABDA0 (:631)
static bool vi_allowed_to_exit(human* hp, vehicle* vp); // 0x00B81200 (:671)
static bool vi_door_open_action(vi_instance* vii, const unsigned int human_index, const vehicle_interaction_waypoint_transition* transition, const float open_time); // 0x00BA1520 (:693)
static void vi_door_close_action(vi_instance* vii, const int door_idx, float close_time); // 0x00BA1760 (:776)
static bool vi_door_close_for_climb(vi_instance* vii, vi_human_manager* hm, const vehicle_interaction_waypoint_transition* transition, float close_time); // 0x00BA17D0 (:800)
static void vi_update_exit_nodes(vi_instance* vii); // 0x00BA1880 (:848)
static void vi_set_refresh_raycast_timer(vi_instance* vii); // 0x00B685B0 (:1116)
static void vi_refresh_raycast(vi_instance* vii); // 0x00B81250 (:1125)
static void vi_clear_corpses(vi_instance* vii, int wpi); // 0x00B8F590 (:1176)
static float vi_human_pf_get_trans_node_weight(vi_instance* vii, const unsigned int human_index, int wpi); // 0x00B81350 (:1205)
static float vi_human_pf_get_final_node_weight(vi_instance* vii, const unsigned int human_index, int wpi); // 0x00B8F7E0 (:1292)
static bool stick_to_floor_raycast_callback_vehicle_interaction(object* obj, void* param); // 0x00B68630 (:1455)
static bool vi_human_pf_check_raycast(vi_instance* vii, int wpi, int prev_wpi); // 0x00B97F60 (:1465)
static bool vi_human_pathfind_to_interior(vi_instance* vii, const unsigned int human_index, int interior_wpi); // 0x00BABE60 (:1728)
static bool vi_human_path_to_seat(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat); // 0x00BB63B0 (:2089)
static bool vi_human_path_to_exit(vi_instance* vii, unsigned int human_index); // 0x00BB6530 (:2161)
static bool vi_request_npc_exit(vi_instance* vii, int human_index, int requester_hmi); // 0x00B814E0 (:2290)
static bool mark_seat_reserved(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat, vvm_waypoint_blocked_type mark); // 0x00BA1D90 (:2331)
static bool vi_seat_reserve(vi_instance* vii, unsigned int human_index); // 0x00BA2170 (:2501)
static bool vi_node_dep_free(const vi_instance* vii, int wpi, int hmi); // 0x00B815E0 (:2581)
static void vi_register_node(vi_instance* vii, int hmi, int wpi, farray<int,44>& waypoint_in_use, const vehicle_interaction_info* vi_info, bool lock); // 0x00B81670 (:2603)
static void vi_register_paths(vi_instance* vii); // 0x00BA2390 (:2635)
static void set_enter_pathfind(vi_instance* vii, unsigned int human_index); // 0x00BB7A10 (:2894)
static void process_enter_pathfind(vi_instance* vii, unsigned int human_index); // 0x00BB6660 (:2926)
static void set_enter_path_to_door(vi_instance* vii, unsigned int human_index); // 0x00B8FE20 (:3004)
static void process_enter_path_to_door(vi_instance* vii, unsigned int human_index); // 0x00BBBE40 (:3135)
static void set_enter_streaming_stall(vi_instance* vii, unsigned int human_index); // 0x00B81800 (:3584)
static void process_enter_streaming_stall(vi_instance* vii, unsigned int human_index); // 0x00B81880 (:3602)
static void vi_enter_teleport(vi_instance* vii, unsigned int human_index); // 0x00B686A0 (:3624)
static void set_enter_teleport_wait(vi_instance* vii, unsigned int human_index); // 0x00BB4D90 (:3629)
static void process_enter_teleport_wait(vi_instance* vii, unsigned int human_index); // 0x00BB4FF0 (:3714)
static void set_enter_complete(vi_instance* vii, unsigned int human_index); // 0x00BB5200 (:3807)
static void set_shared_wait_waypoint_clear(vi_instance* vii, unsigned int human_index); // 0x00BA28C0 (:4026)
static void set_anim_process_continue(vi_instance* vii, unsigned int human_index); // 0x00BB57D0 (:4120)
static void set_anim_cancel(vi_instance* vii, unsigned int human_index); // 0x00BB5860 (:4151)
static void process_shared_wait_waypoint_clear(vi_instance* vii, unsigned int human_index); // 0x00BBDF20 (:4169)
static void set_shared_open_door(vi_instance* vii, unsigned int human_index); // 0x00BA2B00 (:4329)
static void process_door_moving_human_recover_from_stuck(unsigned int human_index, human* hp, vehicle* vp); // 0x00BAC6A0 (:4471)
static void process_door_moving_human(unsigned int human_index, human* hp, vehicle* vp); // 0x00BAC9A0 (:4566)
static void process_shared_open_door(vi_instance* vii, unsigned int human_index); // 0x00BB5900 (:4767)
static void set_shared_transition(vi_instance* vii, unsigned int human_index); // 0x00BB3D10 (:4828)
static void vi_human_to_projectile(vi_human_manager* hm, human* hp); // 0x00B686C0 (:4948)
static void process_shared_transition(vi_instance* vii, unsigned int human_index); // 0x00BBE3F0 (:5000)
static void set_exit_pathfind(vi_instance* vii, unsigned int human_index); // 0x00BA2E20 (:5506)
static void process_exit_pathfind(vi_instance* vii, unsigned int human_index); // 0x00BB6720 (:5565)
static void process_exit_eject(vi_instance* vii, unsigned int human_index); // 0x00BB1C70 (:5650)
static void set_exit_eject(vi_instance* vii, unsigned int human_index, unsigned int perp); // 0x00BB2180 (:5793)
static bool vi_get_exit_teleport_point(vi_human_manager* hm, vehicle* vp); // 0x00B90290 (:5864)
static bool vi_should_maintain_pos_on_exit(vi_instance* vii, unsigned int human_index); // 0x00B81920 (:5959)
static void vi_exit_teleport(vi_instance* vii, unsigned int human_index); // 0x00B98AE0 (:6003)
static void set_exit_complete(vi_instance* vii, unsigned int human_index); // 0x00BB23A0 (:6080)
static void set_ride_default(vi_instance* vii, unsigned int human_index); // 0x00BB3F80 (:6197)
static void process_ride_default(vi_instance* vii, unsigned int human_index); // 0x00BB5AD0 (:6381)
static void set_ride_close_door(vi_instance* vii, unsigned int human_index); // 0x00B908B0 (:6487)
static void process_ride_close_door(vi_instance* vii, unsigned int human_index); // 0x00BB5E70 (:6529)
static void process_ride_custom_anim(vi_instance* vii, unsigned int human_index); // 0x00BB5F80 (:6558)
static void vi_set_logic_state(vi_instance* vii, const unsigned int human_index, vi_state_of_logic state); // 0x00BB7BA0 (:6586)
static void vi_process_logic_state(vi_instance* vii, unsigned int human_index); // 0x00BB6880 (:6635)
static void vi_set_anim_state(vi_instance* vii, const unsigned int human_index, vi_state_of_anim state); // 0x00BB4410 (:6679)
static void vi_process_anim_state(vi_instance* vii, unsigned int human_index); // 0x00BBF580 (:6721)
static void vi_request_exit(vi_instance* vii, unsigned int human_index, bool switching_seat); // 0x00BB44D0 (:7108)
static bool wp_is_protected_pos(const vehicle* vp, int wpi); // 0x00B68800 (:8300)
static bool human_in_protected_vehicle_pos(const vehicle* vp, int hmi); // 0x00B81C30 (:8320)

// A human from a handle, NULL when gone or not a human (rfg.exe inlines this everywhere).
static human* vi_human_from_handle(unsigned int handle)
{
    return handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : NULL;
}

// The game trace (game_trace.h, not in rfg.exe): a step of a human's interaction. One line per step, never per frame.
static void vi_trace_step(vi_instance* vii, unsigned int human_index, const char* step)
{
    if (Game_trace_file == NULL) {
        return;
    }
    const vi_human_manager& hm = vii->humans[human_index];
    human* hp = vi_human_from_handle(hm.handle);
    game_trace_write("vehicle", "%s: human=0x%X player=%d vehicle=%s seat=%d", step, hm.handle, hp != NULL && object_is_player(hp) ? 1 : 0, vii->vvm.vp->info->name, (int)hm.seat);
}

// 0x00B62E90 (global_event_data.h:214)
player_enters_vehicle_event::player_enters_vehicle_event()
    : event_data(EVENT_PLAYER_ENTERS_VEHICLE)
{
}

// 0x00B62EA0 (global_event_data.h:219)
npc_enters_vehicle_event::npc_enters_vehicle_event()
    : event_data(EVENT_NPC_ENTERS_VEHICLE)
{
}

// 0x00B62EB0 (global_event_data.h:225)
player_exits_vehicle_event::player_exits_vehicle_event()
    : event_data(EVENT_PLAYER_EXITS_VEHICLE)
{
}

// An animation by handle, or with -1 no animation (the action argument of the state changes here).
static index_alias vi_index_alias(int index)
{
    return index_alias(index, ANIM_ALIAS_NONE);
}

// 0x00B684B0 (:90)
bool vec_equal_fast(vector v0, vector v1, float delta)
{
    const float dx = v0.x - v1.x;
    const float dy = v0.y - v1.y;
    const float dz = v0.z - v1.z;
    float remaining = dx >= 0.0f ? delta - dx : dx + delta;
    remaining = dy >= 0.0f ? remaining - dy : dy + remaining;
    remaining = dz >= 0.0f ? remaining - dz : dz + remaining;
    return remaining > 0.0f;
}

// 0x00B68580 (:148)
waypoint_node::waypoint_node()
{
    wpi = -1;
    dest_ni = -1;
    weight = VI_WEIGHT_BLOCKED;
    ext_weight = VI_WEIGHT_BLOCKED;
}

// 0x00B6F850
vi_door::vi_door()
{
    open_close_ts.invalidate();
}

// 0x00B62F60 (.h:123)
void vi_door::initialize()
{
    component_id = -1;
    state = VI_DOOR_STATE_CLOSED;
    open_close_ts.invalidate();
}

// 0x00B884D0 (.h:221)
vi_human_manager::vi_human_manager()
{
    wait_ts.invalidate();
    manual_close_door_ts.invalidate();
}

// 0x00B86CD0
vi_human_manager::~vi_human_manager()
{
}

// 0x00B80FF0 (:340)
void vi_human_manager::initialize()
{
    handle = 0xFFFFFFFF;
    interaction = VI_INTERACTION_NONE;
    state = VI_STATE_NONE;
    state_of_anim = VI_STATE_ANIM_NONE;
    seat = VSI_INVALID;
    pos_correction = VI_POS_CORRECTION_NONE;
    human_motion = ZERO_VECTOR;
    ride_state_handle = -1;
    wait_ts.invalidate();
    cur_path_idx = -1;
    last_path_idx = -1;
    path_nodes[0] = -1;
    transition_idx = -1;
    animate_from_wpi = -1;
    animate_dest_wpi = -1;
    door_comp_idx = -1;
    door_idx = -1;
    being_hijacked = 0;
    freeze_hands_next_frame = 0;
    reserve_next_node_only = 0;
    enter_data.initialize();
    exit_data.initialize();
}

// 0x00B8F490
vi_vehicle_manager::vi_vehicle_manager()
{
}

// 0x00B888B0
vi_vehicle_manager::~vi_vehicle_manager()
{
}

// 0x00B97EC0 (:372)
vi_instance::vi_instance()
{
    refresh_raycasts_timestamp.invalidate();
}

// 0x00B95B60
vi_instance::~vi_instance()
{
}

// 0x00B8F4E0 (:377)
int vi_instance::add_human(human* hp)
{
    const int existing = find_human(hp);
    if (existing != -1) {
        return existing;
    }
    if (num_humans < VI_MAX_HUMANS) {
        for (int i = 0; i < VI_MAX_HUMANS; i++) {
            if (humans[i].handle == 0xFFFFFFFF) {
                humans[i].initialize();
                humans[i].handle = hp->get_handle();
                num_humans++;
                return i;
            }
        }
    }
    return -1;
}

// 0x00B81060 (:415)
int vi_instance::find_human(const human* hp) const
{
    if (hp != NULL && hp->get_handle() != 0xFFFFFFFF) {
        for (int i = 0; i < VI_MAX_HUMANS; i++) {
            if (humans[i].handle == hp->get_handle()) {
                return i;
            }
        }
    }
    return -1;
}

// 0x00BB19C0 (:438)
void vi_instance::remove_human(int human_index)
{
    vi_human_manager* hm = &humans[human_index];
    if (hm->handle == 0xFFFFFFFF) {
        return;
    }
    human* hp = vi_human_from_handle(hm->handle);
    hm->interaction = VI_INTERACTION_NONE;
    hm->state = VI_STATE_NONE;
    release_curr_wp_node(this, human_index);
    remove_human_from_seat(this, human_index, hm->enter_data.destination_seat);
    if (object_is_local_player(hm->handle)) {
        player_exits_vehicle_event event;
        event.handle = vvm.vp->get_handle();
        gem_post_event(event);
        player* pp = player_get_local_player();
        if (!Multiplayer && pp != NULL && gamelog_has_logged_vehicle_entry()) {
            // exit reason: 0 normal, 1 the vehicle is flipped, 2 the player died
            char reason = 0;
            if (pp->is_killed()) {
                reason = 2;
            }
            else if (vvm.vp != NULL && vehicle_is_flipped(vvm.vp->get_handle())) {
                reason = 1;
            }
            const char pct_complete = gamelog_get_pct_complete();
            const int time_played = gamelog_get_time_played();
            const vector player_pos = gamelog_get_human_pos(pp);
            const int duration = gamelog_get_vehicle_duration();
            const char difficulty = gamelog_get_difficulty();
            const int locale = gamelog_get_locale();
            const int vehicle_crc = gamelog_get_vehicle_crc(vvm.vp);
            const char seat_index = (char)pp->vehicle_seat_idx;
            const int time = gamelog_get_logging_time();
            log_event_exit_vehicle(gamelog_get_player_checksum(), time, seat_index, vehicle_crc, locale, difficulty, duration, player_pos, time_played, pct_complete, reason);
        }
    }
    if (hp != NULL) {
        hp->vehicle_handle = 0xFFFFFFFF;
        if (hp->rdd.is_loaded()) {
            hp->rdd.get_data()->controller->flags.sliding = 0;
        }
    }
    num_humans--;
    hm->initialize();
    reregister_paths = 1;
}

// 0x00B81090 (:528)
void vi_instance::initialize(vehicle* vp)
{
    vvm.vp = vp;
    num_humans = 0;
    refresh_raycasts_timestamp.invalidate();
    for (int i = 0; i < VI_MAX_HUMANS; i++) {
        humans[i].initialize();
    }
    for (int i = 0; i < VI_MAX_WAYPOINTS; i++) {
        vvm.used_waypoints[i].set_free();
        vvm.used_waypoints[i].block_type = VVM_WP_SEAT_FREE;
    }
    for (int i = 0; i < VI_MAX_DOORS; i++) {
        vvm.doors[i].initialize();
    }
}

// 0x00BB1BD0 (:551)
void vi_instance::passengers_destroy()
{
    for (int i = 0; i < VI_MAX_HUMANS; i++) {
        const unsigned int handle = humans[i].handle;
        if (handle == 0xFFFFFFFF) {
            continue;
        }
        object* objp = World.object_from_handle(handle);
        if (objp == NULL || objp->get_obj_type() != OT_HUMAN) {
            remove_human(i);
        }
        else if (object_is_player(objp)) {
            humans[i].exit_data.flags.maintain_position = 1;
            vi_exit_teleport(this, i);
        }
        else if (humans[i].interaction != VI_INTERACTION_RESERVE) {
            World.destroy_object(objp, false);
            remove_human(i);
        }
    }
    initialize(vvm.vp);
}

// 0x00BB00B0 (:596)
static int release_curr_wp_node(vi_instance* vii, int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->door_idx >= 0) {
        vi_door* door = &vii->vvm.doors[hm->door_idx];
        if (door->state == VI_DOOR_STATE_CLOSED_CLIMBING) {
            door->initialize();
            hm->door_idx = -1;
        }
    }
    if (hm->cur_path_idx == -1) {
        return -1;
    }
    const int wpi = hm->path_nodes[hm->cur_path_idx];
    if (wpi == -1) {
        return -1;
    }
    waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
    if (usage.get_human_index() == human_index) {
        usage.set_free();
        vii->reregister_paths |= usage.reg_on_release;
        usage.reg_on_release = 0;
    }
    else {
        vii->reregister_paths = 1;
    }
    const vehicle_seat_index seat = hm->seat;
    if (vii->vvm.vp->info->vi_info->waypoints[wpi].seat_index == seat && seat != VSI_INVALID) {
        remove_human_from_seat(vii, human_index, seat);
    }
    return wpi;
}

// 0x00BABDA0 (:631)
static void remove_human_from_seat(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat)
{
    if (seat == VSI_INVALID) {
        return;
    }
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    if (hp != NULL) {
        human_combat_abandon_turret(hp);
    }
    if (hm->seat == seat) {
        if (seat == VSI_DRIVER) {
            vp->most_recent_driver_exit_time.set(0);
        }
        vp->m_seat_info[seat].m_current_occupant_handle = 0xFFFFFFFF;
        if (hp != NULL) {
            hp->vehicle_seat_idx = VSI_INVALID;
            human_set_cast_shadows(hp, true, true);
            human_set_cast_drop_shadows(hp, true, true);
        }
        hm->seat = VSI_INVALID;
    }
    if (hm->enter_data.destination_seat == seat) {
        mark_seat_reserved(vii, human_index, seat, VVM_WP_SEAT_FREE);
    }
}

// 0x00B81200 (:671)
static bool vi_allowed_to_exit(human* hp, vehicle* vp)
{
    if (hp->hflags.disallow_vehicle_exit != 0 || human_is_tied_up(hp)) {
        return false;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        // A walker on its mount with nothing under it has nowhere to put the human down. Original bug kept: the cast is
        // checked and a NULL walker would be read.
        walker* wp = vp->get_obj_type() == OT_VEHICLE ? (walker*)vp : NULL;
        if (wp->mount_vehicle == 0xFFFFFFFF && wp->ground_obj_handle == 0xFFFFFFFF) {
            return false;
        }
    }
    return true;
}

// 0x00BA1520 (:693)
static bool vi_door_open_action(vi_instance* vii, const unsigned int human_index, const vehicle_interaction_waypoint_transition* transition, const float open_time)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    const int component_idx = transition->component_obstacle_index;
    vehicle_component* component = &vp->rdd.get_data()->m_components[component_idx];
    if (component->get_type() != VCT_DOOR || (component->get_flags() & VCF_HINGED) == 0) {
        return true;
    }
    int door_idx = 0;
    bool found = false;
    if (hm->door_idx != -1 && vii->vvm.doors[hm->door_idx].component_id == component_idx) {
        found = true;
    }
    else {
        for (int i = 0; i < VI_MAX_DOORS; i++) {
            if (vii->vvm.doors[i].component_id == component_idx) {
                door_idx = i;
                hm->door_idx = i;
                found = true;
                break;
            }
        }
    }
    hm->door_comp_idx = transition->component_obstacle_index;
    if (found) {
        // Original quirk kept: with hm->door_idx already right, door_idx stays 0 below, so a door that is neither of these
        // two states restarts slot 0 and points this human at it.
        vi_door* door = &vii->vvm.doors[hm->door_idx];
        if (door->state == VI_DOOR_STATE_CLOSED_CLIMBING) {
            hm->door_idx = -1;
            hm->wait_ts.add_time(400);
            return false;
        }
        if (door->state == VI_DOOR_STATE_OPEN) {
            door->open_close_ts.set(0);
            return true;
        }
    }
    if ((component->get_flags() & VCF_DETACHED) == 0) {
        if ((component->get_flags() & VCF_UNLATCHED) == 0) {
            vehicle_component_unlatch_hinged(component, vp);
        }
        vehicle_component::door_attr& door_attr = component->m_type_attribute.door;
        const vehicle_component::adi_states state = door_attr.m_state;
        door_attr.m_t_velocity = 1.0f / open_time;
        if (state != vehicle_component::ADI_BEGIN_OPENING && state != vehicle_component::ADI_OPENING && state != vehicle_component::ADI_OPENED) {
            door_attr.m_state = vehicle_component::ADI_BEGIN_OPENING;
        }
    }
    if (transition->component_open_foley != -1 && (component->get_flags() & VCF_DETACHED) == 0 && component->m_type_attribute.door.m_t_actual < 0.04f) {
        audiolib_cue_play_3d(transition->component_open_foley, &vp->pos, NULL, NULL, NULL);
    }
    if (!found) {
        door_idx = 0;
        while (vii->vvm.doors[door_idx].component_id != -1) {
            door_idx++;
            if (door_idx >= VI_MAX_DOORS) {
                return false;
            }
        }
    }
    vi_door* door = &vii->vvm.doors[door_idx];
    door->state = VI_DOOR_STATE_OPEN;
    door->component_id = transition->component_obstacle_index;
    door->open_close_ts.set(0);
    door->wpi_check = hm->animate_from_wpi;
    hm->door_idx = door_idx;
    hm->manual_close_door_ts.set(2000);
    return true;
}

// 0x00BA1760 (:776)
static void vi_door_close_action(vi_instance* vii, const int door_idx, float close_time)
{
    vehicle* vp = vii->vvm.vp;
    if (door_idx == -1 || door_idx >= VI_MAX_DOORS) {
        return;
    }
    vi_door* door = &vii->vvm.doors[door_idx];
    if (door->component_id >= 0) {
        if (door->state == VI_DOOR_STATE_OPEN_FOR_GUNNER) {
            return;
        }
        vehicle_component_close_door(&vp->rdd.get_data()->m_components[door->component_id], vp, close_time);
    }
    door->initialize();
}

// 0x00BA17D0 (:800)
static bool vi_door_close_for_climb(vi_instance* vii, vi_human_manager* hm, const vehicle_interaction_waypoint_transition* transition, float close_time)
{
    const int component_idx = transition->component_obstacle_index;
    int door_idx = 0;
    while (door_idx < VI_MAX_DOORS && vii->vvm.doors[door_idx].component_id != component_idx) {
        door_idx++;
    }
    if (door_idx < VI_MAX_DOORS) {
        if (vii->vvm.doors[door_idx].state == VI_DOOR_STATE_CLOSED_CLIMBING) {
            vii->vvm.doors[door_idx].open_close_ts.set(2000);
            return true;
        }
        vi_door_close_action(vii, door_idx, close_time);
    }
    else {
        door_idx = 0;
        while (door_idx < VI_MAX_DOORS && vii->vvm.doors[door_idx].component_id != -1) {
            door_idx++;
        }
        if (door_idx == VI_MAX_DOORS) {
            return true;
        }
    }
    vi_door* door = &vii->vvm.doors[door_idx];
    door->component_id = component_idx;
    door->state = VI_DOOR_STATE_CLOSED_CLIMBING;
    door->open_close_ts.set(2000);
    hm->door_idx = door_idx;
    return false;
}

// 0x00BA1880 (:848)
// Costs each of the exit node types and picks vi_instance::exit_mode, the cheapest. Cost 0 means take it, 255 closed.
static void vi_update_exit_nodes(vi_instance* vii)
{
    if (vii->exit_mode != 0 && vii->updated_exits != 0) {
        return;
    }
    vehicle* vp = vii->vvm.vp;
    vii->updated_exits = 1;
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    float speed = 0.0f;
    if (!velocity.is_null()) {
        speed = velocity.mag_and_normalize();
    }
    bool on_ground = vp->contact_info.m_contact_list != NULL;
    if (!on_ground) {
        if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
            // Original bug kept: rfg.exe's cast can give NULL when the object type is wrong.
            walker* wp = vp->get_obj_type() == OT_VEHICLE ? (walker*)vp : NULL;
            if (wp->ground_obj_handle == 0xFFFFFFFF) {
                raycast_input in;
                in.radius = 0.0f;
                in.start = wp->pos;
                in.start.y += 0.2f;
                in.end = in.start;
                in.end.y = in.start.y - 2.0f;
                in.filter_params = NULL;
                in.filter_callback = NULL;
                in.m_filter_info = COLLIDE_RAY_DEFAULT;
                in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                in.m_flags = 0;
                raycast_output out;
                on_ground = havok_raycast(in, out);
            }
            else {
                on_ground = true;
            }
        }
        else {
            on_ground = vp->m_flags.m_wheels_on_ground != 0;
        }
    }
    farray<int,11> costs;
    for (int i = 0; i < 11; i++) {
        costs[i] = 0;
    }
    // 0.0042 and 0.15 of the 0-255 range, as the literals rfg.exe rounds at run time
    const int small_cost = fl_round(1.071f) & 0xFF;
    const int large_cost = small_cost + fl_round(38.25f);
    costs[0] = 0xFE;
    for (int i = 1; i <= 8; i++) {
        costs[i] = 0xFF;
    }
    costs[9] = 0;
    costs[10] = 0xFF;
    const float up_y = vp->orient.uvec.y;
    if (up_y > 0.25881904f) { // upright within 75 degrees
        costs[3] = 0;
        costs[4] = 0;
        costs[5] = 0;
    }
    else if (up_y < -0.70710677f) { // upside down within 45 degrees
        costs[6] = 0;
    }
    else {
        const float right_y = vp->orient.rvec.y;
        if (right_y > 0.70710677f) {
            costs[8] = 0;
        }
        else if (right_y < -0.70710677f) {
            costs[7] = 0;
        }
        else {
            costs[9] = 0;
        }
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        if (speed >= 12.517119f) { // 28 mph
            costs[3] += large_cost & 0xFF;
        }
        else {
            costs[4] += large_cost & 0xFF;
            costs[5] += large_cost & 0xFF;
            costs[9] += small_cost;
        }
    }
    if (speed > 1.0f) {
        costs[6] = 0xFF;
    }
    const float forward = vp->orient.fvec.y * velocity.y + velocity.x * vp->orient.fvec.x + vp->orient.fvec.z * velocity.z;
    if (forward > 0.70710677f) {
        costs[5] += small_cost;
    }
    else if (forward < -0.70710677f) {
        costs[4] += small_cost;
    }
    if (vp->m_flags.m_scripted_bail != 0) {
        for (int i = 3; i <= 8; i++) {
            costs[i] += large_cost & 0xFF;
        }
    }
    if (velocity.y * speed > 6.2585597f || !on_ground) { // climbing faster than 14 mph, or in the air
        for (int i = 3; i <= 8; i++) {
            costs[i] += small_cost;
        }
    }
    else if (vp->m_flags.m_on_fire == 0) {
        costs[9] += large_cost & 0xFF;
    }
    else {
        costs[9] += small_cost;
    }
    if (vehicle_is_bus(vp)) {
        if (costs[4] < costs[9]) {
            costs[9] = costs[4];
        }
        if (costs[5] < costs[9]) {
            costs[9] = costs[5];
        }
    }
    int best = 0;
    for (int i = 0; i < 11; i++) {
        if (costs[i] < 0xFF) {
            vii->exit_costs[i] = (unsigned char)costs[i];
            if (costs[i] < costs[best]) {
                best = i;
            }
        }
        else {
            vii->exit_costs[i] = 0xFF;
        }
    }
    if (best == vii->exit_mode) {
        return;
    }
    vii->process_state_again = 0;
    vii->exit_mode = best;
    int remaining = vii->num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        const vi_human_manager& hm = vii->humans[i];
        if (hm.handle != 0xFFFFFFFF) {
            remaining--;
            if (hm.interaction == VI_INTERACTION_EXIT && hm.state != VI_STATE_EXIT_PATHFIND) {
                vi_set_logic_state(vii, i, VI_STATE_EXIT_PATHFIND);
            }
        }
    }
}

// 0x00B685B0 (:1116)
static void vi_set_refresh_raycast_timer(vi_instance* vii)
{
    if (!vii->refresh_raycasts_timestamp.valid()) {
        vii->refresh_raycasts_timestamp.set(VI_RAYCAST_REFRESH_MS);
    }
}

// 0x00B81250 (:1125)
static void vi_refresh_raycast(vi_instance* vii)
{
    if (!vii->refresh_raycasts_timestamp.valid() || !vii->refresh_raycasts_timestamp.elapsed()) {
        return;
    }
    vii->refresh_raycasts_timestamp.invalidate();
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    for (int i = 0; i < info->num_waypoints; i++) {
        waypoint_usage& usage = vii->vvm.used_waypoints[i];
        if (info->waypoints[i].node_type > VI_NODE_TYPE_DOOR_ENTRY) {
            usage.block_type = VVM_WP_ENTRY_CHECK;
        }
        const unsigned char type = usage.block_type;
        if ((type == VVM_WP_SEAT_PF_FAILED || type == VVM_WP_ENTRY_CLEAR || type == VVM_WP_ENTRY_BLOCKED) && info->waypoints[i].node_type <= VI_NODE_TYPE_DOOR_ENTRY) {
            const int holder = usage.get_human_index();
            if (holder != -1) {
                const vi_human_manager& hm = vii->humans[holder];
                if (hm.interaction != VI_INTERACTION_EXIT && i == vi_utilities::waypoint_index_from_seat_index(vii->vvm.vp, hm.enter_data.destination_seat)) {
                    usage.block_type = VVM_WP_SEAT_RESERVED;
                    continue;
                }
            }
            usage.block_type = VVM_WP_SEAT_FREE;
        }
    }
}

// 0x00B8F590 (:1176)
// NPC corpses left in the seat's space fade out so the next human fits.
static void vi_clear_corpses(vi_instance* vii, int wpi)
{
    vehicle* vp = vii->vvm.vp;
    vector pos;
    vp->info->vi_info->get_waypoint_world_pos_orient(vp, wpi, pos, NULL, UP_VECTOR);
    pos.x += vp->orient.uvec.x * 0.35f;
    pos.y += vp->orient.uvec.y * 0.35f;
    pos.z += vp->orient.uvec.z * 0.35f;
    float radius = 0.35f;
    if (vii->vvm.vp->info->vi_info->waypoints[wpi].eject_on_explosion != 0) {
        // half a standing human's height (1.9) higher
        pos.x += vp->orient.uvec.x * 1.9f * 0.5f;
        pos.y += vp->orient.uvec.y * 1.9f * 0.5f;
        pos.z += vp->orient.uvec.z * 1.9f * 0.5f;
        radius = 0.55f;
    }
    unsigned int collected[8];
    const int count = havok_collect_ragdolls_fast(collected, 8, pos, radius);
    for (int i = 0; i < count; i++) {
        if (collected[i] == 0xFFFFFFFF) {
            continue;
        }
        object* objp = World.object_from_handle(collected[i]);
        if (objp != NULL && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC && objp->is_killed()) {
            ((npc*)objp)->fade_out_and_despawn(1000, false);
        }
    }
}

// 0x00B81350 (:1205)
// The cost of walking through a waypoint in the search: cheap when free, more as its holder is harder to move.
static float vi_human_pf_get_trans_node_weight(vi_instance* vii, const unsigned int human_index, int wpi)
{
    const waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
    switch (usage.block_type) {
    case VVM_WP_SEAT_FREE:
    case VVM_WP_ENTRY_CHECK:
    case VVM_WP_ENTRY_CLEAR:
        return 0.7f;
    case VVM_WP_SEAT_PF_FAILED:
        return vii->vvm.vp->info->vi_info->waypoints[wpi].node_type == VI_NODE_TYPE_DOOR_ENTRY ? VI_WEIGHT_BLOCKED : VI_WEIGHT_LAST_CHOICE;
    case VVM_WP_ENTRY_BLOCKED:
        return VI_WEIGHT_BLOCKED;
    default:
        break;
    }
    const int holder = usage.get_human_index();
    if (holder == (int)human_index || holder == -1) {
        return 0.7f;
    }
    const vi_human_manager& hm = vii->humans[holder];
    if (hm.handle == 0xFFFFFFFF) {
        vii->reregister_paths = 1;
        return 0.7f;
    }
    if (hm.interaction == VI_INTERACTION_RESERVE || hm.interaction == VI_INTERACTION_EXIT) {
        return 5.0f;
    }
    if (hm.interaction == VI_INTERACTION_RIDE) {
        human* hp = World.object_from_handle<human>(hm.handle);
        return hp->hflags.disallow_vehicle_exit == 0 && !object_is_player(hp) ? 100.0f : VI_WEIGHT_BLOCKED;
    }
    switch (hm.state) {
    case VI_STATE_RESERVED_SEAT:
        return 1.7f;
    case VI_STATE_ENTER_PATHFIND:
        return 10.0f;
    case VI_STATE_ENTER_TELEPORT_WAIT:
        return 25.0f;
    default:
        break;
    }
    human* hp = World.object_from_handle<human>(hm.handle);
    if (hm.cur_path_idx == -1) {
        return 50.0f;
    }
    if (!object_is_player(hp) && hp->hflags.disallow_vehicle_exit == 0 && hp->hflags.limited_vehicle_exit == 0) {
        return 100.0f;
    }
    return VI_WEIGHT_BLOCKED;
}

// 0x00B8F7E0 (:1292)
// The cost of ending the search on a waypoint: its distance when entering, the exit_costs and the stick when exiting.
static float vi_human_pf_get_final_node_weight(vi_instance* vii, const unsigned int human_index, int wpi)
{
    vi_human_manager* hm = &vii->humans[human_index];
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    human* hp = vi_human_from_handle(hm->handle);
    const vi_interaction interaction = hm->interaction;
    vehicle* vp = vii->vvm.vp;
    if (interaction == VI_INTERACTION_ENTER && hm->effective_curr_wpi() == wpi) {
        return 0.0f;
    }
    const vehicle_interaction_waypoint& wp = info->waypoints[wpi];
    const unsigned int node_type = wp.node_type;
    if (node_type == VI_NODE_TYPE_DOOR_ENTRY || node_type == VI_NODE_TYPE_ENTRY) {
        const bool door_entry = node_type == VI_NODE_TYPE_DOOR_ENTRY;
        if (interaction != VI_INTERACTION_ENTER) {
            return VI_WEIGHT_BLOCKED;
        }
        vector pos;
        info->get_waypoint_world_pos_orient(vp, wpi, pos, NULL, UP_VECTOR);
        const float dx = pos.x - hp->pos.x;
        const float dy = pos.y - hp->pos.y;
        const float dz = pos.z - hp->pos.z;
        const float dist_squared = dx * dx + dy * dy + dz * dz;
        if (wp.short_range != 0 && dist_squared > 3.2f) {
            return VI_WEIGHT_BLOCKED;
        }
        if (door_entry || wp.restrict_vertical_access != 0) {
            vector local = hp->pos;
            vec_change_to_local_coords(local, pos, vp->orient);
            const float height = -local.y;
            if (height > 0.7f) {
                return VI_WEIGHT_BLOCKED;
            }
            if (door_entry && (height > 0.7f || height < -0.2f || local.x * local.x + local.z * local.z > 0.5f)) {
                return VI_WEIGHT_BLOCKED;
            }
        }
        if (wp.last_choice != 0 && !door_entry) {
            return VI_WEIGHT_LAST_CHOICE;
        }
        if (dist_squared < 0.5f) {
            return 0.5f;
        }
        return sqrtf(dist_squared);
    }
    if (node_type < VI_NODE_TYPE_EXIT_FIRST || node_type > VI_NODE_TYPE_EXIT_CORPSE || interaction != VI_INTERACTION_EXIT) {
        return VI_WEIGHT_BLOCKED;
    }
    float weight = (float)((double)vii->exit_costs[node_type] * 1000.0 / 255.0);
    if (object_is_local_player(hp)) {
        // the player's stick prefers the exit it points at
        vector stick(controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index), 0.0f, 0.0f);
        stick.z = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
        if (stick.x * stick.x + stick.z * stick.z > 0.48999998f) {
            vector to_node;
            info->get_waypoint_world_pos_orient(vp, wpi, to_node, NULL, UP_VECTOR);
            to_node.x -= hp->pos.x;
            to_node.y -= hp->pos.y;
            to_node.z -= hp->pos.z;
            to_node.normalize_safe();
            vec_change_to_local_coords(to_node, ZERO_VECTOR, hp->orient);
            stick.normalize();
            const float dot = to_node.x * stick.x + to_node.y * stick.y + to_node.z * stick.z;
            weight += (1.0f - dot) * 0.7f * 3.0f;
        }
    }
    if (hp != NULL && hp->get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)hp)->npc_flags.use_corpse_vehicle_exit != 0) {
        return node_type == VI_NODE_TYPE_EXIT_CORPSE ? 0.7f : VI_WEIGHT_BLOCKED;
    }
    if (node_type == VI_NODE_TYPE_EXIT_ROOF) {
        vector pos;
        matrix orient;
        info->get_waypoint_world_pos_orient(vp, wpi, pos, &orient, UP_VECTOR);
        const float dot = orient.fvec.x * UP_VECTOR.x + orient.fvec.y * UP_VECTOR.y + orient.fvec.z * UP_VECTOR.z;
        weight += (1.0f - dot) * 0.7f;
    }
    else if (hm->exit_data.flags.force_dive_exit != 0) {
        return VI_WEIGHT_BLOCKED;
    }
    return wp.last_choice != 0 ? weight + VI_WEIGHT_LAST_CHOICE : weight + 0.7f;
}

// 0x00B685E0 (:1431)
bool vi_collection_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp == NULL) {
        return true;
    }
    if (hit_objp->get_obj_type() == OT_HUMAN || character_controller_should_push_away(hit_objp, false)) {
        return false;
    }
    while (hit_objp != NULL) {
        if (hit_objp == user_data) {
            return false;
        }
        hit_objp = World.object_from_handle(hit_objp->get_parent());
    }
    return true;
}

// 0x00B68630 (:1455)
static bool stick_to_floor_raycast_callback_vehicle_interaction(object* obj, void* param)
{
    return obj->get_obj_type() != OT_HUMAN && obj->get_obj_type() != OT_ITEM;
}

// 0x00B97F60 (:1465)
// Is the space by an exit node free? Returns true when blocked, and caches the answer in the waypoint's block_type.
static bool vi_human_pf_check_raycast(vi_instance* vii, int wpi, int prev_wpi)
{
    const unsigned int node_type = vii->vvm.vp->info->vi_info->waypoints[wpi].node_type;
    if (node_type <= VI_NODE_TYPE_DOOR_ENTRY) {
        return false;
    }
    waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
    switch (usage.block_type) {
    case VVM_WP_SEAT_PF_FAILED:
    case VVM_WP_ENTRY_CLEAR:
        return false;
    case VVM_WP_ENTRY_BLOCKED:
        return true;
    case VVM_WP_TRANS_RESERVED:
    case VVM_WP_SEAT_FREE:
    case VVM_WP_ENTRY_CHECK:
        break;
    default:
        usage.block_type = VVM_WP_ENTRY_CHECK;
        break;
    }
    vi_set_refresh_raycast_timer(vii);
    vehicle* vp = vii->vvm.vp;
    vector node_pos;
    matrix node_orient;
    vp->info->vi_info->get_waypoint_world_pos_orient(vp, wpi, node_pos, &node_orient, UP_VECTOR);
    vector prev_pos;
    vp->info->vi_info->get_waypoint_world_pos_orient(vp, prev_wpi, prev_pos, NULL, UP_VECTOR);
    if (node_type == VI_NODE_TYPE_EXIT_GROUND) {
        prev_pos.y += 0.5f;
    }
    else {
        prev_pos.x += vp->orient.uvec.x * 0.5f;
        prev_pos.y += vp->orient.uvec.y * 0.5f;
        prev_pos.z += vp->orient.uvec.z * 0.5f;
    }
    vector probe = node_pos;
    if (vp->orient.uvec.y <= 0.25881904f) { // over 75 degrees from upright
        probe.y += 0.5f;
    }
    else {
        const vector& up = vp->orient.uvec;
        float along_up = up.y * (prev_pos.y - node_pos.y) + up.x * (prev_pos.x - node_pos.x) + up.z * (prev_pos.z - node_pos.z);
        if (along_up - 0.5f < 0.0f) {
            along_up = 0.5f;
        }
        if (along_up - 0.9f >= 0.0f) {
            along_up = 0.9f;
        }
        probe.x = up.x * along_up + node_pos.x;
        probe.y = node_pos.y + up.y * along_up;
        probe.z = node_pos.z + up.z * along_up;
    }
    float terrain_y;
    if (havok_terrain_height(terrain_y, probe.x, probe.z, false, NULL)) {
        if (probe.y < terrain_y) {
            usage.block_type = VVM_WP_ENTRY_BLOCKED;
            return true;
        }
        if (probe.y < terrain_y + 0.5f) {
            probe.y = terrain_y + 0.5f;
        }
    }
    if (node_type > VI_NODE_TYPE_ENTRY) {
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        if (velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z > 156.67828f) { // 28 mph squared
            usage.block_type = VVM_WP_ENTRY_CLEAR;
            return false;
        }
    }
    unsigned int collected;
    if (node_type == VI_NODE_TYPE_EXIT_ROOF) {
        const vector capsule_start(node_orient.uvec.x * 0.5f + prev_pos.x, node_orient.uvec.y * 0.5f + prev_pos.y, node_orient.uvec.z * 0.5f + prev_pos.z);
        const float radius = 0.5f;
        const int hits = havok_collect_objects_in_capsule(capsule_start, node_pos, radius, &collected, 1, COLLIDE_RAY_VEHICLE_ENTER_EXIT, vi_collection_callback, vp);
        usage.block_type = hits != 0 ? VVM_WP_ENTRY_BLOCKED : VVM_WP_ENTRY_CLEAR;
        return hits != 0;
    }
    // down to the floor from the probe, 0.4 of a standing height (1.9) under the node
    raycast_input in;
    in.radius = 0.15f;
    in.start = probe;
    in.end.x = node_pos.x - node_orient.uvec.x * 1.9f * 0.4f;
    in.end.y = node_pos.y - node_orient.uvec.y * 1.9f * 0.4f;
    in.end.z = node_pos.z - node_orient.uvec.z * 1.9f * 0.4f;
    in.filter_params = NULL;
    in.filter_callback = stick_to_floor_raycast_callback_vehicle_interaction;
    in.m_filter_info = COLLIDE_RAY_WALKING_SURFACE_ONLY;
    in.m_flags = 0;
    in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    vector floor_pos = node_pos;
    bool found_floor = false;
    {
        raycast_output floor_hits;
        if (havok_spherecast(in, floor_hits)) {
            for (int i = 0; i < floor_hits.m_hits.getSize(); i++) {
                object* objp = World.object_from_handle(floor_hits.m_hits[i].hit_object_handle);
                if (objp != NULL && objp->get_obj_type() != OT_HUMAN) {
                    // Original quirk kept: always the first hit's centre, whichever hit passed the test.
                    floor_pos = floor_hits.m_hits[0].hit_center_pos;
                    if (!vec_equal_fast(floor_hits.m_hits[0].hit_center_pos, in.start, 0.01f)) {
                        found_floor = true;
                    }
                    break;
                }
            }
        }
        else {
            floor_pos = in.end;
            found_floor = false;
        }
    }
    // a standing human's capsule on that floor
    const vector stand_start(node_orient.uvec.x * 0.77f + floor_pos.x, node_orient.uvec.y * 0.77f + floor_pos.y, node_orient.uvec.z * 0.77f + floor_pos.z);
    const vector stand_end(node_orient.uvec.x * 1.4f + floor_pos.x, node_orient.uvec.y * 1.4f + floor_pos.y, node_orient.uvec.z * 1.4f + floor_pos.z);
    const float stand_radius = 0.3f;
    if (havok_collect_objects_in_capsule(stand_start, stand_end, stand_radius, &collected, 1, COLLIDE_RAY_VEHICLE_ENTER_EXIT, vi_collection_callback, vp) == 0) {
        bool clear = true;
        if (node_type > VI_NODE_TYPE_ENTRY) {
            raycast_input path_in;
            path_in.start = prev_pos;
            path_in.radius = 0.35f;
            path_in.end = probe;
            path_in.filter_callback = vehicle_raycast_callback;
            path_in.m_filter_info = COLLIDE_RAY_VEHICLE_ENTER_EXIT;
            path_in.m_flags = 0;
            path_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
            path_in.filter_params = vp;
            raycast_output path_out;
            clear = !havok_spherecast(path_in, path_out);
        }
        if (clear) {
            if (found_floor) {
                usage.block_type = VVM_WP_ENTRY_CLEAR;
                return false;
            }
            // nothing to stand on: only usable as a last resort, checked again later
            usage.block_type = VVM_WP_SEAT_PF_FAILED;
            vi_set_refresh_raycast_timer(vii);
            return false;
        }
    }
    usage.block_type = VVM_WP_ENTRY_BLOCKED;
    return true;
}

// 0x00BABE60 (:1728)
// Searches back from the wanted interior waypoint over the table's transitions (from the seat to the door when
// entering, the other way when exiting) and writes the cheapest route to hm->path_nodes.
static bool vi_human_pathfind_to_interior(vi_instance* vii, const unsigned int human_index, int interior_wpi)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    const vehicle_interaction_info* info = vp->info->vi_info;
    if (info == NULL) {
        return false;
    }
    if (vii->reregister_paths != 0) {
        vi_register_paths(vii);
    }
    const bool entering = hm->interaction == VI_INTERACTION_ENTER;
    int best = -1;
    const int curr_wpi = hm->effective_curr_wpi();
    if (entering && curr_wpi == interior_wpi) {
        hm->path_nodes[0] = (signed char)curr_wpi;
        hm->path_nodes[1] = -1;
        return true;
    }
    if (curr_wpi == -1) {
        hm->auto_connect_to_entry = 0;
    }
    farray<waypoint_node,50> nodes;
    nodes[0].wpi = interior_wpi;
    nodes[0].weight = 0.0f;
    int count = 1;
    nodes[0].ext_weight = vi_human_pf_get_final_node_weight(vii, human_index, nodes[0].wpi) + nodes[0].weight;
    nodes[0].dest_ni = -1;
    int ni = 0;
    do {
        if (nodes[ni].weight < VI_WEIGHT_BLOCKED) {
            nodes[ni].ext_weight = vi_human_pf_get_final_node_weight(vii, human_index, nodes[ni].wpi) + nodes[ni].weight;
            bool candidate;
            if (!entering) {
                candidate = nodes[ni].ext_weight < VI_WEIGHT_BLOCKED;
            }
            else {
                candidate = (curr_wpi == -1 && nodes[ni].ext_weight < VI_WEIGHT_BLOCKED) || curr_wpi == nodes[ni].wpi;
            }
            if (candidate && (best == -1 || nodes[ni].ext_weight < nodes[best].ext_weight)) {
                best = ni;
            }
            for (int t = 0; t < info->num_waypoint_transitions; t++) {
                const vehicle_interaction_waypoint_transition& transition = info->transitions[t];
                const int from = entering ? transition.destination_index : transition.index;
                const int to = entering ? transition.index : transition.destination_index;
                if (from != nodes[ni].wpi) {
                    continue;
                }
                int found = -1;
                for (int j = 0; j < count; j++) {
                    if (nodes[j].wpi == to) {
                        found = j;
                        break;
                    }
                }
                const float weight = vi_human_pf_get_trans_node_weight(vii, human_index, to) + nodes[ni].weight;
                if (weight < VI_WEIGHT_BLOCKED) {
                    if (found == -1) {
                        if (count >= VI_MAX_SEARCH_NODES) {
                            return false;
                        }
                        nodes[count].wpi = to;
                        nodes[count].dest_ni = ni;
                        nodes[count].weight = weight;
                        count++;
                    }
                    else if (weight < nodes[found].weight) {
                        nodes[found].weight = weight;
                        nodes[found].dest_ni = ni;
                    }
                }
            }
            const int node_wpi = nodes[ni].wpi;
            if (entering && info->waypoints[node_wpi].node_type == VI_NODE_TYPE_ENTRY) {
                vector entry_pos;
                info->get_waypoint_world_pos_orient(vii->vvm.vp, node_wpi, entry_pos, NULL, UP_VECTOR);
                bool connected = false;
                if (hm->auto_connect_to_entry != 0 && vec_dist_squared(hp->pos, entry_pos) < 1.5f) {
                    const float weight = vec_dist_squared(hp->pos, entry_pos) + nodes[ni].weight;
                    if (weight < VI_WEIGHT_BLOCKED) {
                        if (count >= VI_MAX_SEARCH_NODES) {
                            return false;
                        }
                        nodes[count].wpi = curr_wpi;
                        nodes[count].dest_ni = ni;
                        nodes[count].weight = weight;
                        count++;
                    }
                    connected = true;
                }
                if (!connected) {
                    for (int j = 0; j < info->num_waypoints; j++) {
                        if (j != node_wpi && info->is_auto_connected_transition_active(vp, j, node_wpi, &entry_pos)) {
                            const float weight = nodes[ni].weight + 1.0f;
                            if (weight < VI_WEIGHT_BLOCKED) {
                                if (count >= VI_MAX_SEARCH_NODES) {
                                    return false;
                                }
                                nodes[count].wpi = j;
                                nodes[count].dest_ni = ni;
                                nodes[count].weight = weight;
                                count++;
                            }
                            break;
                        }
                    }
                }
            }
        }
        ni++;
    } while (ni < count);
    if (best == -1) {
        return false;
    }
    for (;;) {
        const int wpi = nodes[best].wpi;
        vector pos;
        info->get_waypoint_world_pos_orient(vp, wpi, pos, NULL, UP_VECTOR);
        bool accept = vec_xz_dist_squared(pos, hp->pos) < 0.36f && fabsf((pos.y + 0.3f) - hp->pos.y) < 0.6f;
        if (!accept) {
            waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
            if (usage.block_type == VVM_WP_ENTRY_CHECK) {
                // Original bug kept: for the search's first node dest_ni is -1, which reads before the array.
                vi_human_pf_check_raycast(vii, wpi, nodes.elements()[nodes[best].dest_ni].wpi);
                switch (usage.block_type) {
                case VVM_WP_TRANS_RESERVED:
                case VVM_WP_SEAT_RESERVED:
                case VVM_WP_ENTRY_BLOCKED:
                    nodes[best].ext_weight = 1001.0f;
                    break;
                case VVM_WP_SEAT_FREE:
                case VVM_WP_ENTRY_CLEAR:
                    accept = true;
                    break;
                case VVM_WP_SEAT_PF_FAILED:
                    nodes[best].ext_weight += VI_WEIGHT_LAST_CHOICE;
                    break;
                default:
                    return false;
                }
            }
            else {
                switch (usage.block_type) {
                case VVM_WP_TRANS_RESERVED:
                case VVM_WP_SEAT_RESERVED:
                case VVM_WP_ENTRY_BLOCKED:
                    if (wpi == curr_wpi || usage.get_human_index() == (int)human_index) {
                        accept = true;
                    }
                    else {
                        nodes[best].ext_weight = 1001.0f;
                    }
                    break;
                case VVM_WP_SEAT_FREE:
                case VVM_WP_SEAT_PF_FAILED:
                case VVM_WP_ENTRY_CLEAR:
                    accept = true;
                    break;
                default:
                    return false;
                }
            }
        }
        if (accept) {
            int path_len = 0;
            // Original bug kept: a route over 6 nodes overruns path_nodes.
            while (best >= 0) {
                hm->path_nodes[path_len] = (signed char)nodes[best].wpi;
                best = nodes[best].dest_ni;
                path_len++;
            }
            hm->last_path_idx = (signed char)(path_len - 1);
            for (; path_len < VI_MAX_PATH_NODES; path_len++) {
                hm->path_nodes[path_len] = -1;
            }
            vii->reregister_paths = 1;
            return true;
        }
        // That node was blocked: the next cheapest that can end the search.
        best = -1;
        for (int j = 0; j < count; j++) {
            const float ext = nodes[j].ext_weight;
            if (ext >= VI_WEIGHT_BLOCKED) {
                continue;
            }
            bool candidate;
            if (!entering) {
                candidate = info->waypoints[nodes[j].wpi].node_type > VI_NODE_TYPE_ENTRY;
            }
            else if (curr_wpi == -1) {
                candidate = info->waypoints[nodes[j].wpi].node_type == VI_NODE_TYPE_ENTRY;
            }
            else {
                candidate = curr_wpi == nodes[j].wpi;
            }
            if (candidate && (best == -1 || ext < nodes[best].ext_weight)) {
                best = j;
            }
        }
        if (best == -1 || nodes[best].ext_weight > VI_WEIGHT_BLOCKED) {
            return false;
        }
    }
}

// 0x00BB63B0 (:2089)
static bool vi_human_path_to_seat(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (seat == VSI_INVALID) {
        return false;
    }
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    if (info == NULL) {
        return false;
    }
    int wpi = 0;
    while (info->waypoints[wpi].seat_index != seat) {
        wpi++;
        if (wpi >= info->num_waypoints) {
            return false;
        }
    }
    signed char first_wpi = -1;
    signed char effective_wpi = -1;
    if (hm->cur_path_idx >= 0) {
        first_wpi = hm->path_nodes[hm->cur_path_idx];
        effective_wpi = hm->path_nodes[hm->effective_path_idx()];
    }
    if (!vi_human_pathfind_to_interior(vii, human_index, wpi)) {
        if (hm->cur_path_idx == -1) {
            human_set_state(vi_human_from_handle(hm->handle), HS_STANDARD, HMS_IDLE, -1.0f);
        }
        return false;
    }
    if (effective_wpi != -1) {
        if (hm->state_of_anim == VI_STATE_SHARED_TRANSITION || hm->path_nodes[1] == -1) {
            if (hm->reserve_next_node_only == 0) {
                if (hm->animate_dest_wpi != hm->path_nodes[1]) {
                    set_anim_cancel(vii, human_index);
                }
            }
            else if (hm->path_nodes[0] != first_wpi) {
                for (int i = VI_MAX_PATH_NODES - 1; i > 0; i--) {
                    hm->path_nodes[i] = hm->path_nodes[i - 1];
                }
                hm->path_nodes[0] = first_wpi;
                hm->last_path_idx++;
            }
        }
        hm->cur_path_idx = 0;
    }
    hm->wait_ts.set(3000);
    set_anim_process_continue(vii, human_index);
    return true;
}

// 0x00BB6530 (:2161)
static bool vi_human_path_to_exit(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->state == VI_STATE_EXIT_EJECT) {
        return true;
    }
    hm->exit_data.flags.maintain_position = vi_should_maintain_pos_on_exit(vii, human_index) ? 1 : 0;
    vi_update_exit_nodes(vii);
    if (hm->handle == 0xFFFFFFFF) {
        return false;
    }
    if (vii->exit_mode == 0) {
        hm->exit_data.flags.maintain_position = 0;
    }
    else if (hm->exit_data.flags.maintain_position == 0 && hm->state != VI_STATE_ENTER_TELEPORT_WAIT) {
        if (!vi_human_pathfind_to_interior(vii, human_index, hm->effective_anim_wpi())) {
            return false;
        }
        int last = 0;
        while (last < VI_MAX_PATH_NODES - 1 && hm->path_nodes[last + 1] != -1) {
            last++;
        }
        if (hm->animate_from_wpi == hm->path_nodes[last] && hm->reserve_next_node_only == 0) {
            set_anim_cancel(vii, human_index);
        }
        else if (last + 1 < VI_MAX_PATH_NODES) {
            last++;
            hm->path_nodes[last] = hm->animate_from_wpi;
        }
        hm->last_path_idx = (signed char)last;
        if (last + 1 < VI_MAX_PATH_NODES) {
            hm->path_nodes[last + 1] = -1;
        }
        // the search ran from the interior: walk it the other way
        for (int i = 0, j = last; i < j; i++, j--) {
            const signed char swap = hm->path_nodes[j];
            hm->path_nodes[j] = hm->path_nodes[i];
            hm->path_nodes[i] = swap;
        }
        hm->cur_path_idx = 0;
        hm->wait_ts.set(3000);
        set_anim_process_continue(vii, human_index);
        return true;
    }
    vi_exit_teleport(vii, human_index);
    return false;
}

// 0x00B814E0 (:2290)
static bool vi_request_npc_exit(vi_instance* vii, int human_index, int requester_hmi)
{
    const unsigned int handle = vii->humans[human_index].handle;
    if (handle == 0xFFFFFFFF) {
        return false;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == NULL || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC) {
        return false;
    }
    npc* np = (npc*)objp;
    human* requester = requester_hmi >= 0 ? vi_human_from_handle(vii->humans[requester_hmi].handle) : NULL;
    if (np->hflags.disallow_vehicle_exit != 0) {
        return false;
    }
    np->npc_flags.force_out_of_vehicle = 1;
    const vector backward(-np->orient.fvec.x, -np->orient.fvec.y, -np->orient.fvec.z);
    np->request_avoidance(requester, backward, ZERO_VECTOR);
    const vehicle_seat_index seat = vii->humans[human_index].enter_data.destination_seat;
    if (seat != VSI_INVALID) {
        mark_seat_reserved(vii, human_index, seat, VVM_WP_SEAT_FREE);
    }
    return true;
}

// 0x00BA1D90 (:2331)
static bool mark_seat_reserved(vi_instance* vii, const unsigned int human_index, vehicle_seat_index seat, vvm_waypoint_blocked_type mark)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    if (seat == VSI_INVALID) {
        return false;
    }
    const int wpi = vi_utilities::waypoint_index_from_seat_index(vp, seat);
    if (wpi == -1) {
        return false;
    }
    if (mark == VVM_WP_SEAT_RESERVED) {
        if (object_is_player(hp)) {
            // The player takes a seat by force: the others get out of it, or on a bus scatter a moment later.
            bool hijack = vp->m_flags.m_hijacked != 0;
            if (!hijack && vii->vvm.used_waypoints[wpi].block_type == VVM_WP_SEAT_RESERVED) {
                int remaining = vii->num_humans;
                for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
                    if (vii->humans[i].handle == 0xFFFFFFFF) {
                        continue;
                    }
                    remaining--;
                    if (vii->humans[i].enter_data.destination_seat == seat) {
                        hijack = vii->humans[i].interaction == VI_INTERACTION_RESERVE;
                        break;
                    }
                }
            }
            if (hijack) {
                const bool is_bus = vehicle_is_bus(vp);
                int scatter_ms = int_rand_range(400, 1500) + 5000;
                int remaining = vii->num_humans;
                for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
                    vi_human_manager& other = vii->humans[i];
                    if (other.handle == 0xFFFFFFFF) {
                        continue;
                    }
                    remaining--;
                    human* other_hp = vi_human_from_handle(other.handle);
                    if ((unsigned int)i != human_index && !npc_is_defending_player(other_hp)) {
                        other.being_hijacked = -1; // signed 1-bit field: -1 is the set bit
                        if (other.enter_data.destination_seat == seat) {
                            vi_request_npc_exit(vii, i, human_index);
                        }
                        else if (is_bus) {
                            other.exit_data.flags.force_dive_exit = 1;
                            other_hp->hflags.disallow_vehicle_exit = 1;
                            other_hp->avoidance_check_timer.set(scatter_ms);
                            scatter_ms += int_rand_range(200, 1500);
                        }
                    }
                }
            }
        }
        vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(vii->vvm.vp, seat);
        if (mount != NULL && mount->attached_turret != 0xFFFFFFFF) {
            object* objp = World.object_from_handle(mount->attached_turret);
            if (objp != NULL && objp->get_obj_type() == OT_TURRET) {
                human_combat_reserve_turret(hp, (turret*)objp);
            }
        }
        if (vii->vvm.used_waypoints[wpi].block_type != VVM_WP_SEAT_FREE) {
            return false;
        }
    }
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    if (info->waypoints[wpi].restrict_vertical_access != 0) {
        // the waypoint leading up to the seat goes with it
        for (int t = 0; t < info->num_waypoint_transitions; t++) {
            if (info->transitions[t].destination_index == wpi) {
                const int prev = info->transitions[t].index;
                if (mark == VVM_WP_SEAT_RESERVED) {
                    vii->vvm.used_waypoints[prev].block_type = VVM_WP_TRANS_RESERVED;
                }
                else if (info->waypoints[prev].node_type <= VI_NODE_TYPE_DOOR_ENTRY) {
                    vii->vvm.used_waypoints[prev].block_type = mark;
                }
                else {
                    vii->vvm.used_waypoints[prev].block_type = VVM_WP_ENTRY_CHECK;
                }
                break;
            }
        }
    }
    if (mark == VVM_WP_SEAT_RESERVED) {
        remove_human_from_seat(vii, human_index, hm->enter_data.destination_seat);
        hp->reserved_vehicle_handle = vii->vvm.vp->get_handle();
        hm->enter_data.destination_seat = seat;
        hm->enter_data.specified_seat = seat;
        if (object_is_player(hp)) {
            vp->m_flags.m_hijacked = 0;
        }
    }
    else if (hm->enter_data.destination_seat == seat) {
        hm->enter_data.destination_seat = VSI_INVALID;
        if (hp != NULL) {
            hp->reserved_vehicle_handle = 0xFFFFFFFF;
        }
    }
    vii->vvm.used_waypoints[wpi].block_type = mark;
    if (mark == VVM_WP_SEAT_PF_FAILED) {
        vi_set_refresh_raycast_timer(vii);
    }
    return true;
}

// 0x00BA2170 (:2501)
static bool vi_seat_reserve(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (vi_human_from_handle(hm->handle) == NULL) {
        return false;
    }
    vehicle* vp = vii->vvm.vp;
    // Recount the seat waypoints from who wants which seat.
    farray<int,8> wanted;
    int remaining = vii->num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        if (vii->humans[i].handle != 0xFFFFFFFF) {
            remaining--;
            if (vii->humans[i].enter_data.destination_seat != VSI_INVALID) {
                const int seat = vii->humans[i].enter_data.destination_seat;
                wanted.add(seat);
            }
        }
    }
    const vehicle_interaction_info* info = vp->info->vi_info;
    for (int i = 0; i < info->num_waypoints; i++) {
        const int seat = info->waypoints[i].seat_index;
        if (seat == VSI_INVALID) {
            continue;
        }
        int found = -1;
        for (int j = 0; j < wanted.size(); j++) {
            if (wanted[j] == seat) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            wanted.remove_unsorted(found);
            vii->vvm.used_waypoints[i].block_type = VVM_WP_SEAT_RESERVED;
        }
        else if (vii->vvm.used_waypoints[i].block_type == VVM_WP_SEAT_RESERVED) {
            vii->vvm.used_waypoints[i].block_type = VVM_WP_SEAT_FREE;
        }
    }
    vehicle_seat_index seat = (vehicle_seat_index)Vehicle_enter_seat_override;
    if (Vehicle_enter_seat_override < 0) {
        if (hm->enter_data.seat_selection_method != VI_SEAT_SELECTION_METHOD_SPECIFIED) {
            int limit = vehicle_get_num_seats(vp);
            // Original quirk kept: a human with a seat only looks at the seats before it.
            if (hm->enter_data.destination_seat != VSI_INVALID) {
                limit = hm->enter_data.destination_seat;
            }
            for (int s = 0; s < limit; s++) {
                if (mark_seat_reserved(vii, human_index, (vehicle_seat_index)s, VVM_WP_SEAT_RESERVED)) {
                    return true;
                }
            }
            return false;
        }
        seat = hm->enter_data.specified_seat;
        if (seat == VSI_INVALID) {
            return false;
        }
        if (hm->enter_data.destination_seat == seat) {
            return true;
        }
    }
    return mark_seat_reserved(vii, human_index, seat, VVM_WP_SEAT_RESERVED);
}

// 0x00B815E0 (:2581)
static bool vi_node_dep_free(const vi_instance* vii, int wpi, int hmi)
{
    for (;;) {
        const waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
        if (usage.is_path_reserved() && usage.get_human_index() != hmi) {
            return false;
        }
        const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
        if (info->waypoints[wpi].seat_index == VSI_INVALID || info->waypoints[wpi].restrict_vertical_access == 0) {
            return true;
        }
        int t = 0;
        while (t < info->num_waypoint_transitions && info->transitions[t].destination_index != wpi) {
            t++;
        }
        if (t >= info->num_waypoint_transitions) {
            return true;
        }
        wpi = info->transitions[t].index;
    }
}

// 0x00B81670 (:2603)
static void vi_register_node(vi_instance* vii, int hmi, int wpi, farray<int,44>& waypoint_in_use, const vehicle_interaction_info* vi_info, bool lock)
{
    while (waypoint_in_use[wpi] == -1) {
        if (lock) {
            waypoint_in_use[wpi] = hmi;
        }
        waypoint_usage& usage = vii->vvm.used_waypoints[wpi];
        usage.reg_on_release = usage.is_path_reserved() ? 1 : 0;
        usage.set_human_index(hmi);
        if (vi_info->waypoints[wpi].seat_index == VSI_INVALID || vi_info->waypoints[wpi].restrict_vertical_access == 0) {
            return;
        }
        int t = 0;
        while (t < vi_info->num_waypoint_transitions && vi_info->transitions[t].destination_index != wpi) {
            t++;
        }
        if (t >= vi_info->num_waypoint_transitions) {
            return;
        }
        wpi = vi_info->transitions[t].index;
    }
}

// 0x00BA2390 (:2635)
// Rebuilds who holds which waypoint: first what each human stands on, then the enterers' routes forward (reserving
// without locking, later humans first), then the exiters', which take priority over waypoints not yet locked.
static void vi_register_paths(vi_instance* vii)
{
    if (vii->reregister_paths == 0) {
        return;
    }
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    vii->reregister_paths = 0;
    farray<int,8> progress;
    farray<int,44> in_use;
    for (int i = 0; i < VI_MAX_WAYPOINTS; i++) {
        vii->vvm.used_waypoints[i].set_free();
        vii->vvm.used_waypoints[i].reg_on_release = 0;
        in_use[i] = -1;
    }
    if (vii->num_humans == 0) {
        return;
    }
    int remaining = vii->num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        vi_human_manager& hm = vii->humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        progress[i] = -1;
        if (hm.state == VI_STATE_ENTER_STREAMING_STALL) {
            continue;
        }
        bool on_path = false;
        for (int k = 0; k < VI_MAX_PATH_NODES; k++) {
            const signed char node = hm.path_nodes[k];
            if (node < 0) {
                break;
            }
            const bool at_from = node == hm.animate_from_wpi;
            if (at_from && hm.reserve_next_node_only == 0) {
                on_path = true;
            }
            if (on_path) {
                vi_register_node(vii, i, node, in_use, info, true);
                progress[i] = k;
            }
            if (at_from && hm.reserve_next_node_only != 0) {
                on_path = true;
            }
            if (node == hm.animate_dest_wpi) {
                break;
            }
        }
    }
    remaining = vii->num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        vi_human_manager& hm = vii->humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        if (hm.interaction == VI_INTERACTION_EXIT) {
            progress[i]++;
        }
        else if (hm.interaction == VI_INTERACTION_ENTER) {
            for (int k = progress[i] + 1; k < VI_MAX_PATH_NODES; k = progress[i] + 1) {
                const int node = hm.path_nodes[k];
                if (node == -1) {
                    break;
                }
                if (in_use[node] != -1 && in_use[node] != i) {
                    vii->vvm.used_waypoints[node].reg_on_release = 1;
                    const int holder = vii->vvm.used_waypoints[node].get_human_index();
                    if (vii->humans[holder].interaction == VI_INTERACTION_RIDE) {
                        vi_request_npc_exit(vii, holder, i);
                    }
                    break;
                }
                progress[i] = k;
            }
        }
    }
    bool changed;
    int rounds = VI_MAX_PATH_NODES;
    do {
        changed = false;
        for (int i = VI_MAX_HUMANS - 1; i >= 0; i--) {
            vi_human_manager& hm = vii->humans[i];
            if (hm.handle == 0xFFFFFFFF || hm.interaction != VI_INTERACTION_ENTER || progress[i] == -1) {
                continue;
            }
            const int node = hm.path_nodes[progress[i]];
            if (node == -1 || node == hm.animate_dest_wpi) {
                continue;
            }
            vi_register_node(vii, i, node, in_use, info, false);
            if (progress[i] > 0 && vehicle_is_apc(vii->vvm.vp)) {
                signed char transition_idx = -1;
                const vehicle_interaction_waypoint_transition* transition = info->get_transition(transition_idx, hm.path_nodes[progress[i] - 1], node);
                if (transition != NULL) {
                    for (int a = 0; a < transition->num_interactions; a++) {
                        if (transition->actions[a].action == VIA_TRANSITION_CLOSED_DOOR) {
                            vi_door_close_for_climb(vii, &hm, transition, 0.33f);
                            break;
                        }
                    }
                }
            }
            progress[i]--;
            changed = true;
        }
        rounds--;
    } while (changed && rounds > 0);
    for (rounds = VI_MAX_PATH_NODES; rounds > 0; rounds--) {
        if (vii->num_humans < 1) {
            return;
        }
        changed = false;
        remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            vi_human_manager& hm = vii->humans[i];
            if (hm.handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            if (hm.interaction != VI_INTERACTION_EXIT || progress[i] == -1 || progress[i] >= VI_MAX_PATH_NODES) {
                continue;
            }
            const int node = hm.path_nodes[progress[i]];
            if (node == -1) {
                continue;
            }
            if (in_use[node] == -1 || in_use[node] == i) {
                vi_register_node(vii, i, node, in_use, info, true);
                progress[i]++;
                changed = true;
                if (progress[i] >= VI_MAX_PATH_NODES || hm.path_nodes[progress[i]] == -1) {
                    progress[i] = -1;
                }
            }
            else {
                vii->vvm.used_waypoints[node].reg_on_release = 1;
                const int holder = vii->vvm.used_waypoints[node].get_human_index();
                if (vii->humans[holder].interaction == VI_INTERACTION_RIDE) {
                    vi_request_npc_exit(vii, holder, i);
                }
                progress[i] = -1;
            }
        }
        if (!changed) {
            return;
        }
    }
}

// 0x00BB7A10 (:2894)
static void set_enter_pathfind(vi_instance* vii, unsigned int human_index)
{
    vi_trace_step(vii, human_index, "enter: pathfind");
    vi_human_manager* hm = &vii->humans[human_index];
    hm->interaction = VI_INTERACTION_ENTER;
    hm->state = VI_STATE_ENTER_PATHFIND;
    if (vii->post_stream_init == 0) {
        // Original bug kept: walks all 44 usage slots, reading past the table's waypoint array on a smaller vehicle.
        const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
        for (int i = 0; i < VI_MAX_WAYPOINTS; i++) {
            if (info->waypoints[i].node_type > VI_NODE_TYPE_DOOR_ENTRY) {
                vii->vvm.used_waypoints[i].block_type = VVM_WP_ENTRY_CHECK;
            }
        }
        vii->post_stream_init = 1;
    }
    if (hm->enter_data.flags.teleport != 0) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
    }
    if (vii->process_state_again != 0) {
        vi_process_logic_state(vii, human_index);
    }
}

// 0x00BB6660 (:2926)
static void process_enter_pathfind(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->enter_data.flags.teleport != 0) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
        return;
    }
    if (vii->reregister_paths != 0) {
        return;
    }
    if (vi_human_path_to_seat(vii, human_index, hm->enter_data.destination_seat)) {
        hm->state = VI_STATE_NONE;
        set_anim_process_continue(vii, human_index);
        return;
    }
    if (object_is_local_player(hm->handle)) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
        vi_process_logic_state(vii, human_index);
        return;
    }
    mark_seat_reserved(vii, human_index, hm->enter_data.destination_seat, VVM_WP_SEAT_PF_FAILED);
    if (hm->enter_data.seat_selection_method != VI_SEAT_SELECTION_METHOD_SPECIFIED) {
        vi_seat_reserve(vii, human_index);
        if (hm->enter_data.destination_seat != VSI_INVALID) {
            return;
        }
    }
    vi_request_exit(vii, human_index, false);
}

// 0x00B68650 (:2971)
void path_to_door_stop(human* hp, vi_human_manager* hm)
{
    if (hp->pf.path != NULL) {
        bb_pf_free_path(&hp->pf.path);
    }
    human_set_state(hp, HS_PLAYER_PATHING, HMS_IDLE, hp->move_speed);
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
}

// 0x00B81730 (:2980)
void path_to_door_complete(human* hp, vi_human_manager* hm)
{
    hm->cur_path_idx = 0;
    hm->animate_from_wpi = hm->path_nodes[0];
    hm->animate_dest_wpi = hm->path_nodes[0];
    hm->wait_ts.set(3000);
    hm->pos_correction = VI_POS_CORRECTION_VEHICLE_VELOCITY;
    human_enable_gravity(hp, false);
    human_set_state(hp, HS_VEHICLE_ENTERING, HMS_IDLE, -1.0f);
    hm->use_vehicle_collision = -1;
    human_update_collision_filter(hp, true);
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    human_override_default_animation_state(hp, false);
        const int anim_set_index = (int)(hp->char_inst->vp->animation_set - Anim_sets);
    vanim_change_custom_state(hp->char_inst->vp, vi_index_alias(vanim_find_anim_handle(anim_set_index, 0, 1)), vi_index_alias(-1), 0.3f, false);
}

// 0x00B8FE20 (:3004)
static void set_enter_path_to_door(vi_instance* vii, unsigned int human_index)
{
    vi_trace_step(vii, human_index, "enter: to the door");
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    hm->enter_data.flags.use_pf_path = 0;
    hm->state_of_anim = VI_STATE_ENTER_PATH_TO_DOOR;
    hp->combat.combat_ready_timestamp.invalidate();
    hp->combat.combat_aim_timestamp.invalidate();
    human_set_combat_ready(hp, false, 0);
    human_override_default_animation_state(hp, false);
    const vehicle_interaction_info* info = vp->info->vi_info;
    hm->wait_ts.set(500);
    vector door_pos;
    matrix door_orient;
    info->get_waypoint_world_pos_orient(vp, hm->path_nodes[0], door_pos, &door_orient, hp->orient.uvec);
    float terrain_y;
    if (havok_terrain_height(terrain_y, door_pos.x, door_pos.z, false, NULL) && door_pos.y < terrain_y) {
        door_pos.y = terrain_y;
    }
    human_set_state(hp, HS_PLAYER_PATHING, HMS_RUN, -1.0f);
    hm->use_vehicle_collision = 0;
    human_update_collision_filter(hp, true);
    human_set_movement_mode(hp, HMM_PATHFINDING_TO_VEHICLE, HMSM_NONE);
    if (hp->ground_obj_handle == vp->get_handle()) {
        return;
    }
    const float dx = door_pos.x - hp->pos.x;
    const float dy = door_pos.y - hp->pos.y;
    const float dz = door_pos.z - hp->pos.z;
    if (dx * dx + dy * dy + dz * dz < 1.0f) {
        path_to_door_stop(hp, hm);
        return;
    }
    hp->hflags.disable_path_smoothing_for_request = 1;
    if (!human_pathfind_to(hp, door_pos, HS_PLAYER_PATHING, HMS_RUN, 0xFFFFFFFFFFFFFFFFULL, false)) {
        path_to_door_stop(hp, hm);
        return;
    }
    path_node* path = hp->pf.path;
    if (path != NULL && path->next == path->prev) {
        // Two nodes: a path that leads away from the door is worse than walking straight at it.
        const path_node* next = path->next;
        const float straight = vec_xz_dist_squared(hp->pos, door_pos) * 2.0f;
        const float along = (next->pos.y - hp->pos.y) * (door_pos.y - hp->pos.y) + (next->pos.x - hp->pos.x) * (door_pos.x - hp->pos.x) + (next->pos.z - hp->pos.z) * (door_pos.z - hp->pos.z);
        if (straight < vec_xz_dist_squared(next->pos, door_pos) || along < 0.0f) {
            path_to_door_stop(hp, hm);
            return;
        }
    }
    vector bmin, bmax;
    vp->get_local_bbox(bmin, bmax);
    if (vp->get_sub_type() != OT_SUB_VEHICLE_FLYER) {
        path = hp->pf.path;
        if (path == NULL) {
            path_to_door_stop(hp, hm);
            return;
        }
        int num_nodes = 0;
        int num_close = 0;
        path_node* node = path;
        do {
            vector local = node->pos;
            vec_change_to_local_coords(local, vp->pos, vp->orient);
            // a path that strays over 6 m from the vehicle
            if (bbox_find_distance_squared(bmin, bmax, local) > 36.0f) {
                path_to_door_stop(hp, hm);
                return;
            }
            num_nodes++;
            if (vec_xz_dist_squared(node->pos, hp->pos) < 0.25f) {
                num_close++;
            }
            node = node->next;
        } while (node != hp->pf.path && node != NULL);
        if (num_close == num_nodes) {
            path_to_door_stop(hp, hm);
            return;
        }
    }
    hm->enter_data.flags.use_pf_path = 1;
    hm->human_motion = ZERO_VECTOR;
}

// 0x00BBBE40 (:3135)
// Runs the human to the door waypoint: around the vehicle's bounding box when the door is on another side, behind the
// human who holds the door, and straight at it otherwise. Stuck humans are teleported to the door when they own it.
static void process_enter_path_to_door(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_info* info = vp->info->vi_info;
    hp->steering_heading_offset = 0.0f;
    const int door_wpi = hm->path_nodes[0];
    const bool on_vehicle = hp->ground_obj_handle == vp->get_handle();
    bool going_around = false;
    vector door_pos;
    matrix door_orient;
    info->get_waypoint_world_pos_orient(vp, door_wpi, door_pos, &door_orient, hp->orient.uvec);
    float terrain_y;
    if (havok_terrain_height(terrain_y, door_pos.x, door_pos.z, false, NULL) && door_pos.y < terrain_y) {
        door_pos.y = terrain_y;
    }
    hp->last_pos = hp->pos;
    vector to_target(door_pos.x - hp->pos.x, door_pos.y - hp->pos.y, door_pos.z - hp->pos.z);
    const float door_xz_dist_squared = to_target.x * to_target.x + to_target.z * to_target.z;
    float target_dist_squared = to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z;
    float door_dist_squared = target_dist_squared;
    if (door_dist_squared > 1.0f && hp->movement_mode != HMM_PATHFINDING && !on_vehicle && !vehicle_is_mar_raider(vp)) {
        vector local_door = door_pos;
        vec_change_to_local_coords(local_door, vp->pos, vp->orient);
        vector local_human = hp->pos;
        vec_change_to_local_coords(local_human, vp->pos, vp->orient);
        vector bmin, bmax;
        vp->get_local_bbox_slow(bmin, bmax);
        const float center_x = (bmin.x + bmax.x) * 0.5f;
        const float half_x = (bmax.x - bmin.x) * 0.5f;
        const float center_z = (bmin.z + bmax.z) * 0.5f;
        const float half_z = (bmax.z - bmin.z) * 0.5f;
        local_door.x -= center_x;
        local_door.y -= bmin.y;
        local_door.z -= center_z;
        // positions as -1 to 1 across the box
        float door_nx = local_door.x / half_x;
        door_nx = door_nx < -1.0f ? -1.0f : (door_nx >= 1.0f ? 1.0f : door_nx);
        float door_nz = local_door.z / half_z;
        door_nz = door_nz < -1.0f ? -1.0f : (door_nz >= 1.0f ? 1.0f : door_nz);
        const float abs_door_nx = fabsf(door_nx);
        const float abs_door_nz = fabsf(door_nz);
        local_human.x -= center_x;
        local_human.y -= bmin.y;
        local_human.z -= center_z;
        float human_nx = local_human.x / half_x;
        human_nx = human_nx < -1.0f ? -1.0f : (human_nx >= 1.0f ? 1.0f : human_nx);
        float human_nz = local_human.z / half_z;
        human_nz = human_nz < -1.0f ? -1.0f : (human_nz >= 1.0f ? 1.0f : human_nz);
        const float abs_human_nx = fabsf(human_nx);
        const float abs_human_nz = fabsf(human_nz);
        const bool near_center = abs_human_nz + abs_human_nx < 0.3f;
        const float side_match = abs_door_nx > abs_door_nz ? fl_sign(door_nx) * human_nx : fl_sign(door_nz) * human_nz;
        const bool on_door_side = side_match > 0.97f;
        bool same_side = false;
        if (abs_door_nx - abs_door_nz > 0.0f && abs_human_nx - abs_human_nz > 0.08f) {
            same_side = human_nx * door_nx >= 0.0f;
            door_nx = 0.0f;
        }
        else if (abs_door_nx - abs_door_nz < 0.0f && abs_human_nx - abs_human_nz < -0.08f) {
            same_side = human_nz * door_nz >= 0.0f;
            door_nz = 0.0f;
        }
        else if (abs_human_nx >= 1.0f && abs_human_nz >= 1.0f) {
            if (abs_door_nx > abs_door_nz) {
                human_nx = 0.0f;
            }
            else {
                human_nz = 0.0f;
            }
        }
        if (!near_center && !on_door_side && !same_side) {
            // head for the box corner between the human and the door, half a metre out
            going_around = true;
            const float corner_x = fl_sign(human_nx + door_nx) * (half_x + 0.5f);
            const float corner_z = fl_sign(human_nz + door_nz) * (half_z + 0.5f);
            to_target.x = corner_x - local_human.x;
            to_target.y = 0.0f - local_human.y;
            to_target.z = corner_z - local_human.z;
            to_target.unrotate(vp->orient);
            target_dist_squared = to_target.mag_squared();
        }
    }
    // Blocked: the next node is someone else's, or another human is mid-transition by this door.
    bool blocked = false;
    const signed char next_wpi = hm->path_nodes[1];
    if (next_wpi != -1 && vii->vvm.used_waypoints[next_wpi].get_human_index() != (int)human_index) {
        blocked = true;
    }
    else {
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            const vi_human_manager& other = vii->humans[i];
            if (other.handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            if (i != (int)human_index && other.state_of_anim == VI_STATE_SHARED_TRANSITION) {
                object* objp = World.object_from_handle(other.handle);
                if (objp != NULL && objp->get_obj_type() == OT_HUMAN && vec_xz_dist_squared(objp->pos, door_pos) < 1.0f) {
                    blocked = true;
                    break;
                }
            }
        }
    }
    // Holding back: the door is not this human's yet, so aim next to it.
    bool holding_back = false;
    if (vii->reregister_paths != 0 || vii->vvm.used_waypoints[door_wpi].get_human_index() != (int)human_index || blocked) {
        holding_back = true;
        if (!going_around) {
            const int holder = vii->vvm.used_waypoints[door_wpi].get_human_index();
            if (holder < 0 || holder == (int)human_index || vii->humans[holder].interaction != VI_INTERACTION_ENTER) {
                blocked = true;
            }
            else {
                // line up behind the human going to the same door
                object* objp = vii->humans[holder].handle != 0xFFFFFFFF ? World.object_from_handle(vii->humans[holder].handle) : NULL;
                if (objp != NULL && objp->get_obj_type() == OT_HUMAN) {
                    const human* holder_hp = (human*)objp;
                    const vector& facing = holder_hp->steering_vector.is_unit(0.01f) ? holder_hp->steering_vector : holder_hp->orient.fvec;
                    to_target.x = (holder_hp->pos.x - facing.x) - hp->pos.x;
                    to_target.y = (holder_hp->pos.y - facing.y) - hp->pos.y;
                    to_target.z = (holder_hp->pos.z - facing.z) - hp->pos.z;
                    target_dist_squared = to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z;
                    door_dist_squared = target_dist_squared;
                }
            }
        }
        if (blocked) {
            // beside and behind the door: the side the waypoint is on (rfg.exe tests the sign bit, so -0.0 counts as left)
            const float side = (*(const unsigned int*)&info->waypoints[door_wpi].local_pos.x & 0x80000000) != 0 ? -1.0f : 1.0f;
            to_target.x += vp->orient.rvec.x * 0.5f * side + vp->orient.fvec.x * -0.6f;
            to_target.y += vp->orient.rvec.y * 0.5f * side + vp->orient.fvec.y * -0.6f;
            to_target.z += vp->orient.rvec.z * 0.5f * side + vp->orient.fvec.z * -0.6f;
            target_dist_squared = to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z;
            door_dist_squared = target_dist_squared;
        }
    }
    // How hard the human is walking into something, 1 and over being stuck.
    float stuck = 0.0f;
    bool is_stuck = false;
    const float speed = sqrtf(hp->velocity.x * hp->velocity.x + hp->velocity.y * hp->velocity.y + hp->velocity.z * hp->velocity.z);
    if (speed > 1.0f) {
        const float t = Frametime / vi_pf_stuck_time;
        hm->human_motion.x += ((hp->velocity.x - hp->actual_velocity.x) - hm->human_motion.x) * t;
        hm->human_motion.y += (0.0f - hm->human_motion.y) * t;
        hm->human_motion.z += ((hp->velocity.z - hp->actual_velocity.z) - hm->human_motion.z) * t;
        const float along = hp->steering_vector.y * hm->human_motion.y + hp->steering_vector.x * hm->human_motion.x + hp->steering_vector.z * hm->human_motion.z;
        stuck = along / (speed * vi_pf_stuck_threshold);
        is_stuck = stuck > 1.0f;
    }
    if (!is_stuck && target_dist_squared < 1.0f && hp->rdd.is_loaded() && hp->rdd.get_data()->controller->flags.sliding != 0) {
        is_stuck = true;
    }
    if (is_stuck) {
        hm->human_motion = ZERO_VECTOR;
        if (holding_back) {
            path_to_door_stop(hp, hm);
            vii->vvm.used_waypoints[door_wpi].block_type = VVM_WP_SEAT_PF_FAILED;
            vi_set_refresh_raycast_timer(vii);
            set_enter_pathfind(vii, human_index);
            return;
        }
        info->get_waypoint_world_pos_orient(vp, door_wpi, door_pos, &door_orient, UP_VECTOR);
        hp->update_pos_and_orient(door_pos, door_orient, true);
        hp->velocity = ZERO_VECTOR;
        hp->steering_heading_offset = 0.0f;
        hp->steering_vector.zero();
        hm->enter_data.flags.teleport_to_door = 1;
        path_to_door_complete(hp, hm);
        set_shared_wait_waypoint_clear(vii, human_index);
        return;
    }
    if (hm->enter_data.flags.use_pf_path != 0 && hp->pf.path == NULL && hp->movement_mode == HMM_PATHFINDING) {
        human_set_movement_mode(hp, HMM_PATHFINDING_TO_VEHICLE, HMSM_NONE);
    }
    if (target_dist_squared > 0.09f && (on_vehicle || hp->movement_mode != HMM_PATHFINDING)) {
        float t = turn_speed * Frametime;
        if (1.0f - t < 0.0f) {
            t = 1.0f;
        }
        const vector dir = to_target.copy_flatten_normalize_safe();
        hp->steering_vector.x += (dir.x - hp->steering_vector.x) * t;
        hp->steering_vector.y += (dir.y - hp->steering_vector.y) * t;
        hp->steering_vector.z += (dir.z - hp->steering_vector.z) * t;
    }
    // Slide along what the character controller is touching.
    bool touching = false;
    if (stuck > 0.05f && hp->rdd.is_loaded()) {
        vector avoid = ZERO_VECTOR;
        const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold = hp->rdd.get_data()->controller->m_characterProxy->getManifold();
        for (int i = manifold.getSize() - 1; i >= 0; i--) {
            vector steer = hp->steering_vector;
            vector contact;
            havok_to_game_vector(contact, manifold[i].m_contact.getPosition());
            contact.x -= hp->pos.x;
            contact.y -= hp->pos.y;
            contact.z -= hp->pos.z;
            if (contact.y <= vehicle_light_camera_fudge_distance) { // 0.25, the same literal
                continue;
            }
            contact.flatten_normalize_safe();
            const float into = contact.x * steer.x + contact.y * steer.y + contact.z * steer.z;
            if (into <= 0.0f) {
                continue;
            }
            steer.x -= into * contact.x * 1.1f;
            steer.y -= into * contact.y * 1.1f;
            steer.z -= into * contact.z * 1.1f;
            steer.flatten_normalize_safe();
            avoid.x += steer.x;
            avoid.y += steer.y;
            avoid.z += steer.z;
            touching = true;
            object* objp = object_from_world_obj((const hkpWorldObject*)manifold[i].m_rootCollidableB->getOwner());
            if (vp->get_handle() == objp->get_handle()) {
                if (hp->movement_mode == HMM_PATHFINDING) {
                    human_set_movement_mode(hp, HMM_PATHFINDING_TO_VEHICLE, HMSM_NONE);
                }
                const vector to_object(objp->pos.x - hp->pos.x, objp->pos.y - hp->pos.y, objp->pos.z - hp->pos.z);
                vector away = to_object.copy_normalize();
                away.flatten_mag_and_normalize_safe();
                avoid.x -= away.x * 0.5f;
                avoid.y -= away.y * 0.5f;
                avoid.z -= away.z * 0.5f;
            }
        }
        if (touching) {
            avoid.flatten_normalize_safe();
            hp->steering_vector.x += stuck * (avoid.x - hp->steering_vector.x) * Frametime * 19.0f;
            hp->steering_vector.y += (avoid.y - hp->steering_vector.y) * Frametime * 19.0f * stuck;
            hp->steering_vector.z += (avoid.z - hp->steering_vector.z) * Frametime * 19.0f * stuck;
        }
    }
    if (hp->steering_vector.is_null()) {
        hp->steering_vector = hp->orient.fvec;
    }
    hp->steering_vector.flatten_normalize();
    const human_move_speed_info (&speeds)[4] = human_get_move_speed_block(hp);
    vector vehicle_velocity;
    vehicle_get_velocity(vp, vehicle_velocity);
    const float dist = sqrtf(holding_back ? target_dist_squared : door_xz_dist_squared);
    const float wanted = dist * 2.0f + (blocked ? 0.0f : 1.0f);
    float facing = hp->orient.fvec.y * hp->steering_vector.y + hp->steering_vector.x * hp->orient.fvec.x + hp->orient.fvec.z * hp->steering_vector.z + (dist - 1.0f >= 0.0f ? 1.0f : dist) * 0.5f;
    facing = facing - 0.0f < 0.0f ? 0.0f : (facing - 1.0f >= 0.0f ? 1.0f : facing);
    float along_vehicle = hp->steering_vector.x * vehicle_velocity.x + hp->steering_vector.y * vehicle_velocity.y + hp->steering_vector.z * vehicle_velocity.z;
    if (0.0f - along_vehicle >= 0.0f) {
        along_vehicle = 0.0f;
    }
    float move_speed = along_vehicle + facing * wanted;
    if (hp->actual_velocity_mag < move_speed) {
        // speed up at 10 m/s^2
        const float from = 0.0f - hp->move_speed >= 0.0f ? 0.0f : hp->move_speed;
        float delta = move_speed - from;
        if (delta - Frametime * 10.0f >= 0.0f) {
            delta = Frametime * 10.0f;
        }
        move_speed = delta + from;
    }
    int movement_state = hm->enter_data.flags.walk != 0 ? HMS_WALK : HMS_SPRINT;
    if (!blocked && move_speed < 1.0f) {
        move_speed = 1.0f;
    }
    while (movement_state > 0 && !(speeds[movement_state].min_speed < move_speed)) {
        movement_state--;
    }
    if (move_speed - speeds[movement_state].max_speed >= 0.0f) {
        move_speed = speeds[movement_state].max_speed;
    }
    human_set_state(hp, HS_CURRENT, (human_movement_states)movement_state, move_speed);
    if (holding_back && movement_state == HMS_IDLE && target_dist_squared < 0.09f) {
        path_to_door_stop(hp, hm);
    }
    else if (hp->movement_mode == HMM_NONE) {
        human_set_movement_mode(hp, HMM_PATHFINDING_TO_VEHICLE, HMSM_NONE);
    }
    v_animate* animate = hp->char_inst->vp;
    if (animate->flags.interpolate_orient == 0 && target_dist_squared < 2.25f && !holding_back && !going_around) {
        if (!vec_equal_fast(hp->orient.uvec, door_orient.uvec, 0.01f)) {
            vanim_interpolate_uvec(animate, hp, door_orient.uvec, 0.8f);
        }
    }
    // Original quirk kept: each branch only runs when use_vehicle_collision already has the value it then writes.
    if (!on_vehicle && hp->char_inst->vp->flags.interpolate_orient != 0 && !holding_back && !going_around) {
        if (hm->use_vehicle_collision != 0) {
            human_set_state(hp, HS_VEHICLE_ENTERING, HMS_CURRENT, hp->move_speed);
            hm->use_vehicle_collision = -1;
            human_update_collision_filter(hp, true);
        }
    }
    else if (hm->use_vehicle_collision == 0) {
        human_set_state(hp, HS_PLAYER_PATHING, HMS_CURRENT, hp->move_speed);
        hm->use_vehicle_collision = 0;
        human_update_collision_filter(hp, true);
    }
    if (door_xz_dist_squared < 1.0f && !going_around) {
        hm->enter_data.flags.use_pf_path = 0;
        if (hp->pf.path != NULL) {
            bb_pf_free_path(&hp->pf.path);
        }
        human_set_movement_mode(hp, HMM_PATHFINDING_TO_VEHICLE, HMSM_NONE);
        if (!holding_back) {
            if (!touching) {
                hp->steering_vector = to_target.copy_flatten_normalize_safe();
            }
            if (door_dist_squared < 0.25f || (hp->actual_velocity_mag < 1.1f && door_xz_dist_squared < 0.09f && hp->actual_velocity.y >= -0.001f)) {
                hp->steering_vector = door_orient.fvec.copy_flatten_normalize();
                hp->steering_heading_offset = 0.0f;
                path_to_door_complete(hp, hm);
                set_shared_wait_waypoint_clear(vii, human_index);
                return;
            }
        }
    }
    else if (hm->wait_ts.elapsed() && vii->reregister_paths == 0) {
        hm->wait_ts.set(500);
        if (!vi_human_path_to_seat(vii, human_index, hm->enter_data.destination_seat)) {
            set_enter_pathfind(vii, human_index);
            return;
        }
        if (door_wpi != hm->path_nodes[0]) {
            set_enter_path_to_door(vii, human_index);
            return;
        }
    }
    // over 6 m from the vehicle: give up
    vector local_pos = hp->pos;
    vector bmin, bmax;
    vp->get_local_bbox(bmin, bmax);
    vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
    if (bbox_find_distance_squared(bmin, bmax, local_pos) > 36.0f && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER) {
        path_to_door_stop(hp, hm);
        hm->exit_data.flags.teleport = 1;
        hm->exit_data.flags.maintain_position = 1;
        vi_request_exit(vii, human_index, false);
    }
}

// 0x00B81800 (:3584)
// Waiting for the human's or the vehicle's stream: hidden, without collision, carried with the vehicle.
static void set_enter_streaming_stall(vi_instance* vii, unsigned int human_index)
{
    vi_trace_step(vii, human_index, "enter: waiting for streaming");
    vi_human_manager* hm = &vii->humans[human_index];
    hm->state = VI_STATE_ENTER_STREAMING_STALL;
    hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
    human* hp = vi_human_from_handle(hm->handle);
    human_set_state(hp, HS_VEHICLE_SITTING, HMS_IDLE, -1.0f);
    object_set_visibility_plus_children(hp, false);
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    human_update_collision_filter(hp, false);
}

// 0x00B81880 (:3602)
static void process_enter_streaming_stall(vi_instance* vii, unsigned int human_index)
{
    vehicle* vp = vii->vvm.vp;
    human* hp = vi_human_from_handle(vii->humans[human_index].handle);
    if (!hp->rdd.is_loaded()) {
        return;
    }
    human_set_state(hp, HS_VEHICLE_SITTING, HMS_IDLE, -1.0f);
    object_set_visibility_plus_children(hp, false);
    human_update_collision_filter(hp, false);
    human_enable_gravity(hp, false);
    if (vp->rdd.is_loaded()) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
        vi_process_logic_state(vii, human_index);
    }
}

// 0x00B686A0 (:3624)
static void vi_enter_teleport(vi_instance* vii, unsigned int human_index)
{
    vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
    vi_process_logic_state(vii, human_index);
}

// 0x00BB4D90 (:3629)
static void set_enter_teleport_wait(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    vi_seat_reserve(vii, human_index);
    const vehicle_seat_index seat = hm->enter_data.destination_seat;
    if (seat == VSI_INVALID) {
        vi_request_exit(vii, human_index, false);
        return;
    }
    const int wpi = vi_utilities::waypoint_index_from_seat_index(vp, seat);
    if (wpi == -1) {
        vi_request_exit(vii, human_index, false);
        return;
    }
    hm->enter_data.flags.teleport = 1;
    hm->state = VI_STATE_ENTER_TELEPORT_WAIT;
    vi_trace_step(vii, human_index, "enter: teleport to the seat, no animation");
    hm->state_of_anim = VI_STATE_ANIM_NONE;
    if (hm->cur_path_idx >= 0 && wpi == hm->path_nodes[hm->cur_path_idx]) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_COMPLETE);
        return;
    }
    hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    human_set_state(hp, HS_VEHICLE_SITTING, HMS_IDLE, -1.0f);
    object_set_visibility_plus_children(hp, false);
    human_update_collision_filter(hp, false);
    human_enable_gravity(hp, false);
    if (object_is_player(hp) && ovl_inventory_is_open() && object_is_local_player(hp)) {
        ovl_inventory_close();
    }
    if (hp != NULL) {
        human_weapons_immediately_holster_weapon(hp);
    }
    const unsigned int action = vanim_get_anim_instance_handle(hp->char_inst->vp, 0x400);
    if (action != 0xFFFFFFFF) {
        // finish whatever the human was playing at once
        animlib_set_speed_multiplier(hp->char_inst->aim_handle, action, 1000.0f);
    }
    release_curr_wp_node(vii, human_index);
    hm->door_idx = -1;
    hm->cur_path_idx = -1;
    hm->path_nodes[0] = (signed char)wpi;
    hm->path_nodes[1] = -1;
    hm->reserve_next_node_only = 0;
    hm->animate_from_wpi = -1;
    hm->animate_dest_wpi = -1;
    if (hm->enter_data.flags.minimal_processing != 0 && vi_node_dep_free(vii, wpi, human_index)) {
        // Original bug kept: the array is not filled with -1, so what registers depends on the uninitialized stack.
        farray<int,44> in_use;
        vi_register_node(vii, human_index, wpi, in_use, vp->info->vi_info, false);
    }
    else {
        vii->reregister_paths = 1;
    }
    vii->process_state_again = 1;
}

// 0x00BB4FF0 (:3714)
static void process_enter_teleport_wait(vi_instance* vii, unsigned int human_index)
{
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    vi_human_manager* hm = &vii->humans[human_index];
    const signed char next_wpi = hm->path_nodes[hm->cur_path_idx + 1];
    if (next_wpi == -1) {
        set_enter_teleport_wait(vii, human_index);
        if (hm->path_nodes[hm->cur_path_idx + 1] == next_wpi) {
            hm->exit_data.flags.teleport = 1;
            vi_request_exit(vii, human_index, false);
            return;
        }
    }
    human* hp = vi_human_from_handle(hm->handle);
    if (hp->block_movement_anim != -1) {
        return;
    }
    // Original quirk kept: after the re-path above next_wpi is still -1 here, which indexes before the usage array and
    // lands in the exit below.
    if (!vi_node_dep_free(vii, next_wpi, human_index) || vii->reregister_paths != 0) {
        if (vii->reregister_paths == 0) {
            vii->reregister_paths = 1;
        }
        human_set_state(hp, HS_VEHICLE_SITTING, HMS_IDLE, -1.0f);
        object_set_visibility_plus_children(hp, false);
        human_update_collision_filter(hp, false);
        human_enable_gravity(hp, false);
        return;
    }
    vehicle* vp = vii->vvm.vp;
    hm->cur_path_idx++;
    hm->animate_dest_wpi = next_wpi;
    hm->animate_from_wpi = next_wpi;
    if (next_wpi == -1) {
        hm->exit_data.flags.teleport = 1;
        vi_request_exit(vii, human_index, false);
        return;
    }
    vi_clear_corpses(vii, next_wpi);
    info->get_waypoint_world_pos_orient(vp, next_wpi, hp->pos, &hp->orient, UP_VECTOR);
    hp->update_pos_and_orient(hp->pos, hp->orient, true);
    if (vp->havok_handle == 0xFFFFFFFF) {
        hp->velocity = ZERO_VECTOR;
    }
    else {
        havok_body_get_point_velocity(vp->havok_handle, hp->pos, hp->velocity);
    }
    human_controller_set_vel(hp, hp->velocity);
    hp->hflags.use_current_velocity = 0;
    hp->char_inst->vp->flags.use_velocity = 0;
    hm->ride_state_handle = -1;
    vi_set_logic_state(vii, human_index, VI_STATE_ENTER_COMPLETE);
}

// 0x00BB5200 (:3807)
static void set_enter_complete(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->handle == 0xFFFFFFFF) {
        return;
    }
    vi_trace_step(vii, human_index, "enter: complete");
    object* hp_obj = World.object_from_handle(hm->handle);
    if (hp_obj == NULL || hp_obj->get_obj_type() != OT_HUMAN) {
        return;
    }
    human* hp = (human*)hp_obj;
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_info* info = vp->info->vi_info;
    waypoint_usage& usage = vii->vvm.used_waypoints[hm->path_nodes[hm->cur_path_idx]];
    if (usage.block_type != VVM_WP_SEAT_RESERVED) {
        usage.block_type = VVM_WP_SEAT_RESERVED;
    }
    const vehicle_seat_index seat = hm->enter_data.destination_seat;
    hm->seat = seat;
    hm->state = VI_STATE_ENTER_COMPLETE;
    hm->enter_data.specified_seat = seat;
    hp->vehicle_seat_idx = seat;
    vp->m_seat_info[hm->seat].m_current_occupant_handle = hp->get_handle();
    hm->enter_data.flags.teleport_to_door = 0;
    if (vp->m_flags.m_corpse == 0 && vp->m_flags.m_occupied == 0) {
        vp->m_flags.m_occupied = 1;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        walker_play_VI_anim((walker*)vp, 2, 0);
    }
    else if (hm->seat == VSI_DRIVER && vp->vp != NULL) {
        // 0xA5: the driver's seat state of the vehicle's own skeleton
        const int anim = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, 0xA5);
        if (anim >= 0 && vp->vp->desired_state->index != anim) {
            vanim_change_custom_state(vp->vp, vi_index_alias(anim), vi_index_alias(-1), 1.0f, false);
        }
    }
    player* pp = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : NULL;
    vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(vp, hm->seat);
    turret* tp = NULL;
    if (mount != NULL && mount->attached_turret != 0xFFFFFFFF) {
        object* turret_obj = World.object_from_handle(mount->attached_turret);
        if (turret_obj != NULL && turret_obj->get_obj_type() == OT_TURRET) {
            tp = (turret*)turret_obj;
        }
    }
    bool man_turret = true;
    if (hm->seat == VSI_DRIVER) {
        if (!vp->is_killed()) {
            vp->engine_start(object_is_local_player(hp));
        }
        vp->most_recent_driver = hp->get_handle();
        if (object_is_local_player(hp)) {
            pp->last_vehicle_driven = vp->get_handle();
            const char* tip = NULL;
            if (vp->info->vehicle_classification == BVT_TANK || vp->info->vehicle_classification == BVT_ART_TANK) {
                tip = "Tanks";
            }
            else if (vehicle_is_minelayer(vp)) {
                tip = "Mine_Pooper";
            }
            else if (vp->info->default_team == HUMAN_TEAM_EDF && vehicle_has_gunner_seat(vp)) {
                tip = "EDF_Turret";
            }
            else if (vp->get_sub_type() != OT_SUB_VEHICLE_WALKER) {
                tip = "Driving";
            }
            if (tip != NULL) {
                ui_tool_tips_add_tip(tip, 0, NULL);
            }
        }
        vp->team_of_most_recent_driver = hp->current_team;
        vp->most_recent_driver_exit_time.invalidate();
        if (tp == NULL) {
            if (pp != NULL && Player_vehicle_turret_override && vp->m_num_turret_mounts > 0 && vp->m_turret_mounts[0].info->seat == VSI_PASSENGER_1) {
                tp = World.object_from_handle<turret>(vp->m_turret_mounts[0].attached_turret);
            }
            if (tp == NULL) {
                man_turret = false;
            }
        }
    }
    else if (tp == NULL) {
        man_turret = false;
    }
    if (man_turret) {
        human* op = human_combat_turret_get_operator(tp);
        if (hm->seat == VSI_DRIVER) {
            man_turret = op == NULL;
        }
        else if (op != NULL && op != hp) {
            human_combat_abandon_turret(op);
        }
    }
    if (man_turret) {
        if (hm->seat != VSI_DRIVER && info != NULL && vehicle_get_seat_turret(vp, hm->enter_data.destination_seat) != NULL && hm->door_idx != -1) {
            vii->vvm.doors[hm->door_idx].state = VI_DOOR_STATE_OPEN_FOR_GUNNER;
        }
        if (tp->is_functional(pp != NULL)) {
            human_combat_man_turret(hp, tp);
            if (pp != NULL && (hm->seat != VSI_DRIVER || vehicle_is_tank(vp))) {
                vi_utilities::switch_to_on_foot_camera(pp);
            }
        }
    }
    if (hp->hflags.fading_in == 0 && hp->hflags.fading_out == 0 && !hp->fade_with_vehicle()) {
        hp->render_alpha = 1.0f;
    }
    const vehicle_interaction_waypoint& wp = info->waypoints[hm->effective_curr_wpi()];
    if (wp.node_type <= VI_NODE_TYPE_DOOR_ENTRY && wp.last_choice == 0 && wp.eject_on_explosion == 0) {
        human_set_cast_shadows(hp, false, true);
        human_set_cast_drop_shadows(hp, false, true);
    }
    if (pp != NULL) {
        audiolib_vehicle_engine_audio_to_2d(vp->engine_inst);
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
            automobile_reset_handling_for_ai((automobile*)vp, false, hm->seat == VSI_DRIVER);
        }
        if (vp->m_bomb_status == VEHICLE_BOMB_ARMED && !activity_car_bomb_is_active()) {
            activity_car_bomb_start(NULL);
        }
        human_set_movement_mode(hp, HMM_DIRECT, HMSM_NONE);
        pp->health_restore_timestamp.set(0);
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            if (vii->humans[i].handle != 0xFFFFFFFF) {
                remaining--;
                if (vii->humans[i].being_hijacked != 0) {
                    vi_request_npc_exit(vii, i, human_index);
                }
            }
        }
    }
    else {
        human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    }
    human_set_state(hp, hm->seat == VSI_DRIVER ? HS_VEHICLE_DRIVING : HS_VEHICLE_SITTING, HMS_IDLE, 0.0f);
    hm->use_vehicle_collision = -1;
    human_update_collision_filter(hp, true);
    human_enable_gravity(hp, false);
    human_override_default_animation_state(hp, false);
    if (hm->seat == VSI_DRIVER) {
        vp->team_of_most_recent_driver = hp->current_team;
    }
    if (pp == NULL) {
        npc_enters_vehicle_event event;
        event.npc_handle = hp->get_handle();
        event.vehicle_handle = vp->get_handle();
        gem_post_event(event);
    }
    hm->interaction = VI_INTERACTION_RIDE;
    hm->state = VI_STATE_NONE;
    set_ride_default(vii, human_index);
}

// 0x00BA28C0 (:4026)
static void set_shared_wait_waypoint_clear(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    animlib_clear_looping_animations(hp->char_inst->aim_handle, false);
    hm->state_of_anim = VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR;
    hm->transition_idx = -1;
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    if (hm->interaction == VI_INTERACTION_ENTER && hm->enter_data.flags.teleport != 0) {
        vi_set_logic_state(vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
        return;
    }
    if (hm->interaction == VI_INTERACTION_EXIT && hm->exit_data.flags.teleport != 0) {
        vi_exit_teleport(vii, human_index);
        return;
    }
    const int cur = hm->cur_path_idx;
    const signed char cur_wpi = hm->path_nodes[cur];
    const vehicle_interaction_waypoint* waypoints = vii->vvm.vp->info->vi_info->waypoints;
    const bool auto_connect = cur <= 4 && hm->path_nodes[cur + 1] >= 1 && hm->auto_connect_to_entry != 0 && waypoints[hm->path_nodes[cur + 1]].node_type == VI_NODE_TYPE_ENTRY;
    // Original bug kept: a -1 next node indexes the waypoints before their array here.
    if (cur + 1 < VI_MAX_PATH_NODES && ((waypoints[hm->path_nodes[cur + 1]].node_type == VI_NODE_TYPE_ENTRY && waypoints[cur_wpi].node_type == 0 && waypoints[cur_wpi].short_range != 0) || auto_connect)) {
        hm->cur_path_idx = (signed char)(cur + 1);
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
        return;
    }
    if (hm->interaction == VI_INTERACTION_ENTER) {
        human_weapons_immediately_holster_weapon(hp);
    }
    if (waypoints[cur_wpi].seat_index != VSI_INVALID) {
        human_override_default_animation_state(hp, true);
        if (hm->ride_state_handle == -1) {
            // the character's generic driving (0xBE) or sitting (0x89) animation state
            const int state = hp->cur_state == HS_VEHICLE_DRIVING ? 0xBE : 0x89;
            if (!hp->char_inst->is_in_state(state)) {
                hp->char_inst->set_state(vi_index_alias(state), vi_index_alias(-1), 0.3f, false);
            }
        }
        else {
            vanim_change_custom_state(hp->char_inst->vp, vi_index_alias(hm->ride_state_handle), vi_index_alias(-1), 0.3f, false);
        }
    }
    human_override_default_animation_state(hp, true);
    vii->process_state_again = 1;
}

// 0x00BB57D0 (:4120)
static void set_anim_process_continue(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->state_of_anim != VI_STATE_ANIM_NONE) {
        return;
    }
    if (hm->animate_from_wpi == -1) {
        switch (hm->interaction) {
        case VI_INTERACTION_RESERVE:
        case VI_INTERACTION_EXIT:
            break;
        case VI_INTERACTION_ENTER:
            set_enter_path_to_door(vii, human_index);
            break;
        case VI_INTERACTION_RIDE:
            set_ride_default(vii, human_index);
            break;
        default:
            // rfg.exe prints "Int3 critical function at %s(%d)" and carries on
            break;
        }
    }
    else {
        set_shared_wait_waypoint_clear(vii, human_index);
    }
    vii->process_state_again = 1;
}

// 0x00BB5860 (:4151)
static void set_anim_cancel(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    if (hp != NULL && hp->char_inst != NULL) {
        vanim_pause_and_ramp_out_action(hp->char_inst->vp, 0x400, 6);
        v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action != NULL) {
            action->alias = ANIM_ALIAS_NONE;
        }
    }
    hm->reserve_next_node_only = 0;
    hm->animate_dest_wpi = hm->animate_from_wpi;
    hm->ride_state_handle = -1;
    hm->state_of_anim = VI_STATE_ANIM_NONE;
}

// 0x00BBDF20 (:4169)
static void process_shared_wait_waypoint_clear(vi_instance* vii, unsigned int human_index)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    const int cur = hm->cur_path_idx;
    const int next_wpi = cur <= 4 ? hm->path_nodes[cur + 1] : -1;
    if (next_wpi == -1) {
        if (hm->state != VI_STATE_NONE && !hm->wait_ts.elapsed()) {
            return;
        }
        if (hm->interaction == VI_INTERACTION_ENTER) {
            set_enter_complete(vii, human_index);
        }
        else if (hm->interaction == VI_INTERACTION_EXIT) {
            set_exit_complete(vii, human_index);
        }
        return;
    }
    if (hm->wait_ts.elapsed()) {
        if (hm->interaction == VI_INTERACTION_ENTER) {
            vii->vvm.used_waypoints[next_wpi].block_type = VVM_WP_SEAT_PF_FAILED;
            vi_set_refresh_raycast_timer(vii);
            // Original quirk kept: rfg.exe tests the protected position and does the same on both branches.
            human_in_protected_vehicle_pos(vp, human_index);
            set_enter_teleport_wait(vii, human_index);
            vi_process_logic_state(vii, human_index);
            return;
        }
        if (hm->state != VI_STATE_EXIT_PATHFIND) {
            vii->vvm.used_waypoints[next_wpi].block_type = VVM_WP_SEAT_PF_FAILED;
            vi_set_refresh_raycast_timer(vii);
            set_exit_pathfind(vii, human_index);
            return;
        }
    }
    if (!vi_node_dep_free(vii, next_wpi, human_index) || vii->reregister_paths != 0) {
        if (vii->vvm.used_waypoints[next_wpi].is_path_reserved()) {
            return;
        }
        vii->reregister_paths = 1;
        return;
    }
    if (vi_human_pf_check_raycast(vii, next_wpi, hm->path_nodes[cur])) {
        if (hm->state != VI_STATE_NONE) {
            return;
        }
        if (hm->interaction == VI_INTERACTION_ENTER) {
            set_enter_pathfind(vii, human_index);
        }
        else if (hm->interaction == VI_INTERACTION_EXIT) {
            set_exit_pathfind(vii, human_index);
        }
        return;
    }
    if (hm->interaction == VI_INTERACTION_EXIT) {
        // The vehicle's own exit pose first: a walker kneels, a driver's seat animates, a jetter's gun points forward.
        if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
            walker* wp = (walker*)vp;
            walker_play_VI_anim(wp, 3, vp->info->vi_info->waypoints[next_wpi].node_type);
            if (wp->ground_obj_handle != 0xFFFFFFFF) {
                if (vanim_is_playing_action(wp->vp, 0x400)) {
                    return;
                }
                if (wp->vp->desired_state->blend < 0.999f) {
                    return;
                }
            }
        }
        else if (vp->vp != NULL) {
            const vehicle_interaction_info* info = vp->info->vi_info;
            if (info->waypoints[hm->effective_curr_wpi()].seat_index == VSI_DRIVER) {
                const int anim = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, 1);
                if (anim >= 0) {
                    v_anim* desired = vii->vvm.vp->vp->desired_state;
                    if (desired->anim_handle != anim) {
                        vanim_change_custom_state(vii->vvm.vp->vp, vi_index_alias(anim), vi_index_alias(-1), 1.0f, false);
                        return;
                    }
                    if (desired->blend < 0.999f) {
                        return;
                    }
                }
            }
        }
        else if (vehicle_is_min_jetter(vp) && hm->enter_data.destination_seat == VSI_PASSENGER_1) {
            turret* tp = vehicle_get_seat_turret(vp, VSI_PASSENGER_1);
            if (tp != NULL) {
                const matrix orient = tp->get_operation_orient();
                const float heading = fl_angle_cap_radians(orient.extract_heading() - vp->orient.extract_heading(), -3.1415927f);
                if (fabsf(heading * 57.295776f) > 90.0f) {
                    return;
                }
            }
        }
    }
    if (cur + 2 < VI_MAX_PATH_NODES) {
        const int after_next = hm->path_nodes[cur + 2];
        const vehicle_interaction_waypoint& next_wp = vii->vvm.vp->info->vi_info->waypoints[next_wpi];
        if (after_next != -1 && vii->vvm.used_waypoints[after_next].get_human_index() != (int)human_index && next_wp.restrict_vertical_access != 0 && next_wp.node_type == 0) {
            return;
        }
    }
    set_shared_open_door(vii, human_index);
}

// 0x00BA2B00 (:4329)
static void set_shared_open_door(vi_instance* vii, unsigned int human_index)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    const vehicle_interaction_info* info = vp->info->vi_info;
    hm->state_of_anim = VI_STATE_SHARED_OPEN_DOOR;
    const int cur_wpi = hm->path_nodes[hm->cur_path_idx];
    const int next_wpi = hm->path_nodes[hm->cur_path_idx + 1];
    const bool no_door = (hm->auto_connect_to_entry != 0 && info->waypoints[next_wpi].node_type == VI_NODE_TYPE_ENTRY)
        || (info->waypoints[next_wpi].node_type == VI_NODE_TYPE_ENTRY && info->waypoints[cur_wpi].node_type == 0 && info->waypoints[cur_wpi].short_range != 0);
    if (no_door) {
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
        return;
    }
    const vehicle_interaction_waypoint_transition* transition = info->get_transition(hm->transition_idx, cur_wpi, next_wpi);
    if (transition == NULL) {
        hm->enter_data.flags.teleport = 1;
        hm->exit_data.flags.teleport = 1;
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
        return;
    }
    const signed char component_idx = transition->component_obstacle_index;
    if (component_idx < 0) {
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
        return;
    }
    if (!vp->rdd.is_loaded()) {
        return;
    }
    const vehicle_component* component = &vp->rdd.get_data()->m_components[component_idx];
    if (component->get_type() != VCT_DOOR || (component->get_flags() & VCF_HINGED) == 0) {
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
        return;
    }
    if (vehicle_is_apc(vp)) {
        for (int a = 0; a < transition->num_interactions; a++) {
            if (transition->actions[a].action == VIA_TRANSITION_CLOSED_DOOR) {
                if (vi_door_close_for_climb(vii, hm, transition, 0.1f)) {
                    vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
                    return;
                }
                vii->process_state_again = 0;
                vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
                return;
            }
        }
    }
    if (vehicle_get_door_open_pct(vp, component_idx) > 0.6f && vi_door_open_action(vii, human_index, transition, 0.33f)) {
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
        return;
    }
    if (transition->play_animation(vp, hm, VIA_OPEN_DOOR)) {
        hm->door_comp_idx = component_idx;
        return;
    }
    for (int a = 0; a < transition->num_interactions; a++) {
        if (transition->actions[a].action == VIA_TRANSITION) {
            const int anim_handle = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, transition->actions[a].anim_action);
            const anim* animp = anim_get_anim_from_handle(anim_handle);
            if (animlib_get_trigger_time(Trigger_door_open_start, animp->animlib_handle, 0) >= 0) {
                hm->door_comp_idx = component_idx;
                vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
                return;
            }
        }
        if (transition->actions[a].action == VIA_SKIP_TO_NEXT_NODE) {
            vi_door_open_action(vii, human_index, transition, 0.33f);
            vi_set_anim_state(vii, human_index, VI_STATE_SHARED_TRANSITION);
            return;
        }
    }
    vi_door_open_action(vii, human_index, transition, 0.33f);
    vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
    vii->process_state_again = 0;
}

// 0x00BAC6A0 (:4471)
static void process_door_moving_human_recover_from_stuck(unsigned int human_index, human* hp, vehicle* vp)
{
    vi_human_manager* hm = &vp->vii.humans[human_index];
    vi_trace_step(&vp->vii, human_index, "door stuck");
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < 1.0f) {
        // A door that cannot move on a vehicle that is not moving: the waypoint outside it is blocked.
        const int cur_wpi = hm->path_nodes[hm->cur_path_idx];
        const int next_wpi = hm->path_nodes[hm->cur_path_idx + 1];
        const vehicle_interaction_waypoint* waypoints = vp->info->vi_info->waypoints;
        if (waypoints[next_wpi].seat_index == VSI_INVALID) {
            vp->vii.vvm.used_waypoints[next_wpi].block_type = VVM_WP_ENTRY_BLOCKED;
        }
        else if (waypoints[cur_wpi].seat_index == VSI_INVALID) {
            vp->vii.vvm.used_waypoints[cur_wpi].block_type = VVM_WP_ENTRY_BLOCKED;
        }
    }
    hm->door_comp_idx = -1;
    if (hm->interaction == VI_INTERACTION_ENTER) {
        hm->enter_data.flags.teleport = 1;
        vi_set_logic_state(&vp->vii, human_index, VI_STATE_ENTER_TELEPORT_WAIT);
        return;
    }
    if (hm->interaction != VI_INTERACTION_EXIT) {
        v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action != NULL) {
            vanim_pause_and_ramp_out_action(hp->char_inst->vp, 0x400, 3);
            action->alias = ANIM_ALIAS_NONE;
        }
        hm->door_comp_idx = -1;
        vi_set_anim_state(&vp->vii, human_index, VI_STATE_RIDE_DEFAULT);
        return;
    }
    vanim_pause_and_ramp_out_action(hp->char_inst->vp, 0x400, 3);
    v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
    if (action != NULL) {
        action->alias = ANIM_ALIAS_NONE;
    }
    hm->reserve_next_node_only = 0;
    hm->animate_dest_wpi = hm->animate_from_wpi;
    const vehicle_interaction_info* info = vp->info->vi_info;
    hm->ride_state_handle = -1;
    for (int t = 0; t < info->num_waypoint_transitions && hm->ride_state_handle == -1; t++) {
        const vehicle_interaction_waypoint_transition& transition = info->transitions[t];
        if (transition.destination_index != hm->animate_dest_wpi) {
            continue;
        }
        for (int a = 0; a < transition.num_interactions && hm->ride_state_handle == -1; a++) {
            if (transition.actions[a].action == VIA_TRANSITION || transition.actions[a].action == VIA_SKIP_TO_NEXT_NODE) {
                hm->ride_state_handle = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, transition.actions[a].anim_state);
            }
        }
    }
    if (hm->ride_state_handle == -1) {
        hp->char_inst->set_state(vi_index_alias(0x89), vi_index_alias(-1), 0.0f, false); // 0x89: the generic sitting state
    }
    else {
        vanim_change_custom_state(hp->char_inst->vp, vi_index_alias(hm->ride_state_handle), vi_index_alias(-1), 0.0f, false);
    }
    if (hm->exit_data.flags.last_exit_failed != 0 && object_is_player(hp)) {
        vi_exit_teleport(&vp->vii, human_index);
        return;
    }
    hm->exit_data.flags.last_exit_failed = 1;
    hm->wait_ts.set(-1);
    remove_human_from_seat(&vp->vii, human_index, hm->seat);
    vi_set_anim_state(&vp->vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
}

// One of the two trigger pairs of process_door_moving_human: the action's speed so the hand stays on the door.
static void vi_door_moving_set_speed(human* hp, const v_anim* action, float start, float length, float door_next, float door_actual, float cur_time)
{
    const float frame_offset = vi_dmh_anim_frame_offset / 30.0f;
    float speed = ((door_next * length + start + frame_offset) - cur_time) * Frames_per_second;
    speed = speed - 0.1f < 0.0f ? 0.1f : (speed - 1.1f >= 0.0f ? 1.1f : speed);
    if (door_actual * length + start + frame_offset < cur_time || !vi_dmh_use_time_not_speed) {
        speed = 0.1f;
    }
    else {
        const float current = animlib_get_current_speed_multiplier(hp->char_inst->vp->aim_handle, action->instance);
        speed += vi_dmh_speed_smoothing * (current - speed);
    }
    vanim_set_cur_time_and_speed(hp->char_inst->vp, 0x400, -1.0f, speed);
}

// 0x00BAC9A0 (:4566)
// While the human's hand is on a door, the door and the animation keep pace: the animation speeds up or waits, and a
// door that stops moving (blocked by something) is treated as a stuck entry.
static void process_door_moving_human(unsigned int human_index, human* hp, vehicle* vp)
{
    if (hp == NULL || hp->char_inst == NULL || !hp->is_transformed()) {
        return;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    const signed char component_idx = hm->door_comp_idx;
    if (component_idx == -1 || hm->door_idx == -1) {
        return;
    }
    vi_door* door = &vp->vii.vvm.doors[hm->door_idx];
    if (door->state == VI_DOOR_STATE_CLOSED_CLIMBING) {
        const int frame = vanim_get_anim_frame(hp->char_inst->vp, 0x400);
        if ((float)frame / 30.0f > vanim_get_anim_length(hp->char_inst->vp, 0x400) * 0.5f) {
            door->initialize();
        }
        return;
    }
    if (component_idx != door->component_id) {
        return;
    }
    vehicle_component* component = &vp->rdd.get_data()->m_components[component_idx];
    bool clear_door = true;
    if ((component->get_flags() & VCF_DETACHED) == 0) {
        const v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action == NULL || action->anim_handle == -1) {
            return;
        }
        const anim* animp = anim_get_anim_from_handle(action->anim_handle);
        if (animp == NULL) {
            return;
        }
        const vehicle_component::door_attr& attr = component->m_type_attribute.door;
        const float t_actual = attr.m_t_actual;
        const float step = attr.m_t_velocity * Frametime;
        float t_next = t_actual + step;
        t_next = t_next - 0.0f < 0.0f ? 0.0f : (t_next - 1.0f >= 0.0f ? 1.0f : t_next);
        float t_prev = attr.m_t - step;
        t_prev = t_prev - 0.0f < 0.0f ? 0.0f : (t_prev - 1.0f >= 0.0f ? 1.0f : t_prev);
        float t_target = vi_dmh_door_vel_offset * step + (vi_dmh_use_time_not_speed ? t_prev : t_actual);
        t_target = t_target - 0.0f < 0.0f ? 0.0f : (t_target - 1.0f >= 0.0f ? 1.0f : t_target);
        if (door->state == VI_DOOR_STATE_OPEN && t_actual < 0.75f) {
            if (t_actual < 0.02f) {
                if (door->open_close_ts.time_since() > 600) {
                    process_door_moving_human_recover_from_stuck(human_index, hp, vp);
                    return;
                }
            }
            else if (t_target - t_actual > 0.3f) {
                if (component->havok_handle == 0xFFFFFFFF) {
                    process_door_moving_human_recover_from_stuck(human_index, hp, vp);
                    return;
                }
                vector door_velocity;
                havok_body_get_linear_velocity(component->havok_handle, door_velocity);
                if (door_velocity.mag_squared() < 0.09f) {
                    process_door_moving_human_recover_from_stuck(human_index, hp, vp);
                    return;
                }
            }
        }
        const float cur_time = vanim_get_cur_time(hp->char_inst->vp, 0x400);
        // trigger times count 4800 per second
        const float open_start = (float)animlib_get_trigger_time(Trigger_door_open_start, animp->animlib_handle, 0) / 4800.0f;
        if (open_start >= 0.0f) {
            const float open_end = (float)animlib_get_trigger_time(Trigger_door_open_end, animp->animlib_handle, 0) / 4800.0f;
            if (open_end >= 0.0f) {
                const float length = open_end - open_start;
                const float progress = (cur_time - open_start) / length;
                if (t_target <= 0.6f) {
                    clear_door = false;
                    if (open_start <= cur_time || -progress <= t_target) {
                        vi_door_moving_set_speed(hp, action, open_start, length, t_next, t_actual, cur_time);
                        return;
                    }
                }
                else if (hm->state_of_anim == VI_STATE_SHARED_OPEN_DOOR && progress < t_target - 0.2f && hp->char_inst != NULL) {
                    vanim_pause_and_ramp_out_action(hp->char_inst->vp, 0x400, 6);
                    v_anim* current = vanim_get_action(hp->char_inst->vp, 0x400);
                    if (current != NULL) {
                        current->alias = ANIM_ALIAS_NONE;
                    }
                }
            }
        }
        const float close_start = (float)animlib_get_trigger_time(Trigger_door_close_start, animp->animlib_handle, 0) / 4800.0f;
        bool closing = false;
        if (close_start >= 0.0f) {
            const float close_end = (float)animlib_get_trigger_time(Trigger_door_close_end, animp->animlib_handle, 0) / 4800.0f;
            if (close_end >= 0.0f && t_target <= 0.9f) {
                closing = true;
                const float length = close_end - close_start;
                if (close_start <= cur_time || -((cur_time - close_start) / length) <= 1.0f - t_target) {
                    vi_door_moving_set_speed(hp, action, close_start, length, t_next, t_actual, cur_time);
                    return;
                }
            }
        }
        if (closing) {
            clear_door = false;
        }
    }
    if (clear_door) {
        hm->door_comp_idx = -1;
    }
    vanim_set_cur_time_and_speed(hp->char_inst->vp, 0x400, -1.0f, 1.0f);
}

// 0x00BB5900 (:4767)
static void process_shared_open_door(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    if (!hp->char_inst->is_playing_action(0x400)) {
        vi_trace_step(vii, human_index, "anim end: open door");
        set_shared_wait_waypoint_clear(vii, human_index);
        return;
    }
    if (hm->interaction == VI_INTERACTION_ENTER) {
        if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_camera, 0x400, -1)) {
            vi_utilities::switch_to_vehicle_camera(hp, vii->vvm.vp);
        }
        if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_relative_space, 0x400, -1)) {
            hm->use_vehicle_collision = -1;
            hm->pos_correction = VI_POS_CORRECTION_VEHICLE_VELOCITY;
            human_update_collision_filter(hp, true);
            human_enable_gravity(hp, false);
        }
    }
    else if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_camera, 0x400, -1)) {
        vi_utilities::switch_to_on_foot_camera(hp);
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_door_open_start, 0x400, -1)) {
        const anim* animp = anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, 0x400));
        const int open_ticks = animlib_get_trigger_time(Trigger_door_open_end, animp->animlib_handle, 0) - animlib_get_trigger_time(Trigger_door_open_start, animp->animlib_handle, 0);
        const vehicle_interaction_waypoint_transition* transition = vii->vvm.vp->info->vi_info->get_transition(hm->transition_idx, hm->path_nodes[hm->cur_path_idx], hm->path_nodes[hm->cur_path_idx + 1]);
        vi_door_open_action(vii, human_index, transition, (float)open_ticks / 4800.0f);
    }
}

// 0x00BB3D10 (:4828)
static void set_shared_transition(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    hm->state_of_anim = VI_STATE_SHARED_TRANSITION;
    const vehicle_interaction_info* info = vp->info->vi_info;
    const int next_wpi = hm->path_nodes[hm->cur_path_idx + 1];
    if (vii->vvm.used_waypoints[next_wpi].get_human_index() != (int)human_index) {
        vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
        vii->reregister_paths = 1;
        vii->process_state_again = 0;
        return;
    }
    const vehicle_interaction_waypoint_transition* transition = info->get_transition(hm->transition_idx, hm->path_nodes[hm->cur_path_idx], next_wpi);
    if (transition != NULL) {
        vi_clear_corpses(vii, next_wpi);
        const bool last_transition = !(hm->cur_path_idx + 2 < VI_MAX_PATH_NODES && hm->path_nodes[hm->cur_path_idx + 2] != -1);
        if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
            walker* wp = (walker*)vii->vvm.vp;
            if (hm->interaction == VI_INTERACTION_EXIT) {
                walker_play_VI_anim(wp, 4, info->waypoints[next_wpi].node_type);
            }
            else if (last_transition) {
                walker_play_VI_anim(wp, 1, 0);
            }
        }
        if (hm->interaction == VI_INTERACTION_EXIT) {
            remove_human_from_seat(vii, human_index, hm->seat);
        }
        hm->pos_correction = VI_POS_CORRECTION_VEHICLE_VELOCITY;
        human_enable_gravity(hp, false);
        human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
        human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
        if (info->waypoints[next_wpi].node_type == VI_NODE_TYPE_EXIT_GROUND) {
            hm->door_comp_idx = -1;
        }
        if (transition->play_animation(vp, hm, VIA_TRANSITION_CLOSED_DOOR) || transition->play_animation(vp, hm, VIA_TRANSITION)) {
            return;
        }
        if (transition->play_animation(vp, hm, VIA_SKIP_TO_NEXT_NODE)) {
            vi_trace_step(vii, human_index, "transition skipped to the next node");
            hm->cur_path_idx++;
            hm->wait_ts.set(3000);
            hm->reserve_next_node_only = 0;
            vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
            const int wpi = hm->effective_curr_wpi();
            if (vp->info->vi_info->waypoints[wpi].restrict_vertical_access == 0 || vp->info->vi_info->waypoints[wpi].node_type != 0) {
                hm->animate_from_wpi = (signed char)wpi;
                hm->animate_dest_wpi = (signed char)wpi;
                hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
                vi_update_human_pos(vii, human_index);
            }
            vii->process_state_again = 1;
            return;
        }
    }
    vi_trace_step(vii, human_index, "transition without an animation");
    release_curr_wp_node(vii, human_index);
    hm->cur_path_idx++;
    hm->wait_ts.set(3000);
    vi_set_anim_state(vii, human_index, VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR);
}

// 0x00B686C0 (:4948)
static void vi_human_to_projectile(vi_human_manager* hm, human* hp)
{
    if (hm->pos_correction == VI_POS_CORRECTION_HUMAN_PROJECTILE) {
        return;
    }
    hm->pos_correction = VI_POS_CORRECTION_HUMAN_PROJECTILE;
    human_enable_gravity(hp, true);
    hp->hflags.use_current_velocity = 1;
    hp->hflags.use_as_final_velocity = 1;
    // leave at the mean of the animation's speed and the speed the human was moving with
    const float x = (hp->velocity.x + hm->human_motion.x) * 0.5f;
    const float y = (hp->velocity.y + hm->human_motion.y) * 0.5f;
    const float z = (hp->velocity.z + hm->human_motion.z) * 0.5f;
    hp->velocity.x = x;
    hp->velocity.y = y;
    hp->velocity.z = z;
    hp->actual_velocity = hp->velocity;
    hp->actual_velocity_mag = sqrtf(y * y + x * x + z * z);
    hm->human_motion = hp->velocity;
    hm->human_motion.rotate(hp->char_inst->vp->starting_orient);
}

// 0x00BBE3F0 (:5000)
static void process_shared_transition(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->interaction == VI_INTERACTION_EXIT) {
        vi_update_exit_nodes(vii);
        if (hm->state_of_anim != VI_STATE_SHARED_TRANSITION) {
            return;
        }
    }
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_info* info = vp->info->vi_info;
    const bool last_transition = hm->cur_path_idx + 2 >= VI_MAX_PATH_NODES || hm->path_nodes[hm->cur_path_idx + 2] == -1;
    if (!hp->char_inst->is_playing_action(0x400)) {
        vi_trace_step(vii, human_index, "anim end: transition");
        release_curr_wp_node(vii, human_index);
        hm->cur_path_idx++;
        hm->wait_ts.set(3000);
        hm->animate_from_wpi = hm->animate_dest_wpi;
        hm->reserve_next_node_only = 0;
        set_shared_wait_waypoint_clear(vii, human_index);
        return;
    }
    if (hm->interaction == VI_INTERACTION_ENTER) {
        if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_camera, 0x400, -1)) {
            vi_utilities::switch_to_vehicle_camera(hp, vii->vvm.vp);
        }
        if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_relative_space, 0x400, -1)) {
            hm->use_vehicle_collision = -1;
            hm->pos_correction = VI_POS_CORRECTION_VEHICLE_VELOCITY;
            human_update_collision_filter(hp, true);
            human_enable_gravity(hp, false);
        }
    }
    else {
        if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_camera, 0x400, -1)) {
            vi_utilities::switch_to_on_foot_camera(hp);
        }
        const bool roof_exit = info->waypoints[hm->animate_dest_wpi].node_type == VI_NODE_TYPE_EXIT_ROOF;
        if (roof_exit && hm->reserve_next_node_only != 0 && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) {
            human_ragdoll_on_impact(hp, 0.5f, 0.1f, false);
        }
        if (hm->pos_correction != VI_POS_CORRECTION_NONE) {
            const bool relative_space = vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_relative_space, 0x400, -1);
            // landed on something other than the vehicle
            const bool on_ground = hm->pos_correction != VI_POS_CORRECTION_NONE && hp->ground_obj_handle != 0xFFFFFFFF && hp->ground_obj_handle != vp->get_handle();
            vector head;
            human_get_head_pos(hp, head);
            vector body_up(head.x - hp->pos.x, head.y - hp->pos.y, head.z - hp->pos.z);
            body_up.normalize();
            if ((on_ground || relative_space) && last_transition) {
                if (!roof_exit) {
                    v_animate* animate = hp->char_inst->vp;
                    const float up_y = animate->flags.interpolate_orient == 0 ? hp->orient.uvec.y : animate->interpolate_dest_uvec.y;
                    if (up_y < 0.999f) {
                        const int frame = vanim_get_anim_frame(animate, 0x400);
                        const float length = vanim_get_anim_length(hp->char_inst->vp, 0x400);
                        vanim_interpolate_uvec(hp->char_inst->vp, hp, UP_VECTOR, fl_acos(hp->orient.uvec.y) * 0.3f + (length - (float)frame / 30.0f));
                    }
                }
                if (object_is_player(hp) && roof_exit) {
                    vehicle_interaction_bail_all(vp);
                }
                if (on_ground) {
                    hm->pos_correction = VI_POS_CORRECTION_NONE;
                    human_update_collision_filter(hp, true);
                    human_enable_gravity(hp, true);
                    if ((body_up.y < 0.5f || roof_exit) && info->waypoints[hm->animate_dest_wpi].node_type != VI_NODE_TYPE_EXIT_GROUND) {
                        set_exit_eject(vii, human_index, 0xFFFFFFFF);
                        hp->knockdown_timestamp.set(400);
                        hp->steering_vector.zero();
                        return;
                    }
                    vanim_set_action_relative_space(hp->char_inst->vp, 0x400, NULL);
                    if (hp->rdd.is_loaded()) {
                        hp->rdd.get_data()->controller->flags.sliding = 0;
                    }
                }
                else {
                    vanim_set_action_relative_space(hp->char_inst->vp, 0x400, NULL);
                    vi_human_to_projectile(hm, hp);
                    hm->use_vehicle_collision = 0;
                    human_update_collision_filter(hp, true);
                    if (roof_exit || body_up.y < -0.70710677f) {
                        // Head first: tumble. set_exit_complete must not redraw the weapon meanwhile.
                        inventory_item* item = hp->desired_equipped_inv_item;
                        hp->desired_equipped_inv_item = NULL;
                        if (item == NULL) {
                            item = hp->last_equipped_weapon;
                        }
                        hp->last_equipped_weapon = NULL;
                        set_exit_complete(vii, human_index);
                        hp->last_equipped_weapon = item;
                        human_ragdoll_on_impact(hp, 0.3f, 0.1f, true);
                        human_set_state(hp, HS_FALL, HMS_RUN, -1.0f);
                        human_set_movement_mode(hp, HMM_FALL, HMSM_FALL_FREE);
                        hm->use_vehicle_collision = 0;
                        human_update_collision_filter(hp, true);
                        if (hp->rdd.is_loaded()) {
                            v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
                            if (action != NULL) {
                                action->alias = ANIM_ALIAS_NONE;
                                vanim_set_action_relative_space(hp->char_inst->vp, 0x400, NULL);
                            }
                        }
                        vector tumble;
                        tumble.rand_sphere();
                        vanim_interpolate_uvec(hp->char_inst->vp, hp, tumble, 8.0f);
                        return;
                    }
                    // half a second of gravity from where the human will be, and from the exit node
                    raycast_input from_human;
                    from_human.start = hp->pos;
                    from_human.end.x = hp->actual_velocity.x + UP_VECTOR.x * -9.8f * 0.5f + hp->pos.x;
                    from_human.end.y = hp->actual_velocity.y + UP_VECTOR.y * -9.8f * 0.5f + hp->pos.y;
                    from_human.end.z = hp->actual_velocity.z + UP_VECTOR.z * -9.8f * 0.5f + hp->pos.z;
                    from_human.radius = 0.0f;
                    from_human.filter_params = NULL;
                    from_human.filter_callback = NULL;
                    from_human.m_filter_info = COLLIDE_RAY_DEFAULT;
                    from_human.m_flags = 0;
                    from_human.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                    raycast_input from_node;
                    from_node.radius = 0.0f;
                    from_node.filter_params = NULL;
                    from_node.filter_callback = NULL;
                    from_node.m_filter_info = COLLIDE_RAY_DEFAULT;
                    from_node.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                    from_node.m_flags = 0;
                    info->get_waypoint_world_pos_orient(vp, hm->animate_dest_wpi, from_node.start, NULL, UP_VECTOR);
                    from_node.end.x = UP_VECTOR.x * -9.8f * 0.5f + from_node.start.x;
                    from_node.end.y = from_node.start.y + UP_VECTOR.y * -9.8f * 0.5f;
                    from_node.end.z = from_node.start.z + UP_VECTOR.z * -9.8f * 0.5f;
                    raycast_output hits;
                    if (!havok_raycast(from_human, hits) && !havok_raycast(from_node, hits)) {
                        // nothing below: falling
                        set_exit_complete(vii, human_index);
                        human_set_state(hp, HS_FALL, HMS_RUN, -1.0f);
                        human_set_movement_mode(hp, HMM_FALL, HMSM_FALL_START);
                        hm->use_vehicle_collision = 0;
                        human_update_collision_filter(hp, true);
                        if (hp->rdd.is_loaded()) {
                            v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
                            if (action != NULL) {
                                vanim_ramp_out_action(hp->char_inst->vp, 0x400, 10);
                                action->alias = ANIM_ALIAS_NONE;
                            }
                        }
                        vanim_interpolate_uvec(hp->char_inst->vp, hp, UP_VECTOR, 1.5f);
                        return;
                    }
                    if (hp->rdd.is_loaded()) {
                        hp->rdd.get_data()->controller->flags.sliding = 1;
                    }
                }
            }
        }
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_gravity_start, 0x400, -1)) {
        human_enable_gravity(hp, true);
        if (hp->rdd.is_loaded()) {
            vanim_set_action_relative_space(hp->char_inst->vp, 0x400, NULL);
        }
        vi_human_to_projectile(hm, hp);
        human_ragdoll_on_impact(hp, 0.5f, 0.1f, false);
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_door_open_start, 0x400, -1)) {
        const vehicle_interaction_info* vp_info = vp->info->vi_info;
        const vehicle_interaction_waypoint_transition* transition = vp_info->get_transition(hm->transition_idx, -1, -1);
        if (transition != NULL && transition->component_obstacle_index >= 0) {
            const anim* animp = anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, 0x400));
            const int open_ticks = animlib_get_trigger_time(Trigger_door_open_end, animp->animlib_handle, 0) - animlib_get_trigger_time(Trigger_door_open_start, animp->animlib_handle, 0);
            vi_door_open_action(vii, human_index, transition, (float)open_ticks / 4800.0f);
            if (vp_info->waypoints[hm->animate_dest_wpi].node_type == VI_NODE_TYPE_EXIT_GROUND) {
                hm->door_comp_idx = -1;
            }
        }
        if (vp_info->waypoints[hm->animate_dest_wpi].eject_on_explosion != 0 && last_transition) {
            turret* tp = vehicle_get_seat_turret(vii->vvm.vp, hm->enter_data.destination_seat);
            if (tp != NULL) {
                const matrix dest_orient = tp->get_operation_orient();
                vanim_set_action_dest_orient(hp->char_inst->vp, hp->orient, dest_orient, 0x400, hp->char_inst->vp->dest_orient_time);
            }
        }
        if (vp_info->waypoints[hm->animate_dest_wpi].node_type == VI_NODE_TYPE_EXIT_GROUND) {
            human_enable_gravity(hp, true);
        }
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_door_close_start, 0x400, -1)) {
        const anim* animp = anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, 0x400));
        const int close_ticks = animlib_get_trigger_time(Trigger_door_close_end, animp->animlib_handle, 0) - animlib_get_trigger_time(Trigger_door_close_start, animp->animlib_handle, 0);
        // Original bug kept: a door_idx of -1 reads the slot before the array.
        vi_door* door = &vii->vvm.doors[hm->door_idx];
        if (door->state == VI_DOOR_STATE_OPEN_FOR_GUNNER) {
            door->state = VI_DOOR_STATE_OPEN;
        }
        vi_door_close_action(vii, hm->door_idx, (float)close_ticks / 4800.0f);
        hm->door_idx = -1;
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_start, 0x400, -1)) {
        const vehicle_interaction_waypoint_transition* transition = vp->info->vi_info->get_transition(hm->transition_idx, -1, -1);
        if (transition != NULL && transition->component_obstacle_index >= 0) {
            if (!vp->rdd.is_loaded()) {
                return;
            }
            vehicle_component* component = &vp->rdd.get_data()->m_components[transition->component_obstacle_index];
            if ((component->get_flags() & VCF_GLASS) != 0) {
                vector head;
                human_get_head_pos(hp, head);
                const vector dir = hp->velocity.copy_normalize();
                vehicle_component_shatter_glass(component, vp, &head, &dir, &hp->velocity);
            }
        }
    }
    bool ramped_out = false;
    if (hm->interaction == VI_INTERACTION_EXIT && vanim_has_trigger_elapsed(hp->char_inst->vp, Trigger_release_movement, 0x400, -1)) {
        if (object_is_player(hp)) {
            human_set_movement_mode(hp, HMM_DIRECT, HMSM_NONE);
            const float forward = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
            const float turn = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
            if (fabsf(forward) > 0.2f || fabsf(turn) > 0.2f) {
                // the player takes over as soon as the stick moves
                vanim_pause_and_ramp_out_action(hp->char_inst->vp, 0x400, 5);
                ramped_out = true;
            }
        }
        if (!ramped_out && vii->exit_costs[vp->info->vi_info->waypoints[hm->path_dest_wpi()].node_type] == 0xFF) {
            set_exit_eject(vii, human_index, 0xFFFFFFFF);
            if (hm->state_of_anim != VI_STATE_SHARED_TRANSITION) {
                return;
            }
        }
    }
    if (!ramped_out) {
        const int anim_handle = vanim_get_anim_handle(hp->char_inst->vp, 0x400);
        if (anim_handle == -1) {
            return;
        }
        const anim* animp = anim_get_anim_from_handle(anim_handle);
        if (animp == NULL) {
            return;
        }
        const float ramp_out = animlib_get_animation_ramp_out_length(animp->animlib_handle);
        const int frame = vanim_get_anim_frame(hp->char_inst->vp, 0x400);
        const float length = vanim_get_anim_length(hp->char_inst->vp, 0x400);
        const float remaining_time = length - (float)frame / 30.0f;
        if (hm->reserve_next_node_only == 0) {
            vector from_pos;
            vp->info->vi_info->get_waypoint_world_pos_orient(vp, hm->animate_from_wpi, from_pos, NULL, UP_VECTOR);
            const float moved = vec_dist_squared(from_pos, hp->pos);
            // past the start of the animation: let go of the waypoint behind, keep only the next one reserved
            if (frame > 10 || (frame > 6 && moved > 0.2f)) {
                release_curr_wp_node(vii, human_index);
                hm->reserve_next_node_only = -1;
                hp->hflags.cant_hit_with_melee = 0;
            }
        }
        if (last_transition) {
            return;
        }
        if (hm->cur_path_idx + 2 < VI_MAX_PATH_NODES) {
            const int after_next = hm->path_nodes[hm->cur_path_idx + 2];
            // Original bug kept: a -1 node indexes before the waypoints here.
            const bool to_entry = hm->auto_connect_to_entry != 0 && info->waypoints[after_next].node_type == VI_NODE_TYPE_ENTRY;
            if (after_next != -1 && (info->is_auto_connected_transition(hm->animate_dest_wpi, after_next) || to_entry)) {
                hm->auto_connect_to_entry = 0;
                if (remaining_time <= vi_auto_connect_ramp_out && vanim_get_action(hp->char_inst->vp, 0x400)->move_to_end_orient == 0) {
                    vector pos;
                    matrix orient;
                    info->get_waypoint_world_pos_orient(vp, after_next, pos, &orient, UP_VECTOR);
                    vanim_set_action_dest_orient(hp->char_inst->vp, hp->orient, orient, 0x400, vi_auto_connect_ramp_out);
                }
            }
        }
        if (ramp_out < remaining_time) {
            return;
        }
        vanim_set_action_relative_space(hp->char_inst->vp, 0x400, NULL);
        v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action != NULL) {
            action->alias = ANIM_ALIAS_NONE;
        }
    }
    release_curr_wp_node(vii, human_index);
    hm->cur_path_idx++;
    hm->wait_ts.set(3000);
    hm->animate_from_wpi = hm->animate_dest_wpi;
    hm->reserve_next_node_only = 0;
    set_shared_wait_waypoint_clear(vii, human_index);
}

// 0x00BA2E20 (:5506)
static void set_exit_pathfind(vi_instance* vii, unsigned int human_index)
{
    const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->state == VI_STATE_EXIT_EJECT) {
        return;
    }
    vi_trace_step(vii, human_index, "exit: pathfind");
    const int anim_wpi = hm->effective_anim_wpi();
    if (anim_wpi != -1 && info->waypoints[anim_wpi].node_type > VI_NODE_TYPE_ENTRY) {
        return;
    }
    hm->exit_data.flags.maintain_position = vi_should_maintain_pos_on_exit(vii, human_index) ? 1 : 0;
    if (hm->exit_data.flags.teleport == 0 && hm->exit_data.flags.maintain_position == 0 && hm->pos_correction != VI_POS_CORRECTION_NONE && hm->cur_path_idx != -1
        && hm->state != VI_STATE_ENTER_TELEPORT_WAIT && hm->state != VI_STATE_ENTER_STREAMING_STALL) {
        const bool last_transition = !(hm->cur_path_idx + 2 < VI_MAX_PATH_NODES && hm->path_nodes[hm->cur_path_idx + 2] != -1);
        if (hm->state_of_anim == VI_STATE_SHARED_TRANSITION && last_transition && info->waypoints[hm->animate_dest_wpi].node_type > VI_NODE_TYPE_ENTRY
            && !vi_human_pf_check_raycast(vii, hm->animate_dest_wpi, hm->animate_from_wpi)) {
            return;
        }
        hm->state = VI_STATE_EXIT_PATHFIND;
        if (vii->post_stream_init == 0) {
            vii->post_stream_init = 1;
            for (int i = 0; i < info->num_waypoints; i++) {
                if (info->waypoints[i].node_type > VI_NODE_TYPE_DOOR_ENTRY) {
                    vii->vvm.used_waypoints[i].block_type = VVM_WP_ENTRY_CHECK;
                }
            }
        }
        vii->process_state_again = 1;
        return;
    }
    vi_exit_teleport(vii, human_index);
}

// 0x00BB6720 (:5565)
static void process_exit_pathfind(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (vii->reregister_paths != 0) {
        return;
    }
    if (vi_human_path_to_exit(vii, human_index)) {
        human* hp = vi_human_from_handle(hm->handle);
        human_override_default_animation_state(hp, true);
        if (hp->cur_state != HS_DEAD) {
            human_set_state(hp, HS_VEHICLE_EXITING, HMS_IDLE, -1.0f);
        }
        hm->state = VI_STATE_NONE;
        set_anim_process_continue(vii, human_index);
        return;
    }
    if (hm->handle == 0xFFFFFFFF) {
        return;
    }
    human* hp = vi_human_from_handle(hm->handle);
    if (object_is_player(hp)) {
        vi_exit_teleport(vii, human_index);
        return;
    }
    if (vii->vvm.vp->orient.uvec.y >= -0.70710677f) {
        vehicle_interaction_cancel(hp);
        hp->hflags.stuck_in_vehicle = 1;
        return;
    }
    // upside down with no way out
    if (!npc_is_defending_player(hp) && !hp->h_info->flags.vip) {
        human_damage(hp, NULL, -1.0f, DBT_CAR, NULL, NULL, true, false);
        return;
    }
    human_ragdoll_start(hp, false, 0xFFFFFFFF);
}

// 0x00BB1C70 (:5650)
static void process_exit_eject(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    if (hp == NULL) {
        vii->remove_human(human_index);
        return;
    }
    if (hp->ragdoll_idx == -1) {
        vi_set_logic_state(vii, human_index, VI_STATE_EXIT_EJECT);
        return;
    }
    if (!havok_ragdoll_is_added_to_world(hp->ragdoll_idx)) {
        return;
    }
    hm->exit_data.flags.teleport = 1;
    bool eject_seat = false;
    if (hm->cur_path_idx != -1) {
        const int wpi = hm->path_nodes[hm->cur_path_idx];
        const vehicle_interaction_info* info = vii->vvm.vp->info->vi_info;
        if (wpi != -1 && info != NULL && info->waypoints[wpi].eject_on_explosion != 0) {
            eject_seat = true;
        }
    }
    const vector hp_pos = hp->pos;
    const matrix hp_orient(hp->orient);
    const vector hp_velocity = hp->actual_velocity;
    human_combat_abandon_turret(hp);
    vi_set_logic_state(vii, human_index, VI_STATE_EXIT_COMPLETE);
    human_weapons_immediately_holster_weapon(hp);
    hp->char_inst->set_animation_group(ANIMATION_GROUP_DEFAULT, vi_index_alias(-1), 0.3f, false);
    hkpRigidBody* body = havok_body_get_pointer(vii->vvm.vp->havok_handle);
    havok_ragdoll_unsync(hp->ragdoll_idx, hp_pos, hp_orient, hp_velocity, 1.0f);
    havok_ragdoll_jerk(hp->ragdoll_idx, 0.25f);
    havok_ragdoll_activate(hp->ragdoll_idx);
    // The spin goes on as the vehicle's. rfg.exe also works out the vehicle's velocity plus 2 m/s up here and overwrites it.
    vector spin;
    havok_to_game_vector(spin, body->getAngularVelocity());
    spin.x *= 4.0f;
    spin.y *= 4.0f;
    spin.z *= 4.0f;
    havok_ragdoll_apply_angular_velocity(hp->ragdoll_idx, spin, 0xFFFFFFFF);
    vector ragdoll_pos;
    havok_ragdoll_get_pos(hp->ragdoll_idx, ragdoll_pos, RAGDOLL_BODY);
    if (eject_seat) {
        const vector& up = vii->vvm.vp->orient.uvec;
        ragdoll_pos.x += up.x * eject_offset;
        ragdoll_pos.y += up.y * eject_offset;
        ragdoll_pos.z += up.z * eject_offset;
    }
    raycast_input in;
    in.filter_params = NULL;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.start = ragdoll_pos;
    in.start.y += 5.0f;
    in.end = ragdoll_pos;
    in.end.y -= 5.0f;
    in.filter_callback = NULL;
    in.radius = 0.0f;
    in.m_filter_info = COLLIDE_RAY_TERRAIN_ONLY;
    raycast_output out;
    if (havok_raycast(in, out) && out.m_hits[0].hit_pos.y > ragdoll_pos.y - 0.5f) {
        // keep it half a metre above the terrain
        ragdoll_pos.y = out.m_hits[0].hit_pos.y + 0.5f;
        havok_ragdoll_set_pos(hp->ragdoll_idx, ragdoll_pos);
    }
    else if (eject_seat) {
        havok_ragdoll_set_pos(hp->ragdoll_idx, ragdoll_pos);
    }
}

// 0x00BB2180 (:5793)
static void set_exit_eject(vi_instance* vii, unsigned int human_index, unsigned int perp)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_waypoint* waypoints = vp->info->vi_info->waypoints;
    const bool vehicle_lets_go = vp->is_killed() || vp->m_flags.no_ragdoll == 0 || hm->interaction != VI_INTERACTION_EXIT || waypoints[hm->animate_dest_wpi].node_type == VI_NODE_TYPE_EXIT_ROOF;
    if (!(hp->is_killed() || (vehicle_lets_go && !human_is_tied_up(hp))) || hm->state == VI_STATE_EXIT_EJECT) {
        return;
    }
    vi_trace_step(vii, human_index, "exit: eject");
    if (hm->interaction == VI_INTERACTION_RIDE) {
        const vehicle_interaction_waypoint& wp = waypoints[hm->path_nodes[hm->cur_path_idx]];
        if (wp.node_type <= VI_NODE_TYPE_DOOR_ENTRY && wp.last_choice == 0 && wp.eject_on_explosion == 0) {
            hp->hflags.prevent_ragdoll_sfx = 1;
        }
    }
    hm->reserve_next_node_only = 0;
    hm->state = VI_STATE_EXIT_EJECT;
    hm->interaction = VI_INTERACTION_EXIT;
    if (object_is_local_player(hp)) {
        vi_utilities::switch_to_on_foot_camera(hp);
    }
    if (hm->pos_correction == VI_POS_CORRECTION_NONE) {
        vi_set_logic_state(vii, human_index, VI_STATE_EXIT_COMPLETE);
        human_ragdoll_start(hp, false, 0xFFFFFFFF);
        return;
    }
    vanim_set_state_directional_blend_pose(hp->char_inst->vp, ZERO_VECTOR, 0.0f);
    human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
    human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
    if (hp->cur_state != HS_DEAD) {
        human_set_state(hp, HS_VEHICLE_EXITING, HMS_IDLE, -1.0f);
    }
    if (hp->ragdoll_idx != -1) {
        havok_ragdoll_remove_ignore_property(hp->ragdoll_idx);
    }
    hp->velocity = hm->human_motion;
    human_controller_set_vel(hp, hp->velocity);
    hp->hflags.use_current_velocity = 0;
    hp->char_inst->vp->flags.use_velocity = 0;
    human_ragdoll_start(hp, false, perp);
    process_exit_eject(vii, human_index);
}

// 0x00B90290 (:5864)
static bool vi_get_exit_teleport_point(vi_human_manager* hm, vehicle* vp)
{
    human* hp = vi_human_from_handle(hm->handle);
    const vehicle_interaction_info* info = vp->info->vi_info;
    if (hm->last_path_idx >= 0) {
        const int wpi = hm->path_nodes[hm->last_path_idx];
        if (wpi >= 0 && info->waypoints[wpi].node_type >= VI_NODE_TYPE_EXIT_FIRST && vp->vii.vvm.used_waypoints[wpi].block_type == VVM_WP_ENTRY_CLEAR) {
            info->get_waypoint_world_pos_orient(vp, wpi, hp->pos, &hp->orient, UP_VECTOR);
            return true;
        }
    }
    vector candidates[30];
    int count = 0;
    for (int i = 0; i < info->num_waypoints; i++) {
        if (info->waypoints[i].node_type > VI_NODE_TYPE_ENTRY && vp->vii.vvm.used_waypoints[i].block_type != VVM_WP_ENTRY_BLOCKED) {
            info->get_waypoint_world_pos_orient(vp, i, candidates[count], NULL, UP_VECTOR);
            count++;
        }
    }
    vector place;
    if (human_find_place_to_stand_from_list(place, hp->pos, candidates, count, 0xFFFFFFFF)) {
        const vector to_place(place.x - hp->pos.x, place.y - hp->pos.y, place.z - hp->pos.z);
        const vector facing = to_place.copy_flatten_normalize_safe();
        hp->orient.make_quick(facing);
        hp->pos = place;
        return true;
    }
    if (!object_is_player(hp) && !hp->h_info->flags.vip) {
        return false;
    }
    // Who must not be lost: anywhere within 3, then 10 m of where the camera looks, or a tank's first exit node.
    vector seek(hp->pos.x + Rfg_camera.real_orient.fvec.x * 0.5f, Rfg_camera.real_orient.fvec.y * 0.5f + hp->pos.y, Rfg_camera.real_orient.fvec.z * 0.5f + hp->pos.z);
    const base_vehicle_types classification = vp->info->vehicle_classification;
    if (classification == BVT_TANK || classification == BVT_ART_TANK) {
        for (int i = 0; i < info->num_waypoints; i++) {
            if (info->waypoints[i].node_type > VI_NODE_TYPE_ENTRY) {
                info->get_waypoint_world_pos_orient(vp, i, seek, NULL, UP_VECTOR);
                break;
            }
        }
    }
    if (human_find_place_to_stand(place, hp, 3.0f, &seek) || human_find_place_to_stand(place, hp, 10.0f, &seek)) {
        // Original bug kept: turns to the new place only when it is within 0.1 of the human, not when it is far.
        if (vec_equal_fast(place, hp->pos, 0.1f)) {
            const vector to_place(place.x - hp->pos.x, place.y - hp->pos.y, place.z - hp->pos.z);
            hp->orient.make(to_place);
        }
        hp->pos = place;
        return true;
    }
    return false;
}

// 0x00B81920 (:5959)
static bool vi_should_maintain_pos_on_exit(vi_instance* vii, unsigned int human_index)
{
    const vi_human_manager& hm = vii->humans[human_index];
    if (hm.exit_data.flags.maintain_position != 0) {
        return true;
    }
    if (vii->updated_exits != 0 && vii->exit_mode == 0) {
        return false;
    }
    if (hm.cur_path_idx != -1) {
        if (hm.enter_data.flags.teleport_to_door != 0) {
            return false;
        }
        const int anim_wpi = hm.effective_anim_wpi();
        if (anim_wpi != -1) {
            return vii->vvm.vp->info->vi_info->waypoints[anim_wpi].node_type > VI_NODE_TYPE_DOOR_ENTRY;
        }
    }
    return true;
}

// 0x00B98AE0 (:6003)
static void vi_exit_teleport(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    hm->interaction = VI_INTERACTION_EXIT;
    human_set_state(hp, HS_VEHICLE_EXITING, HMS_CURRENT, -1.0f);
    vi_trace_step(vii, human_index, "exit: teleport out of the seat, no animation");
    hm->exit_data.flags.maintain_position = vi_should_maintain_pos_on_exit(vii, human_index) ? 1 : 0;
    if (hm->exit_data.flags.maintain_position != 0) {
        // Under the terrain (a vehicle at the bottom of a pit): do not stay.
        raycast_input in;
        in.start = hp->pos;
        in.start.y += 4.0f;
        in.end.x = hp->pos.x;
        in.end.y = -1000.0f;
        in.end.z = hp->pos.z;
        in.radius = 0.0f;
        in.filter_params = NULL;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        in.m_filter_info = COLLIDE_RAY_TERRAIN_ONLY;
        in.filter_callback = NULL;
        raycast_output out;
        float terrain_y = 0.0f;
        if (havok_raycast(in, out)) {
            terrain_y = out.m_hits[0].hit_pos.y;
        }
        else {
            in.start.y = 1000.0f;
            if (havok_raycast(in, out)) {
                terrain_y = out.m_hits[0].hit_pos.y;
            }
        }
        if (hp->pos.y < terrain_y - 0.25f) {
            hm->exit_data.flags.maintain_position = 0;
        }
    }
    if (hm->exit_data.flags.maintain_position == 0 && vi_get_exit_teleport_point(hm, vii->vvm.vp)) {
        hp->update_pos_and_orient(hp->pos, hp->orient, true);
        vehicle_get_velocity(vii->vvm.vp, hp->velocity);
        hp->hflags.use_current_velocity = 1;
        hp->hflags.use_as_final_velocity = 1;
        if (hp->rdd.is_loaded()) {
            hp->char_inst->vp->flags.use_velocity = 1;
        }
        hm->pos_correction = VI_POS_CORRECTION_NONE;
    }
    if (hp->rdd.is_loaded()) {
        const bool maintain = hm->exit_data.flags.maintain_position != 0;
        v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action != NULL) {
            vanim_ramp_out_action(hp->char_inst->vp, 0x400, maintain ? 10 : 0);
            action->alias = ANIM_ALIAS_NONE;
        }
        vanim_change_state(hp->char_inst->vp, vi_index_alias(1), vi_index_alias(-1), maintain ? 0.3f : 0.001f, false);
    }
    vi_set_logic_state(vii, human_index, VI_STATE_EXIT_COMPLETE);
}

// 0x00BB23A0 (:6080)
static void set_exit_complete(vi_instance* vii, unsigned int human_index)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    hm->state = VI_STATE_EXIT_COMPLETE;
    vi_trace_step(vii, human_index, "exit: complete");
    vi_utilities::switch_to_on_foot_camera(hp);
    human_enable_gravity(hp, true);
    human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
    human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
    if (hm->pos_correction != VI_POS_CORRECTION_HUMAN_PROJECTILE) {
        v_animate* animate = hp->char_inst->vp;
        const float up_y = animate->flags.interpolate_orient == 0 ? hp->orient.uvec.y : animate->interpolate_dest_uvec.y;
        if (up_y < 0.999f) {
            vanim_interpolate_uvec(animate, hp, UP_VECTOR, 0.4f);
        }
    }
    if (hp->char_inst->rdd.is_loaded()) {
        hp->rdd.get_data()->controller->flags.sliding = 0;
        object_set_visibility_plus_children(hp, true);
    }
    if (object_is_player(hp)) {
        if (human_get_prop(hp, PROP_TYPE_ACCESSORY_MISC, NULL) != NULL) {
            player_hide_jetpack_straps(hp);
        }
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && vp->rdd.is_loaded()) {
            automobile_reset_handling_for_ai((automobile*)vp, true, false);
        }
        Rfg_camera.free_params.flags.look_back = 0;
        Rfg_camera.free_params.flags.look_back_last = 0;
        player* pp = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : NULL;
        pp->pflags.crouching_state = 0;
    }
    hp->hflags.cant_hit_with_melee = 0;
    vanim_set_state_directional_blend_pose(hp->char_inst->vp, ZERO_VECTOR, 0.0f);
    human_override_default_animation_state(hp, false);
    if (!hp->is_killed() && !human_is_ragdolled(hp)) {
        if (object_is_player(hp)) {
            human_set_movement_mode(hp, HMM_DIRECT, HMSM_NONE);
            ((player*)hp)->movement_velocity.zero();
        }
        else {
            human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
        }
        human_set_state(hp, HS_STANDARD, HMS_CURRENT, -1.0f);
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER) {
            inventory_item* item = hp->desired_equipped_inv_item;
            if (item == NULL) {
                item = hp->last_equipped_weapon;
            }
            human_equip_inventory_item(hp, item);
        }
    }
    hm->use_vehicle_collision = 0;
    human_update_collision_filter(hp, true);
    vii->remove_human(human_index);
    if (vp->m_flags.m_occupied != 0 && vp->vii.num_humans < 1) {
        vehicle_make_abandoned(vp->get_handle());
    }
    // Original bug kept: remove_human has just cleared exit_data, so this never runs.
    if (hm->exit_data.flags.teleport != 0 && !hp->is_killed()) {
        human_set_stance(hp, HST_CROUCH);
        hp->crouch_to_stand_test_ts.set(0);
        if (hp->rdd.is_loaded()) {
            hp->rdd.get_data()->controller->update_collision_shape_mode(hp);
        }
    }
}

// 0x00BB3F80 (:6197)
static void set_ride_default(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_info* info = vp->info->vi_info;
    hm->state_of_anim = VI_STATE_RIDE_DEFAULT;
    human* hp = vi_human_from_handle(hm->handle);
    const signed char wpi = hm->path_nodes[hm->cur_path_idx];
    if (info->waypoints[wpi].last_choice == 0 && info->waypoints[wpi].eject_on_explosion == 0) {
        hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
        hp->hflags.cant_hit_with_melee = vp->info->default_team == HUMAN_TEAM_EDF || object_is_player(hp) ? 1 : 0;
    }
    else {
        if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_BASH_POS) {
            vi_update_human_pos(vii, human_index);
        }
        hm->pos_correction = VI_POS_CORRECTION_VEHICLE_VELOCITY;
    }
    vehicle_get_velocity(vp, hm->human_motion);
    hm->animate_from_wpi = wpi;
    hm->animate_dest_wpi = wpi;
    hp->rdd.get_data()->controller->update_collision_shape_mode(hp);
    human_set_stance(hp, HST_STAND);
    if (hp->cur_state != HS_VEHICLE_DRIVING && hp->cur_state != HS_VEHICLE_SITTING) {
        human_override_default_animation_state(hp, true);
    }
    if (hm->ride_state_handle == -1) {
        for (int t = 0; t < info->num_waypoint_transitions && hm->ride_state_handle == -1; t++) {
            const vehicle_interaction_waypoint_transition& transition = info->transitions[t];
            if (transition.destination_index != hm->path_nodes[hm->cur_path_idx]) {
                continue;
            }
            for (int a = 0; a < transition.num_interactions && hm->ride_state_handle == -1; a++) {
                const vehicle_transition_action& action = transition.actions[a];
                if (action.action != VIA_TRANSITION && action.action != VIA_SKIP_TO_NEXT_NODE) {
                    continue;
                }
                hm->ride_state_handle = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, action.anim_state);
                if (hm->seat == VSI_DRIVER) {
                    continue;
                }
                // a gunner in the door keeps the door open
                vehicle_turret_mount* mount = vehicle_get_seat_turret_mount(vp, hm->seat);
                if (mount != NULL && mount->attached_turret != 0xFFFFFFFF) {
                    object* objp = World.object_from_handle(mount->attached_turret);
                    if (objp != NULL && objp->get_obj_type() == OT_TURRET && transition.component_obstacle_index != -1) {
                        vi_door_open_action(vii, human_index, &transition, 0.01f);
                        vii->vvm.doors[hm->door_idx].state = VI_DOOR_STATE_OPEN_FOR_GUNNER;
                    }
                }
            }
        }
    }
    if (hm->ride_state_handle != -1) {
        object_set_visibility_plus_children(hp, true);
        if (object_is_player(hp)) {
            const anim* animp = anim_get_anim_from_handle(hm->ride_state_handle);
            for (unsigned int v = 0; v < animp->num_variants; v++) {
                // variant 6: what the player's backpack does in this seat
                if (animp->variants[v].variant_id == 6) {
                    item* torso = human_get_prop(hp, PROP_TYPE_TORSO, NULL);
                    if (torso != NULL) {
                        vanim_change_custom_state(torso->char_inst->vp, index_alias(animp->variants[v].anim_handle, ANIM_ALIAS_IDLE), vi_index_alias(-1), 0.3f, false);
                    }
                }
            }
            if (human_get_prop(hp, PROP_TYPE_ACCESSORY_MISC, NULL) != NULL) {
                player_hide_jetpack_straps(hp);
            }
        }
        vanim_change_custom_state(hp->char_inst->vp, vi_index_alias(hm->ride_state_handle), vi_index_alias(-1), hm->enter_data.flags.teleport != 0 ? 0.0f : 0.3f, false);
    }
    else if (hp->cur_state == HS_VEHICLE_DRIVING) {
        hp->char_inst->set_state(vi_index_alias(0xBE), vi_index_alias(-1), 0.3f, false);
    }
    else if (hp->cur_state == HS_VEHICLE_SITTING) {
        hp->char_inst->set_state(vi_index_alias(0x89), vi_index_alias(-1), 0.3f, false);
    }
    // otherwise rfg.exe prints "Int3 critical function at %s(%d)" and carries on
    GAME_TRACE("vehicle", "ride: human=0x%X vehicle=%s seat=%d state %s", hm->handle, vp->info->name, (int)hm->seat, vi_trace_anim(hm->ride_state_handle));
    if (Rfg_camera.mode != CAMERA_FREE_MODE) {
        vi_utilities::switch_to_vehicle_camera(hp, vp);
    }
    hm->enter_data.flags.teleport = 0;
    hm->enter_data.flags.minimal_processing = 0;
    vii->process_state_again = 0;
    if (vp->m_flags.invisible_driver != 0) {
        object_set_visibility_plus_children(hp, false);
        hp->hflags.disallow_vehicle_exit = 1;
        hp->hflags.invulnerable = 1;
    }
}

// 0x00BB5AD0 (:6381)
static void process_ride_default(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    if (hm->interaction != VI_INTERACTION_RIDE) {
        if (hm->cur_path_idx < 5 && hm->path_nodes[hm->cur_path_idx + 1] != -1) {
            set_shared_wait_waypoint_clear(vii, human_index);
        }
        return;
    }
    human* hp = vi_human_from_handle(hm->handle);
    if (hp->cur_state != HS_VEHICLE_DRIVING && hp->cur_state != HS_VEHICLE_SITTING) {
        human_set_state(hp, hm->seat == VSI_DRIVER ? HS_VEHICLE_DRIVING : HS_VEHICLE_SITTING, HMS_IDLE, 0.0f);
        human_update_collision_filter(hp, true);
    }
    if (hm->exit_data.flags.reexit != 0) {
        hm->exit_data.flags.reexit = 0;
        vi_request_exit(vii, human_index, false);
        return;
    }
    if (hm->seat == VSI_INVALID) {
        if (object_is_player(hp)) {
            vi_request_exit(vii, human_index, false);
        }
        else {
            vi_request_npc_exit(vii, human_index, human_index);
        }
    }
    vehicle* vp = vii->vvm.vp;
    if (vp->info->vi_info == NULL) {
        return;
    }
    const bool reversing = vp->m_flags.m_reverse_last_frame != 0 && vp->forward_velocity < -0.1f;
    const int frames_skipped = hp->transform_frames_skipped;
    if (hm->door_idx != -1) {
        const vi_door& door = vii->vvm.doors[hm->door_idx];
        if (door.component_id != -1 && door.state != VI_DOOR_STATE_OPEN_FOR_GUNNER && hm->manual_close_door_ts.elapsed()) {
            hm->manual_close_door_ts.set(2000);
            const unsigned int component = door.component_id;
            const float open_pct = vehicle_get_door_open_pct(vii->vvm.vp, component);
            if (!vehicle_component_is_attached(vii->vvm.vp, component)) {
                hm->door_idx = -1;
            }
            else if (open_pct > 0.2f) {
                set_ride_close_door(vii, human_index);
                return;
            }
        }
    }
    if (reversing) {
        // variant 10: the look over the shoulder
        const anim* animp = anim_get_anim_from_handle(hm->ride_state_handle);
        for (unsigned int v = 0; v < animp->num_variants; v++) {
            if (animp->variants[v].variant_id == 10) {
                vanim_change_custom_state(hp->char_inst->vp, index_alias(animp->variants[v].anim_handle, ANIM_ALIAS_IDLE), vi_index_alias(-1), 0.6f, false);
            }
        }
    }
    else if (hm->ride_state_handle == -1) {
        const int state = hp->cur_state == HS_VEHICLE_DRIVING ? 0xBE : 0x89;
        if (!hp->char_inst->is_in_state(state)) {
            hp->char_inst->set_state(vi_index_alias(state), vi_index_alias(-1), 0.3f, false);
        }
    }
    else {
        // 50 frames unanimated: snap
        vanim_change_custom_state(hp->char_inst->vp, vi_index_alias(hm->ride_state_handle), vi_index_alias(-1), frames_skipped < 50 ? 0.3f : 0.0f, false);
    }
    const bool hands_frozen = hp->ik_joints[1].freeze_obj_handle != 0xFFFFFFFF && hp->ik_joints[0].freeze_obj_handle != 0xFFFFFFFF;
    const bool settled = hp->char_inst->vp->desired_state->blend > 0.999f && hp->transform_frames_skipped == 0;
    hm->freeze_hands_next_frame = 0;
    if (settled) {
        if (!hands_frozen) {
            hm->freeze_hands_next_frame = -1;
            hp->hflags.dont_lod_bones = 1;
        }
    }
    else if (hands_frozen) {
        human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
        human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
    }
}

// 0x00B908B0 (:6487)
static void set_ride_close_door(vi_instance* vii, unsigned int human_index)
{
    vehicle* vp = vii->vvm.vp;
    vi_human_manager* hm = &vii->humans[human_index];
    const vehicle_interaction_info* info = vp->info->vi_info;
    hm->state_of_anim = VI_STATE_RIDE_CLOSE_DOOR;
    if (!vp->rdd.is_loaded() || hm->door_idx == -1 || hm->cur_path_idx < 1) {
        vi_set_anim_state(vii, human_index, VI_STATE_RIDE_DEFAULT);
        return;
    }
    // -1 asks get_transition to search; any other value is used as an index unchecked.
    signed char transition_idx = -1;
    const vehicle_interaction_waypoint_transition* transition = info->get_transition(transition_idx, hm->path_nodes[hm->cur_path_idx - 1], hm->path_nodes[hm->cur_path_idx]);
    if (transition != NULL) {
        bool has_close_door = false;
        for (int a = 0; a < transition->num_interactions; a++) {
            if (transition->actions[a].action == VIA_CLOSE_DOOR) {
                has_close_door = true;
                break;
            }
        }
        if (!has_close_door) {
            hm->door_idx = -1;
            vi_set_anim_state(vii, human_index, VI_STATE_RIDE_DEFAULT);
            return;
        }
        hm->door_comp_idx = transition->component_obstacle_index;
        transition->play_animation(vp, hm, VIA_CLOSE_DOOR);
    }
    human* hp = vi_human_from_handle(hm->handle);
    human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
    human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
}

// 0x00BB5E70 (:6529)
static void process_ride_close_door(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    if (!hp->char_inst->is_playing_action(0x400)) {
        set_ride_default(vii, human_index);
        return;
    }
    hp->char_inst->vp->flags.adjusting_orient = 1;
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_door_close_start, 0x400, -1)) {
        const anim* animp = anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, 0x400));
        const int close_ticks = animlib_get_trigger_time(Trigger_door_close_end, animp->animlib_handle, 0) - animlib_get_trigger_time(Trigger_door_close_start, animp->animlib_handle, 0);
        vi_door_close_action(vii, hm->door_idx, (float)close_ticks / 4800.0f);
        hm->door_idx = -1;
    }
}

// 0x00BB5F80 (:6558)
static void process_ride_custom_anim(vi_instance* vii, unsigned int human_index)
{
    human* hp = vi_human_from_handle(vii->humans[human_index].handle);
    if (!hp->char_inst->is_playing_action(0x400)) {
        set_ride_default(vii, human_index);
        return;
    }
    if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_enable_spinebend, 0x400, -1)) {
        const unsigned int player_handle = player_get_local_player()->get_handle();
        if (hp->hflags.aim_at_pos != 0 && hp->look_at_handle == player_handle) {
            human_set_look_at_target(hp, ZERO_VECTOR, true, 0.9f);
        }
        else {
            human_set_look_at_target(hp, player_handle, ZERO_VECTOR, true, -1.0f);
        }
    }
}

// 0x00BB7BA0 (:6586)
static void vi_set_logic_state(vi_instance* vii, const unsigned int human_index, vi_state_of_logic state)
{
    switch (state) {
    case VI_STATE_RESERVED_SEAT:
        set_enter_path_to_door(vii, human_index);
        break;
    case VI_STATE_ENTER_PATHFIND:
        set_enter_pathfind(vii, human_index);
        break;
    case VI_STATE_ENTER_TELEPORT_WAIT:
        set_enter_teleport_wait(vii, human_index);
        break;
    case VI_STATE_ENTER_STREAMING_STALL:
        set_enter_streaming_stall(vii, human_index);
        break;
    case VI_STATE_ENTER_COMPLETE:
        set_enter_complete(vii, human_index);
        break;
    case VI_STATE_EXIT_PATHFIND:
        set_exit_pathfind(vii, human_index);
        break;
    case VI_STATE_EXIT_EJECT:
        set_exit_eject(vii, human_index, 0xFFFFFFFF);
        break;
    case VI_STATE_EXIT_COMPLETE:
        set_exit_complete(vii, human_index);
        break;
    default:
        break;
    }
}

// 0x00BB6880 (:6635)
static void vi_process_logic_state(vi_instance* vii, unsigned int human_index)
{
    switch (vii->humans[human_index].state) {
    case VI_STATE_ENTER_PATHFIND:
        process_enter_pathfind(vii, human_index);
        break;
    case VI_STATE_ENTER_TELEPORT_WAIT:
        process_enter_teleport_wait(vii, human_index);
        break;
    case VI_STATE_ENTER_STREAMING_STALL:
        process_enter_streaming_stall(vii, human_index);
        break;
    case VI_STATE_EXIT_PATHFIND:
        process_exit_pathfind(vii, human_index);
        break;
    case VI_STATE_EXIT_EJECT:
        process_exit_eject(vii, human_index);
        break;
    default:
        break;
    }
}

// 0x00BB4410 (:6679)
static void vi_set_anim_state(vi_instance* vii, const unsigned int human_index, vi_state_of_anim state)
{
    switch (state) {
    case VI_STATE_ANIM_NONE:
        vii->humans[human_index].state_of_anim = VI_STATE_ANIM_NONE;
        break;
    case VI_STATE_ENTER_PATH_TO_DOOR:
        set_enter_path_to_door(vii, human_index);
        break;
    case VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR:
        set_shared_wait_waypoint_clear(vii, human_index);
        break;
    case VI_STATE_SHARED_OPEN_DOOR:
        set_shared_open_door(vii, human_index);
        break;
    case VI_STATE_SHARED_TRANSITION:
        set_shared_transition(vii, human_index);
        break;
    case VI_STATE_RIDE_DEFAULT:
        set_ride_default(vii, human_index);
        break;
    case VI_STATE_RIDE_CLOSE_DOOR:
        set_ride_close_door(vii, human_index);
        break;
    default:
        break;
    }
}

// 0x00BBF580 (:6721)
static void vi_process_anim_state(vi_instance* vii, unsigned int human_index)
{
    if (!vii->vvm.vp->rdd.is_loaded()) {
        return;
    }
    const vi_human_manager& hm = vii->humans[human_index];
    if (hm.handle == 0xFFFFFFFF) {
        return;
    }
    // Original bug kept: a handle that is no longer a human reads through NULL here.
    human* hp = vi_human_from_handle(hm.handle);
    if (!hp->rdd.is_loaded()) {
        return;
    }
    switch (hm.state_of_anim) {
    case VI_STATE_ENTER_PATH_TO_DOOR:
        process_enter_path_to_door(vii, human_index);
        break;
    case VI_STATE_SHARED_WAIT_WAYPOINT_CLEAR:
        process_shared_wait_waypoint_clear(vii, human_index);
        break;
    case VI_STATE_SHARED_OPEN_DOOR:
        process_shared_open_door(vii, human_index);
        break;
    case VI_STATE_SHARED_TRANSITION:
        process_shared_transition(vii, human_index);
        break;
    case VI_STATE_RIDE_DEFAULT:
        process_ride_default(vii, human_index);
        break;
    case VI_STATE_RIDE_CLOSE_DOOR:
        process_ride_close_door(vii, human_index);
        break;
    case VI_STATE_RIDE_CUSTOM_ANIM:
        process_ride_custom_anim(vii, human_index);
        break;
    default:
        break;
    }
}

// 0x00BB44D0 (:7108)
static void vi_request_exit(vi_instance* vii, unsigned int human_index, bool switching_seat)
{
    vi_human_manager* hm = &vii->humans[human_index];
    // an NPC on its way out asks again every frame
    if (hm->interaction != VI_INTERACTION_EXIT) {
        vi_trace_step(vii, human_index, switching_seat ? "seat switch requested" : "exit requested");
    }
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    if (hm->interaction == VI_INTERACTION_EXIT) {
        if (hm->exit_data.flags.teleport != 0) {
            vi_exit_teleport(vii, human_index);
        }
        return;
    }
    if (hp->hflags.disallow_vehicle_exit != 0 || human_is_tied_up(hp)) {
        return;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        // Original bug kept: rfg.exe's cast can give NULL when the object type is wrong.
        walker* wp = vp->get_obj_type() == OT_VEHICLE ? (walker*)vp : NULL;
        if (wp->mount_vehicle == 0xFFFFFFFF && wp->ground_obj_handle == 0xFFFFFFFF) {
            return;
        }
    }
    if (hm->state == VI_STATE_RESERVED_SEAT) {
        vii->remove_human(human_index);
        return;
    }
    vanim_set_state_directional_blend_pose(hp->char_inst->vp, ZERO_VECTOR, 0.0f);
    human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
    human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
    human_override_default_animation_state(hp, true);
    human_set_state(hp, HS_VEHICLE_EXITING, HMS_IDLE, -1.0f);
    human_set_stance(hp, HST_STAND);
    human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    human_update_collision_filter(hp, true);
    if (hm->seat == VSI_DRIVER && vp->m_flags.m_hijacked == 0) {
        // the engine keeps running under a driver who bails out at speed
        bool keep_engine = false;
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
            if (object_is_player(hp)) {
                keep_engine = !automobile_get_handbrake_on((automobile*)vp) && 28.0f < (float)automobile_get_mph((automobile*)vp);
            }
            else {
                vi_update_exit_nodes(vii);
                keep_engine = vii->exit_mode == 9;
            }
        }
        if (!keep_engine) {
            vp->engine_stop(object_is_player(hp), false);
        }
    }
    if (vp->m_flags.m_engine_running != 0 && !switching_seat) {
        vehicle_handle_exit_audio(vp, false, object_is_player(hp), false);
    }
    hm->interaction = VI_INTERACTION_EXIT;
    if (vp->get_obj_type() == OT_VEHICLE && vp->get_sub_type() == OT_SUB_VEHICLE_WALKER && vp->rdd.is_loaded() && vanim_is_playing_action(vp->vp, 0x3DC)) {
        vanim_pause_and_ramp_out_action(vp->vp, 0x3DC, 8);
    }
    human_combat_abandon_turret(hp);
    if (object_is_local_player(hp) && camera_get_mode() == CAMERA_TURRET_MODE) {
        vi_utilities::switch_to_vehicle_camera(hp, vp);
    }
    if (hp->rdd.is_loaded()) {
        hp->rdd.get_data()->controller->update_collision_shape_mode(hp);
    }
    vi_set_logic_state(vii, human_index, VI_STATE_EXIT_PATHFIND);
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER) {
        ((player*)hp)->request_all_followers_replan();
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            if (vii->humans[i].handle != 0xFFFFFFFF) {
                remaining--;
                npc* np = World.object_from_handle<npc>(vii->humans[i].handle);
                if (np != NULL) {
                    np->get_ai()->planner.request_replan();
                }
            }
        }
        item* torso = human_get_prop(hp, PROP_TYPE_TORSO, NULL);
        if (torso != NULL) {
            animlib_clear_looping_animations(torso->char_inst->aim_handle, true);
        }
        if (vp->orient.uvec.y < -0.5f) {
            // on its roof, from a seat that needs climbing out of: straight to a ragdoll
            const vehicle_interaction_waypoint& wp = vp->info->vi_info->waypoints[hm->animate_from_wpi];
            if (wp.last_choice != 0 && wp.seat_index != VSI_INVALID) {
                vehicle_interaction_ragdoll(hp, 0xFFFFFFFF);
            }
        }
    }
    vehicle* apc = vii->vvm.vp;
    if (!vehicle_is_apc(apc)) {
        return;
    }
    // Passengers leave an APC armed: their own rifle-class weapon, or an M16 replacing their whole inventory.
    const int first_passenger_seat = apc->m_num_turret_mounts > 0 && apc->m_turret_mounts[0].info->seat != VSI_INVALID ? 1 : 0;
    if (first_passenger_seat + 1 > hm->seat) {
        return;
    }
    inventory_item* item_to_equip = hp->inventory;
    bool found = false;
    while (item_to_equip != NULL) {
        if (item_to_equip->weapon_handle != 0xFFFFFFFF) {
            object* objp = World.object_from_handle(item_to_equip->weapon_handle);
            if (objp != NULL && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON) {
                const weapon_info* w_info = ((weapon*)objp)->w_info;
                // animation groups of two handed firearms (numbers from rfg.exe's jump table)
                switch (w_info->weapon_anim_group) {
                case 1:
                case 5:
                case 6:
                case 0x10:
                case 0x11:
                case 0x12:
                case 0x14:
                case 0x18:
                case 0x19:
                    found = w_info->weapon_class != WEAPON_CLASS_THROWN_GRENADE;
                    break;
                default:
                    break;
                }
            }
        }
        if (found) {
            break;
        }
        item_to_equip = item_to_equip->next;
        if (item_to_equip == hp->inventory) {
            break;
        }
    }
    if (!found) {
        human_delete_all_items_from_inventory(hp, false, true);
        item_to_equip = human_add_item_to_inventory(hp, "m16", 1, 0x3C, -1, true, false, true);
    }
    hp->set_equipped_inv_item(item_to_equip);
    hp->desired_equipped_inv_item = item_to_equip;
}

// 0x00BB2770 (:7865)
// Carries the human with the vehicle each frame: along the transition animation, or fixed to the seat (where sudden
// accelerations of the seat throw passengers out or make them flinch).
static void vi_update_human_pos(vi_instance* vii, unsigned int human_index)
{
    vi_human_manager* hm = &vii->humans[human_index];
    human* hp = vi_human_from_handle(hm->handle);
    vehicle* vp = vii->vvm.vp;
    const vehicle_interaction_info* info = vp->info->vi_info;
    if (hp == NULL || info == NULL || hp->obj_flags.destroyed != 0 || hm->pos_correction == VI_POS_CORRECTION_NONE) {
        return;
    }
    if (hm->state == VI_STATE_ENTER_STREAMING_STALL) {
        hp->update_pos_and_orient(vp->pos, vp->orient, true);
        hp->velocity = ZERO_VECTOR;
        hm->human_motion = ZERO_VECTOR;
        return;
    }
    if (hm->state == VI_STATE_EXIT_EJECT) {
        return;
    }
    if (hm->state_of_anim == VI_STATE_SHARED_OPEN_DOOR || hm->state_of_anim == VI_STATE_SHARED_TRANSITION || hm->state_of_anim == VI_STATE_RIDE_CLOSE_DOOR) {
        process_door_moving_human(human_index, hp, vp);
    }
    if (hp->char_inst == NULL || !hp->char_inst->rdd.is_loaded() || hm->pos_correction == VI_POS_CORRECTION_HUMAN_PROJECTILE) {
        return;
    }
    int wpi = hm->animate_from_wpi;
    if (wpi == -1) {
        if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_BASH_POS || hm->pos_correction == VI_POS_CORRECTION_VEHICLE_VELOCITY) {
            hp->update_pos_and_orient(vp->pos, vp->orient, true);
            vehicle_get_velocity(vp, hp->velocity);
            hm->human_motion = hp->velocity;
        }
        return;
    }
    vector seat_pos;
    matrix seat_orient;
    info->get_waypoint_world_pos_orient(vp, wpi, seat_pos, &seat_orient, UP_VECTOR);
    MAT_CHANGE_UVEC(seat_orient, seat_orient.uvec);
    if (hm->state_of_anim != VI_STATE_RIDE_CUSTOM_ANIM && hp->rdd.is_loaded() && hp->char_inst->is_playing_action(0x400)) {
        if (hm->interaction == VI_INTERACTION_ENTER) {
            vehicle_get_velocity(vp, hm->human_motion);
        }
        else {
            hm->human_motion = hp->velocity;
        }
        if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_VELOCITY || hm->pos_correction == VI_POS_CORRECTION_VEHICLE_BASH_POS) {
            vanim_play_parameters params;
            memset(&params, 0, sizeof(params));
            vi_info_get_animation_space(params, vp, hm, NULL);
            vanim_set_action_relative_space(hp->char_inst->vp, 0x400, &params);
        }
        vanim_process_obj_movements(hp->char_inst->vp, hp->pos, hp->orient, hp->velocity);
        if (hm->pos_correction != VI_POS_CORRECTION_VEHICLE_VELOCITY && hm->pos_correction != VI_POS_CORRECTION_NONE) {
            hp->hflags.use_current_velocity = 0;
            hp->char_inst->vp->flags.use_velocity = 0;
            MAT_CHANGE_UVEC(hp->orient, seat_orient.uvec);
            const vector next_pos(hp->pos.x + hp->velocity.x * Frametime, hp->pos.y + hp->velocity.y * Frametime, hp->pos.z + hp->velocity.z * Frametime);
            hp->update_pos_and_orient(next_pos, hp->orient, true);
            hp->steering_vector.zero();
            return;
        }
        hp->hflags.use_current_velocity = 1;
        hp->hflags.use_as_final_velocity = 1;
        hp->char_inst->vp->flags.use_velocity = 1;
        vector spin = vp->last_angular_velocity;
        if (spin.mag_squared() > 0.1f) {
            // A spinning vehicle: lean with the seat the animation ends in.
            if (hm->reserve_next_node_only != 0 || info->waypoints[hm->animate_dest_wpi].node_type == 8 || info->waypoints[hm->animate_dest_wpi].node_type == 7) {
                info->get_waypoint_world_pos_orient(vp, hm->animate_dest_wpi, seat_pos, &seat_orient, UP_VECTOR);
                MAT_CHANGE_UVEC(seat_orient, seat_orient.uvec);
            }
            v_animate* animate = hp->char_inst->vp;
            if (animate->flags.interpolate_orient == 0) {
                if (seat_orient.uvec * hp->orient.uvec < 0.999f) {
                    MAT_CHANGE_UVEC(hp->orient, seat_orient.uvec);
                }
            }
            else if (animate->interpolate_dest_uvec.y != 1.0f && seat_orient.uvec * animate->interpolate_dest_uvec < 0.999f) {
                animate->interpolate_dest_uvec = seat_orient.uvec;
            }
        }
        hp->update_pos_and_orient(hp->pos, hp->orient, true);
        hp->steering_vector.zero();
        return;
    }
    if (hm->state_of_anim == VI_STATE_SHARED_TRANSITION) {
        wpi = hm->animate_dest_wpi;
        info->get_waypoint_world_pos_orient(vp, wpi, seat_pos, &seat_orient, UP_VECTOR);
        MAT_CHANGE_UVEC(seat_orient, seat_orient.uvec);
    }
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp != NULL && tp->t_info->flags.manned_vehicle_turret && hm->seat != VSI_DRIVER) {
        // turn with the gun, around a point 0.75 m in front of the seat
        const vector pivot(seat_pos.x + seat_orient.fvec.x * 0.75f, seat_pos.y + seat_orient.fvec.y * 0.75f, seat_pos.z + seat_orient.fvec.z * 0.75f);
        seat_orient.rotate_around_local_y(-tp->vehicle_heading_offset);
        seat_pos.x = pivot.x - seat_orient.fvec.x * 0.75f;
        seat_pos.y = pivot.y - seat_orient.fvec.y * 0.75f;
        seat_pos.z = pivot.z - seat_orient.fvec.z * 0.75f;
        hp->update_pos_and_orient(seat_pos, seat_orient, true);
        hm->auto_connect_to_entry = -1;
        return;
    }
    if (hp->cur_state == HS_VEHICLE_DRIVING || hp->cur_state == HS_VEHICLE_SITTING) {
        // The acceleration of the seat, half a sitting height (0.9) up from it.
        const vector body_center(hp->orient.uvec.x * 0.9f * 0.5f + seat_pos.x, seat_pos.y + hp->orient.uvec.y * 0.9f * 0.5f, seat_pos.z + hp->orient.uvec.z * 0.9f * 0.5f);
        vector point_velocity;
        havok_body_get_point_velocity(vp->havok_handle, body_center, point_velocity);
        const vector accel((point_velocity.x - hm->human_motion.x) / Frametime, (point_velocity.y - hm->human_motion.y) / Frametime, (point_velocity.z - hm->human_motion.z) / Frametime);
        hm->human_motion = point_velocity;
        if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_VELOCITY && hm->seat != VSI_DRIVER && !hp->is_invulnerable() && vp->m_flags.no_ragdoll == 0 && vp->m_flags.m_invulnerable == 0) {
            vector local_accel = accel;
            vec_change_to_local_coords(local_accel, ZERO_VECTOR, hp->orient);
            float limit = tp != NULL ? Vi_impulse_ragdoll_turret : Vi_impulse_ragdoll;
            float vert_damp = tp != NULL ? Vi_impulse_ragdoll_vert_damp_turret : Vi_impulse_ragdoll_vert_damp;
            if (human_in_players_vehicle(hp)) {
                vert_damp *= Vi_impulse_ragdoll_guerilla_bonus;
                limit *= Vi_impulse_ragdoll_guerilla_bonus;
            }
            if (local_accel.y - local_accel.y * vert_damp >= 0.0f) {
                local_accel.y *= vert_damp;
            }
            if ((double)limit * (double)limit < (double)local_accel.mag_squared()) {
                hp->play_voice_line(VOICE_LINE_FLINCH, NULL, NULL, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                set_exit_eject(vii, human_index, 0xFFFFFFFF);
                return;
            }
        }
        if (info->waypoints[wpi].last_choice == 0) {
            if (tp == NULL) {
                const float flinch = (1.0f / Vi_vehicle_lean_force + Vi_impulse_ragdoll) * 0.5f;
                if (flinch * flinch < accel.mag_squared() && vehicle_interaction_play_custom_anim(hp, ANIMATION_ACTION_FLINCH_BODY, false, false)) {
                    hp->play_voice_line(VOICE_LINE_FLINCH, NULL, NULL, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                }
            }
        }
        else {
            vector lean(-accel.x * Vi_vehicle_lean_force, -accel.y * Vi_vehicle_lean_force, -accel.z * Vi_vehicle_lean_force);
            vec_change_to_local_coords(lean, ZERO_VECTOR, vii->vvm.vp->orient);
            vanim_set_state_directional_blend_pose(hp->char_inst->vp, lean, Vi_vehicle_lean_blend_time);
        }
    }
    if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_BASH_POS || !hp->rdd.is_loaded()) {
        hp->update_pos_and_orient(seat_pos, seat_orient, true);
        havok_body_get_point_velocity(vp->havok_handle, seat_pos, hp->velocity);
        hp->hflags.use_current_velocity = 0;
        hp->char_inst->vp->flags.use_velocity = 0;
    }
    if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_VELOCITY) {
        hp->update_pos_and_orient(seat_pos, seat_orient, true);
        if (hm->interaction == VI_INTERACTION_RIDE) {
            havok_body_get_point_velocity(vp->havok_handle, seat_pos, hp->velocity);
        }
        else {
            hp->velocity = ZERO_VECTOR;
        }
        hp->hflags.use_current_velocity = 1;
        hp->hflags.use_as_final_velocity = 1;
        hp->char_inst->vp->flags.use_velocity = 1;
    }
    if (hm->freeze_hands_next_frame != 0) {
        human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, hp);
        human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, hp);
        hm->freeze_hands_next_frame = 0;
        hp->hflags.dont_lod_bones = 0;
    }
}

// 0x00B68800 (:8300)
static bool wp_is_protected_pos(const vehicle* vp, int wpi)
{
    const vehicle_interaction_waypoint& wp = vp->info->vi_info->waypoints[wpi];
    return wp.node_type <= VI_NODE_TYPE_DOOR_ENTRY && wp.eject_on_explosion == 0 && wp.last_choice == 0;
}

// 0x00B81C30 (:8320)
// Inside the vehicle, and not in the part of a transition that passes through the door.
static bool human_in_protected_vehicle_pos(const vehicle* vp, int hmi)
{
    const vi_human_manager& hm = vp->vii.humans[hmi];
    human* hp = vi_human_from_handle(hm.handle);
    if (hm.animate_dest_wpi == -1) {
        return false;
    }
    const bool dest_protected = wp_is_protected_pos(vp, hm.animate_dest_wpi);
    const bool from_protected = wp_is_protected_pos(vp, hm.animate_from_wpi);
    if (from_protected == dest_protected) {
        return dest_protected;
    }
    const float length = vanim_get_anim_length(hp->char_inst->vp, 0x400);
    if (length == 0.0f) {
        return dest_protected;
    }
    const float cur_time = vanim_get_cur_time(hp->char_inst->vp, 0x400);
    if (!from_protected && dest_protected) {
        return cur_time > length * 0.35f; // getting in: covered after the first third
    }
    return cur_time < length * 0.65f; // getting out: exposed for the last third
}

// 0x00BB7C60 (:6796)
bool vehicle_interaction_request_enter_internal(human* hp, vehicle* vp, vi_enter_data* parameters)
{
    if (vp == NULL || hp == NULL) {
        return false;
    }
    if (Teleport_into_vehicles) {
        parameters->flags.teleport = 1;
    }
    if (parameters->flags.minimal_processing == 0 && !vi_utilities::can_human_enter_vehicles(hp)) {
        return false;
    }
    if (vp->obj_flags.destroyed != 0 || hp->obj_flags.destroyed != 0) {
        return false;
    }
    vi_instance* vii = &vp->vii;
    if (vp != vii->vvm.vp || (hp->vehicle_handle != 0xFFFFFFFF && hp->vehicle_handle != vp->get_handle())) {
        return false;
    }
    int human_index = vii->find_human(hp);
    if (human_index == -1) {
        human_index = vii->add_human(hp);
        if (human_index == -1) {
            return false;
        }
    }
    else {
        const vi_human_manager& existing = vii->humans[human_index];
        const vehicle_seat_index seat = existing.enter_data.destination_seat;
        if (seat != VSI_INVALID && parameters->specified_seat == seat) {
            if (existing.interaction == VI_INTERACTION_ENTER) {
                if (parameters->flags.teleport == 0 || existing.enter_data.flags.teleport != 0) {
                    return true;
                }
                set_enter_teleport_wait(vii, human_index);
                vi_process_logic_state(vii, human_index);
                return true;
            }
            if (existing.interaction == VI_INTERACTION_RIDE) {
                return true;
            }
        }
        parameters->destination_seat = existing.enter_data.destination_seat;
    }
    GAME_TRACE("vehicle", "enter requested: human=0x%X player=%d vehicle=%s container=%s teleport=%d", hp->get_handle(), object_is_player(hp) ? 1 : 0, vp->info->name, vp->info->mesh_name, parameters->flags.teleport);
    vi_human_manager* hm = &vii->humans[human_index];
    hm->enter_data = *parameters;
    const vehicle_seat_index specified = parameters->specified_seat;
    bool has_seat = specified == hm->enter_data.destination_seat && specified != VSI_INVALID;
    const vi_interaction interaction = hm->interaction;
    if (interaction != VI_INTERACTION_NONE && parameters->seat_selection_method == VI_SEAT_SELECTION_METHOD_SPECIFIED) {
        if (specified == hm->seat && interaction == VI_INTERACTION_RIDE) {
            return true;
        }
        if (has_seat && interaction == VI_INTERACTION_ENTER) {
            if (parameters->flags.teleport == 0 && parameters->flags.minimal_processing == 0) {
                return true;
            }
            set_enter_teleport_wait(vii, human_index);
            vi_process_logic_state(vii, human_index);
            return true;
        }
    }
    if (interaction == VI_INTERACTION_EXIT && hm->state_of_anim == VI_STATE_SHARED_TRANSITION) {
        // Almost out on the last transition with a clear space: let it finish.
        const bool last_transition = hm->cur_path_idx + 2 >= VI_MAX_PATH_NODES || hm->path_nodes[hm->cur_path_idx + 2] == -1;
        const int wpi = hm->effective_curr_wpi();
        if (wpi < 0) {
            return false;
        }
        if (last_transition && vp->info->vi_info->waypoints[wpi].node_type > VI_NODE_TYPE_ENTRY && !vi_human_pf_check_raycast(vii, wpi, hm->animate_from_wpi)) {
            return false;
        }
    }
    if (!has_seat && !vi_seat_reserve(vii, human_index)) {
        const vehicle_seat_index seat = hm->enter_data.destination_seat;
        if (seat == VSI_INVALID) {
            vi_request_exit(vii, human_index, false);
            return false;
        }
        hm->enter_data.specified_seat = seat;
        if (hm->interaction == VI_INTERACTION_ENTER || hm->interaction == VI_INTERACTION_RIDE) {
            return false;
        }
    }
    if (hm->interaction == VI_INTERACTION_RIDE) {
        if (hm->state_of_anim == VI_STATE_RIDE_CUSTOM_ANIM) {
            return false;
        }
        hm->exit_data.initialize();
        vi_request_exit(vii, human_index, true);
        if (hp->ragdoll_idx != -1) {
            return false;
        }
    }
    if (hm->interaction == VI_INTERACTION_NONE) {
        hm->interaction = VI_INTERACTION_RESERVE;
        hm->state = VI_STATE_RESERVED_SEAT;
    }
    if (parameters->flags.reserve_seat_only != 0) {
        return true;
    }
    hm->interaction = VI_INTERACTION_ENTER;
    hp->vehicle_handle = vp->get_handle();
    if (parameters->flags.minimal_processing == 0) {
        // enemies on their way into this vehicle give up
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            const vi_human_manager& other = vii->humans[i];
            if (other.handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            if (other.state == VI_STATE_RESERVED_SEAT || other.state == VI_STATE_ENTER_PATHFIND || other.state_of_anim == VI_STATE_ENTER_PATH_TO_DOOR) {
                human* other_hp = World.object_from_handle<human>(other.handle);
                if (other_hp != NULL && human_teams_are_unfriendly(other_hp, hp)) {
                    vi_request_exit(vii, i, false);
                }
            }
        }
    }
    if (object_is_local_player(hp)) {
        player* pp = World.object_cast<player>(hp);
        player_enters_vehicle_event event;
        event.handle = vii->vvm.vp->get_handle();
        gem_post_event(event);
        pp->direct_control_dir.zero();
        if (pp->cur_state == HS_COVER) {
            player_exit_cover(pp, false);
        }
        player_set_zoom_state((player*)hp, PZS_OFF, false);
        camera_trans_start_linear_transition(0.7f, 7, true);
    }
    if (human_weapons_is_reloading(hp, false)) {
        human_weapons_cancel_reload(hp);
    }
    if (hp->get_equipped_weapon() != NULL) {
        human_weapons_firearm_release_trigger(hp);
    }
    if (!vp->rdd.is_loaded() || !hp->rdd.is_loaded()) {
        hm->enter_data.flags.minimal_processing = 1;
        set_enter_streaming_stall(vii, human_index);
        return true;
    }
    if (parameters->flags.minimal_processing == 0) {
        set_enter_pathfind(vii, human_index);
        player* pp = World.object_cast<player>(hp);
        if (pp != NULL) {
            pp->request_all_followers_replan();
            objective_highlight_ground_marker_remove(vp->get_handle());
        }
        return true;
    }
    if (vii->humans[human_index].interaction != VI_INTERACTION_ENTER) {
        return false;
    }
    set_enter_teleport_wait(vii, human_index);
    vi_process_logic_state(vii, human_index);
    return true;
}

// 0x00B81990 (:7074)
bool vehicle_interaction_passengers_stuck(const vehicle* vp)
{
    if (vp == NULL) {
        return false;
    }
    bool stuck = false;
    int remaining = vp->vii.num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        const vi_human_manager& hm = vp->vii.humans[i];
        if (hm.handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        if (hm.seat != VSI_DRIVER) {
            const human* hp = World.object_from_handle<human>(hm.handle);
            if (hp != NULL) {
                if (hp->cur_state == HS_VEHICLE_EXITING) {
                    return false;
                }
                if (hp->hflags.stuck_in_vehicle != 0) {
                    stuck = true;
                }
            }
        }
    }
    return stuck;
}

// 0x00BB81F0 (:7100)
bool vehicle_interaction_request_enter(human* hp, vehicle* vp, vi_enter_data* parameters)
{
    return vehicle_interaction_request_enter_internal(hp, vp, parameters);
}

// The vehicle a human is in, NULL when gone or not a vehicle (rfg.exe inlines this everywhere).
static vehicle* vi_vehicle_of_human(const human* hp)
{
    if (hp->vehicle_handle == 0xFFFFFFFF) {
        return NULL;
    }
    object* objp = World.object_from_handle(hp->vehicle_handle);
    return objp != NULL && objp->get_obj_type() == OT_VEHICLE ? (vehicle*)objp : NULL;
}

// 0x00BB6060 (:7307)
void vehicle_interaction_request_exit(human* hp, vi_exit_data* parameters)
{
    if (hp == NULL || hp->hflags.disallow_vehicle_exit != 0) {
        return;
    }
    hp->hflags.stuck_in_vehicle = 0;
    if (Teleport_into_vehicles && (player*)hp == player_get_local_player() && parameters != NULL) {
        parameters->flags.teleport = 1;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        object* reserved = hp->reserved_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(hp->reserved_vehicle_handle) : NULL;
        vehicle_interaction_cancel_reservation(hp, reserved != NULL && reserved->get_obj_type() == OT_VEHICLE ? (vehicle*)reserved : NULL);
        return;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    if (parameters != NULL) {
        if (hm->interaction == VI_INTERACTION_EXIT && hm->exit_data.flags.maintain_position == parameters->flags.maintain_position && hm->exit_data.flags.teleport == parameters->flags.teleport) {
            return;
        }
        parameters->flags.force_dive_exit |= hm->exit_data.flags.force_dive_exit;
        hm->exit_data = *parameters;
    }
    else {
        const unsigned char force_dive_exit = hm->exit_data.flags.force_dive_exit;
        hm->exit_data.initialize();
        hm->exit_data.flags.force_dive_exit = force_dive_exit;
    }
    vi_request_exit(&vp->vii, human_index, false);
}

// 0x00B909D0 (:7359)
bool vehicle_interaction_play_custom_anim(human* hp, animation_action action, bool override_anim, bool break_ik)
{
    if (hp == NULL || hp->vehicle_handle == 0xFFFFFFFF || !hp->rdd.is_loaded()) {
        return false;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return false;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    const vehicle_interaction_info* info = vp->info->vi_info;
    if (hm->interaction != VI_INTERACTION_RIDE || (!override_anim && hm->state_of_anim != VI_STATE_RIDE_DEFAULT)) {
        return false;
    }
    const int anim_handle = vanim_find_anim_handle((int)(info->animation_set - Anim_sets), 0, action);
    if (anim_handle == -1) {
        return false;
    }
    hm->state_of_anim = VI_STATE_RIDE_CUSTOM_ANIM;
    if (break_ik) {
        human_freeze_ik(hp, ANIMATION_IK_RIGHT_HAND, NULL);
        human_freeze_ik(hp, ANIMATION_IK_LEFT_HAND, NULL);
    }
    hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
    hp->char_inst->vp->hp = hp;
    vanim_play_custom_action(hp->char_inst->vp, index_alias(anim_handle, ANIM_ALIAS_VEHICLE_TRANSITION), 0x40, 1.0f, 1.0f);
    vector seat_pos;
    info->get_waypoint_world_pos_orient(vp, hm->path_nodes[hm->cur_path_idx], seat_pos, NULL, UP_VECTOR);
    const signed char wpi = hm->path_nodes[hm->cur_path_idx];
    hm->animate_from_wpi = wpi;
    hm->animate_dest_wpi = wpi;
    vanim_play_parameters params;
    memset(&params, 0, sizeof(params));
    vi_info_get_animation_space(params, vp, hm, NULL);
    params.start_err_corr_length = 0.01f;
    params.end_err_corr_begin_time = 0.01f;
    vanim_set_action_relative_space(hp->char_inst->vp, 0x400, &params);
    return true;
}

// 0x00BB4990 (:7442)
void vehicle_interaction_cancel_reservation(human* hp, vehicle* vp)
{
    if (hp == NULL || vp == NULL) {
        return;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    if (hm->interaction == VI_INTERACTION_EXIT) {
        return;
    }
    hm->exit_data.initialize();
    vi_request_exit(&vp->vii, human_index, false);
    if (vi_human_from_handle(vp->reserved_by) == hp) {
        vp->reserved_by = 0xFFFFFFFF;
    }
}

// 0x00BB2670 (:7475)
bool vehicle_interaction_ragdoll(human* hp, unsigned int perp)
{
    if (hp == NULL) {
        return false;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1 || vp->vii.humans[human_index].state == VI_STATE_EXIT_EJECT) {
        return false;
    }
    if (!hp->is_killed() && !vp->is_killed() && vp->m_flags.no_ragdoll != 0) {
        return true;
    }
    set_exit_eject(&vp->vii, human_index, perp);
    return true;
}

// 0x00BB4A30 (:7515)
bool vehicle_interaction_cancel(human* hp)
{
    if (hp == NULL) {
        return false;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return false;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    if (hm->state == VI_STATE_EXIT_EJECT) {
        return true;
    }
    if (hm->interaction != VI_INTERACTION_ENTER) {
        if (hm->interaction != VI_INTERACTION_EXIT) {
            return true;
        }
        if (hm->being_hijacked == 0) {
            // on the way out: straight back to the seat
            vi_enter_data parameters;
            parameters.specified_seat = hm->enter_data.specified_seat;
            parameters.flags.teleport = 1;
            parameters.destination_seat = VSI_INVALID;
            parameters.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
            vehicle_interaction_request_enter_internal(hp, vp->vii.vvm.vp, &parameters);
            return true;
        }
    }
    hm->exit_data.flags.teleport = 1;
    vi_request_exit(&vp->vii, human_index, false);
    return true;
}

// 0x00BB4B00 (:7587)
void vehicle_interaction_bail_all(vehicle* vp)
{
    if (vp == NULL) {
        return;
    }
    vi_instance* vii = &vp->vii;
    if (vp->m_flags.m_scripted_bail != 0) {
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            const unsigned int handle = vii->humans[i].handle;
            if (handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            object* objp = World.object_from_handle(handle);
            if (objp == NULL || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || ((npc*)objp)->npc_flags.tied_up == 0) {
                // Original bug kept: a handle that is no longer a human writes through NULL.
                human* hp = vi_human_from_handle(vii->humans[i].handle);
                hp->hflags.disallow_vehicle_exit = 0;
                hp->hflags.limited_vehicle_exit = 0;
                vi_request_exit(vii, i, false);
            }
        }
    }
    int remaining = vii->num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        const unsigned int handle = vii->humans[i].handle;
        if (handle == 0xFFFFFFFF) {
            continue;
        }
        remaining--;
        if (object_is_player(handle)) {
            vi_request_exit(vii, i, false);
        }
        else {
            vi_request_npc_exit(vii, i, -1);
        }
    }
}

// 0x00BB2720 (:7624)
void vehicle_interaction_remove(human* hp)
{
    if (hp == NULL) {
        return;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index >= 0) {
        vp->vii.remove_human(human_index);
    }
}

// 0x00BBF660 (:7650)
void vehicle_interaction_process()
{
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int v = 0; v < num_vehicles; v++) {
        vehicle* vp = World.get_object<vehicle>(v);
        vi_instance* vii = &vp->vii;
        vii->updated_exits = 0;
        if (vp->obj_flags.destroyed != 0) {
            continue;
        }
        if (vp->rdd.is_loaded()) {
            // doors nobody needs any more
            for (int d = 0; d < VI_MAX_DOORS; d++) {
                vi_door& door = vii->vvm.doors[d];
                const int since = door.open_close_ts.time_since();
                bool expired = since > 4000;
                if (!expired && door.open_close_ts.time_since() > 2500 && door.wpi_check >= 0) {
                    const waypoint_usage& usage = vii->vvm.used_waypoints[door.wpi_check];
                    expired = !usage.is_path_reserved() && usage.reg_on_release == 0;
                }
                if (expired) {
                    if (door.state == VI_DOOR_STATE_CLOSED_CLIMBING) {
                        door.initialize();
                    }
                    else if (door.state == VI_DOOR_STATE_OPEN) {
                        vi_door_close_action(vii, d, 0.33f);
                    }
                }
            }
        }
        const float up_y = vp->orient.uvec.y;
        const bool flipped = up_y < -0.70710677f;
        const bool tilted = flipped || up_y < 0.25881904f;
        int remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            vi_human_manager* hm = &vii->humans[i];
            if (hm->handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            human* hp = vi_human_from_handle(hm->handle);
            if (hp == NULL || hp->obj_flags.destroyed != 0 || ((hp->char_inst == NULL || !hp->rdd.is_loaded()) && hm->state != VI_STATE_ENTER_STREAMING_STALL)) {
                vii->remove_human(i);
            }
            else if (!vp->rdd.is_loaded() && hm->state != VI_STATE_ENTER_STREAMING_STALL && hm->interaction != VI_INTERACTION_RIDE) {
                hm->exit_data.flags.teleport = 1;
                vi_request_exit(vii, i, false);
            }
            else {
                if (hm->pos_correction == VI_POS_CORRECTION_VEHICLE_VELOCITY && hm->interaction == VI_INTERACTION_RIDE && vp->m_flags.no_ragdoll == 0) {
                    // Exposed seats throw their passengers off a vehicle on its side or roof.
                    if (flipped && vp->info->vi_info->waypoints[hm->path_nodes[hm->cur_path_idx]].eject_on_explosion != 0) {
                        set_exit_eject(vii, i, 0xFFFFFFFF);
                    }
                    if (tilted && hm->seat != VSI_DRIVER) {
                        set_exit_eject(vii, i, 0xFFFFFFFF);
                    }
                }
                if (hm->being_hijacked != 0 && hp->hflags.disallow_vehicle_exit != 0 && hm->exit_data.flags.force_dive_exit != 0 && hp->avoidance_check_timer.elapsed()) {
                    hp->hflags.disallow_vehicle_exit = 0;
                    hp->hflags.limited_vehicle_exit = 0;
                    vi_request_exit(vii, i, false);
                }
            }
        }
        vi_refresh_raycast(vii);
        vi_register_paths(vii);
        remaining = vii->num_humans;
        for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
            if (vii->humans[i].handle == 0xFFFFFFFF) {
                continue;
            }
            remaining--;
            do {
                vii->process_state_again = 0;
                vi_process_logic_state(vii, i);
                if (vii->process_state_again != 0) {
                    continue;
                }
                vi_process_anim_state(vii, i);
            } while (vii->process_state_again != 0);
        }
        vii->updated_exits = 0;
    }
}

// 0x00B81A10 (:7797)
// True while the vehicle moves the human. A human thrown off keeps the animation's motion on top of its fall.
bool vehicle_interaction_process_human_pre(human* hp)
{
    if (hp == NULL || hp->vehicle_handle == 0xFFFFFFFF || !hp->rdd.is_loaded()) {
        return false;
    }
    vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return false;
    }
    vi_human_manager* hm = &vp->vii.humans[human_index];
    if (hm->pos_correction == VI_POS_CORRECTION_NONE) {
        return false;
    }
    if (hm->pos_correction == VI_POS_CORRECTION_HUMAN_PROJECTILE) {
        hp->actual_velocity_mag = hp->actual_velocity.mag();
        human_move_fall_update_speed(hp);
        vector root_rotation, root_velocity, movement;
        animlib_get_position_change(root_rotation, root_velocity, movement, hp->char_inst->aim_handle);
        const vector anim_velocity(movement.x / Frametime, movement.y / Frametime, movement.z / Frametime);
        vector delta(anim_velocity.x - hm->human_motion.x, anim_velocity.y - hm->human_motion.y, anim_velocity.z - hm->human_motion.z);
        delta.unrotate(hp->char_inst->vp->starting_orient);
        hp->velocity.x += delta.x;
        hp->velocity.y += delta.y;
        hp->velocity.z += delta.z;
        hp->char_inst->vp->local_anim_movement = movement;
        hm->human_motion = anim_velocity;
        hp->hflags.use_current_velocity = 1;
        hp->hflags.use_as_final_velocity = 1;
    }
    return true;
}

// 0x00BB34D0 (:8192)
bool vehicle_interaction_teleport_human_positions(vehicle* vp)
{
    if (vp->info->vi_info == NULL || vp->obj_flags.destroyed != 0) {
        return false;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        vi_human_manager* hm = &vp->vii.humans[i];
        if (hm->handle != 0xFFFFFFFF) {
            remaining--;
            const vi_correction_type saved = hm->pos_correction;
            hm->pos_correction = VI_POS_CORRECTION_VEHICLE_BASH_POS;
            vi_update_human_pos(&vp->vii, i);
            hm->pos_correction = saved;
        }
    }
    return true;
}

// 0x00BB3560 (:8221)
bool vehicle_interaction_update_human_positions(vehicle* vp)
{
    if (vp->info->vi_info == NULL || vp->obj_flags.destroyed != 0) {
        return false;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; i < VI_MAX_HUMANS && remaining > 0; i++) {
        if (vp->vii.humans[i].handle != 0xFFFFFFFF) {
            remaining--;
            vi_update_human_pos(&vp->vii, i);
        }
    }
    return true;
}

// 0x00B81DB0 (:8354)
bool human_in_protected_vehicle_pos(const human* hp)
{
    const vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    return human_index != -1 && human_in_protected_vehicle_pos(vp, human_index);
}

// 0x00B81E20 (:8375)
bool human_can_quick_exit_vehicle(const human* hp)
{
    const vehicle* vp = vi_vehicle_of_human(hp);
    if (vp == NULL) {
        return false;
    }
    const int human_index = vp->vii.find_human(hp);
    if (human_index == -1) {
        return false;
    }
    const vi_human_manager& hm = vp->vii.humans[human_index];
    if (hm.enter_data.flags.teleport_to_door != 0) {
        return false;
    }
    const int anim_wpi = hm.effective_anim_wpi();
    if (anim_wpi == -1) {
        return true;
    }
    return vp->info->vi_info->waypoints[anim_wpi].node_type > VI_NODE_TYPE_DOOR_ENTRY;
}

// 0x00B90BC0 (:8425)
// rfg.exe inlines get_waypoint_world_pos_orient here, with the world up vector.
bool vehicle_interaction_get_seat_pos_orient(const vehicle* vp, vehicle_seat_index seat_idx, vector& pos, matrix* orient)
{
    pos = vp->pos;
    if (orient != NULL) {
        *orient = vp->orient;
    }
    const vehicle_interaction_info* info = vp->info->vi_info;
    for (int i = 0; i < info->num_waypoints; i++) {
        if (info->waypoints[i].seat_index == seat_idx) {
            info->get_waypoint_world_pos_orient(vp, i, pos, orient, UP_VECTOR);
            return true;
        }
    }
    return false;
}

// 0x00BB8200 (:8449)
bool vehicle_interaction_ragdoll_fall_in_seat(human* hp, vehicle* vp)
{
    vector pos;
    hp->get_center_pos(pos);
    havok_ragdoll_get_pos(hp->ragdoll_idx, pos, RAGDOLL_BODY);
    if (vp == NULL) {
        const unsigned int handle = vehicle_get_closest_vehicle(pos, 0.1f, NULL, true);
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : NULL;
        if (objp == NULL || objp->get_obj_type() != OT_VEHICLE) {
            return false;
        }
        vp = (vehicle*)objp;
    }
    if (vp->obj_flags.destroyed != 0) {
        return false;
    }
    vehicle_seat_index seat;
    vp->info->vi_info->get_closest_seat_for_ragdoll(seat, vp, pos, 0.3f);
    if (seat == VSI_INVALID) {
        return false;
    }
    if (hp->ragdoll_idx < 0) {
        if (!wp_is_protected_pos(vp, vi_utilities::waypoint_index_from_seat_index(vp, seat))) {
            return false;
        }
    }
    else {
        human_ragdoll_stop(hp, false);
    }
    vi_enter_data parameters;
    parameters.destination_seat = VSI_INVALID;
    parameters.flags.teleport = 1;
    parameters.flags.minimal_processing = 1;
    parameters.specified_seat = seat;
    parameters.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    if (!vehicle_interaction_request_enter_internal(hp, vp, &parameters)) {
        return false;
    }
    if (!object_is_player(hp)) {
        World.object_cast<npc>(hp)->get_ai()->planner.request_replan();
        return true;
    }
    if (vp->m_flags.disable_for_player == 0 && vp->m_flags.m_corpse == 0) {
        // the seats the player may stay in: the driver's, and the first gun's
        const int last_seat = vp->m_num_turret_mounts >= 1 ? 1 : 0;
        if (seat <= last_seat) {
            return true;
        }
    }
    vehicle_interaction_request_exit(hp, NULL);
    return true;
}

// 0x00B68830 (:8518)
void vehicle_interaction_highlight_entry_points(vehicle* vp, float radius)
{
    // Original bug kept: starts one past the last waypoint.
    for (int i = vp->info->vi_info->num_waypoints; i >= 0; i--) {
        const vehicle_interaction_waypoint& wp = vp->info->vi_info->waypoints[i];
        if (wp.node_type == VI_NODE_TYPE_ENTRY && wp.restrict_vertical_access == 0) {
            objective_highlight_ground_marker_add(vp->get_handle(), radius, wp.local_pos, false, false);
        }
    }
}
