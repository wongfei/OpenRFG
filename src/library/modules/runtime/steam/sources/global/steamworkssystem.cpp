// Original: library/modules/runtime/steam/sources/global/steamworkssystem.cpp
//
// SteamworksSystem: Steam's start-up (sw_api.dll's SteamAPI_Init), the app ownership check and the Steam UI language
// (stub_impl.md S2). Only GameFramework::initialize creates it, when the application asks for Steam (useSteam).

#include "keen/steam/steamworkssystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

namespace keen
{
    // 0x014F5068: Steam's language names by LanguageId; the regional variants share their language's name, and
    // En_En has none (Steam's "english" is En_Us).
    static const char* s_steamLanguageNames[LanguageId_Count] =
    {
        "", "english", "german", "french", "italian", "spanish", "polish", "russian", "japanese", "brazilian",
        "portuguese", "koreana", "czech",
        // Es_Ar ... Es_Ve
        "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish",
        "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish", "spanish",
        "schinese", "tchinese",
        // Ar_Eg ... Ar_Ye
        "arabic", "arabic", "arabic", "arabic", "arabic", "arabic", "arabic", "arabic", "arabic", "arabic",
        "arabic", "arabic", "arabic", "arabic", "arabic", "arabic",
    };

    // 0x00402460 (:21)
    SteamError SteamworksSystem::create(uint32 appId)
    {
        if (!SW_CCSys_Init())
        {
            debug::nativePrintArgs("Failed to initialize steamworks system...\n");
            return SteamError_FailedInitialization;
        }
        if (!SteamApps()->BIsSubscribedApp(appId))
        {
            debug::nativePrintArgs("You don't own this steam app %u.\n", appId);
            return SteamError_NotSubscribedToApp;
        }
        m_appId = appId;
        return SteamError_Ok;
    }

    // 0x00402370 (:48)
    void SteamworksSystem::destroy()
    {
        SW_CCSys_Shutdown();
    }

    // 0x00402380 (:51)
    uint32 SteamworksSystem::getAppId() const
    {
        return m_appId;
    }

    // 0x004024D0 (:57)
    SteamworksSystem* createSteamworksSystem(MemoryAllocator* pAllocator, SteamError* pResult, uint32 appId)
    {
        SteamworksSystem* pSystem = (SteamworksSystem*)pAllocator->allocate(sizeof(SteamworksSystem), 4u, 0u, nullptr);
        *pResult = pSystem->create(appId);
        if (*pResult == SteamError_Ok)
        {
            return pSystem;
        }
        if (pSystem != nullptr)
        {
            pAllocator->free(pSystem);
        }
        return nullptr;
    }

    // 0x00402390 (:75)
    bool isValidSteamworksSystem(SteamworksSystem* pSteamworksSystem)
    {
        return pSteamworksSystem != nullptr;
    }

    // 0x004023A0 (:80)
    uint32 getAppId(SteamworksSystem* pSteamworksSystem)
    {
        return pSteamworksSystem->getAppId();
    }

    // 0x00402520 (:87): the language of the Steam client's game language setting; En_En when Steam names another one
    LanguageId getCurrentSteamworksLanguageId(SteamworksSystem*)
    {
        const char* pLanguage = SteamApps()->GetCurrentGameLanguage();
        for (uint32 i = LanguageId_En_En; i < LanguageId_Count; ++i)
        {
            if (isStringEqualNoCase(pLanguage, s_steamLanguageNames[i]))
            {
                return (LanguageId)i;
            }
        }
        return LanguageId_En_En;
    }

    // 0x00402570 (:164)
    void destroySteamworksSystem(MemoryAllocator* pAllocator, SteamworksSystem* pSteamworksSystem)
    {
        pSteamworksSystem->destroy();
        if (pSteamworksSystem != nullptr)
        {
            pAllocator->free(pSteamworksSystem);
        }
    }
}
