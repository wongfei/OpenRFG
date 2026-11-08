// Original: volition/vlib/code/cfile/cfile_internal.h
//
// The cfile handle and the private records of cfile.cpp. Placement inferred: cfile::reset is defined in cfile.cpp and
// the PDB records no inline method, but the platform layers and packfile.cpp read cfile fields, so the struct cannot be
// private to cfile.cpp. Other headers only forward declare `struct cfile`.
//
// cfile (decompiled):
// - Allocated from Cfile_list (object_pool<cfile,256,1>) under Cfile_list_cs.
// - m_access_type selects the handle in the union at 0x14C: a keen file (STANDARD, also used for VDIR hits), a
//   packfile entry opened through the packfile's own keen file (PACKFILE), or a cf_memory_handle over memory (MEMORY:
//   a packfile whose data was preloaded, or cf_open(buf, size, ...)).
// - pos/size are positions inside the entry; max_write_size is 0xFFFFFFFF when unlimited.
// - m_flags: 0x1 async open, 0x4 byte-swap needed (platform endianness differs), 0x8 async read aborted
//   (cf_abort_async_read; the next short read returns CF_ERROR_ABORT and clears it).
// cf_registered_search is one entry of the search order kept by cf_add_search/cf_remove_search (16 at most).
#pragma once

#include "volition/vlib/code/cfile/cfile_common.h"

struct cf_keen_open_file;
struct keen_cf_packfile_handle;
struct cf_memory_handle;

// size 0x16C
struct cfile
{
    void reset(); // 0x005649E0 (cfile.cpp:183)

    char name[65]; // 0x00
    char full_pathname[257]; // 0x41
    cf_search_types m_searched_system; // 0x144
    cf_io_access_types m_access_type; // 0x148
    union
    {
        cf_keen_open_file* m_standard_fp; // 0x14C, CF_IO_ACCESS_STANDARD
        keen_cf_packfile_handle* m_packfile_fp; // 0x14C, CF_IO_ACCESS_PACKFILE
        cf_memory_handle* _m_memory_fp; // 0x14C, CF_IO_ACCESS_MEMORY
    };
    cf_io_media_types m_media_type; // 0x150
    cfmode_type mode; // 0x154
    cf_error_codes m_error_code; // 0x158
    unsigned int pos; // 0x15C
    unsigned int size; // 0x160
    unsigned int max_write_size; // 0x164
    unsigned int m_flags; // 0x168
};

// size 0x10
struct cf_memory_handle
{
    unsigned char* buf; // 0x00
    unsigned int size; // 0x04
    bool compressed; // 0x08
    unsigned int compressed_size; // 0x0C
};

// size 0x8
struct cf_registered_search
{
    cf_search_types m_search_type; // 0x00
    cf_io_media_types m_media_type; // 0x04
};
