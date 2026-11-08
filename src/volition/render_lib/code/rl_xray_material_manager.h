// Original: volition/render_lib/code/rl_xray_material_manager.h
// Implementation: volition/render_lib/code/rl_xray_material_manager.cpp
//
// The x-ray vision material overrides.
#pragma once

unsigned char rl_xray_material_manager_register_material(unsigned int mat_handle); // 0x004D4C60 (:65)
void rl_xray_material_manager_unregister_material(unsigned int mat_handle); // 0x004D4C90 (:78)
void rl_xray_material_manager_init(); // 0x004D4CC0 (:89)
void rl_xray_material_manager_shutdown(); // 0x004D4D70 (:98)
unsigned int rl_xray_material_manager_lookup(unsigned char xray_material); // 0x004D4E20 (:107)
