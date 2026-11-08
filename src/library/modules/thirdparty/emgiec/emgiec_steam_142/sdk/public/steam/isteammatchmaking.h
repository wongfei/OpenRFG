// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteammatchmaking.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// Matchmaking callbacks: lobby list results, lobby created/entered, lobby data and member updates; the lobby types.
#pragma once

#include <stdint.h>
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

enum ELobbyType
{
    k_ELobbyTypePrivate = 0,
    k_ELobbyTypeFriendsOnly = 1,
    k_ELobbyTypePublic = 2,
    k_ELobbyTypeInvisible = 3,
};

// size 0x4
struct LobbyMatchList_t
{
    enum
    {
        k_iCallback = 510,
    };

    unsigned int m_nLobbiesMatching; // 0x00
};

// size 0x10
struct LobbyCreated_t
{
    enum
    {
        k_iCallback = 513,
    };

    EResult m_eResult; // 0x00
    uint64_t m_ulSteamIDLobby; // 0x08
};

// size 0x18
struct LobbyEnter_t
{
    enum
    {
        k_iCallback = 504,
    };

    uint64_t m_ulSteamIDLobby; // 0x00
    unsigned int m_rgfChatPermissions; // 0x08
    bool m_bLocked; // 0x0C
    unsigned int m_EChatRoomEnterResponse; // 0x10
};

// size 0x18
struct LobbyDataUpdate_t
{
    enum
    {
        k_iCallback = 505,
    };

    uint64_t m_ulSteamIDLobby; // 0x00
    uint64_t m_ulSteamIDMember; // 0x08
    unsigned char m_bSuccess; // 0x10
};

// size 0x20
struct LobbyChatUpdate_t
{
    enum
    {
        k_iCallback = 506,
    };

    uint64_t m_ulSteamIDLobby; // 0x00
    uint64_t m_ulSteamIDUserChanged; // 0x08
    uint64_t m_ulSteamIDMakingChange; // 0x10
    unsigned int m_rgfChatMemberStateChange; // 0x18
};

enum ELobbyComparison
{
    k_ELobbyComparisonEqualToOrLessThan = -2,
    k_ELobbyComparisonLessThan = -1,
    k_ELobbyComparisonEqual = 0,
    k_ELobbyComparisonGreaterThan = 1,
    k_ELobbyComparisonEqualToOrGreaterThan = 2,
    k_ELobbyComparisonNotEqual = 3,
};

enum ELobbyDistanceFilter
{
    k_ELobbyDistanceFilterClose = 0,
    k_ELobbyDistanceFilterDefault = 1,
    k_ELobbyDistanceFilterFar = 2,
    k_ELobbyDistanceFilterWorldwide = 3,
};

// The Steam matchmaking interface (SteamMatchMaking009), from the PDB's vtable (stub_impl.md S2; up to SetLobbyData in
// multiplayer.md MP3).
class ISteamMatchmaking
{
public:
    virtual int GetFavoriteGameCount() = 0; // vtbl+0x00
    virtual bool GetFavoriteGame(int iGame, uint32_t* pnAppID, uint32_t* pnIP, uint16_t* pnConnPort, uint16_t* pnQueryPort, uint32_t* punFlags, uint32_t* pRTime32LastPlayedOnServer) = 0; // vtbl+0x04
    virtual int AddFavoriteGame(uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32LastPlayedOnServer) = 0; // vtbl+0x08
    virtual bool RemoveFavoriteGame(uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags) = 0; // vtbl+0x0C
    virtual uint64_t RequestLobbyList() = 0; // vtbl+0x10
    virtual void AddRequestLobbyListStringFilter(const char* pchKeyToMatch, const char* pchValueToMatch, ELobbyComparison eComparisonType) = 0; // vtbl+0x14
    virtual void AddRequestLobbyListNumericalFilter(const char* pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType) = 0; // vtbl+0x18
    virtual void AddRequestLobbyListNearValueFilter(const char* pchKeyToMatch, int nValueToBeCloseTo) = 0; // vtbl+0x1C
    virtual void AddRequestLobbyListFilterSlotsAvailable(int nSlotsAvailable) = 0; // vtbl+0x20
    virtual void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter eLobbyDistanceFilter) = 0; // vtbl+0x24
    virtual void AddRequestLobbyListResultCountFilter(int cMaxResults) = 0; // vtbl+0x28
    virtual void AddRequestLobbyListCompatibleMembersFilter(CSteamID steamIDLobby) = 0; // vtbl+0x2C
    virtual CSteamID GetLobbyByIndex(int iLobby) = 0; // vtbl+0x30
    virtual uint64_t CreateLobby(ELobbyType eLobbyType, int cMaxMembers) = 0; // vtbl+0x34
    virtual uint64_t JoinLobby(CSteamID steamIDLobby) = 0; // vtbl+0x38
    virtual void LeaveLobby(CSteamID steamIDLobby) = 0; // vtbl+0x3C
    virtual bool InviteUserToLobby(CSteamID steamIDLobby, CSteamID steamIDInvitee) = 0; // vtbl+0x40
    virtual int GetNumLobbyMembers(CSteamID steamIDLobby) = 0; // vtbl+0x44
    virtual CSteamID GetLobbyMemberByIndex(CSteamID steamIDLobby, int iMember) = 0; // vtbl+0x48
    virtual const char* GetLobbyData(CSteamID steamIDLobby, const char* pchKey) = 0; // vtbl+0x4C
    virtual bool SetLobbyData(CSteamID steamIDLobby, const char* pchKey, const char* pchValue) = 0; // vtbl+0x50
    virtual int GetLobbyDataCount(CSteamID steamIDLobby) = 0; // vtbl+0x54
    virtual bool GetLobbyDataByIndex(CSteamID steamIDLobby, int iLobbyData, char* pchKey, int cchKeyBufferSize, char* pchValue, int cchValueBufferSize) = 0; // vtbl+0x58
    virtual bool DeleteLobbyData(CSteamID steamIDLobby, const char* pchKey) = 0; // vtbl+0x5C
    virtual const char* GetLobbyMemberData(CSteamID steamIDLobby, CSteamID steamIDUser, const char* pchKey) = 0; // vtbl+0x60
    virtual void SetLobbyMemberData(CSteamID steamIDLobby, const char* pchKey, const char* pchValue) = 0; // vtbl+0x64
    virtual bool SendLobbyChatMsg(CSteamID steamIDLobby, const void* pvMsgBody, int cubMsgBody) = 0; // vtbl+0x68
    virtual int GetLobbyChatEntry(CSteamID steamIDLobby, int iChatID, CSteamID* pSteamIDUser, void* pvData, int cubData, EChatEntryType* peChatEntryType) = 0; // vtbl+0x6C
    virtual bool RequestLobbyData(CSteamID steamIDLobby) = 0; // vtbl+0x70
    virtual void SetLobbyGameServer(CSteamID steamIDLobby, uint32_t unGameServerIP, uint16_t unGameServerPort, CSteamID steamIDGameServer) = 0; // vtbl+0x74
    virtual bool GetLobbyGameServer(CSteamID steamIDLobby, uint32_t* punGameServerIP, uint16_t* punGameServerPort, CSteamID* psteamIDGameServer) = 0; // vtbl+0x78
    virtual bool SetLobbyMemberLimit(CSteamID steamIDLobby, int cMaxMembers) = 0; // vtbl+0x7C
    virtual int GetLobbyMemberLimit(CSteamID steamIDLobby) = 0; // vtbl+0x80
    virtual bool SetLobbyType(CSteamID steamIDLobby, ELobbyType eLobbyType) = 0; // vtbl+0x84
    virtual bool SetLobbyJoinable(CSteamID steamIDLobby, bool bLobbyJoinable) = 0; // vtbl+0x88
    virtual CSteamID GetLobbyOwner(CSteamID steamIDLobby) = 0; // vtbl+0x8C
    virtual bool SetLobbyOwner(CSteamID steamIDLobby, CSteamID steamIDNewOwner) = 0; // vtbl+0x90
    virtual bool SetLinkedLobby(CSteamID steamIDLobby, CSteamID steamIDLobbyDependent) = 0; // vtbl+0x94
};

// The accessor SteamMatchmaking() (CSteamAPIContext::m_pSteamMatchmaking, +0x10) is inline in steam_api_internal.h.
