// Original: volition/render_lib/code/rl_material_data.h (the PDB records no header of this name; placement inferred)
// Implementation: volition/render_lib/code/rl_material_data.cpp
//
// Loading a material from a block of data (decompiled): rl_material_data_load resolves the texture, constant name and
// constant block offsets of a crunched rl_material_data and, with init_render_data, binds it to the renderer
// (rl_material_data_fixup: texture handles through bm_load, shader lookup, constant placement).
#pragma once

#include "volition/render_lib/code/rl_material_defines.h"

rl_material_data* rl_material_data_load(unsigned char* buf, int& offset, const char* texture_name_buffer, bool init_render_data); // 0x0053F020 (:189)

// A file static of rl_material_data.cpp in the original. It needs the shader library and the bitmap manager, so it is
// declared here and defined in rl_material_data_render.cpp (rfg_render); the headless tests provide a stand-in.
bool rl_material_data_fixup(rl_material_data* data, const char* texture_name_buffer); // 0x005273C0 (:32)
