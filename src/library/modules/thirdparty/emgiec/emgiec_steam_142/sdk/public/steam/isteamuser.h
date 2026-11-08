// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuser.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// User callbacks: the auth session ticket was created, and the result of validating another user's ticket.
// stub_impl.md S2: ISteamUser (SteamUser019) from the PDB's vtable; emgiec renamed GetHSteamUser to SW_CCSys_GetU.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum EVoiceResult
{
    k_EVoiceResultOK = 0,
    k_EVoiceResultNotInitialized = 1,
    k_EVoiceResultNotRecording = 2,
    k_EVoiceResultNoData = 3,
    k_EVoiceResultBufferTooSmall = 4,
    k_EVoiceResultDataCorrupted = 5,
    k_EVoiceResultRestricted = 6,
    k_EVoiceResultUnsupportedCodec = 7,
    k_EVoiceResultReceiverOutOfDate = 8,
    k_EVoiceResultReceiverDidNotAnswer = 9,
};

enum EUserHasLicenseForAppResult : int;

// size 0x4
class ISteamUser
{
public:
    virtual HSteamUser SW_CCSys_GetU() = 0; // vtbl+0x00
    virtual bool BLoggedOn() = 0; // vtbl+0x04
    virtual CSteamID GetSteamID() = 0; // vtbl+0x08
    virtual int InitiateGameConnection(void* pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) = 0; // vtbl+0x0C
    virtual void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) = 0; // vtbl+0x10
    virtual void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char* pchExtraInfo) = 0; // vtbl+0x14
    virtual bool GetUserDataFolder(char* pchBuffer, int cubBuffer) = 0; // vtbl+0x18
    virtual void StartVoiceRecording() = 0; // vtbl+0x1C
    virtual void StopVoiceRecording() = 0; // vtbl+0x20
    virtual EVoiceResult GetAvailableVoice(uint32* pcbCompressed, uint32* pcbUncompressed_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) = 0; // vtbl+0x24
    virtual EVoiceResult GetVoice(bool bWantCompressed, void* pDestBuffer, uint32 cbDestBufferSize, uint32* nBytesWritten, bool bWantUncompressed_Deprecated, void* pUncompressedDestBuffer_Deprecated, uint32 cbUncompressedDestBufferSize_Deprecated, uint32* nUncompressBytesWritten_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) = 0; // vtbl+0x28
    virtual EVoiceResult DecompressVoice(const void* pCompressed, uint32 cbCompressed, void* pDestBuffer, uint32 cbDestBufferSize, uint32* nBytesWritten, uint32 nDesiredSampleRate) = 0; // vtbl+0x2C
    virtual uint32 GetVoiceOptimalSampleRate() = 0; // vtbl+0x30
    virtual HAuthTicket GetAuthSessionTicket(void* pTicket, int cbMaxTicket, uint32* pcbTicket) = 0; // vtbl+0x34
    virtual EBeginAuthSessionResult BeginAuthSession(const void* pAuthTicket, int cbAuthTicket, CSteamID steamID) = 0; // vtbl+0x38
    virtual void EndAuthSession(CSteamID steamID) = 0; // vtbl+0x3C
    virtual void CancelAuthTicket(HAuthTicket hAuthTicket) = 0; // vtbl+0x40
    virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) = 0; // vtbl+0x44
    virtual bool BIsBehindNAT() = 0; // vtbl+0x48
    virtual void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) = 0; // vtbl+0x4C
    virtual SteamAPICall_t RequestEncryptedAppTicket(void* pDataToInclude, int cbDataToInclude) = 0; // vtbl+0x50
    virtual bool GetEncryptedAppTicket(void* pTicket, int cbMaxTicket, uint32* pcbTicket) = 0; // vtbl+0x54
    virtual int GetGameBadgeLevel(int nSeries, bool bFoil) = 0; // vtbl+0x58
    virtual int GetPlayerSteamLevel() = 0; // vtbl+0x5C
    virtual SteamAPICall_t RequestStoreAuthURL(const char* pchRedirectURL) = 0; // vtbl+0x60
    virtual bool BIsPhoneVerified() = 0; // vtbl+0x64
    virtual bool BIsTwoFactorEnabled() = 0; // vtbl+0x68
    virtual bool BIsPhoneIdentifying() = 0; // vtbl+0x6C
    virtual bool BIsPhoneRequiringVerification() = 0; // vtbl+0x70

    // 0x00 vfptr
};

// size 0x8
struct GetAuthSessionTicketResponse_t
{
    enum
    {
        k_iCallback = 163,
    };

    unsigned int m_hAuthTicket; // 0x00
    EResult m_eResult; // 0x04
};

// size 0x14
struct ValidateAuthTicketResponse_t
{
    enum
    {
        k_iCallback = 143,
    };

    CSteamID m_SteamID; // 0x00
    EAuthSessionResponse m_eAuthSessionResponse; // 0x08
    CSteamID m_OwnerSteamID; // 0x0C
};
