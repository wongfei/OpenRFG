// Original: library/modules/runtime/useraccount/include/keen/useraccount/steam/useraccountsystem_steam.hpp
// Implementation: library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp
//
// The Steam user account system (UserAccountSteam: the signed-in Steam user). Placement of UserAccountSteam inferred.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

namespace keen
{
    struct InputSystem;
    struct LocalPlayerIdStructureType;
    class MemoryAllocator;
    struct UserAccount;
    struct UserAccountId;
    struct UserAccountSystem;
    struct UserAccountSystemCreationParameters;

    // size 0x10A
    struct UserAccountSteam
    {
        enum
        {
            UserNameLen = 256,
        };

        UserAccountSteam(); // 0x00CBAB60

        CSteamID steamId; // 0x00
        char userName[257]; // 0x08
        bool isConnected; // 0x109
    };

    namespace useraccount
    {
        UserAccountSystem* createUserAccountSystem(MemoryAllocator* pAllocator, const UserAccountSystemCreationParameters& userAccountSystemCreationParameters); // 0x00CBAFB0 (:38)
        void destroyUserAccountSystem(UserAccountSystem* pSystem, MemoryAllocator* pAllocator); // 0x00CBACE0 (:74)
        void updateUserAccountSystem(UserAccountSystem*); // 0x00CBAB80 (:82)
        uint32 getAccountCount(const UserAccountSystem* pSystem); // 0x00CBAB90 (:86)
        uint32 fillAccountList(UserAccount* pUserAccounts, uint32 capacity, const UserAccountSystem* pSystem); // 0x00CBAD00 (:91)
        bool isUserLoggedIn(const UserAccountSystem* pSystem, const UserAccountId& id); // 0x00CBADF0 (:108)
        uint32 showUserLoginSystemUi(UserAccountSystem* pSystem, InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId); // 0x00CBABA0 (:125)
        bool getUserAccount(UserAccount* pUserAccount, const UserAccountSystem* pSystem, const LocalPlayerIdStructureType* localPlayerId); // 0x00CBAEF0 (:134)
        bool getInitialUserAccount(UserAccount* pUserAccount, const UserAccountSystem* pSystem); // 0x00CBAF60 (:152)
    }
}
