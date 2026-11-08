// Original: volition/render_lib/code/rl_texture.h
// Implementation: volition/render_lib/code/rl_texture.cpp
//
// Texture functions of render_lib (bitmap handles to keen textures).
#pragma once


struct bitmap_entry;

void rl_texture_register(bitmap_entry* be); // 0x005D4710 (.h:20)
void rl_texture_unregister(bitmap_entry* be); // 0x005D4730 (.h:28)
void rl_texture_get_dimensions(int tex_handle, int& w, int& h, float& scale_x, float& scale_y, int& mip_levels); // 0x00500470 (:6)
