// Original: library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp
//
// The Steam user account system (stub_impl.md S2): one account, the Steam user the client is logged in as (its Steam
// id and persona name). Without a Steamworks system (no Steam) nothing is created and the framework has no user
// account system.
//
// Deviation (stub_impl.md S2, OpenRFG with --no-steam): rfg.exe exits when Steam does not start, so it never calls
// these without a system. OpenRFG runs without Steam with --no-steam (application.cpp deviation 1); the queries that
// take the (then null) system answer "no account" at their first line, as the stand-ins did.

#include "keen/useraccount/steam/useraccountsystem_steam.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "keen/useraccount/steam/useraccounthelpers_steam.hpp"
#include "library/modules/runtime/useraccount/sources/global/useraccountsysteminternal.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <new>
#include <string.h>

namespace keen
{
    // The implicit constructors (0x00CBAB60, 0x00CBAB50, 0x00CBAC50): the Steam ids cleared.
    UserAccountSteam::UserAccountSteam()
    {
    }

    UserAccountSystemBase::UserAccountSystemBase()
    {
    }

    UserAccountSystem::UserAccountSystem()
    {
    }

    namespace useraccount
    {
        // Local player 0 is the Steam user (the handle is the player index, keen's LocalPlayerIdStructureType*).
        static uint8 getLocalPlayerIndex(const LocalPlayerIdStructureType* playerId)
        {
            return (uint8)(uintptr_t)playerId;
        }

        // 0x00CBAFB0 (:38)
        UserAccountSystem* createUserAccountSystem(MemoryAllocator* pAllocator, const UserAccountSystemCreationParameters& userAccountSystemCreationParameters)
        {
            if (!isValidSteamworksSystem(userAccountSystemCreationParameters.pSteamworksSystem))
            {
                debug::nativePrintArgs("[useraccount] Steam useraccountsystem couldn't be created because steamworks system is not valid.\n");
                return nullptr;
            }
            const CSteamID steamId = SteamUser()->GetSteamID();
            if (!steamId.IsValid())
            {
                debug::nativePrintArgs("[useraccount] Invalid SteamID.\n");
                return nullptr;
            }
            void* pMemory = pAllocator->allocate(sizeof(UserAccountSystem), 4u, 0u, nullptr);
            UserAccountSystem* pSystem = pMemory != nullptr ? new (pMemory) UserAccountSystem() : nullptr;
            initializeUserAccountSystemBase(pSystem, userAccountSystemCreationParameters);
            pSystem->accountCount = 0;
            for (uint32 i = 0; i < sizeof(pSystem->accounts) / sizeof(pSystem->accounts[0]); ++i)
            {
                pSystem->accounts[i].isConnected = false;
            }
            UserAccountSteam& account = pSystem->accounts[0];
            copyString(account.userName, sizeof(account.userName), SteamFriends()->GetPersonaName());
            account.steamId = steamId;
            account.isConnected = true;
            pSystem->accountCount = 1;
            return pSystem;
        }

        // 0x00CBACE0 (:74)
        void destroyUserAccountSystem(UserAccountSystem* pSystem, MemoryAllocator* pAllocator)
        {
            if (pSystem != nullptr)
            {
                pAllocator->free(pSystem);
            }
        }

        // 0x00CBAB80 (:82)
        void updateUserAccountSystem(UserAccountSystem*)
        {
        }

        // 0x00CBAB90 (:86)
        uint32 getAccountCount(const UserAccountSystem* pSystem)
        {
            if (pSystem == nullptr) // deviation: no Steam
            {
                return 0;
            }
            return pSystem->accountCount;
        }

        // 0x00CBAD00 (:91)
        uint32 fillAccountList(UserAccount* pUserAccounts, uint32 capacity, const UserAccountSystem* pSystem)
        {
            if (pSystem == nullptr) // deviation: no Steam
            {
                return 0;
            }
            const uint32 count = pSystem->accountCount < capacity ? pSystem->accountCount : capacity;
            for (uint32 i = 0; i < count; ++i)
            {
                writeUserAccountId(&pUserAccounts[i].id, pSystem->accounts[i].steamId);
                copyString(pUserAccounts[i].name.providerData, sizeof(pUserAccounts[i].name.providerData), pSystem->accounts[i].userName);
                pUserAccounts[i].guestIndex = 0;
                pUserAccounts[i].isLoggedIn = pSystem->accounts[i].isConnected;
            }
            return count;
        }

        // 0x00CBADF0 (:108): the account is connected and Steam is logged on
        bool isUserLoggedIn(const UserAccountSystem* pSystem, const UserAccountId& id)
        {
            if (pSystem == nullptr) // deviation: no Steam
            {
                return false;
            }
            CSteamID steamId;
            readUserAccountId(&steamId, id);
            for (uint32 i = 0; i < pSystem->accountCount; ++i)
            {
                if (pSystem->accounts[i].steamId == steamId)
                {
                    return pSystem->accounts[i].isConnected && SteamUser()->BLoggedOn();
                }
            }
            debug::nativePrintArgs("[useraccount] Could not check isUserLoggedIn(), because account with id %u could not be found\n", steamId.GetAccountID());
            return false;
        }

        // 0x00CBABA0 (:125): Steam has no login UI (the invalid operation handle)
        uint32 showUserLoginSystemUi(UserAccountSystem*, InputSystem*, const LocalPlayerIdStructureType*)
        {
            return 1u;
        }

        // 0x00CBAEF0 (:134): the Steam user for local player 0
        bool getUserAccount(UserAccount* pUserAccount, const UserAccountSystem* pSystem, const LocalPlayerIdStructureType* localPlayerId)
        {
            if (pSystem == nullptr) // deviation: no Steam
            {
                return false;
            }
            if (getLocalPlayerIndex(localPlayerId) != 0 || pSystem->accountCount == 0)
            {
                return false;
            }
            if (pUserAccount != nullptr)
            {
                writeUserAccountId(&pUserAccount->id, pSystem->accounts[0].steamId);
                copyString(pUserAccount->name.providerData, sizeof(pUserAccount->name.providerData), pSystem->accounts[0].userName);
                pUserAccount->guestIndex = 0;
                pUserAccount->isLoggedIn = pSystem->accounts[0].isConnected;
            }
            return true;
        }

        // 0x00CBAF60 (:152)
        bool getInitialUserAccount(UserAccount* pUserAccount, const UserAccountSystem* pSystem)
        {
            if (pSystem == nullptr) // deviation: no Steam
            {
                return false;
            }
            if (pUserAccount != nullptr)
            {
                writeUserAccountId(&pUserAccount->id, pSystem->accounts[0].steamId);
                copyString(pUserAccount->name.providerData, sizeof(pUserAccount->name.providerData), pSystem->accounts[0].userName);
                pUserAccount->guestIndex = 0;
                pUserAccount->isLoggedIn = pSystem->accounts[0].isConnected;
            }
            return true;
        }
    }
}
