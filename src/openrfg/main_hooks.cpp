// OpenRFG's start-up hook (orig_main.md 2.9; new code, not recovered): the multiplayer command line, read before
// WinMain, and the stub of frontend_music_update (gs_boot_process, the boot screen thread), which steps aside for the
// real one in rfg_audio (SFX.md SX5).

#include "stub_trace.h"

#include "volition/rfg/code/gamewide/gamemusic.h"
#include "main_options.h"

// The multiplayer command line (orig_main.md deviation 3), read before WinMain reads the argument vector (it may add
// -username for --name).
namespace
{
    struct main_options_setup
    {
        main_options_setup()
        {
            main_options_parse();
        }
    };
    main_options_setup Main_options_setup;
}

// ---- gamemusic.cpp
#ifndef OPENRFG_AUDIO // SFX.md SX5: rfg_audio defines it (OpenRFG)
void frontend_music_update() // STUB: no audio, 2.9
{
    GAME_STUB_HIT("frontend_music_update");
}
#endif
