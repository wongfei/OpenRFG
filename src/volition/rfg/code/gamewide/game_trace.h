// The game trace of OpenRFG --game-trace FILE (new code, not recovered, not in rfg.pdb): a line per game state change,
// world load, mission step, script fired, script action, event, objective, dialogue line, cutscene, player death, vehicle
// interaction step and first call of a stand-in, plus a status line every 5 s and a line for a crash (src/openrfg/main_game_trace.cpp). Each
// line is flushed, so a crash keeps what led to it. Line: seconds since the process started, Framecount, category, text.
//
// Off unless the option is given: GAME_TRACE then only tests a pointer and does not evaluate its arguments.
//
// Header-only (C++17 inline variables), as stub_trace.h: the libraries and test programs that use it get no new link
// dependency but Framecount.
#pragma once

#include "volition/vlib/code/os/frametime.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

inline FILE* Game_trace_file; // set by game_trace_open
inline void (*Game_trace_tick_hook)(); // per process step of gameseq_process with the trace open (OpenRFG)

inline void game_trace_write(const char* category, _Printf_format_string_ const char* format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    // clock() on Windows: wall clock milliseconds since the process started. One fprintf call: FILE's lock keeps a line
    // whole between threads (stream threads reach stand-ins).
    const long now = (long)clock();
    fprintf(Game_trace_file, "%5ld.%03ld f%-7d %-9s %s\n", now / 1000, now % 1000, Framecount, category, text);
    fflush(Game_trace_file);
}

#define GAME_TRACE(category, ...) \
    do \
    { \
        if (Game_trace_file != nullptr) \
        { \
            game_trace_write(category, __VA_ARGS__); \
        } \
    } while (0)

inline void game_trace_write_exit()
{
    GAME_TRACE("trace", "process exit");
}

// Opens the trace file (false if it cannot be written). No "process exit" line at the end of the file means the process
// crashed or was killed.
inline bool game_trace_open(const char* path)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path, "w") != 0 || file == nullptr)
    {
        return false;
    }
    Game_trace_file = file;
    GAME_TRACE("trace", "start");
    atexit(game_trace_write_exit);
    return true;
}

// The object's level name, "type N" for an object without one (most retail objects), "none" for no object (world.cpp;
// world::get_object_name is private).
const char* game_trace_object_name(unsigned int handle);
