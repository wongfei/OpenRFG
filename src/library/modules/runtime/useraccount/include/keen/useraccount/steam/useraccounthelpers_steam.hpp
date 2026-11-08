// Original: library/modules/runtime/useraccount/include/keen/useraccount/steam/useraccounthelpers_steam.hpp
// Implementation: library/modules/runtime/useraccount/sources/steam/useraccounthelpers_steam.cpp
//
// Reading and writing user account ids (Steam).
#pragma once

#include "keen/base/types.hpp"

class CSteamID;

namespace keen
{
    struct UserAccountId;

    namespace useraccount
    {
        void writeUserAccountId(UserAccountId* pUserAccountId, CSteamID nativeUserId); // 0x00CBABE0 (:6)
        void readUserAccountId(CSteamID* pNativeUserId, const UserAccountId& userAccountId); // 0x00CBAB00 (:14)
    }
}
