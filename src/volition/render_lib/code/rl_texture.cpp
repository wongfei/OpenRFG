// Original: volition/render_lib/code/rl_texture.cpp
//
// The bmpman texture callbacks (bm_init gets rl_texture_register/unregister) and texture dimensions by handle. A
// handle with bit 24 set is a dynamic texture (rl_dyn_texture_base), any other one a bitmap handle.

#include "volition/render_lib/code/rl_texture.h"

#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/vlib/code/bmpman/bmpman.h"

// The handle bit of dynamic textures (rl_dyn_texture_base::m_tex_handle).
static const int RL_DYN_TEXTURE_HANDLE_FLAG = 0x1000000;

// 0x005D4710 (.h:20)
void rl_texture_register(bitmap_entry* be)
{
    rl_render_lib::get_instance()->load_texture(*be);
}

// 0x005D4730 (.h:28)
void rl_texture_unregister(bitmap_entry* be)
{
    rl_render_lib::get_instance()->unload_texture(*be);
}

// 0x00500470 (:6)
void rl_texture_get_dimensions(int tex_handle, int& w, int& h, float& scale_x, float& scale_y, int& mip_levels)
{
    if (tex_handle == -1)
    {
        return;
    }
    scale_x = 1.0f;
    scale_y = 1.0f;
    if ((tex_handle & RL_DYN_TEXTURE_HANDLE_FLAG) == 0)
    {
        bm_get_dimensions(tex_handle, w, h, scale_x, scale_y, mip_levels);
        return;
    }
    rl_dyn_texture_base* texture_p = rl_dyn_texture_base::find_by_handle(tex_handle); // inlined
    if (texture_p != nullptr)
    {
        texture_p->get_dimensions(w, h, mip_levels);
    }
}
