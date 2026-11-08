// Original: volition/rfg/code/level/undergrowth_shared.h
// Implementation: volition/rfg/code/level/undergrowth_shared.cpp
//
// Undergrowth data shared with the tools: the layer data of a territory (rfg_terrainx_shared.h holds the types).
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;

void undergrowth_load_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat); // 0x0091BF40 (:6)
