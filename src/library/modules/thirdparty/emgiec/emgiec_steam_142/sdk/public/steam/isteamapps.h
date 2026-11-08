// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamapps.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): ISteamApps (STEAMAPPS_INTERFACE_VERSION008) from the PDB's
// vtable.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

// size 0x4
class ISteamApps
{
public:
    virtual bool BIsSubscribed() = 0; // vtbl+0x00
    virtual bool BIsLowViolence() = 0; // vtbl+0x04
    virtual bool BIsCybercafe() = 0; // vtbl+0x08
    virtual bool BIsVACBanned() = 0; // vtbl+0x0C
    virtual const char* GetCurrentGameLanguage() = 0; // vtbl+0x10
    virtual const char* GetAvailableGameLanguages() = 0; // vtbl+0x14
    virtual bool BIsSubscribedApp(AppId_t appID) = 0; // vtbl+0x18
    virtual bool BIsDlcInstalled(AppId_t appID) = 0; // vtbl+0x1C
    virtual uint32 GetEarliestPurchaseUnixTime(AppId_t nAppID) = 0; // vtbl+0x20
    virtual bool BIsSubscribedFromFreeWeekend() = 0; // vtbl+0x24
    virtual int GetDLCCount() = 0; // vtbl+0x28
    virtual bool BGetDLCDataByIndex(int iDLC, AppId_t* pAppID, bool* pbAvailable, char* pchName, int cchNameBufferSize) = 0; // vtbl+0x2C
    virtual void InstallDLC(AppId_t nAppID) = 0; // vtbl+0x30
    virtual void UninstallDLC(AppId_t nAppID) = 0; // vtbl+0x34
    virtual void RequestAppProofOfPurchaseKey(AppId_t nAppID) = 0; // vtbl+0x38
    virtual bool GetCurrentBetaName(char* pchName, int cchNameBufferSize) = 0; // vtbl+0x3C
    virtual bool MarkContentCorrupt(bool bMissingFilesOnly) = 0; // vtbl+0x40
    virtual uint32 GetInstalledDepots(AppId_t appID, uint32* pvecDepots, uint32 cMaxDepots) = 0; // vtbl+0x44
    virtual uint32 GetAppInstallDir(AppId_t appID, char* pchFolder, uint32 cchFolderBufferSize) = 0; // vtbl+0x48
    virtual bool BIsAppInstalled(AppId_t appID) = 0; // vtbl+0x4C
    virtual CSteamID GetAppOwner() = 0; // vtbl+0x50
    virtual const char* GetLaunchQueryParam(const char* pchKey) = 0; // vtbl+0x54
    virtual bool GetDlcDownloadProgress(AppId_t nAppID, uint64* punBytesDownloaded, uint64* punBytesTotal) = 0; // vtbl+0x58
    virtual int GetAppBuildId() = 0; // vtbl+0x5C
    virtual void RequestAllProofOfPurchaseKeys() = 0; // vtbl+0x60
    virtual SteamAPICall_t GetFileDetails(const char* pszFileName) = 0; // vtbl+0x64

    // 0x00 vfptr
};
