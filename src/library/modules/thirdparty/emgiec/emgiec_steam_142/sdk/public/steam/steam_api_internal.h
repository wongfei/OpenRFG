// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h
//
// Steamworks SDK 1.42 as emgiec ships it, third party (stub_impl.md S2): the inline half of steam_api.h. The interface
// accessors (SteamUser() ...) read the module's CSteamAPIContext, which SW_CCSys_DynamicInit (SteamInternal_ContextInit)
// fills through SteamInternal_OnContextInit whenever Steam (re)starts; without Steam (no pipe) every interface is
// nullptr. The callback templates register with sw_api.dll's callback manager. rfg.exe keeps out-of-line copies of
// these (the VAs below); here they are inline, as the SDK writes them.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"

// 0x004023D0 (:17)
inline void SteamInternal_OnContextInit(void* p)
{
    ((CSteamAPIContext*)p)->Clear();
    if (SW_CCSys_GetPInterface() != 0)
    {
        ((CSteamAPIContext*)p)->Init();
    }
}

// 0x00402430 (:23)
inline CSteamAPIContext& SteamInternal_ModuleContext()
{
    // SW_CCSys_DynamicInit takes { void (*pFn)(void* pCtx); uintp counter; CSteamAPIContext ctx; }.
    static void* s_CallbackCounterAndContext[sizeof(CSteamAPIContext) / sizeof(void*) + 2] = { (void*)&SteamInternal_OnContextInit, 0 };
    return *(CSteamAPIContext*)SW_CCSys_DynamicInit(s_CallbackCounterAndContext);
}

// 0x007CAF70 (:36)
inline ISteamUser* SteamUser()
{
    return SteamInternal_ModuleContext().SteamUser();
}

// 0x008A6900 (:37)
inline ISteamFriends* SteamFriends()
{
    return SteamInternal_ModuleContext().SteamFriends();
}

// 0x00C13790 (:38)
inline ISteamUtils* SteamUtils()
{
    return SteamInternal_ModuleContext().SteamUtils();
}

// 0x005D3A10 (:39)
inline ISteamMatchmaking* SteamMatchmaking()
{
    return SteamInternal_ModuleContext().SteamMatchmaking();
}

// 0x00C137B0 (:40)
inline ISteamUserStats* SteamUserStats()
{
    return SteamInternal_ModuleContext().SteamUserStats();
}

// 0x00402440 (:41)
inline ISteamApps* SteamApps()
{
    return SteamInternal_ModuleContext().SteamApps();
}

// 0x00CD1700 (:43)
inline ISteamNetworking* SteamNetworking()
{
    return SteamInternal_ModuleContext().SteamNetworking();
}

// 0x00CCD9D0 (:44)
inline ISteamRemoteStorage* SteamRemoteStorage()
{
    return SteamInternal_ModuleContext().SteamRemoteStorage();
}

// 0x00C79380 (:47)
inline ISteamController* SteamController()
{
    return SteamInternal_ModuleContext().SteamController();
}

inline CSteamAPIContext::CSteamAPIContext()
{
    Clear();
}

// 0x00402090 (:61)
inline void CSteamAPIContext::Clear()
{
    m_pSteamClient = nullptr;
    m_pSteamUser = nullptr;
    m_pSteamFriends = nullptr;
    m_pSteamUtils = nullptr;
    m_pSteamMatchmaking = nullptr;
    m_pSteamUserStats = nullptr;
    m_pSteamApps = nullptr;
    m_pSteamMatchmakingServers = nullptr;
    m_pSteamNetworking = nullptr;
    m_pSteamRemoteStorage = nullptr;
    m_pSteamHTTP = nullptr;
    m_pSteamScreenshots = nullptr;
    m_pController = nullptr;
    m_pSteamUGC = nullptr;
    m_pSteamAppList = nullptr;
    m_pSteamMusic = nullptr;
    m_pSteamMusicRemote = nullptr;
    m_pSteamHTMLSurface = nullptr;
    m_pSteamInventory = nullptr;
    m_pSteamVideo = nullptr;
    m_pSteamParentalSettings = nullptr;
}

// 0x004020E0 (:89): every interface at the version the SDK was built with; false at the first one Steam does not have.
inline bool CSteamAPIContext::Init()
{
    HSteamUser hSteamUser = SW_CCSys_GetUInterface();
    HSteamPipe hSteamPipe = SW_CCSys_GetPInterface();
    if (!hSteamPipe)
    {
        return false;
    }
    m_pSteamClient = (ISteamClient*)SW_CCSys_CreateInternalModule("SteamClient017");
    if (!m_pSteamClient)
    {
        return false;
    }
    m_pSteamUser = m_pSteamClient->GetISteamUser(hSteamUser, hSteamPipe, "SteamUser019");
    if (!m_pSteamUser)
    {
        return false;
    }
    m_pSteamFriends = m_pSteamClient->GetISteamFriends(hSteamUser, hSteamPipe, "SteamFriends015");
    if (!m_pSteamFriends)
    {
        return false;
    }
    m_pSteamUtils = m_pSteamClient->GetISteamUtils(hSteamPipe, "SteamUtils009");
    if (!m_pSteamUtils)
    {
        return false;
    }
    m_pSteamMatchmaking = m_pSteamClient->GetISteamMatchmaking(hSteamUser, hSteamPipe, "SteamMatchMaking009");
    if (!m_pSteamMatchmaking)
    {
        return false;
    }
    m_pSteamMatchmakingServers = m_pSteamClient->GetISteamMatchmakingServers(hSteamUser, hSteamPipe, "SteamMatchMakingServers002");
    if (!m_pSteamMatchmakingServers)
    {
        return false;
    }
    m_pSteamUserStats = m_pSteamClient->GetISteamUserStats(hSteamUser, hSteamPipe, "STEAMUSERSTATS_INTERFACE_VERSION011");
    if (!m_pSteamUserStats)
    {
        return false;
    }
    m_pSteamApps = m_pSteamClient->GetISteamApps(hSteamUser, hSteamPipe, "STEAMAPPS_INTERFACE_VERSION008");
    if (!m_pSteamApps)
    {
        return false;
    }
    m_pSteamNetworking = m_pSteamClient->GetISteamNetworking(hSteamUser, hSteamPipe, "SteamNetworking005");
    if (!m_pSteamNetworking)
    {
        return false;
    }
    m_pSteamRemoteStorage = m_pSteamClient->GetISteamRemoteStorage(hSteamUser, hSteamPipe, "STEAMREMOTESTORAGE_INTERFACE_VERSION014");
    if (!m_pSteamRemoteStorage)
    {
        return false;
    }
    m_pSteamScreenshots = m_pSteamClient->GetISteamScreenshots(hSteamUser, hSteamPipe, "STEAMSCREENSHOTS_INTERFACE_VERSION003");
    if (!m_pSteamScreenshots)
    {
        return false;
    }
    m_pSteamHTTP = m_pSteamClient->GetISteamHTTP(hSteamUser, hSteamPipe, "STEAMHTTP_INTERFACE_VERSION002");
    if (!m_pSteamHTTP)
    {
        return false;
    }
    m_pController = m_pSteamClient->GetISteamController(hSteamUser, hSteamPipe, "SteamController006");
    if (!m_pController)
    {
        return false;
    }
    m_pSteamUGC = m_pSteamClient->GetISteamUGC(hSteamUser, hSteamPipe, "STEAMUGC_INTERFACE_VERSION010");
    if (!m_pSteamUGC)
    {
        return false;
    }
    m_pSteamAppList = m_pSteamClient->GetISteamAppList(hSteamUser, hSteamPipe, "STEAMAPPLIST_INTERFACE_VERSION001");
    if (!m_pSteamAppList)
    {
        return false;
    }
    m_pSteamMusic = m_pSteamClient->GetISteamMusic(hSteamUser, hSteamPipe, "STEAMMUSIC_INTERFACE_VERSION001");
    if (!m_pSteamMusic)
    {
        return false;
    }
    m_pSteamMusicRemote = m_pSteamClient->GetISteamMusicRemote(hSteamUser, hSteamPipe, "STEAMMUSICREMOTE_INTERFACE_VERSION001");
    if (!m_pSteamMusicRemote)
    {
        return false;
    }
    m_pSteamHTMLSurface = m_pSteamClient->GetISteamHTMLSurface(hSteamUser, hSteamPipe, "STEAMHTMLSURFACE_INTERFACE_VERSION_004");
    if (!m_pSteamHTMLSurface)
    {
        return false;
    }
    m_pSteamInventory = m_pSteamClient->GetISteamInventory(hSteamUser, hSteamPipe, "STEAMINVENTORY_INTERFACE_V002");
    if (!m_pSteamInventory)
    {
        return false;
    }
    m_pSteamVideo = m_pSteamClient->GetISteamVideo(hSteamUser, hSteamPipe, "STEAMVIDEO_INTERFACE_V002");
    if (!m_pSteamVideo)
    {
        return false;
    }
    m_pSteamParentalSettings = m_pSteamClient->GetISteamParentalSettings(hSteamUser, hSteamPipe, "STEAMPARENTALSETTINGS_INTERFACE_VERSION001");
    if (!m_pSteamParentalSettings)
    {
        return false;
    }
    return true;
}

// 0x005D0B50 (steam_api.h:261)
inline CCallbackBase::CCallbackBase()
{
    m_nCallbackFlags = 0;
    m_iCallback = 0;
}

// <16> 0x00C0F580 (steam_api.h:286)
template<int sizeof_P>
inline CCallbackImpl<sizeof_P>::~CCallbackImpl()
{
    if (m_nCallbackFlags & k_ECallbackFlagsRegistered)
    {
        SW_CCSys_RemoveCallbackFunc(this);
    }
}

// <16> 0x00C0F600 (steam_api.h:287)
template<int sizeof_P>
inline void CCallbackImpl<sizeof_P>::SetGameserverFlag()
{
    m_nCallbackFlags |= k_ECallbackFlagsGameServer;
}

// <16> 0x00C0F5A0 (steam_api.h:291)
template<int sizeof_P>
inline void CCallbackImpl<sizeof_P>::Run(void* pvParam, bool, SteamAPICall_t)
{
    Run(pvParam);
}

// <16> 0x00C0F5B0 (steam_api.h:292)
template<int sizeof_P>
inline int CCallbackImpl<sizeof_P>::GetCallbackSizeBytes()
{
    return sizeof_P;
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD11B0 (:209)
template<typename T, typename P>
inline CCallResult<T, P>::CCallResult()
{
    m_hAPICall = k_uAPICallInvalid;
    m_pObj = nullptr;
    m_Func = nullptr;
    m_iCallback = P::k_iCallback;
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD11F0 (:218)
template<typename T, typename P>
inline void CCallResult<T, P>::Set(SteamAPICall_t hAPICall, T* p, func_t func)
{
    if (m_hAPICall)
    {
        SW_CCSys_UnregisterCallResult(this, m_hAPICall);
    }
    m_hAPICall = hAPICall;
    m_pObj = p;
    m_Func = func;
    if (hAPICall)
    {
        SW_CCSys_RegisterCallResult(this, hAPICall);
    }
}

template<typename T, typename P>
inline bool CCallResult<T, P>::IsActive() const
{
    return m_hAPICall != k_uAPICallInvalid;
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD1660 (:238)
template<typename T, typename P>
inline void CCallResult<T, P>::Cancel()
{
    if (m_hAPICall != k_uAPICallInvalid)
    {
        SW_CCSys_UnregisterCallResult(this, m_hAPICall);
        m_hAPICall = k_uAPICallInvalid;
    }
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD3D10 (:249)
template<typename T, typename P>
inline CCallResult<T, P>::~CCallResult()
{
    Cancel();
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD1290 (:255)
template<typename T, typename P>
inline void CCallResult<T, P>::Run(void* pvParam)
{
    m_hAPICall = k_uAPICallInvalid;
    (m_pObj->*m_Func)((P*)pvParam, false);
}

// <GameLinkInternet,LobbyMatchList_t> 0x00CD1250 (:262)
template<typename T, typename P>
inline void CCallResult<T, P>::Run(void* pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall)
{
    if (hSteamAPICall == m_hAPICall)
    {
        m_hAPICall = k_uAPICallInvalid;
        (m_pObj->*m_Func)((P*)pvParam, bIOFailure);
    }
}

// <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14AB0 (:279)
template<typename T, typename P, bool bGameserver>
inline CCallback<T, P, bGameserver>::CCallback(T* pObj, func_t func)
    : m_pObj(nullptr), m_Func(nullptr)
{
    if (bGameserver)
    {
        this->SetGameserverFlag();
    }
    Register(pObj, func);
}

// <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14730 (:289)
template<typename T, typename P, bool bGameserver>
inline void CCallback<T, P, bGameserver>::Register(T* pObj, func_t func)
{
    if (!pObj || !func)
    {
        return;
    }
    if (this->m_nCallbackFlags & CCallbackBase::k_ECallbackFlagsRegistered)
    {
        Unregister();
    }
    if (bGameserver)
    {
        this->SetGameserverFlag();
    }
    m_pObj = pObj;
    m_Func = func;
    // SteamAPI_RegisterCallback sets k_ECallbackFlagsRegistered.
    SW_CCSys_InitCallbackFunc(this, P::k_iCallback);
}

// <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C13720 (:304)
template<typename T, typename P, bool bGameserver>
inline void CCallback<T, P, bGameserver>::Unregister()
{
    SW_CCSys_RemoveCallbackFunc(this);
}

// <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14B00 (:311)
template<typename T, typename P, bool bGameserver>
inline void CCallback<T, P, bGameserver>::Run(void* pvParam)
{
    (m_pObj->*m_Func)((P*)pvParam);
}
