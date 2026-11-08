// Original: library/modules/runtime/useraccount/include/keen/useraccount/useraccounttypes.hpp
//
// User account types: ids, names, accounts and the asynchronous operation results.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum UserAccountAsyncOperationState
    {
        UserAccountAsyncOperationState_Invalid = 0,
        UserAccountAsyncOperationState_Busy = 1,
        UserAccountAsyncOperationState_Error = 2,
        UserAccountAsyncOperationState_Success = 3,
    };

    enum UserAccountAsyncOperationType
    {
        UserAccountAsyncOperationType_Invalid = 0,
        UserAccountAsyncOperationType_ShowSelectUserUi = 1,
    };

    // size 0x9
    struct UserAccountId
    {
        uint8 providerData[8]; // 0x00
        bool isValid; // 0x08
    };

    // size 0x9
    struct UserAccountAsyncOperationShowSelectUiResult
    {
        UserAccountId userId; // 0x00
    };

    // size 0x101
    struct UserAccountName
    {
        char providerData[257]; // 0x00
    };

    // size 0x10C
    struct UserAccount
    {
        uint8 guestIndex; // 0x00
        bool isLoggedIn; // 0x01
        UserAccountId id; // 0x02
        UserAccountName name; // 0x0B
    };

    // size 0x9
    struct UserAccountAsyncOperationResultData
    {
        UserAccountAsyncOperationShowSelectUiResult showSelectUi; // 0x00
    };

    // size 0x10
    struct UserAccountAsyncOperationResult
    {
        UserAccountAsyncOperationType type; // 0x00
        UserAccountAsyncOperationResultData resultData; // 0x04
        bool success; // 0x0D
    };
}
