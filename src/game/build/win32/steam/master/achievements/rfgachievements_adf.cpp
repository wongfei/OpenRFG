// Original: game/build/win32/steam/master/achievements/rfgachievements_adf.cpp
//
// Generated from the achievement definition file (stub_impl.md S2): per AchievementId the progress that unlocks it and
// its id on each platform. The tables are rfg.exe's data (0x01348480, 0x01348578, 0x01348670), read from the image.

#include "game/build/win32/steam/master/achievements/rfgachievements_adf.hpp"

namespace keen
{
    // 0x01348480: the progress at which each achievement unlocks
    static const uint32 s_achievementUnlockCountMap[AchievementId_Count] =
    {
        1, 1, 2, 1, 1, 1, 1, 1, 1, 5,
        25, 50, 104, 5, 18, 16, 36, 300, 50, 25,
        1000000000, 250, 3, 100, 100, 50, 50, 1, 5, 10,
        1, 1, 1, 6, 21, 18, 1000, 10000, 100000, 250,
        250, 5000, 4, 8, 16, 100, 1, 4, 40, 25000000,
        1, 1, 1, 1, 11, 8, 100, 41, 1, 4,
        75,
    };

    // 0x01348578: the development online service's ids (the achievement ids themselves)
    static const uint32 s_achievementIdMapDevonline[AchievementId_Count] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
        60,
    };

    // 0x01348670: the Steam achievement indices (ACHI_<n>/STAT_<n>); the achievements Steam does not have share 1
    static const uint32 s_achievementIdMapSteam[AchievementId_Count] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35, 36, 37, 1, 1,
        38, 1, 39, 40, 41, 42, 43, 44, 45, 46,
        1, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        56,
    };

    // 0x00CE6130 (:24)
    const uint32 getAchievementIdCount()
    {
        return AchievementId_Count;
    }

    // 0x00CE6140 (:25)
    const uint32* getAchievementUnlockValueMap()
    {
        return s_achievementUnlockCountMap;
    }

    // 0x00CE6150 (:26)
    uint32 getPlatinumAchievementId()
    {
        return AchievementId_red_faction_guerrilla;
    }

    // 0x00CE6160 (:54)
    const uint32* getAchievementIdMapDevonline()
    {
        return s_achievementIdMapDevonline;
    }

    // 0x00CE6170 (:62)
    const uint32* getAchievementIdMap()
    {
        return s_achievementIdMapSteam;
    }
}
