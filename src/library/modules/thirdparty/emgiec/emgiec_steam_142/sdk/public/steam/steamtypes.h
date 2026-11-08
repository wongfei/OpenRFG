// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): the integer typedefs the interfaces are declared with (the same
// global typedefs as keen/base/types.hpp, which the PDB records as ::uint32 etc.), the pipe/user handles and the
// asynchronous call handle. The SDK's int8/int16/int32/int64 are left out (nothing here uses them, and they would be
// global names for every TU of the game).
#pragma once

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef short sint16;
typedef long long sint64;

typedef int HSteamPipe;
typedef int HSteamUser;
typedef uint64 SteamAPICall_t;
typedef uint32 AppId_t;
typedef uint64 SteamLeaderboard_t;
typedef uint64 SteamLeaderboardEntries_t;
typedef uint64 UGCHandle_t;
typedef uint32 HAuthTicket;
const HAuthTicket k_HAuthTicketInvalid = 0;
typedef uint64 ControllerHandle_t;
typedef uint64 ControllerActionSetHandle_t;
typedef uint64 ControllerDigitalActionHandle_t;
typedef uint64 ControllerAnalogActionHandle_t;

// k_uAPICallInvalid
const SteamAPICall_t k_uAPICallInvalid = 0;
