// Original: volition/rfg/code/hud/hud_floating_text.h
// Implementation: volition/rfg/code/hud/hud_floating_text.cpp
//
// Floating text in the world (damage numbers, XP).
#pragma once

void hud_floating_text_system_init(); // 0x00842F00 (:106)
void hud_floating_text_level_init_post(); // 0x00842F30 (:120)
void hud_floating_text_render(); // 0x00842F40 (:128)
void hud_floating_text_add(float x, float y, const char* str, int r, int g, int b, int font_id); // 0x00842F90 (:140)
void hud_floating_text_add_centered(float cx, float cy, const char* str, int r, int g, int b, int font_id); // 0x00843070 (:163)
void hud_floating_text_process(); // 0x00843110 (:178)
