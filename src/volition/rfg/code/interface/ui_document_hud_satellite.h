// Original: volition/rfg/code/interface/ui_document_hud_satellite.h
// Implementation: volition/rfg/code/interface/ui_document_hud_satellite.cpp
//
// The satellite view module document (single player).
#pragma once

#include "volition/vlib/code/util/varray.h"

void hud_ui_satellite_create_data_items(); // 0x008D92D0 (:99)
void hud_ui_satellite_process_info(); // 0x008D97B0 (:277)
void hud_ui_satellite_process_target(); // 0x008987D0 (:412)
void hud_ui_satellite_enter(); // 0x00916EE0 (:454)
void hud_ui_satellite_exit(); // 0x00904BB0 (:485)
void hud_ui_satellite_load_items(bool assets_only); // 0x00910BD0 (:510)
void hud_ui_satellite_unload_items(bool assets_only); // 0x008F5680 (:528)
void hud_ui_satellite_process(); // 0x008F5700 (:549)
void hud_ui_satellite_get_doc_handles(farray<int,16>& handles); // 0x008C6630 (:598)
void hud_ui_satellite_render(); // 0x008D9E50 (:604)
void hud_ui_satellite_report_fire_taken(unsigned int veh_handle); // 0x008988A0 (:612)
void hud_ui_satellite_set_temp_message(const char* const msg, int display_ms); // 0x008988C0 (:621)
