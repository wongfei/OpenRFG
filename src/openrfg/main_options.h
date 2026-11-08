// The multiplayer command line of OpenRFG (orig_main.md section 3, deviation 3; new code, not recovered). rfg.exe has
// no such options: its multiplayer menus take the settings from the player. OpenRFG's command line drives
// the original menus with them (main_ui_command_line.cpp).
//
//   --map NAME         the level (a file name such as mp_quarantine, or a level's display name); default mp_quarantine
//   --players N        the host launches when N players are in the lobby (1-16); default 2
//   --ip ADDR          the host's IPv4 address for Join (a direct join); default 127.0.0.1. A loopback or own address
//                      is replaced by the host's own network address (MP2's loopback finding)
//   --port N           the host's port (the LAN matchmaking port); default 1005
//   --search           Join searches the LAN (the server browser) and joins the first lobby found instead
//   --name NAME        the local user name (the framework's -username: the lobby's player name); default -username,
//                      else %USERNAME%
//   --host / --join    skip the main menu: after the boot the program presses Host or Join itself (and the title
//                      screen, as --notitle)
//   --notitle          the original UI (UI.md deviation 4): the title screen catches the local player at once
//   --sp               skip the main menu and start a new single player game without its opening movie (and the title
//                      screen, as --notitle; stub_impl.md S9)
//   --sp-zone NAME     the zone --sp starts in; default rfg.exe's new game zone, terr01_03_12.rfgzone_pc
//   --sp-pos=X,Y,Z     where --sp puts the player instead of a zone's player start (to reach a place that spawns
//                      something)
//   --sp-load N        skip the start-up videos, the title screen and the main menu and load the single player save in
//                      slot N (0-11, the save file's slot index), as the main menu's Load Game screen does when that
//                      slot is selected. A slot that screen would not list (empty, another campaign) is reported
//   --save-dir DIR     keep the saves in this folder, as --no-steam-cloud-saves does in its default
//                      <roaming app data>/OpenRFG/save (so a scratch folder never writes to the real saves)
//   --no-steam-cloud-saves  keep the saves local (application.cpp deviation 4). By default they go through Steam's
//                      remote storage as in rfg.exe, onto the user's real saves
//   --no-steam         skip the original Steam start-up (GameFrameworkParameters::useSteam, stub_impl.md S2), which needs the
//                      Steam client running and signed in and sets SteamAppId=667720. With it there are no Steam user
//                      accounts, saves, achievements or internet lobbies (LAN only)
//   --point-limit N, --time-limit MIN, --rounds N   the mode's limits (default: the mode's)
//   --results-time S   the results screen continues by itself after S seconds (0: it waits for the player); default
//                      10 with --host/--join, else 0
//   --audio-log FILE   a line per audiolib_cue_play result (SFX.md deviation 3.1, main_audio_log.h); OpenRFG only
//   --stub-trace FILE  the stand-ins (src/stubs) the program reached: a "hit NAME" line at each one's first call, a
//                      "calls N NAME" line per stand-in at exit (stub_impl.md S0, stub_trace.h)
//   --game-trace FILE  a line per game state change, world load, mission step, script fired, script action, event,
//                      objective, dialogue line, cutscene, player death and first call of a stand-in, a status line
//                      every 5 s and a line for a crash (game_trace.h, main_game_trace.cpp)
//   --window-pos X Y   the top left corner of the window's frame in physical desktop pixels, whatever the DPI scaling
//                      (negative on a monitor left of or above the primary one); default rfg.exe's (60, 60) client area.
//                      No effect in fullscreen
//
// A bad value is reported in a message box at the main menu, and --host/--join are then ignored.
#pragma once

struct main_options
{
    const char* map = "mp_quarantine";
    int players = 1;
    char ip[64] = "127.0.0.1";
    unsigned short port = 1005;
    bool search = false;
    bool notitle = false; // UI.md deviation 4 (OpenRFG with the original UI)
    const char* name = nullptr;
    bool host = false;
    bool join = false;
    bool sp = false; // stub_impl.md S9
    const char* sp_zone = "terr01_03_12.rfgzone_pc"; // main_menu_process_delayed_selection's new game zone
    int sp_load = -1; // the save slot --sp-load loads, -1: none
    int point_limit = -1;
    int time_limit = -1;
    int rounds = -1;
    int results_time = -1; // -1: the default (10 s with --host/--join, else 0)
    char error[256] = {}; // the first bad value, empty when all are good
};

// Parses the CRT's argument vector once (__argc/__argv, before WinMain reads it). --name is passed on to the framework
// as -username NAME (the argument vector is extended), unless -username is there already. The system link table hash
// check skip (multiplayer.md MP4) is set here too.
void main_options_parse();

const main_options& main_options_get();

// The seconds the results screen waits before it continues by itself (0: it waits for the player).
int main_options_results_time();

// The LAN port this instance binds (GameLinkLan's local port, the multiplayer.md 2.3 deviation, set before the
// multiplayer start-up): the host binds --port. A joining instance binds --port too when it is free (two machines:
// the original addressing), else the next free port above it (a second instance on the same machine).
unsigned short main_options_choose_lan_port(bool host);

// The host's lobby for a direct join: --ip (a loopback or own address replaced by this machine's own network address)
// and --port, as GameLinkLan's lobby id (the host's network address).
void main_options_direct_lobby(unsigned int* ip4_network_order, unsigned short* port);
