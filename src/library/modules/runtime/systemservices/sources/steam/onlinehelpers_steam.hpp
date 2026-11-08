// Original: library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.hpp
// Implementation: library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.cpp
//
// Conversions between Steam ids and system service user ids.
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

class CSteamID;

namespace unit4
{
    struct RemoteUserId;

    namespace systemservices
    {
        const CSteamID& getInvalidCSteamID(); // 0x00C12E30 (:8)
        uint64_t getInvalidSteamID64(); // 0x00C12E40 (:12)
        RemoteUserId convertToOnlineSystemUserId(const CSteamID& userCSteamID, uint8 guestIndex); // 0x00C138C0 (:17)
        CSteamID convertToCSteamID(const RemoteUserId& userId); // 0x00C13900 (:29)
    }
}
