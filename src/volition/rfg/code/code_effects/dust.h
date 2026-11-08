// Original: volition/rfg/code/code_effects/dust.h
// Implementation: volition/rfg/code/code_effects/dust.cpp
//
// Dust kicked up around the camera and by vehicles: the weather table's dust settings, the terrain effect material,
// start/end and per-frame update.
#pragma once


class rl_renderer;
struct xml_element;

void dust_enable(bool enabled); // 0x006E18D0 (:98)
void dust_start(); // 0x006E18E0 (:105)
void dust_end(); // 0x006E18F0 (:112)
void dust_update_render(void*, rl_renderer*); // 0x006E6F80 (:119)
void dust_update(); // 0x006EBBF0 (:279)
bool weather_internal_parse_dust(xml_element* dust); // 0x006E1B30 (:474)
void dust_level_init_pre(); // 0x006E1D10 (:547)
void dust_level_shutdown(); // 0x006E1D40 (:553)
void dust_system_init(); // 0x006E7560 (:560)
void dust_system_shutdown(); // 0x006E1D50 (:575)
bool dust_object_is_dust_effect(unsigned int obj); // 0x006E1D60 (:580)
