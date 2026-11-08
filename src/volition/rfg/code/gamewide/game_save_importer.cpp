// Original: volition/rfg/code/gamewide/game_save_importer.cpp
//
// Imports the saves of the original release of the game (Steam app 20500) from its folder under the Steam user's data:
// the directory file "savedir.rfad_pc" (a 64 byte text line per save), the saves "rfasave_NN.rfas_pc" and the profile
// "player_options.rfgo_pc". Everything here only reads those files.
//
// Deviation: OpenRFG with --no-steam has no Steam user, so getSaveGameImportDirectory answers an empty path (rfg.exe
// always has one, SteamUser would be null here) and the lookups find nothing.

#include "volition/rfg/code/gamewide/game_save_importer.h"

#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"
#include "volition/rfg/code/gamewide/rfgsaveinterface.hpp"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool g_openrfgUseSteam; // application.cpp, deviation 1

// Magic number: a 64 byte line per save in the directory file.
static const unsigned int Save_directory_line_size = 0x40;

// 0x0253C934, 0x0253C938 (function statics in rfg.exe)
static bool s_readDirectoryFromRegistry;
static keen::FilePath s_importDirectory;

// 0x007C37C0 (:13): "version_autosave_missions_activities_districts_territory_district_month_day_year_hours_minutes_seconds_dlc"
bool scan_game_info(game_save_info* pGame_info, const char* pString)
{
    int version = 0;
    char auto_save = 0;
    sscanf(pString, "%i_%c_%i_%i_%i_%i_%i_%i_%i_%i_%i_%i_%i_%i", &version, &auto_save, &pGame_info->num_missions_completed, &pGame_info->num_activities_completed,
           &pGame_info->districts_liberated, &pGame_info->territory_index, &pGame_info->district_index, &pGame_info->month, &pGame_info->day, &pGame_info->year,
           &pGame_info->hours, &pGame_info->minutes, &pGame_info->seconds, &pGame_info->dlc_id);
    // Magic number: the version of the original release's lines.
    if (version != 4)
    {
        pGame_info->used_slot = false;
        return false;
    }
    pGame_info->auto_save = auto_save == 't';
    pGame_info->used_slot = true;
    return true;
}

// 0x007D1410 (:39)
bool importSaveGameInfo(game_save_info* pInfo, unsigned int index, const char* pFolderName)
{
    keen::FilePath path(pFolderName, true);
    path.setFileNameWithExtension("savedir.rfad_pc");
    FILE* file = fopen(path.getCompletePath(), "rb");
    if (file == nullptr)
    {
        return false;
    }
    bool found = false;
    fseek(file, 0, SEEK_END);
    const unsigned int size = (unsigned int)ftell(file);
    if ((index + 1) * Save_directory_line_size <= size)
    {
        // The lines are not terminated: one byte more than rfg.exe's buffer. Deviation: rfg.exe's 64 bytes can run into
        // the next heap block when a line fills them.
        char line[Save_directory_line_size + 1] = {};
        fseek(file, index * Save_directory_line_size, SEEK_SET);
        fread(line, 1, Save_directory_line_size, file);
        found = scan_game_info(pInfo, line);
    }
    fclose(file);
    return found;
}

// 0x007D1540 (:78)
bool importSaveGame(keen::rfg::RfgSaveFile* pTargetData, unsigned int index, const char* pFolderName)
{
    memset(pTargetData, 0, sizeof(keen::rfg::RfgSaveFile));
    if (!importSaveGameInfo(&pTargetData->info, index, pFolderName))
    {
        return false;
    }
    keen::FilePath path(pFolderName, true);
    char file_name[64];
    keen::formatString(file_name, sizeof(file_name), "rfasave_%02d.rfas_pc", index);
    path.setFileNameWithExtension(file_name);
    FILE* file = fopen(path.getCompletePath(), "rb");
    if (file == nullptr)
    {
        return false;
    }
    fseek(file, 0, SEEK_END);
    const unsigned int size = (unsigned int)ftell(file);
    fseek(file, 0, SEEK_SET);
    const bool fits = size <= sizeof(pTargetData->data);
    if (fits)
    {
        fread(pTargetData->data, size, 1, file);
        pTargetData->size_used = (int)size;
        pTargetData->exists = true;
    }
    fclose(file);
    return fits;
}

// 0x007D1660 (:154): null target only checks the header. The offsets are those of the old file's layout.
bool importSaveGameProfile(keen::rfg::RfgSaveProfileData* pTargetData, const char* pFolderName)
{
    keen::FilePath path(pFolderName, true);
    path.setFileNameWithExtension("player_options.rfgo_pc");
    // Magic numbers: the file's header, then its data.
    const unsigned int read_size = pTargetData != nullptr ? 0xF2C : 8;
    unsigned char* buffer = (unsigned char*)malloc(read_size);
    if (buffer == nullptr)
    {
        return false;
    }
    bool valid = false;
    FILE* file = fopen(path.getCompletePath(), "rb");
    if (file != nullptr)
    {
        const size_t bytes_read = fread(buffer, 1, read_size, file);
        fclose(file);
        // Magic numbers: "PGFR", the file version, the largest size of its data.
        if (bytes_read >= 8 && *(const unsigned int*)buffer == 0x52464750 && *(const short*)(buffer + 4) == 0x29 && *(const unsigned short*)(buffer + 6) < 0xF2D)
        {
            if (pTargetData != nullptr)
            {
                const unsigned int* fields = (const unsigned int*)buffer;
                player_profile_data& profile = pTargetData->profile_data;
                memset(pTargetData, 0, sizeof(*pTargetData));
                profile.mp_xp = fields[2];
                profile.mp_bonus_xp = fields[3];
                profile.mp_playlist_id = fields[4];
                profile.mp_ffa_player_crc = fields[5];
                profile.mp_edf_player_crc = fields[6];
                profile.mp_gur_player_crc = fields[7];
                profile.mp_badge = fields[8];
                memcpy(&profile.mp_prefs_history, &fields[9], 0x50);
                profile.sledge_unique_id = fields[0x1D];
                memcpy(profile.tool_tip_info, &fields[0x177], 0x80);
                const bool completed = *(const bool*)(buffer + 0x65D);
                profile.new_data.game_completed_dlc = completed;
                profile.golden_hammer_unlocked = completed;
                profile.game_completed = completed;
                profile.wc_maps_unlocked = completed;
                const unsigned short stats_size = *(const unsigned short*)(buffer + 0x65E);
                profile.stats_buffer_size = stats_size;
                memcpy(profile.stats_buffer, &fields[0x198], stats_size);
            }
            valid = true;
        }
    }
    free(buffer);
    return valid;
}

// 0x007D1820 (:219): "<Steam folder>/userdata/<account id>/20500/remote", once from the registry.
const char* getSaveGameImportDirectory()
{
    if (!s_readDirectoryFromRegistry)
    {
        bool failed = true;
        HKEY software;
        // Deviation: no Steam user with --no-steam.
        if (g_openrfgUseSteam && RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &software) == ERROR_SUCCESS)
        {
            HKEY valve;
            if (RegOpenKeyExA(software, "Valve", 0, KEY_READ, &valve) == ERROR_SUCCESS)
            {
                HKEY steam;
                if (RegOpenKeyExA(valve, "Steam", 0, KEY_READ, &steam) == ERROR_SUCCESS)
                {
                    BYTE value[MAX_PATH];
                    DWORD value_size = sizeof(value);
                    DWORD value_type;
                    if (RegQueryValueExA(steam, "InstallPath", nullptr, &value_type, value, &value_size) == ERROR_SUCCESS && value_type == REG_SZ)
                    {
                        s_importDirectory.setCompletePath((const char*)value, true);
                        s_importDirectory.pushDirectory("userdata");
                        sprintf((char*)value, "%u", SteamUser()->GetSteamID().GetAccountID());
                        s_importDirectory.pushDirectory((const char*)value);
                        // Magic number: the original release's Steam app.
                        s_importDirectory.pushDirectory("20500");
                        s_importDirectory.pushDirectory("remote");
                        failed = false;
                    }
                    RegCloseKey(steam);
                }
                RegCloseKey(valve);
            }
            RegCloseKey(software);
        }
        s_readDirectoryFromRegistry = true;
        if (failed && g_openrfgUseSteam)
        {
            keen::debug::nativePrintArgs("[saveimport] Error reading Steam executable folder from registry.\n");
        }
    }
    return s_importDirectory.getCompletePath();
}
