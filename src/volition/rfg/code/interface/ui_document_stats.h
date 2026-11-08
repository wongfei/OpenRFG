// Original: volition/rfg/code/interface/ui_document_stats.h
// Implementation: volition/rfg/code/interface/ui_document_stats.cpp
//
// The statistics screen (rfg_ui_stat_screen) of the main menu and pause menu. Placement of stat_screen_pages inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

enum stat_screen_pages
{
    SSP_STATS_PAGE = 0,
    SSP_ACHIEVEMENTS = 1,
    SSP_NUM_PAGES = 2,
};

void ui_stats_clean_up_stats_page(); // 0x0089E4E0 (:258)
void ui_stats_clean_up_achievements_page(); // 0x0089E520 (:271)
void ui_stats_set_up_new_page(stat_screen_pages new_page); // 0x0089E560 (:284)
void ui_stats_create_header_data(); // 0x008C9A10 (:323)
void ui_stats_create_tab_data(); // 0x008C9A90 (:343)
void ui_stats_create_toggle_data(); // 0x008C9B20 (:367)
void ui_stats_create_hint_data(); // 0x008C9BA0 (:387)
void ui_stats_create_row_data(); // 0x008C9C20 (:409)
void ui_stats_create_stat_row_data(); // 0x008C9C50 (:422)
void ui_stats_create_data_items(); // 0x008C9C80 (:435)
void ui_stats_process_header_data(); // 0x0089E5F0 (:446)
void ui_stats_process_tab_data(); // 0x0089E750 (:474)
void ui_stats_process_toggle_data(); // 0x0089E8A0 (:491)
void ui_stats_process_hint_data(); // 0x0089E960 (:505)
void ui_stats_process_stats(); // 0x0089EB40 (:534)
void ui_stats_process_achievements(); // 0x0089EFF0 (:705)
void ui_stats_process_data_items(); // 0x0089F390 (:794)
void ui_stats_process_mouse(); // 0x008CA1C0 (:972)
void ui_stats_process_input(); // 0x008CA230 (:1006)
void ui_stats_init(); // 0x009170C0 (:1175)
void ui_stats_enter(game_state prev_state); // 0x00908500 (:1188)
void ui_stats_exit(game_state prev_state); // 0x008EF610 (:1225)
void ui_stats_handshake(rl_renderer*); // 0x0089F3E0 (:1252)
void ui_stats_process(); // 0x009124D0 (:1256)
void ui_stats_render(rl_renderer*); // 0x008F6B20 (:1298)
void ui_stats_show(); // 0x0089F3F0 (:1307)
