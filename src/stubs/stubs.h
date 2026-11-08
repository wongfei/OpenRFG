// The shared part of OpenRFG's stand-ins (new code, not recovered): the stream2 callback stand-ins' macros and what
// the module files define for one another. The test programs' half (the counters and test seams) is
// tests/stubs/test_stubs.h; the headless driver's own interface is tests/headless/headless_stubs.h.
#pragma once

#include "stub_trace.h"

#include "volition/rfg/code/gamewide/game_stream2_prims.h"

// A stream2 primitive's load and unload callbacks that report "loaded" (the data stays where stream2 read it). They
// do not count their calls (tests/stubs/test_stubs.h has the counting HEADLESS_PRIM_CALLBACKS); the stand-in trace
// (stub_trace.h) does. type is the primitive type, for the reader.
#define STUB_PRIM_CALLBACKS(load, unload, type) \
    bool load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_size) \
    { \
        GAME_STUB_HIT(#load); \
        return true; \
    } \
    void unload(const char* filename, unsigned int srid) \
    { \
        GAME_STUB_HIT(#unload); \
    }

// A stream2 container's callbacks (one function can serve several container types; type names the first).
#define STUB_CONTAINER_CALLBACKS(load, unload, type) \
    void load(unsigned int srid) \
    { \
        GAME_STUB_HIT(#load); \
    } \
    void unload(unsigned int srid) \
    { \
        GAME_STUB_HIT(#unload); \
    }

// The handle the effect and mover fader stand-ins return for "none".
static const unsigned int INVALID_HANDLE = 0xFFFFFFFF;
