// Original: library/modules/thirdparty/zlib/zlib123/src/zlib.h (zlib 1.2.3, third party)
//
// Seam only: the z_stream layout that zlib_stream (volition/vlib/code/util/zlib_vlib.h) holds by value, plus the
// entry points vlib calls. The real zlib 1.2.3 header replaces this file when zlib is built.
// Layout from rfg.pdb; zlib is C, so the functions have C linkage (public symbols _inflateInit_, _inflate, ...).
//
// OpenRFG builds define OPENRFG_EXTERNAL_ZLIB and use the vendored zlib (external/zlib, 1.2.11) instead: same
// z_stream layout (0x38 bytes), same stream format.
#pragma once

#if defined(OPENRFG_EXTERNAL_ZLIB)
#include <zlib.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

struct internal_state;

typedef void* (*alloc_func)(void* opaque, unsigned int items, unsigned int size);
typedef void (*free_func)(void* opaque, void* address);

// size 0x38
typedef struct z_stream_s
{
    unsigned char* next_in; // 0x00
    unsigned int avail_in; // 0x04
    unsigned long total_in; // 0x08
    unsigned char* next_out; // 0x0C
    unsigned int avail_out; // 0x10
    unsigned long total_out; // 0x14
    char* msg; // 0x18
    struct internal_state* state; // 0x1C
    alloc_func zalloc; // 0x20
    free_func zfree; // 0x24
    void* opaque; // 0x28
    int data_type; // 0x2C
    unsigned long adler; // 0x30
    unsigned long reserved; // 0x34
} z_stream;

typedef z_stream* z_streamp;

int inflateInit_(z_streamp strm, const char* version, int stream_size); // 0x00406F80 (inflate.c:191)
int inflate(z_streamp strm, int flush); // 0x004070B0 (inflate.c:557)

#ifdef __cplusplus
}
#endif

#endif
