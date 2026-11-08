// Original: volition/rfg/code/gameplay/gameflow/gameflow_common.cpp
//
// Gameflow helpers (decompiled; header comment in gameflow_common.h).
//
// Partial: the importance table and gameflow_common_get_importance only.

#include "volition/rfg/code/gameplay/gameflow/gameflow_common.h"

// Built by the variadic constructor (0x0106B8F0).
str_data_hash_table<unsigned int, 32, 32, unsigned char> Object_importance_table( // 0x02246040
    "none", (unsigned int)OBJECT_IMPORTANCE_LEVEL_NONE,
    "low", (unsigned int)OBJECT_IMPORTANCE_LEVEL_LOW,
    "medium", (unsigned int)OBJECT_IMPORTANCE_LEVEL_MEDIUM,
    "high", (unsigned int)OBJECT_IMPORTANCE_LEVEL_HIGH,
    nullptr, 0u);

// 0x0078BC60 (:51)
// Unknown names are OBJECT_IMPORTANCE_LEVEL_NONE.
object_importance_level gameflow_common_get_importance(const char* level_name)
{
    unsigned int level;
    if (Object_importance_table.get(level_name, level))
    {
        return (object_importance_level)level;
    }
    return OBJECT_IMPORTANCE_LEVEL_NONE;
}
