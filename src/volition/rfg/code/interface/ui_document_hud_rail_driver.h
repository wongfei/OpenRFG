// Original: volition/rfg/code/interface/ui_document_hud_rail_driver.h
// Implementation: volition/rfg/code/interface/ui_document_hud_rail_driver.cpp
//
// The rail driver module document (single player).
#pragma once


class vector2;

extern bool hud_ui_rail_driver_render_enabled; // 0x01664A48

void hud_ui_rail_driver_calc_verts(); // 0x008D8DE0 (:79)
void hud_ui_rail_driver_create_data_items(); // 0x008D9200 (:151)
void hud_ui_rail_driver_process_info(); // 0x008C6450 (:174)
void hud_ui_rail_driver_load_items(bool assets_only); // 0x00910B00 (:221)
void hud_ui_rail_driver_unload_items(bool assets_only); // 0x008F5430 (:238)
void hud_ui_rail_driver_enter(); // 0x00910B70 (:262)
void hud_ui_rail_driver_exit(); // 0x008F54B0 (:272)
void hud_ui_rail_driver_process(); // 0x008F5540 (:279)
void hud_ui_rail_driver_render(); // 0x008D9280 (:339)
vector2* hud_ui_rail_driver_get_xray_verts(int& num_verts); // 0x008D92B0 (:346)
void hud_ui_rail_driver_set_visible(bool visible); // 0x00898740 (:354)
bool hud_ui_rail_driver_is_visible(); // 0x008987B0 (:383)
