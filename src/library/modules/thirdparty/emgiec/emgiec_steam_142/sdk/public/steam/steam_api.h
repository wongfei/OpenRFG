// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h
//
// Steamworks SDK 1.42 as emgiec ships it (stub_impl.md S2): the interfaces, the callback base classes and
// CSteamAPIContext, the table of interfaces SteamInternal_ModuleContext keeps (steam_api_internal.h). The flat API is
// sw_api.dll's SW_CCSys_* (sw_api.h). Every inline body is in steam_api_internal.h, as in the SDK.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamapps.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamclient.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamcontroller.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamfriends.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteammatchmaking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamnetworking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamremotestorage.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuser.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuserstats.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamutils.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"
#include "library/modules/thirdparty/sw_api/sw_api.h"

// size 0xC. The base of every callback and call result the Steam client calls back (through sw_api.dll's callback
// manager). Overloads are declared in the SDK's order; MSVC puts an overload group's slots in reverse declaration
// order (the vtbl offsets are the PDB's).
class CCallbackBase
{
public:
    CCallbackBase(); // 0x005D0B50 (steam_api.h:261)
    virtual void Run(void* pvParam) = 0; // vtbl+0x04
    virtual void Run(void* pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall) = 0; // vtbl+0x00
    int GetICallback()
    {
        return m_iCallback;
    }
    virtual int GetCallbackSizeBytes() = 0; // vtbl+0x08

protected:
    enum
    {
        k_ECallbackFlagsRegistered = 1,
        k_ECallbackFlagsGameServer = 2,
    };

    // 0x00 vfptr
    uint8 m_nCallbackFlags; // 0x04
    int m_iCallback; // 0x08

private:
    CCallbackBase(const CCallbackBase&);
    CCallbackBase& operator=(const CCallbackBase&);
};

// size 0xC. A callback whose parameter struct is sizeof_P bytes.
template<int sizeof_P>
class CCallbackImpl : protected CCallbackBase
{
public:
    ~CCallbackImpl(); // <16> 0x00C0F580 (steam_api.h:286)
    void SetGameserverFlag(); // <16> 0x00C0F600 (steam_api.h:287)

protected:
    virtual void Run(void* pvParam) = 0; // vtbl+0x04
    virtual void Run(void* pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall); // vtbl+0x00 <16> 0x00C0F5A0 (steam_api.h:291)
    virtual int GetCallbackSizeBytes(); // vtbl+0x08 <16> 0x00C0F5B0 (steam_api.h:292)
};

// size (<GameLinkInternet,LobbyMatchList_t>) 0x28. Calls a member function of T when the asynchronous call hAPICall
// completes with P.
template<typename T, typename P>
class CCallResult : private CCallbackBase
{
public:
    typedef void (T::*func_t)(P*, bool);

    CCallResult(); // <GameLinkInternet,LobbyMatchList_t> 0x00CD11B0 (steam_api_internal.h:209)
    ~CCallResult(); // <GameLinkInternet,LobbyMatchList_t> 0x00CD3D10 (steam_api_internal.h:249)
    void Set(SteamAPICall_t hAPICall, T* p, func_t func); // <GameLinkInternet,LobbyMatchList_t> 0x00CD11F0 (steam_api_internal.h:218)
    bool IsActive() const;
    void Cancel(); // <GameLinkInternet,LobbyMatchList_t> 0x00CD1660 (steam_api_internal.h:238)
    void SetGameserverFlag()
    {
        m_nCallbackFlags |= k_ECallbackFlagsGameServer;
    }

private:
    virtual void Run(void* pvParam); // vtbl+0x04 <GameLinkInternet,LobbyMatchList_t> 0x00CD1290 (steam_api_internal.h:255)
    virtual void Run(void* pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall); // vtbl+0x00 <GameLinkInternet,LobbyMatchList_t> 0x00CD1250 (steam_api_internal.h:262)
    virtual int GetCallbackSizeBytes() // vtbl+0x08 <GameLinkInternet,LobbyMatchList_t> 0x00CD11E0 (steam_api.h:317)
    {
        return sizeof(P);
    }

    SteamAPICall_t m_hAPICall; // 0x10
    T* m_pObj; // 0x18
    func_t m_Func; // 0x20
};

// size (<ISteamStatsCallback,UserStatsReceived_t,0>) 0x14. Calls a member function of T when Steam posts callback P.
template<typename T, typename P, bool bGameserver = false>
class CCallback : public CCallbackImpl<sizeof(P)>
{
public:
    typedef void (T::*func_t)(P*);

    CCallback(T* pObj, func_t func); // <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14AB0 (steam_api_internal.h:279)
    void Register(T* pObj, func_t func); // <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14730 (steam_api_internal.h:289)
    void Unregister(); // <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C13720 (steam_api_internal.h:304)

protected:
    virtual void Run(void* pvParam); // vtbl+0x04 <ISteamStatsCallback,UserStatsReceived_t,0> 0x00C14B00 (steam_api_internal.h:311)

    T* m_pObj; // 0x0C
    func_t m_Func; // 0x10
};

// size 0x54
class CSteamAPIContext
{
public:
    CSteamAPIContext();
    void Clear(); // 0x00402090 (steam_api_internal.h:61)
    bool Init(); // 0x004020E0 (steam_api_internal.h:89)
    ISteamClient* SteamClient() const
    {
        return m_pSteamClient;
    }
    ISteamUser* SteamUser() const // 0x007B99E0 (:159)
    {
        return m_pSteamUser;
    }
    ISteamFriends* SteamFriends() const // 0x008834D0 (:160)
    {
        return m_pSteamFriends;
    }
    ISteamUtils* SteamUtils() const // 0x00C12BB0 (:161)
    {
        return m_pSteamUtils;
    }
    ISteamMatchmaking* SteamMatchmaking() const // 0x005D0B40 (:162)
    {
        return m_pSteamMatchmaking;
    }
    ISteamUserStats* SteamUserStats() const // 0x00C12BC0 (:163)
    {
        return m_pSteamUserStats;
    }
    ISteamApps* SteamApps() const // 0x00402080 (:164)
    {
        return m_pSteamApps;
    }
    ISteamMatchmakingServers* SteamMatchmakingServers() const
    {
        return m_pSteamMatchmakingServers;
    }
    ISteamNetworking* SteamNetworking() const // 0x00CD0120 (:166)
    {
        return m_pSteamNetworking;
    }
    ISteamRemoteStorage* SteamRemoteStorage() const // 0x00CCCCB0 (:167)
    {
        return m_pSteamRemoteStorage;
    }
    ISteamController* SteamController() const // 0x00C75F10 (:170)
    {
        return m_pController;
    }

private:
    ISteamClient* m_pSteamClient; // 0x00
    ISteamUser* m_pSteamUser; // 0x04
    ISteamFriends* m_pSteamFriends; // 0x08
    ISteamUtils* m_pSteamUtils; // 0x0C
    ISteamMatchmaking* m_pSteamMatchmaking; // 0x10
    ISteamUserStats* m_pSteamUserStats; // 0x14
    ISteamApps* m_pSteamApps; // 0x18
    ISteamMatchmakingServers* m_pSteamMatchmakingServers; // 0x1C
    ISteamNetworking* m_pSteamNetworking; // 0x20
    ISteamRemoteStorage* m_pSteamRemoteStorage; // 0x24
    ISteamScreenshots* m_pSteamScreenshots; // 0x28
    ISteamHTTP* m_pSteamHTTP; // 0x2C
    ISteamController* m_pController; // 0x30
    ISteamUGC* m_pSteamUGC; // 0x34
    ISteamAppList* m_pSteamAppList; // 0x38
    ISteamMusic* m_pSteamMusic; // 0x3C
    ISteamMusicRemote* m_pSteamMusicRemote; // 0x40
    ISteamHTMLSurface* m_pSteamHTMLSurface; // 0x44
    ISteamInventory* m_pSteamInventory; // 0x48
    ISteamVideo* m_pSteamVideo; // 0x4C
    ISteamParentalSettings* m_pSteamParentalSettings; // 0x50
};
