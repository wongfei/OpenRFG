// Original: volition/rfg/code/hud/fog_of_war.h
// Implementation: volition/rfg/code/hud/fog_of_war.cpp
//
// Fog of war of the maps (single player).
#pragma once


struct bbox;
struct cfile;
class rl_renderer;
class vector;

extern bool Fog_of_war_render; // 0x01660D77

void unused_dcf_fow_opacity(); // 0x0083F650 (:135)
void fog_of_war_init(); // 0x00850AB0 (:778)
void fog_of_war_clear(); // 0x0083F7F0 (:804)
void fog_of_war_update(const vector& center, float radius); // 0x0083F800 (:813)
bool fog_of_war_is_index_posn_obscured(int x, int z); // 0x0083FAD0 (:874)
bool fog_of_war_is_posn_obscured(const vector& posn); // 0x0083FAF0 (:887)
void fog_of_war_render(const bbox& viewing_area, const bbox& render_area); // 0x00867010 (:903)
void fog_of_war_rendering_alloc(rl_renderer& renderer); // 0x0087A5B0 (:986)
void fog_of_war_rendering_dealloc(); // 0x0083FB80 (:1014)
void fog_of_war_save(cfile* file); // 0x0083FC30 (:1048)
void fog_of_war_load(cfile* file); // 0x0083FCC0 (:1088)
