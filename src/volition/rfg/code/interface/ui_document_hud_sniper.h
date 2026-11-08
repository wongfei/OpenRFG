// Original: volition/rfg/code/interface/ui_document_hud_sniper.h
// Implementation: volition/rfg/code/interface/ui_document_hud_sniper.cpp
//
// The sniper scope module document.
#pragma once

void hud_ui_sniper_create_data_items(); // 0x008D9E70 (:58)
void hud_ui_sniper_process_info(); // 0x008C6670 (:80)
void hud_ui_sniper_load_items(bool assets_only); // 0x00910C40 (:116)
void hud_ui_sniper_unload_items(bool assets_only); // 0x008F5880 (:133)
void hud_ui_sniper_enter(); // 0x00910CB0 (:153)
void hud_ui_sniper_exit(); // 0x008F5900 (:163)
void hud_ui_sniper_process(); // 0x008F5960 (:170)
void hud_ui_sniper_render(); // 0x008D9EE0 (:220)
void hud_ui_sniper_set_visible(bool visible); // 0x008988F0 (:228)
bool hud_ui_sniper_is_visible(); // 0x00898930 (:249)
