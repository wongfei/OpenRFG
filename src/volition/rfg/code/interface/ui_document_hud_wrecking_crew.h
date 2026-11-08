// Original: volition/rfg/code/interface/ui_document_hud_wrecking_crew.h
// Implementation: volition/rfg/code/interface/ui_document_hud_wrecking_crew.cpp
//
// The wrecking crew HUD documents. Placement of hud_wrecking_crew_docs inferred.
//
// wrecking.md WC0 (rfg.pdb): the text sets, barrel rows, icon and player states that wrecking_crew.cpp and the mode
// files fill for this HUD, and its data item ids. Placement inferred (misc_classes.md M7 assigned them to this header;
// the PDB records no file for data types and enums).
#pragma once

enum hud_wrecking_crew_docs
{
    HWCD_NONE = -1,
    HWCD_MAIN = 0,
    HWCD_SPLASH = 1,
    HWCD_HINTS = 2,
    NUM_HUD_WRECKING_CREW_DOCS = 3,
};

enum hud_wrecking_crew_data_items
{
    HWCDI_NONE = -1,
    HWCDI_HEADER = 0,
    HWCDI_TIMER = 1,
    HWCDI_PLAYERS = 2,
    HWCDI_SCORE = 3,
    HWCDI_WEAPONS = 4,
    HWCDI_STAMP = 5,
    HWCDI_MESSAGE = 6,
    HWCDI_COUNTER = 7,
    HWCDI_STRESS = 8,
    HWCDI_SPLASH_TITLE = 9,
    HWCDI_SPLASH_INFO = 10,
    HWCDI_SPLASH_TURN_END = 11,
    HWCDI_SPLASH_HINTS = 12,
    HWCDI_SPLASH_BARRELS = 13,
    HWCDI_SPLASH_PLAYERS = 14,
    HWCDI_SPLASH_VETO = 15,
    HWCDI_HINTS = 16,
    NUM_HUD_WRECKING_CREW_DATA_ITEMS = 17,
};

enum hud_wrecking_crew_icon_state
{
    HWCIS_NORMAL = 0,
    HWCIS_PULSE = 1,
    HWCIS_FLASH = 2,
};

enum hud_wrecking_crew_player_state
{
    HWCPS_NORMAL = 0,
    HWCPS_DISABLED = 1,
    HWCPS_ACTIVE = 2,
};

// size 0x82
struct hud_wrecking_crew_text_set
{
    bool should_display; // 0x00
    wchar_t label[32]; // 0x02
    wchar_t text[32]; // 0x42
};

// size 0x48
struct hud_wrecking_crew_barrel_text_row
{
    wchar_t label[32]; // 0x00
    int value_one; // 0x40
    int value_two; // 0x44
};

// The fall guy barrel summary of the turn end splash.
// size 0x1B0
struct hud_wrecking_crew_barrel_text_set
{
    bool should_display; // 0x00
    hud_wrecking_crew_barrel_text_row row_one; // 0x04
    hud_wrecking_crew_barrel_text_row mini_row_one; // 0x4C
    hud_wrecking_crew_barrel_text_row row_two; // 0x94
    hud_wrecking_crew_barrel_text_row mini_row_two; // 0xDC
    hud_wrecking_crew_barrel_text_row row_three; // 0x124
    wchar_t final_row_label[32]; // 0x16C
    int final_row_value; // 0x1AC
};

void hud_ui_wrecking_crew_system_init(); // 0x0089A890 (:1258)
void hud_ui_wrecking_crew_enter(); // 0x00916FC0 (:1270)
void hud_ui_wrecking_crew_exit(); // 0x00904C50 (:1288)
void hud_ui_wrecking_crew_load_items(bool assets_only); // 0x00910EC0 (:1294)
void hud_ui_wrecking_crew_unload_items(bool assets_only); // 0x008F5DA0 (:1318)
void hud_ui_wrecking_crew_process(float dtime); // 0x008F5E40 (:1347)
void hud_ui_wrecking_crew_render(); // 0x008DA810 (:1449)
int hud_ui_wrecking_crew_get_doc_handle(hud_wrecking_crew_docs doc); // 0x0089A900 (:1472)
void hud_ui_wrecking_crew_show_stamp(const char* message); // 0x0089A920 (:1479)
void hud_ui_wrecking_crew_hide_stamp(); // 0x0089A9A0 (:1491)
void hud_ui_wrecking_crew_show_message(const char* title, const char* label); // 0x0089A9E0 (:1500)
void hud_ui_wrecking_crew_hide_message(); // 0x0089AA90 (:1513)
void hud_ui_wrecking_crew_show_counter(const wchar_t* text, bool is_numeric); // 0x0089AAD0 (:1522)
void hud_ui_wrecking_crew_hide_counter(); // 0x0089ABB0 (:1536)
