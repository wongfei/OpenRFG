// Original: game/build/win32/steam/master/achievements/rfgachievements_adf.hpp
// Implementation: game/build/win32/steam/master/achievements/rfgachievements_adf.cpp
//
// Generated from the achievement definition file: AchievementId, the platform id maps and unlock values. Placement of
// AchievementId inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum AchievementId
    {
        AchievementId_red_faction_guerrilla = 0,
        AchievementId_welcoming_comitte = 1,
        AchievementId_martian_tea_party = 2,
        AchievementId_spread_the_word = 3,
        AchievementId_death_from_above = 4,
        AchievementId_friendly_skies = 5,
        AchievementId_dont_tread_on_me = 6,
        AchievementId_d_etat = 7,
        AchievementId_red_dawn = 8,
        AchievementId_insurgent = 9,
        AchievementId_guerrilla = 10,
        AchievementId_freedom_fighter = 11,
        AchievementId_revolutionary = 12,
        AchievementId_clean_and_righteous = 13,
        AchievementId_warp_speed = 14,
        AchievementId_got_any_fingers_left = 15,
        AchievementId_lost_memories = 16,
        AchievementId_working_the_land = 17,
        AchievementId_free_your_mind = 18,
        AchievementId_one_man_army = 19,
        AchievementId_disaster_area = 20,
        AchievementId_broken_supply_line = 21,
        AchievementId_power_to_the_people = 22,
        AchievementId_tank_buster = 23,
        AchievementId_best_friends_forever = 24,
        AchievementId_coming_down = 25,
        AchievementId_freed_space = 26,
        AchievementId_just_the_beginning = 27,
        AchievementId_start_of_something_special = 28,
        AchievementId_doing_your_part = 29,
        AchievementId_juggernaut = 30,
        AchievementId_doozer = 31,
        AchievementId_grab_some_popcorn = 32,
        AchievementId_try_anything_once = 33,
        AchievementId_check_your_map = 34,
        AchievementId_tools_of_the_trade = 35,
        AchievementId_field_tested = 36,
        AchievementId_battle_scarred = 37,
        AchievementId_war_veteran = 38,
        AchievementId_a_winner_is_you = 39,
        AchievementId_topher_would_be_proud = 40,
        AchievementId_courier_of_pain = 41,
        AchievementId_experimenter = 42,
        AchievementId_detective = 43,
        AchievementId_mad_genius = 44,
        AchievementId_jack_of_all_trades = 45,
        AchievementId_the_high_and_mighty = 46,
        AchievementId_party_time = 47,
        AchievementId_cant_get_enough = 48,
        AchievementId_wrecking_ball = 49,
        AchievementId_red_faction_member = 50,
        AchievementId_bound_by_blood = 51,
        AchievementId_family_vengeance = 52,
        AchievementId_a_greater_purpose = 53,
        AchievementId_deliverance_defender = 54,
        AchievementId_tumbling_down = 55,
        AchievementId_mobile_bombs = 56,
        AchievementId_structural_integrity = 57,
        AchievementId_purge_the_valley = 58,
        AchievementId_ares_bloodlust = 59,
        AchievementId_the_power_of_one = 60,
        AchievementId_Count = 61,
    };

    const uint32 getAchievementIdCount(); // 0x00CE6130 (:24)
    const uint32* getAchievementUnlockValueMap(); // 0x00CE6140 (:25)
    uint32 getPlatinumAchievementId(); // 0x00CE6150 (:26)
    const uint32* getAchievementIdMapDevonline(); // 0x00CE6160 (:54)
    const uint32* getAchievementIdMap(); // 0x00CE6170 (:62)
}
