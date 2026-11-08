// discord-rpc (DISCORD-RPC.DLL, the public Discord rich presence library rfg.exe imports): the declarations the
// recovered code calls (multiplayer.md MP2), as the library's public header has them. Placement inferred.
#pragma once

#include <stdint.h>

extern "C"
{
    struct DiscordUser
    {
        const char* userId;
        const char* username;
        const char* discriminator;
        const char* avatar;
    };

    enum
    {
        DISCORD_REPLY_NO = 0,
        DISCORD_REPLY_YES = 1,
        DISCORD_REPLY_IGNORE = 2,
    };

    // size 0x48
    struct DiscordRichPresence
    {
        const char* state; // 0x00
        const char* details; // 0x04
        int64_t startTimestamp; // 0x08
        int64_t endTimestamp; // 0x10
        const char* largeImageKey; // 0x18
        const char* largeImageText; // 0x1C
        const char* smallImageKey; // 0x20
        const char* smallImageText; // 0x24
        const char* partyId; // 0x28
        int partySize; // 0x2C
        int partyMax; // 0x30
        const char* matchSecret; // 0x34
        const char* joinSecret; // 0x38
        const char* spectateSecret; // 0x3C
        int8_t instance; // 0x40
    };

    // size 0x18
    struct DiscordEventHandlers
    {
        void (*ready)(const DiscordUser* request); // 0x00
        void (*disconnected)(int errorCode, const char* message); // 0x04
        void (*errored)(int errorCode, const char* message); // 0x08
        void (*joinGame)(const char* joinSecret); // 0x0C
        void (*spectateGame)(const char* spectateSecret); // 0x10
        void (*joinRequest)(const DiscordUser* request); // 0x14
    };

    // orig_main.md OM3: keen::rfg::Application starts the library, runs its callbacks and shuts it down.
    // DISCORD-RPC.DLL::Discord_Initialize
    void Discord_Initialize(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId);
    // DISCORD-RPC.DLL::Discord_Shutdown
    void Discord_Shutdown(void);
    // DISCORD-RPC.DLL::Discord_RunCallbacks
    void Discord_RunCallbacks(void);
    // DISCORD-RPC.DLL::Discord_Respond: answers a join request
    void Discord_Respond(const char* userid, int reply);
    // DISCORD-RPC.DLL::Discord_UpdatePresence: the rich presence shown on the user's Discord profile
    void Discord_UpdatePresence(const DiscordRichPresence* presence);
}
