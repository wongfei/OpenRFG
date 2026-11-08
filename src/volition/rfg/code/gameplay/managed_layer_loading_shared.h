// Original: volition/rfg/code/gameplay/managed_layer_loading_shared.h
// Implementation: volition/rfg/code/gameplay/managed_layer_loading_shared.cpp
//
// Shared between the game and the tools: managed_layer_loading_parse_shared turns every <managed_layer_info> of an xml
// file into a streamed container of the given type with one .layer_pc primitive per <layer_file_N>.
// XML_MANAGED_LAYER_FILE_NAME_TAGS is {"layer_file_0", ..., "layer_file_15"}. Placement of the global inferred.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
class mempool;

extern const char* XML_MANAGED_LAYER_FILE_NAME_TAGS[16]; // 0x016514B0

bool managed_layer_loading_parse_shared(const char* system_name, const char* xml_filename, const char* xml_tag_to_find, unsigned char container_type, mempool_base* tmp_pool, mempool* pool, bool preload, vlib_platform plat, bool is_temp, int dlc_id); // 0x0075E730 (:40)
