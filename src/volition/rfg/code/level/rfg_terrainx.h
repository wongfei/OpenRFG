// Original: volition/rfg/code/level/rfg_terrainx.h
// Implementation: volition/rfg/code/level/rfg_terrainx.cpp
//
// Terrain loading on the game side. terrain_load_terrain (GSP_TERRAIN load callback) takes an rfg_terrain from Terrains
// (object_pool of 9, file static) and registers it in Terrain_filename_hash; terrain_instantiate creates the terrain
// object's physics (height field, stitch pieces, invisible barriers). terrain_load_subzone (GSP_TERRAIN_HIGH_LOD) takes
// an rfg_terrain_subzone from Terrain_subzones (81) and links it into its terrain_data (terrain_mark_subzone_loaded);
// world::monitor_grid instantiates loaded subzones near the streaming position. terrain_get_loading_stats reports the
// data sizes of a loaded terrain.
#pragma once

#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/vlib/code/memory/object_pool.h"

class object;

// size 0x44
struct rfg_terrain
{
    terrain_data* terrain_data_p; // 0x00
    char filename[64]; // 0x04
};

// size 0x14
struct rfg_terrain_subzone
{
    rfg_terrain_subzone(); // 0x0091FAB0

    terrain_subzone_data* subzone_data_p; // 0x00
    bool instanciated; // 0x04
    vector midpoint; // 0x08
};

// size 0x94
struct terrain_loading_stats
{
    // size 0x8
    struct data_stat
    {
        void set(int, int);

        int m_size; // 0x00
        int m_percent; // 0x04
    };

    char filename[64]; // 0x00
    int total_bytes; // 0x40
    data_stat physics; // 0x44
    data_stat hi_lod; // 0x4C
    data_stat low_lod; // 0x54
    data_stat path_finding; // 0x5C
    data_stat stitch_physics; // 0x64
    data_stat stitch_mesh_data; // 0x6C
    data_stat invisible_barrier_physics; // 0x74
    data_stat layer_map_data; // 0x7C
    data_stat undergrowth; // 0x84
    data_stat terrain_texgen; // 0x8C
};

extern object_pool<rfg_terrain_subzone,81,1> Terrain_subzones; // 0x02F97C60

void unused_dcf_test_terrain_cast_shadows(); // 0x0092AC40 (:84)
void unused_dcf_R_stitch_batching_toggle(); // 0x0091B200 (:150)
bool terrain_instantiate_subzone(rfg_terrain_subzone* terrain_subzone_p, terrain_data* terrain_data_p); // 0x009386F0 (:262)
bool terrain_deinstantiate_subzone(rfg_terrain_subzone* terrain_subzone_p, terrain_data* terrain_data_p); // 0x0092AFA0 (:338)
bool terrain_load_subzone(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00938970 (:370)
bool terrain_load_terrain(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00939180 (:621)
void terrain_unload_subzone(const char* filename, unsigned int srid); // 0x0093A0E0 (:1151)
void terrain_unload_terrain(const char* filename, unsigned int srid); // 0x0093A250 (:1211)
void terrain_system_init(); // 0x0091B370 (:1354)
rfg_terrain* terrain_find(const char* filename); // 0x0092B030 (:1364)
terrain_loading_stats* terrain_get_loading_stats(const char* filename); // 0x0091B380 (:1371)
bool terrain_instantiate(rfg_terrain* tp, object* objp); // 0x0092B070 (:1391)
void terrain_deinstantiate(rfg_terrain* tp); // 0x0092B1D0 (:1432)

// rfg_terrain_subzone's constructor is compiler generated; the placement of the runtime types here is inferred.
