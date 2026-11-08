// Original: volition/rfg/code/hud/hud_ui_handbook.h
// Implementation: volition/rfg/code/hud/hud_ui_handbook.cpp
//
// The handbook screen (entries and videos). Placement of the video types inferred.
#pragma once

#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"

struct cfile;
struct handbook_node;
struct mission_briefing_line_info;
class rl_renderer;
struct xml_element;

enum bink_state
{
    BINK_STATE_STOPPED = 0,
    BINK_STATE_PLAY = 1,
    BINK_STATE_PLAYING = 2,
    BINK_STATE_LOOPING_END = 3,
    BINK_STATE_PAUSED = 4,
};

// size 0x44
struct handbook_video_info
{
    void reset(); // 0x00847240 (:101)
    handbook_video_info(); // 0x00847290

    const char* filename; // 0x00
    mission_briefing_line_info* line_info; // 0x04
    bink_state state; // 0x08
    int handle; // 0x0C
    int num_frames; // 0x10
    vector2i position; // 0x14
    vector2i size; // 0x1C
    float alpha; // 0x24
    handbook_node* handbook_node; // 0x28
    human_voice_persona_handle curr_persona_handle; // 0x2C
    voice_line_handle curr_vl_handle; // 0x30
    bool playing_audio_line; // 0x34
    int which_line_index; // 0x38
    unsigned int mempool_mark; // 0x3C
    unsigned int mempool_mark_gpu; // 0x40
};

// size 0x2C
struct handbook_node
{
    // size 0x1
    struct handbook_flags
    {
        unsigned char is_video : 1; // 0x00:0
        unsigned char video_is_playing : 1; // 0x00:1
        unsigned char video_watched : 1; // 0x00:2
        unsigned char selectable : 1; // 0x00:3
        unsigned char viewed : 1; // 0x00:4
        unsigned char default_viewed : 1; // 0x00:5
        unsigned char default_unlocked : 1; // 0x00:6
    };

    void reset() // 0x00847200 (.h:67)
    {
        name = nullptr;
        name_crc = 0;
        description_crc = 0;
        info = nullptr;
        filename = nullptr;
        voice_line = INVALID_VOICE_LINE_HANDLE;
        index = -1;
        unlock_number = -1;
        flags.is_video = 0;
        flags.video_is_playing = 0;
        flags.video_watched = 0;
        flags.selectable = 0;
        flags.viewed = 0;
        flags.default_viewed = 0;
        flags.default_unlocked = 0;
        next = nullptr;
        child = nullptr;
    }

    const char* name; // 0x00
    unsigned int name_crc; // 0x04
    unsigned int description_crc; // 0x08
    const char* info; // 0x0C
    const char* filename; // 0x10
    voice_line_handle voice_line; // 0x14
    int index; // 0x18
    int unlock_number; // 0x1C
    handbook_flags flags; // 0x20
    handbook_node* next; // 0x24
    handbook_node* child; // 0x28
};

void unused_dcf_handbook_unlock_all(); // 0x00868880 (:248)
void unused_dcf_handbook_unlock(); // 0x00857040 (:259)
void unused_dcf_handbook_unlock_bftp(); // 0x00857070 (:268)
void hud_ui_handbook_add_node_to_list(handbook_node* root, handbook_node* new_node); // 0x008472A0 (:283)
handbook_node* hud_ui_handbook_find_dir_node(handbook_node* root, const char* dir_name); // 0x00868930 (:304)
void hud_ui_handbook_sort_children(handbook_node* root); // 0x008472E0 (:360)
int hud_ui_handbook_count_children(handbook_node* root, int cur_count); // 0x00847340 (:403)
int hud_ui_handbook_count_viewable_children(handbook_node* root, int cur_count); // 0x00847380 (:423)
handbook_node* hud_ui_handbook_find_page(int page_index, bool count_invisible); // 0x008473D0 (:445)
int hud_ui_handbook_count_viewable_pages(); // 0x00847450 (:466)
void hud_ui_handbook_set_page_as_newest(handbook_node* find_me); // 0x008474D0 (:487)
bool hud_ui_handbook_child_node_is_unread(handbook_node* root); // 0x00847610 (:553)
handbook_node* hud_ui_handbook_get_child(handbook_node* root, int index, int& depth, bool count_locked); // 0x00847650 (:580)
handbook_node* hud_ui_handbook_find_node(unsigned int name_crc); // 0x00847740 (:636)
handbook_node* hud_ui_handbook_find_node(int id); // 0x008477E0 (:676)
bool hud_ui_handbook_parse_entry(xml_element* xml_root, const char* parent_name, handbook_node* node); // 0x00847880 (:717)
void hud_ui_handbook_load_table_data(); // 0x00876FF0 (:815)
void hud_ui_handbook_load_tool_tips_data(); // 0x00868A60 (:867)
void hud_ui_handbook_pause_video(bool pause); // 0x00847B20 (:962)
void hud_ui_handbook_stop_video(); // 0x00857110 (:1007)
void hud_ui_handbook_trigger_video_line(int which_line); // 0x008571A0 (:1040)
void hud_ui_handbook_create_header_data(); // 0x008481E0 (:1405)
void hud_ui_handbook_create_page_tab_data(); // 0x00848260 (:1428)
void hud_ui_handbook_create_row_data(); // 0x00848300 (:1459)
void hud_ui_handbook_create_hints_data(); // 0x00848330 (:1472)
void hud_ui_handbook_create_page_data(); // 0x008483B0 (:1497)
void hud_ui_handbook_create_data_items(); // 0x00848540 (:1612)
void handbook_entry_set_viewed(handbook_node* node, int curr_time, int start_time); // 0x00848580 (:1625)
void hud_ui_process_mouse_input(); // 0x008485C0 (:1639)
void hud_ui_handbook_process_scroll_bar(); // 0x00848880 (:1779)
void hud_ui_handbook_process_input(); // 0x008575F0 (:1849)
void hud_ui_handbook_process_header(); // 0x00848A90 (:2055)
void hud_ui_handbook_process_page_tab(); // 0x00848B90 (:2075)
void hud_ui_handbook_process_rows(); // 0x00848CC0 (:2109)
void hud_ui_handbook_proccess_hints(); // 0x008491B0 (:2218)
void hud_ui_handbook_process_page(); // 0x00868AF0 (:2247)
void hud_ui_handbook_process_info(); // 0x00869010 (:2485)
bool hud_ui_handbook_exit_notify(game_state new_state); // 0x00857AB0 (:2500)
void hud_ui_handbook_init(); // 0x008770E0 (:2566)
void hud_ui_handbook_level_init_pre(); // 0x008493A0 (:2603)
void hud_ui_handbook_enter(game_state prev_state); // 0x00869030 (:2640)
void hud_ui_handbook_exit(game_state new_state); // 0x00849470 (:2714)
void hud_ui_handbook_handshake(rl_renderer*); // 0x00849480 (:2724)
void hud_ui_handbook_process(); // 0x008691E0 (:2788)
void hud_ui_handbook_render(rl_renderer*); // 0x00849560 (:2919)
bool hud_ui_handbook_show(); // 0x00849590 (:2929)
void hud_ui_handbook_level_shutdown(); // 0x008495E0 (:2944)
void hud_ui_handbook_do_frame(); // 0x008495F0 (:2949)
void hud_ui_handbook_save(cfile* save_file); // 0x00869470 (:2959)
void hud_ui_handbook_load(cfile* save_file); // 0x00849630 (:2991)
void hud_ui_handbook_unlock_entry(unsigned int name_crc); // 0x00849720 (:3036)
void hud_ui_handbook_unlock_entry(const char* name); // 0x00849770 (:3055)
void hud_ui_handbook_unlock_bftp_entry(int index); // 0x00849790 (:3064)
