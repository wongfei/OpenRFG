// Original: volition/rfg/code/ai/percept.h
// Implementation: volition/rfg/code/ai/percept.cpp
//
// ai_perception, what an npc knows: beliefs about other objects, attention and awareness, look-at and steering modes,
// target selection, threat and team disposition, and the per-frame processing of movers and thrown objects
// (process_movers / process_thrown_objects, called from ai_system_do_frame). Placement of the attention enums and the
// mover and thrown object path infos inferred.
#pragma once

#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct ai_belief;
struct ai_belief_requirements;
struct ai_belief_specification;
class ai_system;
struct explosion_info;
struct hazard_physics_info;
class human;
struct human_melee_combat_move_data;
struct nav_cell;
class navpoint;
class npc;
class object;
class object_mover;
class player;
class projectile;
class rfg_mover;
class turret;
class vehicle;
struct view_frustum_parameters;
class weapon;
class ai_combat_action_parameters;
struct weapon_fire_info;
struct weapon_info;

enum attention_awareness_levels
{
    ATTENTION_AWARENESS_USE_CURRENT_MODE = -1,
    ATTENTION_AWARENESS_NORMAL = 0,
    ATTENTION_AWARENESS_FOCUSED = 1,
    ATTENTION_AWARENESS_OBLIVIOUS = 2,
    NUM_ATTENTION_AWARENESS_LEVELS = 3,
};

enum attention_look_at_modes
{
    ATTENTION_USE_CURRENT_MODE = -1,
    ATTENTION_LOOK_AT_NONE = 0,
    ATTENTION_LOOK_AT_BELIEF = 1,
    ATTENTION_LOOK_AT_OBJECT = 2,
    ATTENTION_AIM_STRAIGHT_AHEAD = 3,
    ATTENTION_AIM_AT_BELIEF = 4,
    ATTENTION_AIM_AT_OBJECT = 5,
    ATTENTION_IDLE = 6,
    ATTENTION_SEARCH = 7,
    ATTENTION_SEARCH_COVER_MY_SIX = 8,
    ATTENTION_ACQUIRE_VISUAL = 9,
    NUM_ATTENTION_LOOK_AT_MODES = 10,
};

enum attention_steering_modes
{
    ATTENTION_STEERING_USE_CURRENT_MODE = -1,
    ATTENTION_STEERING_USE_DEFAULT = 0,
    ATTENTION_STEERING_TURN_TO_TARGET = 1,
    ATTENTION_STEERING_PLANT_FEET = 2,
    NUM_ATTENTION_STEERING_MODES = 3,
};

enum ai_belief_update_modes
{
    AI_BELIEF_UPDATE_OVERWRITE = 0,
    AI_BELIEF_UPDATE_IF_NEWER = 1,
    AI_BELIEF_UPDATE_IF_MORE_RELEVANT = 2,
    NUM_AI_BELIEF_UPDATE_MODES = 3,
};

enum ai_engage_target_results
{
    AI_ENGAGE_ALLOWED = 0,
    AI_ENGAGE_NOT_ALLOWED_TOO_MANY = 1,
    AI_ENGAGE_NOT_ALLOWED_ALERT_LEVEL = 2,
    AI_ENGAGE_NOT_ALLOWED_DISTANCE = 3,
    NUM_AI_ENGAGE_RESULTS = 4,
};

// size 0x48
struct mover_path_info
{
    mover_path_info(); // 0x005EF2C0

    const object_mover* omp; // 0x00
    vector center_of_mass; // 0x04
    vector velocity; // 0x10
    float radius; // 0x1C
    bool is_large; // 0x20
    vector trajectory_points[3]; // 0x24
};

// size 0x3C
struct thrown_object_path_info
{
    thrown_object_path_info(); // 0x005EF2D0

    const weapon* wp; // 0x00
    vector velocity; // 0x04
    float radius; // 0x10
    unsigned int culprit; // 0x14
    vector trajectory_points[3]; // 0x18
};

// size 0x2E8
class ai_plan_conditions;
ai_plan_conditions ai_plan_conditions_create_working_set(const npc* np);

class ai_perception
{
    // weapon::bullet_autoaim reads attention.mode (friends are not recorded in rfg.pdb)
    friend class weapon;
    friend bool attack_get_sidestep_adjustment(vector& move_dir, const npc* np, const weapon* wp); // reads last_decent_shot_time (inferred)
    friend human_stances get_attack_stance(const ai_combat_action_parameters& params); // reads the line of fire times (inferred)
    friend class npc; // is_in_combat reads the hostile beliefs (inferred)
    friend class activity_convoy_data; // update_npc_beliefs reads a belief bin (inlined)
    friend class ai_system; // get_target_handle reads attention.target (inferred)
    template<typename T> friend class belief_obj_iterator; // (inferred)
    friend class alert_shout_unfriendly; // inform_npc updates the listener's beliefs (inferred)
    friend class alert_shout_corpse; // inform_npc updates the listener's beliefs (inferred)
    friend class ai_planner; // stun resets the attention (inferred)
    friend class advance_goal; // store_setup_data resets the attention (inferred)
    friend class evade_vehicle_goal; // commit_setup_data notes the driver (inferred)
    friend class path_to_melee_range_action; // do_action notes an armed player (inferred)
    friend class move_to_melee_range_action; // do_action notes an armed player (inferred)
    friend class find_disturbance_action; // uses the low alert frustum whatever the level (inferred)
    friend void threaten_note_enemy(ai_perception& perception, const human* enemy, bool react); // threaten_actions.cpp (inferred)
    friend ai_plan_conditions ai_plan_conditions_create_working_set(const npc* np); // reads the belief bins (inferred)
    friend class activity_raid_data; // check_npc_beliefs reads the belief bins (inlined)
    friend void do_straggler_tracking(npc* np); // activity_raid.cpp reads the belief bins (inlined)
    friend void maybe_set_alert_level_green(); // activity_raid.cpp reads the belief bins (inlined)
    friend bool activity_raid_npc_should_prepare(const npc* np); // activity_raid.cpp reads the belief bins (inlined)

public:
    enum analyze_shot_modes
    {
        ANALYZE_SHOT_ACTUAL_BULLET = 0,
        ANALYZE_SHOT_EXACT_LOCATION = 1,
        ANALYZE_SHOT_LAST_LOCATION = 2,
        NUM_ANALYZE_SHOT_MODES = 3,
    };

    enum shot_results
    {
        SHOT_RESULT_HIT_TARGET = 0,
        SHOT_RESULT_GOOD_SHOT = 1,
        SHOT_RESULT_NEAR_MISS = 2,
        SHOT_RESULT_NEUTRAL = 3,
        SHOT_RESULT_BAD_SHOT = 4,
        NUM_SHOT_RESULTS = 5,
    };

    enum attention_directions
    {
        ATTENTION_DIRECTION_LEFT = 0,
        ATTENTION_DIRECTION_RIGHT = 1,
        ATTENTION_DIRECTION_TARGET = 2,
        ATTENTION_DIRECTION_CENTER = 3,
        NUM_ATTENTION_DIRECTIONS = 4,
    };

    // size 0x8
    struct temporary_projectile_record
    {
        const projectile* proj; // 0x00
        unsigned int flags; // 0x04
    };

    // size 0x30
    struct temporary_eye_pos_orient
    {
        temporary_eye_pos_orient(const temporary_eye_pos_orient& __that); // 0x005EE300
        temporary_eye_pos_orient(); // 0x005EE2F0

        vector eff_eye_pos; // 0x00
        matrix eff_eye_orient; // 0x0C
    };

    // size 0x1C
    struct target_tracking_point
    {
        target_tracking_point(); // 0x005EE120

        vector pos; // 0x00
        vector vel; // 0x0C
        timestamp time; // 0x18
    };

    // size 0x124
    struct target_tracking_info
    {
        void clear(); // 0x0063DAF0 (:2573)
        target_tracking_info(); // 0x0064CA20 (.h:582)
        ~target_tracking_info(); // 0x0063E3B0

        unsigned int last_target; // 0x00
        circ_array<target_tracking_point,10> history; // 0x04
    };

    // size 0xC
    struct player_friendly_fire_info
    {
        void reset(const npc* np); // 0x005ECF90 (:1056)
        bool register_fire(float amount, const human* victim, const human* attacker); // 0x005ECFF0 (:1084)
        void do_frame(); // 0x005ED0D0 (:1138)
        bool is_hostile() const; // 0x005ED110 (:1150)
        player_friendly_fire_info(); // 0x005EEC40

        bool enabled; // 0x00
        float damage_total; // 0x04
        timestamp valid_time; // 0x08
    };

    // size 0x60
    struct attention_info
    {
        // size 0x1
        struct attention_info_flags
        {
            bool check_right : 1; // 0x00:0
            bool right_side_clear : 1; // 0x00:1
            bool left_side_clear : 1; // 0x00:2
            bool aiming_at_target : 1; // 0x00:3
        };

        void clear(); // 0x005EEC50 (:7185)
        attention_info(); // 0x0063E3C0
        ~attention_info(); // 0x0063E3F0

        attention_look_at_modes mode; // 0x00
        attention_steering_modes steering; // 0x04
        attention_awareness_levels awareness; // 0x08
        attention_info_flags flags; // 0x0C
        unsigned int target; // 0x10
        ai_belief_types belief_type; // 0x14
        circ_array<unsigned int,2> bored_idle_targets; // 0x18
        unsigned int idle_target; // 0x28
        ai_belief_types idle_belief_type; // 0x2C
        timestamp bored_timer; // 0x30
        timestamp update_target_timer; // 0x34
        timestamp current_target_timer; // 0x38
        float idle_not_ready_dot; // 0x3C
        float idle_ready_dot; // 0x40
        float search_spread_angle; // 0x44
        timestamp left_side_timer; // 0x48
        timestamp right_side_timer; // 0x4C
        timestamp center_timer; // 0x50
        attention_directions direction; // 0x54
        int aim_update_time; // 0x58
        timestamp last_decent_shot_time; // 0x5C
    };

    // size 0x8
    struct ai_belief_bin
    {
        int count; // 0x00
        ai_belief* beliefs; // 0x04
    };

    ai_perception(); // 0x006670B0 (:7119)
    void clear(); // 0x0063E400 (:7137)
    void init(npc* np_); // 0x006671A0 (:7130)
    void do_frame(); // 0x006C2FA0 (:9588)
    void do_attention_frame(); // 0x006A7610 (:10876)
    static void process_movers(); // 0x00668720 (:9850)
    static void process_thrown_objects(); // 0x006B9B80 (:9966)
    void explosion_observed(const human* attacker, const vector& pos, const explosion_info* e_info); // 0x006B8AE0 (:7374)
    void pain_sound_heard(const human* target, const human* attacker, const weapon_info* w_info); // 0x006B8E10 (:7489)
    void misc_sound_heard(const human* source, const vector& pos); // 0x006671C0 (:7636)
    void weapon_sound_heard(const human* attacker, const weapon_info* w_info); // 0x006BE230 (:7665)
    void bullet_sound_heard(const human* attacker, const weapon_info* w_info, const vector& pos); // 0x006BE6A0 (:7777)
    void building_stress_sound_heard(const rfg_mover* building, float stress_amount); // 0x00667540 (:8067)
    void building_damage_sound_heard(const rfg_mover* building, const human* attacker, const vector& damage_pos); // 0x006B9490 (:8108)
    void building_collapse_heard(const rfg_mover* building); // 0x0063EAD0 (:8133)
    void vehicle_damage_heard(const vehicle* target, const human* attacker, const vector& damage_pos, bool vehicle_collision); // 0x006BEB20 (:8185)
    void footstep_sound_heard(const human* target); // 0x006B9580 (:8262)
    void register_bumped_into(const human* target); // 0x006B95D0 (:8282)
    void register_vehicle_is_on_fire(const vehicle* vp); // 0x0063EC60 (:8323)
    void register_hit(const human* attacker, float damage_taken, const weapon_info* w_info); // 0x006B9270 (:7864)
    void threw_grenade(const projectile* proj); // 0x005EEDF0 (:7950)
    void dropped_grenade(const projectile* proj); // 0x00667260 (:7961)
    void register_taunt(const human* taunter); // 0x006672A0 (:7972)
    void register_projectile_attached(const projectile* proj); // 0x00667380 (:7987)
    void register_bomb_set(const projectile* proj); // 0x006673E0 (:8003)
    void register_charge_placed(const projectile* proj); // 0x00667460 (:8015)
    void register_team_change(human_teams old_team, human_teams new_team); // 0x005EEE10 (:8058)
    void alert_hazard(const npc* other_npc, const object* hazard); // 0x00667660 (:8341)
    void alert_enemy(const npc* other_npc, const human* enemy); // 0x006B9680 (:8382)
    void alert_collapsing_building(const npc* other_npc, const rfg_mover* building); // 0x00667730 (:8360)
    void alert_unfriendly_target(const human* target); // 0x00667800 (:8414)
    void alert_corpse(const human* corpse, const human* enemy); // 0x006BD770 (:3706)
    void alert_threatened(const human* threatener); // 0x00665EF0 (:3716)
    void pf_dest_obstructed(); // 0x0063EC80 (:8572)
    ai_belief* record_los_result(human* hp, bool result); // 0x006B9B30 (:8731)
    void search_successful(const human*);
    void grant_belief(const ai_belief* belief); // 0x006B97A0 (:8429)
    bool force_search(const human* enemy, const vector& pos); // 0x006B98E0 (:8502)
    bool force_enemy_knowledge(const human* enemy); // 0x006A73B0 (:8516)
    void go_hostile(const human* enemy, bool react); // 0x006BB6F0 (.h:198)
    bool start_beatdown(const human* target); // 0x00667880 (:8532)
    bool cancel_beatdown(const human* target); // 0x00667910 (:8562)
    void order_attack_enemy(const human*);
    void flag_unsafe_building(const object* mover); // 0x0063E0B0 (:3375)
    bool is_in_closest_building(const human* hp, rfg_mover** building_ptr, float building_tolerance, float terrain_tolerance) const; // 0x00667BF0 (:8938)
    bool is_in_closest_building(rfg_mover** building_ptr, float building_tolerance, float terrain_tolerance) const; // 0x00667CE0 (:8955)
    bool is_on_navmesh(const human* hp, nav_cell** nav_cell_ptr, float terrain_tolerance) const; // 0x005EEE80 (:8765)
    bool is_on_navmesh(nav_cell** nav_cell_ptr, float terrain_tolerance) const; // 0x005EEF40 (:8800)
    bool get_nearest_navpoint_or_navcell(nav_cell*& ncp, navpoint*& npp, vector* navmesh_pos_out, float* xz_dist_squared) const; // 0x00667950 (:8820)
    bool has_clear_line_of_travel(const human* hp, float max_dist, float y_tolerance) const; // 0x00667D10 (:9107)
    bool has_clear_line_of_travel(const vector& end_pos, navpoint* start_npp, navpoint* end_npp) const; // 0x005EEF60 (:8969)
    bool building_is_safe(const rfg_mover* building, bool friendly_only) const; // 0x0063ECB0 (:9337)
    rfg_mover* get_nearest_safe_building(bool friendly_only) const; // 0x0063ED60 (:9361)
    rfg_mover* get_nearest_unsafe_building() const; // 0x0063EDE0 (:9376)
    float get_unsafe_building_dist(const rfg_mover* building) const; // 0x005EF2A0 (:9388)
    float get_distance_squared_to_nearest_building(const vector& pos, rfg_mover** building_ptr) const; // 0x00667010 (:7076)
    float get_distance_squared_to_nearest_building(const human* hp, rfg_mover** building_ptr) const; // 0x00667080 (:7104)
    float get_distance_squared_to_nearest_building(rfg_mover** building_ptr) const; // 0x00667090 (:7108)
private:
    bool within_view_frustum(const vector& target_pos, const view_frustum_parameters& params) const; // 0x005EDDF0 (:2445)
    bool within_view_frustum(const object* target, bool do_raycast) const; // 0x006A5710 (:2486)
public:
    bool within_view_frustum(const vector& target_pos) const; // 0x0063EF40 (:9470)
    bool has_los_to_position(const vector& pos, bool ignore_combat_target) const; // 0x006836E0 (:9485)
    bool enemy_in_visual_range(const human* target) const; // 0x00668640 (:9412)
    float get_min_flee_dist(const human* target) const; // 0x0063EE60 (:9448)
    float get_max_notice_dist(const human* target) const; // 0x0063EEB0 (:9454)
    bool enemy_in_los(unsigned int enemy_handle) const; // 0x0063EFC0 (:9518)
    bool unfriendly_in_los(unsigned int target_handle) const; // 0x0063F000 (:9539)
    bool can_hit_target() const; // 0x005EE390 (:4407)
    bool can_hit_close_to_target() const; // 0x005EE410 (:4437)
    bool recent_shot_was_bad() const; // 0x005EE460 (:4461)
    int get_time_since_last_decent_shot() const; // 0x005DC840 (.h:268)
    bool aiming_at(const object* target, const weapon* wp, bool check_range) const; // 0x00680EE0 (:4477)
    bool firing_los_clear(const object* target, human_stances stance, const vector* start_pos) const; // 0x006810E0 (:4565)
    bool get_effective_target_pos(vector& pos, const object* target, bool* use_pos_belief, float* radius_sq, bool force_head_pos) const; // 0x00666790 (:4794)
    bool is_in_melee_move_range(const object* target, const human_melee_combat_move_data* move_data, const vector* eff_pos) const; // 0x005EE4B0 (:4951)
    const human_melee_combat_move_data* get_melee_move_in_range(const object* target, const weapon_info* w_info, bool allow_outside) const; // 0x00681570 (:5013)
    bool is_in_melee_range(const object* target, const weapon_info* w_info) const; // 0x0068E980 (.h:281)
    void acknowledge_enemy(const human* enemy); // 0x0063F0D0 (:10466)
    bool on_alert() const; // 0x005EF340 (:10484)
    bool has_enemy_belief() const; // 0x005FB630 (.h:291)
    bool has_enemy_contact() const; // 0x005FB640 (.h:295)
    bool any_enemy_in_los() const;
    bool any_enemy_player_in_los() const; // 0x006838E0 (:10498)
    bool has_player_enemy_belief() const; // 0x00683920 (:10514)
    ai_engage_target_results can_engage_target_reason(const human* target) const; // 0x00668C90 (:10535)
    bool can_engage_target(const human* target) const; // 0x00673A00 (.h:308)
    bool is_in_ally_line_of_fire() const; // 0x005DC860 (.h:311)
    bool ally_is_in_line_of_fire() const; // 0x005DC870 (.h:312)
    void deprecate_all_enemy_beliefs(unsigned int enemy_handle, ai_belief* enemy_exists_belief); // 0x00669090 (:10751)
    void deprecate_all_enemy_beliefs(); // 0x005EF380 (:10728)
    static bool vehicle_is_heavy(const vehicle* vp); // 0x005EF470 (:10775)
    static bool human_is_in_heavy_vehicle(const human* hp); // 0x0063F160 (:10785)
    bool enemy_is_being_attacked_from_turret(const human* hp) const; // 0x00683960 (:10797)
    bool is_in_friendly_player_line_of_fire() const; // 0x005EF490 (:10845)
    bool target_tracking_get_aggregate_velocity(vector& velocity, const human* target, float seconds) const; // 0x00665610 (:2728)
    bool target_tracking_get_current_velocity(vector& velocity, const human* target, bool cheat_if_out_of_sight) const; // 0x006653D0 (:2677)
    bool target_tracking_extrapolate(vector& pos, vector& velocity, const human* target, float lookahead, bool check_obstructions) const; // 0x00680710 (:2793)
    bool in_combat() const; // 0x005FB650 (.h:344)
    void pay_attention(const ai_belief* belief, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level); // 0x00683B60 (:10955)
    void pay_attention(unsigned int target_handle, ai_belief_types belief_type, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level); // 0x006691D0 (:10960)
    void pay_attention(const object* obj, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level); // 0x00683B30 (:10949)
    void pay_attention(unsigned int target_handle, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level); // 0x00683B10 (:10944)
    void stop_paying_attention(bool reset_spine); // 0x005EF5E0 (:11002)
    void maybe_look_away_from_corpse(); // 0x006692E0 (:11018)
    bool set_attention_mode(attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level); // 0x0063F1E0 (:11073)
    attention_look_at_modes get_attention_mode() const; // 0x005DC880 (.h:363)
    attention_steering_modes get_attention_steering_mode() const;
    ai_belief* get_attention_belief() const; // 0x0063F1A0 (:10920)
    ai_belief_types get_attention_belief_type() const;
    unsigned int get_attention_handle() const; // 0x005DC890 (.h:369)
    object* get_attention_target() const; // 0x005EF5C0 (:10929)
    int num_beliefs(ai_belief_types type, const ai_belief_requirements& requirements) const; // 0x005EF6E0 (:11334)
    int num_beliefs(ai_belief_types type) const; // 0x005DC8A0 (.h:389)
    int num_relevant_beliefs(ai_belief_types type, float cutoff, const ai_belief_requirements& requirements) const; // 0x005EF730 (:11353)
    ai_belief* get_first_belief(ai_belief_types type) const; // 0x005DC8B0 (.h:397)
    ai_belief* get_next_belief(const ai_belief* belief) const; // 0x005EF7A0 (:11382)
    ai_belief* get_belief(ai_belief_types type, const vector& pos, const ai_belief_requirements& requirements, object_type obj_type) const; // 0x005EF890 (:11441)
    ai_belief* get_belief(ai_belief_types type, const ai_belief_requirements& requirements, object_type obj_type) const; // 0x005EF830 (:11426)
    ai_belief* get_belief(ai_belief_types type, const object* obj) const; // 0x005EF800 (:11418)
    ai_belief* get_belief(ai_belief_types type, unsigned int handle) const; // 0x005EF7D0 (:11408)
    ai_belief* get_belief(ai_belief_types type) const; // 0x005DC8C0 (.h:406)
    ai_belief* get_most_recent_belief(ai_belief_types type, const ai_belief_requirements& requirements, object_type obj_type) const; // 0x005EF950 (:11471)
    ai_belief* get_recent_belief(ai_belief_types type, int cutoff_msec, const ai_belief_requirements& requirements, object_type obj_type) const; // 0x005EF9E0 (:11489)
    bool deprecate_belief(ai_belief_types type, const object* obj); // 0x0064C9D0 (.h:420)
    bool deprecate_belief(ai_belief_types type, unsigned int handle); // 0x0063F6A0 (:11318)
    void deprecate_belief(ai_belief* belief); // 0x005EF620 (:11293)
    void deprecate_all_beliefs_about_object(unsigned int obj); // 0x00669060 (:10718)
    ai_belief* find_matching_belief(const ai_belief_specification* specifications, int num_specifications, unsigned int obj_override) const; // 0x0063F6E0 (:11527)
    int collect_all_matching_beliefs(ai_belief** beliefs, int max_beliefs, const ai_belief_specification* specifications, int num_specifications, unsigned int obj_override) const; // 0x005EFA60 (:11546)
private:
    void notice_humans(); // 0x006C2940 (:3114)
    void notice_corpses(); // 0x006C2DC0 (:3264)
    void notice_objects(); // 0x006B8100 (:3338)
    void notice_projectiles(); // 0x006C2E30 (:3308)
    bool notice_buildings(); // 0x00680B60 (:3416)
    void get_temporary_eye_pos_orient(temporary_eye_pos_orient& eye_pos_orient) const; // 0x005EE150 (:3091)
    bool notice_ally(human* hp); // 0x00665BE0 (:3558)
    bool notice_friendly(human* hp); // 0x00665D70 (:3617)
    bool notice_unfriendly(human* hp, const temporary_eye_pos_orient eye_pos_orient); // 0x006B81A0 (:3649)
    bool notice_enemy(human* hp, const temporary_eye_pos_orient eye_pos_orient); // 0x006BD910 (:3821)
    bool notice_corpse(human* hp); // 0x006BD7B0 (:3741)
    bool notice_projectile(projectile* proj, const temporary_projectile_record* old_projectiles, int num_old_projectiles); // 0x006BDA40 (:4111)
    bool notice_turret(turret* tp); // 0x00665FD0 (:3857)
    bool notice_vehicle(vehicle* ve); // 0x006A5850 (:3903)
    bool notice_building(const rfg_mover* building); // 0x006660E0 (:4010)
    void evaluate_corpses(); // 0x00683820 (:9553)
    void update_corpse_belief(const human* corpse); // 0x00665E60 (:3687)
    unsigned int get_human_belief_flags(const human* hp) const; // 0x0063D920 (:2194)
    float get_human_belief_relevance(const human* hp) const; // 0x005EDDB0 (:2183)
    ai_belief* add_hazard(const object* obj, float relevance); // 0x00665000 (:2225)
    ai_belief* add_hazard(const object* obj, const hazard_physics_info& haz_physics); // 0x006806E0 (:2221)
    void get_view_frustum_params(view_frustum_parameters& params, const human* target) const; // 0x0063D990 (:2406)
    bool should_do_fov_check(const human* target) const; // 0x00665090 (:2247)
    bool has_los_to_target(const object* target) const; // 0x00681350 (:4655)
    bool enemy_is_aiming_at_me(const human* enemy, vector* aim_pos) const; // 0x005EE630 (:5140)
    bool enemy_wants_to_kill_me(const human* enemy) const; // 0x005EE980 (:5241)
    bool target_in_fov(const human* target, const temporary_eye_pos_orient eye_pos_orient) const; // 0x006650F0 (:2272)
    bool check_ally_against_line_of_fire(const human* ally, float dist_sq) const; // 0x005EDF80 (:2534)
    void register_in_ally_line_of_fire(); // 0x005EE100 (:2559)
    void register_ally_in_line_of_fire(); // 0x005EE110 (:2564)
    void target_tracking_update_target(const human* target, bool visible); // 0x0063DB10 (:2586)
    shot_results analyze_shot_result(const object* target_obj, const weapon_fire_info& fire_info, analyze_shot_modes mode); // 0x0063E630 (:7251)
    bool situation_allows_shooting(const object* target, const weapon* wp); // 0x0063F040 (:10120)
    void update_can_shoot_at(const object* target, weapon* wp, const weapon_fire_info* wfip); // 0x006BEDE0 (:10160)
    void record_shot_result(const object* target_obj, const vector& hit_pos, shot_results result); // 0x006BA080 (:10325)
    void update_shot_at_by_enemy(const human* attacker, const weapon_info* w_info); // 0x00668B50 (:10373)
    void maybe_react_to_player_weapon(const player* pp, const weapon_info* w_info); // 0x005EF2E0 (:10433)
    void maybe_react_to_player_jetpack(const player* pp); // 0x005EF310 (:10455)
    bool is_valid_idle_attention_target(const object* obj, const vector& idle_direction) const; // 0x005EEA20 (:5283)
    void get_new_idle_attention_target(bool& can_be_bored, const ai_belief*& belief, const object*& obj, const vector* idle_direction) const; // 0x00681C20 (:5389)
    bool update_idle_attention_target(const ai_belief*& new_belief, const object*& new_obj, const vector* idle_direction); // 0x006A5C40 (:5323)
    object* get_current_idle_attention_target() const; // 0x005EEAF0 (:5376)
    ai_belief* get_current_idle_attention_belief() const; // 0x0063E100 (:5298)
    vector get_idle_attention_direction() const; // 0x005EE9D0 (:5261)
    void do_attention_idle_frame(); // 0x006A5DC0 (:5547)
    void do_attention_search_frame(); // 0x006A61D0 (:5646)
    void do_attention_belief_frame(); // 0x00666E70 (:5983)
    void do_attention_object_frame(); // 0x00666DF0 (:5955)
    void do_attention_acquire_frame(); // 0x006A6730 (:5811)
    void enter_idle_attention(); // not in rfg.exe: the idle reset it inlines
    void enter_search_attention(attention_look_at_modes look_at_mode, float ready_dot, float spread_angle); // not in rfg.exe: the search reset it inlines
    void update_attention_steering_mode(attention_look_at_modes look_at_mode, attention_steering_modes steering_mode); // 0x005EEB10 (:6012)
    void set_look_at_object(const object* obj, bool aim_at, float aim_speed, bool reset); // 0x0063E150 (:6069)
    bool belief_should_expire(ai_belief* belief) const; // 0x00664760 (:1412)
    void clear_beliefs(ai_belief_types type, const ai_belief_requirements& requirements); // 0x005EED20 (:7223)
    void clear_beliefs(ai_belief_types type); // 0x005EECF0 (:7216)
    ai_belief* update_or_add_belief(const ai_belief* other_belief, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x00664980 (:1592)
    ai_belief* update_or_add_belief(const ai_belief* other_belief, ai_belief_update_modes mode); // 0x006648C0 (:1568)
    ai_belief* update_or_add_belief(ai_belief_types type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x00664BE0 (:1658)
    ai_belief* update_or_add_belief(ai_belief_types type, const object* obj, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x00664B10 (:1637)
    ai_belief* update_or_add_belief(ai_belief_types type, const object* obj, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x00664A40 (:1616)
    ai_belief* update_or_add_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x00664CA0 (:1670)
    ai_belief* add_new_belief(const ai_belief* other_belief, float relevance); // 0x00664DC0 (:1717)
    ai_belief* add_new_belief(const ai_belief* other_belief); // 0x00664D60 (:1695)
    ai_belief* add_new_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags); // 0x00664FA0 (:1789)
    ai_belief* add_new_belief(ai_belief_types type, const object* obj, const vector& pos, float relevance, unsigned int flags); // 0x00664E80 (:1758)
    ai_belief* add_new_belief(ai_belief_types type, const object* obj, float relevance, unsigned int flags); // 0x00664E20 (:1739)
    ai_belief* add_new_belief(ai_belief_types type, const vector& pos, float relevance, unsigned int flags); // 0x00664EE0 (:1777)
    bool update_existing_belief(ai_belief* belief, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x0063D820 (:1825)
    bool update_existing_belief(ai_belief* belief, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x0063D7F0 (:1820)
    bool update_existing_belief(ai_belief* belief, unsigned int flags); // 0x005ED8F0 (:1813)
    ai_belief* add_new_belief_internal(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags); // 0x0063D850 (:1877)
    bool update_existing_belief_internal(ai_belief* belief, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x005ED910 (:1967)
    ai_belief* update_special_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode); // 0x005ED9F0 (:2042)
    bool insert_belief_into_bin(ai_belief* belief); // 0x005ED750 (:1496)
    ai_belief* record_los_result_unfriendly(human* hp, bool result); // 0x006A73F0 (:8665)
    ai_belief* record_los_result_enemy(human* hp, bool result); // 0x006B9940 (:8581)
    ai_belief* acquire_ally_combat_target(const human* ally); // 0x006BD710 (:3528)
    ai_belief* suggest_enemy_exists(const human* enemy, const vector& guess_pos, float relevance, bool no_investigation); // 0x006B82D0 (:6164)
    ai_belief* note_possible_enemy_location(const human* enemy, float relevance, bool react); // 0x006B8520 (:6248)
    ai_belief* confirm_enemy_location(const human* enemy, bool react); // 0x006B8AC0 (:6377)
    ai_belief* go_hostile_on_unfriendly_target(ai_belief* unfriendly_belief, const human* enemy, ai_belief** enemy_exists_belief, const unsigned int* flags, bool react); // 0x006A6E60 (:6395)
    ai_belief* record_enemy_is_visible(const human* enemy); // 0x006A71A0 (:6492)
    void record_enemy_not_visible(const human* enemy); // 0x0063E340 (:6558)
    void check_combat_target(); // 0x00682AF0 (:6574)
    void update_combat_target(); // 0x00682D60 (:6655)
    bool should_switch_targets(const human* new_target) const; // 0x00683060 (:6795)
    bool consider_switching_targets(const human* new_target); // 0x00683630 (:6999)
    bool maybe_fudge_last_contact_belief(const human* new_target); // 0x00666F30 (:6963)
    bool should_ignore_player() const; // 0x005EEBD0 (:6763)
    bool treat_as_undercover(const human* target) const; // 0x006657B0 (:2903)
    human_team_dispositions get_effective_disposition(const human* target, bool check_undercover) const; // 0x00665AF0 (:2994)
    int remaining_enemy_search_time(const ai_belief* enemy_exists_belief) const; // 0x0063DEE0 (:3045)
    int remaining_enemy_search_time(unsigned int enemy_handle) const; // 0x0063DFD0 (:3079)
public:
    ~ai_perception(); // 0x0064B190

private:
    npc* np; // 0x00
    ai_system* ai; // 0x04
    timestamp_pair notice_humans_time; // 0x08
    timestamp_pair notice_friendlies_time; // 0x10
    timestamp_pair notice_corpses_time; // 0x18
    timestamp_pair notice_objects_time; // 0x20
    timestamp_pair notice_projectiles_time; // 0x28
    timestamp_pair notice_buildings_time; // 0x30
    timestamp notice_building_timeout; // 0x38
    vector notice_buildings_pos; // 0x3C
    bool was_in_closest_building; // 0x48
    timestamp belief_cleanup_time; // 0x4C
    timestamp check_muzzle_time; // 0x50
    timestamp muzzle_stuck_time; // 0x54
    timestamp in_ally_line_of_fire_time; // 0x58
    timestamp ally_in_line_of_fire_time; // 0x5C
    const object* temp_target; // 0x60
    vector temp_target_dir; // 0x64
    float temp_target_dist; // 0x70
    float temp_max_ally_fire_dist_sq; // 0x74
    bool updated_combat_target; // 0x78
    target_tracking_info target_tracking; // 0x7C
public:
    player_friendly_fire_info player_friendly_fire; // 0x1A0
private:
    attention_info attention; // 0x1AC
    ai_belief_bin belief_bins[27]; // 0x20C
    ai_belief* dead_belief_list; // 0x2E4
};

// size 0x10
struct view_frustum_parameters
{
    view_frustum_parameters(); // 0x005ECF70 (:87)

    bool initialized; // 0x00
    bool use_fov; // 0x01
    float length; // 0x04
    float h_angle; // 0x08
    float v_angle; // 0x0C
};

// size 0x44
struct melee_candidate_selection_info
{
    melee_candidate_selection_info(); // 0x006645A0 (:122)
    ~melee_candidate_selection_info(); // 0x00666DE0

    farray<human_melee_combat_move_data const*,10> candidates; // 0x00
    float least_min_dist; // 0x38
    float least_max_dist; // 0x3C
    const human_melee_combat_move_data* fallback_move; // 0x40
};

void ai_update_combat_target_building(npc* np); // 0x006804B0 (:949)
void ai_process_target_building_update(); // 0x006645D0 (:1009)
bool ignore_debug_projectile(const projectile* proj); // 0x005ED6C0 (:1337)
bool target_tracking_obstruction_callback(object* hit_objp, void* cb_data); // 0x005EE130 (:2777)
bool collect_movers_callback(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x0063E000 (:3279)
bool collect_buildings_callback(object* hit_objp, unsigned int shape_key, const void* data); // 0x005EE330 (:3397)
void ai_ignore_player(bool enable); // 0x00669670 (:12836)
