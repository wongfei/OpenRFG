// Original: volition/rfg/code/interface/option_menus/option_menu_system.h
//
// The option menus without a header of their own: the main options menu, display (resolution and detail, the PC
// display settings Current_display_info and their cache, the resolution change stages), audio (volumes, voice chat),
// game options (camera, subtitles, ...), the PC page, the cheats and extras menus, and the Live options; the default
// hint objects of an options document. Placement of the types and functions inferred: the PDB records no header for
// these sources.
#pragma once

#include "keen/graphics/graphicssystem.hpp"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_message_box.h"

struct rfg_game_options;
class rl_renderer;
struct rl_screen_res;
class vint_object_base;

enum display_default_settings
{
    DISPLAY_DEFAULT_LOW = 0,
    DISPLAY_DEFAULT_MED = 1,
    DISPLAY_DEFAULT_HIGH = 2,
    DISPLAY_DEFAULT_VERY_HIGH = 3,
    DISPLAY_DEFAULT_CUSTOM = 4,
    NUM_DISPLAY_DEFAULT_SETTINGS = 5,
};

enum detail_option_pages
{
    DETAIL_OPTIONS_PAGE_INVALID = -1,
    DETAIL_OPTIONS_PAGE_GENERAL = 0,
    DETAIL_OPTIONS_PAGE_ADVANCED = 1,
    NUM_DETAIL_OPTIONS_PAGES = 2,
};

enum resolution_change_stage
{
    RC_STAGE_INVALID = -1,
    RC_STAGE1 = 0,
    RC_STAGE2 = 1,
    RC_STAGE3 = 2,
    RC_STAGE4 = 3,
    RC_STAGE5 = 4,
    RC_STAGE_UNLOAD = 5,
    RC_STAGE_CHANGE_RES = 6,
    RC_STAGE_RESET_UTILITY = 7,
    RC_STAGE_LOADED = 8,
    RC_STAGE_DELAY_FRAMES = 9,
    RC_STAGE_PHASE2_BEGIN = 10,
    RC_STAGE_REVERT_RESOLUTION = 11,
    RC_STAGE_EXIT = 12,
};

enum steam_achievement_ids
{
    SAI_WELCOME_TO_MARS = 1,
    SAI_YOURE_IN_THE_FACTION_NOW = 2,
    SAI_LIBERATE_PARKER = 3,
    SAI_LIBERATE_DUST = 4,
    SAI_LIBERATE_BADLANDS = 5,
    SAI_LIBERATE_OASIS = 6,
    SAI_LIBERATE_EOS = 7,
    SAI_SHOT_HEARD_FROM_EARTH = 8,
    SAI_ON_THE_SIDE = 9,
    SAI_JOINED_THE_FIGHT = 10,
    SAI_IN_IT_TO_WIN_IT = 11,
    SAI_GUERRILLA_FIGHTER = 12,
    SAI_HURRICANE_MASON = 13,
    SAI_WARP_SPEED = 14,
    SAI_DEMOLITION_MASTER = 15,
    SAI_LOST_MEMORIES = 16,
    SAI_WORKING_THE_LAND = 17,
    SAI_THE_TRUTH = 18,
    SAI_UNSTOPPABLE = 19,
    SAI_DISASTER_AREA = 20,
    SAI_BROKEN_SUPPLY_LINE = 21,
    SAI_POWER_TO_THE_PEOPLE = 22,
    SAI_TANK_BUSTER = 23,
    SAI_UP_CLOSE_AND_PERSONAL = 24,
    SAI_DEMOLITIONS_SQUAD = 25,
    SAI_FREED_SPACE = 26,
    SAI_1ST_RANKED_WIN = 27,
    SAI_1ST_SOCIAL_WIN = 28,
    SAI_DEADLY = 29,
    SAI_LAID_WASTE = 30,
    SAI_BUILDER = 31,
    SAI_SPECTATOR = 32,
    SAI_SIGHT_SEEING = 33,
    SAI_WORLD_TRAVELER = 34,
    SAI_WEAPONS_EXPERT = 35,
    SAI_NOVICE = 36,
    SAI_PROFICIENT = 37,
    SAI_VETERAN = 38,
    SAI_WINNER = 39,
    SAI_PLAYER = 40,
    SAI_ASSASSIN = 41,
    SAI_MYSTERY_SKILLS = 42,
    SAI_MYSTERY_EXPERTISE = 43,
    SAI_MYSTERY_MASTER = 44,
    SAI_BACKPACK_CERTIFICATION = 45,
    SAI_HIGH_AND_MIGHTY = 46,
    SAI_JUST_HAVING_FUN = 47,
    SAI_SERIOUS_COMPETITOR = 48,
    SAI_WRECKING_BALL = 49,
    SAI_RED_FACTION_MEMBER = 50,
    SAI_BOUND_BY_BLOOD = 51,
    SAI_FAMILY_VENGEANCE = 52,
    SAI_A_GREATER_PURPOSE = 53,
    SAI_DELIVERANCE_DEFENDER = 54,
    SAI_TUMBLING_DOWN = 55,
    SAI_MOBILE_BOMBS = 56,
    SAI_STRUCTURAL_INTEGRITY = 57,
    SAI_PURGE_THE_VALLEY = 58,
    SAI_ARES_BLOODLUST = 59,
    SAI_THE_POWER_OF_ONE = 60,
    NUM_STEAM_ACHIEVEMENT_IDS = 61,
};

// size 0x24
struct options_default_hint_objects
{
    options_default_hint_objects(); // 0x00888560 (.h:95)

    vint_object_base* group_objects[3]; // 0x00
    vint_object_base* icon_objects[3]; // 0x0C
    vint_object_base* text_objects[3]; // 0x18
};

// size 0x13C
struct display_info_pc
{

    rl_screen_info m_screen_info; // 0x00
    rl_level_of_detail m_level_of_detail; // 0xA0
    display_default_settings m_general_lod_setting; // 0x134
    bool m_subtitles : 1; // 0x138:0
    bool m_vsyncEnabled : 1; // 0x138:1
};

extern display_info_pc Current_display_info; // 0x02C0F400 (file static in the binary)
extern resolution_change_stage Res_change_state; // 0x0166423C

void options_init(); // 0x009198A0 (options_main.cpp:300)
void options_menu_enter(game_state prev_state); // 0x008E7FC0 (options_main.cpp:375)
void options_menu_exit(game_state new_state); // 0x008D6F80 (options_main.cpp:383)
void options_menu_process(); // 0x00918BA0 (options_main.cpp:394)
void options_menu_handshake(rl_renderer*); // 0x00892BB0 (options_main.cpp:437)
void options_menu_render(rl_renderer*); // 0x00903F10 (options_main.cpp:441)
bool options_menu_is_extras_menu(int selection); // 0x00892BC0 (options_main.cpp:458)
void display_info_clear_supported_resolutions(); // 0x00891B10 (options_display.cpp:483)
int display_info_find_resolution_mode(keen::DisplayMode new_mode, bool ignoreRefresh); // 0x00891B20 (options_display.cpp:488)
int display_info_find_resolution_mode(unsigned int width, unsigned int height, unsigned int rr_num, unsigned int rr_denom, bool ignoreRefresh); // 0x00891B70 (options_display.cpp:508)
bool resolution_is_supported(const keen::DisplayMode& mode); // 0x00891BC0 (options_display.cpp:518)
void display_info_add_resolution(const keen::DisplayMode& res); // 0x00891CD0 (options_display.cpp:534)
int display_info_get_current_resolution_mode(); // 0x008AF800 (options_display.cpp:676)
void display_info_add_supported_resolutions(); // 0x00892250 (options_display.cpp:685)
void display_info_add_supported_aa_settings(); // 0x008AF880 (options_display.cpp:708)
int display_info_find_aa_setting(rl_antialias_lod setting); // 0x00892350 (options_display.cpp:720)
keen::graphics::WindowMode display_get_window_mode(); // 0x008AF8D0 (options_display.cpp:733)
rl_screen_res display_info_get_default_resolution(); // 0x00892380 (options_display.cpp:739)
bool display_info_is_resolution_supported(keen::DisplayMode resolution); // 0x008923A0 (options_display.cpp:744)
bool display_options_load_from_cache(rfg_game_options* game_options, rl_screen_info& screen_info, rl_level_of_detail& lod, bool& vsync_enabled, int& camera_fov); // 0x00892400 (options_display.cpp:789)
bool display_options_write_to_cache(rfg_game_options* game_options); // 0x00892480 (options_display.cpp:802)
bool display_options_are_changes_different_from_defaults(); // 0x008AF9A0 (options_display.cpp:827)
void detail_options_create_tab_data_item(); // 0x008D6E80 (options_display.cpp:861)
void display_options_process_tab_data(); // 0x008924E0 (options_display.cpp:887)
void display_reset_defaults(); // 0x008C3F90 (options_display.cpp:928)
void set_detail_page(detail_option_pages page); // 0x009024F0 (options_display.cpp:1343)
void display_menu_enter(game_state prev_state); // 0x0090F800 (options_display.cpp:1475)
void display_menu_exit(game_state new_state); // 0x008F3FA0 (options_display.cpp:1528)
void display_menu_process_mouse_input(); // 0x00902AF0 (options_display.cpp:1566)
void display_menu_process_input(); // 0x00902C40 (options_display.cpp:1618)
void display_menu_process(); // 0x00915F60 (options_display.cpp:1646)
void display_menu_handshake(rl_renderer*); // 0x008AFC00 (options_display.cpp:1712)
void display_menu_render(rl_renderer*); // 0x00902D70 (options_display.cpp:1721)
void display_parse_presets(); // 0x00892850 (options_display.cpp:1741)
bool display_has_been_inited(); // 0x00892860 (options_display.cpp:1745)
void display_set_user_settings(int custom_gamma, int custom_brightness, int custom_contrast, int preset_choice); // 0x00892870 (options_display.cpp:1750)
void display_get_user_settings(int& custom_gamma, int& custom_brightness, int& custom_contrast, int& preset_choice); // 0x00892880 (options_display.cpp:1758)
void resolution_change_enter(game_state prev_state); // 0x008AFCF0 (options_display.cpp:1897)
void resolution_change_exit(game_state new_state); // 0x008C4360 (options_display.cpp:1906)
void resolution_change_process(); // 0x009160E0 (options_display.cpp:2045)
void resolution_change_handshake(rl_renderer*); // 0x008928B0 (options_display.cpp:2086)
void resolution_change_render(rl_renderer*); // 0x009030C0 (options_display.cpp:2090)
void resolution_change_render_thread(); // 0x008AFD10 (options_display.cpp:2095)
bool audio_are_changes_different_from_defaults(); // 0x00891460 (options_audio.cpp:243)
void audio_reset_defaults(); // 0x008C3E10 (options_audio.cpp:266)
float get_voicechat_volume(); // 0x008917F0 (options_audio.cpp:425)
void audio_menu_enter(game_state prev_state); // 0x0090F700 (options_audio.cpp:580)
void audio_menu_exit(game_state new_state); // 0x008F3EA0 (options_audio.cpp:612)
void audio_menu_create_menu(); // 0x009016E0 (options_audio.cpp:694)
void audio_menu_process(); // 0x00915DC0 (options_audio.cpp:754)
void audio_menu_handshake(rl_renderer*); // 0x00891900 (options_audio.cpp:832)
void audio_menu_render(rl_renderer*); // 0x00901CC0 (options_audio.cpp:837)
bool game_options_are_changes_different_from_defaults(); // 0x008928C0 (options_game_options.cpp:180)
void game_options_set_menu_selections(); // 0x008C4410 (options_game_options.cpp:204)
void game_options_reset_defaults(); // 0x008E77C0 (options_game_options.cpp:285)
bool game_options_reset_tips_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x008AFDB0 (options_game_options.cpp:303)
void game_options_menu_enter(game_state prev_state); // 0x0090FA00 (options_game_options.cpp:699)
void game_options_menu_exit(game_state new_state); // 0x008F40A0 (options_game_options.cpp:749)
void game_options_menu_process_mouse_input(); // 0x00903DD0 (options_game_options.cpp:780)
void game_options_menu_process(); // 0x00916200 (options_game_options.cpp:812)
void game_options_menu_handshake(rl_renderer*); // 0x00892B60 (options_game_options.cpp:930)
void game_options_menu_render(rl_renderer*); // 0x00903EE0 (options_game_options.cpp:938)
void options_menu_get_default_hint_elements(int doc_handle, options_default_hint_objects& objects); // 0x008D6FB0 (options_pc.cpp:9)
int options_menu_process_mouse_on_default_hints(options_default_hint_objects& objects); // 0x008C4880 (options_pc.cpp:26)
void cheats_menu_unlock_cheat_for_achievement(steam_achievement_ids achievement_id); // 0x0088F5F0 (cheats_menu.cpp:240)
void cheats_menu_enter(game_state prev_state); // 0x0090F140 (cheats_menu.cpp:374)
void cheats_menu_exit(game_state new_state); // 0x008F3B90 (cheats_menu.cpp:405)
void cheats_menu_process(); // 0x00915A00 (cheats_menu.cpp:464)
void cheats_menu_handshake(rl_renderer*); // 0x0088F650 (cheats_menu.cpp:503)
void cheats_menu_render(rl_renderer*); // 0x008FD460 (cheats_menu.cpp:508)
bool cheats_menu_cheats_unlocked(); // 0x0088F660 (cheats_menu.cpp:525)
void extras_menu_init(); // 0x009174E0 (extras_menu.cpp:280)
void extras_menu_enter(game_state prev_state); // 0x00890EB0 (extras_menu.cpp:296)
void extras_menu_exit(game_state new_state); // 0x008D6A80 (extras_menu.cpp:301)
void extras_menu_process(); // 0x00915CF0 (extras_menu.cpp:306)
void extras_menu_handshake(rl_renderer*); // 0x00890EC0 (extras_menu.cpp:333)
void extras_menu_render(rl_renderer*); // 0x00900CA0 (extras_menu.cpp:337)
void extras_menu_init_wc_maps(); // 0x00890ED0 (extras_menu.cpp:354)
void xbox_live_menu_enter(game_state prev_state); // 0x00892BD0 (options_xlive.cpp:87)
void xbox_live_menu_exit(game_state new_state); // 0x00892BE0 (options_xlive.cpp:92)
void xbox_live_menu_process(); // 0x00892BF0 (options_xlive.cpp:96)
void xbox_live_menu_render(rl_renderer*); // 0x00892CA0 (options_xlive.cpp:144)
void xbox_live_quickmatch_menu_enter(game_state prev_state); // 0x00892DE0 (options_xlive_quickmatch.cpp:86)
void xbox_live_quickmatch_menu_exit(game_state new_state); // 0x00892DF0 (options_xlive_quickmatch.cpp:91)
void xbox_live_quickmatch_menu_process(); // 0x00892E00 (options_xlive_quickmatch.cpp:95)
void xbox_live_quickmatch_menu_render(rl_renderer*); // 0x00892EA0 (options_xlive_quickmatch.cpp:139)
