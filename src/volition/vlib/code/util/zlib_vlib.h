// Original: volition/vlib/code/util/zlib_vlib.h
// Implementation: volition/vlib/code/util/zlib_vlib.cpp
//
// zlib (1.2.3) with a private allocator: zlib_stream owns a 384 KB buffer handed out in 512-byte pages
// (page_in_use[768]: the first page of a block holds 0x8000 | page count, the following ones 0x8000), so inflate and
// deflate never touch the heap (decompiled from zlib_stream::malloc). cfile keeps two of them (Cfile_decompress).
// zmalloc/zfree are the z_stream callbacks; opaque is the zlib_stream (zmalloc forwards to zlib_stream::malloc, zfree
// clears the block's page_in_use entries).
#pragma once

#include "volition/vlib/code/memory/vmemory.h"
#include "library/modules/thirdparty/zlib/zlib123/src/zlib.h"

enum compression_level
{
    ZC_BEST_SPEED = 1,
    ZC_BEST_COMPRESSION = 9,
};

enum zlib_result
{
    ZR_OK = 0,
    ZR_ERROR = 1,
    ZR_MORE_MEM = 2,
};

// size 0x60688
class zlib_stream
{
public:
    zlib_stream(const char* name); // 0x005951A0 (:55)
    void compress_init(compression_level level); // 0x005B33A0 (:62)
    bool compress(unsigned char* output_buffer, unsigned int output_size, unsigned int* output_written, unsigned char* input_buffer, unsigned int input_size, unsigned int* input_read, bool flush); // 0x00595200 (:102)
    void compress_finish(); // 0x005952A0 (:128)
    void decompress_init(); // 0x005B3420 (:137)
    zlib_result decompress(unsigned char* output_buffer, unsigned int output_size, unsigned int* output_written, unsigned char* input_buffer, unsigned int input_size, unsigned int* input_read); // 0x005952B0 (:177)
    void decompress_finish(); // 0x005953B0 (:221)
private:
    friend void* zmalloc(void* opaque, unsigned int items, unsigned int size); // inferred: the callbacks call malloc/free
    friend void zfree(void* opaque, void* addr);
    void reset_mem_manager(); // 0x005953C0 (:228)
    void* malloc(unsigned int items, unsigned int size); // 0x005953E0 (:244)
    void free(void* addr); // 0x00595480 (:302)
public:
    ~zlib_stream(); // 0x00597A20

private:
    mempool pool; // 0x00
    unsigned short page_in_use[768]; // 0x50
    unsigned char buffer[393216]; // 0x650
    z_stream_s zstream; // 0x60650
};

void* zmalloc(void* opaque, unsigned int items, unsigned int size); // 0x005B3330 (:26)
void zfree(void* opaque, void* addr); // 0x005B3350 (:33)
void zlib_init(); // 0x00595180 (:41)
void zlib_shutdown(); // 0x00595190 (:45)
