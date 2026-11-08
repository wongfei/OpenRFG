// Original: library/modules/runtime/systemservices/sources/steam/onlinestats_steam.hpp
// Implementation: library/modules/runtime/systemservices/sources/steam/onlinestats_steam.cpp
//
// SteamStats, stats through Steam user stats (SteamStatType). Placement of the stat type inferred.
#pragma once

#include "keen/base/types.hpp"
#include "unit4/systemservices/systemservices.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlineapicallbackinterfaces_steam.hpp"

class ISteamUserStats;
struct UserStatsReceived_t;
struct UserStatsStored_t;

namespace keen
{
    enum SteamStatType
    {
        SteamStatType_Unknown = -1,
        SteamStatType_Int = 0,
        SteamStatType_Float = 1,
        SteamStatType_Count = 2,
    };

    // size 0x350
    class SteamStats : public ISteamStatsCallback
    {
    public:
        SteamStats(); // 0x00C14DD0 (:15)
        void create(uint64 gameSteamID64, const unit4::StatUpdateData* pStatUpdateMap, uint32 statUpdateCount); // 0x00C13920 (:41)
        void destroy(); // 0x00C12E50 (:59)
        void update(float timeStep); // 0x00C13970 (:86)
        bool updateHeroStatSet(const uint32 statId, double newValue); // 0x00C12E90 (:133)
        bool updateHeroStatAdd(const uint32 statId, double addend); // 0x00C12F50 (:153)
        bool resetAll(); // 0x00C130B0 (:214)
    private:
        virtual void onUserStatsReceived(UserStatsReceived_t* pParameters); // vtbl+0x04 0x00C130C0 (:221)
        virtual void onUserStatsStored(UserStatsStored_t* pParameters); // vtbl+0x08 0x00C13110 (:250)
    public:
        virtual ~SteamStats(); // vtbl+0x00 0x00C149D0

    private:
        ISteamUserStats* m_pSteamUserStats; // 0x2C
        uint64 m_gameSteamID64; // 0x30
        const unit4::StatUpdateData* m_pStatUpdateMap; // 0x38
        uint32 m_statUpdateCount; // 0x3C
        bool m_requestingUserStats; // 0x40
        bool m_statsAvailable; // 0x41
        bool m_storingStats; // 0x42
        bool m_hasUnstoredStats; // 0x43
        float m_secondsSinceLastUpdate; // 0x44
        bool m_hasCached; // 0x48
        double m_cachedAdds[64]; // 0x50
        SteamStatType m_statTypes[64]; // 0x250
    };
}
