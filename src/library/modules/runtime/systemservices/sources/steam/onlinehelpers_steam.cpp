// Original: library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.cpp
//
// Conversions between Steam ids and the system services' remote user ids (stub_impl.md S2): the provider data is the
// 64-bit Steam id.

#include "library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "unit4/systemservices/systemservices.hpp"

#include <string.h>

namespace unit4
{
    namespace systemservices
    {
        // 0x038B70D4: the nil Steam id
        static CSteamID s_invalidCSteamID;

        // 0x00C12E30 (:8)
        const CSteamID& getInvalidCSteamID()
        {
            return s_invalidCSteamID;
        }

        // 0x00C12E40 (:12)
        uint64_t getInvalidSteamID64()
        {
            return s_invalidCSteamID.ConvertToUint64();
        }

        // 0x00C138C0 (:17)
        RemoteUserId convertToOnlineSystemUserId(const CSteamID& userCSteamID, uint8 guestIndex)
        {
            RemoteUserId id;
            memset(&id, 0, sizeof(id));
            memcpy(id.providerData, &userCSteamID, sizeof(id.providerData));
            id.isValid = userCSteamID.IsValid();
            id.guestIndex = guestIndex;
            return id;
        }

        // 0x00C13900 (:29)
        CSteamID convertToCSteamID(const RemoteUserId& userId)
        {
            CSteamID steamId;
            memcpy(&steamId, userId.providerData, sizeof(steamId));
            return steamId;
        }
    }
}
