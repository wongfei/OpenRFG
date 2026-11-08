// Original: volition/rfg/code/geom/decal_info_shared.h
// Implementation: volition/rfg/code/geom/decal_info_shared.cpp
//
// The decal table parser shared with the tools.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;
struct pasm_data;

void decal_info_parse_table_shared(pasm_data* peg_data, mempool_base* tmp_pool, mempool* pool, mempool* peg_pool, vlib_platform plat); // 0x0080AE60 (:109)
