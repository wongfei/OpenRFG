// Original: volition/rfg/code/gameplay/activities/activity_vrail_internal.h
//
// The vehicle rail activity (a modal activity): riding shotgun infos, story chapters, sections and dialogue lines, the
// active sub-states and radio lines. Placement of the riding shotgun types inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/activities/vrail_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class base_activity_data;
struct cfile;
enum dialogue_priorities;
class district;
class npc;
class object;
class object_patrol;
class object_squad;
class rfg_mover;
class vehicle_rail_definition;

enum riding_shotgun_active_states
{
    INVALID_RS_ACTIVE_STATE = -1,
    RS_ACTIVE_PRE_SPAWN = 0,
    RS_ACTIVE_STATE_DO_SPAWN = 1,
    RS_ACTIVE_STATE_WAIT_JORGE = 2,
    RS_ACTIVE_STATE_WAIT_PLAYER = 3,
    RS_ACTIVE_STATE_RUNNING = 4,
    RS_ACTIVE_STATE_WAIT_SPLASH_SCREEN = 5,
    NUM_RS_ACTIVE_STATES = 6,
};

enum riding_shotgun_radio_lines
{
    RIDING_SHOTGUN_RADIO_LINE_NONE = -1,
    RIDING_SHOTGUN_RADIO_LINE_ACTIVITY_START = 0,
    RIDING_SHOTGUN_RADIO_LINE_ONE_TIME_INTRO = 1,
    RIDING_SHOTGUN_RADIO_LINE_VEHICLE_PRE_EXPLODE = 2,
    RIDING_SHOTGUN_RADIO_LINE_JORGE_BORED = 3,
    RIDING_SHOTGUN_RADIO_LINE_JORGE_SAYS_BYE = 4,
    RIDING_SHOTGUN_RADIO_LINE_MASON_ARRIVAL_JORGE_RES = 5,
    RIDING_SHOTGUN_RADIO_LINE_VEH_HEALTH_30 = 6,
    RIDING_SHOTGUN_RADIO_LINE_VEH_HEALTH_60 = 7,
    RIDING_SHOTGUN_RADIO_LINE_TARGET_DESTROYED = 8,
    RIDING_SHOTGUN_RADIO_LINE_JORGE_ARRIVAL = 9,
    RIDING_SHOTGUN_RADIO_LINE_JORGE_LEAVES_BEFORE_START = 10,
    RIDING_SHOTGUN_RADIO_LINE_MASON_SAYS_BYE = 11,
    RIDING_SHOTGUN_RADIO_LINE_MASON_ARRIVAL = 12,
    NUM_RIDING_SHOTGUN_RADIO_LINES = 13,
};

// size 0xC
struct riding_shotgun_dialogue_line_data
{
    voice_line_handle handle; // 0x00
    int delay_ms; // 0x04
    bool player_line; // 0x08
};

// size 0x50
struct riding_shotgun_story_section_data
{
    riding_shotgun_story_section_data(); // 0x00785CE0
    ~riding_shotgun_story_section_data(); // 0x00785D00
    riding_shotgun_story_section_data& operator=(const riding_shotgun_story_section_data& __that); // 0x007AC7F0

    unsigned int num_lines; // 0x00
    farray<riding_shotgun_dialogue_line_data,5> dialogue_lines; // 0x04
};

// size 0x4C8
struct riding_shotgun_story_chapter_data
{
    riding_shotgun_story_chapter_data(); // 0x0079A160
    ~riding_shotgun_story_chapter_data(); // 0x0079A170
    riding_shotgun_story_chapter_data& operator=(const riding_shotgun_story_chapter_data& __that); // 0x007B8B20

    unsigned int chapter_number; // 0x00
    unsigned int num_sections; // 0x04
    farray<riding_shotgun_story_section_data,15> chapter_sections; // 0x08
};

// size 0x70
struct riding_shotgun_info
{
    char* name; // 0x00
    char* caption; // 0x04
    char* secondary_caption; // 0x08
    const base_activity_data* base_data; // 0x0C
    const vehicle_rail_definition* vrail_data; // 0x10
    float target_score; // 0x14
    float par_score; // 0x18
    int max_civilians; // 0x1C
    float max_civilian_penalty; // 0x20
    int vehicle_hit_points; // 0x24
    gfm_alert_levels alert_level_start; // 0x28
    voice_line_handle radio_lines[13]; // 0x2C
    activity_standard_rewards standard_rewards; // 0x60
    float extra_reward; // 0x6C
};

// size 0x20
struct riding_shotgun_data
{
    riding_shotgun_data(); // 0x00768EF0 (.h:128)
    void clear(); // 0x007541C0 (.h:134)

    int soldiers; // 0x00
    float soldier_damage; // 0x04
    float building_damage; // 0x08
    float vehicle_damage; // 0x0C
    int civilians; // 0x10
    float civilian_damage; // 0x14
    float nuetral_damage; // 0x18
    float penalty; // 0x1C
};

// size 0xD9C
class activity_vehicle_rail_data : public activity_base_modal
{
public:
    activity_vehicle_rail_data(); // 0x007A00C0 (.h:263)
    virtual void system_init(); // vtbl+0x1C 0x007B90A0 (activity_vrail.cpp:629)
    virtual void level_init(); // vtbl+0x24 0x007B3170 (activity_vrail.cpp:638)
    virtual const char* get_title() const; // vtbl+0x80 0x0075A4E0 (activity_vrail.cpp:649)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00770E60 (activity_vrail.cpp:663)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00785DD0 (activity_vrail.cpp:690)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x00785DC0 (activity_vrail.cpp:676)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x0079A1A0 (activity_vrail.cpp:723)
    virtual bool activity_start(); // vtbl+0x38 0x007AEB70 (activity_vrail.cpp:762)
    virtual void activity_end(); // vtbl+0x3C 0x007AEE80 (activity_vrail.cpp:882)
    virtual void activity_kill(); // vtbl+0x40 0x007AEEE0 (activity_vrail.cpp:899)
    virtual void activity_complete(); // vtbl+0x4C 0x007B31D0 (activity_vrail.cpp:907)
    void activity_end_common(); // 0x007AEC50 (activity_vrail.cpp:808)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x007A9960 (activity_vrail.cpp:1041)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00770EE0 (activity_vrail.cpp:926)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B8BD0 (activity_vrail.cpp:993)
    virtual void activity_do_frame_clean_up(); // vtbl+0x64 0x0075A4F0 (activity_vrail.cpp:983)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x00770F60 (activity_vrail.cpp:960)
    virtual bool start_activity_spawn(const object* spawn_objp, const district* dp); // vtbl+0x08 0x007A26F0 (activity_vrail.cpp:916)
    void start_marker_effect();
    void stop_marker_effect();
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x0075A510 (activity_vrail.cpp:1118)
    virtual void register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints); // vtbl+0xB8 0x0079A240 (activity_vrail.cpp:1127)
    virtual void register_death(const object* target_obj, const object* attacker_obj); // vtbl+0xBC 0x00785EF0 (activity_vrail.cpp:1218)
    virtual void register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent); // vtbl+0xC0 0x00786100 (activity_vrail.cpp:1279)
    virtual void register_building_destroyed(const rfg_mover* objp); // vtbl+0xC4 0x0079A490 (activity_vrail.cpp:1321)
    virtual void save_activity_state(cfile* save_file); // vtbl+0xD0 0x0075A520 (activity_vrail.cpp:1337)
    virtual void load_activity_state(cfile* save_file); // vtbl+0xD4 0x0075A560 (activity_vrail.cpp:1345)
    void play_radio_line(riding_shotgun_radio_lines line, dialogue_priorities override_priority); // 0x00786290 (activity_vrail.cpp:1360)
    void append_radio_line(riding_shotgun_radio_lines line, int time_delay); // 0x0075A5A0 (activity_vrail.cpp:1402)
    virtual void render_icons() const; // vtbl+0x74 0x0079A500 (activity_vrail.cpp:1434)
    bool get_drop_point(vector& drop_pos) const; // 0x0075A620 (activity_vrail.cpp:1447)
    float get_target_score(); // 0x0075A650 (activity_vrail.cpp:1457)
    float get_display_score(); // 0x0075A670 (activity_vrail.cpp:1466)
    float get_building_score(); // 0x0075A690 (activity_vrail.cpp:1475)
    float get_vehicle_score(); // 0x0075A6B0 (activity_vrail.cpp:1484)
    float get_soldier_score(); // 0x0075A6D0 (activity_vrail.cpp:1493)
    float get_base_score() const; // 0x0075A6F0 (activity_vrail.cpp:1502)
    float get_current_penalty() const; // 0x0075A750 (activity_vrail.cpp:1511)
    float get_current_score() const; // 0x0075A810 (activity_vrail.cpp:1520)
    bool target_score_reached() const; // 0x0075A8A0 (activity_vrail.cpp:1525)
    const char* get_chapter_name() const; // 0x007541F0 (.h:378)
    bool play_story_section(unsigned int section_number); // 0x00786380 (activity_vrail.cpp:1591)
    void play_next_story_section(); // 0x007864D0 (activity_vrail.cpp:1645)
    vrail_patrol_state get_patrol_state() const; // 0x0075A990 (activity_vrail.cpp:1654)
    bool is_ready() const; // 0x00754200 (.h:388)
    bool is_running() const; // 0x00754210 (.h:391)
    unsigned int get_patrol() const; // 0x00754240 (.h:394)
    unsigned int get_patrol_squad() const; // 0x00754250 (.h:397)
    unsigned int get_patrol_vehicle() const; // 0x0077E340 (.h:398)
    void set_chase_vehicle_count(int new_count); // 0x0077E370 (.h:401)
    bool is_riding_shotgun_activity(); // 0x00754260 (.h:405)
    int get_squads(base_array<object_squad*>& squad_list); // 0x0079A650 (activity_vrail.cpp:2442)
    void stop_squad_spawning(); // 0x007A0250 (.h:410)
    void determine_reward_and_end(); // 0x007B3340 (activity_vrail.cpp:2117)
protected:
    bool set_up_vrail_patrol(); // 0x00770E90 (activity_vrail.cpp:706)
    void reset(); // 0x007AEF00 (activity_vrail.cpp:1665)
    void do_frame_active_pre_spawn(); // 0x007864F0 (activity_vrail.cpp:1710)
    void do_frame_active_do_spawn(); // 0x007B8390 (activity_vrail.cpp:1725)
    void do_frame_active_wait_jorge(); // 0x00786520 (activity_vrail.cpp:1755)
    void do_frame_active_wait_player(); // 0x00786610 (activity_vrail.cpp:1787)
    void do_frame_active_running(); // 0x007B8440 (activity_vrail.cpp:1892)
    void render_icon_player() const; // 0x00786890 (activity_vrail.cpp:1961)
    void render_icon_attackers() const; // 0x007868B0 (activity_vrail.cpp:1982)
    bool object_setup(); // 0x007B6D20 (activity_vrail.cpp:1999)
    object_squad* spawn_squad(const object_patrol* patrol); // 0x007B31F0 (activity_vrail.cpp:2076)
    bool vrail_completed(); // 0x0075A9A0 (activity_vrail.cpp:2190)
    void update_display_score(); // 0x0075A9C0 (activity_vrail.cpp:2195)
    void stop_all_flares() const; // 0x00785E80 (activity_vrail.cpp:1027)
public:
    virtual ~activity_vehicle_rail_data(); // vtbl+0x00 0x0077E3E0

protected:
    shared_vrail_patrol_data patrol_data; // 0x44C
    shared_vrail_spawn_data spawn_data; // 0x478
    const vehicle_rail_definition* current_vrail_def; // 0xD34
    unsigned int vrail_patrol_handle; // 0xD38
    unsigned int riding_shotgun_node_handle; // 0xD3C
    vector drop_off_pos; // 0xD40
    riding_shotgun_active_states active_state; // 0xD4C
    timestamp max_start_period; // 0xD50
    timestamp spawn_timeout; // 0xD54
    unsigned int squad_handle; // 0xD58
    float goal_damage; // 0xD5C
    riding_shotgun_data current_score; // 0xD60
    float display_score; // 0xD80
public:
    const riding_shotgun_info* current_info; // 0xD84
protected:
    bool played_timeout_warning; // 0xD88
    bool played_vrail_end; // 0xD89
    bool played_fire_message; // 0xD8A
    bool played_veh_dam30_message; // 0xD8B
    bool played_veh_dam60_message; // 0xD8C
    bool vrail_successfully_completed; // 0xD8D
    bool vrail_started_and_failed; // 0xD8E
    bool vrail_has_ended; // 0xD8F
    voice_line_handle most_recent_line_handle; // 0xD90
    const riding_shotgun_story_chapter_data* current_story_chapter; // 0xD94
    unsigned int story_section_number; // 0xD98
};

extern activity_vehicle_rail_data Vehicle_rail_activity_data; // 0x0226C5E8
