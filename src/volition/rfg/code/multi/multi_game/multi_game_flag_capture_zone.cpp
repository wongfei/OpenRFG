// Original: volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.cpp
//
// The flag capture zones (markers of type MMT_FLAG_CAPTURE_ZONE): every frame in Capture the Flag the players inside
// a zone's box are handed to the mode (a carrier at its own zone captures).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_capture_the_flag.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/player.h"

// ---- file-static globals
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.

// 0x009F0E20 (:14): the players in each capture zone's box (16 at most), from the last marker by priority. Original:
// a collected handle that is no longer a player is passed on as null.
void multi_object_flag_capture_zone_process()
{
    const multi_gameplay_mode mode = Multiplayer ? Multi_gameplay_mode : MGPM_NONE;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* zone = Multi_markers_prioritized[i];
        if (zone->get_marker_type() != MMT_FLAG_CAPTURE_ZONE)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        zone->get_bbox(bmin, bmax);
        unsigned int handles[16];
        const int count = World.collect_objects<player>(bmin, bmax, handles, 16, nullptr, nullptr);
        for (int j = 0; j < count; j++)
        {
            if (mode == MGPM_CAPTURE_THE_FLAG)
            {
                capture_the_flag_mode_player_in_flag_capture_zone(World.object_from_handle<player>(handles[j]), zone);
            }
        }
    }
}

// 0x00998FE0 (:33): the team's capture zone (the last of its markers by priority)
multi_object_marker* multi_object_flag_capture_zone_get_zone_by_team(human_teams team)
{
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* zone = Multi_markers_prioritized[i];
        if (zone->get_marker_type() == MMT_FLAG_CAPTURE_ZONE && zone->team == team)
        {
            return zone;
        }
    }
    return nullptr;
}
