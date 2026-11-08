// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.cpp
//
// wrecking.md WC1: the Mayhem mode. A turn allows ceil((round + 1) * mayhem_attack_multiplier) attacks of the weapon;
// projectiles still in flight count as used.

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.h"

#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <math.h>

static int Mayhem_turn_length_ms = 45000; // 0x01651920
static int Mayhem_turn_length_per_round_ms = 15000; // 0x01651924
static int Mayhem_attacks_used; // 0x022296C0

// 0x00761850 (:32)
static int mayhem_get_attacks_per_turn()
{
    if (Wrecking_crew_current_weapon == nullptr)
    {
        return 0;
    }
    return (int)(float)ceil((float)(Wrecking_crew_current_round + 1) * Wrecking_crew_current_weapon->mayhem_attack_multiplier);
}

// 0x00791190 (:40): the attacks made this turn plus the projectiles in flight that are not singularity bombs.
static int mayhem_get_attacks_used()
{
    int used = Mayhem_attacks_used;
    const int num_projectiles = World.get_num_objects<projectile>();
    for (int i = 0; i < num_projectiles; i++)
    {
        const projectile* pp = World.get_object<projectile>(i);
        if (pp->w_info->weapon_class != WEAPON_CLASS_SINGULARITY_BOMB)
        {
            used++;
        }
    }
    return used;
}

// 0x007911E0 (:54): the attacks made this turn plus every projectile in flight.
static int mayhem_get_attacks_almost_used()
{
    int used = Mayhem_attacks_used;
    const int num_projectiles = World.get_num_objects<projectile>();
    for (int i = 0; i < num_projectiles; i++)
    {
        used++;
    }
    return used;
}

// 0x007618A0 (:70)
void wrecking_crew_mode_mayhem_gameplay_process()
{
}

// 0x007618B0 (:75)
void wrecking_crew_mode_mayhem_render(wrecking_crew_states state)
{
}

// 0x007618C0 (:81)
void wrecking_crew_mode_mayhem_match_begin()
{
}

// 0x007618D0 (:85)
void wrecking_crew_mode_mayhem_round_begin()
{
}

// 0x007618E0 (:89)
int wrecking_crew_mode_mayhem_round_end()
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

// 0x00761910 (:100): the turn grows by 15 s each round.
int wrecking_crew_mode_mayhem_players_turn_begin()
{
    Mayhem_attacks_used = 0;
    return Mayhem_turn_length_per_round_ms * Wrecking_crew_current_round + Mayhem_turn_length_ms;
}

// 0x00761930 (:107)
int wrecking_crew_mode_mayhem_players_turn_end(int time_left)
{
    return wrecking_crew_get_player_with_highest_round_score();
}

// 0x00791210 (:112)
bool wrecking_crew_mode_mayhem_is_players_turn_over()
{
    const int per_turn = mayhem_get_attacks_per_turn();
    return mayhem_get_attacks_used() >= per_turn;
}

// 0x00791280 (:117)
bool wrecking_crew_mode_mayhem_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type)
{
    const int per_turn = mayhem_get_attacks_per_turn();
    return mayhem_get_attacks_used() < per_turn;
}

// 0x00761940 (:122)
void wrecking_crew_mode_mayhem_player_attacked(const weapon_info* w_info, damaged_by_types attack_type)
{
    Mayhem_attacks_used++;
    gameplay_play_sfx(GSFX_WRECKING_CREW_FIRED_WEAPON);
}

// 0x007912F0 (:129): the magazine holds at most the attacks left, the reserve the rest of them.
void wrecking_crew_mode_mayhem_get_weapon_ammo(int* magazine, int* reserve)
{
    if (Wrecking_crew_current_weapon == nullptr)
    {
        return;
    }
    const int left = mayhem_get_attacks_per_turn() - mayhem_get_attacks_used();
    if (left < *magazine)
    {
        *magazine = left;
    }
    *reserve = left - *magazine;
}

// 0x00791360 (:146): the attacks left, counting every projectile in flight.
void wrecking_crew_mode_mayhem_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    *override_icon = false;
    const int per_turn = mayhem_get_attacks_per_turn();
    _snprintf_safe(buffer, buffer_len, "%d", per_turn - mayhem_get_attacks_almost_used());
}

// 0x00791410 (:154): flash at a quarter of the attacks left or on the last one.
hud_wrecking_crew_icon_state wrecking_crew_mode_mayhem_get_weapon_icon_state()
{
    if (Wrecking_crew_state != WCS_PLAYERS_TURN)
    {
        return HWCIS_NORMAL;
    }
    const int per_turn = mayhem_get_attacks_per_turn();
    if ((float)(1.0 - (double)(float)mayhem_get_attacks_used() / (double)(float)per_turn) <= 0.25f || mayhem_get_attacks_used() + 1 == per_turn)
    {
        return HWCIS_FLASH;
    }
    return HWCIS_NORMAL;
}

// 0x00761960 (:171)
hud_wrecking_crew_icon_state wrecking_crew_mode_mayhem_get_time_icon_state()
{
    return HWCIS_NORMAL;
}
