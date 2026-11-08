// SW_API.DLL: emgiec's Steamworks wrapper, the only way rfg.exe reaches Steam (it does not import steam_api.dll; the
// wrapper loads it). Not shipped as source; the declarations are written from rfg.exe's imports (stub_impl.md S2) and
// link against the import library of the shipped DLL (tools/dll_implib/dll_implib.py). Placement inferred.
//
// The SW_CCSys_* functions are the Steamworks SDK 1.42 flat API under other names; the inline SDK code that calls them
// (steam_api_internal.h, CSteamAPIContext::Init) shows which is which:
//     SW_CCSys_Init                 SteamAPI_Init
//     SW_CCSys_Shutdown             SteamAPI_Shutdown
//     SW_CCSys_ProcessApiCb         SteamAPI_RunCallbacks
//     SW_CCSys_InitCallbackFunc     SteamAPI_RegisterCallback
//     SW_CCSys_RemoveCallbackFunc   SteamAPI_UnregisterCallback
//     SW_CCSys_RegisterCallResult   SteamAPI_RegisterCallResult
//     SW_CCSys_UnregisterCallResult SteamAPI_UnregisterCallResult
//     SW_CCSys_GetUInterface        SteamAPI_GetHSteamUser
//     SW_CCSys_GetPInterface        SteamAPI_GetHSteamPipe
//     SW_CCSys_CreateInternalModule SteamInternal_CreateInterface
//     SW_CCSys_DynamicInit          SteamInternal_ContextInit
// All are __cdecl with C linkage (undecorated exports).
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

class CCallbackBase;

extern "C"
{
    // True when Steam started (keen::SteamworksSystem::create).
    bool SW_CCSys_Init();
    void SW_CCSys_Shutdown();
    // Runs the posted callbacks and finished call results (keen::GameFramework's frame).
    void SW_CCSys_ProcessApiCb();
    void SW_CCSys_InitCallbackFunc(CCallbackBase* pCallback, int iCallback);
    void SW_CCSys_RemoveCallbackFunc(CCallbackBase* pCallback);
    void SW_CCSys_RegisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall);
    void SW_CCSys_UnregisterCallResult(CCallbackBase* pCallback, SteamAPICall_t hAPICall);
    HSteamUser SW_CCSys_GetUInterface();
    HSteamPipe SW_CCSys_GetPInterface();
    void* SW_CCSys_CreateInternalModule(const char* pVersion);
    // Calls the context's init function (its first pointer) when Steam (re)started and returns the context
    // (SteamInternal_ModuleContext's static block, CSteamAPIContext after the function and the counter).
    void* SW_CCSys_DynamicInit(void* pContextInitData);

    // True when the Steam client offers achievements (SW_API.DLL::SW_HasAchievements).
    char SW_HasAchievements();
    // True when the Steam client offers invites (SW_API.DLL::SW_HasInvites; UI.md UI5, the player options box).
    char SW_HasInvites();
    // True when the Steam client offers leaderboards (SW_API.DLL::SW_HasLeaderboards).
    char SW_HasLeaderboards();
}
