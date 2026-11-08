// Original: volition/rfg/code/level/rfg_zonex_shared.cpp
//
// Zone headers and files, the zone and territory name helpers (file names, grid coordinates, zone boxes) and the zone
// file readers: rfg_zonex_get_data parses a loaded file in place, rfg_zonex_get_next_object walks its objects and
// rfg_zonex_merge_persistent_zone_files builds a territory's persistent zone from its "p_" files.
//
// Zone file names are "<territory>_<xx>_<yy>[_<zzz>]": xx and yy are the cell of the 16x16 grid of 511 m zones
// (x grows east, y grows south from 7 at the origin), the optional zzz a sub id. get_territory_info returns false for
// other names, after copying the name without extension into territory_name.
//
// Notes (decompiled behaviour, kept as is):
// - get_territory_files lists the persistent zone ("<territory>_persistent_15_00") first, but for a territory the
//   name filter then drops it again (its territory name parses as "<territory>_persistent").

#include "volition/rfg/code/level/rfg_zonex_shared.h"

#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// 0x00919C50 (.h:124)
zone_header::zone_header()
{
    signature = 0;
    version = 0;
    num_objects = 0;
    num_handles = 0;
    district_hash = 0;
    flags = 0;
}

// 0x009281A0 (.h:166)
zone_file::zone_file()
    : relation_data(false)
{
    // The original also assigns a freshly cleared table to relation_data (a no-op).
    data = nullptr;
    data_size = 0;
    relation_data_size = 0;
    objects = nullptr;
    object_data_size = 0;
    handle_data = nullptr;
    handle_data_size = 0;
}

// 0x00922E00
zone_file::~zone_file()
{
}

// 0x0091B590 (:15)
// LTCG: num_files in EDI.
static bool already_in_list(const char** file_list, int num_files, const char* lookup)
{
    for (int i = 0; i < num_files; i++)
    {
        if (v_stricmp(file_list[i], lookup) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x00922A50 (:34)
bool rfg_zonex_get_persistent_zone_filename(char* p_zone_name, int p_zone_name_size, const char* start_zone)
{
    char territory_name[64];
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(start_zone, territory_name, grid_x, grid_y, grid_z);
    _snprintf_safe(p_zone_name, (unsigned int)p_zone_name_size, "%s_%s_%02d_%02d%s", territory_name, "persistent", 15, 0, ".rfgzone_pc");
    return true;
}

// 0x00922AC0 (:86)
int rfg_zonex_get_territory_files(bool& is_territory, const char** files, int max_files, const char* filename, const char* ext)
{
    char territory_name[64];
    char persistent_territory_name[256];
    char persistent_pattern[64];
    char pattern[256];
    char file_territory_name[64];
    int grid_x;
    int grid_y;
    int grid_z;
    is_territory = rfg_zonex_get_territory_info(filename, territory_name, grid_x, grid_y, grid_z);
    rfg_zonex_get_territory_info(filename, persistent_territory_name, grid_x, grid_y, grid_z);
    _snprintf_safe(persistent_pattern, sizeof(persistent_pattern), "%s_%s_%02d_%02d%s", persistent_territory_name, "persistent", 15, 0, ".rfgzone_pc");
    const unsigned int num_persistent = cf_find_pattern(files, (unsigned int)max_files, persistent_pattern);
    if (!is_territory)
    {
        sprintf(pattern, "%s%s", territory_name, ext);
        return (int)(cf_find_pattern(files + num_persistent, (unsigned int)max_files - num_persistent, pattern) + num_persistent);
    }

    // A territory: all its zones, then keep the ones whose name parses to this territory, without duplicates.
    sprintf(pattern, "%s_*%s", territory_name, ext);
    unsigned int num_files = num_persistent + cf_find_pattern(files + num_persistent, (unsigned int)max_files - num_persistent, pattern);
    if (num_files == 0)
    {
        return 0;
    }
    unsigned int last = num_files - 1;
    unsigned int i = 0;
    do
    {
        bool keep = rfg_zonex_get_territory_info(files[i], file_territory_name, grid_x, grid_y, grid_z) && v_stricmp(territory_name, file_territory_name) == 0;
        if (keep && already_in_list(files, (int)i, files[i]))
        {
            keep = false;
        }
        if (keep)
        {
            i++;
            continue;
        }
        // Drop it: the last name takes its place.
        num_files--;
        if (i == last)
        {
            return (int)num_files;
        }
        files[i] = files[last];
        last--;
    } while (i < num_files);
    return (int)num_files;
}

// 0x0091B5D0 (:170)
bool rfg_zonex_get_territory_info(const char* start_zone, char* territory_name, int& grid_x, int& grid_y, int& grid_z)
{
    grid_x = 8;
    grid_y = 8;
    grid_z = 0;
    char name[1024];
    cf_drop_ext(name, sizeof(name), start_zone);
    strcpy(territory_name, name);
    const int len = (int)strlen(name);
    if (len < 6)
    {
        return false;
    }
    if (!isdigit(name[len - 1]) || !isdigit(name[len - 2]))
    {
        return false;
    }
    unsigned int y = (unsigned int)((name[len - 2] - '0') * 10 + (name[len - 1] - '0'));
    unsigned int z = 0;
    int sep = len - 3;
    if (isdigit(name[len - 3]))
    {
        // "<territory>_xx_yy_zzz"
        if (len < 10)
        {
            return false;
        }
        z = (unsigned int)((name[len - 3] - '0') * 100) + y;
        if (name[len - 4] != '_')
        {
            return false;
        }
        if (!isdigit(name[len - 5]) || !isdigit(name[len - 6]))
        {
            return false;
        }
        y = (unsigned int)((name[len - 6] - '0') * 10 + (name[len - 5] - '0'));
        sep = len - 7;
    }
    if (name[sep] != '_' || !isdigit(name[sep - 1]) || !isdigit(name[sep - 2]))
    {
        return false;
    }
    const unsigned int x = (unsigned int)((name[sep - 2] - '0') * 10 + (name[sep - 1] - '0'));
    if (name[sep - 3] != '_' || x >= 16 || y >= 16 || z >= 0xFFFF)
    {
        return false;
    }
    grid_x = (int)x;
    grid_y = (int)y;
    grid_z = (int)z;
    strncpy(territory_name, name, (size_t)(sep - 3));
    territory_name[sep - 3] = '\0';
    return true;
}

// 0x0091B810 (:276)
void rfg_zonex_make_territory_cell_name(char* fname, const char* territory_name, int grid_x, int grid_y)
{
    char x_name[16];
    char y_name[16];
    sprintf(x_name, grid_x < 10 ? "0%d" : "%d", grid_x);
    sprintf(y_name, grid_y < 10 ? "0%d" : "%d", grid_y);
    sprintf(fname, "%s_%s_%s%s", territory_name, x_name, y_name, ".rfgzone_pc");
}

// 0x0091B8B0 (:298)
void rfg_zonex_get_zone_offset(int grid_x, int grid_y, vector& offset)
{
    offset.x = (float)(grid_x - 8) * 511.0f + 255.5f;
    offset.y = 0.0f;
    offset.z = (float)(7 - grid_y) * 511.0f + 255.5f;
}

// 0x0091B920 (:310)
void rfg_zonex_get_zone_offset(const char* fname, vector& offset)
{
    char territory_name[256];
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(fname, territory_name, grid_x, grid_y, grid_z);
    rfg_zonex_get_zone_offset(grid_x, grid_y, offset);
}

// 0x00922CB0 (:322)
void rfg_zonex_get_zone_bbox(int grid_x, int grid_y, vector& bmin, vector& bmax)
{
    const float center_z = (float)(7 - grid_y) * 511.0f + 255.5f;
    const float center_x = (float)(grid_x - 8) * 511.0f + 255.5f;
    bmin.x = center_x - 255.5f;
    bmin.y = -511.0f;
    bmin.z = center_z - 255.5f;
    bmax.x = center_x + 255.5f;
    bmax.y = 511.0f;
    bmax.z = center_z + 255.5f;
}

// 0x0092B2C0 (:330)
void rfg_zonex_get_zone_bbox(const char* fname, vector& bmin, vector& bmax)
{
    if (str_stri(fname, "persistent") != nullptr)
    {
        rfg_zonex_get_maximal_bbox(bmin, bmax);
        return;
    }
    char territory_name[256];
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(fname, territory_name, grid_x, grid_y, grid_z);
    rfg_zonex_get_zone_bbox(grid_x, grid_y, bmin, bmax);
}

// 0x00922D70 (:376)
void rfg_zonex_get_maximal_bbox(vector& bmin, vector& bmax)
{
    // The 16x16 zone grid: the corner zones (0,0) and (15,15), joined.
    bmin.x = -4088.0f;
    bmin.y = -511.0f;
    bmin.z = 3577.0f;
    bmax.x = -3577.0f;
    bmax.y = 511.0f;
    bmax.z = 4088.0f;
    vector far_min;
    far_min.x = 3577.0f;
    far_min.y = -511.0f;
    far_min.z = -4088.0f;
    vector far_max;
    far_max.x = 4088.0f;
    far_max.y = 511.0f;
    far_max.z = -3577.0f;
    bbox_expand(bmin, bmax, far_min, far_max);
}

// 0x0091BB20 (:385)
unsigned short rfg_zonex_get_grid_id(const char* fname)
{
    // (x << 12) | (y << 8) | low byte of the sub id.
    char territory_name[256];
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(fname, territory_name, grid_x, grid_y, grid_z);
    return (unsigned short)((((grid_x & 0xF) << 4 | (grid_y & 0xF)) << 8) | (grid_z & 0xFF));
}

// 0x0091BC70 (:431)
bool rfg_zonex_get_grid_coords(const char* fname, int& grid_x, int& grid_y, int& grid_z)
{
    char territory_name[256];
    return rfg_zonex_get_territory_info(fname, territory_name, grid_x, grid_y, grid_z);
}

// ---- zone file readers (H2)

// <unsigned int,unsigned int,&uint_hash,7280,unsigned short,4,2> 0x0092E690 (.h:93)
template<typename K, typename D, unsigned int (*H)(K, unsigned int), int N, typename I, int KEY_ALIGN, int PAD>
void zone_object_hash_table_serialized_template<K, D, H, N, I, KEY_ALIGN, PAD>::initialize_zone_object_hash_table(data_hash_table<K, D, H, N, I>& out, const zone_object_hash_table_serialized_template<K, D, H, N, I, KEY_ALIGN, PAD>& data, int in_entry_data_num_objects)
{
    out.initialize_from_data(data.free, data.slot, data.next, data.entry_key, data.entry_data, in_entry_data_num_objects);
}

typedef zone_object_hash_table_serialized_template<unsigned int, unsigned int, &uint_hash, 7280, unsigned short, 4, 2> zone_relation_image;

// 0x00931880 (:45)
bool rfg_zonex_get_data(zone_file* zone, unsigned char* data, int size)
{
    memcpy(&zone->header, data, sizeof(zone_header));
    // Only version 36 is accepted.
    if (zone->header.version >= 37 || zone->header.version <= 35)
    {
        return false;
    }
    int offset = sizeof(zone_header);
    if ((zone->header.flags & 5) == 0)
    {
        zone_relation_image::initialize_zone_object_hash_table(zone->relation_data, *(const zone_relation_image*)(data + offset), zone->header.num_objects);
        zone->relation_data_size = sizeof(zone_relation_image);
        offset = 0x15560; // header + relation image
    }
    else
    {
        zone->relation_data_size = 0;
    }
    zone->handle_data = data + offset;
    const int handle_size = zone->header.num_handles * 4;
    offset += handle_size;
    zone->objects = data + offset;
    zone->handle_data_size = handle_size;
    zone->object_data_size = size - offset;
    return true;
}

// 0x0091B9E0 (:344)
void rfg_zonex_get_zone_bbox(const vector& pos, vector& bmin, vector& bmax)
{
    bmin.x = pos.x - 255.5f;
    bmin.y = -511.0f;
    bmin.z = pos.z - 255.5f;
    bmax.x = pos.x + 255.5f;
    bmax.y = 511.0f;
    bmax.z = pos.z + 255.5f;
}

// 0x0091BA60 (:355)
void rfg_zonex_get_subzone_bbox(vector& bmin, vector& bmax, int subzone_x, int subzone_z, const vector& zone_bmin, const vector& zone_bmax)
{
    // A zone is 3x3 subzones of 511 / 3 units; both y values are the zone bmin.y.
    bmin.x = (float)subzone_x * 170.33333f + zone_bmin.x;
    bmin.y = zone_bmin.y;
    bmin.z = (float)subzone_z * 170.33333f + zone_bmin.z;
    bmax.x = (float)(subzone_x + 1) * 170.33333f + zone_bmin.x;
    bmax.y = zone_bmin.y;
    bmax.z = (float)(subzone_z + 1) * 170.33333f + zone_bmin.z;
}

// 0x0091BB90 (:398)
bool rfg_zonex_get_grid_coords(const vector& pos, int& grid_x, int& grid_y)
{
    grid_x = (int)((pos.x + 4088.0f) / 511.0f);
    grid_y = (int)((4088.0f - pos.z) / 511.0f);
    return grid_x >= 0 && grid_x < 16 && grid_y >= 0 && grid_y < 16;
}

// 0x0091BC00 (:415)
unsigned short rfg_zonex_get_grid_xy(const vector& pos)
{
    const unsigned int grid_x = (unsigned int)((pos.x + 4088.0f) / 511.0f);
    const unsigned int grid_y = (unsigned int)((4088.0f - pos.z) / 511.0f);
    if (grid_x < 16 && grid_y < 16)
    {
        return (unsigned short)((grid_y & 0xF) | ((grid_x << 4) & 0xFF));
    }
    VLIB_CRITICAL_FAILURE();
    return 0;
}

// 0x0091BCD0 (:439)
cfile* rfg_zonex_open_objects(const zone_file* zf)
{
    return cf_open(zf->objects, zf->object_data_size, "rb", PLATFORM_PC);
}

// 0x0091BD00 (:445)
bool rfg_zonex_get_next_object(zone_object*& zobjp, cfile* f)
{
    // block_size covers the zone_object header and its property block.
    if (cf_eof(f))
    {
        return false;
    }
    zobjp = (zone_object*)cf_cur_memory_fp(f);
    if (zobjp->block_size == 0 || !cf_seek(f, zobjp->block_size, CF_SEEK_CUR))
    {
        return false;
    }
    return cf_pad_align_read(4, f);
}

// 0x0091BD50 (:473)
void rfg_zonex_close_objects(cfile* f)
{
    if (f != nullptr)
    {
        cf_close(f);
    }
}

// 0x0091BD60 (:478)
bool rfg_zonex_load_object_pblock(rfg_prop_block& pblock, zone_object* objp)
{
    return pblock.load((unsigned char*)(objp + 1));
}

// Reads a whole file into the scratch pool and parses it as a zone (rfg_zonex_merge_persistent_zone_files, inlined
// twice in the original).
static bool read_zone_file(zone_file& zf, const char* filename, mempool_base& scratch_pool)
{
    unsigned int file_size = 0;
    cf_get_file_size(&file_size, filename, false);
    if (file_size == 0)
    {
        return false;
    }
    unsigned char* data = (unsigned char*)scratch_pool.alloc(file_size, 0);
    if (data == nullptr)
    {
        return false;
    }
    cfile* f = cf_open(filename, "rb", PLATFORM_PC, false);
    if (f == nullptr)
    {
        return false;
    }
    cf_read(data, file_size, f, false);
    cf_close(f);
    return rfg_zonex_get_data(&zf, data, file_size);
}

// 0x00931930 (:483)
bool rfg_zonex_merge_persistent_zone_files(zone_file* z, const char* start_zone, mempool_base& perm_pool, mempool_base& scratch_pool, vlib_platform plat)
{
    // The persistent objects of a territory come from its "p_<zone>" files: their objects are concatenated behind a
    // header and an empty relation image, and the relation table is rebuilt from the object handles (handle -> offset).
    // The early returns keep the scratch pool marks (as in the original).
    const unsigned int start_mark = scratch_pool.mark();
    bool is_territory = false;
    const char** files = (const char**)scratch_pool.alloc(0x404, 0);
    char name[64];
    char ext[64];
    sprintf(name, "p_%s", start_zone);
    sprintf(ext, "%s", ".rfgzone_pc");
    cf_add_ext(name, sizeof(name), name, ext, false);
    const int num_files = rfg_zonex_get_territory_files(is_territory, files, 0x101, name, ext);

    // Pass 1: object count and size.
    int num_objects = 0;
    int object_data_size = 0;
    for (int i = 0; i < num_files; i++)
    {
        const unsigned int file_mark = scratch_pool.mark();
        zone_file zf;
        if (!read_zone_file(zf, files[i], scratch_pool))
        {
            return false;
        }
        num_objects += zf.header.num_objects;
        object_data_size += zf.object_data_size;
        scratch_pool.restore_to_mark(file_mark);
    }

    const int data_size = object_data_size + 0x15560;
    z->data = (unsigned char*)perm_pool.alloc(data_size, 0);
    memset(z->data, 0, data_size);
    z->header.num_objects = num_objects;
    z->data_size = data_size;
    z->header.signature = 0x454E4F5A; // "ZONE"
    z->header.version = 36;
    z->header.num_handles = 0;
    z->header.district_hash = 0;
    z->header.flags = 2;
    memcpy(z->data, &z->header, sizeof(zone_header));
    z->object_data_size = object_data_size;
    z->handle_data = nullptr;
    z->handle_data_size = 0;
    z->relation_data_size = sizeof(zone_relation_image);
    z->objects = z->data + 0x15560;

    // Pass 2: copy the objects.
    int offset = 0x15560;
    for (int i = 0; i < num_files; i++)
    {
        const unsigned int file_mark = scratch_pool.mark();
        zone_file zf;
        if (!read_zone_file(zf, files[i], scratch_pool))
        {
            return false;
        }
        memcpy(z->data + offset, zf.objects, zf.object_data_size);
        offset += zf.object_data_size;
        scratch_pool.restore_to_mark(file_mark);
    }

    // The relation table: handle -> offset of the object in z->objects.
    z->relation_data.clear();
    unsigned int object_offset = 0;
    for (int i = 0; i < z->header.num_objects; i++)
    {
        const zone_object* zobj = (const zone_object*)(z->objects + object_offset);
        if (!z->relation_data.insert_key_data(zobj->handle, object_offset))
        {
            keen::debug::nativePrintArgs("[rfg_zonex_shared.cpp] <rfg_zonex_merge_persistent_zone_files> Failed to insert Zone Object! Handle:%u Offset:%u i:%d \n", zobj->handle, object_offset, i);
        }
        object_offset += zobj->block_size;
        object_offset += (object_offset & 3) == 0 ? 0 : 4 - (object_offset & 3);
    }
    scratch_pool.restore_to_mark(start_mark);
    return true;
}

// 0x0091D130
zone_object::zone_object()
{
}
