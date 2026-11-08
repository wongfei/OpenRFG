// The multiplayer command line of OpenRFG (orig_main.md deviation 3; new code, not recovered). See main_options.h.

// winsock2 before any windows.h
#include <winsock2.h>
#include <ws2tcpip.h>

#include "main_audio_log.h"
#include "main_options.h"
#include "stub_trace.h"

#include "volition/rfg/code/gamewide/game_trace.h"

#include "game/sources/application.hpp"
#include "keen/network/networkaddress.hpp"
#include "library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#if defined(OPENRFG_UI)
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_startup_video.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

namespace
{
    main_options Options;
    bool Parsed;

    void report(const char* format, const char* value)
    {
        if (Options.error[0] == '\0')
        {
            snprintf(Options.error, sizeof(Options.error), format, value);
        }
    }

    // A decimal integer in [low, high], else reported (the value stays as it was).
    bool read_int(const char* option, const char* text, int low, int high, int& value)
    {
        char* end = nullptr;
        const long parsed = strtol(text, &end, 10);
        if (end == text || *end != '\0' || parsed < low || parsed > high)
        {
            char what[128];
            snprintf(what, sizeof(what), "%s: bad value \"%%s\"", option);
            report(what, text);
            return false;
        }
        value = (int)parsed;
        return true;
    }

    // The argument vector with "-username NAME" appended (keen's parseArguments reads it).
    void add_user_name(const char* name)
    {
        for (int i = 1; i < __argc; i++)
        {
            if (strcmp(__argv[i], "-username") == 0)
            {
                return;
            }
        }
        // Deviation from rfg.exe: --name adds -username NAME, so that the user account stand-in has one local
        // account without Steam.
        // Magic number: room for the program's arguments and the two added.
        static char* arguments[128];
        int count = 0;
        for (int i = 0; i < __argc && count < 125; i++)
        {
            arguments[count++] = __argv[i];
        }
        arguments[count++] = (char*)"-username";
        arguments[count++] = (char*)name;
        arguments[count] = nullptr;
        __argc = count;
        __argv = arguments;
    }

    // True when nothing on this machine has the UDP port bound (an exclusive bind on any address succeeds).
    bool udp_port_free(unsigned short port)
    {
        const SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (probe == INVALID_SOCKET)
        {
            return true;
        }
        BOOL exclusive = TRUE;
        setsockopt(probe, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        const bool free = bind(probe, (const sockaddr*)&address, sizeof(address)) == 0;
        closesocket(probe);
        return free;
    }

    // A loopback address or one of this machine's own addresses.
    bool is_own_address(unsigned int ip4_network_order)
    {
        if ((ntohl(ip4_network_order) >> 24) == 127)
        {
            return true;
        }
        char host[256];
        if (gethostname(host, sizeof(host)) != 0)
        {
            return false;
        }
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        addrinfo* list = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &list) != 0)
        {
            return false;
        }
        bool own = false;
        for (const addrinfo* entry = list; entry != nullptr && !own; entry = entry->ai_next)
        {
            own = ((const sockaddr_in*)entry->ai_addr)->sin_addr.s_addr == ip4_network_order;
        }
        freeaddrinfo(list);
        return own;
    }
}

void main_options_parse()
{
    if (Parsed)
    {
        return;
    }
    Parsed = true;
    for (int i = 1; i < __argc; i++)
    {
        const char* arg = __argv[i];
        const char* value = i + 1 < __argc ? __argv[i + 1] : nullptr;
        // The option with two values.
        if (strcmp(arg, "--window-pos") == 0)
        {
            if (i + 2 >= __argc)
            {
                report("%s: the X and Y values are missing", arg);
                continue;
            }
            // Magic numbers: Win32 window coordinates are 16 bit (a negative value is a monitor left of or above the primary).
            int x = 0;
            int y = 0;
            if (read_int(arg, __argv[i + 1], -32768, 32767, x) && read_int(arg, __argv[i + 2], -32768, 32767, y))
            {
                g_openrfgWindowPosSet = true;
                g_openrfgWindowPosX = x;
                g_openrfgWindowPosY = y;
            }
            i += 2;
            continue;
        }
        if (strncmp(arg, "--sp-pos=", 9) == 0)
        {
            float x;
            float y;
            float z;
            if (sscanf(arg + 9, "%f,%f,%f", &x, &y, &z) != 3)
            {
                report("--sp-pos: bad value \"%s\"", arg + 9);
                continue;
            }
            g_openrfgStartPos.x = x;
            g_openrfgStartPos.y = y;
            g_openrfgStartPos.z = z;
            g_openrfgStartPosSet = true;
            continue;
        }
        // The options with a value.
        const bool takes_value = strcmp(arg, "--map") == 0 || strcmp(arg, "--players") == 0 || strcmp(arg, "--ip") == 0 || strcmp(arg, "--port") == 0 ||
                                 strcmp(arg, "--name") == 0 || strcmp(arg, "--point-limit") == 0 || strcmp(arg, "--time-limit") == 0 ||
                                 strcmp(arg, "--rounds") == 0 || strcmp(arg, "--results-time") == 0 || strcmp(arg, "--audio-log") == 0 ||
                                 strcmp(arg, "--stub-trace") == 0 || strcmp(arg, "--game-trace") == 0 || strcmp(arg, "--sp-zone") == 0 ||
                                 strcmp(arg, "--save-dir") == 0 || strcmp(arg, "--sp-load") == 0;
        if (takes_value && value == nullptr)
        {
            report("%s: the value is missing", arg);
            continue;
        }
        if (strcmp(arg, "--host") == 0)
        {
            Options.host = true;
        }
        else if (strcmp(arg, "--join") == 0)
        {
            Options.join = true;
        }
        // Deviation from rfg.exe: --sp and --sp-zone start a single player game directly, the same call as
        // video_cutscene_exit_state without the movie.
        else if (strcmp(arg, "--sp") == 0)
        {
            Options.sp = true;
        }
        else if (strcmp(arg, "--sp-zone") == 0)
        {
            Options.sp_zone = value;
        }
        else if (strcmp(arg, "--sp-load") == 0)
        {
            // Magic number: the save file has 12 slots (game_save_generic.cpp's Num_save_slots).
            read_int("--sp-load", value, 0, 11, Options.sp_load);
        }
        else if (strcmp(arg, "--save-dir") == 0)
        {
            g_openrfgSaveDirectory = value;
            // The folder is only used by the local saves: without this, a scratch folder would be ignored and the run
            // would write to the user's real Steam Cloud saves.
            g_openrfgSteamCloudSaves = false;
        }
        else if (strcmp(arg, "--no-steam-cloud-saves") == 0)
        {
            g_openrfgSteamCloudSaves = false;
        }
        else if (strcmp(arg, "--search") == 0)
        {
            Options.search = true;
        }
        else if (strcmp(arg, "--notitle") == 0)
        {
            Options.notitle = true;
        }
        else if (strcmp(arg, "--no-steam") == 0)
        {
            g_openrfgUseSteam = false;
        }
        else if (strcmp(arg, "--map") == 0)
        {
            Options.map = value;
        }
        else if (strcmp(arg, "--name") == 0)
        {
            if (value[0] == '\0')
            {
                report("--name: the name is empty%s", "");
            }
            else
            {
                Options.name = value;
            }
        }
        else if (strcmp(arg, "--ip") == 0)
        {
            in_addr address;
            if (inet_pton(AF_INET, value, &address) != 1)
            {
                report("--ip: \"%s\" is not an IPv4 address", value);
            }
            else
            {
                strncpy_s(Options.ip, value, _TRUNCATE);
            }
        }
        else if (strcmp(arg, "--port") == 0)
        {
            int port = Options.port;
            read_int("--port", value, 1, 65535, port);
            Options.port = (unsigned short)port;
        }
        else if (strcmp(arg, "--players") == 0)
        {
            // Magic number: rfg.exe's LAN lobbies have 16 slots.
            read_int("--players", value, 1, 16, Options.players);
        }
        else if (strcmp(arg, "--point-limit") == 0)
        {
            read_int("--point-limit", value, 0, 65535, Options.point_limit);
        }
        else if (strcmp(arg, "--time-limit") == 0)
        {
            read_int("--time-limit", value, 0, 255, Options.time_limit);
        }
        else if (strcmp(arg, "--rounds") == 0)
        {
            read_int("--rounds", value, 1, 255, Options.rounds);
        }
        else if (strcmp(arg, "--results-time") == 0)
        {
            // Magic number: an hour at most.
            read_int("--results-time", value, 0, 3600, Options.results_time);
        }
        else if (strcmp(arg, "--audio-log") == 0)
        {
            // SFX.md deviation 3.1: the hooks go in before the boot looks up any cue.
            if (!main_audio_log_start(value))
            {
                report("--audio-log: cannot write \"%s\" (or no audio library)", value);
            }
        }
        else if (strcmp(arg, "--stub-trace") == 0)
        {
            // stub_impl.md S0: the stand-ins reached (stub_trace.h); the ones reached before this point are written too.
            if (!stub_trace_open(value))
            {
                report("--stub-trace: cannot write \"%s\"", value);
            }
        }
        else if (strcmp(arg, "--game-trace") == 0)
        {
            // The game trace (game_trace.h), with the first call of each stand-in.
            if (game_trace_open(value))
            {
                Stub_trace_hit_hook = [](const char* name) { GAME_TRACE("stub", "%s", name); };
            }
            else
            {
                report("--game-trace: cannot write \"%s\"", value);
            }
        }
        else
        {
            continue;
        }
        if (takes_value)
        {
            i++;
        }
    }
    if ((Options.host ? 1 : 0) + (Options.join ? 1 : 0) + (Options.sp ? 1 : 0) + (Options.sp_load >= 0 ? 1 : 0) > 1)
    {
        report("--host, --join, --sp and --sp-load%s: only one of them", "");
    }
    if (Options.error[0] != '\0')
    {
        Options.host = false;
        Options.join = false;
        Options.sp = false;
        Options.sp_load = -1;
    }
    if (Options.name != nullptr)
    {
        add_user_name(Options.name);
    }
    if (g_openrfgUseSteam)
    {
        // stub_impl.md S2 deviation 1: the original Steam start-up. Outside the Steam client's launch, SteamAPI_Init
        // takes the app id from the environment (rfg.exe: 667720), as steam_appid.txt would give it.
        _putenv_s("SteamAppId", "667720");
        _putenv_s("SteamGameId", "667720");
    }
    // Without it a system link match ends at its start (multiplayer.md MP4, multi_core_session_base.h).
    g_openrfgSkipSystemLinkTableHashCheck = true;
#if defined(OPENRFG_UI)
    // UI.md deviation 4: --notitle skips the title screen; --host/--join/--sp/--sp-load skip it too (they skip the main
    // menu).
    g_openrfgMainMenuNoTitle = Options.notitle || Options.host || Options.join || Options.sp || Options.sp_load >= 0;
    g_openrfgSkipStartupVideos = g_openrfgMainMenuNoTitle;
#endif
}

const main_options& main_options_get()
{
    return Options;
}

int main_options_results_time()
{
    if (Options.results_time >= 0)
    {
        return Options.results_time;
    }
    // Magic number: 10 s of results when nobody sits at the program.
    return Options.host || Options.join ? 10 : 0;
}

unsigned short main_options_choose_lan_port(bool host)
{
    if (host)
    {
        return Options.port;
    }
    // Deviation from rfg.exe: it has one fixed port. A client takes the first free UDP port from --port up,
    // so that several instances can run on one machine.
    // Magic number: up to 16 instances on one machine.
    for (unsigned int port = Options.port; port < (unsigned int)Options.port + 16 && port <= 65535; port++)
    {
        if (udp_port_free((unsigned short)port))
        {
            return (unsigned short)port;
        }
    }
    return Options.port;
}

void main_options_direct_lobby(unsigned int* ip4_network_order, unsigned short* port)
{
    in_addr address;
    inet_pton(AF_INET, Options.ip, &address);
    *ip4_network_order = address.s_addr;
    *port = Options.port;
    // The loopback replacement (deviation 3): the LAN link names stations by the address their packets come from, and a
    // client joined over 127.0.0.1 is listed by the host under another address and dropped (multiplayer.md MP2). A
    // host on this machine is reached under its own network address (the one its lobby id carries).
    if (is_own_address(address.s_addr))
    {
        keen::NetworkAddress own;
        if (kaiko::gamelink::openrfgGetLanOwnHostAddress(&own))
        {
            *ip4_network_order = own.ip4Address;
        }
    }
}
