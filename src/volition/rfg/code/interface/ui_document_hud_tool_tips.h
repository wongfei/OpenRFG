// Original: volition/rfg/code/interface/ui_document_hud_tool_tips.h
// Implementation: volition/rfg/code/interface/ui_document_hud_tool_tips.cpp
//
// Tool tips (tool_tips.xtbl): the tip data, the shown instances, their locations and the tool tip document.
#pragma once

#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/time/timestamp.h"

struct cfile;
struct xml_element;

enum tool_tip_location
{
    INVALID_TOOL_TIP_LOCATION = -1,
    TOOL_TIP_LOCATION_NE = 0,
    TOOL_TIP_LOCATION_NW = 1,
    TOOL_TIP_LOCATION_SE = 2,
    TOOL_TIP_LOCATION_SW = 3,
    TOOL_TIP_LOCATION_C = 4,
    TOOL_TIP_LOCATION_NONE = 5,
    NUM_TOOL_TIP_LOCATIONS = 6,
};

// size 0x28
class tool_tip_base_data
{
public:
    // size 0x1
    struct tip_flags
    {
        bool pause_game : 1; // 0x00:0
        bool run_paused : 1; // 0x00:1
        bool minor_tip : 1; // 0x00:2
        bool save_to_profile : 1; // 0x00:3
        bool always_show : 1; // 0x00:4
    };

    tool_tip_base_data(); // 0x008AAC00 (.h:74)
    void reset(); // 0x00898A00 (:434)
    bool load(xml_element* xml_root); // 0x008DA270 (:450)

    checksum_stri name_crc; // 0x00
    const char* title; // 0x04
    unsigned int description_crc; // 0x08
    const char* icon; // 0x0C
    const char* animation; // 0x10
    tool_tip_location screen_location; // 0x14
    tool_tip_location arrow_location; // 0x18
    int num_times_to_play; // 0x1C
    float duration; // 0x20
    tip_flags flags; // 0x24
};

// size 0x30
struct tool_tip_instance
{
    void reset(); // 0x00888510 (.h:56)
    tool_tip_instance(); // 0x00888530 (.h:63)

    tool_tip_base_data* base_tool_tip; // 0x00
    char override_icon[32]; // 0x04
    float time_left; // 0x24
    bool running; // 0x28
    timestamp delay_before_displaying; // 0x2C
};

extern unsigned int Current_tool_tip; // 0x02C08074
extern tool_tip_instance Tool_tip_queue[4]; // 0x02C108D0
extern bool Tool_tips_enabled; // 0x01664A49
extern unsigned char Tool_tips_times_displayed[128]; // 0x02BF4490 (external since UI6)

void unused_dcf_display_tool_tip(); // 0x008EB700 (:116)
void unused_dcf_reset_tool_tips(); // 0x008B2B40 (:127)
event_result tool_tips_report_dialog_line_finished(const event_data& in_event, notify_info* info); // 0x00898950 (:138)
tool_tip_location get_tip_location_from_name(const char* name); // 0x00898980 (:153)
tool_tip_base_data* find_tool_tip(const checksum_stri& name_crc); // 0x008C67A0 (:165)
tool_tip_instance* get_available_tool_tip_slot(); // 0x008989C0 (:184)
void load_tool_tips_data(); // 0x008EB750 (:207)
void tool_tips_queue_process(float dtime); // 0x008D9F00 (:257)
void ui_tool_tips_doc_init(); // 0x00910D10 (:557)
void ui_tool_tip_process_info(); // 0x008EB900 (:601)
void ui_tool_tips_init(); // 0x00910E00 (:707)
void ui_tool_tips_init_2(); // 0x008EBE10 (:712)
void ui_tool_tips_add_tip(const char* name, const int delay_ms, const char* icon_image); // 0x008DA4F0 (:734)
void ui_tool_tips_add_tip(const checksum_stri& name_crc, const int delay_ms, const char* icon_image); // 0x008C6820 (:751)
void ui_tool_tips_remove_tip(const char* name); // 0x008B2B60 (:824)
void ui_tool_tips_remove_tip(const checksum_stri& name_crc); // 0x00898A30 (:835)
bool ui_tool_tips_tip_is_queued(const char* name); // 0x008B2B80 (:890)
bool ui_tool_tips_tip_is_queued(const checksum_stri& name_crc); // 0x00898B50 (:899)
bool ui_tool_tips_tip_has_been_played(const char* name, bool no_more_plays_remaining); // 0x008DA520 (:925)
bool ui_tool_tips_tip_has_been_played(const checksum_stri& name_crc, bool no_more_plays_remaining); // 0x008C6980 (:936)
bool ui_tool_tips_tips_are_enabled(); // 0x00898B80 (:955)
void ui_tool_tips_process(float dtime); // 0x008EBFE0 (:964)
void ui_tool_tips_render(); // 0x008DA590 (:989)
void ui_tool_tips_reset_data(); // 0x00898BA0 (:1007)
void ui_tool_tips_level_init_pre(); // 0x008DA5F0 (:1015)
void ui_tool_tips_level_shutdown(); // 0x00898BC0 (:1026)
void ui_tool_tips_shutdown(); // 0x008F5A70 (:1038)
void ui_tool_tips_system_save(cfile* save_file); // 0x008C69E0 (:1049)
void ui_tool_tips_system_load(cfile* save_file); // 0x008C6AD0 (:1073)
void ui_tool_tips_system_save_to_profile(); // 0x008C6BC0 (:1100)
void ui_tool_tips_system_load_from_profile(); // 0x008C6CA0 (:1127)
bool ui_tool_tips_is_active(); // 0x00898C50 (:1146)
const tool_tip_base_data* ui_tool_tips_get_first_tip(); // 0x008B2BC0 (:1153)
const tool_tip_base_data* ui_tool_tips_get_next_tip(const tool_tip_base_data* current_tip); // 0x008C6D80 (:1157)
