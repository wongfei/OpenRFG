// Original: volition/rfg/code/weapons/weapons_table_parser.h
// Implementation: volition/rfg/code/weapons/weapons_table_parser.cpp
//
// Reads the weapons table (weapons.xtbl and the multiplayer overrides) into the weapon_info array.
#pragma once

void unused_dcf_reload_weapons(); // 0x00BE4450 (:102)
void unused_dcf_max_weapon_unique_id(); // 0x00BDA210 (:121)
void weapons_read_table(bool mp_override, bool refresh, int dlc_id); // 0x00BDF8A0 (:1202)
