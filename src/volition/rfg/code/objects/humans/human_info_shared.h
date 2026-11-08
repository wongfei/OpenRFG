// Original: volition/rfg/code/objects/humans/human_info_shared.h
// Implementation: volition/rfg/code/objects/humans/human_info_shared.cpp
//
// Shared with the tools: build_human_containers_shared creates one streamed container per human type (GSC_HUMAN) with
// its meshes, rig, morphs and the props of its items.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
class mempool;

void build_human_containers_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat, bool verify_only, int dlc_id); // 0x00A9D1E0 (:353)
