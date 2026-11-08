// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamfriends.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// Friends callbacks: GameLobbyJoinRequested_t (the user accepted a lobby invite) and PersonaStateChange_t.
// stub_impl.md S2: ISteamFriends (SteamFriends015) from the PDB's vtable.
#pragma once

#include <stdint.h>
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum EPersonaState : int;
enum EFriendRelationship : int;
enum EOverlayToStoreFlag : int;

// size 0x18
struct FriendGameInfo_t
{
    CGameID m_gameID; // 0x00
    uint32 m_unGameIP; // 0x08
    uint16 m_usGamePort; // 0x0C
    uint16 m_usQueryPort; // 0x0E
    CSteamID m_steamIDLobby; // 0x10
};

// size 0x4
class ISteamFriends
{
public:
    virtual const char* GetPersonaName() = 0; // vtbl+0x00
    virtual SteamAPICall_t SetPersonaName(const char* pchPersonaName) = 0; // vtbl+0x04
    virtual EPersonaState GetPersonaState() = 0; // vtbl+0x08
    virtual int GetFriendCount(int iFriendFlags) = 0; // vtbl+0x0C
    virtual CSteamID GetFriendByIndex(int iFriend, int iFriendFlags) = 0; // vtbl+0x10
    virtual EFriendRelationship GetFriendRelationship(CSteamID steamIDFriend) = 0; // vtbl+0x14
    virtual EPersonaState GetFriendPersonaState(CSteamID steamIDFriend) = 0; // vtbl+0x18
    virtual const char* GetFriendPersonaName(CSteamID steamIDFriend) = 0; // vtbl+0x1C
    virtual bool GetFriendGamePlayed(CSteamID steamIDFriend, FriendGameInfo_t* pFriendGameInfo) = 0; // vtbl+0x20
    virtual const char* GetFriendPersonaNameHistory(CSteamID steamIDFriend, int iPersonaName) = 0; // vtbl+0x24
    virtual int GetFriendSteamLevel(CSteamID steamIDFriend) = 0; // vtbl+0x28
    virtual const char* GetPlayerNickname(CSteamID steamIDPlayer) = 0; // vtbl+0x2C
    virtual int GetFriendsGroupCount() = 0; // vtbl+0x30
    virtual sint16 GetFriendsGroupIDByIndex(int iFG) = 0; // vtbl+0x34
    virtual const char* GetFriendsGroupName(sint16 friendsGroupID) = 0; // vtbl+0x38
    virtual int GetFriendsGroupMembersCount(sint16 friendsGroupID) = 0; // vtbl+0x3C
    virtual void GetFriendsGroupMembersList(sint16 friendsGroupID, CSteamID* pOutSteamIDMembers, int nMembersCount) = 0; // vtbl+0x40
    virtual bool HasFriend(CSteamID steamIDFriend, int iFriendFlags) = 0; // vtbl+0x44
    virtual int GetClanCount() = 0; // vtbl+0x48
    virtual CSteamID GetClanByIndex(int iClan) = 0; // vtbl+0x4C
    virtual const char* GetClanName(CSteamID steamIDClan) = 0; // vtbl+0x50
    virtual const char* GetClanTag(CSteamID steamIDClan) = 0; // vtbl+0x54
    virtual bool GetClanActivityCounts(CSteamID steamIDClan, int* pnOnline, int* pnInGame, int* pnChatting) = 0; // vtbl+0x58
    virtual SteamAPICall_t DownloadClanActivityCounts(CSteamID* psteamIDClans, int cClansToRequest) = 0; // vtbl+0x5C
    virtual int GetFriendCountFromSource(CSteamID steamIDSource) = 0; // vtbl+0x60
    virtual CSteamID GetFriendFromSourceByIndex(CSteamID steamIDSource, int iFriend) = 0; // vtbl+0x64
    virtual bool IsUserInSource(CSteamID steamIDUser, CSteamID steamIDSource) = 0; // vtbl+0x68
    virtual void SetInGameVoiceSpeaking(CSteamID steamIDUser, bool bSpeaking) = 0; // vtbl+0x6C
    virtual void ActivateGameOverlay(const char* pchDialog) = 0; // vtbl+0x70
    virtual void ActivateGameOverlayToUser(const char* pchDialog, CSteamID steamID) = 0; // vtbl+0x74
    virtual void ActivateGameOverlayToWebPage(const char* pchURL) = 0; // vtbl+0x78
    virtual void ActivateGameOverlayToStore(AppId_t nAppID, EOverlayToStoreFlag eFlag) = 0; // vtbl+0x7C
    virtual void SetPlayedWith(CSteamID steamIDUserPlayedWith) = 0; // vtbl+0x80
    virtual void ActivateGameOverlayInviteDialog(CSteamID steamIDLobby) = 0; // vtbl+0x84
    virtual int GetSmallFriendAvatar(CSteamID steamIDFriend) = 0; // vtbl+0x88
    virtual int GetMediumFriendAvatar(CSteamID steamIDFriend) = 0; // vtbl+0x8C
    virtual int GetLargeFriendAvatar(CSteamID steamIDFriend) = 0; // vtbl+0x90
    virtual bool RequestUserInformation(CSteamID steamIDUser, bool bRequireNameOnly) = 0; // vtbl+0x94
    virtual SteamAPICall_t RequestClanOfficerList(CSteamID steamIDClan) = 0; // vtbl+0x98
    virtual CSteamID GetClanOwner(CSteamID steamIDClan) = 0; // vtbl+0x9C
    virtual int GetClanOfficerCount(CSteamID steamIDClan) = 0; // vtbl+0xA0
    virtual CSteamID GetClanOfficerByIndex(CSteamID steamIDClan, int iOfficer) = 0; // vtbl+0xA4
    virtual uint32 GetUserRestrictions() = 0; // vtbl+0xA8
    virtual bool SetRichPresence(const char* pchKey, const char* pchValue) = 0; // vtbl+0xAC
    virtual void ClearRichPresence() = 0; // vtbl+0xB0
    virtual const char* GetFriendRichPresence(CSteamID steamIDFriend, const char* pchKey) = 0; // vtbl+0xB4
    virtual int GetFriendRichPresenceKeyCount(CSteamID steamIDFriend) = 0; // vtbl+0xB8
    virtual const char* GetFriendRichPresenceKeyByIndex(CSteamID steamIDFriend, int iKey) = 0; // vtbl+0xBC
    virtual void RequestFriendRichPresence(CSteamID steamIDFriend) = 0; // vtbl+0xC0
    virtual bool InviteUserToGame(CSteamID steamIDFriend, const char* pchConnectString) = 0; // vtbl+0xC4
    virtual int GetCoplayFriendCount() = 0; // vtbl+0xC8
    virtual CSteamID GetCoplayFriend(int iCoplayFriend) = 0; // vtbl+0xCC
    virtual int GetFriendCoplayTime(CSteamID steamIDFriend) = 0; // vtbl+0xD0
    virtual AppId_t GetFriendCoplayGame(CSteamID steamIDFriend) = 0; // vtbl+0xD4
    virtual SteamAPICall_t JoinClanChatRoom(CSteamID steamIDClan) = 0; // vtbl+0xD8
    virtual bool LeaveClanChatRoom(CSteamID steamIDClan) = 0; // vtbl+0xDC
    virtual int GetClanChatMemberCount(CSteamID steamIDClan) = 0; // vtbl+0xE0
    virtual CSteamID GetChatMemberByIndex(CSteamID steamIDClan, int iUser) = 0; // vtbl+0xE4
    virtual bool SendClanChatMessage(CSteamID steamIDClanChat, const char* pchText) = 0; // vtbl+0xE8
    virtual int GetClanChatMessage(CSteamID steamIDClanChat, int iMessage, void* prgchText, int cchTextMax, EChatEntryType* peChatEntryType, CSteamID* psteamidChatter) = 0; // vtbl+0xEC
    virtual bool IsClanChatAdmin(CSteamID steamIDClanChat, CSteamID steamIDUser) = 0; // vtbl+0xF0
    virtual bool IsClanChatWindowOpenInSteam(CSteamID steamIDClanChat) = 0; // vtbl+0xF4
    virtual bool OpenClanChatWindowInSteam(CSteamID steamIDClanChat) = 0; // vtbl+0xF8
    virtual bool CloseClanChatWindowInSteam(CSteamID steamIDClanChat) = 0; // vtbl+0xFC
    virtual bool SetListenForFriendsMessages(bool bInterceptEnabled) = 0; // vtbl+0x100
    virtual bool ReplyToFriendMessage(CSteamID steamIDFriend, const char* pchMsgToSend) = 0; // vtbl+0x104
    virtual int GetFriendMessage(CSteamID steamIDFriend, int iMessageID, void* pvData, int cubData, EChatEntryType* peChatEntryType) = 0; // vtbl+0x108
    virtual SteamAPICall_t GetFollowerCount(CSteamID steamID) = 0; // vtbl+0x10C
    virtual SteamAPICall_t IsFollowing(CSteamID steamID) = 0; // vtbl+0x110
    virtual SteamAPICall_t EnumerateFollowingList(uint32 unStartIndex) = 0; // vtbl+0x114
    virtual bool IsClanPublic(CSteamID steamIDClan) = 0; // vtbl+0x118
    virtual bool IsClanOfficialGameGroup(CSteamID steamIDClan) = 0; // vtbl+0x11C

    // 0x00 vfptr
};

// size 0x10
struct GameLobbyJoinRequested_t
{
    enum
    {
        k_iCallback = 333,
    };

    CSteamID m_steamIDLobby; // 0x00
    CSteamID m_steamIDFriend; // 0x08
};

// size 0x10
struct PersonaStateChange_t
{
    enum
    {
        k_iCallback = 304,
    };

    uint64_t m_ulSteamID; // 0x00
    int m_nChangeFlags; // 0x08
};
