// Original: volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator_internal.h
//
// Reinforcement coordinator data: reinforcement profiles per alert level, the guerrilla squads that answer EDF attacks,
// the master spawn info (roads, nodes, cells near the player), cached orders and the per-team allocations of
// reinforcement squads. Placement of the enums inferred.
#pragma once

#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/gameplay/activities/reinforcement_squad.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class guerrilla_reinforcement_info;
struct nav_cell;
class object;
class object_path_road;
class object_squad;
class object_squad_spawn_node;
class player;
struct squad_definition;
class squad_spawn_parameters;
class vehicle_spawn_group_info;
struct xml_element;

enum desired_reinforcement_group
{
    INVALID_DESIRED_REINFORCEMENT_GROUP = -1,
    REINFORCEMENT_GROUP_ON_FOOT = 0,
    REINFORCEMENT_GROUP_IN_VEHICLE = 1,
    REINFORCEMENT_GROUP_FLYER = 2,
    NUM_DESIRED_REINFORCEMENT_GROUPS = 3,
};

// size 0x4
struct reinforcement_spawn_type
{
    unsigned int on_foot : 1; // 0x00:0
    unsigned int in_vehicle : 1; // 0x00:1
    unsigned int flyer : 1; // 0x00:2
};

// size 0x1C
struct reinforcement_road_spawn_info
{
    reinforcement_road_spawn_info(); // 0x0075B290

    road_spline_identifier road_id; // 0x00
    float dist_to_player_sqrd; // 0x08
    float travel_distance; // 0x0C
    vector spawn_pos; // 0x10
};

// size 0x10
class squad_reinforcement_profile
{
public:
    squad_reinforcement_profile(); // 0x007710B0 (.h:161)
    bool load(xml_element* xml_squad_root); // 0x0075B400 (gfm_reinforcement_coordinator.cpp:1972)
    void reset(); // 0x0075AF60 (.h:177)

    const squad_definition* squad_def; // 0x00
    const squad_definition* casual_squad_def; // 0x04
    bool destroy_partial; // 0x08
    const vehicle_spawn_group_info* veh_spawn_group; // 0x0C
};

// size 0x6C
class reinforcement_profile
{
public:
    reinforcement_profile(); // 0x00787010 (.h:189)
    bool load(xml_element* xml_reinforcement_root); // 0x007878B0 (gfm_reinforcement_coordinator.cpp:2029)
    bool all_slots_reserved() const; // 0x00771500 (gfm_reinforcement_coordinator.cpp:2013)
    bool has_vehicle_reservation() const; // 0x0075AF70 (.h:219)
    void reset(); // 0x007710C0 (.h:222)

    const char* name; // 0x00
    gfm_alert_levels required_alert_level; // 0x04
    int edf_progression_level; // 0x08
    int spawn_delay_ms; // 0x0C
    reinforcement_spawn_type reinforcement_type; // 0x10
    const vehicle_spawn_group_info* reservation_group; // 0x14
    const vehicle_spawn_group_info* required_group; // 0x18
    farray<squad_reinforcement_profile,4> squad_info_list; // 0x1C
};

// size 0x14
class guerrilla_squad_info
{
public:
    guerrilla_squad_info(); // 0x00771070 (.h:85)
    void do_frame(gfm_alert_levels alert_level); // 0x007B8620 (gfm_reinforcement_coordinator.cpp:867)
    void do_frame_live(gfm_alert_levels alert_level, object_squad* squad); // 0x0075B1B0 (gfm_reinforcement_coordinator.cpp:893)
    void do_frame_none(gfm_alert_levels alert_level); // 0x007B6EA0 (gfm_reinforcement_coordinator.cpp:937)
    void do_spawn(const guerrilla_reinforcement_info* guerrilla_info); // 0x007B3890 (gfm_reinforcement_coordinator.cpp:995)
    void release_squad(object_squad* squad); // 0x0075B140 (gfm_reinforcement_coordinator.cpp:879)
    void reset(int respawn_delay_ms); // 0x0075B110 (gfm_reinforcement_coordinator.cpp:855)

    const guerrilla_reinforcement_info* info; // 0x00
    unsigned int squad_handle; // 0x04
    timestamp lifetime; // 0x08
    timestamp respawn_period; // 0x0C
    bool hud_warning; // 0x10
};

// size 0xC
class guerrilla_reinforcement_info
{
public:
    guerrilla_reinforcement_info(); // 0x00771050 (.h:69)
    bool load(xml_element* xml_root); // 0x00771380 (gfm_reinforcement_coordinator.cpp:1048)
    void reset(); // 0x0075B260 (gfm_reinforcement_coordinator.cpp:1040)

    const squad_definition* squad_def; // 0x00
    float morale_level; // 0x04
    int respawn_delay_ms; // 0x08
};

// size 0x488
class master_spawn_info
{
public:
    master_spawn_info(bool vehicle_flag, float min_spawn_dist, float max_spawn_dist); // 0x00786F60 (.h:113)
    void build_road_info(); // 0x0079AD00 (gfm_reinforcement_coordinator.cpp:1096)
    void build_node_info(); // 0x00787320 (gfm_reinforcement_coordinator.cpp:1155)
    void build_cell_info(); // 0x00787430 (gfm_reinforcement_coordinator.cpp:1197)
    void invalidate(); // 0x0075B280 (gfm_reinforcement_coordinator.cpp:1083)
    void reset(); // 0x00771460 (gfm_reinforcement_coordinator.cpp:1089)
    ~master_spawn_info(); // 0x00787070

    const bool VEHICLE_PF; // 0x00
    const float MIN_SPAWN_DISTANCE; // 0x04
    const float MIN_SPAWN_DISTANCE_SQRD; // 0x08
    const float MAX_SPAWN_DISTANCE; // 0x0C
    const float MAX_SPAWN_DISTANCE_SQRD; // 0x10
    farray<reinforcement_road_spawn_info,32> road_spawn_list; // 0x14
    farray<object_squad_spawn_node const*,32> node_spawn_list; // 0x3A4
    farray<nav_cell const*,16> cell_spawn_list; // 0x434
    struct
    {
        unsigned int road_info_valid : 1; // 0x00:0
        unsigned int node_info_valid : 1; // 0x00:1
        unsigned int cell_info_valid : 1; // 0x00:2
    } spawn_flags; // 0x484
};

// size 0x1C
class cached_orders_info
{
public:
    cached_orders_info(); // 0x00771120 (.h:424)
    bool allow_full_player_update(unsigned int& new_witness_handle); // 0x007876E0 (gfm_reinforcement_coordinator.cpp:1295)
    bool allow_partial_player_update(); // 0x0075B300 (gfm_reinforcement_coordinator.cpp:1350)
    void full_update_player_location(unsigned int npc_handle, const player* pp); // 0x00771470 (gfm_reinforcement_coordinator.cpp:1279)
    void partial_update_player_location(unsigned int npc_handle, const player* pp); // 0x0075B2D0 (gfm_reinforcement_coordinator.cpp:1287)
    const vector& get_orders_location(); // 0x007877E0 (gfm_reinforcement_coordinator.cpp:1374)
    void reset(); // 0x0075B2A0 (gfm_reinforcement_coordinator.cpp:1269)
    bool in_use() const; // 0x0075AFB0 (.h:457)

    unsigned int witness_handle; // 0x00
    vector player_pos; // 0x04
    timestamp los_timeout; // 0x10
    timestamp green_reset_timeout; // 0x14
    int last_framecount; // 0x18
};

extern cached_orders_info Orders_info; // 0x016572D0

// size 0x18
class road_search_info
{
public:
    road_search_info(); // 0x0075AF90 (.h:360)
    void reset(); // 0x0075B350 (gfm_reinforcement_coordinator.cpp:1404)

    const object_path_road* road; // 0x00
    vector connection_pos; // 0x04
    float connection_dist; // 0x10
    float base_travel_dist; // 0x14
};

// size 0x28
class road_node_search_info
{
public:
    road_node_search_info(); // 0x0075AFA0 (.h:380)
    void reset(); // 0x0075B380 (gfm_reinforcement_coordinator.cpp:1416)

    const object_path_road* road; // 0x00
    int road_index; // 0x04
    vector node_pos; // 0x08
    vector destination_pos; // 0x14
    float travel_dist; // 0x20
    float total_dist_sqrd; // 0x24
};

// size 0x44
class spawn_location_cache
{
public:
    spawn_location_cache(); // 0x007710F0 (.h:406)
    void reset(); // 0x0075B3C0 (gfm_reinforcement_coordinator.cpp:1430)

    vector player_pos; // 0x00
    vector spawn_pos; // 0x0C
    matrix spawn_orient; // 0x18
    unsigned int spawn_obj_handle; // 0x3C
    unsigned int road_handle; // 0x40
};

// size 0x78
class squad_reinforcement_allocation
{
public:
    squad_reinforcement_allocation(); // 0x007A9A10 (.h:235)
    void spawn_squad(squad_spawn_parameters& squad_params); // 0x007B6FC0 (gfm_reinforcement_coordinator.cpp:2247)
    void spawn_squad(const nav_cell* spawn_cell); // 0x007B8950 (gfm_reinforcement_coordinator.cpp:2212)
    void spawn_squad(const object_squad_spawn_node* squad_spawn_node); // 0x007B8850 (gfm_reinforcement_coordinator.cpp:2181)
    void spawn_squad(const road_spline_identifier& road_id); // 0x007B8670 (gfm_reinforcement_coordinator.cpp:2119)
    void do_frame(); // 0x007B39A0 (gfm_reinforcement_coordinator.cpp:2113)
    void render_minimap(); // 0x0079AEF0 (gfm_reinforcement_coordinator.cpp:2287)
    bool is_in_use() const; // 0x0075AF80 (.h:262)
    void reset(); // 0x007A2CD0 (.h:273)
    ~squad_reinforcement_allocation(); // 0x0077B000

    const squad_reinforcement_profile* squad_info; // 0x00
    reinforcement_squad base_squad_info; // 0x04
    unsigned int spawn_obj_handle; // 0x64
    vector spawned_pos; // 0x68
    struct
    {
        unsigned int spawned : 1; // 0x00:0
        unsigned int road_spawn : 1; // 0x00:1
        unsigned int node_spawn : 1; // 0x00:2
        unsigned int cell_spawn : 1; // 0x00:3
    } squad_flags; // 0x74
};

// size 0x204
class reinforcement_allocation
{
public:
    reinforcement_allocation(gfm_alert_levels alert_level, spawn_slot_types slot_type); // 0x007AF040 (.h:291)
    bool all_reinforcements_eliminated() const; // 0x00787870 (gfm_reinforcement_coordinator.cpp:1537)
    void release_and_reset(); // 0x007A9A40 (gfm_reinforcement_coordinator.cpp:1445)
    void do_frame(gfm_alert_levels alert_level); // 0x007B9620 (gfm_reinforcement_coordinator.cpp:1480)
    void render_minimap(); // 0x007A2D90 (gfm_reinforcement_coordinator.cpp:1489)
private:
    void reset(); // 0x007A2CF0 (gfm_reinforcement_coordinator.cpp:1466)
    void do_frame_active(); // 0x007B94A0 (gfm_reinforcement_coordinator.cpp:1565)
    void do_frame_spawn(); // 0x007B9420 (gfm_reinforcement_coordinator.cpp:1850)
    void do_frame_inactive(); // 0x007B6F20 (gfm_reinforcement_coordinator.cpp:1885)
    void do_spawn(squad_reinforcement_allocation& squad_info); // 0x007B9380 (gfm_reinforcement_coordinator.cpp:1816)
    bool do_spawn_node(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info); // 0x007B8C80 (gfm_reinforcement_coordinator.cpp:1625)
    bool do_spawn_road(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info); // 0x007B92F0 (gfm_reinforcement_coordinator.cpp:1650)
    bool do_spawn_cell(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info); // 0x007B8DB0 (gfm_reinforcement_coordinator.cpp:1791)
    bool do_spawn_road_directional(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info, const vector& direction); // 0x007B90C0 (gfm_reinforcement_coordinator.cpp:1663)
    bool do_spawn_road_near(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info); // 0x007B8CD0 (gfm_reinforcement_coordinator.cpp:1733)
    const reinforcement_profile* pick_new_profile(); // 0x0079AE90 (gfm_reinforcement_coordinator.cpp:1953)
    const vehicle_spawn_group_info* pick_spawn_group(); // 0x007714B0 (gfm_reinforcement_coordinator.cpp:1922)
public:
    ~reinforcement_allocation(); // 0x0079A960

    const gfm_alert_levels base_alert_level; // 0x00
    const spawn_slot_types base_slot_type; // 0x04
    const reinforcement_profile* current_profile; // 0x08
    timestamp respawn_period; // 0x0C
    farray<squad_reinforcement_allocation,4> reinforcement_squads; // 0x10
    struct
    {
        unsigned int spawned : 1; // 0x00:0
        unsigned int resource_reserved : 1; // 0x00:1
    } reinforcement_flags; // 0x200
};

// size 0x38
struct team_spawn_location
{
    void reset(); // 0x0075B5A0 (gfm_reinforcement_coordinator.cpp:3074)
    team_spawn_location(); // 0x00A1CE20

    const object* spawn_objp; // 0x00
    const object_path_road* road; // 0x04
    vector spawn_pos; // 0x08
    matrix spawn_orient; // 0x14
};
