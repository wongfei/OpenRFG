// Original: library/modules/runtime/useraccount/include/keen/useraccount/useraccountsystem.hpp
// Implementation: library/modules/runtime/useraccount/sources/global/useraccountsystem.cpp
//
// The user account system API: creation, the signed-in accounts per local player, and asynchronous account operations.
// UserAccountSystem itself is defined in sources/global/useraccountsysteminternal.hpp.
#pragma once

#include "keen/base/types.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

namespace keen
{
    class SteamworksSystem;
    struct UserAccountSystem;
    struct UserAccountSystemBase;

    // size 0x104
    struct UserAccountSystemCreationParameters
    {
        UserAccountSystemCreationParameters() // 0x00C0E000 (useraccounttypes.hpp:101): nothing is initialized
        {
        }

        SteamworksSystem* pSteamworksSystem; // 0x00
        char userNameWin32[256]; // 0x04
    };

    namespace useraccount
    {
        void initializeUserAccountSystemBase(UserAccountSystemBase* pSystem, const UserAccountSystemCreationParameters& userAccountSystemCreationParameters); // 0x00CBA8F0 (:27)
        bool isUserAccountIdEqual(const UserAccountId& id0, const UserAccountId& id1); // 0x00CBA920 (:41)
        bool isUserAccountEqual(const UserAccount& account1, const UserAccount& account2); // 0x00CBA9C0 (:51)
        uint32 createAsyncOperationHandle(UserAccountSystemBase* pSystem, UserAccountAsyncOperationType type); // 0x00CBA9F0 (:65)
        void setAsyncOperationFinished(UserAccountSystemBase* pSystem, uint32 handle, const UserAccountAsyncOperationResultData& resultData, bool success); // 0x00CBAA30 (:80)
        UserAccountAsyncOperationState getUserAccountOperationState(const UserAccountSystem* pSystem, uint32 handle); // 0x00CBAA90 (:99)
        UserAccountAsyncOperationResult getUserAccountOperationResult(UserAccountSystem* pSystem, uint32 handle); // 0x00CBAAB0 (:116)
    }
}
