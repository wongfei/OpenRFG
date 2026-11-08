// Original: volition/rfg/code/level/rfg_stream_layout_shared.cpp
//
// Partial: the packfile build path. Not implemented: building the cell containers from loose zone files
// (Use_packfiles off: stream_grid::add_zone, add_file, add_chunk, add_effect*, add_effects_from_object_props,
// add_fmesh, stream_layer::add_file, stream_grid_cell::add_file / start_recording_files / finish_recording_files,
// sl_is_preload_file, sl_get_resource_textures, stream_layout_add_base_packfiles,
// stream_layout_release_base_packfiles and that branch of stream_layout_level_load).
//
// Packfile path (decompiled): stream_layout_level_init adds the territory's temp packfiles ("/gamebuild\<terr>_l0/_l1"
// for streaming, the precache packfile, zonescript_<terr> into a pregame split pool). stream_layout_level_load builds
// the grid from the territory's zone list (sl_build_minimal_grid: layer 0 is 48x48 cells of 170.33 m, layer 1 16x16
// of 511 m, both over the 16x16 zone world box; a cell exists where a zone covers it), reads the container records
// of <terr>_l0/_l1.asm_pc (or <terr>.asm_pc) and looks up each cell's four containers by name
// ("<layer>_cXXZZ", "<layer>h_", "<layer>v_", "<layer>p_"), then reads every zone file (sl_slurp_zone_and_terrain)
// for the zone callback and loads the stream mask file.
//
// Notes (decompiled behaviour, kept as is):
// - get_cell_list does not clamp to the grid: positions near the edge give indices outside the cell array, which
//   stream_grid::do_frame reads without checking.
// - create_cell_mask_region stores uninitialised region handles for the broadphases that do not exist (zero here).
// - sl_slurp_zone_and_terrain creates the stitch containers from the first stitch record only, once per record.
// - stream_layout_level_init returns false without disabling standard access again when a packfile is missing.

#include "volition/rfg/code/level/rfg_stream_layout_shared.h"

#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <string.h>

const char* Layout_category_name[9] = { "unknown", "zone", "terrain", "stitch", "chunk", "chunk texture", "effect", "undergrowth", "terrain texture" }; // 0x016688C0

// ---- file-static globals
static vlib_platform Layout_platform; // 0x02CEBB44
static bool Stream_layout_inited; // 0x02CEC14E
static bool Layout_inited; // 0x02CEC14C
static bool Layout_streaming; // 0x02CEC14D
static char Layout_territory_name[64]; // 0x02CEC150
static char Layout_territory_start_zone[64]; // 0x02CEC190
static stream_grid* Layout_grid; // 0x02CEC1D0, stream_layout_get_grid
static mempool_base* Layout_pool; // 0x02CEC1D4
static mempool_base* Layout_tmp_pool; // 0x02CEC1D8
static mempool_base* zonescript_preload_pool_ptr; // 0x02CEC1DC
// File extensions per vlib_platform; every platform entry holds the PC extension in this build.
static const char* Zone_platform_ext[8] = { ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc", ".rfgzone_pc" }; // 0x016688E4
static const char* Stitch_platform_ext_slshared[8] = { ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc" }; // 0x01668904
static const char* Vpp_platform_ext[8] = { ".vpp_pc", ".vpp_pc", ".vpp_pc", ".vpp_pc", ".vpp_pc", ".vpp_pc", ".vpp_pc", ".vpp_pc" }; // 0x01668A64
static const char* Smask_platform_ext[8] = { ".smsk_pc", ".smsk_pc", ".smsk_pc", ".smsk_pc", ".smsk_pc", ".smsk_pc", ".smsk_pc", ".smsk_pc" }; // 0x01668AA4

// The zone world box: zones (0,0) and (15,15) joined (as rfg_zonex_get_maximal_bbox).
static void layout_world_box(vector& bmin, vector& bmax)
{
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

// Sets up one layer of the minimal grid over the world box; false when the cell array cannot be allocated.
static bool layout_init_layer(stream_layer& layer, int dim, float cell_size, float radius, int cell_load_distance, const vector& world_min, const vector& world_max, mempool_base* pool)
{
    layer.dim_x = dim;
    layer.dim_z = dim;
    layer.num_cells = dim * dim;
    layer.cells = (stream_grid_cell**)pool->alloc((unsigned int)layer.num_cells * sizeof(stream_grid_cell*), 0);
    if (layer.cells == nullptr)
    {
        return false;
    }
    for (int i = 0; i < layer.num_cells; i++)
    {
        layer.cells[i] = nullptr;
    }
    layer.bmin = world_min;
    layer.bmax = world_max;
    layer.stream_radius = radius;
    layer.cell_load_distance = cell_load_distance;
    layer.calc_stream_box(layer.stream_bmin, layer.stream_bmax, ZERO_VECTOR);
    layer.cell_dim.x = cell_size;
    layer.cell_dim.y = 0.0f;
    layer.cell_dim.z = cell_size;
    return true;
}

// 0x009309E0 (:690)
// LTCG: terr_name in ECX.
static stream_grid* sl_build_minimal_grid(const char* terr_name, int num_files, const char** files, mempool_base* pool)
{
    stream_grid* const grid = (stream_grid*)pool->alloc(sizeof(stream_grid), 0);
    if (grid == nullptr)
    {
        return nullptr;
    }
    new (grid) stream_grid(pool);
    grid->terr = territory_shared_find(terr_name);
    vector world_min;
    vector world_max;
    layout_world_box(world_min, world_max);
    if (!layout_init_layer(grid->layers[LAYER_LEVEL], 48, 170.33333f, 340.0f, 2, world_min, world_max, pool))
    {
        return nullptr;
    }
    if (!layout_init_layer(grid->layers[LAYER_TERRAIN], 16, 511.0f, 510.0f, 1, world_min, world_max, pool))
    {
        return nullptr;
    }

    // A cell exists where a zone covers it.
    for (int f = 0; f < num_files; f++)
    {
        vector zone_min;
        vector zone_max;
        rfg_zonex_get_zone_bbox(files[f], zone_min, zone_max);
        char name[64];
        cf_drop_ext(name, sizeof(name), files[f]);
        for (unsigned short l = 0; l < NUM_LAYERS; l++)
        {
            stream_layer& layer = grid->layers[l];
            const int x0 = (int)ceil((zone_min.x - layer.bmin.x) / layer.cell_dim.x);
            const int x1 = (int)ceil((zone_max.x - layer.bmin.x) / layer.cell_dim.x);
            const int z0 = (int)ceil((zone_min.z - layer.bmin.z) / layer.cell_dim.z);
            const int z1 = (int)ceil((zone_max.z - layer.bmin.z) / layer.cell_dim.z);
            for (int z = z0; z < z1; z++)
            {
                for (int x = x0; x < x1; x++)
                {
                    const int index = layer.dim_x * z + x;
                    if (layer.cells[index] != nullptr)
                    {
                        continue;
                    }
                    stream_grid_cell* cell = (stream_grid_cell*)pool->alloc(sizeof(stream_grid_cell), 0x10);
                    if (cell != nullptr)
                    {
                        new (cell) stream_grid_cell();
                    }
                    cell->grid_x = (unsigned char)x;
                    cell->grid_z = (unsigned char)z;
                    cell->layer_index = l;
                    layer.cells[index] = cell;
                }
            }
        }
    }
    return grid;
}

// 0x00936990 (:862)
// LTCG: texture_list in ECX, fmesh_list in EDX, stitch_infos in EBP, num_stitch_infos in EDI, plat in ESI.
static zone_file* sl_slurp_zone_and_terrain(const char* filename, unsigned char* terrain_data, int size, str_data_hash_table<unsigned char, 1024, 64, unsigned short>* texture_list, str_data_hash_table<unsigned char, 1024, 64, unsigned short>* fmesh_list, terrain_stitch_info*& stitch_infos, int& num_stitch_infos, vlib_platform plat, mempool_base* pool)
{
    // Reads <filename>.rfgzone_pc into pool; with texture or foliage lists it also reads the terrain file names and
    // creates the stitch containers.
    char ext[56];
    char zone_name[64];
    sprintf(ext, "%s", ".rfgzone_pc");
    cf_add_ext(zone_name, sizeof(zone_name), filename, ext, false);
    unsigned int zone_size;
    if (!cf_get_file_size(&zone_size, zone_name, false))
    {
        return nullptr;
    }
    zone_file* const zone = (zone_file*)pool->alloc(sizeof(zone_file), 0);
    if (zone == nullptr)
    {
        return nullptr;
    }
    new (zone) zone_file();
    unsigned char* const data = (unsigned char*)pool->alloc(zone_size, 0);
    if (data == nullptr)
    {
        return nullptr;
    }
    cfile* const f = cf_open(zone_name, "rb", PLATFORM_PC, false);
    if (f == nullptr)
    {
        return nullptr;
    }
    cf_read(data, zone_size, f, false);
    cf_close(f);
    if (!rfg_zonex_get_data(zone, data, (int)zone_size))
    {
        return nullptr;
    }
    if (texture_list == nullptr && fmesh_list == nullptr)
    {
        return zone;
    }

    char terrain_ext[56];
    char terrain_name[64];
    sprintf(terrain_ext, "%s", ".cterrain_pc");
    cf_add_ext(terrain_name, sizeof(terrain_name), zone_name, terrain_ext, false);
    if (!terrain_load_filenames_SLOW(terrain_name, terrain_data, size, texture_list, fmesh_list, stitch_infos, num_stitch_infos, plat))
    {
        return nullptr;
    }
    if (texture_list != nullptr)
    {
        for (int i = 0; i < num_stitch_infos; i++)
        {
            // The name is a self-relative offset in the first record (see the file comment).
            const unsigned int* const name_offset = (const unsigned int*)((const unsigned char*)stitch_infos + 0x10);
            char stitch_name[64];
            cf_add_ext(stitch_name, sizeof(stitch_name), (const char*)name_offset + *name_offset, Stitch_platform_ext_slshared[plat], false);
            chunk_create_container(stitch_name, plat, false, Layout_pool, Layout_tmp_pool);
        }
    }
    return zone;
}

// ---- stream_grid_cell

// 0x0091AA90 (:1049)
stream_grid_cell::stream_grid_cell()
{
    srid[0] = 0;
    srid[1] = 0;
    srid[2] = 0;
    srid[3] = 0;
    stream_mask = 0xFFFFFFFF;
}

// 0x0091AAB0 (:1058)
void stream_grid_cell::clear()
{
    for (int i = 0; i < 4; i++)
    {
        if (srid[i] != 0)
        {
            stream2_container_delete(srid[i]);
        }
    }
    srid[0] = 0;
    srid[1] = 0;
    srid[2] = 0;
    srid[3] = 0;
    stream_mask = 0xFFFFFFFF;
}

// 0x0091AB20 (:1118)
void stream_grid_cell::get_container_name(char* fname, char* h_fname, char* v_fname, char* precache_fname)
{
    if (fname != nullptr)
    {
        sprintf(fname, "%d_c%02d%02d", layer_index, grid_x, grid_z);
    }
    if (h_fname != nullptr)
    {
        sprintf(h_fname, "%dh_c%02d%02d", layer_index, grid_x, grid_z);
    }
    if (v_fname != nullptr)
    {
        sprintf(v_fname, "%dv_c%02d%02d", layer_index, grid_x, grid_z);
    }
    if (precache_fname != nullptr)
    {
        sprintf(precache_fname, "%dp_c%02d%02d", layer_index, grid_x, grid_z);
    }
}

// 0x0091ABC0 (:1301)
unsigned int stream_grid_cell::get_streaming_srid()
{
    // Packfiles: the h or v container that is being streamed; otherwise the base container.
    if (!Use_packfiles)
    {
        return srid[0];
    }
    if (srid[1] == 0)
    {
        if (srid[2] == 0)
        {
            return 0;
        }
    }
    else if (stream2_get_status(srid[1]) != ST_UNLOADED)
    {
        return srid[1];
    }
    if (srid[2] != 0 && stream2_get_status(srid[2]) != ST_UNLOADED)
    {
        return srid[2];
    }
    return 0;
}

// ---- stream_layer

// 0x009293A0 (:1330)
stream_layer::stream_layer()
{
    cells = nullptr;
    num_cells = 0;
    dim_z = -1;
    dim_x = -1;
    cell_dim = ZERO_VECTOR;
}

// 0x00921A60 (:1338)
void stream_layer::clear()
{
    for (int i = 0; i < masked_cells.size(); i++)
    {
        for (int r = 0; r < 4; r++)
        {
            if (masked_cells[i].n_region[r] != nullptr)
            {
                havok_bpo_delete(masked_cells[i].n_region[r]);
                masked_cells[i].n_region[r] = nullptr;
            }
        }
    }
    masked_cells.remove_all();
    for (int i = 0; i < active_cells.size(); i++)
    {
        stream_grid_cell* const cell = active_cells[i];
        for (int s = 0; s < 4; s++)
        {
            if (cell->srid[s] != 0 && stream2_get_status(cell->srid[s]) != ST_UNLOADED)
            {
                stream2_release(cell->srid[s]);
            }
        }
    }
    active_cells.remove_all();
    for (int i = 0; i < num_cells; i++)
    {
        if (cells[i] != nullptr)
        {
            cells[i]->clear();
        }
    }
}

// 0x0091AC20 (:1380)
void stream_layer::get_cell_bbox(vector& _bmin, vector& _bmax, stream_grid_cell* cell)
{
    _bmin.x = (float)cell->grid_x * cell_dim.x + bmin.x;
    _bmin.y = -10000.0f;
    _bmin.z = cell_dim.z * (float)cell->grid_z + bmin.z;
    _bmax.x = cell_dim.x + _bmin.x;
    _bmax.y = 10000.0f;
    _bmax.z = _bmin.z + cell_dim.z;
}

// 0x0091ACF0 (:1398)
stream_grid_cell* stream_layer::get_grid_cell(int x, int z)
{
    if (x < 0 || dim_x <= x || z < 0 || dim_z <= z)
    {
        return nullptr;
    }
    const int index = dim_x * z + x;
    if (index < 0 || index >= dim_x * dim_z)
    {
        return nullptr;
    }
    return cells[index];
}

// 0x00929410 (:1415)
int stream_layer::get_cell_list(const vector& _bmin, const vector& _bmax, bbox* cellsp, int* cell_indices, int max_cells)
{
    // The cells overlapping the box, row by row (not clamped to the grid).
    vector world_min;
    vector world_max;
    layout_world_box(world_min, world_max);
    const int x0 = (int)floor((double)(((_bmin.x + 0.005f) - world_min.x) / cell_dim.x));
    const int x1 = (int)floor((double)(((_bmax.x - 0.005f) - world_min.x) / cell_dim.x));
    const int z1 = (int)floor((double)(((_bmax.z - 0.005f) - world_min.z) / cell_dim.z));
    int count = 0;
    for (int z = (int)floor((double)(((_bmin.z + 0.005f) - world_min.z) / cell_dim.z)); z <= z1; z++)
    {
        for (int x = x0; x <= x1; x++)
        {
            if (cell_indices != nullptr)
            {
                cell_indices[count] = dim_x * z + x;
            }
            if (cellsp != nullptr)
            {
                bbox& box = cellsp[count];
                box.min.x = (float)x * cell_dim.x + world_min.x;
                box.min.y = 0.0f;
                box.min.z = (float)z * cell_dim.z + world_min.z;
                box.max = box.min;
                box.max.x = cell_dim.x + box.max.x;
                box.max.z = box.max.z + cell_dim.z;
            }
            count++;
            if (max_cells <= count)
            {
                return count;
            }
        }
    }
    return count;
}

// 0x00929840 (:1463)
void stream_layer::remove_cells(const vector& _bmin, const vector& _bmax)
{
    int indices[64];
    const int count = get_cell_list(_bmin, _bmax, nullptr, indices, 64);
    for (int i = 0; i < count; i++)
    {
        const int index = indices[i];
        if (index > -1 && cells[index] != nullptr)
        {
            cells[index]->clear();
            cells[indices[i]] = nullptr;
        }
    }
}

// 0x0091AD50 (:1482)
void stream_layer::calc_stream_box(vector& _bmin, vector& _bmax, const vector& pos)
{
    _bmin.x = pos.x - stream_radius;
    _bmin.y = -512.0f;
    _bmin.z = pos.z - stream_radius;
    _bmax.x = stream_radius + pos.x;
    _bmax.y = 512.0f;
    _bmax.z = stream_radius + pos.z;
}

// Bit of chk in src's stream mask: the (2 * load distance + 1)^2 neighbourhood, row-major, centred on src.
static int stream_mask_bit(const stream_layer& layer, const stream_grid_cell* src, const stream_grid_cell* chk)
{
    const int side = layer.cell_load_distance * 2 + 1;
    return ((int)src->grid_z - (int)chk->grid_z) * side + (side * side) / 2 - (int)src->grid_x + (int)chk->grid_x;
}

// 0x0091ADF0 (:1492)
bool stream_layer::is_masked(const stream_grid_cell* src, const stream_grid_cell* chk)
{
    return ((1u << (stream_mask_bit(*this, src, chk) & 0x1F)) & src->stream_mask) == 0;
}

// 0x0091AE40 (:1500)
bool stream_layer::toggle_mask(stream_grid_cell* src, const stream_grid_cell* chk)
{
    const int side = cell_load_distance * 2 + 1;
    const int half = (side * side) / 2;
    const int bit = stream_mask_bit(*this, src, chk);
    if (bit < 0 || bit > half * 2)
    {
        return false;
    }
    const unsigned int mask = 1u << (bit & 0x1F);
    src->stream_mask ^= mask;
    return (src->stream_mask & mask) != 0;
}

// 0x00929920 (:1517)
void stream_layer::create_cell_mask_region(stream_grid_cell* gc)
{
    // A phantom over the masked cell in every broadphase (the full safe height).
    for (int i = 0; i < masked_cells.size(); i++)
    {
        if (masked_cells[i].gc == gc)
        {
            return;
        }
    }
    vector region_min;
    vector region_max;
    region_max.x = cell_dim.x;
    region_min.x = (float)gc->grid_x * region_max.x + bmin.x;
    region_max.z = cell_dim.z;
    region_max.x = region_max.x + region_min.x;
    region_min.z = (float)gc->grid_z * region_max.z + bmin.z;
    region_max.z = region_max.z + region_min.z;
    region_min.y = Havok_safe_broadphase.min.y + 1.0f;
    region_max.y = Havok_safe_broadphase.max.y - 1.0f;
    masked_cell masked;
    masked.gc = gc;
    for (int r = 0; r < 4; r++)
    {
        masked.n_region[r] = nullptr;
    }
    for (int i = 0; i < havok_bpo_get_num_broadphases(); i++)
    {
        masked.n_region[i] = havok_bpo_create(i, region_min, region_max, (void*)0xFFFFFFFF, 0x40);
    }
    masked_cells.add(masked);
}

// 0x00921B80 (:1549)
void stream_layer::delete_cell_mask_region(stream_grid_cell* gc)
{
    for (int i = 0; i < masked_cells.size(); i++)
    {
        if (masked_cells[i].gc != gc)
        {
            continue;
        }
        masked_cell& masked = masked_cells[i];
        for (int r = 0; r < havok_bpo_get_num_broadphases(); r++)
        {
            if (masked.n_region[r] != nullptr)
            {
                havok_bpo_delete(masked.n_region[r]);
                masked.n_region[r] = nullptr;
            }
        }
        masked_cells.remove_unsorted(i);
        return;
    }
}

// 0x00921C50 (:1585)
void stream_layer::stream_new_cells(farray<stream_grid_cell*,25>& cellsp, unsigned char priority)
{
    // The base and precache containers of every new cell; with packfiles also the h/v containers, which cover a row
    // or a column of cells each: per new cell, whichever direction has more h or v containers is requested.
    unsigned int min_x = 1000000;
    int max_x = -1000000;
    unsigned int min_z = 1000000;
    int max_z = -1000000;
    for (int i = 0; i < cellsp.size(); i++)
    {
        stream_grid_cell* const cell = cellsp[i];
        if (cell->grid_x < min_x)
        {
            min_x = cell->grid_x;
        }
        if (max_x < (int)cell->grid_x)
        {
            max_x = cell->grid_x;
        }
        if (cell->grid_z < min_z)
        {
            min_z = cell->grid_z;
        }
        if (max_z < (int)cell->grid_z)
        {
            max_z = cell->grid_z;
        }
        if (cell->srid[0] != 0)
        {
            stream2_request(cell->srid[0]);
        }
        if (cell->srid[3] != 0)
        {
            stream2_set_priority(cell->srid[3], priority);
            stream2_request(cell->srid[3]);
        }
    }
    if (!Use_packfiles)
    {
        return;
    }

    // 5x5 window of the new cells, indexed [x * 5 + z].
    stream_grid_cell* window[25] = {};
    for (int i = 0; i < cellsp.size(); i++)
    {
        stream_grid_cell* const cell = cellsp[i];
        window[(cell->grid_x - min_x) * 5 + cell->grid_z - min_z] = cell;
    }
    const int num_z = max_z - (int)min_z;
    const int num_x = max_x - (int)min_x;
    for (int z = 0; z <= num_z; z++)
    {
        for (int x = 0; x <= num_x; x++)
        {
            if (window[x * 5 + z] == nullptr)
            {
                continue;
            }
            int h_count = 0;
            for (int x2 = x; x2 <= num_x; x2++)
            {
                if (window[x2 * 5 + z] != nullptr && window[x2 * 5 + z]->srid[1] != 0)
                {
                    h_count++;
                }
            }
            int v_count = 0;
            for (int z2 = z; z2 <= num_z; z2++)
            {
                if (window[x * 5 + z2] != nullptr && window[x * 5 + z2]->srid[2] != 0)
                {
                    v_count++;
                }
            }
            if (h_count < v_count)
            {
                for (int z2 = z; z2 <= num_z; z2++)
                {
                    stream_grid_cell*& cell = window[x * 5 + z2];
                    if (cell == nullptr)
                    {
                        continue;
                    }
                    const unsigned int srid = cell->srid[2] != 0 ? cell->srid[2] : cell->srid[1];
                    if (srid != 0)
                    {
                        stream2_set_priority(srid, priority);
                        stream2_request(srid);
                        cell = nullptr;
                    }
                }
            }
            else
            {
                for (int x2 = x; x2 <= num_x; x2++)
                {
                    stream_grid_cell*& cell = window[x2 * 5 + z];
                    if (cell == nullptr)
                    {
                        continue;
                    }
                    const unsigned int srid = cell->srid[1] != 0 ? cell->srid[1] : cell->srid[2];
                    if (srid != 0)
                    {
                        stream2_set_priority(srid, priority);
                        stream2_request(srid);
                        cell = nullptr;
                    }
                }
            }
        }
    }
}

// 0x00921F00 (:1704)
void stream_layer::release_old_cells(farray<stream_grid_cell*,25>& cellsp)
{
    for (int i = 0; i < cellsp.size(); i++)
    {
        stream_grid_cell* const cell = cellsp[i];
        for (int s = 0; s < 4; s++)
        {
            if (cell->srid[s] != 0 && stream2_get_ref_count(cell->srid[s]) > 0)
            {
                stream2_release(cell->srid[s]);
            }
        }
    }
}

// ---- stream_grid

// 0x00929B70 (:1750)
stream_grid::stream_grid(mempool_base* pool)
{
    layers = (stream_layer*)pool->alloc(NUM_LAYERS * sizeof(stream_layer), 0);
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        new (&layers[i]) stream_layer();
    }
    compact_ts.invalidate();
    compact_wait_ts.invalidate();
    m_tmp_table = nullptr;
    terr = nullptr;
    compact_super_emergency = false;
    ns_base = 0;
    ns_precache = 0;
    landmarks_srid = 0;
}

// Releases (when loaded) and deletes a territory-wide container.
static void layout_delete_container(unsigned int& srid)
{
    if (srid != 0)
    {
        if (stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        stream2_container_delete(srid);
    }
    srid = 0;
}

// 0x00921F60 (:1770)
void stream_grid::clear()
{
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        layers[i].clear();
    }
    layout_delete_container(ns_base);
    layout_delete_container(ns_precache);
    layout_delete_container(landmarks_srid);
    terr = nullptr;
    compact_super_emergency = false;
    compact_ts.invalidate();
    compact_wait_ts.invalidate();
}

// 0x00930FB0 (:3790)
void stream_grid::do_frame(const vector& stream_pos, bool single_zone_mode)
{
    // The level cell under the stream position masks level cells (stream_mask).
    stream_grid_cell* current = nullptr;
    int current_index;
    if (layers[LAYER_LEVEL].get_cell_list(stream_pos, stream_pos, nullptr, &current_index, 1) > 0 && layers[LAYER_LEVEL].cells[current_index] != nullptr)
    {
        current = layers[LAYER_LEVEL].cells[current_index];
    }

    for (int l = LAYER_TERRAIN; l >= LAYER_LEVEL; l--)
    {
        stream_layer& layer = layers[l];
        const unsigned char priority = (unsigned char)(l == LAYER_TERRAIN ? STREAM_PRIORITY_TERRAIN : STREAM_PRIORITY_NORMAL);
        farray<stream_grid_cell*,25> in_range;
        farray<stream_grid_cell*,25> masked;
        farray<stream_grid_cell*,25> new_cells;
        farray<stream_grid_cell*,25> old_cells;
        layer.calc_stream_box(layer.stream_bmin, layer.stream_bmax, stream_pos);
        int indices[256];
        bbox boxes[256];
        const int count = layer.get_cell_list(layer.stream_bmin, layer.stream_bmax, boxes, indices, 256);
        for (int i = 0; i < count; i++)
        {
            stream_grid_cell* const cell = layer.cells[indices[i]];
            if (cell == nullptr)
            {
                continue;
            }
            if (current == nullptr || l != LAYER_LEVEL || !layer.is_masked(current, cell))
            {
                in_range.add(cell);
            }
            else
            {
                masked.add(cell);
            }
        }
        for (int i = 0; i < masked.size(); i++)
        {
            layer.create_cell_mask_region(masked[i]);
        }
        for (int i = 0; i < in_range.size(); i++)
        {
            stream_grid_cell* const cell = in_range[i];
            layer.delete_cell_mask_region(cell);
            if (layer.active_cells.find(cell, false) < 0)
            {
                new_cells.add(cell);
            }
        }
        if (new_cells.size() > 0)
        {
            layer.stream_new_cells(new_cells, priority);
        }
        for (int i = 0; i < layer.active_cells.size(); i++)
        {
            stream_grid_cell* const cell = layer.active_cells[i];
            if (in_range.find(cell, false) < 0 || masked.find(cell, false) >= 0)
            {
                old_cells.add(cell);
            }
        }
        if (old_cells.size() > 0)
        {
            layer.release_old_cells(old_cells);
        }
        layer.active_cells.remove_all();
        for (int i = 0; i < in_range.size(); i++)
        {
            layer.active_cells.add(in_range[i]);
        }
    }

    // The territory-wide containers stay requested.
    if (ns_base != 0 && stream2_get_ref_count(ns_base) == 0)
    {
        stream2_request(ns_base);
    }
    if (ns_precache != 0 && stream2_get_ref_count(ns_precache) == 0)
    {
        stream2_request(ns_precache);
    }
    if (landmarks_srid != 0 && stream2_get_ref_count(landmarks_srid) == 0)
    {
        stream2_request(landmarks_srid);
    }
    do_fragmentation_handling();
}

// 0x00922180 (:3910)
void stream_grid::do_fragmentation_handling()
{
    // When the world allocator reports fragmentation, the stream radii shrink (to half, then to 0.3 / 0.33 on a
    // second report) for a while; when the compact timer runs out the default radii come back and the cells whose
    // containers failed are released.
    if (compact_ts.valid())
    {
        if (!compact_ts.elapsed())
        {
            return;
        }
        set_stream_radius(LAYER_LEVEL, 340.0f);
        set_stream_radius(LAYER_TERRAIN, 510.0f);
        for (int l = LAYER_TERRAIN; l >= LAYER_LEVEL; l--)
        {
            stream_layer& layer = layers[l];
            for (int i = 0; i < layer.active_cells.size(); i++)
            {
                stream_grid_cell* const cell = layer.active_cells[i];
                bool failed = false;
                for (int s = 0; s < 4; s++)
                {
                    if (cell->srid[s] != 0 && stream2_get_status(cell->srid[s]) == ST_ERROR)
                    {
                        failed = true;
                    }
                }
                if (!failed)
                {
                    continue;
                }
                for (int s = 0; s < 4; s++)
                {
                    if (cell->srid[s] != 0)
                    {
                        stream2_release(cell->srid[s]);
                    }
                }
                layer.active_cells.remove_unsorted(i);
                i--;
            }
        }
        compact_ts.invalidate();
        Stream_world_allocator.set_frag_fail(false);
        return;
    }

    if (compact_wait_ts.valid() && !compact_wait_ts.elapsed())
    {
        return;
    }
    if (!Stream_world_allocator.get_frag_fail())
    {
        compact_super_emergency = false;
        compact_wait_ts.invalidate();
        return;
    }
    if (!compact_super_emergency)
    {
        set_stream_radius(LAYER_LEVEL, 170.0f);
        set_stream_radius(LAYER_TERRAIN, 336.6f);
        compact_super_emergency = true;
        compact_wait_ts.set(15000);
        compact_ts.set(5000);
        return;
    }
    set_stream_radius(LAYER_LEVEL, 102.00001f);
    set_stream_radius(LAYER_TERRAIN, 168.3f);
    compact_super_emergency = false;
    compact_wait_ts.set(20000);
    compact_ts.set(6500);
}

// 0x0091AEA0 (:3989)
void unused_dcf_force_compact()
{
    Stream_world_allocator.set_frag_fail(true);
}

// 0x0091AEB0 (:3995)
void stream_grid::set_stream_radius(layout_layer layer, float radius)
{
    // Keeps the stream box centred where it was.
    if (layer >= NUM_LAYERS)
    {
        return;
    }
    stream_layer& l = layers[layer];
    l.stream_radius = radius;
    vector center;
    center.x = (l.stream_bmin.x + l.stream_bmax.x) * 0.5f;
    center.y = 0.0f;
    center.z = (l.stream_bmin.z + l.stream_bmax.z) * 0.5f;
    l.calc_stream_box(l.stream_bmin, l.stream_bmax, center);
}

// 0x0091AFB0 (:4007)
float stream_grid::get_stream_radius(layout_layer layer)
{
    if (layer < NUM_LAYERS)
    {
        return layers[layer].stream_radius;
    }
    return 1.0f;
}

// ---- system and level

// 0x0091AFD0 (:4223)
// LTCG: tmp_pool in EAX.
static void stream_layout_load_mask_file(const char* filename, mempool_base* tmp_pool, vlib_platform plat)
{
    // Per level cell (layer 0, in cell order) the stream mask: header 0xDAEB, version 1, two unused words, count.
    if (Layout_grid == nullptr)
    {
        return;
    }
    mempool_mark_scope scope(tmp_pool);
    cfile* const f = cf_open_buffered(filename, tmp_pool, "rb", plat);
    if (f == nullptr)
    {
        return;
    }
    if (cf_read_uint32(f) != 0xDAEB || cf_read_uint32(f) != 1)
    {
        VLIB_CRITICAL_FAILURE();
        cf_close(f);
        return;
    }
    cf_read_uint32(f);
    cf_read_uint32(f);
    const int count = (int)cf_read_uint32(f);
    for (int i = 0; i < count; i++)
    {
        stream_grid_cell* const cell = Layout_grid->layers[LAYER_LEVEL].cells[i];
        const unsigned int mask = cf_read_uint32(f);
        if (cell != nullptr)
        {
            cell->stream_mask = mask;
        }
    }
    cf_close(f);
}

// 0x0091B0C0 (:4280)
void stream_layout_system_init(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat, int dlc_id, keen::TaskSystem* pTaskSystem)
{
    if (!Stream_layout_inited)
    {
        Stream_layout_inited = true;
        Layout_platform = plat;
    }
}

// 0x0091B0E0 (:4340)
void stream_layout_system_shutdown()
{
    if (Stream_layout_inited)
    {
        Stream_layout_inited = false;
    }
}

// Packfile name in the game build directory or the DLC's directory.
static void layout_packfile_name(char* name, int dlc_id, const char* game_format, const char* dlc_format, const char* file_name, const char* ext)
{
    if (dlc_id == -1)
    {
        _snprintf_safe(name, 0x100, game_format, "/gamebuild", file_name, ext);
    }
    else
    {
        _snprintf_safe(name, 0x100, dlc_format, dlc_path_callback((unsigned int)dlc_bundle_id_to_index(dlc_id)), file_name, ext);
    }
}

// 0x0092A860 (:4364)
bool stream_layout_level_init(const char* start_zone, bool streaming, int dlc_id)
{
    Layout_streaming = streaming;
    strcpy(Layout_territory_start_zone, start_zone);
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(start_zone, Layout_territory_name, grid_x, grid_y, grid_z);
    if (!Use_packfiles)
    {
        // The loose-file build adds the territory packfiles instead (not implemented, see the file comment).
        VLIB_CRITICAL_FAILURE();
        return false;
    }

    game_enable_standard_access();
    char name[256];
    if (streaming)
    {
        // <terr>_l0 and <terr>_l1, or <terr> itself.
        for (int i = 0; i < 2; i++)
        {
            if (dlc_id == -1)
            {
                _snprintf_safe(name, sizeof(name), "%s\\%s_l%d%s", "/gamebuild", Layout_territory_name, i, Vpp_platform_ext[Layout_platform]);
            }
            else
            {
                _snprintf_safe(name, sizeof(name), "%s%s_l%d%s", dlc_path_callback((unsigned int)dlc_bundle_id_to_index(dlc_id)), Layout_territory_name, i, Vpp_platform_ext[Layout_platform]);
            }
            if (!packfile_temp_add(name, nullptr, false, false))
            {
                sprintf(name, "%s%s", Layout_territory_name, Vpp_platform_ext[Layout_platform]);
                if (!packfile_temp_add(name, nullptr, false, false))
                {
                    VLIB_CRITICAL_FAILURE();
                    return false;
                }
            }
        }
    }
    else
    {
        layout_packfile_name(name, dlc_id, "%s\\%s%s", "%s%s%s", Layout_territory_name, Vpp_platform_ext[Layout_platform]);
        if (!packfile_temp_add(name, nullptr, false, false))
        {
            VLIB_CRITICAL_FAILURE();
            return false;
        }
    }

    // The precache packfile (*.precache found in the packfiles added so far).
    char pattern[64];
    _snprintf_safe(pattern, sizeof(pattern), "*%s", ".precache");
    const char* found;
    if (cf_find_pattern(&found, 1, pattern) == 1)
    {
        char precache_name[64];
        cf_drop_ext(precache_name, sizeof(precache_name), found);
        layout_packfile_name(name, dlc_id, "%s\\%s%s", "%s%s%s", precache_name, Vpp_platform_ext[Layout_platform]);
        if (!packfile_temp_add(name, nullptr, false, false))
        {
            VLIB_CRITICAL_FAILURE();
            return false;
        }
    }

    if (streaming)
    {
        // zonescript_<terr>, preloaded into the pregame split pool until the level is loaded.
        layout_packfile_name(name, dlc_id, "%s\\zonescript_%s%s", "%szonescript_%s%s", Layout_territory_name, Vpp_platform_ext[Layout_platform]);
        zonescript_preload_pool_ptr = salloc_pregame_split_lock(0x1400000, 0);
        if (zonescript_preload_pool_ptr == nullptr)
        {
            return false;
        }
        if (!packfile_temp_add(name, zonescript_preload_pool_ptr, false, true))
        {
            VLIB_CRITICAL_FAILURE();
            return false;
        }
    }
    game_disable_standard_access();
    return true;
}

// 0x00937CB0 (:4593)
stream_grid* stream_layout_level_load(mempool_base* pool, mempool_base* tmp_pool, void (*num_zones_cb)(int), void (*zone_cb)(const char*, zone_file*))
{
    char zone_ext[60];
    sprintf(zone_ext, "%s", ".rfgzone_pc");
    Layout_pool = pool;
    Layout_tmp_pool = tmp_pool;
    bool is_territory;
    const char* files[257];
    const int num_files = rfg_zonex_get_territory_files(is_territory, files, 257, Layout_territory_start_zone, zone_ext);
    stream_grid* const grid = sl_build_minimal_grid(Layout_territory_name, num_files, files, pool);
    Layout_grid = grid;
    if (!Use_packfiles)
    {
        // The loose-file build (num_zones_cb, stream_grid::add_zone) is not implemented, see the file comment.
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }

    // The containers of the cells and the territory come from the container records.
    char name[64];
    if (Layout_streaming)
    {
        sprintf(name, "%s_l0%s", Layout_territory_name, ".asm_pc");
        if (!stream2_read_container_data(name, tmp_pool, pool, false, false, false))
        {
            return nullptr;
        }
        sprintf(name, "%s_l1%s", Layout_territory_name, ".asm_pc");
    }
    else
    {
        sprintf(name, "%s%s", Layout_territory_name, ".asm_pc");
    }
    if (!stream2_read_container_data(name, tmp_pool, pool, false, false, false))
    {
        return nullptr;
    }
    for (int l = 0; l < NUM_LAYERS; l++)
    {
        stream_layer& layer = grid->layers[l];
        for (int i = 0; i < layer.num_cells; i++)
        {
            stream_grid_cell* const cell = layer.cells[i];
            if (cell == nullptr)
            {
                continue;
            }
            char base_name[64];
            char h_name[64];
            char v_name[64];
            char precache_name[64];
            cell->get_container_name(base_name, h_name, v_name, precache_name);
            cell->srid[0] = stream2_container_find(base_name);
            cell->srid[1] = stream2_container_find(h_name);
            cell->srid[2] = stream2_container_find(v_name);
            cell->srid[3] = stream2_container_find(precache_name);
        }
    }
    if (!Layout_streaming)
    {
        grid->ns_base = stream2_container_find("ns_base");
        grid->ns_precache = stream2_container_find("ns_precache");
    }

    // Each zone file is read for the zone callback; the data lives in tmp_pool until the callback returns.
    for (int i = 0; i < num_files; i++)
    {
        mempool_mark_scope scope(tmp_pool);
        unsigned char* const terrain_data = (unsigned char*)tmp_pool->alloc(0xFFFF, 0);
        char zone_name[64];
        terrain_stitch_info* stitch_infos = nullptr;
        int num_stitch_infos;
        cf_drop_ext(zone_name, sizeof(zone_name), files[i]);
        zone_file* const zone = sl_slurp_zone_and_terrain(zone_name, terrain_data, 0xFFFF, nullptr, nullptr, stitch_infos, num_stitch_infos, Layout_platform, tmp_pool);
        if (zone_cb != nullptr)
        {
            zone_cb(zone_name, zone);
        }
    }
    grid->landmarks_srid = stream2_container_find("Landmark LODs");

    char mask_name[64];
    cf_add_ext(mask_name, sizeof(mask_name), Layout_territory_name, Smask_platform_ext[Layout_platform], false);
    stream_layout_load_mask_file(mask_name, tmp_pool, Layout_platform);
    if (Use_packfiles && Layout_streaming)
    {
        // The zonescript packfile was only needed while loading.
        packfile_clear_last_temp_packfile();
        zonescript_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)zonescript_preload_pool_ptr);
        zonescript_preload_pool_ptr = nullptr;
    }
    grid->set_stream_radius(LAYER_LEVEL, 340.0f);
    grid->set_stream_radius(LAYER_TERRAIN, 510.0f);
    Layout_inited = true;
    return grid;
}

// 0x0091B110 (:4856)
void full_screen_map_image_level_shutdown()
{
    char name[64];
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            sprintf(name, "%s_%02d_%02d_map", Layout_territory_name, x, y);
            const unsigned int srid = stream2_container_find(name);
            if (srid != 0)
            {
                stream2_container_delete(srid);
            }
        }
    }
    sprintf(name, "%s_map", Layout_territory_name);
    const unsigned int srid = stream2_container_find(name);
    if (srid != 0)
    {
        stream2_container_delete(srid);
    }
}

// 0x009229B0 (:4881)
void stream_layout_level_shutdown()
{
    if (!Layout_inited)
    {
        return;
    }
    Layout_grid->clear();
    Layout_grid = nullptr;
    chunk_level_shutdown();
    full_screen_map_image_level_shutdown();
    packfile_clear_temp_packfiles();
    Layout_inited = false;
}

// 0x0091B1B0 (:4909)
stream_grid* stream_layout_get_grid()
{
    return Layout_inited ? Layout_grid : nullptr;
}
