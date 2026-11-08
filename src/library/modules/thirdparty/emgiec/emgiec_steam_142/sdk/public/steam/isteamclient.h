// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamclient.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): ISteamClient (SteamClient017), which CSteamAPIContext::Init asks
// for every other interface. Written from the PDB's vtable (pdbdump header ISteamClient); the pipe and user handles
// are HSteamPipe/HSteamUser (int in the PDB).
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

class ISteamUser;
class ISteamGameServer;
class ISteamFriends;
class ISteamUtils;
class ISteamMatchmaking;
class ISteamMatchmakingServers;
class ISteamUserStats;
class ISteamGameServerStats;
class ISteamApps;
class ISteamNetworking;
class ISteamRemoteStorage;
class ISteamScreenshots;
class ISteamHTTP;
class ISteamController;
class ISteamUGC;
class ISteamAppList;
class ISteamMusic;
class ISteamMusicRemote;
class ISteamHTMLSurface;
class ISteamInventory;
class ISteamVideo;
class ISteamParentalSettings;

// size 0x4
class ISteamClient
{
public:
    virtual HSteamPipe CreateSteamPipe() = 0; // vtbl+0x00
    virtual bool BReleaseSteamPipe(HSteamPipe hSteamPipe) = 0; // vtbl+0x04
    virtual HSteamUser ConnectToGlobalUser(HSteamPipe hSteamPipe) = 0; // vtbl+0x08
    virtual HSteamUser CreateLocalUser(HSteamPipe* phSteamPipe, EAccountType eAccountType) = 0; // vtbl+0x0C
    virtual void ReleaseUser(HSteamPipe hSteamPipe, HSteamUser hUser) = 0; // vtbl+0x10
    virtual ISteamUser* GetISteamUser(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x14
    virtual ISteamGameServer* GetISteamGameServer(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x18
    virtual void SetLocalIPBinding(uint32 unIP, uint16 usPort) = 0; // vtbl+0x1C
    virtual ISteamFriends* GetISteamFriends(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x20
    virtual ISteamUtils* GetISteamUtils(HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x24
    virtual ISteamMatchmaking* GetISteamMatchmaking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x28
    virtual ISteamMatchmakingServers* GetISteamMatchmakingServers(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x2C
    virtual void* GetISteamGenericInterface(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x30
    virtual ISteamUserStats* GetISteamUserStats(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x34
    virtual ISteamGameServerStats* GetISteamGameServerStats(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x38
    virtual ISteamApps* GetISteamApps(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x3C
    virtual ISteamNetworking* GetISteamNetworking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x40
    virtual ISteamRemoteStorage* GetISteamRemoteStorage(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x44
    virtual ISteamScreenshots* GetISteamScreenshots(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x48
protected:
    virtual void RunFrame() = 0; // vtbl+0x4C
public:
    virtual uint32 GetIPCCallCount() = 0; // vtbl+0x50
    virtual void SetWarningMessageHook(void (__cdecl* pFunction)(int, const char*)) = 0; // vtbl+0x54
    virtual bool BShutdownIfAllPipesClosed() = 0; // vtbl+0x58
    virtual ISteamHTTP* GetISteamHTTP(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x5C
protected:
    virtual void* DEPRECATED_GetISteamUnifiedMessages(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x60
public:
    virtual ISteamController* GetISteamController(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x64
    virtual ISteamUGC* GetISteamUGC(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x68
    virtual ISteamAppList* GetISteamAppList(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x6C
    virtual ISteamMusic* GetISteamMusic(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x70
    virtual ISteamMusicRemote* GetISteamMusicRemote(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x74
    virtual ISteamHTMLSurface* GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x78
protected:
    virtual void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(void (__cdecl* pFunc)()) = 0; // vtbl+0x7C
    virtual void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(void (__cdecl* pFunc)()) = 0; // vtbl+0x80
    virtual void Set_SteamAPI_CCheckCallbackRegisteredInProcess(uint32 (__cdecl* pFunc)(int)) = 0; // vtbl+0x84
public:
    virtual ISteamInventory* GetISteamInventory(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x88
    virtual ISteamVideo* GetISteamVideo(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x8C
    virtual ISteamParentalSettings* GetISteamParentalSettings(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char* pchVersion) = 0; // vtbl+0x90

    // 0x00 vfptr
};
