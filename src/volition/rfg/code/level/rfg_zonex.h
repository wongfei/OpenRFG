// Original: volition/rfg/code/level/rfg_zonex.h
// Implementation: volition/rfg/code/level/rfg_zonex.cpp
//
// Zone loading on the game side (decompiled). zone_load_zone is the load callback of the GSP_ZONE primitive type: it
// takes a zone_file from Zones (object_pool of 32, file static), parses the CPU data in place (rfg_zonex_get_data) and
// registers it in Zone_filename_hash under the file name; zone_find looks it up, zone_unload_zone frees it. The
// persistent zone (<territory>_persistent) is built by merging the territory's zones (zone_create_persistent_zone_file,
// rfg_zonex_merge_persistent_zone_files). world::post_load_stream and world::serialize_zone read zones through
// Zone_filename_hash.
#pragma once

#include "volition/rfg/code/level/rfg_zonex_shared.h"

bool zone_load_zone(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x0093A4F0 (:42)
void zone_unload_zone(const char* filename, unsigned int srid); // 0x00931730 (:80)
void zone_system_init(); // 0x0091B580 (:95)
zone_file* zone_find(const char* filename); // 0x0092B280 (:100)
zone_file* zone_create_persistent_zone_file(const char* start_zone); // 0x0093A5D0 (:117)
void zone_free_persistent_zone_file(const char* start_zone); // 0x00931790 (:155)
