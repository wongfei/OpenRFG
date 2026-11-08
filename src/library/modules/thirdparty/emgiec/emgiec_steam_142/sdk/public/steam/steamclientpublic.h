// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// CSteamID (a 64-bit Steam id: account id, instance, account type, universe), its enums, EResult and the auth session
// results (M7).
// CSteamID is declared under `#pragma pack(push, 1)` as in the SDK (keen::UserAccountSteam is 0x10A bytes and
// UserStatsReceived_t::m_steamIDUser sits at 0xC).
#pragma once

#include <stdint.h>

#pragma pack(push, 1)

enum EResult
{
    k_EResultOK = 1,
    k_EResultFail = 2,
    k_EResultNoConnection = 3,
    k_EResultInvalidPassword = 5,
    k_EResultLoggedInElsewhere = 6,
    k_EResultInvalidProtocolVer = 7,
    k_EResultInvalidParam = 8,
    k_EResultFileNotFound = 9,
    k_EResultBusy = 10,
    k_EResultInvalidState = 11,
    k_EResultInvalidName = 12,
    k_EResultInvalidEmail = 13,
    k_EResultDuplicateName = 14,
    k_EResultAccessDenied = 15,
    k_EResultTimeout = 16,
    k_EResultBanned = 17,
    k_EResultAccountNotFound = 18,
    k_EResultInvalidSteamID = 19,
    k_EResultServiceUnavailable = 20,
    k_EResultNotLoggedOn = 21,
    k_EResultPending = 22,
    k_EResultEncryptionFailure = 23,
    k_EResultInsufficientPrivilege = 24,
    k_EResultLimitExceeded = 25,
    k_EResultRevoked = 26,
    k_EResultExpired = 27,
    k_EResultAlreadyRedeemed = 28,
    k_EResultDuplicateRequest = 29,
    k_EResultAlreadyOwned = 30,
    k_EResultIPNotFound = 31,
    k_EResultPersistFailed = 32,
    k_EResultLockingFailed = 33,
    k_EResultLogonSessionReplaced = 34,
    k_EResultConnectFailed = 35,
    k_EResultHandshakeFailed = 36,
    k_EResultIOFailure = 37,
    k_EResultRemoteDisconnect = 38,
    k_EResultShoppingCartNotFound = 39,
    k_EResultBlocked = 40,
    k_EResultIgnored = 41,
    k_EResultNoMatch = 42,
    k_EResultAccountDisabled = 43,
    k_EResultServiceReadOnly = 44,
    k_EResultAccountNotFeatured = 45,
    k_EResultAdministratorOK = 46,
    k_EResultContentVersion = 47,
    k_EResultTryAnotherCM = 48,
    k_EResultPasswordRequiredToKickSession = 49,
    k_EResultAlreadyLoggedInElsewhere = 50,
    k_EResultSuspended = 51,
    k_EResultCancelled = 52,
    k_EResultDataCorruption = 53,
    k_EResultDiskFull = 54,
    k_EResultRemoteCallFailed = 55,
    k_EResultPasswordUnset = 56,
    k_EResultExternalAccountUnlinked = 57,
    k_EResultPSNTicketInvalid = 58,
    k_EResultExternalAccountAlreadyLinked = 59,
    k_EResultRemoteFileConflict = 60,
    k_EResultIllegalPassword = 61,
    k_EResultSameAsPreviousValue = 62,
    k_EResultAccountLogonDenied = 63,
    k_EResultCannotUseOldPassword = 64,
    k_EResultInvalidLoginAuthCode = 65,
    k_EResultAccountLogonDeniedNoMail = 66,
    k_EResultHardwareNotCapableOfIPT = 67,
    k_EResultIPTInitError = 68,
    k_EResultParentalControlRestricted = 69,
    k_EResultFacebookQueryError = 70,
    k_EResultExpiredLoginAuthCode = 71,
    k_EResultIPLoginRestrictionFailed = 72,
    k_EResultAccountLockedDown = 73,
    k_EResultAccountLogonDeniedVerifiedEmailRequired = 74,
    k_EResultNoMatchingURL = 75,
    k_EResultBadResponse = 76,
    k_EResultRequirePasswordReEntry = 77,
    k_EResultValueOutOfRange = 78,
    k_EResultUnexpectedError = 79,
    k_EResultDisabled = 80,
    k_EResultInvalidCEGSubmission = 81,
    k_EResultRestrictedDevice = 82,
    k_EResultRegionLocked = 83,
    k_EResultRateLimitExceeded = 84,
    k_EResultAccountLoginDeniedNeedTwoFactor = 85,
    k_EResultItemDeleted = 86,
    k_EResultAccountLoginDeniedThrottle = 87,
    k_EResultTwoFactorCodeMismatch = 88,
    k_EResultTwoFactorActivationCodeMismatch = 89,
    k_EResultAccountAssociatedToMultiplePartners = 90,
    k_EResultNotModified = 91,
    k_EResultNoMobileDevice = 92,
    k_EResultTimeNotSynced = 93,
    k_EResultSmsCodeFailed = 94,
    k_EResultAccountLimitExceeded = 95,
    k_EResultAccountActivityLimitExceeded = 96,
    k_EResultPhoneActivityLimitExceeded = 97,
    k_EResultRefundToWallet = 98,
    k_EResultEmailSendFailure = 99,
    k_EResultNotSettled = 100,
    k_EResultNeedCaptcha = 101,
    k_EResultGSLTDenied = 102,
    k_EResultGSOwnerDenied = 103,
    k_EResultInvalidItemType = 104,
    k_EResultIPBanned = 105,
    k_EResultGSLTExpired = 106,
    k_EResultInsufficientFunds = 107,
    k_EResultTooManyPending = 108,
    k_EResultNoSiteLicensesFound = 109,
    k_EResultWGNetworkSendExceeded = 110,
    k_EResultAccountNotFriends = 111,
    k_EResultLimitedUserAccount = 112,
};

enum EUniverse
{
    k_EUniverseInvalid = 0,
    k_EUniversePublic = 1,
    k_EUniverseBeta = 2,
    k_EUniverseInternal = 3,
    k_EUniverseDev = 4,
    k_EUniverseMax = 5,
};

enum EAccountType
{
    k_EAccountTypeInvalid = 0,
    k_EAccountTypeIndividual = 1,
    k_EAccountTypeMultiseat = 2,
    k_EAccountTypeGameServer = 3,
    k_EAccountTypeAnonGameServer = 4,
    k_EAccountTypePending = 5,
    k_EAccountTypeContentServer = 6,
    k_EAccountTypeClan = 7,
    k_EAccountTypeChat = 8,
    k_EAccountTypeConsoleUser = 9,
    k_EAccountTypeAnonUser = 10,
    k_EAccountTypeMax = 11,
};

enum EBeginAuthSessionResult
{
    k_EBeginAuthSessionResultOK = 0,
    k_EBeginAuthSessionResultInvalidTicket = 1,
    k_EBeginAuthSessionResultDuplicateRequest = 2,
    k_EBeginAuthSessionResultInvalidVersion = 3,
    k_EBeginAuthSessionResultGameMismatch = 4,
    k_EBeginAuthSessionResultExpiredTicket = 5,
};

enum EAuthSessionResponse
{
    k_EAuthSessionResponseOK = 0,
    k_EAuthSessionResponseUserNotConnectedToSteam = 1,
    k_EAuthSessionResponseNoLicenseOrExpired = 2,
    k_EAuthSessionResponseVACBanned = 3,
    k_EAuthSessionResponseLoggedInElseWhere = 4,
    k_EAuthSessionResponseVACCheckTimedOut = 5,
    k_EAuthSessionResponseAuthTicketCanceled = 6,
    k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed = 7,
    k_EAuthSessionResponseAuthTicketInvalid = 8,
    k_EAuthSessionResponsePublisherIssuedBan = 9,
};

// size 0x8
class CSteamID
{
public:
    // size 0x8
    union SteamID_t
    {
        // size 0x8
        struct SteamIDComponent_t
        {
            unsigned int m_unAccountID : 32; // 0x00:0
            unsigned int m_unAccountInstance : 20; // 0x04:0
            unsigned int m_EAccountType : 4; // 0x04:20
            EUniverse m_EUniverse : 8; // 0x04:24
        };

        union
        {
            SteamIDComponent_t m_comp; // 0x00
            uint64_t m_unAll64Bits; // 0x00
        };
    };

private:
    CSteamID(int);
    CSteamID(unsigned int);
public:
    CSteamID(const char*, EUniverse);
    CSteamID(uint64_t ulSteamID); // 0x005D39F0 (.h:618)
    CSteamID(unsigned int, unsigned int, EUniverse, EAccountType);
    CSteamID(unsigned int, EUniverse, EAccountType);
    CSteamID(); // 0x00458A10 (.h:575)
    void Set(unsigned int, EUniverse, EAccountType);
    void InstancedSet(unsigned int, unsigned int, EUniverse, EAccountType);
    void FullSet(uint64_t, EUniverse, EAccountType);
    void SetFromUint64(uint64_t ulSteamID); // 0x005D0B20 (.h:687)
    void Clear(); // 0x00CD00D0 (.h:696)
    uint64_t ConvertToUint64() const; // 0x005D0B30 (.h:739)
    uint64_t GetStaticAccountKey() const;
    void CreateBlankAnonLogon(EUniverse);
    void CreateBlankAnonUserLogon(EUniverse);
    bool BBlankAnonAccount() const;
    bool BGameServerAccount() const;
    bool BPersistentGameServerAccount() const;
    bool BAnonGameServerAccount() const;
    bool BContentServerAccount() const;
    bool BClanAccount() const;
    bool BChatAccount() const;
    bool IsLobby() const;
    bool BIndividualAccount() const;
    bool BAnonAccount() const;
    bool BAnonUserAccount() const;
    bool BConsoleUserAccount() const;
    void SetAccountID(unsigned int);
    void SetAccountInstance(unsigned int);
    void ClearIndividualInstance();
    bool HasNoIndividualInstance() const;
    unsigned int GetAccountID() const; // 0x007B99D0 (.h:887)
    unsigned int GetUnAccountInstance() const;
    EAccountType GetEAccountType() const;
    EUniverse GetEUniverse() const;
    void SetEUniverse(EUniverse);
    bool IsValid() const; // 0x00941740 (.h:947)
    static const char* Render(uint64_t);
    const char* Render() const;
    void SetFromString(const char*, EUniverse);
    bool SetFromStringStrict(const char*, EUniverse);
    bool SetFromSteam2String(const char*, EUniverse);
    bool operator==(const CSteamID& val) const; // 0x00CBA8D0 (.h:908)
    bool operator!=(const CSteamID& val) const; // 0x00CD00F0 (.h:909)
    bool operator<(const CSteamID&) const;
    bool operator>(const CSteamID&) const;
    bool BValidExternalSteamID() const;

private:
    SteamID_t m_steamid; // 0x00
};


// The inline members the recovered code calls (multiplayer.md MP2; the Steamworks SDK defines them in this header).
// 0x00458A10 (.h:575): every field 0 (account id, instance, type, universe)
inline CSteamID::CSteamID()
{
    m_steamid.m_unAll64Bits = 0;
}

// 0x005D39F0 (.h:618)
inline CSteamID::CSteamID(uint64_t ulSteamID)
{
    m_steamid.m_unAll64Bits = ulSteamID;
}

// 0x005D0B20 (.h:687)
inline void CSteamID::SetFromUint64(uint64_t ulSteamID)
{
    m_steamid.m_unAll64Bits = ulSteamID;
}

// 0x00CD00D0 (.h:696)
inline void CSteamID::Clear()
{
    m_steamid.m_unAll64Bits = 0;
}

// 0x005D0B30 (.h:739)
inline uint64_t CSteamID::ConvertToUint64() const
{
    return m_steamid.m_unAll64Bits;
}

// 0x007B99D0 (.h:887)
inline unsigned int CSteamID::GetAccountID() const
{
    return m_steamid.m_comp.m_unAccountID;
}

// 0x00941740 (.h:947): the SDK's validity rules (account type in range, universe set, per-type id/instance checks).
inline bool CSteamID::IsValid() const
{
    if (m_steamid.m_comp.m_EAccountType <= k_EAccountTypeInvalid || m_steamid.m_comp.m_EAccountType >= k_EAccountTypeMax)
    {
        return false;
    }
    if (m_steamid.m_comp.m_EUniverse <= k_EUniverseInvalid || m_steamid.m_comp.m_EUniverse >= k_EUniverseMax)
    {
        return false;
    }
    if (m_steamid.m_comp.m_EAccountType == k_EAccountTypeIndividual)
    {
        // Magic number: 4, k_unSteamUserWebInstance (the highest instance of an individual account).
        if (m_steamid.m_comp.m_unAccountID == 0 || m_steamid.m_comp.m_unAccountInstance > 4)
        {
            return false;
        }
    }
    if (m_steamid.m_comp.m_EAccountType == k_EAccountTypeClan)
    {
        if (m_steamid.m_comp.m_unAccountID == 0 || m_steamid.m_comp.m_unAccountInstance != 0)
        {
            return false;
        }
    }
    if (m_steamid.m_comp.m_EAccountType == k_EAccountTypeGameServer)
    {
        if (m_steamid.m_comp.m_unAccountID == 0)
        {
            return false;
        }
    }
    return true;
}

// 0x00CBA8D0 (.h:908)
inline bool CSteamID::operator==(const CSteamID& val) const
{
    return m_steamid.m_unAll64Bits == val.m_steamid.m_unAll64Bits;
}

// 0x00CD00F0 (.h:909)
inline bool CSteamID::operator!=(const CSteamID& val) const
{
    return !operator==(val);
}

// size 0x8: an app, mod, shortcut or P2P file id (a game id as Steam reports it in callbacks).
class CGameID
{
public:
    enum EGameIDType
    {
        k_EGameIDTypeApp = 0,
        k_EGameIDTypeGameMod = 1,
        k_EGameIDTypeShortcut = 2,
        k_EGameIDTypeP2P = 3,
    };

    // size 0x8
    struct GameID_t
    {
        unsigned int m_nAppID : 24; // 0x00:0
        unsigned int m_nType : 8; // 0x00:24
        unsigned int m_nModID : 32; // 0x04:0
    };

    CGameID()
    {
        m_ulGameID = 0;
    }

private:
    union
    {
        uint64_t m_ulGameID; // 0x00
        GameID_t m_gameID; // 0x00
    };
};

// Enums the interfaces name in methods the port does not call: declared opaque with the int size the SDK's enums have.
enum EChatEntryType : int;
#pragma pack(pop)
