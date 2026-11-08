// Original: volition/rfg/code/gameplay/gameflow/gameflow_common.h
// Implementation: volition/rfg/code/gameplay/gameflow/gameflow_common.cpp
//
// Gameflow helpers shared by the gameflow managers: the object importance level names ("none", "low", "medium",
// "high") and their lookup.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/vlib/code/util/hash_table.h"

extern str_data_hash_table<unsigned int, 32, 32, unsigned char> Object_importance_table; // 0x02246040, name -> object_importance_level

object_importance_level gameflow_common_get_importance(const char* level_name); // 0x0078BC60 (:51)
