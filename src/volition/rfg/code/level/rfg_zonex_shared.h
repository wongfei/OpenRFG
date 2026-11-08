// Original: volition/rfg/code/level/rfg_zonex_shared.h
// Implementation: volition/rfg/code/level/rfg_zonex_shared.cpp
//
// Zone files (.rfgzone_pc for a territory cell, .layer_pc for a managed layer), shared with the tools (decompiled from
// rfg_zonex_get_data and rfg_zonex_get_next_object).
// File layout: zone_header (version 36 only), then unless flags & 5 a serialized object relation table
// (zone_object_hash_table_serialized_template, 0x15548 bytes: handle -> index for parent/sibling/child links, turned
// into zone_file::relation_data by initialize_zone_object_hash_table), then num_handles 32-bit handles, then the objects
// to the end of the file. Each object is a zone_object header followed by block_size bytes of rfg_prop_block data
// (rfg_zonex_load_object_pblock), padded to 4. zone_file points into the loaded CPU data and does not own it.
// Grid ids: a zone's grid id is ((x << 4) | z) << 8 for the cell coordinates parsed from its name (two decimal digits
// each); rfg_zonex_get_zone_offset / get_zone_bbox convert between cell coordinates and world space.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

struct cfile;
class mempool_base;
class rfg_prop_block;

// size 0x18
struct zone_header
{
    zone_header(); // 0x00919C50 (.h:124)

    unsigned int signature; // 0x00
    unsigned int version; // 0x04
    int num_objects; // 0x08
    int num_handles; // 0x0C
    unsigned int district_hash; // 0x10
    unsigned int flags; // 0x14
};

// size 0x15580
struct zone_file
{
    zone_file(); // 0x009281A0 (.h:166)
    ~zone_file(); // 0x00922E00

    unsigned char* data; // 0x00
    int data_size; // 0x04
    zone_header header; // 0x08
    data_hash_table<unsigned int,unsigned int,&uint_hash,7280,unsigned short> relation_data; // 0x20
    bool has_relation_data; // 0x15568
    int relation_data_size; // 0x1556C
    unsigned char* objects; // 0x15570
    int object_data_size; // 0x15574
    unsigned char* handle_data; // 0x15578
    int handle_data_size; // 0x1557C
};

// size 0x34
struct zone_object
{
    zone_object(); // 0x0091D130

    unsigned int classname_hash; // 0x00
    unsigned int handle; // 0x04
    vector bmin; // 0x08
    vector bmax; // 0x14
    unsigned short flags; // 0x20
    unsigned short block_size; // 0x22
    unsigned int parent; // 0x24
    unsigned int sibling; // 0x28
    unsigned int child; // 0x2C
    unsigned int num; // 0x30
};

// The relation table as stored in a zone file: the hash_table / data_hash_table members laid out without their
// vtable (stupid_vtable_padding stands in for it) and with the padding of the target platform (KEY_ALIGN, PAD).
// size 0x15548 for <unsigned int,unsigned int,&uint_hash,7280,unsigned short,4,2>
template<typename K, typename D, unsigned int (*H)(K, unsigned int), int N, typename I, int KEY_ALIGN, int PAD>
struct zone_object_hash_table_serialized_template
{
    typedef data_hash_table<K, D, H, N, I> zone_object_hash_table;

    static void initialize_zone_object_hash_table(data_hash_table<K, D, H, N, I>& out, const zone_object_hash_table_serialized_template<K, D, H, N, I, KEY_ALIGN, PAD>& data, int in_entry_data_num_objects); // <unsigned int,unsigned int,&uint_hash,7280,unsigned short,4,2> 0x0092E690 (.h:93)

    unsigned char stupid_vtable_padding[4]; // 0x00
    I free; // 0x04
    I slot[N]; // 0x06
    I next[N]; // 0x38E6
    unsigned char more_stupid_padding[PAD]; // 0x71C6
    K entry_key[N]; // 0x71C8
    D entry_data[N]; // 0xE388
};

bool rfg_zonex_get_persistent_zone_filename(char* p_zone_name, int p_zone_name_size, const char* start_zone); // 0x00922A50 (:34)
bool rfg_zonex_get_data(zone_file* zone, unsigned char* data, int size); // 0x00931880 (:45)
int rfg_zonex_get_territory_files(bool& is_territory, const char** files, int max_files, const char* filename, const char* ext); // 0x00922AC0 (:86)
bool rfg_zonex_get_territory_info(const char* start_zone, char* territory_name, int& grid_x, int& grid_y, int& grid_z); // 0x0091B5D0 (:170)
void rfg_zonex_make_territory_cell_name(char* fname, const char* territory_name, int grid_x, int grid_y); // 0x0091B810 (:276)
void rfg_zonex_get_zone_offset(int grid_x, int grid_y, vector& offset); // 0x0091B8B0 (:298)
void rfg_zonex_get_zone_offset(const char* fname, vector& offset); // 0x0091B920 (:310)
void rfg_zonex_get_zone_bbox(int grid_x, int grid_y, vector& bmin, vector& bmax); // 0x00922CB0 (:322)
void rfg_zonex_get_zone_bbox(const char* fname, vector& bmin, vector& bmax); // 0x0092B2C0 (:330)
void rfg_zonex_get_zone_bbox(const vector& pos, vector& bmin, vector& bmax); // 0x0091B9E0 (:344)
void rfg_zonex_get_subzone_bbox(vector& bmin, vector& bmax, int subzone_x, int subzone_z, const vector& zone_bmin, const vector& zone_bmax); // 0x0091BA60 (:355)
void rfg_zonex_get_maximal_bbox(vector& bmin, vector& bmax); // 0x00922D70 (:376)
unsigned short rfg_zonex_get_grid_id(const char* fname); // 0x0091BB20 (:385)
bool rfg_zonex_get_grid_coords(const vector& pos, int& grid_x, int& grid_y); // 0x0091BB90 (:398)
unsigned short rfg_zonex_get_grid_xy(const vector& pos); // 0x0091BC00 (:415)
bool rfg_zonex_get_grid_coords(const char* fname, int& grid_x, int& grid_y, int& grid_z); // 0x0091BC70 (:431)
cfile* rfg_zonex_open_objects(const zone_file* zf); // 0x0091BCD0 (:439)
bool rfg_zonex_get_next_object(zone_object*& zobjp, cfile* f); // 0x0091BD00 (:445)
void rfg_zonex_close_objects(cfile* f); // 0x0091BD50 (:473)
bool rfg_zonex_load_object_pblock(rfg_prop_block& pblock, zone_object* objp); // 0x0091BD60 (:478)
bool rfg_zonex_merge_persistent_zone_files(zone_file* z, const char* start_zone, mempool_base& perm_pool, mempool_base& scratch_pool, vlib_platform plat); // 0x00931930 (:483)

// zone_object's constructor is compiler generated; its placement here is inferred.
