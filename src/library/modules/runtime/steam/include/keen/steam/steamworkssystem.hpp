// Original: library/modules/runtime/steam/include/keen/steam/steamworkssystem.hpp
// Implementation: library/modules/runtime/steam/sources/global/steamworkssystem.cpp
//
// SteamworksSystem, initialization of the Steam API (app id check, callbacks pump) and its errors. Placement of
// SteamworksSystem inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/language.hpp"

namespace keen
{
    class MemoryAllocator;

    enum SteamError
    {
        SteamError_Ok = 0,
        SteamError_FailedInitialization = 1,
        SteamError_NotSubscribedToApp = 2,
    };

    // size 0x4
    class SteamworksSystem
    {
    public:
        SteamError create(uint32 appId); // 0x00402460 (:21)
        void destroy(); // 0x00402370 (:48)
        uint32 getAppId() const; // 0x00402380 (:51)

    private:
        uint32 m_appId; // 0x00
    };

    SteamworksSystem* createSteamworksSystem(MemoryAllocator* pAllocator, SteamError* pResult, uint32 appId); // 0x004024D0 (:57)
    bool isValidSteamworksSystem(SteamworksSystem* pSteamworksSystem); // 0x00402390 (:75)
    uint32 getAppId(SteamworksSystem* pSteamworksSystem); // 0x004023A0 (:80)
    LanguageId getCurrentSteamworksLanguageId(SteamworksSystem* pSteamworksSystem); // 0x00402520 (:87)
    void destroySteamworksSystem(MemoryAllocator* pAllocator, SteamworksSystem* pSteamworksSystem); // 0x00402570 (:164)
}
