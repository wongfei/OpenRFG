// Original: volition/rfg/code/nvidia_highlights.cpp
//
// NVIDIA ShadowPlay highlights through the GeForce Experience SDK (GfeSDK.dll, stub_impl.md S2): the SDK is created at
// start-up with the highlights scopes; once the user granted them, the five highlights are configured and the game
// saves kill/death videos and screenshots into the "gameplay" group, whose summary the menus open. Without GeForce
// Experience NVGSDK_Create fails and every call does nothing. The SDK's callbacks run from NVGSDK_Poll (polled
// callbacks); one recursive mutex guards the state. Notes (decompiled behaviour, kept as is): the functions that hold
// the mutex while calling has_permission or has_all_permissions lock it a second time (recursively) through them.

#include "volition/rfg/code/nvidia_highlights.h"
#include "keen/base/mutex.hpp"
#include "keen/base/systemtimer.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/highlights/ihighlights.h"
#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/isdk.h"
#include "library/modules/thirdparty/nvidiahighlights/include/gfesdk/sdk_types_ipc.h"

#include <string.h>

enum NvidiaHighlightsState
{
    NvidiaHighlightsState_NotInitialized = 0,
    NvidiaHighlightsState_Initialized = 1,
};

// Magic numbers: room for 64 scope permissions and 4 requested scopes.
static const unsigned int Max_scope_permissions = 64;
static const unsigned int Max_requested_scopes = 4;

static keen::Mutex s_nvidiaHighlightsMutex; // 0x01D5D4C8
static NvidiaHighlightsState s_state; // 0x01D59C34
static NVGSDK_HANDLE* s_pNvidiaSDKHandle; // 0x01D59C38
static NVGSDK_CreateResponse s_nvidiaCreateResponse;
static NVGSDK_ScopePermission s_scopePermissions[Max_scope_permissions];
static unsigned int s_numScopePermissions;
static NVGSDK_Scope s_currentRequestedScopes[Max_requested_scopes];
static unsigned int s_numCurrentRequestedScopes;
static unsigned int s_numHighlights;
static bool s_requestNumHighlightsInProgress;
static unsigned int s_lastTimeNumHighlightsRequested;
static bool s_groupOpen;

// 0x0163F0F4 ...: the highlights' English names
static NVGSDK_LocalizedPair s_highlightNameKillingSpree[] = { { "en-us", "Killing Spree" } };
static NVGSDK_LocalizedPair s_highlightNameKill[] = { { "en-us", "Kill" } };
static NVGSDK_LocalizedPair s_highlightNameDeath[] = { { "en-us", "Death" } };
static NVGSDK_LocalizedPair s_highlightNameDestruction[] = { { "en-us", "Destruction" } };
static NVGSDK_LocalizedPair s_highlightNameExplosion[] = { { "en-us", "Explosion" } };

// 0x01640FC8: the ids come from the header's statics (a dynamic initializer, as in rfg.exe)
static NVGSDK_Highlight s_highlightDefinitions[] =
{
    { s_highlightIdKillingSpree, true, NVGSDK_HIGHLIGHT_TYPE_INCIDENT, NVGSDK_HIGHLIGHT_SIGNIFICANCE_VERY_GOOD, s_highlightNameKillingSpree, 1 },
    { s_highlightIdKill, true, NVGSDK_HIGHLIGHT_TYPE_UNANNOUNCED, NVGSDK_HIGHLIGHT_SIGNIFICANCE_GOOD, s_highlightNameKill, 1 },
    { s_highlightIdDeath, true, NVGSDK_HIGHLIGHT_TYPE_STATE_CHANGE, NVGSDK_HIGHLIGHT_SIGNIFICANCE_VERY_BAD, s_highlightNameDeath, 1 },
    { s_highlightIdDestruction, true, NVGSDK_HIGHLIGHT_TYPE_UNANNOUNCED, NVGSDK_HIGHLIGHT_SIGNIFICANCE_GOOD, s_highlightNameDestruction, 1 },
    { s_highlightIdExplosion, true, NVGSDK_HIGHLIGHT_TYPE_UNANNOUNCED, NVGSDK_HIGHLIGHT_SIGNIFICANCE_GOOD, s_highlightNameExplosion, 1 },
};

bool nvidia_highlights_has_all_permissions();

// 0x005D1200 (:42)
static void __stdcall nvidia_highlights_get_num_highlights_callback(NVGSDK_RetCode ret, const NVGSDK_Highlights_NumberOfHighlights* pNumHighlights, void*)
{
    s_nvidiaHighlightsMutex.lock(0);
    s_requestNumHighlightsInProgress = false;
    s_numHighlights = ret >= 0 && pNumHighlights != nullptr ? pNumHighlights->numberOfHighlights : 0;
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D1250 (:64): each scope's permission updated, new scopes appended
static void nvidia_highlights_handle_permissions_changed(NVGSDK_ScopePermission* scopePermissionTable, unsigned int scopePermissionTableSize)
{
    s_nvidiaHighlightsMutex.lock(0);
    for (unsigned int i = 0; i < scopePermissionTableSize; ++i)
    {
        bool found = false;
        for (unsigned int j = 0; j < s_numScopePermissions; ++j)
        {
            if (scopePermissionTable[i].scope == s_scopePermissions[j].scope)
            {
                s_scopePermissions[j].permission = scopePermissionTable[i].permission;
                found = true;
            }
        }
        if (!found && s_numScopePermissions < Max_scope_permissions)
        {
            s_scopePermissions[s_numScopePermissions] = scopePermissionTable[i];
            s_numScopePermissions++;
        }
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D12E0 (:87)
static void __stdcall nvidia_highlights_notification_callback(NVGSDK_NotificationType type, const NVGSDK_Notification* pResponse, void*)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (type == NVGSDK_NOTIFICATION_PERMISSIONS_CHANGED)
    {
        nvidia_highlights_handle_permissions_changed(pResponse->permissionsChanged.scopePermissionTable, pResponse->permissionsChanged.scopePermissionTableSize);
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D1320 (:106)
static void __stdcall nvidia_empty_callback(NVGSDK_RetCode ret, void*)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (ret < 0)
    {
        keen::debug::nativePrintArgs("[nvidia_highlights] nvidia_empty_callback: ret: %x , %s\n", ret, NVGSDK_RetCodeToString(ret));
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D4010 (:116): the permissions the user granted configure the highlights
static void __stdcall nvidia_permission_request_callback(NVGSDK_RetCode ret, void*)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (ret < 0)
    {
        keen::debug::nativePrintArgs("[nvidia_highlights] nvidia_permission_request_callback: ret: %x , %s\n", ret, NVGSDK_RetCodeToString(ret));
    }
    else
    {
        nvidia_highlights_configure();
    }
    s_numCurrentRequestedScopes = 0;
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D4060 (:131): the SDK with the three highlights scopes; false without GeForce Experience
bool nvidia_highlights_init()
{
    s_nvidiaHighlightsMutex.create("NvidiaHighlightsMutex");
    NVGSDK_Scope scopes[3] = { NVGSDK_SCOPE_HIGHLIGHTS, NVGSDK_SCOPE_HIGHLIGHTS_VIDEO, NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT };
    NVGSDK_ScopePermission scopePermissions[3];
    NVGSDK_CreateInputParams params;
    memset(&params, 0, sizeof(params));
    params.appName = "Red Faction Guerrilla Re-Mars-tered";
    params.scopeTable = scopes;
    params.scopeTableSize = 3;
    params.pollForCallbacks = true;
    params.notificationCallback = nvidia_highlights_notification_callback;
    memset(&s_nvidiaCreateResponse, 0, sizeof(s_nvidiaCreateResponse));
    // Original behaviour kept: the response keeps pointing at this stack table after the call.
    s_nvidiaCreateResponse.scopePermissionTable = scopePermissions;
    s_nvidiaCreateResponse.scopePermissionTableSize = 3;
    if (NVGSDK_Create(&s_pNvidiaSDKHandle, &params, &s_nvidiaCreateResponse) < 0)
    {
        return false;
    }
    nvidia_highlights_handle_permissions_changed(s_nvidiaCreateResponse.scopePermissionTable, s_nvidiaCreateResponse.scopePermissionTableSize);
    s_state = NvidiaHighlightsState_Initialized;
    if (nvidia_highlights_has_all_permissions())
    {
        nvidia_highlights_configure();
    }
    return true;
}

// 0x005D1360 (:177)
void nvidia_highlights_shutdown()
{
    s_nvidiaHighlightsMutex.lock(0);
    if (s_state != NvidiaHighlightsState_NotInitialized)
    {
        NVGSDK_Release(s_pNvidiaSDKHandle);
        s_state = NvidiaHighlightsState_NotInitialized;
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D70D0 (:201): the SDK's callbacks, and the highlight count asked for every 10 seconds
void nvidia_highlights_poll()
{
    s_nvidiaHighlightsMutex.lock(0);
    if (s_state != NvidiaHighlightsState_NotInitialized)
    {
        const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
        NVGSDK_Poll(s_pNvidiaSDKHandle);
        // Magic number: 10000 ms between the count requests.
        if (now - s_lastTimeNumHighlightsRequested > 10000u)
        {
            nvidia_highlights_request_num_highlights(s_groupIdGameplay);
            s_lastTimeNumHighlightsRequested = now;
        }
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D4170 (:251): the scopes that must be asked for, once at a time
void nvidia_highlights_ask_user_for_permission()
{
    s_nvidiaHighlightsMutex.lock(0);
    if (s_state != NvidiaHighlightsState_NotInitialized && s_numCurrentRequestedScopes == 0)
    {
        const unsigned int count = s_numScopePermissions;
        for (unsigned int i = 0; i < count; ++i)
        {
            if (s_scopePermissions[i].permission == NVGSDK_PERMISSION_MUST_ASK && s_numCurrentRequestedScopes < Max_requested_scopes)
            {
                s_currentRequestedScopes[s_numCurrentRequestedScopes] = s_scopePermissions[i].scope;
                s_numCurrentRequestedScopes++;
            }
        }
        if (count != 0 && s_numCurrentRequestedScopes != 0)
        {
            NVGSDK_RequestPermissionsParams params;
            params.scopeTable = s_currentRequestedScopes;
            params.scopeTableSize = s_numCurrentRequestedScopes;
            NVGSDK_RequestPermissionsAsync(s_pNvidiaSDKHandle, &params, nvidia_permission_request_callback, nullptr);
        }
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D13B0 (:280)
bool nvidia_highlights_is_enabled()
{
    s_nvidiaHighlightsMutex.lock(0);
    const bool enabled = s_state != NvidiaHighlightsState_NotInitialized && s_pNvidiaSDKHandle != nullptr;
    s_nvidiaHighlightsMutex.unlock();
    return enabled;
}

// 0x005D13F0 (:287)
unsigned int nvidia_highlights_get_num_highlights()
{
    s_nvidiaHighlightsMutex.lock(0);
    const unsigned int count = s_numHighlights;
    s_nvidiaHighlightsMutex.unlock();
    return count;
}

// 0x005D1420 (:294)
bool nvidia_highlights_has_permission(NVGSDK_Scope scope)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_is_enabled())
    {
        for (unsigned int i = 0; i < s_numScopePermissions; ++i)
        {
            if (s_scopePermissions[i].scope == scope && s_scopePermissions[i].permission == NVGSDK_PERMISSION_GRANTED)
            {
                s_nvidiaHighlightsMutex.unlock();
                return true;
            }
        }
    }
    s_nvidiaHighlightsMutex.unlock();
    return false;
}

// 0x005D14B0 (:313)
bool nvidia_highlights_has_all_permissions()
{
    s_nvidiaHighlightsMutex.lock(0);
    const bool granted = nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS) && nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS_VIDEO) &&
                         nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT);
    s_nvidiaHighlightsMutex.unlock();
    return granted;
}

// 0x005D1510 (:391)
void nvidia_highlights_configure()
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS))
    {
        NVGSDK_HighlightConfigParams params;
        params.highlightDefinitionTable = s_highlightDefinitions;
        params.highlightTableSize = sizeof(s_highlightDefinitions) / sizeof(s_highlightDefinitions[0]);
        params.defaultLocale = "en-us";
        NVGSDK_Highlights_ConfigureAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D1570 (:408)
void nvidia_highlights_open_group(const char* groupId)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS) && !s_groupOpen)
    {
        NVGSDK_HighlightOpenGroupParams params;
        params.groupId = groupId;
        params.groupDescriptionTable = nullptr;
        params.groupDescriptionTableSize = 0;
        NVGSDK_Highlights_OpenGroupAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
        s_groupOpen = true;
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D15E0 (:427)
void nvidia_highlights_close_group(const char* groupId, bool destroy)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS) && s_groupOpen)
    {
        NVGSDK_HighlightCloseGroupParams params;
        memset(&params, 0, sizeof(params));
        params.groupId = groupId;
        params.destroyHighlights = destroy;
        NVGSDK_Highlights_CloseGroupAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
        s_groupOpen = false;
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D1650 (:447)
void nvidia_highlights_save_screenshot(const char* highlightId, const char* groupId)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT) && nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS))
    {
        NVGSDK_ScreenshotHighlightParams params;
        params.groupId = groupId;
        params.highlightId = highlightId;
        NVGSDK_Highlights_SetScreenshotHighlightAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D16C0 (:463): the video from startDelta to endDelta ms around now
void nvidia_highlights_save_video(const char* highlightId, const char* groupId, int startDelta, int endDelta)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS_VIDEO) && nvidia_highlights_has_permission(NVGSDK_SCOPE_HIGHLIGHTS))
    {
        NVGSDK_VideoHighlightParams params;
        params.groupId = groupId;
        params.highlightId = highlightId;
        params.startDelta = startDelta;
        params.endDelta = endDelta;
        NVGSDK_Highlights_SetVideoHighlightAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D4230 (:481): the overlay's summary of the group, all tags and significances
void nvidia_highlights_open_summary(const char* groupId)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_is_enabled())
    {
        NVGSDK_GroupView view;
        view.groupId = groupId;
        view.tagsFilter = NVGSDK_HIGHLIGHT_TYPE_NONE;
        view.significanceFilter = NVGSDK_HIGHLIGHT_SIGNIFICANCE_NONE;
        NVGSDK_SummaryParams params;
        params.groupSummaryTable = &view;
        params.groupSummaryTableSize = 1;
        NVGSDK_Highlights_OpenSummaryAsync(s_pNvidiaSDKHandle, &params, nvidia_empty_callback, nullptr);
    }
    s_nvidiaHighlightsMutex.unlock();
}

// 0x005D42D0 (:501): one count request at a time
void nvidia_highlights_request_num_highlights(const char* groupId)
{
    s_nvidiaHighlightsMutex.lock(0);
    if (nvidia_highlights_is_enabled() && !s_requestNumHighlightsInProgress)
    {
        NVGSDK_GroupView view;
        view.groupId = groupId;
        view.tagsFilter = NVGSDK_HIGHLIGHT_TYPE_NONE;
        view.significanceFilter = NVGSDK_HIGHLIGHT_SIGNIFICANCE_NONE;
        NVGSDK_Highlights_GetNumberOfHighlightsAsync(s_pNvidiaSDKHandle, &view, nvidia_highlights_get_num_highlights_callback, nullptr);
        s_requestNumHighlightsInProgress = true;
    }
    s_nvidiaHighlightsMutex.unlock();
}
