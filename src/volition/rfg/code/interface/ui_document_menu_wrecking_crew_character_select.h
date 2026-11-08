// Original: volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.h
// Implementation: volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.cpp
//
// The wrecking crew character select.
#pragma once

// The selected row (0x01664BFC, initial 1); the player is the row / 4. controller_options.cpp reads it to edit that
// player's controls.
extern int Player_character_select_selection;

void wccs_stream_in_image(const char* image_name); // 0x0089D220 (:242)
bool wccs_is_a_streamed_image(const char* target_name); // 0x0089D2B0 (:268)
void wccs_stream_out_image(const char* image_name); // 0x0089D2F0 (:291)
void wccs_mark_image_to_stream_out(const char* stream_out_image_name, const char* stream_in_image_name); // 0x0089D370 (:311)
void wccs_process_image_stream_system(); // 0x0089D3C0 (:331)
void wccs_drop_streamed_images(); // 0x0089D470 (:355)
void ui_document_menu_wrecking_crew_character_select_init(); // 0x009118D0 (:1190)
void ui_document_menu_wrecking_crew_character_controller_options_cancel(); // 0x0089D790 (:1208)
void ui_document_menu_wrecking_crew_character_controller_options_save(); // 0x0089D7A0 (:1212)
int ui_document_menu_wrecking_crew_character_get_player_index(); // 0x0089D7B0 (:1216)
