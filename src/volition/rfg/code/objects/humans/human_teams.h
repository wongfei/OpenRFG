// Original: volition/rfg/code/objects/humans/human_teams.h
// Implementation: volition/rfg/code/objects/humans/human_teams.cpp (partial)
//
// Human teams (single player teams; the multiplayer names alias the same values). Placement inferred.
#pragma once

#include "volition/vlib/code/math/crc.h"

struct cfile;
class human;
class object;

enum human_teams
{
    HUMAN_TEAM_NONE = -1,
    HUMAN_TEAM_GUERILLA = 0,
    HUMAN_TEAM_EDF = 1,
    HUMAN_TEAM_CIVILIAN = 2,
    HUMAN_TEAM_MP_NEUTRAL = 2,
    HUMAN_TEAM_MARAUDER = 3,
    HUMAN_TEAM_MP_SPECTATOR = 3,
    NUM_HUMAN_TEAMS = 4,
};

// size 0x10
class human_team_names
{
public:
    human_team_names(const char* new_internal_name, const char* new_display_name); // 0x00A0D240 (:26)

    checksum_stri_pair internal_name; // 0x00
    checksum_stri_pair display_name; // 0x08
};

enum human_team_dispositions
{
    TEAM_DISPOSITION_HOSTILE = 0,
    TEAM_DISPOSITION_UNFRIENDLY = 1,
    TEAM_DISPOSITION_NONE = 2,
    TEAM_DISPOSITION_FRIENDLY = 3,
    TEAM_DISPOSITION_ALLIED = 4,
    NUM_HUMAN_TEAM_DISPOSITIONS = 5,
};

extern human_team_dispositions Current_human_team_dispositions[NUM_HUMAN_TEAMS][NUM_HUMAN_TEAMS]; // 0x03023820

// ---- inline in the original header
bool human_team_is_unaligned(human_teams team); // 0x005DBC10 (.h:90)
bool human_team_is_non_combatant(human_teams team); // 0x005DBC30 (.h:93)
bool human_teams_are_allies(human_teams team1, human_teams team2); // 0x005DBC40 (.h:99)
bool human_teams_are_allies(const human* hp, human_teams team); // 0x005DBC60 (.h:100)
bool human_teams_are_allies(const human* hp1, const human* hp2); // 0x005DBC80 (.h:101)
bool human_teams_are_friendly(human_teams team1, human_teams team2); // 0x005DBCA0 (.h:104)
bool human_teams_are_friendly(const human* hp, human_teams team); // 0x005DBCC0 (.h:105)
bool human_teams_are_friendly(const human* hp1, const human* hp2); // 0x005DBCE0 (.h:106)
bool human_teams_are_unfriendly(human_teams team1, human_teams team2); // 0x005DBD00 (.h:109)
bool human_teams_are_unfriendly(const human* hp, human_teams team); // 0x005DBD20 (.h:110)
bool human_teams_are_unfriendly(const human* hp1, const human* hp2); // 0x005DBD40 (.h:111)
bool human_teams_are_enemies(human_teams team1, human_teams team2); // 0x007539E0 (.h:114)
bool human_teams_are_enemies(const human* hp, human_teams team); // 0x005DBD60 (.h:115)
bool human_teams_are_enemies(const human* hp1, const human* hp2); // 0x005DBD80 (.h:116)

// ---- human_teams.cpp
human_teams human_teams_get_team(const object* obj); // 0x00A56A20 (:134)
human_teams human_team_from_name(const char* name); // 0x00A0D2A0 (:200)
const char* human_team_get_internal_name(human_teams team); // 0x00A0D340 (:233)
const wchar_t* human_team_get_display_name(human_teams team); // 0x00A0D380 (:256)
void human_teams_reset_all_dispositions(); // 0x00A0D3D0 (:306)
void human_teams_set_disposition(human_teams team1, human_teams team2, human_team_dispositions disposition); // 0x00A0D400 (:323)
void human_teams_reset_disposition(human_teams team1, human_teams team2, human_team_dispositions disposition); // 0x00A0D450 (:345)
bool human_team_is_non_combatant(const human* hp); // 0x00A0D4A0 (:367)
human_team_dispositions human_teams_get_disposition(human_teams team1, human_teams team2); // 0x00A0D4C0 (:379)
human_team_dispositions human_teams_get_disposition(const human* hp, human_teams team); // 0x00A0D510 (:384)
human_team_dispositions human_teams_get_disposition(const human* hp1, const human* hp2); // 0x00A0D560 (:390)
void human_teams_save(cfile* save_file); // 0x00A0D5A0 (:410)
void human_teams_load(cfile* save_file); // 0x00A0D5C0 (:419)

// ---- inline in the original header (definitions)
// 0x005DBC10 (.h:90): no team, the civilians, or a multiplayer-only team.
inline bool human_team_is_unaligned(human_teams team)
{
    return !((int)team > -1 && team != HUMAN_TEAM_CIVILIAN && (int)team < 4);
}

// 0x005DBC30 (.h:93)
inline bool human_team_is_non_combatant(human_teams team)
{
    return team == HUMAN_TEAM_CIVILIAN;
}

// 0x005DBC40 (.h:99)
inline bool human_teams_are_allies(human_teams team1, human_teams team2)
{
    return human_teams_get_disposition(team1, team2) == TEAM_DISPOSITION_ALLIED;
}

// 0x005DBC60 (.h:100)
inline bool human_teams_are_allies(const human* hp, human_teams team)
{
    return human_teams_get_disposition(hp, team) == TEAM_DISPOSITION_ALLIED;
}

// 0x005DBC80 (.h:101)
inline bool human_teams_are_allies(const human* hp1, const human* hp2)
{
    return human_teams_get_disposition(hp1, hp2) == TEAM_DISPOSITION_ALLIED;
}

// 0x005DBCA0 (.h:104)
inline bool human_teams_are_friendly(human_teams team1, human_teams team2)
{
    return human_teams_get_disposition(team1, team2) > TEAM_DISPOSITION_NONE;
}

// 0x005DBCC0 (.h:105)
inline bool human_teams_are_friendly(const human* hp, human_teams team)
{
    return human_teams_get_disposition(hp, team) > TEAM_DISPOSITION_NONE;
}

// 0x005DBCE0 (.h:106)
inline bool human_teams_are_friendly(const human* hp1, const human* hp2)
{
    return human_teams_get_disposition(hp1, hp2) > TEAM_DISPOSITION_NONE;
}

// 0x005DBD00 (.h:109)
inline bool human_teams_are_unfriendly(human_teams team1, human_teams team2)
{
    return human_teams_get_disposition(team1, team2) < TEAM_DISPOSITION_NONE;
}

// 0x005DBD20 (.h:110)
inline bool human_teams_are_unfriendly(const human* hp, human_teams team)
{
    return human_teams_get_disposition(hp, team) < TEAM_DISPOSITION_NONE;
}

// 0x005DBD40 (.h:111)
inline bool human_teams_are_unfriendly(const human* hp1, const human* hp2)
{
    return human_teams_get_disposition(hp1, hp2) < TEAM_DISPOSITION_NONE;
}

// 0x007539E0 (.h:114)
inline bool human_teams_are_enemies(human_teams team1, human_teams team2)
{
    return human_teams_get_disposition(team1, team2) == TEAM_DISPOSITION_HOSTILE;
}

// 0x005DBD60 (.h:115)
inline bool human_teams_are_enemies(const human* hp, human_teams team)
{
    return human_teams_get_disposition(hp, team) == TEAM_DISPOSITION_HOSTILE;
}

// 0x005DBD80 (.h:116)
inline bool human_teams_are_enemies(const human* hp1, const human* hp2)
{
    return human_teams_get_disposition(hp1, hp2) == TEAM_DISPOSITION_HOSTILE;
}
