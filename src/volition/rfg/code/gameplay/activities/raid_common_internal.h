// Original: volition/rfg/code/gameplay/activities/raid_common_internal.h
//
// Raid definition data: the squads of a raid (common_raid_squad_info, raid_squad_info), the definition entry
// (base_raid_definition_data), exit cells and the splined search data used to place them. Placement of the helper
// types inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class ai_system;
class base_activity_data;
struct explosion_info;
class object_path_road;
struct squad_definition;
class vehicle_spawn_group_info;
struct xml_element;

// size 0x18
struct raid_exit_cell
{
    raid_exit_cell(); // 0x0075BA50

    vector center_pos; // 0x00
    uint64_t cell_handle; // 0x10
};

// size 0x24
struct splined_search_data
{
    splined_search_data(); // 0x0075B8C0

    bool search_down; // 0x00
    const object_path_road* prp; // 0x04
    vector region_center; // 0x08
    float region_radius_sqrd; // 0x14
    vector previous_point; // 0x18
};

// size 0x20
struct prepare_raid_cover_collect_data
{
    prepare_raid_cover_collect_data(); // 0x005E7630

    vector raid_target_pos; // 0x00
    vector dummy_pos; // 0x0C
    float raid_target_to_cover_pos_dist_sqr; // 0x18
    const ai_system* ai; // 0x1C
};

// size 0x10
class common_raid_squad_info
{
public:
    // size 0x1
    struct common_raid_squad_info_flags
    {
        bool required : 1; // 0x00:0
    };

    common_raid_squad_info(); // 0x00768DE0 (.h:59)
    void reset(); // 0x0075B8D0 (raid_common.cpp:778)
    human_teams get_team() const; // 0x0075B8F0 (raid_common.cpp:788)
    bool load(const xml_element* xml_squad_info_root); // 0x00771A00 (raid_common.cpp:794)

    const squad_definition* squad_def; // 0x00
    common_raid_squad_info_flags squad_flags; // 0x04
    const vehicle_spawn_group_info* veh_spawn_group; // 0x08
    int spawn_delay_ms; // 0x0C
};

// size 0x34
class raid_squad_info
{
public:
    raid_squad_info(); // 0x00768E00 (.h:101)
    void reset(); // 0x0075B900 (raid_common.cpp:835)
    human_teams get_team() const; // 0x0075B940 (raid_common.cpp:851)

    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* veh_spawn_group; // 0x04
    int spawn_delay_ms; // 0x08
    unsigned int spawn_object; // 0x0C
    unsigned int defended_object; // 0x10
    unsigned int orders_handle; // 0x14
    unsigned int squad_handle; // 0x18
    timestamp orders_creation_time; // 0x1C
    vector orders_destination_loc; // 0x20
    raid_team_states orders_state; // 0x2C
    struct
    {
        bool spawned : 1; // 0x00:0
        bool members_checked : 1; // 0x00:1
        bool required : 1; // 0x00:2
        bool guard_spawn_object : 1; // 0x00:3
        bool interior_spawn : 1; // 0x00:4
        bool inital_spawn : 1; // 0x00:5
        bool defending_team : 1; // 0x00:6
        bool attacking_team : 1; // 0x00:7
    } raid_squad_flags; // 0x30
};

// size 0x174
class base_raid_definition_data
{
public:
    // size 0x1
    struct raid_flags_type
    {
        bool disable_grenades : 1; // 0x00:0
    };

    base_raid_definition_data(); // 0x0077DEF0 (.h:158)
    void reset(); // 0x00771AD0 (raid_common.cpp:861)
    bool load(xml_element* xml_root); // 0x00789BA0 (raid_common.cpp:898)

    const char* raid_name; // 0x00
    const base_activity_data* base_data; // 0x04
    int level; // 0x08
    int max_startup_delay_ms; // 0x0C
    human_teams defending_team; // 0x10
    raid_flags_type raid_flags; // 0x14
    voice_line_handle start_radio_line; // 0x18
    voice_line_handle demolition_radio_line; // 0x1C
    float radius; // 0x20
    float radius_sqrd; // 0x24
    float safe_distance; // 0x28
    float safe_distance_sqrd; // 0x2C
    const explosion_info* explosive_info; // 0x30
    int explosive_delay; // 0x34
    farray<common_raid_squad_info,16> squad_prop_list; // 0x38
    int attacker_min; // 0x148
    int attacker_max; // 0x14C
    int defender_min; // 0x150
    int defender_max; // 0x154
    activity_standard_rewards standard_rewards; // 0x158
    voice_line_handle intro_line; // 0x164
    voice_line_handle vehicle_entry_line; // 0x168
    voice_line_handle travel_line; // 0x16C
    animation_action intro_anim_action; // 0x170
};
