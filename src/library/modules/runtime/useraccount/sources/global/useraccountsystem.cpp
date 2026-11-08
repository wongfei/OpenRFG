// Original: library/modules/runtime/useraccount/sources/global/useraccountsystem.cpp
//
// The platform independent half of the user account system (stub_impl.md S2): account id comparison and the one
// asynchronous operation slot the platform part reports through.

#include "keen/useraccount/useraccountsystem.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/useraccount/sources/global/useraccountsysteminternal.hpp"

#include <string.h>

namespace keen
{
    namespace useraccount
    {
        // Magic number: 1, the handle of "no operation" (there is one operation slot, handle 0).
        static const uint32 InvalidAsyncOperationHandle = 1u;

        // 0x00CBA8F0 (:27). Original behaviour kept: the parameters are copied twice, before and after the operation
        // slots are reset.
        void initializeUserAccountSystemBase(UserAccountSystemBase* pSystem, const UserAccountSystemCreationParameters& userAccountSystemCreationParameters)
        {
            pSystem->creationParameters = userAccountSystemCreationParameters;
            for (uint32 i = 0; i < sizeof(pSystem->runningOperations) / sizeof(pSystem->runningOperations[0]); ++i)
            {
                pSystem->runningOperations[i].state = UserAccountAsyncOperationState_Invalid;
            }
            pSystem->creationParameters = userAccountSystemCreationParameters;
        }

        // 0x00CBA920 (:41): two valid ids with the same provider data
        bool isUserAccountIdEqual(const UserAccountId& id0, const UserAccountId& id1)
        {
            if (!id0.isValid || !id1.isValid)
            {
                return false;
            }
            return memcmp(id0.providerData, id1.providerData, sizeof(id0.providerData)) == 0;
        }

        // 0x00CBA9C0 (:51)
        bool isUserAccountEqual(const UserAccount& account1, const UserAccount& account2)
        {
            if (account1.guestIndex != account2.guestIndex)
            {
                return false;
            }
            return isUserAccountIdEqual(account1.id, account2.id);
        }

        // 0x00CBA9F0 (:65): the free slot, else the invalid handle
        uint32 createAsyncOperationHandle(UserAccountSystemBase* pSystem, UserAccountAsyncOperationType type)
        {
            for (uint32 i = 0; i < sizeof(pSystem->runningOperations) / sizeof(pSystem->runningOperations[0]); ++i)
            {
                if (pSystem->runningOperations[i].state == UserAccountAsyncOperationState_Invalid)
                {
                    pSystem->runningOperations[i].state = UserAccountAsyncOperationState_Busy;
                    pSystem->runningOperations[i].type = type;
                    return i;
                }
            }
            return InvalidAsyncOperationHandle;
        }

        // 0x00CBAA30 (:80)
        void setAsyncOperationFinished(UserAccountSystemBase* pSystem, uint32 handle, const UserAccountAsyncOperationResultData& resultData, bool success)
        {
            UserAccountAsyncOperation& operation = pSystem->runningOperations[handle];
            if (operation.state != UserAccountAsyncOperationState_Busy)
            {
                debug::nativePrintArgs("[useraccount] Could not set operation because operation is already released.\n");
                return;
            }
            operation.state = success ? UserAccountAsyncOperationState_Success : UserAccountAsyncOperationState_Error;
            operation.result.type = operation.type;
            operation.result.resultData = resultData;
            operation.result.success = success;
        }

        // 0x00CBAA90 (:99)
        UserAccountAsyncOperationState getUserAccountOperationState(const UserAccountSystem* pSystem, uint32 handle)
        {
            if (handle == InvalidAsyncOperationHandle)
            {
                return UserAccountAsyncOperationState_Invalid;
            }
            return pSystem->runningOperations[handle].state;
        }

        // 0x00CBAAB0 (:116): the result, which releases the slot. Original behaviour kept: an unfinished operation is
        // only reported, its (stale) result returned and its slot released anyway.
        UserAccountAsyncOperationResult getUserAccountOperationResult(UserAccountSystem* pSystem, uint32 handle)
        {
            UserAccountAsyncOperation& operation = pSystem->runningOperations[handle];
            if (operation.state != UserAccountAsyncOperationState_Error && operation.state != UserAccountAsyncOperationState_Success)
            {
                debug::nativePrintArgs("[useraccount] Get result failed. Operation is in wrong state.\n");
            }
            const UserAccountAsyncOperationResult result = operation.result;
            operation.state = UserAccountAsyncOperationState_Invalid;
            return result;
        }
    }
}
