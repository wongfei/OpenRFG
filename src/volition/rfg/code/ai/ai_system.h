// Original: volition/rfg/code/ai/ai_system.h
// Implementation: volition/rfg/code/ai/ai_system.cpp
//
// ai_system, the AI of one npc (npc::ai, from ai_system_create): perception, planner, persona, scripted behaviours,
// pedestrian and ambient action state, reactions, cover and vehicle seat preferences. Per frame npc_process_all calls
// ai_system::do_frame_pre and do_frame for each npc. ai_system_do_frame (decompiled) runs once per gameplay frame
// before them: it resets the per-frame rate limiters (building collection, noticing humans, can-shoot updates,
// sidesteps, cover search), advances the queued pathfind and replan limiters, the search objects, defense
// coordination and formations, then ai_perception::process_movers / process_thrown_objects. mission_system_do_frame
// calls ai_system_mission_do_frame. Placement of ai_pedestrian_info and ai_reaction_info inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/ambient_action_manager.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_cover_node_enums.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct ai_belief;
struct ambient_road_info;
class gfm_orders;
class human;
struct human_melee_combat_move_data;
class navpoint;
class npc;
class object;
class object_convoy;
class object_guard_node;
class object_patrol;
class object_squad;
class patrol_node_handle;
class player;
class projectile;
class vehicle;
struct weapon_info;

// size 0x74
class ai_pedestrian_info
{
public:
    ai_pedestrian_info(); // 0x0064CA70 (.h:118)
    void reset(); // 0x0060A770 (:738)
    const navpoint* get_next_navpoint() const; // 0x0060A860 (:777)
    const navpoint* get_last_navpoint() const; // 0x0060A890 (:788)
    bool near_ped_nodes(const vector& pos) const; // 0x0060A7C0 (:752)
    void remove_waypoint(unsigned int last_waypoint_handle); // 0x0064E070 (:800)
    bool is_old_waypoint(unsigned int nav_handle) const; // 0x0064E0F0 (:819)
    void add_new_waypoints(const farray<unsigned int,8>& new_waypoints); // 0x00673E40 (:825)
    ~ai_pedestrian_info(); // 0x005F9C70

    timestamp animation_cooldown; // 0x00
    timestamp pause_timer; // 0x04
    bool paused; // 0x08
    timestamp npc_greeting_timeout; // 0x0C
    unsigned int greeting_npc_handle; // 0x10
private:
    farray<unsigned int,8> ambient_waypoints; // 0x14
    farray<unsigned int,8> old_waypoints; // 0x44
};

// size 0x10
struct ai_reaction_info
{
    void reset(); // 0x005DE3F0 (:907)
    ai_reaction_info(); // 0x005F9C90

    unsigned int target_handle; // 0x00
    ai_belief_types belief_type; // 0x04
    timestamp time_left; // 0x08
    unsigned int reason; // 0x0C
};

// size 0x24 (rfg.pdb)
struct ai_tension_level_info
{
    int threshold; // 0x00
    int min_react_time; // 0x04
    int max_react_time; // 0x08
    int explosive_reaction_probs[NUM_AI_PROJECTILE_REACTIONS]; // 0x0C
    int projectile_reaction_probs[NUM_AI_PROJECTILE_REACTIONS]; // 0x18
};

extern const ai_tension_level_info Ai_tension_level_info_table[NUM_AI_TENSION_LEVELS]; // 0x012C8330

// size 0x6A8
class ai_system
{
    // human_combat.cpp reads master_handle and the planner (friends are not recorded in rfg.pdb).
    friend bool human_combat_can_play_large_flinch_or_stumble(const human* hp);
    friend class ai_perception; // reads squad_handle and the rest of its owner directly (inferred)
public:
    enum can_play_stun_results
    {
        CAN_PLAY_STUN_NO = 0,
        CAN_PLAY_STUN_YES = 1,
        CAN_PLAY_STUN_RAGDOLL_ONLY = 2,
        NUM_CAN_PLAY_STUN_RESULTS = 3,
    };

    // size 0x8
    class ai_covering_fire_info
    {
    public:
        void set(const npc* ally); // 0x005DE2E0 (:404)
        npc* get() const; // 0x0060A530 (:411)
        void clear(); // 0x005DE2C0 (:398)
        ai_covering_fire_info(); // 0x005F9CA0

    private:
        unsigned int ally_handle; // 0x00
        timestamp valid_time; // 0x04
    };

    void init(npc* np_); // 0x00673E50 (:993)
    void reset(); // 0x00673EC0 (:1007)
    void reset_common(); // 0x0064E120 (:1017)
    void reset_volatile_data(); // 0x005DE670 (:1190)
    void free(); // 0x00A05F70 (.h:281)
    void register_destream(); // 0x0060A8D0 (:1210)
    bool is_despawning() const; // 0x005DE6C0 (:1243)
    bool can_despawn(bool* offscreen) const; // 0x0060A570 (:632)
    bool do_despawn(bool force_despawn); // 0x0060A8E0 (:1222)
    void do_frame_pre(); // 0x006B53A0 (:1258)
    void do_frame(); // 0x006C3680 (:1298)
    void register_hit(const human* attacker, float damage_taken, const weapon_info* w_info); // 0x00A05F80 (.h:303)
    void maybe_switch_to_wounded_animation(flinch_directions flinch_dir, object_hit_location hit_location); // 0x0060A950 (:1431)
    bool is_idle() const; // 0x005DE700 (:1550)
    bool is_stunned() const; // 0x005DC960 (.h:310)
    bool is_pathing_to(const vector& destination, float xz_tolerance_sq, float y_tolerance) const; // 0x005DE750 (:1569)
    bool get_path_dest(vector& path_dest) const; // 0x005DE710 (:1557)
    bool is_done_pathfinding(float time) const; // 0x005DE7E0 (:1588)
    bool has_hostile_contact() const; // 0x005DE8C0 (:1622)
    bool is_engaging_player() const; // 0x0060AC80 (:1634)
    player* is_in_player_squad(bool include_defense_orders) const; // 0x0064E330 (:1650)
    bool is_fleeing() const; // 0x005DE8D0 (:1668)
    bool usable_squad_vehicle(const vehicle* vp) const; // 0x0064E390 (:1719)
    bool should_stay_in_fetal_position() const; // 0x005DE930 (:1805)
    bool should_go_to_fetal_position() const; // 0x005DE970 (:1817)
    vehicle* passenger_of_squadmate() const; // 0x0064E570 (:1832)
    bool trigger_custom_behavior(ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id, bool replan); // 0x0060B2A0 (:2233)
    void end_custom_behavior(); // 0x005DEA40 (:2247)
    bool has_custom_behavior() const; // 0x005DC970 (.h:360)
    ai_morale_levels get_district_morale_level() const; // 0x005DEB80 (:3172)
    ai_control_levels get_district_control_level() const; // 0x005DEC00 (:3189)
    ai_pathfind_results pathfind_to(human* target, human_movement_states m_state, human_stances stance, ai_pathfind_priorities priority, const vector* eff_pos); // 0x006B5450 (:3280)
    ai_pathfind_results pathfind_to(const vector& pos, human_movement_states m_state, human_stances stance, ai_pathfind_priorities priority, uint64_t end_nav_cell_hint, bool disable_smoothing); // 0x0068F040 (:3336)
    bool can_pathfind() const; // 0x005DC980 (.h:395)
    void set_pathfind_delay(int msec); // 0x005DC990 (.h:396)
    void set_pathfind_delay_rand(int min_msec, int max_msec); // 0x005DC9A0 (.h:397)
    ai_pathfind_results set_direct_movement(const vector& vec, human_movement_states m_state, human_stances stance, float move_speed); // 0x0064F020 (:3459)
    ai_pathfind_results set_direct_movement_fast(const vector& dir, human_movement_states m_state, human_stances stance, float move_speed); // 0x0060B880 (:3464)
    ai_pathfind_results set_direct_movement_to_pos(const vector& target_pos, uint64_t target_navcell, human_movement_states m_state, human_stances stance, bool disable_smoothing); // 0x0060BD30 (:3613)
    void stop_moving(); // 0x005DECA0 (:3620)
    anim_set_variants get_base_anim_set() const; // 0x005DEE40 (:3711)
    anim_set_variants get_temp_anim_set() const; // 0x005DEE70 (:3718)
    bool set_base_anim_set(anim_set_variants new_variant, bool civilian_only, bool force); // 0x005DEEA0 (:3734)
    bool set_temp_anim_set(anim_set_variants new_variant); // 0x005DEF40 (:3777)
    void update_on_fire(); // 0x0064F060 (:3789)
    void exit_on_fire(); // 0x0060BDC0 (:3800)
    bool play_explosion_flinch(float impulse_mag, const vector& impulse_dir); // 0x0060BE40 (:3822)
    bool play_explosion_knockdown(float impulse_mag, const vector& impulse_dir, const vector& angular_impulse, bool force_ragdoll); // 0x0060BEE0 (:3851)
    bool register_electrocuted(int elec_ms); // 0x0060BD60 (:3662)
    void start_stumble(animation_action stumble_anim); // 0x0060C0E0 (:3899)
    void register_shove(const human* attacker); // 0x006BBBC0 (:3925)
    void register_hit_by_car(const vehicle* vp, const human* driver); // 0x005DEF90 (:3941)
    void notify_vehicle_disabled(); // 0x0060C140 (:3948)
    void start_slave_state(const human* master); // 0x0060C170 (:3959)
    void end_slave_state(const human* master); // 0x0060C1D0 (:3966)
    bool is_slave() const; // 0x005DC9B0 (.h:447)
    int get_stunned_animation() const; // 0x00A05FB0 (.h:450)
    void set_desired_burst_size(int num_shots); // 0x005DC9C0 (.h:456)
    int get_desired_burst_size() const; // 0x005DC9E0 (.h:457)
    void fire_weapon(); // 0x005DEFC0 (:3988)
    void stop_firing(); // 0x005DEFE0 (:3998)
    void fire_secondary_weapon(); // 0x005DEFD0 (:3993)
    bool should_react(unsigned int reason) const; // 0x005DF0C0 (:4063)
    bool request_reaction(const ai_belief* belief, unsigned int reason, int time); // 0x005DF1B0 (:4115)
    bool request_reaction(unsigned int handle, unsigned int reason, int time); // 0x005DF150 (:4101)
    bool request_reaction(const object* obj, unsigned int reason, int time); // 0x005DF0F0 (:4087)
    bool request_reaction_bomb(const projectile* proj, unsigned int reason, int time); // 0x005DF210 (:4133)
    void end_reaction(unsigned int reason); // 0x005DF270 (:4152)
    void assign_squad(unsigned int new_squad_handle); // 0x005DE9D0 (:1866)
    void clear_squad(); // 0x005DE9F0 (:1875)
    bool leave_squad(); // 0x0064E610 (:1883)
    unsigned int get_squad_handle() const; // 0x005DC9F0 (.h:485)
    object_squad* get_squad() const; // 0x0060B370 (:2486)
    bool is_in_my_squad(const npc* npp) const; // 0x0060B430 (:2523)
    bool is_in_my_squad(unsigned int np_handle) const; // 0x0060B3D0 (:2510)
    bool collect_squadmates(base_array<npc*>& squadmates, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data) const; // 0x0064E9C0 (:2537)
    void do_death_func(); // 0x0068EEE0 (:587)
    bool request_covering_fire(npc* ally); // 0x00674420 (:2998)
    void end_covering_fire_request(); // 0x0060B730 (:3073)
    bool can_provide_covering_fire(const npc* ally) const; // 0x0064EDD0 (:3116)
    bool can_receive_covering_fire() const; // 0x0060B7F0 (:3092)
    npc* get_ally_covering_me() const; // 0x0064CB00 (.h:508)
    npc* is_providing_covering_fire() const; // 0x0064CB40 (.h:509)
    void register_team_change(human_teams old_team, human_teams new_team); // 0x0060AD40 (:1928)
    void assigned_new_orders(); // 0x0060AD70 (:1937)
    gfm_orders* get_orders() const; // 0x0060B3A0 (:2495)
    gfm_orders_types get_orders_type() const; // 0x0064E6C0 (:1949)
    bool has_orders_of_type(gfm_orders_types type) const; // 0x00673A20 (.h:525)
    const object* get_orders_object() const; // 0x0064E700 (:1960)
    bool near_orders_location(float buffer) const; // 0x0064E740 (:1972)
    bool leave_formation(); // 0x006BB7F0 (:2316)
    bool leave_vehicle_formation(); // 0x006BB850 (:2332)
    unsigned int get_formation_handle() const; // 0x005DCA00 (.h:541)
    unsigned int get_vehicle_formation_handle() const; // 0x005DCA10 (.h:542)
    unsigned int follow_in_formation(const object* target, ai_formation_types type); // 0x006BB8D0 (:2375)
    unsigned int vehicle_follow_in_formation(vehicle* vp, const object* target, ai_formation_types type); // 0x006BB960 (:2393)
    unsigned int vehicle_follow_convoy_start(); // 0x006BBA20 (:2417)
    bool vehicle_follow_convoy_stop(); // 0x006BBB20 (:2462)
    bool is_in_convoy_formation() const; // 0x006746F0 (:4343)
    bool is_following_leader(const npc* leader_np) const; // 0x0064E920 (:2355)
    bool vehicle_is_following_leader(const object* leader) const; // 0x0064E970 (:2365)
    const object_patrol* get_patrol_object() const; // 0x00674580 (:4294)
    const object_patrol* get_cached_patrol_object() const; // 0x0060C4F0 (:4358)
    object_convoy* get_convoy_object() const; // 0x00674670 (:4326)
    unsigned int get_convoy_handle() const; // 0x00674600 (:4309)
    const patrol_node_handle* get_patrol_node() const; // 0x0064F100 (:4365)
    bool has_patrol_type_orders() const; // 0x0064F170 (:4380)
    void update_patrol_node(const patrol_node_handle* new_node); // 0x0064F1C0 (:4388)
    void update_patrol_speed(vehicle* vp, const patrol_node_handle* new_node, const object_patrol* patrol); // 0x0060C520 (:4407)
    void update_road_usage(vehicle* vp, const patrol_node_handle* new_node); // 0x0060C5A0 (:4420)
    bool group_time_elapsed() const;
    int get_time_at_guard_node() const; // 0x0060AE50 (:2085)
    void reset_time_at_guard_node(); // 0x005DCA20 (.h:587)
    int get_time_at_guard_facing() const; // 0x0060AEB0 (:2094)
    int get_time_in_guard_node_group() const; // 0x0060AF10 (:2105)
    void reset_time_in_guard_node_group(); // 0x005DCA30 (.h:594)
    bool is_in_guard_group(int group_id) const; // 0x0060AF70 (:2122)
    object* get_defend_object() const; // 0x0060C240 (:4188)
    bool outside_defensive_perimeter(const vector& pos, bool force_eval, const float* buffer_dist) const; // 0x0060C290 (:4217)
    bool has_valid_guard_post() const; // 0x00673EF0 (:2028)
    object_guard_node* get_guard_node() const; // 0x0060ADB0 (:2007)
    bool is_at_guard_post() const; // 0x0068EFA0 (:2071)
    bool reserve_guard_pos(const vector& new_guard_pos); // 0x005DEA00 (:2197)
    bool reserve_guard_node(object_guard_node* gnp, signed char dir_index); // 0x0060AFD0 (:2132)
    void release_guard_pos(); // 0x0060B220 (:2219)
    signed char get_guard_direction_index() const; // 0x0060AE00 (:2017)
    bool get_guard_pos(vector& gp) const; // 0x00674360 (:2209)
    ai_tension_levels get_tension_level() const; // 0x0060B2E0 (:2261)
    void increase_tension_level(int amount); // 0x0060B330 (:2280)
    ai_projectile_reactions pick_projectile_reaction(ai_tension_levels tension_level, bool is_explosive) const; // 0x005DEA50 (:2296)
    unsigned int get_target_handle() const;
    human* get_human_target() const; // 0x0060C200 (:4172)
    void yield_next_frame(); // 0x005DF5D0 (:4563)
    bool should_yield_frame(); // 0x005DCA40 (.h:648)
    void clear_yield_next_frame(); // 0x005DCA50 (.h:649)
    bool should_cycle_flee_behavior() const; // 0x005DEB50 (:2768)
    void maybe_cycle_flee_behavior(); // 0x0060B640 (:2938)
    bool road_route_valid() const; // 0x0060B570 (:2900)
    bool get_current_road_destination(ambient_road_info& destination_info) const; // 0x0060B4A0 (:2789)
    void remove_current_road_destination(); // 0x0064ED50 (:2863)
    void remove_road_destination_index(unsigned int last_road_visited, unsigned short last_road_index); // 0x0064EC50 (:2831)
    const char* get_action_node_type() const; // 0x0060B450 (:2576)
    bool allowed_to_use_action_node() const; // 0x006743A0 (:2597)
    navpoint* get_safe_building_nav(bool friendly_only) const; // 0x0064EBE0 (:2739)
    bool is_done_reinforcing() const; // 0x0064EAE0 (:2700)
    bool can_teleport_into_vehicle(const vehicle* vp, ai_vehicle_seat_types seat_type) const; // 0x005DEAA0 (:2655)
    void stun(animation_action anim_action, vanim_aliases anim_alias, float anim_speed, unsigned int anim_flags); // 0x0060AA80 (:1470)
private:
    void create_persona(); // 0x005DE430 (:1096)
    void do_fire_frame(); // 0x005DEFF0 (:4009)
    void enter_stunned_state(animation_state anim_state, int stun_time, vanim_aliases anim_alias); // 0x0060AB70 (:1511)
    void update_stunned_state(); // 0x0064DE90 (:509)
    animation_action get_explosion_knockdown_animation(float impulse_mag, const vector& impulse_dir, float* rot_angle) const; // 0x005DF4A0 (:4506)
    animation_action get_explosion_flinch_animation(float impulse_mag, const vector& impulse_dir) const; // 0x005DF2B0 (:4447)
    can_play_stun_results can_play_stun_animation() const; // 0x005DE300 (:425)
    void end_slave_state_internal(); // 0x005DEFA0 (:3971)
    void despawn_me(bool fade); // 0x0060A6F0 (:714)
    bool point_is_dangerous_to_path_to(const vector& pos) const; // 0x0064EF00 (:3222)
    ai_pathfind_results set_direct_movement_to_pos_internal(const vector& target_pos, uint64_t target_navcell, unsigned int target_navpoint, human_movement_states m_state, human_stances stance, bool disable_smoothing); // 0x0060BA10 (:3526)
public:
    ai_system(); // 0x00672F70

    ai_scripting_behaviors custom_behavior; // 0x00
    unsigned int custom_behavior_target; // 0x04
    int custom_behavior_value; // 0x08
    int custom_action_id; // 0x0C
    ai_projectile_reactions projectile_reaction; // 0x10
    timestamp tension_timer; // 0x14
    ai_perception perception; // 0x18
    ai_planner planner; // 0x300
    ai_pathfind_results pf_failure_reason; // 0x3B8
    vector desired_goal_pos; // 0x3BC
    uint64_t desired_goal_nav_cell_hint_handle; // 0x3C8
    animation_action desired_anim_action; // 0x3D0
    voice_lines desired_voice_line; // 0x3D4
    vector desired_evade_dir; // 0x3D8
    vector desired_destination; // 0x3E4
    matrix desired_orient; // 0x3F0
    unsigned int desired_destination_object; // 0x414
    unsigned int desired_leave_building_transition; // 0x418
    unsigned int desired_enter_building_transition; // 0x41C
    unsigned int desired_search_handle; // 0x420
    unsigned int desired_search_navpoint; // 0x424
    unsigned int desired_follow_target; // 0x428
    unsigned int desired_vehicle; // 0x42C
    ai_vehicle_seat_types desired_vehicle_seat_type; // 0x430
    float desired_vehicle_speed; // 0x434
    float desired_vehicle_dist_to_goal_pos; // 0x438
    cover_node_stances desired_cover_stance; // 0x43C
    cover_node_firing_positions desired_cover_firing_position; // 0x440
    const human_melee_combat_move_data* desired_melee_move; // 0x444
    unsigned int desired_defense_assignment_handle; // 0x448
    ai_oppression_actions oppression_action; // 0x44C
    ai_system_flags flags; // 0x450
    unsigned int desired_target; // 0x454
    vector desired_target_pos; // 0x458
    timestamp desired_target_recalc_timestamp; // 0x464
    unsigned int desired_weapon; // 0x468
    ai_persona_flags persona_flags; // 0x46C
    timestamp persona_cycle_flee_behavior_time; // 0x470
    timestamp action_timer; // 0x474
    timestamp animation_timer; // 0x478
    timestamp stop_following_timer; // 0x47C
    timestamp evaluate_cover_timer; // 0x480
    timestamp in_cover_timer; // 0x484
    timestamp out_of_cover_timer; // 0x488
    timestamp not_using_cover_timer; // 0x48C
    timestamp stance_change_timer; // 0x490
    timestamp bomb_timer; // 0x494
    int action_state; // 0x498
    int action_substate; // 0x49C
    vector action_pos; // 0x4A0
    unsigned int action_obj_handle; // 0x4AC
    timestamp guard_node_watch_period; // 0x4B0
    ai_pedestrian_info pedestrian_info; // 0x4B4
    ai_ambient_action_info amb_action_info; // 0x528
    timestamp_pair gawk_timer; // 0x570
    farray<road_segment_info,8> road_route; // 0x578
    farray<road_segment_info,8> last_road_route; // 0x5C8
    unsigned int flee_destination_handle; // 0x618
    vector2 flee_destination_offset; // 0x61C
    timestamp exhaustion_timer; // 0x624
    ai_reaction_info reaction_info; // 0x628
    timestamp salute_timer; // 0x638
    timestamp electrocution_timer; // 0x63C
    unsigned int reserved_corpse_handle; // 0x640
    vector last_reinforcement_pos; // 0x644
private:
    npc* np; // 0x650
    timestamp pathfind_timer; // 0x654
    unsigned int squad_handle; // 0x658
    unsigned int formation_handle; // 0x65C
    unsigned int vehicle_formation_handle; // 0x660
    ai_covering_fire_info covering_fire_provide; // 0x664
    ai_covering_fire_info covering_fire_receive; // 0x66C
    vector guard_pos; // 0x674
    unsigned int guard_node_handle; // 0x680
    signed char guard_direction_index; // 0x684
    timestamp time_at_guard_node; // 0x688
    timestamp time_at_guard_facing; // 0x68C
    timestamp time_in_guard_node_group; // 0x690
    int desired_burst_size; // 0x694
    int stunned_animation; // 0x698
    timestamp stunned_timer; // 0x69C
    unsigned int master_handle; // 0x6A0
};

void ai_system_init(); // 0x0060A8C0 (:920)
void ai_system_level_init_pre(); // 0x006BB710 (:926)
void ai_system_mission_do_frame(); // 0x005DE420 (:942)
void ai_system_do_frame(); // 0x006BB780 (:966)
bool ai_system_consider_player_famous(); // 0x005DEC80 (:3207)
ai_system* ai_system_create(); // 0x0064F260 (:4574)
void ai_system_release(ai_system* ai); // 0x00674710 (:4579)
void ai_system_set_alert_level_green(); // 0x005DF600 (:4589)
void ai_system_process_alert_level_change(); // 0x0064F270 (:4594)
void ai_system_process_team_disposition_change(human_teams team); // 0x0064F350 (:4632)

// 0x005DC9F0 (.h:485)
inline unsigned int ai_system::get_squad_handle() const
{
    return squad_handle;
}

// 0x005DCA00 (.h:541)
inline unsigned int ai_system::get_formation_handle() const
{
    return formation_handle;
}

// No address: rfg.exe inlines it (ai + 0x1D4, attention.target of the perception at 0x18).
inline unsigned int ai_system::get_target_handle() const
{
    return perception.attention.target;
}
