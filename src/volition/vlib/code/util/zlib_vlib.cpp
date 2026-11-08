// Original: volition/vlib/code/util/zlib_vlib.cpp
//
// zlib_stream runs zlib without heap allocations: zlib's allocations come from the stream's own 384 KB buffer, handed
// out in runs of 512-byte pages (first-fit; page_in_use holds 0x8000 | run length on the first page of a run and
// 0x8000 on the others). Decompression uses Z_SYNC_FLUSH. decompress returns ZR_MORE_MEM when the output buffer filled
// up before the input was used up, ZR_OK when the stream ended or all input was consumed, ZR_ERROR otherwise. With a
// null output buffer the data is inflated into a scratch buffer and dropped (used to skip data).
// The version string passed to zlib is the one rfg.exe was built with.

#include "volition/vlib/code/util/zlib_vlib.h"

#include <string.h>

static const unsigned int Zlib_page_size = 512;
static const unsigned int Zlib_page_count = 768;

// 0x005B3330 (:26)
void* zmalloc(void* opaque, unsigned int items, unsigned int size)
{
    return ((zlib_stream*)opaque)->malloc(items, size);
}

// 0x005B3350 (:33)
void zfree(void* opaque, void* addr)
{
    ((zlib_stream*)opaque)->free(addr);
}

// 0x00595180 (:41)
void zlib_init()
{
}

// 0x00595190 (:45)
void zlib_shutdown()
{
}

// 0x005951A0 (:55)
zlib_stream::zlib_stream(const char* name)
{
    // name is not used; the pool member is only default constructed (it is not used either).
}

// 0x00597A20
zlib_stream::~zlib_stream()
{
}

// 0x005B33A0 (:62)
void zlib_stream::compress_init(compression_level level)
{
    zstream.avail_in = 0;
    zstream.total_in = 0;
    zstream.next_out = nullptr;
    zstream.avail_out = 0;
    zstream.total_out = 0;
    zstream.next_in = nullptr;
    zstream.zalloc = zmalloc;
    zstream.zfree = zfree;
    zstream.opaque = this;
    reset_mem_manager();
    deflateInit_(&zstream, level, "1.2.3", sizeof(z_stream));
}

// 0x00595200 (:102)
bool zlib_stream::compress(unsigned char* output_buffer, unsigned int output_size, unsigned int* output_written, unsigned char* input_buffer, unsigned int input_size, unsigned int* input_read, bool flush)
{
    zstream.next_in = input_buffer;
    zstream.next_out = output_buffer;
    zstream.avail_in = input_size;
    zstream.avail_out = output_size;
    zstream.total_in = 0;
    zstream.total_out = 0;
    const int result = deflate(&zstream, flush ? Z_SYNC_FLUSH : Z_NO_FLUSH);
    *output_written = zstream.total_out;
    *input_read = zstream.total_in;
    if (flush && zstream.avail_out == 0)
    {
        return false;
    }
    return result == Z_OK;
}

// 0x005952A0 (:128)
void zlib_stream::compress_finish()
{
    deflateEnd(&zstream);
}

// 0x005B3420 (:137)
void zlib_stream::decompress_init()
{
    zstream.avail_in = 0;
    zstream.total_in = 0;
    zstream.next_out = nullptr;
    zstream.avail_out = 0;
    zstream.total_out = 0;
    zstream.next_in = nullptr;
    zstream.zalloc = zmalloc;
    zstream.zfree = zfree;
    zstream.opaque = this;
    reset_mem_manager();
    inflateInit_(&zstream, "1.2.3", sizeof(z_stream));
}

// 0x005952B0 (:177)
zlib_result zlib_stream::decompress(unsigned char* output_buffer, unsigned int output_size, unsigned int* output_written, unsigned char* input_buffer, unsigned int input_size, unsigned int* input_read)
{
    zstream.next_in = input_buffer;
    zstream.avail_in = input_size;
    zstream.total_in = 0;
    zstream.total_out = 0;
    int result;
    if (output_buffer == nullptr)
    {
        unsigned char scratch[0x800];
        do
        {
            zstream.next_out = scratch;
            zstream.avail_out = sizeof(scratch);
            result = inflate(&zstream, Z_SYNC_FLUSH);
        }
        while (result == Z_OK && zstream.avail_out == 0 && zstream.avail_in != 0);
    }
    else
    {
        zstream.next_out = output_buffer;
        zstream.avail_out = output_size;
        result = inflate(&zstream, Z_SYNC_FLUSH);
    }
    *output_written = zstream.total_out;
    *input_read = zstream.total_in;
    if (result == Z_OK)
    {
        return zstream.avail_out == 0 ? ZR_MORE_MEM : ZR_OK;
    }
    if (result == Z_BUF_ERROR)
    {
        return zstream.avail_in != 0 ? ZR_ERROR : ZR_OK;
    }
    return result == Z_STREAM_END ? ZR_OK : ZR_ERROR;
}

// 0x005953B0 (:221)
void zlib_stream::decompress_finish()
{
    inflateEnd(&zstream);
}

// 0x005953C0 (:228)
void zlib_stream::reset_mem_manager()
{
    memset(page_in_use, 0, sizeof(page_in_use));
}

// 0x005953E0 (:244)
void* zlib_stream::malloc(unsigned int items, unsigned int size)
{
    // First fit: the first run of free pages long enough.
    const unsigned int page_count = (items * size + Zlib_page_size - 1) / Zlib_page_size;
    unsigned int first = 0;
    while (first < Zlib_page_count + 1 - page_count)
    {
        unsigned int page = first;
        if (page_in_use[first] == 0)
        {
            page = first + 1;
            while (page < first + page_count && page_in_use[page] == 0)
            {
                ++page;
            }
            if (page >= first + page_count)
            {
                for (unsigned int i = first; i < first + page_count; ++i)
                {
                    page_in_use[i] = (unsigned short)(i == first ? (page_count | 0x8000) : 0x8000);
                }
                return buffer + first * Zlib_page_size;
            }
        }
        first = page + 1;
    }
    return nullptr;
}

// 0x00595480 (:302)
void zlib_stream::free(void* addr)
{
    const unsigned int first = (unsigned int)((unsigned char*)addr - buffer) / Zlib_page_size;
    const unsigned int end = (page_in_use[first] & 0x7FFF) + first;
    for (unsigned int i = first; i < end; ++i)
    {
        page_in_use[i] = 0;
    }
}
