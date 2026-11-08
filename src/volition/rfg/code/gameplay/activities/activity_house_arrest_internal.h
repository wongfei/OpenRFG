// Original: volition/rfg/code/gameplay/activities/activity_house_arrest_internal.h
//
// The house arrest activity (a modal activity): the start infos, guard squads and hostages per house, the hostage status
// (captive, freed, passenger, dead) and the prisoners. Placement of house_arrest_hostage_status inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_load_state;
class district;
class house_arrest_level_data;
class house_arrest_squad_data;
class object;
class object_activity_spawn;
class object_dummy;
class object_house_arrest_node;
class object_npc_spawn_node;
class object_patrol;
class object_roadblock_node;
class object_squad;
class object_squad_spawn_node;
class squad_spawn_parameters;

enum house_arrest_hostage_status
{
    HOSTAGE_STATUS_INVALID = -1,
    HOSTAGE_STATUS_TIED_UP = 0,
    HOSTAGE_STATUS_FREED = 1,
    HOSTAGE_STATUS_RESCUED = 2,
    HOSTAGE_STATUS_DEAD = 3,
    HOSTAGE_STATUS_PASSANGER = 4,
    NUM_HOSTAGE_STATUS = 5,
};

// size 0xC
class house_arrest_start_info
{
public:
    house_arrest_start_info(); // 0x00769150 (.h:43)
    void reset(); // 0x007545C0 (.h:49)

    object_house_arrest_node* ha_node; // 0x00
    object_activity_spawn* ha_spawn_node; // 0x04
    float dist_sqrd; // 0x08
};

// size 0xC
class house_arrest_squad_info
{
public:
    house_arrest_squad_info(); // 0x00769170 (.h:59)
    void reset(); // 0x007545E0 (.h:65)

    unsigned int squad_handle; // 0x00
    unsigned int orders_handle; // 0x04
    const house_arrest_squad_data* base_squad_data; // 0x08
};

// size 0x14
class house_arrest_hostage_info
{
public:
    house_arrest_hostage_info(); // 0x00769190 (.h:74)
    void reset(); // 0x00754600 (.h:87)

    unsigned int spawn_node_handle; // 0x00
    unsigned int hostage_handle; // 0x04
    unsigned int action_node_handle; // 0x08
    unsigned int objective_highlight_handle; // 0x0C
    struct
    {
        bool spawned : 1; // 0x00:0
        bool rescued : 1; // 0x00:1
        bool killed : 1; // 0x00:2
    } hostage_flags; // 0x10
};

// size 0x8
class hostage_info
{
public:
    hostage_info() // 0x00769140 (activity_house_arrest.h:51)
    {
        reset();
    }
    void reset() // 0x00754550 (activity_house_arrest.h:56)
    {
        np = nullptr;
        status = HOSTAGE_STATUS_INVALID;
    }

    const npc* np; // 0x00
    house_arrest_hostage_status status; // 0x04
};

// size 0x5D8
class activity_house_arrest : public activity_base_modal
{
public:
    activity_house_arrest(); // 0x0077E860 (.h:102)
    virtual void system_init(); // vtbl+0x1C 0x007589A0 (activity_house_arrest.cpp:378)
    virtual void level_init(); // vtbl+0x24 0x007824B0 (activity_house_arrest.cpp:384)
    virtual bool activity_start(); // vtbl+0x38 0x007AD800 (activity_house_arrest.cpp:495)
    virtual void activity_end(); // vtbl+0x3C 0x007AD880 (activity_house_arrest.cpp:525)
    virtual void activity_kill(); // vtbl+0x40 0x007AD8A0 (activity_house_arrest.cpp:533)
    virtual const char* get_title() const; // vtbl+0x80 0x007589B0 (activity_house_arrest.cpp:545)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x0076D540 (activity_house_arrest.cpp:558)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x007589C0 (activity_house_arrest.cpp:583)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00798430 (activity_house_arrest.cpp:1310)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B60E0 (activity_house_arrest.cpp:710)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x007B2450 (activity_house_arrest.cpp:1244)
    virtual activity_load_state activity_do_frame_loading(); // vtbl+0x54 0x00782750 (activity_house_arrest.cpp:686)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x00798840 (activity_house_arrest.cpp:2406)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00782D50 (activity_house_arrest.cpp:1959)
    virtual void record_victory(); // vtbl+0xB0 0x007B6240 (activity_house_arrest.cpp:1983)
    virtual void record_loss(); // vtbl+0xB4 0x00758AF0 (activity_house_arrest.cpp:2041)
    virtual void report_recruitment(npc* np); // vtbl+0x8C 0x007A1B70 (activity_house_arrest.cpp:1610)
    virtual void report_action_node_end(unsigned int action_node_handle); // vtbl+0x98 0x0076DCA0 (activity_house_arrest.cpp:1584)
    virtual void render_icons() const; // vtbl+0x74 0x00783220 (activity_house_arrest.cpp:2486)
    void clean_house(); // 0x007A9070 (activity_house_arrest.cpp:2758)
    void count_prisoners(int& num_total, int& num_killed) const; // 0x0076E2D0 (activity_house_arrest.cpp:2622)
    bool all_hostages_dead(); // 0x0076E250 (activity_house_arrest.cpp:2577)
    bool all_hostages_saved(); // 0x0076E290 (activity_house_arrest.cpp:2598)
    void delete_prisoners(); // 0x0076E6B0 (activity_house_arrest.cpp:2873)
    bool house_ready() const; // 0x0076E740 (activity_house_arrest.cpp:2892)
    void do_spawn_pre(const object_house_arrest_node* ha_node); // 0x0076DF40 (activity_house_arrest.cpp:1925)
    void do_spawn_post(const object_house_arrest_node* ha_node); // 0x007987D0 (activity_house_arrest.cpp:1905)
    bool do_spawn_hostage(house_arrest_hostage_info& node_info); // 0x0076DD30 (activity_house_arrest.cpp:1679)
    void do_despawn_hostage(house_arrest_hostage_info& node_info); // 0x00758A20 (activity_house_arrest.cpp:1667)
    bool do_spawn_barricade(const object_dummy* dummy_node); // 0x0076DEF0 (activity_house_arrest.cpp:1851)
    bool do_spawn_roadblock(object_roadblock_node* roadblock); // 0x00758AD0 (activity_house_arrest.cpp:1899)
    void spawn_reinforcement(house_arrest_squad_info& squad_info); // 0x007B2470 (activity_house_arrest.cpp:1815)
    void create_orders(const object_squad_spawn_node* spawn_node, squad_spawn_parameters& spawn_params); // 0x007986B0 (activity_house_arrest.cpp:1745)
    bool create_orders_patrol(const object_squad_spawn_node* spawn_node, const object_patrol* patrol, squad_spawn_parameters& spawn_params); // 0x00758A50 (activity_house_arrest.cpp:1726)
    void create_orders_squad_node(const object_squad_spawn_node* spawn_node, squad_spawn_parameters& spawn_params); // 0x00758A90 (activity_house_arrest.cpp:1736)
    bool house_destreamed() const; // 0x0076D3B0 (activity_house_arrest.cpp:407)
    void reset(); // 0x0076D450 (activity_house_arrest.cpp:453)
    void play_house_arrest_line(const vector& pos, human_teams team, voice_lines line); // 0x00782C70 (activity_house_arrest.cpp:1557)
    void play_chat_line(); // 0x00782A10 (activity_house_arrest.cpp:1476)
    void hostage_draw_weapon(); // 0x0076DBE0 (activity_house_arrest.cpp:1441)
    void parse_activity_objects(); // 0x00783180 (activity_house_arrest.cpp:2307)
    void do_frame_reinforcements(); // 0x007B25C0 (activity_house_arrest.cpp:1862)
    void do_frame_hostages(bool active_state); // 0x00782F50 (activity_house_arrest.cpp:2223)
    void do_frame_hostage_spawn(); // 0x0076E150 (activity_house_arrest.cpp:2179)
    void do_frame_squad_management(); // 0x00782E40 (activity_house_arrest.cpp:2108)
    void do_frame_edf_count(); // 0x0076DFD0 (activity_house_arrest.cpp:2047)
    void spawn_all_hostages(); // 0x0076E110 (activity_house_arrest.cpp:2160)
    virtual bool start_activity_spawn(const object* spawn_objp, const district* dp); // vtbl+0x08 0x007A1C60 (activity_house_arrest.cpp:2354)
    bool load_single_hostage_node(const object_npc_spawn_node* npc_node); // 0x00782520 (activity_house_arrest.cpp:646)
    void set_squad_flags(object_squad* squad); // 0x0076DE30 (activity_house_arrest.cpp:1790)
    bool are_all_hostages_freed() const; // 0x0076E360 (activity_house_arrest.cpp:2648)
    void enable_objective_highlights(); // 0x0076E3F0 (activity_house_arrest.cpp:2678)
    void stop_objective_highlight(unsigned int npc_handle); // 0x0076E640 (activity_house_arrest.cpp:2719)
    void process_results(); // 0x00758B30 (activity_house_arrest.cpp:2735)
    bool is_ha_npc(const npc* np) const; // 0x0076DB90 (activity_house_arrest.cpp:1285)
    void add_start_up_icons(); // 0x00782910 (activity_house_arrest.cpp:1053)
    void add_start_up_icon_active(); // 0x0076D830 (activity_house_arrest.cpp:985)
    void do_frame_start_up(bool hard_load); // 0x007B2350 (activity_house_arrest.cpp:1214)
    void do_frame_start_up_inactive(bool hard_load, house_arrest_start_info& start_info); // 0x007B2310 (activity_house_arrest.cpp:1113)
    bool do_frame_start_up_active(house_arrest_start_info& start_info); // 0x0076D9A0 (activity_house_arrest.cpp:1132)
    bool get_nearest_activity(house_arrest_start_info& start_info); // 0x007827E0 (activity_house_arrest.cpp:935)
    unsigned int get_informant_npc(); // 0x0076D7B0 (activity_house_arrest.cpp:817)
    void informant_spawn(const house_arrest_start_info& start_info); // 0x007B20B0 (activity_house_arrest.cpp:859)
    void informant_despawn(); // 0x0076DA50 (activity_house_arrest.cpp:1185)
    void release_and_reset_squad(); // 0x0076DAA0 (activity_house_arrest.cpp:1200)
    void spawn_effect_kill(); // 0x007589D0 (activity_house_arrest.cpp:1176)
    unsigned int get_activity_start_node_from_handle(unsigned int base_handle); // 0x00758BA0 (activity_house_arrest.cpp:2897)
    void notify_npc_at_safehouse(npc* np); // 0x0076DB10 (activity_house_arrest.cpp:1256)
    void check_hostage_execution_timer(); // 0x0076D620 (activity_house_arrest.cpp:588)
    void check_hostage_falling(); // 0x0076D700 (activity_house_arrest.cpp:629)
    virtual ~activity_house_arrest(); // vtbl+0x00 0x0077E960

    unsigned int hud_message_handle; // 0x44C
    int prisoners_rescued_total; // 0x450
    int hostages_killed_by_player; // 0x454
    int defender_count; // 0x458
    unsigned int ha_node_handle; // 0x45C
    const house_arrest_level_data* base_house_arrest_data; // 0x460
    npc_flags_struct npc_flags; // 0x464
    human_flags hflags; // 0x470
    vector house_center; // 0x480
    farray<house_arrest_squad_info,8> reinforcement_squad_list; // 0x48C
    farray<unsigned int,16> squad_node_list; // 0x4FC
    farray<house_arrest_hostage_info,3> hostage_info_list; // 0x54C
    timestamp do_frame_polling_period; // 0x598
    timestamp hostage_line_period; // 0x59C
    timestamp response_line_period; // 0x5A0
    vector last_hostage_line_loc; // 0x5A4
    timestamp chat_period; // 0x5B0
    timestamp vehicle_complaint_period; // 0x5B4
    timestamp defender_count_event; // 0x5B8
    timestamp post_spawn_duration; // 0x5BC
    timestamp hostage_kill_timer; // 0x5C0
    timestamp leaving_hostage_message_timer; // 0x5C4
    struct
    {
        bool hostage_freed : 1; // 0x00:0
        bool all_hostages_freed : 1; // 0x00:1
        bool all_reinforcements_spawned : 1; // 0x00:2
        bool safe_house_line_played : 1; // 0x00:3
        bool all_objective_highlights_started : 1; // 0x00:4
        bool does_not_have_interface : 1; // 0x00:5
        bool played_spot_line : 1; // 0x00:6
        bool splash_screen_active : 1; // 0x00:7
        bool added_safehouse_marker : 1; // 0x01:0
        bool gps_triggered : 1; // 0x01:1
    } ha_flags; // 0x5C8
private:
    unsigned int start_up_squad_handle; // 0x5CC
    unsigned int activity_spawn_node_handle; // 0x5D0
    unsigned int house_arrest_node_handle; // 0x5D4
};

extern activity_house_arrest House_arrest_activity; // 0x022481E0
extern float Player_kills_hostage_morale_penalty; // 0x01651334
