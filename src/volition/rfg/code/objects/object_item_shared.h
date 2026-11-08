// Original: volition/rfg/code/objects/object_item_shared.h
// Implementation: volition/rfg/code/objects/object_item_shared.cpp
//
// Item table parsing shared with the tools: the common part of an item entry, its asset container and peg, and the
// preload, multiplayer and backpack tags.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;

bool item_is_preload_tag(const char* tag); // 0x00A3CB60 (:347)
bool item_is_shared_backpack_tag(const char* tag); // 0x00A19860 (:354)
bool item_is_multiplayer_tag(const char* tag); // 0x00A19880 (:359)
bool item_build_peg(const char* streaming_cat); // 0x00A198A0 (:364)
bool item_build_container(const char* streaming_cat); // 0x00A198E0 (:369)

extern unsigned int Dlc_item_preload_cpu_mark; // 0x02533800
extern unsigned int Dlc_item_preload_gpu_mark; // 0x02533804
bool item_parse_table_shared(mempool_base* tmp_pool, mempool* container_pool, mempool* pool, bool multiplayer_pass, vlib_platform plat, bool verify_only, int dlc_id); // 0x00A66800 (:374)
