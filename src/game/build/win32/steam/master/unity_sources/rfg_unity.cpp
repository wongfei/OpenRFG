// Original: game/build/win32/steam/master/unity_sources/rfg_unity.cpp (the unity build of volition/rfg/code/*.cpp)
//
// rfg.cpp whole, for the programs on the original game loop (multiplayer.md MP1, the rfg_loop library). The render
// library compiles rfg.cpp by itself with only the Pegs callbacks (OPENRFG_RFG_PEG_CALLBACKS_ONLY). The other files of
// the original unity (nvidia_highlights.cpp, multi_server_browser.cpp, object_effect_debug.cpp, game_render_terrain.cpp)
// are not ported or are in the rfg library.

#include "volition/rfg/code/rfg.cpp"
