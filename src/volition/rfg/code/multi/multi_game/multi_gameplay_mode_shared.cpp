// Original: volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.cpp
//
// The gameplay mode names (display name for the menus, internal name for the tables).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"

// ---- file-static globals
// multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] (0x0167E4A8) is used here too; placed in multi_core_platform.cpp.

// 0x0094EDC0 (:50)
const char* multi_gameplay_mode_get_display_name(multi_gameplay_mode mode)
{
    if ((unsigned int)mode < MGPM_NUM_MODES)
    {
        return Multi_gameplay_mode_names[mode].display_name;
    }
    return nullptr;
}

// 0x0094EDE0 (:60)
const char* multi_gameplay_mode_get_internal_name(multi_gameplay_mode mode)
{
    if ((unsigned int)mode < MGPM_NUM_MODES)
    {
        return Multi_gameplay_mode_names[mode].internal_name;
    }
    return nullptr;
}

// 0x0094EE00 (:72): by display or internal name
multi_gameplay_mode multi_gameplay_mode_from_name(const char* name)
{
    for (int mode = 0; mode < MGPM_NUM_MODES; mode++)
    {
        if (v_stricmp(name, Multi_gameplay_mode_names[mode].display_name) == 0 || v_stricmp(name, Multi_gameplay_mode_names[mode].internal_name) == 0)
        {
            return (multi_gameplay_mode)mode;
        }
    }
    return MGPM_NONE;
}
