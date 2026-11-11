// Stand-in for SW_API.DLL (sw_api.h), new code: built instead of linking lib/win32/sw_api.lib when CMake's
// OPENRFG_WITH_CUSTOM_SWAPI is on. Steam never starts: SW_CCSys_Init fails, there is no user or pipe, no interface is
// created, callbacks and call results are accepted and never run, and the SW_Has* feature queries say no.
//
// With it, GameFramework::initialize gives up when the application asks for Steam (as rfg.exe does when Steam does not
// start), so OpenRFG has to run with --no-steam. The program needs neither sw_api.dll nor steam_api.dll next to it.
//
// Only the exports rfg.exe imports are provided (sw_api.h); the DLL's LZ4_* and the other SW_* exports are not used.
#include "library/modules/thirdparty/sw_api/sw_api.h"

#include <stdint.h>

namespace
{
    // SteamInternal_ContextInit's counter: a module context whose counter differs is (re)initialized. Starts at 1 so
    // the first call clears the context (counter 0 in SteamInternal_ModuleContext's static block); bumped on shutdown
    // like the SDK's, which would clear the contexts again.
    uintptr_t s_contextCounter = 1;

    // SW_CCSys_DynamicInit's argument (steam_api_internal.h, SteamInternal_ModuleContext):
    // { void (*pFn)(void* pCtx); uintptr_t counter; CSteamAPIContext ctx; }.
    struct ContextInitData
    {
        void (*pFn)(void* pCtx);
        uintptr_t counter;
        // CSteamAPIContext follows.
    };
}

extern "C"
{
    bool SW_CCSys_Init()
    {
        return false;
    }

    void SW_CCSys_Shutdown()
    {
        ++s_contextCounter;
    }

    void SW_CCSys_ProcessApiCb()
    {
    }

    // The SDK sets k_ECallbackFlagsRegistered here; left clear, so the callbacks' destructors do not unregister.
    void SW_CCSys_InitCallbackFunc(CCallbackBase* pCallback, int iCallback)
    {
        (void)pCallback;
        (void)iCallback;
    }

    void SW_CCSys_RemoveCallbackFunc(CCallbackBase* pCallback)
    {
        (void)pCallback;
    }

    void SW_CCSys_RegisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall)
    {
        (void)pCallback;
        (void)hAPICall;
    }

    void SW_CCSys_UnregisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall)
    {
        (void)pCallback;
        (void)hAPICall;
    }

    // 0: no user and no pipe, so CSteamAPIContext::Init (SteamInternal_OnContextInit) leaves every interface nullptr.
    HSteamUser SW_CCSys_GetUInterface()
    {
        return 0;
    }

    HSteamPipe SW_CCSys_GetPInterface()
    {
        return 0;
    }

    void* SW_CCSys_CreateInternalModule(const char* pVersion)
    {
        (void)pVersion;
        return nullptr;
    }

    // SteamInternal_ContextInit: runs the context's init function when the counter moved on, returns the context.
    void* SW_CCSys_DynamicInit(void* pContextInitData)
    {
        ContextInitData* pData = (ContextInitData*)pContextInitData;
        void* pContext = pData + 1;
        if (pData->counter != s_contextCounter)
        {
            pData->pFn(pContext);
            pData->counter = s_contextCounter;
        }
        return pContext;
    }

    char SW_HasAchievements()
    {
        return 0;
    }

    char SW_HasInvites()
    {
        return 0;
    }

    char SW_HasLeaderboards()
    {
        return 0;
    }
}
