// Original: volition/rfg/code/gamewide/verify_gamesave.h
// Implementation: volition/rfg/code/gamewide/verify_gamesave.cpp
//
// The verify game save state, pushed by verify_gamesave_start to load a save: it borrows the gameplay GPU mempool,
// reads the save into it (verify_gamesave_loaded is the completion callback), checks the MD5 at its end (a corrupt
// save shows a message box and pops the state), copies it into the world's load buffer and starts loading the
// territory from it.
#pragma once


struct game_save_info;

void verify_gamesave_system_init(); // 0x007F02B0 (:267)
void verify_gamesave_start(game_save_info* save, bool reset_destruction); // 0x007E7650 (:281)
void verify_gamesave_loaded(void* buf, int size); // 0x007E7680 (:292)
