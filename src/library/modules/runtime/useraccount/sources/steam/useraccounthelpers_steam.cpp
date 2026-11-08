// Original: library/modules/runtime/useraccount/sources/steam/useraccounthelpers_steam.cpp
//
// A Steam user account id is the 64-bit Steam id in the account id's provider data (stub_impl.md S2).

#include "keen/useraccount/steam/useraccounthelpers_steam.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

#include <string.h>

namespace keen
{
    namespace useraccount
    {
        // 0x00CBABE0 (:6)
        void writeUserAccountId(UserAccountId* pUserAccountId, CSteamID nativeUserId)
        {
            memset(pUserAccountId->providerData, 0, sizeof(pUserAccountId->providerData));
            memmove(pUserAccountId->providerData, &nativeUserId, sizeof(nativeUserId));
            pUserAccountId->isValid = true;
        }

        // 0x00CBAB00 (:14)
        void readUserAccountId(CSteamID* pNativeUserId, const UserAccountId& userAccountId)
        {
            memmove(pNativeUserId, userAccountId.providerData, sizeof(*pNativeUserId));
        }
    }
}
