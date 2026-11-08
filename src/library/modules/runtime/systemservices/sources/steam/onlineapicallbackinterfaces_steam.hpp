// Original: library/modules/runtime/systemservices/sources/steam/onlineapicallbackinterfaces_steam.hpp
//
// Callback interfaces for Steam stats and achievements results: the constructors register the Steam callbacks, which
// call the (virtual) handlers; the destructors unregister them (CCallback's destructor).
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuserstats.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

namespace keen
{
    // size 0x2C
    class ISteamStatsCallback
    {
    public:
        ISteamStatsCallback() // 0x00C14C60 (.h:22)
            : m_onUserStatsReceived(this, &ISteamStatsCallback::onUserStatsReceived), m_onUserStatsStored(this, &ISteamStatsCallback::onUserStatsStored)
        {
        }
        virtual ~ISteamStatsCallback() // vtbl+0x00 0x00C14800 (.h:25)
        {
        }
    private:
        virtual void onUserStatsReceived(UserStatsReceived_t*) // vtbl+0x04 0x00C14840 (.h:28)
        {
        }
        virtual void onUserStatsStored(UserStatsStored_t*) // vtbl+0x08 0x00C14850 (.h:29)
        {
        }

        // 0x00 vfptr
        CCallback<ISteamStatsCallback,UserStatsReceived_t,0> m_onUserStatsReceived; // 0x04
        CCallback<ISteamStatsCallback,UserStatsStored_t,0> m_onUserStatsStored; // 0x18
    };

    // size 0x18
    class ISteamAchievementsCallback
    {
    public:
        ISteamAchievementsCallback() // 0x00C14CE0 (.h:72)
            : m_onUserAchievementStored(this, &ISteamAchievementsCallback::onUserAchievementStored)
        {
        }
        virtual ~ISteamAchievementsCallback() // vtbl+0x00 0x00C148C0 (.h:75)
        {
        }
    private:
        virtual void onUserAchievementStored(UserAchievementStored_t*) // vtbl+0x04 0x00C148E0 (.h:78)
        {
        }

        // 0x00 vfptr
        CCallback<ISteamAchievementsCallback,UserAchievementStored_t,0> m_onUserAchievementStored; // 0x04
    };
}
