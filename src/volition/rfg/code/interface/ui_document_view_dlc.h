// Original: volition/rfg/code/interface/ui_document_view_dlc.h
// Implementation: volition/rfg/code/interface/ui_document_view_dlc.cpp
//
// The DLC view of the extras menu.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

// The DLC levels the view lists (set by dlc_level_init too; placement inferred).
extern int Num_downloaded_levels; // 0x02C08190

void unused_dcf_refresh_dlc(); // 0x0089F400 (:185)
void ui_view_dlc_process_input(); // 0x008CA7D0 (:1434)
void ui_view_dlc_create_header_data(); // 0x008CA9F0 (:1560)
void ui_view_dlc_create_row_data(); // 0x008CAAA0 (:1592)
void ui_view_dlc_create_hints_data(); // 0x008CAAD0 (:1605)
void ui_view_dlc_create_data_items(); // 0x008CAB60 (:1631)
void ui_view_dlc_process_header_data(); // 0x0089F4B0 (:1638)
void ui_view_dlc_process_row_data(); // 0x0089F8D0 (:1689)
void ui_view_dlc_process_hints_data(); // 0x0089FCF0 (:1804)
void ui_view_dlc_process_data_items(); // 0x0089FE80 (:1830)
void ui_view_dlc_init(); // 0x00917110 (:1866)
void ui_view_dlc_enter(game_state prev_state); // 0x00908CD0 (:1880)
bool ui_view_dlc_exit_notify(game_state new_state); // 0x008B7240 (:1938)
void ui_view_dlc_exit(game_state prev_state); // 0x008EF900 (:1987)
void ui_view_dlc_handshake(rl_renderer*); // 0x0089FEA0 (:2011)
void ui_view_dlc_process(); // 0x00912600 (:2072)
void ui_view_dlc_render(rl_renderer*); // 0x008F6B50 (:2135)
void ui_view_dlc_show(); // 0x0089FF80 (:2147)
void ui_view_dlc_get_handles(int& doc, int& header, int& row_group, int& hints); // 0x0089FFB0 (:2152)
void ui_view_dlc_release_handles(); // 0x0089FFF0 (:2161)
bool dlc_menu_enabled(); // 0x008A0010 (:2209)
