// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuserstats.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// User stats callbacks: stats received/stored, achievement stored, and the leaderboard find/upload/download results.
// stub_impl.md S2: ISteamUserStats (STEAMUSERSTATS_INTERFACE_VERSION011) from the PDB's vtable, the leaderboard enums
// and entry.
#pragma once

#include <stdint.h>
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum ELeaderboardDataRequest
{
    k_ELeaderboardDataRequestGlobal = 0,
    k_ELeaderboardDataRequestGlobalAroundUser = 1,
    k_ELeaderboardDataRequestFriends = 2,
    k_ELeaderboardDataRequestUsers = 3,
};

enum ELeaderboardSortMethod
{
    k_ELeaderboardSortMethodNone = 0,
    k_ELeaderboardSortMethodAscending = 1,
    k_ELeaderboardSortMethodDescending = 2,
};

enum ELeaderboardDisplayType
{
    k_ELeaderboardDisplayTypeNone = 0,
    k_ELeaderboardDisplayTypeNumeric = 1,
    k_ELeaderboardDisplayTypeTimeSeconds = 2,
    k_ELeaderboardDisplayTypeTimeMilliSeconds = 3,
};

enum ELeaderboardUploadScoreMethod
{
    k_ELeaderboardUploadScoreMethodNone = 0,
    k_ELeaderboardUploadScoreMethodKeepBest = 1,
    k_ELeaderboardUploadScoreMethodForceUpdate = 2,
};

// size 0x20
struct LeaderboardEntry_t
{
    CSteamID m_steamIDUser; // 0x00
    int m_nGlobalRank; // 0x08
    int m_nScore; // 0x0C
    int m_cDetails; // 0x10
    UGCHandle_t m_hUGC; // 0x18
};

// size 0x4. Overloads are declared in the SDK's order; MSVC puts an overload group's slots in reverse declaration order
// (the vtbl offsets are the PDB's).
class ISteamUserStats
{
public:
    virtual bool RequestCurrentStats() = 0; // vtbl+0x00
    virtual bool GetStat(const char* pchName, int* pData) = 0; // vtbl+0x08
    virtual bool GetStat(const char* pchName, float* pData) = 0; // vtbl+0x04
    virtual bool SetStat(const char* pchName, int nData) = 0; // vtbl+0x10
    virtual bool SetStat(const char* pchName, float fData) = 0; // vtbl+0x0C
    virtual bool UpdateAvgRateStat(const char* pchName, float flCountThisSession, double dSessionLength) = 0; // vtbl+0x14
    virtual bool GetAchievement(const char* pchName, bool* pbAchieved) = 0; // vtbl+0x18
    virtual bool SetAchievement(const char* pchName) = 0; // vtbl+0x1C
    virtual bool ClearAchievement(const char* pchName) = 0; // vtbl+0x20
    virtual bool GetAchievementAndUnlockTime(const char* pchName, bool* pbAchieved, uint32* punUnlockTime) = 0; // vtbl+0x24
    virtual bool StoreStats() = 0; // vtbl+0x28
    virtual int GetAchievementIcon(const char* pchName) = 0; // vtbl+0x2C
    virtual const char* GetAchievementDisplayAttribute(const char* pchName, const char* pchKey) = 0; // vtbl+0x30
    virtual bool IndicateAchievementProgress(const char* pchName, uint32 nCurProgress, uint32 nMaxProgress) = 0; // vtbl+0x34
    virtual uint32 GetNumAchievements() = 0; // vtbl+0x38
    virtual const char* GetAchievementName(uint32 iAchievement) = 0; // vtbl+0x3C
    virtual SteamAPICall_t RequestUserStats(CSteamID steamIDUser) = 0; // vtbl+0x40
    virtual bool GetUserStat(CSteamID steamIDUser, const char* pchName, int* pData) = 0; // vtbl+0x48
    virtual bool GetUserStat(CSteamID steamIDUser, const char* pchName, float* pData) = 0; // vtbl+0x44
    virtual bool GetUserAchievement(CSteamID steamIDUser, const char* pchName, bool* pbAchieved) = 0; // vtbl+0x4C
    virtual bool GetUserAchievementAndUnlockTime(CSteamID steamIDUser, const char* pchName, bool* pbAchieved, uint32* punUnlockTime) = 0; // vtbl+0x50
    virtual bool ResetAllStats(bool bAchievementsToo) = 0; // vtbl+0x54
    virtual SteamAPICall_t FindOrCreateLeaderboard(const char* pchLeaderboardName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType) = 0; // vtbl+0x58
    virtual SteamAPICall_t FindLeaderboard(const char* pchLeaderboardName) = 0; // vtbl+0x5C
    virtual const char* GetLeaderboardName(SteamLeaderboard_t hSteamLeaderboard) = 0; // vtbl+0x60
    virtual int GetLeaderboardEntryCount(SteamLeaderboard_t hSteamLeaderboard) = 0; // vtbl+0x64
    virtual ELeaderboardSortMethod GetLeaderboardSortMethod(SteamLeaderboard_t hSteamLeaderboard) = 0; // vtbl+0x68
    virtual ELeaderboardDisplayType GetLeaderboardDisplayType(SteamLeaderboard_t hSteamLeaderboard) = 0; // vtbl+0x6C
    virtual SteamAPICall_t DownloadLeaderboardEntries(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardDataRequest eLeaderboardDataRequest, int nRangeStart, int nRangeEnd) = 0; // vtbl+0x70
    virtual SteamAPICall_t DownloadLeaderboardEntriesForUsers(SteamLeaderboard_t hSteamLeaderboard, CSteamID* prgUsers, int cUsers) = 0; // vtbl+0x74
    virtual bool GetDownloadedLeaderboardEntry(SteamLeaderboardEntries_t hSteamLeaderboardEntries, int index, LeaderboardEntry_t* pLeaderboardEntry, int* pDetails, int cDetailsMax) = 0; // vtbl+0x78
    virtual SteamAPICall_t UploadLeaderboardScore(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int nScore, const int* pScoreDetails, int cScoreDetailsCount) = 0; // vtbl+0x7C
    virtual SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t hSteamLeaderboard, UGCHandle_t hUGC) = 0; // vtbl+0x80
    virtual SteamAPICall_t GetNumberOfCurrentPlayers() = 0; // vtbl+0x84
    virtual SteamAPICall_t RequestGlobalAchievementPercentages() = 0; // vtbl+0x88
    virtual int GetMostAchievedAchievementInfo(char* pchName, uint32 unNameBufLen, float* pflPercent, bool* pbAchieved) = 0; // vtbl+0x8C
    virtual int GetNextMostAchievedAchievementInfo(int iIteratorPrevious, char* pchName, uint32 unNameBufLen, float* pflPercent, bool* pbAchieved) = 0; // vtbl+0x90
    virtual bool GetAchievementAchievedPercent(const char* pchName, float* pflPercent) = 0; // vtbl+0x94
    virtual SteamAPICall_t RequestGlobalStats(int nHistoryDays) = 0; // vtbl+0x98
    virtual bool GetGlobalStat(const char* pchStatName, int64_t* pData) = 0; // vtbl+0xA0
    virtual bool GetGlobalStat(const char* pchStatName, double* pData) = 0; // vtbl+0x9C
    virtual int GetGlobalStatHistory(const char* pchStatName, int64_t* pData, uint32 cubData) = 0; // vtbl+0xA8
    virtual int GetGlobalStatHistory(const char* pchStatName, double* pData, uint32 cubData) = 0; // vtbl+0xA4

    // 0x00 vfptr
};

// size 0x18
struct UserStatsReceived_t
{
    enum
    {
        k_iCallback = 1101,
    };

    uint64_t m_nGameID; // 0x00
    EResult m_eResult; // 0x08
    CSteamID m_steamIDUser; // 0x0C
};

// size 0x10
struct UserStatsStored_t
{
    enum
    {
        k_iCallback = 1102,
    };

    uint64_t m_nGameID; // 0x00
    EResult m_eResult; // 0x08
};

// size 0x98
struct UserAchievementStored_t
{
    enum
    {
        k_iCallback = 1103,
    };

    uint64_t m_nGameID; // 0x00
    bool m_bGroupAchievement; // 0x08
    char m_rgchAchievementName[128]; // 0x09
    unsigned int m_nCurProgress; // 0x8C
    unsigned int m_nMaxProgress; // 0x90
};

// size 0x10
struct LeaderboardFindResult_t
{
    enum
    {
        k_iCallback = 1104,
    };

    uint64_t m_hSteamLeaderboard; // 0x00
    unsigned char m_bLeaderboardFound; // 0x08
};

// size 0x20
struct LeaderboardScoreUploaded_t
{
    enum
    {
        k_iCallback = 1106,
    };

    unsigned char m_bSuccess; // 0x00
    uint64_t m_hSteamLeaderboard; // 0x08
    int m_nScore; // 0x10
    unsigned char m_bScoreChanged; // 0x14
    int m_nGlobalRankNew; // 0x18
    int m_nGlobalRankPrevious; // 0x1C
};

// size 0x18
struct LeaderboardScoresDownloaded_t
{
    enum
    {
        k_iCallback = 1105,
    };

    uint64_t m_hSteamLeaderboard; // 0x00
    uint64_t m_hSteamLeaderboardEntries; // 0x08
    int m_cEntryCount; // 0x10
};
