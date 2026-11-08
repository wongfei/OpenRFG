// Original: volition/vlib/code/os/vconsole.h
// Implementation: volition/vlib/code/os/vconsole.cpp
//
// Debug console and multiplayer text chat (decompiled behaviour):
// - console_init (from rfg_init_stage_1_done) copies the vconsole_config callbacks (drawing is done by the game,
//   see game_draw_console_* in rfg.h), clears the 256-line scrollback, loads the command history and starts in
//   VCM_CHAT mode. console_shutdown is registered with atexit.
// - Commands are static console_command objects ("dcf" = debug console function). The constructor registers the
//   command (setup_params) and stores the function for its dcf_type; DCF_TYPE_BOOL/INT/FLOAT/VECTOR3 commands act as
//   variables read with console_get_variable_value.
// - console_do_command(fmt, ...) runs one command line. console_run_script(name) runs "<name>.txt" or
//   "<name>.script" line by line, skipping // and /* */ comments; rfg_init_stage_2_done ends with
//   console_run_script("gamestart.txt").
// - Per frame: console_do_frame_process runs inside the sim/render handshake; console_do_frame(dosbox) processes
//   input and renders.
// - rfg.exe registers hud_on, hud_off, say and team_say; the console opens only for the multiplayer chat
//   (orig_main.md OM5: no console key in retail).
#pragma once

#include "volition/vlib/code/math/vector3.h"

class color;

enum vconsole_mode
{
    VCM_NORMAL = 0,
    VCM_CHAT = 1,
    VCM_TEAMCHAT = 2,
};

enum vchat_position
{
    VCP_INGAME = 0,
    VCP_LOBBY = 1,
    VCP_PARTY = 2,
    VCP_PARTY_RIGHT = 3,
    VCP_ENDGAME = 4,
    VCP_INGAME_SCOREBOARD = 5,
};

enum dcf_type
{
    DCF_TYPE_NONE = -1,
    DCF_TYPE_BOOL = 0,
    DCF_TYPE_INT = 1,
    DCF_TYPE_FLOAT = 2,
    DCF_TYPE_FUNC = 3,
    DCF_TYPE_VECTOR3 = 4,
};

// size 0x1C
class vconsole_config
{
public:
    vconsole_config(); // 0x00563C00 (.h:125)

    void (*draw_rect)(int, int, int, int, color&); // 0x00
    void (*draw_str)(const char*, int, int, int, int&, int&, color&); // 0x04
    void (*update_func)(vconsole_config&); // 0x08
    int safe_area_left; // 0x0C
    int screen_width; // 0x10
    int screen_height; // 0x14
    bool save_history; // 0x18
};

// size 0x14
class console_command
{
public:
    console_command(const char* _name, const char* _help, dcf_type _type, vector (*_func)()); // 0x005C4FC0 (:1148)
    console_command(const char* _name, const char* _help, dcf_type _type, float (*_func)()); // 0x005C4F90 (:1141)
    console_command(const char* _name, const char* _help, dcf_type _type, int (*_func)()); // 0x005C4F60 (:1134)
    console_command(const char* _name, const char* _help, dcf_type _type, bool (*_func)()); // 0x005C4F30 (:1127)
    console_command(const char* _name, const char* _help, dcf_type _type, void (*_func)()); // 0x005C4F00 (:1120)
private:
    int setup_params(const char* _name, const char* _help, dcf_type _type); // 0x005C4E60 (:1094)

public:
    const char* name; // 0x00
    const char* help; // 0x04
    // size 0x4
    union
    {
        void (*vfunc)(); // 0x00
        bool (*bfunc)(); // 0x00
        int (*ifunc)(); // 0x00
        float (*ffunc)(); // 0x00
        vector (*v3func)(); // 0x00
    } func; // 0x08
    dcf_type type; // 0x0C
    bool enabled; // 0x10
};

// size 0x10
struct console_variable_value
{
    console_variable_value(); // 0x00AE7AD0

    // size 0x4
    union
    {
        bool b; // 0x00
        int i; // 0x00
        float f; // 0x00
    } val; // 0x00
    vector v3val; // 0x04
};

extern bool Console_inited; // 0x01CE87A6
extern int Console_command; // 0x01CDB4F4, a console command is being run (not just listed)
extern unsigned int Console_arg_type; // 0x01CD2904, type bits of the last console_get_arg
extern int Console_arg_int; // 0x01CDB908, the value of the last integer console_get_arg
extern char Console_arg[256]; // 0x01CDD440, the text of the last console_get_arg
extern int Console_help; // 0x01CDB0E4, the command was asked for its help text
extern const char* Console_command_line; // 0x01CDB0E0, the command line being run
extern bool Console_active; // 0x01CE87B8
extern char Console_arg_org[256]; // 0x01CD29D0, the last console_get_arg as typed
extern int Console_status; // 0x01CDE554, the command is asked for its value
extern float Console_arg_float; // 0x01CE8458
extern int Console_offset_x; // 0x01519700
extern int repeate_key_interval; // 0x01519704
extern unsigned int last_key; // 0x01CE87C4
extern unsigned int last_key_last_repeat; // 0x01CE87C8

void unused_dcf_print_to_file(); // 0x005CC090 (:237)
void unused_dcf_print_to_tty(); // 0x005C4910 (:275)
void unused_dcf_console_close(); // 0x005BE230 (:350)
void unused_dcf_console_set_bg_color(); // 0x005BE260 (:362)
int get_chat_window_x(); // 0x0058C620 (:391)
int get_chat_window_y(); // 0x0058C630 (:409)
int get_chat_window_w(); // 0x0058C660 (:425)
int get_chat_window_h(); // 0x0058C670 (:442)
void console_render_gr(); // 0x005AE9E0 (:589)
void console_render_chat(); // 0x005AECB0 (:687)
void save_console_history(); // 0x005CC240 (:948)
int console_do_command(const char*, ...); // 0x005C5000 (:1174)
void console_output(const char* text, color* clr, bool chat_text); // 0x005AF380 (:1334)
void console_printf(const char*, ...); // 0x005AF470 (:1390)
void console_chat_printf(const char*, ...); // 0x005AF4B0 (:1403)
void console_process_dos(); // 0x005C5380 (:1417)
void console_process_gr(); // 0x005CC320 (:1479)
void console_do_frame_process(bool dosbox); // 0x005CF7E0 (:1991)
void console_do_frame_render(); // 0x005AF4F0 (:2016)
void console_do_frame(bool dosbox); // 0x005CF820 (:2046)
bool console_get_arg(unsigned int type, int optional); // 0x005AF510 (:2086)
void console_deactivate(); // 0x005AF950 (:2222)
void console_activate(); // 0x0058CAC0 (:2233)
bool console_is_active(); // 0x0058CAD0 (:2241)
bool console_is_moving(); // 0x0058CAE0 (:2248)
int console_get_bottom_y(); // 0x0058CAF0 (:2253)
void console_init(vconsole_config* init_config); // 0x005CD150 (:2265)
void unused_dcf_script(); // 0x005D02D0 (:2318)
void console_run_script(const char* script_filename); // 0x005CF880 (:2339)
void dc_printf(const char*, ...); // 0x005AF9A0 (:2416)
dcf_type console_get_type(const char* command); // 0x005C5440 (:2437)
bool console_command_enable(const char* command, bool enable); // 0x005C54B0 (:2458)
console_command* console_command_get_by_name(const char* command); // 0x005C5520 (:2482)
dcf_type console_get_variable_value(const char* command, console_variable_value* value); // 0x005C5590 (:2506)
char* str_back_find_char(char* begin, char* end, char needle); // 0x0058CB30 (:2553)
char* strncpy_intelligent(char* buffer, int buffer_size, const char* delims, char* source); // 0x0058CB50 (:2578)
int find_substring(const char* needle, const char* haystack); // 0x0058CC10 (:2613)
void unused_dcf_search(); // 0x005C56A0 (:2662)
bool console_command_bool(const char* command_name, const char* bool_variable_name, bool bool_variable); // 0x005AFA10 (:2812)
float console_command_float(const char* command_name, const char* float_variable_name, float& float_variable); // 0x005AFB10 (:2845)
int console_command_int(const char* command_name, const char* int_variable_name, int int_variable); // 0x005AFC10 (:2875)
void console_set_console_mode(vconsole_mode new_mode); // 0x0058CD30 (:2905)
vconsole_mode console_get_console_mode(); // 0x0058CD50 (:2921)
bool console_is_in_chat_mode(); // 0x0058CD60 (:2926)
void console_activate_chat_say(); // 0x005AFCF0 (:2935)
void console_activate_chat_teamsay(); // 0x005AFDD0 (:2954)
int get_prefix_render_width(const char* name, bool team); // 0x005AFEB0 (:2974)
int get_chat_num_character_in_line(const char* text, int max_width); // 0x005AFF50 (:2992)
void console_add_textchat_line(const char* player_name, const char* text_line, bool team_message, bool wrapped, color team_color, bool print_to_console); // 0x005B0060 (:3022)
bool console_is_chat_window_enabled(); // 0x0058CD80 (:3084)
void console_disable_chat_window(); // 0x0058CD90 (:3093)
void console_enable_chat_window(vchat_position chat_position); // 0x0058CDB0 (:3101)
void console_get_scrollup_region(vector& top_left, vector& bottom_right); // 0x0058CE00 (:3136)
void console_get_scrolldown_region(vector& top_left, vector& bottom_right); // 0x0058CFC0 (:3163)
void console_scrollup_mouse_over(bool over); // 0x0058D1B0 (:3187)
void console_scrolldown_mouse_over(bool over); // 0x0058D1C0 (:3196)
void console_scrollup_pressed(); // 0x0058D1D0 (:3205)
void console_scrolldown_pressed(); // 0x0058D210 (:3226)
void console_disable_chat_render(bool disable); // 0x0058D250 (:3244)
vchat_position get_console_chat_window_pos(); // 0x0058D260 (:3253)
void console_change_chat_window_pos(vchat_position chat_position); // 0x0058D270 (:3258)
