// Original: volition/rfg/code/gamewide/player_profile.cpp
//
// The player profile on PC (UI.md UI4): one profile, the profile part of the save data (Last_profile_data_from_save_data),
// reset by the title screen's catch and saved with the save data. Nothing here waits: the profile is never busy and its
// save always succeeds.

#include "volition/rfg/code/gamewide/player_profile.h"

#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"

#include <string.h>

// rfg.pdb places it with game_save_generic.cpp (the save data's reader and writer, not ported); here until that is.
player_profile_data Last_profile_data_from_save_data; // 0x02279080

// Magic number: 0xFF, no sledgehammer chosen.
static const int No_sledge = 0xFF;

// 0x007E9D90 (:11)
void player_profile_data::reset()
{
    memset(this, 0, sizeof(*this));
    sledge_unique_id = No_sledge;
    stats_reset_profile_stats();
}

// 0x007EEC30 (:19)
void loaded_new_profile()
{
    stats_serialize_in_profile_stats();
    ui_tool_tips_system_load_from_profile();
}

// 0x007E9DB0 (:28)
// player_profile_data::reset of the save data's profile, inlined.
bool player_profile_init()
{
    memset(&Last_profile_data_from_save_data, 0, sizeof(Last_profile_data_from_save_data));
    Last_profile_data_from_save_data.sledge_unique_id = No_sledge;
    stats_reset_profile_stats();
    return true;
}

// 0x007C5950 (:35)
player_profile_data* player_profile_get(const char* const, int)
{
    return &Last_profile_data_from_save_data;
}

// 0x007D24A0 (:43)
int player_profile_save(const char* const, int)
{
    game_save_write_profile_data();
    return 0;
}

// 0x007C5960 (:53)
void player_profile_process()
{
}

// 0x007C5970 (:57)
bool player_profile_busy(int)
{
    return false;
}

// 0x007C5980 (:63)
bool player_profile_last_save_successful(int)
{
    return true;
}

// 0x007C5990 (:68)
void player_profile_set_thread_ownership(const char* const)
{
}
