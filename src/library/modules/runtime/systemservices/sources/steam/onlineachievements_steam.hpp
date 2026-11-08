// Original: library/modules/runtime/systemservices/sources/steam/onlineachievements_steam.hpp
// Implementation: library/modules/runtime/systemservices/sources/steam/onlineachievements_steam.cpp
//
// SteamAchievements, achievements through Steam user stats.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlineapicallbackinterfaces_steam.hpp"

class ISteamUserStats;
struct UserAchievementStored_t;
struct UserStatsStored_t;

namespace keen
{
    // size 0x68
    class SteamAchievements : public ISteamAchievementsCallback, public ISteamStatsCallback
    {
    public:
        SteamAchievements(); // 0x00C14D30 (:9)
        void create(uint64 gameSteamID64, const uint32* pAchievementIdMap, const uint32* pAchievementUnlockCountMap, uint32 achievementCount, uint32 platinumAchievementId); // 0x00C13830 (:18)
        void destroy(); // 0x00C12BD0 (:40)
        bool resetAll(); // 0x00C12BE0 (:49)
        bool updateAchievementStatus(uint32 achievementId, uint32 status); // 0x00C12BF0 (:56)
    private:
        virtual void onUserStatsStored(UserStatsStored_t* pParameters); // vtbl+0x08 0x00C12CC0 (:103)
        virtual void onUserAchievementStored(UserAchievementStored_t* pParameters); // vtbl+0x04 0x00C13880 (:130)
        void checkPlatinumAchievementUnlocked(); // 0x00C12D20 (:146)
    public:
        virtual ~SteamAchievements(); // vtbl+0x00 0x00C14930

    private:
        ISteamUserStats* m_pSteamUserStats; // 0x44
        uint64 m_gameSteamID64; // 0x48
        const uint32* m_pAchievementIdMap; // 0x50
        const uint32* m_pAchievementUnlockCountMap; // 0x54
        uint32 m_achievementCount; // 0x58
        uint32 m_platinumAchievementId; // 0x5C
        bool m_platinumAchievementUnlocked; // 0x60
    };
}
