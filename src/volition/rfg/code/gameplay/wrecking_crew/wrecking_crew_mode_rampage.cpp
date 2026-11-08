// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_rampage.cpp
//
// wrecking.md WC1: the Rampage mode. Every attack and backpack activation costs turn time (the weapon's and backpack's
// rampage_time_penalty_ms from wrecking_crew.xtbl).

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_rampage.h"

#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/vlib/code/util/unicode_tools.h"

static int Rampage_time_per_turn_ms = 180000; // 0x01651928

// 0x00761970 (:37)
void wrecking_crew_mode_rampage_gameplay_process()
{
}

// 0x00761980 (:41)
void wrecking_crew_mode_rampage_render(wrecking_crew_states state)
{
}

// 0x00761990 (:47)
void wrecking_crew_mode_rampage_match_begin()
{
}

// 0x007619A0 (:51)
void wrecking_crew_mode_rampage_round_begin()
{
}

// 0x007619B0 (:55)
int wrecking_crew_mode_rampage_round_end()
{
    const int winner = wrecking_crew_get_player_with_most_round_wins();
    // Original: a -1 winner (no players) reads through a null status.
    const wrecking_crew_player_status* status = (unsigned int)winner < 4 ? &Wrecking_crew_player_status[winner] : nullptr;
    if (status->round_wins < Wrecking_crew_options.rounds_to_win)
    {
        return -1;
    }
    return winner;
}

// 0x007619E0 (:66)
int wrecking_crew_mode_rampage_players_turn_begin()
{
    return Rampage_time_per_turn_ms;
}

// 0x007619F0 (:71)
int wrecking_crew_mode_rampage_players_turn_end(int time_left)
{
    return wrecking_crew_get_player_with_highest_round_score();
}

// 0x007914D0 (:76): an attack is allowed while the time left covers its penalty for every projectile still in flight.
bool wrecking_crew_mode_rampage_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type)
{
    const wrecking_crew_weapon_info* weapon = Wrecking_crew_current_weapon;
    if (weapon == nullptr)
    {
        return false;
    }
    int time_left = Wrecking_crew_timer.time_until();
    const int num_projectiles = World.get_num_objects<projectile>();
    for (int i = 0; i < num_projectiles; i++)
    {
        time_left -= weapon->rampage_time_penalty_ms;
    }
    return time_left >= 0;
}

// 0x00761A00 (:93)
void wrecking_crew_mode_rampage_player_attacked(const weapon_info* w_info, damaged_by_types attack_type)
{
    const wrecking_crew_weapon_info* weapon = Wrecking_crew_current_weapon;
    if (Wrecking_crew_state == WCS_PLAYERS_TURN && weapon != nullptr)
    {
        if (Wrecking_crew_timer.valid())
        {
            Wrecking_crew_timer.deduct_time(weapon->rampage_time_penalty_ms);
        }
        gameplay_play_sfx(GSFX_WRECKING_CREW_FIRED_WEAPON);
    }
}

// 0x00761A50 (:108)
void wrecking_crew_mode_rampage_player_activated_backpack()
{
    const wrecking_crew_backpack_info* backpack = Wrecking_crew_current_backpack;
    if (Wrecking_crew_state == WCS_PLAYERS_TURN && backpack != nullptr)
    {
        if (Wrecking_crew_timer.valid())
        {
            Wrecking_crew_timer.deduct_time(backpack->rampage_time_penalty_ms);
        }
        gameplay_play_sfx(GSFX_WRECKING_CREW_FIRED_WEAPON);
    }
}

// 0x00761AA0 (:123): the penalty in seconds.
void wrecking_crew_mode_rampage_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    *override_icon = false;
    if (Wrecking_crew_current_weapon != nullptr)
    {
        _snprintf_safe(buffer, buffer_len, "-%d", Wrecking_crew_current_weapon->rampage_time_penalty_ms / 1000);
    }
}

// 0x00761AE0 (:133): the penalty in seconds, for backpacks that can be activated.
void wrecking_crew_mode_rampage_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    *override_icon = false;
    const wrecking_crew_backpack_info* backpack = Wrecking_crew_current_backpack;
    if (backpack != nullptr)
    {
        const multi_backpack_info* b_info = multi_game_backpack_get_info(backpack->backpack_type);
        if (b_info != nullptr && b_info->activatable)
        {
            _snprintf_safe(buffer, buffer_len, "-%d", backpack->rampage_time_penalty_ms / 1000);
        }
    }
}
