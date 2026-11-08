// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamnetworking.h
//
// Recovery scope (seam): Steamworks SDK 1.42, third party; only what the keen modules hold by value. 
// Networking callbacks: a P2P session request from another user.
// stub_impl.md S2: ISteamNetworking (SteamNetworking005) from the PDB's vtable, its send types and session state.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

enum EP2PSend
{
    k_EP2PSendUnreliable = 0,
    k_EP2PSendUnreliableNoDelay = 1,
    k_EP2PSendReliable = 2,
    k_EP2PSendReliableWithBuffering = 3,
};

enum ESNetSocketConnectionType : int;

// size 0x14
struct P2PSessionState_t
{
    uint8 m_bConnectionActive; // 0x00
    uint8 m_bConnecting; // 0x01
    uint8 m_eP2PSessionError; // 0x02
    uint8 m_bUsingRelay; // 0x03
    int m_nBytesQueuedForSend; // 0x04
    int m_nPacketsQueuedForSend; // 0x08
    uint32 m_nRemoteIP; // 0x0C
    uint16 m_nRemotePort; // 0x10
};

typedef uint32 SNetSocket_t;
typedef uint32 SNetListenSocket_t;

// size 0x4
class ISteamNetworking
{
public:
    virtual bool SendP2PPacket(CSteamID steamIDRemote, const void* pubData, uint32 cubData, EP2PSend eP2PSendType, int nChannel) = 0; // vtbl+0x00
    virtual bool IsP2PPacketAvailable(uint32* pcubMsgSize, int nChannel) = 0; // vtbl+0x04
    virtual bool ReadP2PPacket(void* pubDest, uint32 cubDest, uint32* pcubMsgSize, CSteamID* psteamIDRemote, int nChannel) = 0; // vtbl+0x08
    virtual bool AcceptP2PSessionWithUser(CSteamID steamIDRemote) = 0; // vtbl+0x0C
    virtual bool CloseP2PSessionWithUser(CSteamID steamIDRemote) = 0; // vtbl+0x10
    virtual bool CloseP2PChannelWithUser(CSteamID steamIDRemote, int nChannel) = 0; // vtbl+0x14
    virtual bool GetP2PSessionState(CSteamID steamIDRemote, P2PSessionState_t* pConnectionState) = 0; // vtbl+0x18
    virtual bool AllowP2PPacketRelay(bool bAllow) = 0; // vtbl+0x1C
    virtual SNetListenSocket_t CreateListenSocket(int nVirtualP2PPort, uint32 nIP, uint16 nPort, bool bAllowUseOfPacketRelay) = 0; // vtbl+0x20
    virtual SNetSocket_t CreateP2PConnectionSocket(CSteamID steamIDTarget, int nVirtualPort, int nTimeoutSec, bool bAllowUseOfPacketRelay) = 0; // vtbl+0x24
    virtual SNetSocket_t CreateConnectionSocket(uint32 nIP, uint16 nPort, int nTimeoutSec) = 0; // vtbl+0x28
    virtual bool DestroySocket(SNetSocket_t hSocket, bool bNotifyRemoteEnd) = 0; // vtbl+0x2C
    virtual bool DestroyListenSocket(SNetListenSocket_t hSocket, bool bNotifyRemoteEnd) = 0; // vtbl+0x30
    virtual bool SendDataOnSocket(SNetSocket_t hSocket, void* pubData, uint32 cubData, bool bReliable) = 0; // vtbl+0x34
    virtual bool IsDataAvailableOnSocket(SNetSocket_t hSocket, uint32* pcubMsgSize) = 0; // vtbl+0x38
    virtual bool RetrieveDataFromSocket(SNetSocket_t hSocket, void* pubDest, uint32 cubDest, uint32* pcubMsgSize) = 0; // vtbl+0x3C
    virtual bool IsDataAvailable(SNetListenSocket_t hListenSocket, uint32* pcubMsgSize, SNetSocket_t* phSocket) = 0; // vtbl+0x40
    virtual bool RetrieveData(SNetListenSocket_t hListenSocket, void* pubDest, uint32 cubDest, uint32* pcubMsgSize, SNetSocket_t* phSocket) = 0; // vtbl+0x44
    virtual bool GetSocketInfo(SNetSocket_t hSocket, CSteamID* pSteamIDRemote, int* peSocketStatus, uint32* punIPRemote, uint16* punPortRemote) = 0; // vtbl+0x48
    virtual bool GetListenSocketInfo(SNetListenSocket_t hListenSocket, uint32* pnIP, uint16* pnPort) = 0; // vtbl+0x4C
    virtual ESNetSocketConnectionType GetSocketConnectionType(SNetSocket_t hSocket) = 0; // vtbl+0x50
    virtual int GetMaxPacketSize(SNetSocket_t hSocket) = 0; // vtbl+0x54

    // 0x00 vfptr
};

// size 0x8
struct P2PSessionRequest_t
{
    enum
    {
        k_iCallback = 1202,
    };

    CSteamID m_steamIDRemote; // 0x00
};
