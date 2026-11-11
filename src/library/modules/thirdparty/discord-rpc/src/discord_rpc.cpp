// Stand-in for discord-rpc.dll (discord_rpc.h), new code: built instead of linking lib/win32/discord-rpc.lib when
// CMake's OPENRFG_WITH_CUSTOM_DISCORD is on. Discord is never reached: nothing connects, no presence is shown, and
// none of the event handlers given to Discord_Initialize is ever called (no ready, no join request). Every caller
// ignores the results (keen::rfg::Application, multi_core_platform.cpp), so the game runs as without the Discord
// client. The program does not need discord-rpc.dll next to it.
//
// Only the 5 functions rfg.exe imports are provided (discord_rpc.h); the DLL's other exports are not used.
#include "library/modules/thirdparty/discord-rpc/include/discord_rpc.h"

extern "C"
{
    void Discord_Initialize(const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId)
    {
        (void)applicationId;
        (void)handlers;
        (void)autoRegister;
        (void)optionalSteamId;
    }

    void Discord_Shutdown(void)
    {
    }

    void Discord_RunCallbacks(void)
    {
    }

    void Discord_Respond(const char* userid, int reply)
    {
        (void)userid;
        (void)reply;
    }

    void Discord_UpdatePresence(const DiscordRichPresence* presence)
    {
        (void)presence;
    }
}
