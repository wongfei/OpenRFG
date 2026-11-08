// Original: volition/rfg/code/vehicle/vehicle_table_parser_shared.h
// Implementation: volition/rfg/code/vehicle/vehicle_table_parser_shared.cpp
//
// Shared with the tools: build_vehicle_containers_shared creates one streamed container per vehicle (GSC_VEHICLE) with
// its .ccar_pc, animations and interaction data.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
class mempool;

void build_vehicle_containers_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat, bool verify_only, bool release_build, int dlc_id); // 0x00BA30C0 (:171)
