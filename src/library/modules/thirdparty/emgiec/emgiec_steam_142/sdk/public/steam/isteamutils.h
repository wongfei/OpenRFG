// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamutils.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): ISteamUtils (SteamUtils009) from the PDB's vtable.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum ENotificationPosition : int;
enum ESteamAPICallFailure : int;
enum EGamepadTextInputMode : int;
enum EGamepadTextInputLineMode : int;

// size 0x4
class ISteamUtils
{
public:
    virtual uint32 GetSecondsSinceAppActive() = 0; // vtbl+0x00
    virtual uint32 GetSecondsSinceComputerActive() = 0; // vtbl+0x04
    virtual EUniverse GetConnectedUniverse() = 0; // vtbl+0x08
    virtual uint32 GetServerRealTime() = 0; // vtbl+0x0C
    virtual const char* GetIPCountry() = 0; // vtbl+0x10
    virtual bool GetImageSize(int iImage, uint32* pnWidth, uint32* pnHeight) = 0; // vtbl+0x14
    virtual bool GetImageRGBA(int iImage, uint8* pubDest, int nDestBufferSize) = 0; // vtbl+0x18
    virtual bool GetCSERIPPort(uint32* unIP, uint16* usPort) = 0; // vtbl+0x1C
    virtual uint8 GetCurrentBatteryPower() = 0; // vtbl+0x20
    virtual uint32 GetAppID() = 0; // vtbl+0x24
    virtual void SetOverlayNotificationPosition(ENotificationPosition eNotificationPosition) = 0; // vtbl+0x28
    virtual bool IsAPICallCompleted(SteamAPICall_t hSteamAPICall, bool* pbFailed) = 0; // vtbl+0x2C
    virtual ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t hSteamAPICall) = 0; // vtbl+0x30
    virtual bool GetAPICallResult(SteamAPICall_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed) = 0; // vtbl+0x34
protected:
    virtual void RunFrame() = 0; // vtbl+0x38
public:
    virtual uint32 GetIPCCallCount() = 0; // vtbl+0x3C
    virtual void SetWarningMessageHook(void (__cdecl* pFunction)(int, const char*)) = 0; // vtbl+0x40
    virtual bool IsOverlayEnabled() = 0; // vtbl+0x44
    virtual bool BOverlayNeedsPresent() = 0; // vtbl+0x48
    virtual SteamAPICall_t CheckFileSignature(const char* szFileName) = 0; // vtbl+0x4C
    virtual bool ShowGamepadTextInput(EGamepadTextInputMode eInputMode, EGamepadTextInputLineMode eLineInputMode, const char* pchDescription, uint32 unCharMax, const char* pchExistingText) = 0; // vtbl+0x50
    virtual uint32 GetEnteredGamepadTextLength() = 0; // vtbl+0x54
    virtual bool GetEnteredGamepadTextInput(char* pchText, uint32 cchText) = 0; // vtbl+0x58
    virtual const char* GetSteamUILanguage() = 0; // vtbl+0x5C
    virtual bool IsSteamRunningInVR() = 0; // vtbl+0x60
    virtual void SetOverlayNotificationInset(int nHorizontalInset, int nVerticalInset) = 0; // vtbl+0x64
    virtual bool IsSteamInBigPictureMode() = 0; // vtbl+0x68
    virtual void StartVRDashboard() = 0; // vtbl+0x6C
    virtual bool IsVRHeadsetStreamingEnabled() = 0; // vtbl+0x70
    virtual void SetVRHeadsetStreamingEnabled(bool bEnabled) = 0; // vtbl+0x74

    // 0x00 vfptr
};
