// Original: library/modules/runtime/systemservices/sources/steam/onlinestats_steam.cpp
//
// SteamStats (stub_impl.md S2): the game's hero stats as Steam user stats, by the stat update map's names. A stat's
// type (int or float) is learned from the first Set/Get Steam accepts. Additions before the stats arrived from Steam
// are cached and applied later; changed stats are stored at most every 5 seconds.

#include "library/modules/runtime/systemservices/sources/steam/onlinestats_steam.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

namespace keen
{
    // Magic number: the stats are requested and stored at most every 5 seconds.
    static const float StatsUpdateIntervalSeconds = 5.0f;

    // 0x00C14DD0 (:15)
    SteamStats::SteamStats()
    {
        m_statUpdateCount = 0;
        m_pStatUpdateMap = nullptr;
        m_pSteamUserStats = nullptr;
        m_requestingUserStats = false;
        m_statsAvailable = false;
        m_storingStats = false;
        m_hasUnstoredStats = false;
        m_hasCached = false;
        m_secondsSinceLastUpdate = 0.0f;
        for (uint32 i = 0; i < sizeof(m_cachedAdds) / sizeof(m_cachedAdds[0]); ++i)
        {
            m_cachedAdds[i] = 0.0;
        }
        for (uint32 i = 0; i < sizeof(m_statTypes) / sizeof(m_statTypes[0]); ++i)
        {
            m_statTypes[i] = SteamStatType_Unknown;
        }
    }

    // 0x00C149D0
    SteamStats::~SteamStats()
    {
    }

    // 0x00C13920 (:41)
    void SteamStats::create(uint64 gameSteamID64, const unit4::StatUpdateData* pStatUpdateMap, uint32 statUpdateCount)
    {
        m_pSteamUserStats = SteamUserStats();
        m_gameSteamID64 = gameSteamID64;
        m_statUpdateCount = statUpdateCount;
        m_pStatUpdateMap = pStatUpdateMap;
        if (m_pSteamUserStats->RequestCurrentStats())
        {
            m_requestingUserStats = true;
        }
    }

    // 0x00C12E50 (:59)
    void SteamStats::destroy()
    {
        m_statUpdateCount = 0;
        m_pStatUpdateMap = nullptr;
        m_pSteamUserStats = nullptr;
        m_requestingUserStats = false;
        m_statsAvailable = false;
        m_storingStats = false;
        m_hasUnstoredStats = false;
        m_hasCached = false;
        m_secondsSinceLastUpdate = 0.0f;
        for (uint32 i = 0; i < sizeof(m_cachedAdds) / sizeof(m_cachedAdds[0]); ++i)
        {
            m_cachedAdds[i] = 0.0;
        }
        for (uint32 i = 0; i < sizeof(m_statTypes) / sizeof(m_statTypes[0]); ++i)
        {
            m_statTypes[i] = SteamStatType_Unknown;
        }
    }

    // 0x00C13970 (:86): without stats, request them again (every 5 s); with them, apply the cached additions and
    // store the changes (every 5 s)
    void SteamStats::update(float timeStep)
    {
        m_secondsSinceLastUpdate += timeStep;
        if (!m_statsAvailable)
        {
            if (m_requestingUserStats || m_secondsSinceLastUpdate < StatsUpdateIntervalSeconds)
            {
                return;
            }
            if (m_pSteamUserStats->RequestCurrentStats())
            {
                m_requestingUserStats = true;
                m_secondsSinceLastUpdate = 0.0f;
                return;
            }
        }
        else
        {
            if (m_hasCached)
            {
                for (uint32 statId = 0; statId < m_statUpdateCount; ++statId)
                {
                    updateHeroStatAdd(statId, m_cachedAdds[statId]);
                    m_cachedAdds[statId] = 0.0;
                }
                m_hasCached = false;
            }
            if (!m_hasUnstoredStats || m_storingStats || m_secondsSinceLastUpdate < StatsUpdateIntervalSeconds)
            {
                return;
            }
            if (m_pSteamUserStats->StoreStats())
            {
                m_storingStats = true;
                m_hasUnstoredStats = false;
            }
        }
        m_secondsSinceLastUpdate = 0.0f;
    }

    // 0x00C12E90 (:133): as an int stat, else as a float stat
    bool SteamStats::updateHeroStatSet(const uint32 statId, double newValue)
    {
        if (m_statTypes[statId] == SteamStatType_Unknown || m_statTypes[statId] == SteamStatType_Int)
        {
            if (m_pSteamUserStats->SetStat(m_pStatUpdateMap[statId].statName, (int)newValue))
            {
                m_hasUnstoredStats = true;
                m_statTypes[statId] = SteamStatType_Int;
                return true;
            }
        }
        if (m_statTypes[statId] == SteamStatType_Unknown || m_statTypes[statId] == SteamStatType_Float)
        {
            if (m_pSteamUserStats->SetStat(m_pStatUpdateMap[statId].statName, (float)newValue))
            {
                m_hasUnstoredStats = true;
                m_statTypes[statId] = SteamStatType_Float;
                return true;
            }
        }
        return false;
    }

    // 0x00C12F50 (:153): read, add, set (as an int stat, else as a float stat); cached until the stats arrived
    bool SteamStats::updateHeroStatAdd(const uint32 statId, double addend)
    {
        if (!m_statsAvailable)
        {
            m_cachedAdds[statId] += addend;
            m_hasCached = true;
            m_hasUnstoredStats = true;
            m_statTypes[statId] = SteamStatType_Unknown;
            return true;
        }
        int intValue = 0;
        float floatValue = 0.0f;
        if (m_statTypes[statId] == SteamStatType_Unknown || m_statTypes[statId] == SteamStatType_Int)
        {
            if (m_pSteamUserStats->GetStat(m_pStatUpdateMap[statId].statName, &intValue))
            {
                intValue += (int)addend;
                if (!m_pSteamUserStats->SetStat(m_pStatUpdateMap[statId].statName, intValue))
                {
                    return false;
                }
                m_hasUnstoredStats = true;
                m_statTypes[statId] = SteamStatType_Int;
                return true;
            }
        }
        if (m_statTypes[statId] == SteamStatType_Unknown || m_statTypes[statId] == SteamStatType_Float)
        {
            if (m_pSteamUserStats->GetStat(m_pStatUpdateMap[statId].statName, &floatValue))
            {
                floatValue = (float)addend + floatValue;
                if (m_pSteamUserStats->SetStat(m_pStatUpdateMap[statId].statName, floatValue))
                {
                    m_hasUnstoredStats = true;
                    m_statTypes[statId] = SteamStatType_Float;
                    return true;
                }
            }
        }
        return false;
    }

    // 0x00C130B0 (:214): the stats only (not the achievements)
    bool SteamStats::resetAll()
    {
        return m_pSteamUserStats->ResetAllStats(false);
    }

    // 0x00C130C0 (:221)
    void SteamStats::onUserStatsReceived(UserStatsReceived_t* pParameters)
    {
        if (pParameters->m_nGameID != m_gameSteamID64)
        {
            return;
        }
        if (pParameters->m_eResult != k_EResultOK)
        {
            if (pParameters->m_eResult == k_EResultInvalidParam)
            {
                debug::nativePrintArgs("[online] Invalid parameters specified for user stat read.\n");
            }
            m_requestingUserStats = false;
            m_statsAvailable = false;
            return;
        }
        m_requestingUserStats = false;
        m_statsAvailable = true;
    }

    // 0x00C13110 (:250): a failed store is retried by the next update
    void SteamStats::onUserStatsStored(UserStatsStored_t* pParameters)
    {
        if (pParameters->m_nGameID != m_gameSteamID64)
        {
            return;
        }
        if (pParameters->m_eResult == k_EResultOK)
        {
            m_storingStats = false;
            return;
        }
        if (pParameters->m_eResult == k_EResultNoConnection)
        {
            debug::nativePrintArgs("[online] Stat update failed because no connection.\n");
        }
        else if (pParameters->m_eResult == k_EResultInvalidParam)
        {
            debug::nativePrintArgs("[online] Invalid parameters specified for user stat update.\n");
        }
        else
        {
            debug::nativePrintArgs("[online] Unhandled error in onUserStatsStored.\n");
        }
        m_storingStats = false;
        m_hasUnstoredStats = true;
    }
}
