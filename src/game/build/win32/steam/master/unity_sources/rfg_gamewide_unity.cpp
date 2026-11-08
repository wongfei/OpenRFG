// Original: game/build/win32/steam/master/unity_sources/rfg_gamewide_unity.cpp (the gamewide unity build)
//
// The gamewide files of the original game loop (multiplayer.md MP1), one translation unit as in rfg.exe: they share
// file statics (game_handshake.cpp reads gameseq.cpp's state stack and Wait_for_prev_done_frame, game_clock.cpp reads
// gamestate_gameplay.cpp's Game_clock, gamewide.cpp reads gameseq.cpp's state stack). The order puts each static before
// its readers. The render-side gamewide files are rfg_gamewide_render_unity.cpp (render library); tweak_table.cpp and
// game_stream2_register.cpp are in the rfg library. downloadable_content.cpp (MP4, whole since stub_impl.md S5) reads
// gameseq.cpp's state stack and defines Current_bundle_id and Dlc_level_mempool_mark for gamestate_gameplay.cpp.
// stats.cpp (stub_impl.md S5): before downloadable_content.cpp (stats_reset_specific) and gamestate_gameplay.cpp
// (Achievements_enabled).

#include "volition/rfg/code/gamewide/gameseq.cpp"
#include "volition/rfg/code/gamewide/stats.cpp"
#include "volition/rfg/code/gamewide/downloadable_content.cpp"
#include "volition/rfg/code/gamewide/game_handshake.cpp"
// game_render_thread.cpp (orig_main.md OM4): it writes gameseq.cpp's Wait_for_prev_done_frame and gamewide.cpp reads
// its Rt_init.
#include "volition/rfg/code/gamewide/game_render_thread.cpp"
#include "volition/rfg/code/gamewide/gamestate_gameplay.cpp"
#include "volition/rfg/code/gamewide/game_clock.cpp"
#include "volition/rfg/code/gamewide/gamewide.cpp"
