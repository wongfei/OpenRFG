// Original: library/modules/runtime/useraccount/sources/global/useraccountsysteminternal.hpp
//
// UserAccountSystem, its platform independent base and asynchronous operations. Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/useraccount/steam/useraccountsystem_steam.hpp"
#include "keen/useraccount/useraccountsystem.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

namespace keen
{
    // size 0x18
    struct UserAccountAsyncOperation
    {
        UserAccountAsyncOperationType type; // 0x00
        UserAccountAsyncOperationState state; // 0x04
        UserAccountAsyncOperationResult result; // 0x08
    };

    // size 0x11C
    struct UserAccountSystemBase
    {
        UserAccountSystemBase(); // 0x00CBAB50

        UserAccountSystemCreationParameters creationParameters; // 0x00
        UserAccountAsyncOperation runningOperations[1]; // 0x104
    };

    // size 0x11C0
    struct UserAccountSystem : public UserAccountSystemBase
    {
        UserAccountSystem(); // 0x00CBAC50

        UserAccountSteam accounts[16]; // 0x11C
        uint32 accountCount; // 0x11BC
    };
}
