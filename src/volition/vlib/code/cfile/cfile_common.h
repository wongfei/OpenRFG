// Original: volition/vlib/code/cfile/cfile_common.h
//
// Types shared by cfile.cpp and the platform layers (keen_cfile, pc_cfile). Placement inferred.
// cf_found_info is the result of a search (cf_find_file): which system found the file, on which media, its full
// name and size, and the packfile entry when it is in a packfile.
#pragma once

struct v_packfile_entry;

enum cf_io_media_types
{
    CF_IO_MEDIA_NONE = -1,
    CF_IO_MEDIA_HDD = 0,
    CF_IO_MEDIA_DVD = 1,
    CF_IO_MEDIA_HOST = 2,
    CF_IO_MEDIA_MEMORY = 5,
    CF_IO_MEDIA_NUM_TYPES = 6,
};

enum cf_search_types
{
    CF_SEARCH_NONE = -1,
    CF_SEARCH_STANDARD = 0,
    CF_SEARCH_PACKFILE = 1,
    CF_SEARCH_VDIR = 2,
    CF_SEARCH_NUM_TYPES = 3,
};

enum cf_io_access_types
{
    CF_IO_ACCESS_NONE = -1,
    CF_IO_ACCESS_STANDARD = 0,
    CF_IO_ACCESS_PACKFILE = 1,
    CF_IO_ACCESS_MEMORY = 2,
    CF_IO_ACCESS_UNKNOWN = 3,
    CF_IO_ACCESS_NUM_TYPES = 4,
};

enum cfmode_type
{
    CF_MODE_NONE = 0,
    CF_MODE_READ = 1,
    CF_MODE_WRITE = 2,
};

enum cf_seek_mode
{
    CF_SEEK_SET = 0,
    CF_SEEK_CUR = 1,
    CF_SEEK_END = 2,
};

enum cf_error_codes
{
    CF_ERROR_NONE = 0,
    CF_ERROR_READ = 1,
    CF_ERROR_WRITE = 2,
    CF_ERROR_ABORT = 3,
    CF_ERROR_NUM_CODES = 4,
};

// size 0x110
struct cf_found_info
{
    enum constants
    {
        CFFI_FULL_NAME_ARRAY_SIZE = 256,
    };

    cf_search_types m_searched_system; // 0x00
    cf_io_media_types m_media_type; // 0x04
    char m_full_name[256]; // 0x08
    unsigned int m_size; // 0x108
    v_packfile_entry* vpe; // 0x10C
};
