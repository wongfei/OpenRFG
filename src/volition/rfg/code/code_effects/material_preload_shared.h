// Original: volition/rfg/code/code_effects/material_preload_shared.h
// Implementation: volition/rfg/code/code_effects/material_preload_shared.cpp
//
// Material preloading shared with the tools: the stream data of preloaded materials.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;

void material_preload_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat); // 0x006E4A10 (:48)
void material_preload_shared_system_init(); // 0x006E4B20 (:85)
void material_preload_shared_system_shutdown(); // 0x006E4B50 (:98)
