// Original: volition/rfg/code/gameplay/activities/activity_raid_internal.h
//
// The raid activity (a modal activity, base of the attack and defend raids): its participants, demolition points and
// team demolition infos, raid targets (All_raid_targets), stragglers and the raid voice lines. Placement of the helper
// types inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/raid_common_internal.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_load_state;
struct bbox;
class object;
class object_bounding_box;
class object_raid_node;
class object_spawn_region;
class object_squad;
class object_squad_spawn_node;
class player;

enum raid_voice_types
{
    RAID_VOICE_NONE = -1,
    RAID_VOICE_PLAYER_ARRIVES = 0,
    RAID_VOICE_FALL_BACK = 1,
    RAID_VOICE_PUSH_FORWARD = 2,
    RAID_VOICE_GUERRILLA_REINFORCEMENTS = 3,
    RAID_VOICE_EDF_REINFORCEMENTS = 4,
    RAID_VOICE_WIN = 5,
    NUM_RAID_VOICE_TYPES = 6,
};

enum raid_sides
{
    RAID_SIDE_OFFENSE = 0,
    RAID_SIDE_DEFENSE = 1,
    NUM_RAID_SIDES = 2,
};

enum raid_type_flag_positions
{
    RAID_TYPE_FLAG_POS_REQUIRED = 0,
    RAID_TYPE_FLAG_POS_SPAWN_AT_TARGET = 1,
    NUM_RAID_TYPE_FLAG_POSITIONS = 2,
};

enum straggler_tracking_debug_status
{
    STDS_REENTER = 0,
    STDS_PROGRESS = 1,
    STDS_TRACKING = 2,
    STDS_KILLED = 3,
    NUM_STRAGGLER_TRACKING_DEBUG_STATUSES = 4,
};

// size 0x8
struct raid_voice_info
{
    raid_voice_info(voice_lines sl, voice_lines rl); // 0x007591A0 (activity_raid.cpp:91)

    voice_lines spoken_line; // 0x00
    voice_lines radio_line; // 0x04
};

// size 0x14
struct raid_straggler_info
{
    raid_straggler_info(); // 0x007591D0

    unsigned int npc_handle; // 0x00
    timestamp straggle_timer; // 0x04
    vector start_pos; // 0x08
};

// size 0x34
struct demolition_point
{
    demolition_point(); // 0x00759260

    vector pos; // 0x00
    matrix orient; // 0x0C
    int time; // 0x30
};

// size 0x38
struct demolition_point_eval_info
{
    demolition_point_eval_info(); // 0x0076EE60

    demolition_point demo_point; // 0x00
    float dist_sqrd; // 0x34
};

// size 0x24
struct team_demolition_info
{
    team_demolition_info(); // 0x0094D540

    float damage_delta; // 0x00
    unsigned int vip_handle; // 0x04
    timestamp vip_active_timer; // 0x08
    timestamp vip_pass_timer; // 0x0C
    timestamp vip_icon_timer; // 0x10
    timestamp vip_notification_timer; // 0x14
    timestamp vip_auto_swap_timer; // 0x18
    int xp_score_counter; // 0x1C
    int team_xp_score_counter; // 0x20
};

// size 0x8
struct raid_target_pair
{
    unsigned int target_handle; // 0x00
    unsigned int raid_handle; // 0x04
};

// size 0x20
class activity_raid_participant_info
{
public:
    activity_raid_participant_info(); // 0x00768D50 (activity_raid.h:33)
    void reset(); // 0x007592B0 (activity_raid.cpp:738)
    int hostile_humans_alive() const; // 0x007592D0 (activity_raid.cpp:744)
    int alied_humans_alive() const; // 0x00759310 (activity_raid.cpp:764)

    int humans_alive[4]; // 0x00
    int humans_dead[4]; // 0x10
};

// size 0x1F48
class activity_raid_data : public activity_base_modal
{
public:
    // size 0x4
    struct raid_flags_struct
    {
        unsigned int raid_started : 1; // 0x00:0
        unsigned int raid_complete : 1; // 0x00:1
        unsigned int attacker_victory : 1; // 0x00:2
        unsigned int defender_victory : 1; // 0x00:3
        unsigned int offensive_raid : 1; // 0x00:4
        unsigned int effect_stopped : 1; // 0x00:5
        unsigned int player_entered_region : 1; // 0x00:6
        unsigned int cheering_started : 1; // 0x00:7
        unsigned int watching_started : 1; // 0x00:8
        unsigned int skip_demolition : 1; // 0x00:9
        unsigned int splash_screen_active : 1; // 0x00:10
    };

    activity_raid_data(activity_modes raid_mode); // 0x0077DF10 (.h:85)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x00783B90 (activity_raid.cpp:1319)
    virtual void system_init(); // vtbl+0x1C 0x00759380 (activity_raid.cpp:856)
    virtual void level_init(); // vtbl+0x24 0x0076EE70 (activity_raid.cpp:864)
    virtual void activity_reset(); // vtbl+0x04 0x007A94F0 (activity_raid.cpp:879)
    virtual bool activity_start(); // vtbl+0x38 0x007AE870 (activity_raid.cpp:3616)
    virtual void activity_end(); // vtbl+0x3C 0x007AE720 (activity_raid.cpp:1079)
    virtual void activity_kill(); // vtbl+0x40 0x00798F40 (activity_raid.cpp:1087)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00783CC0 (activity_raid.cpp:1413)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B82E0 (activity_raid.cpp:1541)
    virtual activity_load_state activity_do_frame_loading(); // vtbl+0x54 0x00783CF0 (activity_raid.cpp:1425)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x007B66B0 (activity_raid.cpp:3428)
    virtual bool accept_new_squad(object_squad* squad); // vtbl+0x30 0x00784780 (activity_raid.cpp:3212)
    virtual void record_victory(); // vtbl+0xB0 0x007B6580 (activity_raid.cpp:3326)
    virtual bool is_inside_region(const vector& point) const; // vtbl+0xDC 0x00759440 (activity_raid.cpp:1102)
    virtual bool lock_alert_level() const; // vtbl+0xA0 0x00759490 (activity_raid.cpp:1110)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x007594A0 (activity_raid.cpp:1122)
    virtual bool is_player_cover_blown() const; // vtbl+0x9C 0x007594C0 (activity_raid.cpp:1138)
    virtual void render_icons() const; // vtbl+0x74 0x0076F000 (activity_raid.cpp:1195)
    virtual void render_regions() const; // vtbl+0x78 0x0076EED0 (activity_raid.cpp:1145)
    const activity_raid_participant_info* get_participant_info() const; // 0x00759350 (activity_raid.cpp:842)
    bool has_orders(unsigned int) const;
    bool npc_is_involved_in_raid(const npc* np) const; // 0x007595B0 (activity_raid.cpp:1796)
    bool human_is_involved_in_raid(const human* hp) const; // 0x0076F320 (activity_raid.cpp:1774)
    bool is_defending_team(const human* hp) const; // 0x00759530 (activity_raid.cpp:1586)
    virtual bool is_defending_team(human_teams team) const; // vtbl+0xE0 0x00759510 (activity_raid.cpp:1580)
    bool is_attacking_team(const human* hp) const; // 0x00759580 (activity_raid.cpp:1598)
    virtual bool is_attacking_team(human_teams team) const; // vtbl+0xE4 0x00759560 (activity_raid.cpp:1592)
    void play_raid_line(human_teams speaker_team, raid_voice_types voice_type); // 0x007A1DD0 (activity_raid.cpp:1610)
    void play_raid_line_response(const npc* call_np, voice_line_handle call_line, voice_lines response_line); // 0x00798F80 (activity_raid.cpp:1716)
    void play_state_transition_line(human_teams team, raid_team_states new_attacker_state, bool team_is_attacker); // 0x007A2070 (activity_raid.cpp:1938)
    bool building_is_being_demolished() const; // 0x00754010 (.h:203)
    const object_bounding_box* find_default_bbox() const; // 0x0076FAE0 (activity_raid.cpp:3393)
    const object_spawn_region* find_spawn_region(human_teams team, bool vehicle_spawn); // 0x00798CE0 (activity_raid.cpp:536)
    const player* player_in_raid_region(); // 0x00783A70 (activity_raid.cpp:824)
    void spawn_raid_objects(const object_raid_node* raid_node); // 0x00798DB0 (activity_raid.cpp:791)
    void collect_raid_objects(const object* spawn_objp, farray<object const*,512>& raid_objects); // 0x007849C0 (activity_raid.cpp:3373)
    void declare_winner(human_teams winning_team); // 0x00759390 (activity_raid.cpp:973)
    vector pick_point_in_box(const bbox& building_bbox, const vector& size); // 0x00759690 (activity_raid.cpp:2979)
    vector pick_destination_location(); // 0x0076FA10 (activity_raid.cpp:3056)
    int pick_far_destination_location_index(); // 0x0076F9D0 (activity_raid.cpp:3041)
    bool is_far_from_other_orders(const vector& pos); // 0x0076F970 (activity_raid.cpp:3017)
    void shuffle_destination_list(); // 0x0076F8F0 (activity_raid.cpp:2999)
    bool spawn_raid_squad(raid_squad_info& squad_info); // 0x007B2870 (activity_raid.cpp:3089)
    void do_frame_free_squads(); // 0x00783B00 (activity_raid.cpp:1299)
    void do_frame_convert_squads(); // 0x00783D30 (activity_raid.cpp:1477)
    void do_frame_ready_npc_participants(); // 0x00783BA0 (activity_raid.cpp:1330)
    void do_frame_npc_participants(); // 0x007AE740 (activity_raid.cpp:1375)
    void do_frame_player_wait(); // 0x007595D0 (activity_raid.cpp:1807)
    void do_frame_player_active(); // 0x00783F10 (activity_raid.cpp:1848)
    void do_frame_stat_tracking(); // 0x007A22C0 (activity_raid.cpp:2071)
    void do_frame_spawning(); // 0x007B6400 (activity_raid.cpp:2295)
    void do_frame_referee(); // 0x007A2580 (activity_raid.cpp:2901)
    void do_frame_demolish_building(); // 0x007994E0 (activity_raid.cpp:2704)
    void do_frame_orders(); // 0x00799480 (activity_raid.cpp:2598)
    void do_frame_orders_attacker(); // 0x00784340 (activity_raid.cpp:2615)
    void do_frame_orders_defender(); // 0x00784490 (activity_raid.cpp:2659)
    int find_shortest_edf_spawn_period(int time_since_start_ms) const; // 0x0076F440 (activity_raid.cpp:2257)
    void refree_check_building(); // 0x0076F740 (activity_raid.cpp:2810)
    void refree_check_humans(); // 0x0076F7C0 (activity_raid.cpp:2842)
    void collect_all_raid_npcs(farray<npc*,100>& npc_list); // 0x007842A0 (activity_raid.cpp:2581)
    void collect_all_raid_squads(farray<object_squad*,100>& squad_list); // 0x00784200 (activity_raid.cpp:2564)
    const human* get_enemy_human(const npc* np, const farray<npc*,100>& npc_list); // 0x0076F370 (activity_raid.cpp:2179)
    void set_weapons_flag(bool damage_flag, bool grenade_flag, bool turret_flag); // 0x007991A0 (activity_raid.cpp:2424)
    void convert_to_ambient(); // 0x007841A0 (activity_raid.cpp:2478)
    virtual void clear_all_flags(); // vtbl+0xE8 0x00799380 (activity_raid.cpp:2491)
    void set_squad_weapons_flags(object_squad* squad, bool required_squad); // 0x0076F5A0 (activity_raid.cpp:2529)
    bool all_squad_members_spawned() const; // 0x0076F4D0 (activity_raid.cpp:2383)
    bool should_demolish_target_building() const; // 0x0076F550 (activity_raid.cpp:2409)
    bool start_demolish_building(); // 0x007845E0 (activity_raid.cpp:2757)
    void clean_raid(); // 0x007AE4A0 (activity_raid.cpp:1013)
    void check_npc_beliefs(); // 0x007990D0 (activity_raid.cpp:2216)
    const object* get_spawn_object(human_teams);
    void supliment_squad_list(const farray<object const*,512>& raid_objects); // 0x007995F0 (activity_raid.cpp:3177)
    void add_squad_to_list(const common_raid_squad_info& squad_info); // 0x007996F0 (activity_raid.cpp:3280)
    void add_squad_to_list(const object_squad_spawn_node* squad_node); // 0x007848B0 (activity_raid.cpp:3248)
    void queue_raid_watch(); // 0x00784A60 (activity_raid.cpp:3686)
    void queue_raid_celebration(); // 0x00784BF0 (activity_raid.cpp:3738)
    void set_attacker_state(raid_squad_info& squad_info); // 0x00784040 (activity_raid.cpp:1913)
    void set_attacker_state(raid_team_states new_attacker_state); // 0x007A20E0 (activity_raid.cpp:1961)
    void set_defender_state(raid_squad_info& squad_info); // 0x007840F0 (activity_raid.cpp:2001)
    void set_defender_state(raid_team_states new_defender_state); // 0x007A21D0 (activity_raid.cpp:2028)
    raid_team_states get_squad_state(unsigned int squad_handle, raid_team_states default_state) const; // 0x0076EFB0 (activity_raid.cpp:1179)
    bool raid_ready() const; // 0x00754020 (.h:334)
    int time_until_demolition(); // 0x007594D0 (activity_raid.cpp:1162)
    float building_percent_destroyed();
    unsigned int find_defend_object(human_teams team, const vector& spawn_pos); // 0x0076ECB0 (activity_raid.cpp:569)
    bool is_inside_team_orders(const vector& pos, human_teams team) const; // 0x0076F250 (activity_raid.cpp:1447)
    int find_victory_period() const; // 0x0076FBF0 (activity_raid.cpp:3589)
    bool all_required_spawns_spawned() const; // 0x0076F8B0 (activity_raid.cpp:2882)
    virtual ~activity_raid_data(); // vtbl+0x00 0x0077E0E0

    farray<vector,64> external_dest_list; // 0x44C
    unsigned int raid_handle; // 0x75C
    timestamp stat_tracking_period; // 0x760
    timestamp raid_duration; // 0x764
    timestamp raid_termination_period; // 0x768
    timestamp victory_polling_timer; // 0x76C
    timestamp spawn_polling_timer; // 0x770
    timestamp accelerated_spawn_timeout; // 0x774
    timestamp npc_polling_timer; // 0x778
    timestamp belief_timer; // 0x77C
    timestamp attacker_orders_evaluation; // 0x780
    timestamp defender_orders_evaluation; // 0x784
    timestamp demolish_building_timer; // 0x788
    timestamp conversion_delay; // 0x78C
    farray<unsigned int,16> raid_target_list; // 0x790
    unsigned int enemy_contacts[4]; // 0x7E0
    unsigned int attacker_bounding_boxes[5]; // 0x7F0
    unsigned int defender_bounding_boxes[5]; // 0x804
    raid_flags_struct raid_flags; // 0x818
    npc_flags_struct npc_flags; // 0x81C
    human_flags hflags; // 0x828
    int voice_flags; // 0x838
    int radio_flags; // 0x83C
    const base_raid_definition_data* default_raid_info; // 0x840
    vector raid_center; // 0x844
    matrix raid_object_orient; // 0x850
    vector orders_location; // 0x874
    farray<raid_squad_info,32> reinforcement_squad_list; // 0x880
    farray<unsigned int,32> spawn_node_list; // 0xF10
    farray<raid_exit_cell,128> exit_cell_list; // 0xFA0
    farray<demolition_point,16> demolition_points; // 0x1BB0
    activity_raid_participant_info participant_info; // 0x1F00
    float attacker_value; // 0x1F20
    float defender_value; // 0x1F24
    int attackers_alive; // 0x1F28
    int defenders_alive; // 0x1F2C
    int spawn_region_idx; // 0x1F30
    timestamp raid_celebration_duration; // 0x1F34
    raid_team_states attacker_state; // 0x1F38
    raid_team_states defender_state; // 0x1F3C
    bool player_is_marauder; // 0x1F40
};
