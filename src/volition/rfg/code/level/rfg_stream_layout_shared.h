// Original: volition/rfg/code/level/rfg_stream_layout_shared.h
// Implementation: volition/rfg/code/level/rfg_stream_layout_shared.cpp
//
// World streaming grid (decompiled). stream_layout_level_load builds a stream_grid for the territory from the zone
// files: two stream_layers (LAYER_LEVEL, LAYER_TERRAIN) of square cells. Each stream_grid_cell owns up to four
// containers named "<layer>_cXXZZ" plus h/v/p variants (get_container_name): srid[0] base, [1] h, [2] v, [3] precache.
// stream_grid::do_frame(stream_pos): for each layer, the cells within stream_radius of the position are requested
// (stream_new_cells, priority STREAM_PRIORITY_TERRAIN for the terrain layer, 128 otherwise) and the ones that left the
// range are released (release_old_cells). In the level layer a cell outside the current cell's stream_mask is masked
// instead: create_cell_mask_region puts Havok phantoms (masked_cell::n_region) around it. Afterwards the territory-wide
// containers ns_base, ns_precache and landmarks_srid are kept requested, and fragmentation of Stream_world_allocator
// is handled (do_fragmentation_handling, frag_fail).
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
struct bbox;
class havok_bpo;
struct territory;
struct zone_file;
struct terrain_stitch_info;
struct visual_effect_info;
struct level_object_base_properties;

namespace keen
{
    struct TaskSystem;
}

enum layout_category
{
    LAYOUT_ZONE_CATEGORY = 1,
    LAYOUT_TERRAIN_CATEGORY = 2,
    LAYOUT_STITCH_CATEGORY = 3,
    LAYOUT_CHUNK_CATEGORY = 4,
    LAYOUT_LEVEL_TEXTURE_CATEGORY = 5,
    LAYOUT_EFFECT_CATEGORY = 6,
    LAYOUT_FMESH_CATEGORY = 7,
    LAYOUT_ALWAYS_LOADED_CATEGORY = 8,
    NUM_LAYOUT_CATEGORIES = 9,
};

enum layout_layer
{
    LAYER_LEVEL = 0,
    LAYER_TERRAIN = 1,
    NUM_LAYERS = 2,
};

// size 0x18
class stream_grid_cell
{
public:
    stream_grid_cell(); // 0x0091AA90 (:1049)
    void clear(); // 0x0091AAB0 (:1058)
    void add_file(const char* filename, layout_category category, const vector& bmin, const vector& bmax, vlib_platform plat, const char* gpu_ext, unsigned int obj_handle, unsigned int subchunk_uid); // 0x0091AAF0 (:1077)
    void get_container_name(char* fname, char* h_fname, char* v_fname, char* precache_fname); // 0x0091AB20 (:1118)
    void start_recording_files(mempool_base* tmp_pool); // 0x00936BB0 (:1135)
    void finish_recording_files(vlib_platform plat, mempool_base* dst_pool); // 0x00928FA0 (:1144)
    unsigned int get_streaming_srid(); // 0x0091ABC0 (:1301)

    unsigned int stream_mask; // 0x00
    unsigned int srid[4]; // 0x04
    unsigned char grid_x; // 0x14
    unsigned char grid_z; // 0x15
    unsigned short layer_index; // 0x16
};

// size 0x4C0
class stream_layer
{
public:
    // size 0x14
    struct masked_cell
    {
        stream_grid_cell* gc; // 0x00
        havok_bpo* n_region[4]; // 0x04
    };

    stream_layer(); // 0x009293A0 (:1330)
    void clear(); // 0x00921A60 (:1338)
    void add_file(const char* filename, layout_category category, const vector& _bmin, const vector& _bmax, vlib_platform plat, const char* gpu_ext, unsigned int obj_handle, unsigned int subchunk_handle); // 0x00929AC0 (:1724)
    void get_cell_bbox(vector& _bmin, vector& _bmax, stream_grid_cell* cell); // 0x0091AC20 (:1380)
    stream_grid_cell* get_grid_cell(int x, int z); // 0x0091ACF0 (:1398)
    int get_cell_list(const vector& _bmin, const vector& _bmax, bbox* cellsp, int* cell_indices, int max_cells); // 0x00929410 (:1415)
    void remove_cells(const vector& _bmin, const vector& _bmax); // 0x00929840 (:1463)
    void calc_stream_box(vector& _bmin, vector& _bmax, const vector& pos); // 0x0091AD50 (:1482)
    bool is_masked(const stream_grid_cell* src, const stream_grid_cell* chk); // 0x0091ADF0 (:1492)
    bool toggle_mask(stream_grid_cell* src, const stream_grid_cell* chk); // 0x0091AE40 (:1500)
    void stream_new_cells(farray<stream_grid_cell*,25>& cellsp, unsigned char priority); // 0x00921C50 (:1585)
    void release_old_cells(farray<stream_grid_cell*,25>& cellsp); // 0x00921F00 (:1704)
    void create_cell_mask_region(stream_grid_cell* gc); // 0x00929920 (:1517)
    void delete_cell_mask_region(stream_grid_cell* gc); // 0x00921B80 (:1549)

    stream_grid_cell** cells; // 0x00
    int num_cells; // 0x04
    int dim_x; // 0x08
    int dim_z; // 0x0C
    int cell_load_distance; // 0x10
    float stream_radius; // 0x14
    vector bmin; // 0x18
    vector bmax; // 0x24
    vector stream_bmin; // 0x30
    vector stream_bmax; // 0x3C
    farray<stream_grid_cell*,25> active_cells; // 0x48
    farray<masked_cell,50> masked_cells; // 0xBC
    vector cell_dim; // 0x4B4
};

// size 0x24
class stream_grid
{
public:
    stream_grid(mempool_base* pool); // 0x00929B70 (:1750)
    void clear(); // 0x00921F60 (:1770)
    bool add_zone(const char* filename, const zone_file* zp, str_data_hash_table<unsigned char,1024,64,unsigned short>* terrain_textures, str_data_hash_table<unsigned char,1024,64,unsigned short>* fmesh_list, terrain_stitch_info* stitch_infos, int num_stitch_infos, vlib_platform plat, mempool_base* dst_pool, mempool_base* tmp_pool); // 0x00936C00 (:1961)
private:
    bool add_file(const char* filename, layout_category category, layout_layer layer, const vector& _bmin, const vector& _bmax, vlib_platform plat, const char* gpu_ext, unsigned int obj_handle, unsigned int subchunk_handle); // 0x00929C40 (:2331)
    bool add_chunk(const char* filename, layout_category category, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat, bool is_stitch, unsigned int obj_handle, unsigned int subchunk_uid); // 0x00929CC0 (:2345)
    bool add_effect(unsigned int handle, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat); // 0x0092A0A0 (:2415)
    bool add_effect(visual_effect_info* vinfo, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat); // 0x0092A070 (:2400)
    bool add_effect(const char* filename, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat); // 0x00929EF0 (:2379)
    bool add_effects_from_object_props(const level_object_base_properties* props, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat); // 0x0092A100 (:2429)
    bool add_fmesh(const char* filename, layout_layer layer, const vector& bmin, const vector& bmax, vlib_platform plat); // 0x0092A6B0 (:2551)
public:
    void do_frame(const vector& stream_pos, bool single_zone_mode); // 0x00930FB0 (:3790)
    void do_fragmentation_handling(); // 0x00922180 (:3910)
    void set_stream_radius(layout_layer layer, float radius); // 0x0091AEB0 (:3995)
    float get_stream_radius(layout_layer layer); // 0x0091AFB0 (:4007)

    stream_layer* layers; // 0x00
    territory* terr; // 0x04
    unsigned int ns_base; // 0x08
    unsigned int ns_precache; // 0x0C
    unsigned int landmarks_srid; // 0x10
private:
    bool compact_super_emergency; // 0x14
    timestamp_realtime compact_ts; // 0x18
    timestamp_realtime compact_wait_ts; // 0x1C
public:
    str_data_hash_table<unsigned char,255,64,unsigned char>* m_tmp_table; // 0x20
};

extern const char* Layout_category_name[9]; // 0x016688C0

void unused_dcf_force_compact(); // 0x0091AEA0 (:3989)
void stream_layout_system_init(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat, int dlc_id, keen::TaskSystem* pTaskSystem); // 0x0091B0C0 (:4280)
void stream_layout_system_shutdown(); // 0x0091B0E0 (:4340)
bool stream_layout_level_init(const char* start_zone, bool streaming, int dlc_id); // 0x0092A860 (:4364)
bool stream_layout_add_base_packfiles(const char* start_zone); // 0x009228A0 (:4512)
stream_grid* stream_layout_level_load(mempool_base* pool, mempool_base* tmp_pool, void (*num_zones_cb)(int), void (*zone_cb)(const char*, zone_file*)); // 0x00937CB0 (:4593)
void stream_layout_release_base_packfiles(); // 0x0091B100 (:4851)
void full_screen_map_image_level_shutdown(); // 0x0091B110 (:4856)
void stream_layout_level_shutdown(); // 0x009229B0 (:4881)
stream_grid* stream_layout_get_grid(); // 0x0091B1B0 (:4909)
