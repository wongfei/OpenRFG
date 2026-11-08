// Original: volition/rfg/code/level/rfg_zonex.cpp
//
// Zone files loaded by stream2 (decompiled). zone_load_zone parses the CPU data of a .rfgzone_pc / .layer_pc in place
// and registers the zone_file under the file name; the persistent zone of a territory ("<territory>_persistent_15_00")
// is built from the territory's "p_" zone files on disk (rfg_zonex_merge_persistent_zone_files) and registered the same
// way. Up to 32 zones at once (Zones pool).
//
// Zone_filename_hash is file static here, but world.cpp, world_serialize.cpp and world_restore.cpp read it too (LTCG
// inlined zone_find into them); the reimplementation of those files calls zone_find instead.

#include "volition/rfg/code/level/rfg_zonex.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

// ---- file-static globals
static object_pool<zone_file,32,1> Zones; // 0x02CEC200
static str_data_hash_table<zone_file*,32,64,unsigned char> Zone_filename_hash; // 0x02FE5110

// 0x0093A4F0 (:42)
bool zone_load_zone(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (Zones.get_free() < 1)
    {
        return false;
    }
    zone_file* zone = Zones.alloc(0);
    zone->data = cpu_data;
    zone->data_size = cpu_data_size;
    if (!rfg_zonex_get_data(zone, cpu_data, cpu_data_size))
    {
        Zones.free(zone);
        return false;
    }
    Zone_filename_hash.insert_key_data(filename, zone);
    return true;
}

// 0x00931730 (:80)
void zone_unload_zone(const char* filename, unsigned int srid)
{
    zone_file* zone = zone_find(filename);
    if (zone != nullptr)
    {
        Zone_filename_hash.remove(filename);
        Zones.free(zone);
    }
}

// 0x0091B580 (:95)
void zone_system_init()
{
}

// 0x0092B280 (:100)
zone_file* zone_find(const char* filename)
{
    zone_file* zone = nullptr;
    Zone_filename_hash.get(filename, zone);
    return zone;
}

// Name of the territory's persistent zone file: "<territory>_persistent_15_00.rfgzone_pc".
static void get_persistent_zone_name(char* name, int name_size, const char* start_zone)
{
    // Like the original, the territory name is looked up twice (the second call only to get the same name again).
    char territory_name[64];
    char territory_name2[64];
    int grid_x;
    int grid_y;
    int grid_z;
    rfg_zonex_get_territory_info(start_zone, territory_name, grid_x, grid_y, grid_z);
    rfg_zonex_get_territory_info(start_zone, territory_name2, grid_z, grid_y, grid_x);
    _snprintf_safe(name, (unsigned int)name_size, "%s_%s_%02d_%02d%s", territory_name, "persistent", 15, 0, ".rfgzone_pc");
}

// 0x0093A5D0 (:117)
zone_file* zone_create_persistent_zone_file(const char* start_zone)
{
    char name[64];
    get_persistent_zone_name(name, sizeof(name), start_zone);
    zone_file* zone = zone_find(name);
    if (zone == nullptr)
    {
        if (Zones.get_free() < 1)
        {
            return nullptr;
        }
        zone = Zones.alloc(0);
        if (zone == nullptr)
        {
            keen::debug::nativePrintArgs("[rfg_zonex.cpp] <zone_create_persistent_zone_file> failed to alloc Zone! \n");
        }
        Zone_filename_hash.insert_key_data(name, zone);
    }
    rfg_zonex_merge_persistent_zone_files(zone, start_zone, Level_mempool, Common_mempool, PLATFORM_PC);
    return zone;
}

// 0x00931790 (:155)
void zone_free_persistent_zone_file(const char* start_zone)
{
    char name[64];
    get_persistent_zone_name(name, sizeof(name), start_zone);
    zone_file* zone = nullptr;
    if (Zone_filename_hash.get(name, zone))
    {
        Zone_filename_hash.remove(name);
        Zones.free(zone);
    }
}
