// Original: volition/rfg/code/hud/in_world_gps_shared.h
// Implementation: volition/rfg/code/hud/in_world_gps_shared.cpp
//
// The GPS route shared with the minimap.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

struct foliage_mesh;
class mempool_base;

void in_world_gps_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat); // 0x0084C170 (:72)
void in_world_gps_shared_system_init(); // 0x0084C250 (:100)
void in_world_gps_shared_system_shutdown(); // 0x0084C280 (:113)
foliage_mesh* in_world_gps_get_chevron_fmesh(); // 0x0084C2B0 (:126)
