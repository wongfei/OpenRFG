// Original: volition/rfg/code/interface/ui_document_save_load.h
// Implementation: volition/rfg/code/interface/ui_document_save_load.cpp
//
// The save and load game document (rfg_ui_load_game). Placement of save_load_ui_type inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_message_box.h"

struct game_save_info;
class rl_renderer;

enum save_load_ui_type
{
    SLUT_LOAD = 0,
    SLUT_SAVE = 1,
    SLUT_IMPORT_SELECT = 2,
    SLUT_IMPORT_WRITE = 3,
};

void qsort_imported_save_data(game_save_info* infos, unsigned int* slots, unsigned int count); // 0x0089DAF0 (:289)
void hud_ui_save_load_clear_list(); // 0x0089DD80 (:347)
void hud_ui_save_load_get_list(); // 0x0089DDB0 (:357)
const char* hud_ui_save_load_get_district_name(unsigned char zone_col, unsigned char zone_row); // 0x008C8FD0 (:413)
bool hud_ui_save_load_no_space_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x008B6770 (:427)
bool hud_ui_save_load_load_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x0089DF10 (:453)
bool hud_ui_save_load_save_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x0089DF80 (:470)
bool hud_ui_save_load_delete_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x0089E040 (:502)
void hud_ui_save_load_set_selected(int new_index, int current_index); // 0x0089E100 (:541)
void hud_ui_save_load_selection_made(bool reset_destruction); // 0x009076E0 (:570)
void hud_ui_save_load_process_scroll_bar(); // 0x008C9040 (:659)
void hud_ui_save_load_process_mouse(int num_items, int current_index); // 0x009079B0 (:727)
void hud_ui_save_load_process_input(); // 0x00907D80 (:883)
void hud_ui_save_load_create_header_data(); // 0x008C9300 (:1090)
void hud_ui_save_load_create_row_data(); // 0x008C93B0 (:1123)
void hud_ui_save_load_create_hints_data(); // 0x008C93E0 (:1136)
void hud_ui_save_load_create_data_items(); // 0x008C9470 (:1162)
void hud_ui_save_load_process_header(); // 0x008DBA90 (:1169)
void hud_ui_save_load_process_rows(); // 0x008EEF10 (:1264)
void hud_ui_save_load_proccess_hints(); // 0x008C94B0 (:1420)
void hud_ui_save_load_process_info(); // 0x008EF410 (:1530)
void hud_ui_save_load_init(); // 0x00917070 (:1544)
void hud_ui_save_load_enter(game_state prev_state); // 0x00908390 (:1559)
void hud_ui_save_load_exit(game_state new_state); // 0x008EF420 (:1611)
void hud_ui_save_load_handshake(rl_renderer*); // 0x0089E1B0 (:1657)
void hud_ui_save_load_process(); // 0x00912370 (:1717)
void hud_ui_save_load_render(rl_renderer*); // 0x008F6AF0 (:1798)
void hud_ui_save_load_show(save_load_ui_type type); // 0x0089E310 (:1810)
void hud_ui_save_load_kill_msgbox(); // 0x008EF5C0 (:1824)
