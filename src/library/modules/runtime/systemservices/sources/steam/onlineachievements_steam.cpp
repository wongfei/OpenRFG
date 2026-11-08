// Original: library/modules/runtime/systemservices/sources/steam/onlineachievements_steam.cpp
//
// SteamAchievements (stub_impl.md S2): an achievement's progress is the Steam stat STAT_<id> (Steam unlocks the
// achievement from it); the platinum achievement is set once every other achievement is achieved.

#include "library/modules/runtime/systemservices/sources/steam/onlineachievements_steam.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

namespace keen
{
    // Magic number: 0xFFFFFFFF, no platinum achievement.
    static const uint32 NoPlatinumAchievement = 0xFFFFFFFFu;

    // 0x00C14D30 (:9)
    SteamAchievements::SteamAchievements()
    {
        m_achievementCount = 0;
        m_pAchievementUnlockCountMap = nullptr;
        m_pAchievementIdMap = nullptr;
        m_pSteamUserStats = nullptr;
    }

    // 0x00C14930
    SteamAchievements::~SteamAchievements()
    {
    }

    // 0x00C13830 (:18)
    void SteamAchievements::create(uint64 gameSteamID64, const uint32* pAchievementIdMap, const uint32* pAchievementUnlockCountMap, uint32 achievementCount, uint32 platinumAchievementId)
    {
        m_pSteamUserStats = SteamUserStats();
        m_gameSteamID64 = gameSteamID64;
        m_pAchievementIdMap = pAchievementIdMap;
        m_pAchievementUnlockCountMap = pAchievementUnlockCountMap;
        m_achievementCount = achievementCount;
        m_platinumAchievementId = platinumAchievementId;
        m_platinumAchievementUnlocked = false;
        checkPlatinumAchievementUnlocked();
    }

    // 0x00C12BD0 (:40)
    void SteamAchievements::destroy()
    {
        m_achievementCount = 0;
        m_pAchievementUnlockCountMap = nullptr;
        m_pAchievementIdMap = nullptr;
        m_pSteamUserStats = nullptr;
    }

    // 0x00C12BE0 (:49)
    bool SteamAchievements::resetAll()
    {
        return m_pSteamUserStats->ResetAllStats(true);
    }

    // 0x00C12BF0 (:56): the achievement's stat set to the status and stored
    bool SteamAchievements::updateAchievementStatus(uint32 achievementId, uint32 status)
    {
        // Magic number: "STAT_nnn" and its terminator.
        char statName[9];
        formatString(statName, sizeof(statName), "STAT_%03d", m_pAchievementIdMap[achievementId]);
        if (!m_pSteamUserStats->SetStat(statName, (int)status))
        {
            debug::nativePrintArgs("[online] Stat '%s' for achievement '%d' couldn't be set to %u.\n", statName, m_pAchievementIdMap[achievementId], status);
            return false;
        }
        if (!m_pSteamUserStats->StoreStats())
        {
            debug::nativePrintArgs("[online] Attempt to store changes for %d/'%s' failed.\n", m_pAchievementIdMap[achievementId], statName);
            return false;
        }
        return true;
    }

    // 0x00C12CC0 (:103)
    void SteamAchievements::onUserStatsStored(UserStatsStored_t* pParameters)
    {
        if (pParameters->m_nGameID != m_gameSteamID64 || pParameters->m_eResult == k_EResultOK)
        {
            return;
        }
        if (pParameters->m_eResult == k_EResultNoConnection)
        {
            debug::nativePrintArgs("[online] No connection in user stat update.\n");
        }
        else if (pParameters->m_eResult == k_EResultInvalidParam)
        {
            debug::nativePrintArgs("[online] Invalid parameters specified for user stat update.\n");
        }
        else
        {
            debug::nativePrintArgs("[online] Unhandled error in onUserStatsStored.\n");
        }
    }

    // 0x00C13880 (:130)
    void SteamAchievements::onUserAchievementStored(UserAchievementStored_t* pParameters)
    {
        if (pParameters->m_nGameID == m_gameSteamID64)
        {
            checkPlatinumAchievementUnlocked();
        }
    }

    // 0x00C12D20 (:146): every achievement but the platinum one (ACHI_<id>) achieved
    void SteamAchievements::checkPlatinumAchievementUnlocked()
    {
        if (m_platinumAchievementId == NoPlatinumAchievement || m_platinumAchievementUnlocked)
        {
            return;
        }
        // Magic number: room for the achievement's API name.
        char platinumName[128];
        formatString(platinumName, sizeof(platinumName), "ACHI_%03u", m_pAchievementIdMap[m_platinumAchievementId]);
        for (uint32 i = 0; i < m_achievementCount; ++i)
        {
            const uint32 steamAchievementIndex = m_pAchievementIdMap[i];
            bool achieved = false;
            if (m_pSteamUserStats->GetAchievementName(steamAchievementIndex) == nullptr)
            {
                return;
            }
            if (compareString(m_pSteamUserStats->GetAchievementName(steamAchievementIndex), platinumName) != 0)
            {
                if (!m_pSteamUserStats->GetAchievement(m_pSteamUserStats->GetAchievementName(steamAchievementIndex), &achieved) || !achieved)
                {
                    return;
                }
            }
        }
        // Magic number: 0x7FFFFFFF, the status of a completed achievement.
        updateAchievementStatus(m_platinumAchievementId, 0x7FFFFFFFu);
        m_platinumAchievementUnlocked = true;
    }
}
