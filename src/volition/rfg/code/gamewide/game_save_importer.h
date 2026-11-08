// Original: volition/rfg/code/gamewide/game_save_importer.h
// Implementation: volition/rfg/code/gamewide/game_save_importer.cpp
//
// Imports the saves of the original release (the old save folder layout) into keen's save files: the save info, a
// save game and the profile.
#pragma once


struct game_save_info;

namespace keen
{
    namespace rfg
    {
        struct RfgSaveFile;
        struct RfgSaveProfileData;
    }
}

bool scan_game_info(game_save_info* pGame_info, const char* pString); // 0x007C37C0 (:13)
bool importSaveGameInfo(game_save_info* pInfo, unsigned int index, const char* pFolderName); // 0x007D1410 (:39)
bool importSaveGame(keen::rfg::RfgSaveFile* pTargetData, unsigned int index, const char* pFolderName); // 0x007D1540 (:78)
bool importSaveGameProfile(keen::rfg::RfgSaveProfileData* pTargetData, const char* pFolderName); // 0x007D1660 (:154)
const char* getSaveGameImportDirectory(); // 0x007D1820 (:219)
