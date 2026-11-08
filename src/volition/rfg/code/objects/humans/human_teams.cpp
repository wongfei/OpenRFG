// Original: volition/rfg/code/objects/humans/human_teams.cpp
//
// Partial: the team names, the dispositions and human_teams_get_team (decompiled).
// Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/humans/human_teams.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/string_comp.h"

#include <string.h>

// 0x00A0D240 (:26)
human_team_names::human_team_names(const char* new_internal_name, const char* new_display_name)
    : internal_name(new_internal_name), display_name(new_display_name)
{
}

// ---- file statics
// 0x01694810 (:40), internal names and the localization ids of the display names, indexed by human_teams
static const human_team_names HUMAN_TEAM_NAMES[NUM_HUMAN_TEAMS] = {
    human_team_names("Guerilla", "MENU_GUERRILLA"),
    human_team_names("EDF", "MENU_EDF"),
    human_team_names("Civilian", "MENU_CIVILIAN"),
    human_team_names("Marauder", "MENU_MARAUDER"),
};

// 0x00A0D2A0 (:200)
// By internal or display name (case insensitive); "Neutral" is the civilian team in multiplayer.
human_teams human_team_from_name(const char* name)
{
    const checksum_stri name_checksum(name, 0);
    for (int team = 0; team < NUM_HUMAN_TEAMS; team++)
    {
        const human_team_names& names = HUMAN_TEAM_NAMES[team];
        if (name_checksum == names.internal_name.str_checksum && v_stricmp(name, names.internal_name.str) == 0)
        {
            return (human_teams)team;
        }
        if (name_checksum == names.display_name.str_checksum && v_stricmp(name, names.display_name.str) == 0)
        {
            return (human_teams)team;
        }
    }
    if (Multiplayer && v_stricmp(name, "Neutral") == 0)
    {
        return HUMAN_TEAM_MP_NEUTRAL;
    }
    return HUMAN_TEAM_NONE;
}

// 0x00A0D340 (:233)
const char* human_team_get_internal_name(human_teams team)
{
    if ((unsigned int)team > HUMAN_TEAM_MARAUDER)
    {
        return "None";
    }
    if (Multiplayer)
    {
        if (team == HUMAN_TEAM_MP_NEUTRAL)
        {
            return "Neutral";
        }
        if (team == HUMAN_TEAM_MP_SPECTATOR)
        {
            return "SPECTATOR";
        }
    }
    return HUMAN_TEAM_NAMES[team].internal_name.str;
}

// 0x00A0D380 (:256)
const wchar_t* human_team_get_display_name(human_teams team)
{
    if ((unsigned int)team >= NUM_HUMAN_TEAMS)
    {
        return L"None";
    }
    const char* id_name = Multiplayer && team == HUMAN_TEAM_MP_NEUTRAL ? "MENU_NEUTRAL" : HUMAN_TEAM_NAMES[team].display_name.str;
    const wchar_t* display_name = localization_get_display_string_req(id_name, nullptr);
    return display_name != nullptr ? display_name : L"!!Null team name";
}

// ---- dispositions (P3: human_system_init resets them)
// Default_human_team_dispositions (0x01310F38) is a file static of human.cpp, earlier in the same unity TU.
human_team_dispositions Current_human_team_dispositions[NUM_HUMAN_TEAMS][NUM_HUMAN_TEAMS]; // 0x03023820

// 0x00A0D270 (:94, LTCG: team1 in EDI, team2 in ESI)
static human_team_dispositions human_teams_get_disposition_internal(human_teams team1, human_teams team2)
{
    if ((unsigned int)team1 >= NUM_HUMAN_TEAMS || (unsigned int)team2 >= NUM_HUMAN_TEAMS)
    {
        return TEAM_DISPOSITION_NONE;
    }
    if (Multiplayer && !multi_game_mode_has_teams())
    {
        return TEAM_DISPOSITION_HOSTILE;
    }
    return Current_human_team_dispositions[team1][team2];
}

// 0x00A0D3D0 (:306)
void human_teams_reset_all_dispositions()
{
    memcpy(Current_human_team_dispositions, Default_human_team_dispositions, sizeof(Current_human_team_dispositions));
    for (int team = HUMAN_TEAM_GUERILLA; team < NUM_HUMAN_TEAMS; team++)
    {
        ai_system_process_team_disposition_change((human_teams)team);
    }
}

// 0x00A0D400 (:323)
void human_teams_set_disposition(human_teams team1, human_teams team2, human_team_dispositions disposition)
{
    if ((unsigned int)team1 < NUM_HUMAN_TEAMS && (unsigned int)team2 < NUM_HUMAN_TEAMS && team1 != team2 &&
        (unsigned int)disposition < NUM_HUMAN_TEAM_DISPOSITIONS)
    {
        Current_human_team_dispositions[team1][team2] = disposition;
        Current_human_team_dispositions[team2][team1] = disposition;
        ai_system_process_team_disposition_change(team1);
        ai_system_process_team_disposition_change(team2);
    }
}

// 0x00A0D450 (:345)
void human_teams_reset_disposition(human_teams team1, human_teams team2, human_team_dispositions disposition)
{
    if ((unsigned int)team1 < NUM_HUMAN_TEAMS && (unsigned int)team2 < NUM_HUMAN_TEAMS)
    {
        Current_human_team_dispositions[team1][team2] = Default_human_team_dispositions[team1][team2];
        Current_human_team_dispositions[team2][team1] = Default_human_team_dispositions[team2][team1];
        ai_system_process_team_disposition_change(team1);
        ai_system_process_team_disposition_change(team2);
    }
}

// 0x00A0D4A0 (:367)
bool human_team_is_non_combatant(const human* hp)
{
    return hp != nullptr && hp->get_team() == HUMAN_TEAM_CIVILIAN;
}

// 0x00A0D4C0 (:379)
human_team_dispositions human_teams_get_disposition(human_teams team1, human_teams team2)
{
    return human_teams_get_disposition_internal(team1, team2);
}

// 0x00A0D510 (:384)
human_team_dispositions human_teams_get_disposition(const human* hp, human_teams team)
{
    if (hp == nullptr)
    {
        return TEAM_DISPOSITION_NONE;
    }
    return human_teams_get_disposition_internal(hp->get_team(), team);
}

// 0x00A0D560 (:390)
human_team_dispositions human_teams_get_disposition(const human* hp1, const human* hp2)
{
    if (hp1 == nullptr || hp2 == nullptr)
    {
        return TEAM_DISPOSITION_NONE;
    }
    if (hp1 == hp2)
    {
        return TEAM_DISPOSITION_ALLIED;
    }
    return human_teams_get_disposition_internal(hp1->get_team(), hp2->get_team());
}

// 0x00A0D5A0 (:410)
void human_teams_save(cfile* save_file)
{
    cf_write(Current_human_team_dispositions, sizeof(Current_human_team_dispositions), save_file);
}

// 0x00A0D5C0 (:419)
void human_teams_load(cfile* save_file)
{
    cf_read(Current_human_team_dispositions, sizeof(Current_human_team_dispositions), save_file, false);
}

// 0x00A56A20 (:134)
// The team of an object: a human's current team, an rfg mover's, the team of a vehicle's most recent driver, a spawn
// region's or marker's, a spawn node's group or squad, a flag's, a weapon's default team; a backpack is its carrier's
// (or its rack's; civilian on neither). Anything else (and null) has none.
human_teams human_teams_get_team(const object* obj)
{
    while (obj != nullptr)
    {
        switch (obj->get_obj_type())
        {
        case OT_HUMAN:
            return ((const human*)obj)->current_team;
        case OT_MOVER:
            if (obj->get_sub_type() != OT_SUB_MOVER_RFG)
            {
                return HUMAN_TEAM_NONE;
            }
            return ((const object_mover*)obj)->team;
        case OT_VEHICLE:
            return ((const vehicle*)obj)->team_of_most_recent_driver;
        case OT_SPAWN_REGION:
            return ((const object_spawn_region*)obj)->team;
        case OT_NPC_SPAWN_NODE:
            return ((const object_npc_spawn_node*)obj)->h_spawn_group->get_team();
        case OT_SQUAD_SPAWN_NODE:
            return ((const object_squad_spawn_node*)obj)->get_team();
        case OT_MULTI_MARKER:
            return ((const multi_object_marker*)obj)->team;
        case OT_ITEM:
            if (obj->get_sub_type() == OT_SUB_ITEM_MULTI_FLAG)
            {
                return ((const multi_object_flag*)obj)->team;
            }
            if (obj->get_sub_type() == OT_SUB_ITEM_MULTI_BACKPACK)
            {
                const multi_object_backpack* backpack = (const multi_object_backpack*)obj;
                if (backpack->get_player_attached_to() != nullptr)
                {
                    obj = backpack->get_player_attached_to();
                }
                else if (backpack->get_backpack_rack() != nullptr)
                {
                    obj = backpack->get_backpack_rack();
                }
                else
                {
                    return HUMAN_TEAM_CIVILIAN;
                }
                continue;
            }
            if (obj->get_sub_type() == OT_SUB_ITEM_WEAPON)
            {
                return ((const weapon*)obj)->w_info->default_team;
            }
            return HUMAN_TEAM_NONE;
        default:
            return HUMAN_TEAM_NONE;
        }
    }
    return HUMAN_TEAM_NONE;
}

// Voice_line_disable_hash (0x01694850) is not this TU's: Ghidra lists human_team_from_name as a user only because the
// loop's end pointer &HUMAN_TEAM_NAMES[NUM_HUMAN_TEAMS] has its address.
