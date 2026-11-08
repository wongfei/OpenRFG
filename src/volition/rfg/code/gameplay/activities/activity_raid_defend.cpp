// Original: volition/rfg/code/gameplay/activities/activity_raid_defend.cpp

#include "volition/rfg/code/gameplay/activities/activity_raid_defend_internal.h"

#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/vlib/code/math/vector3.h"

defensive_raid_data Defensive_raid; // 0x022330C8

// 0x0077E130 (.h:18)
defensive_raid_data::defensive_raid_data()
    : activity_raid_data(ACTIVITY_MODE_RAID_DEFEND)
{
}

// 0x0077E150
defensive_raid_data::~defensive_raid_data()
{
}

// 0x00785520 (:18)
static bool all_defensive_raids_completed()
{
    for (int i = 0; i < World.get_num_objects<object_raid_node>(); i++)
    {
        const object_raid_node* node = World.get_object<object_raid_node>(i);
        if (node->is_defensive_raid() && !node->get_completed_flag())
        {
            return false;
        }
    }
    return true;
}

// Magic numbers: a raid spawns between 150 and 400 metres from the player.
static bool in_raid_spawn_range(const vector& player_pos, const object_raid_node* node)
{
    const float dist = vec_dist_squared(player_pos, node->pos);
    return dist >= 22500.0f && dist <= 160000.0f;
}

// 0x00785580 (:37)
static bool collect_spawn_objects_complete(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handle)
{
    for (int i = 0; i < World.get_num_objects<object_raid_node>(); i++)
    {
        const object_raid_node* node = World.get_object<object_raid_node>(i);
        if (node->is_defensive_raid() && node->can_spawn() && in_raid_spawn_range(player_pos, node))
        {
            if (spawn_objects.size() < spawn_objects.capacity())
            {
                spawn_objects.add(node);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    return spawn_objects.size() > 0;
}

// 0x00785670 (:67). rfg.exe: LTCG, district_handle in ECX, unused.
static bool collect_spawn_objects_incomplete(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handle)
{
    for (int i = 0; i < World.get_num_objects<object_raid_node>(); i++)
    {
        const object_raid_node* node = World.get_object<object_raid_node>(i);
        if (node->is_defensive_raid() && !node->get_completed_flag() && node->can_spawn() && in_raid_spawn_range(player_pos, node))
        {
            spawn_objects.add(node);
        }
    }
    return spawn_objects.size() > 0;
}

// 0x00785750 (:111)
bool defensive_raid_data::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    if (all_defensive_raids_completed())
    {
        return collect_spawn_objects_complete(spawn_objects, player_pos, district_handlep);
    }
    return collect_spawn_objects_incomplete(spawn_objects, player_pos, district_handlep);
}

// 0x00759D20 (:122)
bool defensive_raid_data::is_defending_team(human_teams team) const
{
    const player* pp = player_get_local_player();
    return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_ALLIED;
}

// 0x00759D50 (:134)
bool defensive_raid_data::is_attacking_team(human_teams team) const
{
    const player* pp = player_get_local_player();
    return pp != nullptr && human_teams_get_disposition(pp, team) == TEAM_DISPOSITION_HOSTILE;
}

// 0x00759D80 (:149)
const char* defensive_raid_data::get_title() const
{
    return "ACT_RAID_DEFENSE";
}

// 0x00759D90 (:163)
const char* defensive_raid_data::get_subtitle() const
{
    return "ACT_RAID_PROTECT";
}

// 0x00759DA0 (:168)
const char* defensive_raid_data::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(0x15); // Magic number: the raid's icon
}
