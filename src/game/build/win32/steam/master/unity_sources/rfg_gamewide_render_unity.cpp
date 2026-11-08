// Original: game/build/win32/steam/master/unity_sources (the gamewide unity build; name of this subset inferred)
//
// The gamewide render code is compiled as one translation unit, so its file statics are shared between the .cpp files:
// the time of day writes game_render.cpp's fog, HDR state, color correction and particle lighting statics and appends
// to its update-render task list. The sources are included in path order; only the render-side ones are listed (the
// rest of gamewide is in the GPU-free rfg library or not implemented).

#include "volition/rfg/code/gamewide/game_render.cpp"
#include "volition/rfg/code/gamewide/game_time_of_day.cpp"
#include "volition/rfg/code/gamewide/time_of_day_shared.cpp"
