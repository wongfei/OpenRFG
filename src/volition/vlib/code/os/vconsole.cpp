// Original: volition/vlib/code/os/vconsole.cpp
//
// orig_main.md OM5: whole. The developer console and the multiplayer text chat (see vconsole.h). The console is drawn
// by the game through vconsole_config's callbacks (rfg.cpp's game_draw_console_*); input comes from the vlib keyboard's
// typed-key ring (key.cpp). rfg.exe registers four commands: hud_on, hud_off (the HUD, not ported) and say, team_say
// (multi_game.cpp); retail never opens the console with a key (game_input_poll's debug key flag is never set), only
// the chat keys of a multiplayer game do (console_activate_chat_say/_teamsay).
// The functions that print through a buffer of vlib's thread-local block (TLS + 0x38, 2 KB) use a thread_local buffer.
// Notes (decompiled behaviour, kept as is):
// - console_process_gr's key repeat reads Key_states with the typed key code, modifier bits (0x1000 and up) included.
//   Deviation: codes past the 256 keys count as released here (rfg.exe reads unrelated memory there).
// - Entering a command compares it with Command_history[Command_history_last] before checking that the history has
//   an entry. Deviation: an empty history counts as a different command here (rfg.exe reads the memory before it).
// - The "!prefix" history recall and the history search compare case-insensitively up to the shorter length.

#include "volition/vlib/code/os/vconsole.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/input/inputsystem.hpp"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/parse.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/string_util.h"

#include <conio.h>
#include <math.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One line of the console scrollback. Placement inferred.
// size 0x5F
struct console_line
{
    console_line(); // 0x0058C600

    char text[91]; // 0x00
    color clr; // 0x5B
};

// One line of the text chat. Placement inferred.
// size 0x82
struct chat_console_line
{
    chat_console_line(); // 0x0058C610

    char player_name[33]; // 0x00
    char text[91]; // 0x21
    bool team_message; // 0x7C
    bool wrapped_line; // 0x7D
    color clr; // 0x7E
};

enum char_code
{
    LETTER = 0,
    QUOTE = 1,
    SPECIAL = 2,
    EOF_CODE = 3,
    DIGIT = 4,
};

enum token_code
{
    NO_TOKEN = 0,
    IDENTIFIER = 1,
    NUMBER = 2,
    STRING = 3,
};

// keen key ids (the typed-key codes of key.cpp; bits 0x1000/0x2000/0x4000 are shift/ctrl/alt).
static const unsigned int CONSOLE_KEY_BACKSPACE = 0x0E;
static const unsigned int CONSOLE_KEY_TAB = 0x0F;
static const unsigned int CONSOLE_KEY_ENTER = 0x10;
static const unsigned int CONSOLE_KEY_ESCAPE = 0x12;
static const unsigned int CONSOLE_KEY_PAGE_UP = 0x14;
static const unsigned int CONSOLE_KEY_PAGE_DOWN = 0x15;
static const unsigned int CONSOLE_KEY_END = 0x16;
static const unsigned int CONSOLE_KEY_HOME = 0x17;
static const unsigned int CONSOLE_KEY_LEFT = 0x18;
static const unsigned int CONSOLE_KEY_UP = 0x19;
static const unsigned int CONSOLE_KEY_RIGHT = 0x1A;
static const unsigned int CONSOLE_KEY_DOWN = 0x1B;
static const unsigned int CONSOLE_KEY_DELETE = 0x1E;
static const unsigned int CONSOLE_KEY_F3 = 0x53;
static const unsigned int CONSOLE_KEY_ALTGR = 0x6B;
// Magic number: 0xFF, a second enter code (no key id maps to it).
static const unsigned int CONSOLE_KEY_ENTER_2 = 0xFF;
static const unsigned int CONSOLE_KEY_SHIFT_BACKSPACE = 0x100E;
static const unsigned int CONSOLE_KEY_SHIFT_ENTER = 0x1010;
static const unsigned int CONSOLE_KEY_ALT_BACKSPACE = 0x400E;

// Magic numbers: 256 lines of scrollback and of chat, 16 commands of history (1 KB each), 90 characters per line.
static const int Console_num_lines = 256;
static const int Console_history_size = 16;
static const int Console_max_line_length = 0x5A;
// Magic numbers: the console's height is 15% of the screen (2.2% in the chat modes), and the cursor blinks every 300 ms.
static const float Console_height_pct = 0.15f;
static const float Console_chat_height_pct = 0.022f;
static const int Console_cursor_blink_ms = 300;
// Magic number: chat lines stay 10 s.
static const int Chat_line_time_ms = 10000;
// Magic number: three chat lines are shown (fl_round(3.0)).
static const int Chat_num_visible_lines = 3;

// ---- globals (vconsole.h)
bool Console_inited; // 0x01CE87A6
int Console_command; // 0x01CDB4F4
unsigned int Console_arg_type; // 0x01CD2904
int Console_arg_int; // 0x01CDB908
char Console_arg[256]; // 0x01CDD440
int Console_help; // 0x01CDB0E4
const char* Console_command_line; // 0x01CDB0E0
bool Console_active; // 0x01CE87B8
char Console_arg_org[256]; // 0x01CD29D0
int Console_status; // 0x01CDE554
float Console_arg_float; // 0x01CE8458
int Console_offset_x = 25; // 0x01519700
int repeate_key_interval = 500; // 0x01519704
unsigned int last_key; // 0x01CE87C4
unsigned int last_key_last_repeat; // 0x01CE87C8

// ---- file-static globals
static bool Console_dosbox = true; // 0x01519592, cleared by the first console_do_frame(false)
static vchat_position Chat_position = VCP_LOBBY; // 0x015196E0
static int Command_history_last = -1; // 0x015196E4
static char Console_file[16] = "console_history"; // 0x015196E8
static const char* Scanner_bufferp = ""; // 0x015196F8
static char Scanner_token_string[128]; // 0x01CDE4D0
static char* Scanner_tokenp = Scanner_token_string; // 0x015196FC
static timestamp Chat_timer; // 0x0163E99C
static timestamp_realtime Console_cursor_timestamp; // 0x0163E9A0
static color Console_background_color(16, 16, 16, 128); // 0x0163E9A4
static color Textchat_background_color(66, 50, 36, 80); // 0x0163E9A8
static color Console_default_text_color(220, 220, 220, 255); // 0x0163E9B0
static int Textchat_lines_scroll_index; // 0x01CD2AD4
static char Scanner_ch; // 0x01CD2B1C
static bool Textchat_mouse_over_up; // 0x01CD2B1D
static bool Textchat_console_full; // 0x01CD2B1E
static bool Console_cursor_visible; // 0x01CD2B1F
static bool Textchat_mouse_over_down; // 0x01CD349F
static char_code Scanner_char_table[256]; // 0x01CD5FB8
static int Textchat_lines_next_index; // 0x01CDB0E8
static char Command_line_entered[1024]; // 0x01CDB0F0
static int Console_lines_next_index; // 0x01CDD7C0
static char Scanner_word_string[128]; // 0x01CDE3D0
static token_code Scanner_token; // 0x01CDE550
static int Console_lines_scroll_index; // 0x01CE25C4
static char Command_history[16][1024]; // 0x01CE3F78
static char Command_line_current[1024]; // 0x01CE8018
static jmp_buf Console_bad_arg; // 0x01CE8418
static bool Chat_rendering_disabled; // 0x01CE879B
static vconsole_mode Console_mode; // 0x01CE87A0
static bool Chat_window_enabled; // 0x01CE87A4
static bool Chat_window_enabled_ingame; // 0x01CE87A5
static bool Console_window_moving; // 0x01CE87A7
static int Num_console_commands; // 0x01CE87A8
static int Console_debug_on; // 0x01CE87AC
static unsigned int Command_line_xoffset; // 0x01CE87B0
static int Command_history_scroll; // 0x01CE87B4
static int Console_y; // 0x01CE87BC
static int Swallow_input_num_frames; // 0x01CE87C0
static chat_console_line Textchat_lines[256]; // 0x01D00CE0
static vconsole_config Config; // 0x01D08F1C, copy of the console_init argument
static console_line Console_line_current; // 0x01D0CAC0, the input line, starts with "] "
static console_line Console_lines[256]; // 0x01D50DA0
static bool find_failed; // 0x01D584DC (a function static of console_process_gr)
static bool in_find_mode; // 0x01D584DD (a function static of console_process_gr)

// The print buffer of vlib's thread-local block.
static thread_local char Console_print_buffer[2048];

// 0x0058C600
console_line::console_line()
{
}

// 0x0058C610
chat_console_line::chat_console_line()
{
}

// 0x00563C00 (.h:125)
vconsole_config::vconsole_config()
{
    draw_rect = nullptr;
    draw_str = nullptr;
    update_func = nullptr;
    safe_area_left = 0;
    screen_width = 0;
    screen_height = 0;
    save_history = false;
}

// The (wrapped) restart of the cursor's blink: shown for the next 300 ms.
static void console_reset_cursor_blink()
{
    Console_cursor_timestamp.set(Console_cursor_blink_ms);
    Console_cursor_visible = true;
}

// Forgets all key presses and the typed keys (the chat input starts clean).
static void console_flush_keys()
{
    for (int i = 0; i < 256; i++)
    {
        Key_states[i].down = false;
        Key_states[i].just_down = false;
        Key_states[i].last_repeat = 0;
        Key_states[i].time_down = 0;
    }
    Key_state_change_tail = 0;
    Key_state_change_head = 0;
    Key_buffer_tail = 0;
    Key_buffer_head = 0;
}

// The console's height in pixels for its mode.
static float console_height_pct()
{
    return Console_mode == VCM_NORMAL ? Console_height_pct : Console_chat_height_pct;
}

// 0x005CC090 (:237)
// Writes "name - help" of every command to console_commands.txt. Its command object was stripped (unused).
void unused_dcf_print_to_file()
{
    if (Console_command != 0)
    {
        cfile* p_cfile = cf_open("console_commands.txt", "wt", PLATFORM_PC, false);
        if (p_cfile == nullptr)
        {
            return;
        }
        for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
        {
            const console_command* command = e->def;
            if (command == nullptr)
            {
                continue;
            }
            char line[256];
            if (command->help == nullptr)
            {
                strcpy(line, command->name);
            }
            else
            {
                sprintf(line, "%s - %s\n", command->name, command->help);
            }
            cf_write(line, (unsigned int)strlen(line), p_cfile);
        }
        cf_close(p_cfile);
    }
    if (Console_help != 0)
    {
        console_output("Print out console commands to the text file console_commands.txt", nullptr, false);
    }
}

// 0x005C4910 (:275)
// Formats each command as "name   - help" in columns of 79 characters. Its output was stripped in the retail build:
// the lines are built and dropped. Its command object was stripped too (unused).
void unused_dcf_print_to_tty()
{
    unsigned int longest = 0xFFFFFFFF;
    if (Console_command != 0)
    {
        for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
        {
            if (e->def != nullptr)
            {
                const unsigned int len = (unsigned int)strlen(e->def->name);
                if (longest < len || longest == 0xFFFFFFFF)
                {
                    longest = len;
                }
            }
        }
        const int help_column = (int)longest + 3;
        for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
        {
            const console_command* command = e->def;
            if (command == nullptr)
            {
                continue;
            }
            char line[256];
            memset(line, ' ', sizeof(line));
            if (command->help == nullptr)
            {
                strcpy(line, command->name);
                for (char* p = line; *p != '\0'; p++)
                {
                    *p = (char)tolower(*p);
                }
            }
            else
            {
                sprintf(line, "%s", command->name);
                for (char* p = line; *p != '\0'; p++)
                {
                    *p = (char)tolower(*p);
                }
                line[strlen(line)] = ' ';
                sprintf(line + help_column, "- %s", command->help);
            }
        }
    }
    if (Console_help != 0)
    {
        console_output("Print out console commands to the tty window", nullptr, false);
    }
}

// 0x005BE230 (:350)
// Unused (command object stripped).
void unused_dcf_console_close()
{
    if (Console_command != 0)
    {
        console_deactivate();
    }
    if (Console_help != 0)
    {
        console_output("Close the console window", nullptr, false);
    }
}

// 0x005BE260 (:362)
// Unused (command object stripped). Red, green and blue required, alpha optional.
void unused_dcf_console_set_bg_color()
{
    if (Console_command != 0)
    {
        console_get_arg(8, 0);
        Console_background_color.red = (unsigned char)Console_arg_int;
        console_get_arg(8, 0);
        Console_background_color.green = (unsigned char)Console_arg_int;
        console_get_arg(8, 0);
        Console_background_color.blue = (unsigned char)Console_arg_int;
        if (console_get_arg(8, 1))
        {
            Console_background_color.alpha = (unsigned char)Console_arg_int;
        }
    }
    if (Console_help != 0)
    {
        console_output("Set the background color for the console", nullptr, false);
    }
}

// 0x0058C620 (:391)
int get_chat_window_x()
{
    return 5;
}

// 0x0058C630 (:409)
// Magic number: 92.6% of the screen height.
int get_chat_window_y()
{
    if ((int)Chat_position > -1 && (int)Chat_position < 6)
    {
        return (int)((float)Config.screen_height * 0.926f);
    }
    return 0;
}

// 0x0058C660 (:425)
int get_chat_window_w()
{
    return 14;
}

// 0x0058C670 (:442)
// Magic numbers: 54 pixels at 768 lines.
int get_chat_window_h()
{
    return (int)(((float)Config.screen_height * 54.0f) / 768.0f);
}

// The chat window's height as the chat renderer computes it (double precision).
static int chat_window_h_d()
{
    return (int)(((double)(float)Config.screen_height * 54.0) / 768.0);
}

// 0x0058C6A0 (:458)
static void scanner_get_char()
{
    Scanner_ch = *Scanner_bufferp;
    if (Scanner_ch != '\0')
    {
        Scanner_bufferp++;
    }
}

// 0x0058C6C0 (:468)
// Letters (with _ $ ? ! # : * \), digits (with . - +), the quote and the end; everything else is special.
static void scanner_init()
{
    for (int i = 0; i < 256; i++)
    {
        Scanner_char_table[i & 0xFF] = SPECIAL;
    }
    for (int i = '0'; i <= '9'; i++)
    {
        Scanner_char_table[i] = DIGIT;
    }
    for (int i = 'A'; i <= 'Z'; i++)
    {
        Scanner_char_table[i] = LETTER;
    }
    for (int i = 'a'; i <= 'z'; i++)
    {
        Scanner_char_table[i] = LETTER;
    }
    Scanner_char_table['.'] = DIGIT;
    Scanner_char_table['-'] = DIGIT;
    Scanner_char_table['+'] = DIGIT;
    Scanner_char_table['_'] = LETTER;
    Scanner_char_table['$'] = LETTER;
    Scanner_char_table['?'] = LETTER;
    Scanner_char_table['!'] = LETTER;
    Scanner_char_table['#'] = LETTER;
    Scanner_char_table['"'] = QUOTE;
    Scanner_char_table[0] = EOF_CODE;
    Scanner_char_table[':'] = LETTER;
    Scanner_char_table['*'] = LETTER;
    Scanner_char_table['\\'] = LETTER;
    Scanner_ch = '\0';
}

// 0x0058C790 (:495)
static void scanner_skip_blanks()
{
    while (Scanner_ch == ' ' || Scanner_ch == '\t')
    {
        scanner_get_char();
    }
}

// 0x0058C7D0 (:502)
// The token in lower case (A-Z only).
static void scanner_downshift_word()
{
    strcpy(Scanner_word_string, Scanner_token_string);
    for (char* p = Scanner_word_string; *p != '\0'; p++)
    {
        if (*p > '@' && *p < '[')
        {
            *p += ' ';
        }
    }
}

// 0x0058C810 (:516)
static void scanner_get_word()
{
    while (Scanner_char_table[(unsigned char)Scanner_ch] == LETTER || Scanner_char_table[(unsigned char)Scanner_ch] == DIGIT)
    {
        *Scanner_tokenp++ = Scanner_ch;
        scanner_get_char();
    }
    *Scanner_tokenp = '\0';
    Scanner_token = IDENTIFIER;
}

// 0x0058C870 (:527)
// A quoted string, quotes kept. In the chat modes it runs to the end of the line (a chat message may contain quotes).
static void scanner_get_string()
{
    *Scanner_tokenp = '"';
    char* p = Scanner_tokenp + 1;
    scanner_get_char();
    char c = Scanner_ch;
    const bool to_end = Console_mode != VCM_NORMAL;
    while ((to_end || c != '"') && c != '\0')
    {
        *p++ = c;
        scanner_get_char();
        c = Scanner_ch;
    }
    scanner_get_char();
    *p = '"';
    Scanner_tokenp = p + 1;
    *Scanner_tokenp = '\0';
    Scanner_token = STRING;
}

// 0x0058C900 (:543)
// The next token: a word (lower-cased into Scanner_word_string), a quoted string (as is) or one special character.
static void scanner_get_token()
{
    scanner_skip_blanks();
    const char_code code = Scanner_char_table[(unsigned char)Scanner_ch];
    Scanner_tokenp = Scanner_token_string;
    Scanner_token_string[0] = '\0';
    switch (code)
    {
    case LETTER:
    case DIGIT:
        scanner_get_word();
        break;
    case QUOTE:
        scanner_get_string();
        strcpy(Scanner_word_string, Scanner_token_string);
        return;
    case EOF_CODE:
        Scanner_token = NO_TOKEN;
        break;
    default:
        Scanner_token_string[0] = Scanner_ch;
        Scanner_token_string[1] = '\0';
        Scanner_tokenp = Scanner_token_string + 1;
        scanner_get_char();
        Scanner_token = IDENTIFIER;
        break;
    }
    scanner_downshift_word();
}

// 0x0058C9D0 (:572)
static void scanner_start_command(char* s)
{
    Scanner_bufferp = s;
    scanner_get_char();
}

// 0x005AE9E0 (:589)
// The console slides down from the top (instantly: 1,000,000 pixels a second) over the scrollback, the input line
// at its bottom; the cursor blinks after the cursor position.
void console_render_gr()
{
    if (Config.draw_str == nullptr || Config.draw_rect == nullptr)
    {
        return;
    }
    color black(0, 0, 0, 0);
    int unused_w = 0;
    int line_h = 0;
    Config.draw_str("", 0, 0, -1, unused_w, line_h, black);
    const int x = Console_offset_x + Config.safe_area_left;
    const float screen_h = (float)Config.screen_height;
    const int shown_y = (int)(screen_h * console_height_pct()) - line_h;
    int y = 0;
    if (Console_window_moving)
    {
        // Magic number: the window moves 1,000,000 pixels per second.
        if (Console_active)
        {
            y = Console_y + (int)(Frametime_real * 1e6);
            if (shown_y <= y)
            {
                Console_window_moving = false;
                y = shown_y;
            }
        }
        else
        {
            y = Console_y - (int)(Frametime_real * 1e6);
            const int hidden_y = shown_y - (int)(screen_h * console_height_pct());
            if (y <= hidden_y)
            {
                Console_window_moving = false;
                y = hidden_y;
            }
        }
    }
    else
    {
        y = shown_y;
        if (!Console_active)
        {
            return;
        }
    }
    Console_y = y;
    const double height = Console_mode == VCM_NORMAL ? (double)Config.screen_height * (double)Console_height_pct : (double)Config.screen_height * (double)Console_chat_height_pct;
    Config.draw_rect(0, 0, Config.screen_width, ((int)height - shown_y) + Console_y, Console_background_color);
    int cursor_x = 0;
    int unused_h = 0;
    Config.draw_str(Console_line_current.text, x, Console_y, (int)Command_line_xoffset + 2, cursor_x, unused_h, Console_default_text_color);
    int line = Console_lines_scroll_index;
    int line_y = Console_y - line_h;
    int w = 0;
    int h = 0;
    if (line < 0)
    {
        line = Console_lines_next_index - 1;
        if (line < 0)
        {
            line = Console_num_lines - 1;
        }
    }
    else
    {
        Config.draw_str("[in scroll mode - press END to leave scroll mode]", x, line_y, -1, w, h, Console_default_text_color);
        line_y -= line_h;
    }
    for (int i = 0; i < Console_num_lines; i++)
    {
        if (Console_lines[line].text[0] != '\0')
        {
            Config.draw_str(Console_lines[line].text, x, line_y, -1, w, h, Console_default_text_color);
        }
        line_y -= line_h;
        if (line_y < 0)
        {
            break;
        }
        line--;
        if (line < 0)
        {
            line = Console_num_lines - 1;
        }
    }
    if (Console_cursor_timestamp.valid() && Console_cursor_timestamp.elapsed())
    {
        Console_cursor_timestamp.set(Console_cursor_blink_ms);
        if (Console_cursor_visible)
        {
            Console_cursor_visible = false;
            return;
        }
        Console_cursor_visible = true;
    }
    else if (!Console_cursor_visible)
    {
        return;
    }
    Config.draw_str("_", cursor_x + 1 + x, Console_y, -1, w, h, Console_default_text_color);
}

// 0x005AECB0 (:687)
// The chat window at the bottom left: a background, scroll buttons and a thumb on its left edge, and the last three
// lines ("(TEAM) " name, then the text) with a drop shadow. In game and at the end of a game it fades out over the
// last second of a line's 10 s; on the in-game scoreboard it stays.
void console_render_chat()
{
    if (!Chat_window_enabled || Chat_rendering_disabled || Config.draw_str == nullptr || Config.draw_rect == nullptr)
    {
        return;
    }
    if (Config.update_func != nullptr)
    {
        Config.update_func(Config);
    }
    color black(0, 0, 0, 0);
    static color Chat_name_text_color(0xFC, 0, 0, 0xFF); // 0x01D584C4
    static color Chat_default_text_color(0xFC, 0x96, 5, 0xFF); // 0x01D584C0
    static color Chat_old_text_color(0xFC, 0x96, 5, 0xB2); // 0x01D584BC
    int unused_w = 0;
    int line_h = 0;
    Config.draw_str("", 0, 0, -1, unused_w, line_h, black);
    const vchat_position position = Chat_position;
    const int window_h = chat_window_h_d();
    const int window_y = get_chat_window_y();
    const int first_line_y = window_y + window_h - 2;
    float fade = 1.0f;
    if (position == VCP_INGAME || position == VCP_ENDGAME)
    {
        if (Textchat_lines_next_index < 1 || Chat_timer.elapsed())
        {
            fade = 0.0f;
        }
        else
        {
            const int left = Chat_timer.time_until();
            // Magic number: the last 1000 ms fade.
            fade = left < 1000 ? (float)((double)(float)left / 1000.0) : 1.0f;
        }
    }
    if (position == VCP_INGAME_SCOREBOARD)
    {
        fade = 1.0f;
    }
    static color render_col_1(0xFC, 0x96, 5, 0); // 0x01D584B8
    static color render_col_2(0xFC, 0x96, 5, 0); // 0x01D584B4
    // Magic numbers: the background, highlighted and normal button alphas.
    Textchat_background_color.alpha = (unsigned char)(int)((double)fade * 80.0);
    render_col_1.alpha = (unsigned char)(int)((double)fade * 200.0);
    render_col_2.alpha = (unsigned char)(int)((double)fade * 95.0);
    Config.draw_rect(7, window_y + 2, 10, 10, Textchat_mouse_over_up ? render_col_1 : render_col_2);
    Config.draw_rect(7, get_chat_window_y() + chat_window_h_d() - 12, 10, 10, Textchat_mouse_over_down ? render_col_1 : render_col_2);
    const int track_h = chat_window_h_d() - 26;
    int thumb_bottom = get_chat_window_y() + chat_window_h_d() - 13;
    const int next = Textchat_lines_next_index;
    const bool full = Textchat_console_full;
    const int lines = full ? Console_num_lines : next;
    int thumb_h = (int)((3.0 / (double)(float)lines) * (double)(float)track_h);
    if (!full && next < Chat_num_visible_lines)
    {
        thumb_h = 0;
    }
    if (Textchat_lines_scroll_index != -1)
    {
        int offset = 0;
        if (!full)
        {
            offset = (int)((1.0 - (double)(float)Textchat_lines_scroll_index / (double)(float)(next - 1)) * (double)(float)(track_h - thumb_h));
        }
        else if (next - 1 < Textchat_lines_scroll_index)
        {
            // Magic number: 1/256 of the track per line.
            offset = (int)(((double)(float)(Console_num_lines - Textchat_lines_scroll_index) + (double)(float)(next - 1)) * 0.00390625 * (double)(float)track_h);
        }
        else
        {
            offset = (int)((double)(float)Textchat_lines_scroll_index * 0.00390625 * (double)(float)track_h);
        }
        thumb_bottom -= offset;
    }
    Config.draw_rect(7, thumb_bottom - thumb_h, 10, thumb_h, render_col_2);
    Config.draw_rect(5, get_chat_window_y(), 14, chat_window_h_d(), Textchat_background_color);
    int y = first_line_y - line_h;
    const unsigned char alpha = (unsigned char)(int)((double)fade * 255.0);
    Chat_default_text_color.alpha = alpha;
    // Magic number: older lines at 70% alpha.
    Chat_old_text_color.alpha = (unsigned char)(int)((double)alpha * 0.7);
    Chat_name_text_color.alpha = alpha;
    int line = Textchat_lines_scroll_index;
    if (line < 0)
    {
        line = Textchat_lines_next_index - 1;
        if (line < 0)
        {
            line = Console_num_lines - 1;
        }
    }
    // Magic number: the shadow's alpha.
    color shadow(0, 0, 0, (unsigned char)(int)((double)fade * 250.0));
    for (int i = 0; i < Chat_num_visible_lines; i++)
    {
        const chat_console_line& chat = Textchat_lines[line];
        if (chat.text[0] != '\0')
        {
            color clr = chat.clr;
            if (clr == color::WHITE)
            {
                // Magic number: white lines are drawn red.
                clr = color(0xC9, 0x2D, 0x2D, 0xFF);
            }
            clr.alpha = alpha;
            int x = 0;
            int w = 0;
            int h = 0;
            if (!chat.wrapped_line)
            {
                if (chat.team_message)
                {
                    Config.draw_str("(TEAM) ", 25, y + 2, -1, w, h, shadow);
                    Config.draw_str("(TEAM) ", 23, y, -1, w, h, clr);
                    x = w;
                }
                Config.draw_str(chat.player_name, x + 25, y + 2, -1, w, h, shadow);
                Config.draw_str(chat.player_name, x + 23, y, -1, w, h, clr);
                x += w;
            }
            Config.draw_str(" ", x + 23, y, -1, w, h, Chat_default_text_color);
            x += w;
            if (Chat_window_enabled_ingame && i > 0)
            {
                Chat_default_text_color = Chat_old_text_color;
            }
            Config.draw_str(chat.text, x + 25, y + 2, -1, w, h, shadow);
            Config.draw_str(chat.text, x + 23, y, -1, w, h, Chat_default_text_color);
            y -= line_h;
            if (y < 0)
            {
                return;
            }
        }
        line--;
        if (line < 0)
        {
            line = Console_num_lines - 1;
        }
    }
}

// 0x0058C9F0 (:862)
// A newline moves the input line into the scrollback; other characters are appended (the last one replaced when the
// line is full). Unreferenced.
[[maybe_unused]] static void console_append_char_to_current_line(char c)
{
    if (c == '\n')
    {
        strcpy(Console_lines[Console_lines_next_index].text, Console_line_current.text);
        Console_lines[Console_lines_next_index].clr = Console_default_text_color;
        Console_lines_next_index++;
        if (Console_lines_next_index > Console_num_lines - 1)
        {
            Console_lines_next_index = 0;
        }
        Console_line_current.text[0] = '\0';
        return;
    }
    const size_t len = strlen(Console_line_current.text);
    if (len + 1 < 0x5B)
    {
        Console_line_current.text[len] = c;
        Console_line_current.text[len + 1] = '\0';
        return;
    }
    Console_line_current.text[len - 1] = c;
}

// The input line goes into the scrollback (console_append_char_to_current_line('\n') inlined).
static void console_commit_current_line()
{
    strcpy(Console_lines[Console_lines_next_index].text, Console_line_current.text);
    Console_lines[Console_lines_next_index].clr = Console_default_text_color;
    Console_lines_next_index++;
    if (Console_lines_next_index > Console_num_lines - 1)
    {
        Console_lines_next_index = 0;
    }
    Console_line_current.text[0] = '\0';
}

// 0x005C4B50 (:888)
static void console_display_help()
{
    console_output("Available functions:\n", nullptr, false);
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        if (e->def->help == nullptr)
        {
            console_printf(" %s\n", e->def->name);
        }
        else
        {
            console_printf(" %s - %s\n", e->def->name, e->def->help);
        }
    }
}

// 0x005CC1B0 (:918)
// The history file: the last index (0-15), then that many + 1 null-terminated commands.
static void load_console_history()
{
    cfile* p_cfile = cf_open(Console_file, "rb", PLATFORM_PC, false);
    if (p_cfile == nullptr)
    {
        return;
    }
    if (cf_tell(p_cfile) < cf_size(p_cfile))
    {
        Command_history_last = cf_read_int32(p_cfile);
        if ((unsigned int)(Command_history_last - 1) < (unsigned int)(Console_history_size - 1))
        {
            if (Command_history_last > -1)
            {
                for (int i = 0; i <= Command_history_last; i++)
                {
                    // Magic number: 0x5A, the line length.
                    cf_read_string_null_term(Command_history[i], Console_max_line_length, p_cfile);
                }
                cf_close(p_cfile);
                return;
            }
        }
        else
        {
            Command_history_last = 0;
        }
    }
    cf_close(p_cfile);
}

// 0x005CC240 (:948)
// Only with vconsole_config::save_history; the file is opened even while cf_open is locked.
void save_console_history()
{
    if (!Config.save_history)
    {
        return;
    }
    const bool was_locked = Cf_open_locked;
    if (Cf_open_locked)
    {
        Cf_open_locked = false;
    }
    cfile* p_cfile = cf_open(Console_file, "wb", PLATFORM_PC, false);
    if (was_locked)
    {
        Cf_open_locked = true;
    }
    if (p_cfile == nullptr)
    {
        return;
    }
    cf_write_int32(Command_history_last, p_cfile);
    for (int i = 0; i <= Command_history_last; i++)
    {
        cf_write(Command_history[i], (unsigned int)strlen(Command_history[i]) + 1, p_cfile);
    }
    cf_close(p_cfile);
}

// 0x005CC2F0 (:989)
// Registered with atexit by console_init.
static void console_shutdown()
{
    save_console_history();
    for (int i = 0; i < Console_num_lines; i++)
    {
        Console_lines[i].text[0] = '\0';
    }
    Console_line_current.text[0] = '\0';
}

// 0x005C4BD0 (:1002)
static void console_show_common_prefix_commands()
{
    const unsigned int len = (unsigned int)strlen(Command_line_current);
    console_printf("Possible commands for prefix '%s'", Command_line_current);
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        const console_command* command = e->def;
        if (len <= strlen(command->name) && keen::compareStringNoCase(Command_line_current, command->name, len) == 0)
        {
            console_printf(command->name);
        }
    }
}

// 0x005C4C80 (:1026)
// Tab: one match completes the command (with a space); several complete their common prefix and are listed.
static void console_complete_command()
{
    const unsigned int len = (unsigned int)strlen(Command_line_current);
    int matches = 0;
    char common[128];
    const console_command* match = nullptr;
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        const console_command* command = e->def;
        if (len > strlen(command->name) || keen::compareStringNoCase(Command_line_current, command->name, len) != 0)
        {
            continue;
        }
        match = command;
        if (matches == 0)
        {
            strcpy(common, command->name);
        }
        else
        {
            for (unsigned int i = len; common[i] != '\0'; i++)
            {
                if (tolower(command->name[i]) != tolower(common[i]))
                {
                    common[i] = '\0';
                    break;
                }
            }
        }
        matches++;
    }
    if (matches == 0)
    {
        console_printf("No console command found to match '%s'", Command_line_current);
        return;
    }
    if (matches == 1)
    {
        sprintf(Command_line_current, "%s ", match->name);
    }
    else
    {
        console_show_common_prefix_commands();
        strcpy(Command_line_current, common);
    }
    Command_line_xoffset = (unsigned int)strlen(Command_line_current);
}

// 0x005C4E60 (:1094)
// Registers the command (it stays enabled); a second command of the same name is refused.
int console_command::setup_params(const char* _name, const char* _help, dcf_type _type)
{
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        if (keen::compareStringNoCase(e->def->name, _name) == 0)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return 0;
        }
    }
    name = _name;
    help = _help;
    type = _type;
    enabled = true;
    Num_console_commands++;
    return 1;
}

// 0x005C4F00 (:1120)
console_command::console_command(const char* _name, const char* _help, dcf_type _type, void (*_func)())
{
    if (setup_params(_name, _help, _type) != 0)
    {
        func.vfunc = _func;
    }
}

// 0x005C4F30 (:1127)
console_command::console_command(const char* _name, const char* _help, dcf_type _type, bool (*_func)())
{
    if (setup_params(_name, _help, _type) != 0)
    {
        func.bfunc = _func;
    }
}

// 0x005C4F60 (:1134)
console_command::console_command(const char* _name, const char* _help, dcf_type _type, int (*_func)())
{
    if (setup_params(_name, _help, _type) != 0)
    {
        func.ifunc = _func;
    }
}

// 0x005C4F90 (:1141)
console_command::console_command(const char* _name, const char* _help, dcf_type _type, float (*_func)())
{
    if (setup_params(_name, _help, _type) != 0)
    {
        func.ffunc = _func;
    }
}

// 0x005C4FC0 (:1148)
console_command::console_command(const char* _name, const char* _help, dcf_type _type, vector (*_func)())
{
    if (setup_params(_name, _help, _type) != 0)
    {
        func.v3func = _func;
    }
}

static bool console_in_chat_mode()
{
    return Console_mode == VCM_CHAT || Console_mode == VCM_TEAMCHAT;
}

// 0x005C5000 (:1174)
// Runs one command line (printf formatted). "debug <command>" traces the parsing; "help"/"man" alone lists the
// commands, "help <command>" (or a '?' anywhere, outside the chat modes) runs the command in help mode. A bad argument
// (console_get_arg's longjmp) ends the command with -1. Returns 0 when a command ran (or was disabled).
int console_do_command(const char* text, ...)
{
    char command_line[256];
    va_list args;
    va_start(args, text);
    vsprintf(command_line, text, args);
    va_end(args);
    Console_command_line = command_line;
    Console_debug_on = 0;
    scanner_start_command(command_line);
#pragma warning(suppress : 4611) // console_get_arg reports a bad argument with longjmp
    if (setjmp(Console_bad_arg) != 0)
    {
        return -1;
    }
    int mode = 0;
    console_get_arg(0xFFFFFFFF, 0);
    if (keen::compareStringNoCase(Console_arg, "debug") == 0)
    {
        Console_debug_on = 1;
        console_output("Command line:", nullptr, false);
        console_output(Console_command_line, nullptr, false);
        console_get_arg(0xFFFFFFFF, 0);
    }
    if (!console_in_chat_mode() && (keen::compareStringNoCase(Console_arg, "help") == 0 || keen::compareStringNoCase(Console_arg, "man") == 0))
    {
        mode = 2;
        console_get_arg(0xFFFFFFFF, 0);
        if ((Console_arg_type & 1) != 0)
        {
            console_display_help();
            return 0;
        }
    }
    if (!console_in_chat_mode() && strstr(Console_command_line, "?") != nullptr)
    {
        mode = 2;
    }
    if ((Console_arg_type & 2) == 0)
    {
        console_printf("Invalid keyword '%s'\n", Console_arg);
        return -1;
    }
    if (Console_debug_on != 0)
    {
        console_printf("Searching for command '%s'\n", Console_arg);
    }
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        console_command* command = e->def;
        if (keen::compareStringNoCase(Console_arg, command->name) != 0)
        {
            continue;
        }
        if (!command->enabled)
        {
            console_printf("Skipping disabled command '%s'\n", Console_arg);
            return 0;
        }
        if (mode == 0)
        {
            if (Console_debug_on != 0)
            {
                console_printf("Calling function '%s'\n", Console_arg);
            }
            Console_command = 1;
            Console_help = 0;
            Console_status = 0;
        }
        else if (mode == 1)
        {
            if (Console_debug_on != 0)
            {
                console_printf("Checking status for '%s'\n", Console_arg);
            }
            Console_command = 0;
            Console_help = 0;
            Console_status = 1;
        }
        else
        {
            if (Console_debug_on != 0)
            {
                console_printf("Doing help for '%s'\n", Console_arg);
            }
            Console_command = 0;
            Console_help = 1;
            Console_status = 0;
        }
        const bool was_locked = Cf_open_locked;
        switch (command->type)
        {
        case DCF_TYPE_BOOL:
            command->func.bfunc();
            break;
        case DCF_TYPE_INT:
            command->func.ifunc();
            break;
        case DCF_TYPE_FLOAT:
            command->func.ffunc();
            break;
        case DCF_TYPE_FUNC:
            // A command may open files even while cf_open is locked.
            if (Cf_open_locked)
            {
                Cf_open_locked = false;
            }
            command->func.vfunc();
            if (was_locked)
            {
                Cf_open_locked = true;
            }
            break;
        case DCF_TYPE_VECTOR3:
            command->func.v3func();
            break;
        default:
            break;
        }
        if (mode == 0)
        {
            console_get_arg(0xFFFFFFFF, 0);
            if ((Console_arg_type & 1) == 0)
            {
                console_printf("Ignoring the unused command line tail '%s %s'\n", Console_arg_org, Console_command_line);
            }
        }
        return 0;
    }
    console_printf("Unknown command '%s'\n", Console_arg);
    return -1;
}

// 0x005AF380 (:1334)
// Before the first frame (dosbox) the text goes to the process console; then into the scrollback, one line per
// newline (at most 89 characters each), only while the console is open or for chat text.
void console_output(const char* text, color*, bool chat_text)
{
    if (Console_dosbox)
    {
        for (; *text != '\0'; text++)
        {
            _putch(*text);
        }
        return;
    }
    if (!Console_active && !chat_text)
    {
        return;
    }
    const char* end = text + strlen(text);
    do
    {
        const char* newline = strchr(text, '\n');
        if (newline == nullptr)
        {
            newline = end;
        }
        size_t count = (size_t)(newline - text);
        // Magic number: 89 characters per line.
        if (count > 0x58)
        {
            count = 0x59;
        }
        strncpy(Console_lines[Console_lines_next_index].text, text, count);
        Console_lines[Console_lines_next_index].text[count] = '\0';
        Console_lines_next_index++;
        if (Console_lines_next_index > Console_num_lines - 1)
        {
            Console_lines_next_index = 0;
        }
        text = newline + 1;
    } while (text < end);
}

// 0x005AF470 (:1390)
void console_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(Console_print_buffer, fmt, args);
    va_end(args);
    console_output(Console_print_buffer, nullptr, false);
}

// 0x005AF4B0 (:1403)
void console_chat_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(Console_print_buffer, fmt, args);
    va_end(args);
    console_output(Console_print_buffer, nullptr, true);
}

// 0x005C5380 (:1417)
// The process console's input (before the first frame): typed characters echo, backspace erases, enter runs.
void console_process_dos()
{
    if (_kbhit() == 0)
    {
        return;
    }
    const unsigned int c = (unsigned int)_getch();
    if (c == 8)
    {
        if (Command_line_xoffset != 0)
        {
            Command_line_xoffset--;
            Command_line_current[Command_line_xoffset] = '\0';
            const char erase[4] = {8, ' ', 8, 0};
            console_output(erase, nullptr, false);
        }
        return;
    }
    if (c == '\r')
    {
        console_output("\n", nullptr, false);
        console_do_command(Command_line_current);
        Command_line_xoffset = 0;
        Command_line_current[0] = '\0';
        return;
    }
    if (c < 0x80)
    {
        Command_line_current[Command_line_xoffset] = (char)c;
        Command_line_xoffset++;
        Command_line_current[Command_line_xoffset] = '\0';
        const char echo[2] = {(char)c, 0};
        console_output(echo, nullptr, false);
    }
}

// 0x0058CA80 (:1459)
// Removes a history entry, moving the later ones down.
static void remove_console_command(int command_idx)
{
    if (command_idx < 0 || command_idx >= Command_history_last)
    {
        return;
    }
    for (int i = command_idx; i < Command_history_last; i++)
    {
        strcpy(Command_history[i], Command_history[i + 1]);
    }
}

// Copies a history entry into the input line, the cursor at its end.
static void console_recall_history(int index)
{
    strcpy(Command_line_current, Command_history[index]);
    Command_line_xoffset = (unsigned int)strlen(Command_line_current);
}

// Whether byte is a UTF-8 continuation byte (the cursor skips them).
static bool console_utf8_continuation(char byte)
{
    return ((unsigned char)byte & 0xC0) == 0x80;
}

// Removes the character at the cursor (with its UTF-8 continuation bytes).
static void console_delete_at_cursor()
{
    unsigned int len = (unsigned int)strlen(Command_line_current);
    if (Command_line_xoffset >= len)
    {
        return;
    }
    char* p = Command_line_current + Command_line_xoffset;
    memmove(p, p + 1, len - Command_line_xoffset);
    while (Command_line_xoffset < (unsigned int)strlen(Command_line_current) && console_utf8_continuation(*p))
    {
        len = (unsigned int)strlen(Command_line_current);
        memmove(p, p + 1, len - Command_line_xoffset);
    }
}

// Enter: "!prefix" recalls the last history entry starting with prefix; in the chat modes the "(say) " or
// "(team say) " prompt becomes the say or team_say command; the line goes into the scrollback and runs. A command
// that ran goes to the end of the history (once) and the history is saved; a chat line closes the console.
static void console_enter_command()
{
    if (Command_line_current[0] == '\0')
    {
        return;
    }
    if (Command_line_current[0] == '!' && Command_history_last > -1)
    {
        for (int i = Command_history_last; i > -1; i--)
        {
            unsigned int n = (unsigned int)strlen(Command_line_current) - 1;
            const unsigned int history_len = (unsigned int)strlen(Command_history[i]);
            if (history_len <= n)
            {
                n = history_len;
            }
            if (keen::compareStringNoCase(Command_line_current + 1, Command_history[i], n) == 0)
            {
                console_recall_history(i);
                break;
            }
        }
    }
    char message[92];
    if (Console_mode == VCM_CHAT)
    {
        // Magic number: 6 characters of "(say) ".
        strcpy(message, Command_line_current + 6);
        strcpy(Command_line_current, "say \"");
        strcat(Command_line_current, message);
    }
    else if (Console_mode == VCM_TEAMCHAT)
    {
        // Magic number: 11 characters of "(team say) ".
        strcpy(message, Command_line_current + 11);
        strcpy(Command_line_current, "team_say \"");
        strcat(Command_line_current, message);
    }
    console_commit_current_line();
    if (console_do_command(Command_line_current) == 0)
    {
        const bool same_as_last = Command_history_last > -1 && keen::compareStringNoCase(Command_line_current, Command_history[Command_history_last]) == 0;
        if (!same_as_last)
        {
            for (int i = 0; i < Command_history_last; i++)
            {
                if (keen::compareStringNoCase(Command_line_current, Command_history[i]) == 0)
                {
                    remove_console_command(i);
                    Command_history_last--;
                    break;
                }
            }
            if (Command_history_last < Console_history_size - 1)
            {
                Command_history_last++;
            }
            else
            {
                remove_console_command(0);
            }
            strcpy(Command_history[Command_history_last], Command_line_current);
        }
        Command_line_xoffset = 0;
        Command_line_current[0] = '\0';
        Command_line_entered[0] = '\0';
        Command_history_scroll = 0;
        in_find_mode = false;
        save_console_history();
        console_reset_cursor_blink();
        if (console_is_in_chat_mode())
        {
            console_deactivate();
        }
        return;
    }
    Command_line_xoffset = 0;
    Command_line_current[0] = '\0';
    Command_line_entered[0] = '\0';
    Command_history_scroll = 0;
    in_find_mode = false;
    save_console_history();
    console_reset_cursor_blink();
}

// The history entry of a search step matches what was typed (case-insensitive, up to the shorter length).
static bool console_history_matches(int index, bool empty_is_mismatch)
{
    unsigned int n = (unsigned int)strlen(Command_line_entered);
    const unsigned int history_len = (unsigned int)strlen(Command_history[index]);
    if (history_len <= n)
    {
        n = history_len;
    }
    const int compare = keen::compareStringNoCase(Command_line_entered, Command_history[index], n);
    return !(empty_is_mismatch && n == 0) && compare == 0;
}

// Up: with typed text, the previous history entry starting with it (searching backwards, wrapping); without, or
// after a failed search, simply the previous entry.
static void console_history_up(int history_last)
{
    if (Command_line_current[0] == '\0')
    {
        in_find_mode = true;
        find_failed = true;
    }
    else if (!in_find_mode || !find_failed)
    {
        if (!in_find_mode)
        {
            strcpy(Command_line_entered, Command_line_current);
            in_find_mode = true;
            find_failed = false;
        }
        const int start = Command_history_scroll;
        for (;;)
        {
            Command_history_scroll--;
            if (Command_history_scroll < 0)
            {
                Command_history_scroll = Command_history_last;
            }
            if (Command_history_scroll > -1)
            {
                if (!console_history_matches(Command_history_scroll, true))
                {
                    if (Command_history_scroll == start)
                    {
                        break;
                    }
                    continue;
                }
                console_recall_history(Command_history_scroll);
            }
            if (Command_history_scroll != start)
            {
                if (Command_history_scroll > -1)
                {
                    console_recall_history(Command_history_scroll);
                }
                return;
            }
            break;
        }
        console_output("(No match found in history)", nullptr, false);
        find_failed = true;
        return;
    }
    Command_history_scroll--;
    if (Command_history_scroll < 0)
    {
        Command_history_scroll = history_last;
    }
    if (Command_history_scroll > -1)
    {
        console_recall_history(Command_history_scroll);
    }
}

// Down: the same forwards (wrapping to the first entry).
static void console_history_down(int history_last)
{
    if (Command_line_current[0] == '\0')
    {
        in_find_mode = true;
        find_failed = true;
    }
    else if (!in_find_mode || !find_failed)
    {
        if (!in_find_mode)
        {
            strcpy(Command_line_entered, Command_line_current);
            in_find_mode = true;
            find_failed = false;
        }
        const int start = Command_history_scroll;
        for (;;)
        {
            Command_history_scroll++;
            if (Command_history_last < Command_history_scroll)
            {
                Command_history_scroll = 0;
            }
            if (!console_history_matches(Command_history_scroll, false))
            {
                if (Command_history_scroll == start)
                {
                    break;
                }
                continue;
            }
            console_recall_history(Command_history_scroll);
            if (Command_history_scroll != start)
            {
                if (Command_history_scroll > -1)
                {
                    console_recall_history(Command_history_scroll);
                }
                return;
            }
            break;
        }
        console_output("(No match found in history)", nullptr, false);
        find_failed = true;
        return;
    }
    Command_history_scroll++;
    if (history_last < Command_history_scroll)
    {
        Command_history_scroll = 0;
    }
    else if (Command_history_scroll < 0)
    {
        return;
    }
    console_recall_history(Command_history_scroll);
}

// A typed key: its text (shift from the code, inverted by caps lock; AltGr, num lock) inserted at the cursor.
static void console_insert_text(unsigned int key)
{
    char text[64];
    text[0] = '\0';
    if (Key_inited)
    {
        // Magic numbers: key code bit 12 is shift; lock states 1 and 0 are caps lock and num lock.
        bool shift = ((key >> 12) & 1) != 0;
        if (Key_lock_states[1])
        {
            shift = !shift;
        }
        keen::input::convertKeyEventToText(text, sizeof(text), key & 0xFF, shift, Key_states[CONSOLE_KEY_ALTGR].down, Key_lock_states[0]);
    }
    const unsigned int line_len = (unsigned int)strlen(Command_line_current);
    unsigned int text_len = (unsigned int)strlen(text);
    if (Console_window_moving || line_len + text_len + 1 > 0x59)
    {
        return;
    }
    // Room for the text at the cursor.
    unsigned int end = line_len + 1;
    unsigned int at = Command_line_xoffset;
    for (; text_len != 0; text_len--)
    {
        for (unsigned int i = end; at < i; i--)
        {
            Command_line_current[i] = Command_line_current[i - 1];
        }
        end++;
        at++;
    }
    if (text[0] == '\0')
    {
        return;
    }
    for (unsigned int i = 0; i < sizeof(text) && text[i] != '\0'; i++)
    {
        // Magic number: 0x58, the longest input line.
        if (strlen(Command_line_current) < 0x58)
        {
            Command_line_current[Command_line_xoffset] = text[i];
            Command_line_xoffset++;
        }
    }
    console_reset_cursor_blink();
    in_find_mode = false;
}

// 0x005CC320 (:1479)
// The typed keys of this frame (a held key repeats after 500 ms, then a third faster each time down to 10 ms). The
// first frames after opening the chat swallow their keys.
void console_process_gr()
{
    unsigned int key = key_buf_pop();
    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    if (last_key != 0)
    {
        const bool down = last_key < 256 && Key_states[last_key].down && !Key_states[last_key].masked;
        if (!down || All_keys_masked)
        {
            repeate_key_interval = 500;
        }
        else if (last_key_last_repeat + (unsigned int)repeate_key_interval < now)
        {
            repeate_key_interval /= 3;
            key = last_key;
            if (repeate_key_interval < 10)
            {
                repeate_key_interval = 10;
            }
        }
    }
    int history_last = Command_history_last;
    unsigned int previous_key = last_key;
    if (Swallow_input_num_frames > 0)
    {
        Swallow_input_num_frames--;
        return;
    }
    for (;;)
    {
        last_key = key;
        if (last_key == 0 || !Console_active)
        {
            last_key = previous_key;
            return;
        }
        last_key_last_repeat = now;
        switch (last_key)
        {
        case CONSOLE_KEY_SHIFT_ENTER:
            console_deactivate();
            break;
        case CONSOLE_KEY_ALT_BACKSPACE:
        {
            // Back to the previous space, cut there.
            int at = (int)Command_line_xoffset;
            while (at > -1 && Command_line_current[at] != ' ')
            {
                at--;
            }
            if (at < 0)
            {
                at = 0;
            }
            Command_line_xoffset = (unsigned int)at;
            Command_line_current[at] = '\0';
            in_find_mode = false;
            break;
        }
        case CONSOLE_KEY_SHIFT_BACKSPACE:
            Command_line_xoffset = 0;
            Command_line_current[0] = '\0';
            in_find_mode = false;
            break;
        case CONSOLE_KEY_BACKSPACE:
            // Left one character (in the chat modes not into the "(say) "/"(team say) " prompt), then delete.
            if (Command_line_xoffset != 0)
            {
                const unsigned int len = (unsigned int)strlen(Command_line_current);
                if ((Console_mode == VCM_CHAT && len > 6) || (Console_mode != VCM_CHAT && (Console_mode != VCM_TEAMCHAT || len > 11)))
                {
                    do
                    {
                        Command_line_xoffset--;
                    } while (Command_line_xoffset != 0 && console_utf8_continuation(Command_line_current[Command_line_xoffset]));
                }
            }
            console_delete_at_cursor();
            console_reset_cursor_blink();
            in_find_mode = false;
            break;
        case CONSOLE_KEY_DELETE:
            console_delete_at_cursor();
            console_reset_cursor_blink();
            in_find_mode = false;
            break;
        case CONSOLE_KEY_TAB:
            console_complete_command();
            console_reset_cursor_blink();
            in_find_mode = false;
            break;
        case CONSOLE_KEY_ENTER:
        case CONSOLE_KEY_ENTER_2:
            console_enter_command();
            break;
        case CONSOLE_KEY_ESCAPE:
            if (console_is_in_chat_mode())
            {
                console_deactivate();
            }
            break;
        case CONSOLE_KEY_PAGE_UP:
            if (Console_lines_scroll_index == -1)
            {
                Console_lines_scroll_index = Console_lines_next_index - 3;
                if (Console_lines_scroll_index < 0)
                {
                    Console_lines_scroll_index = Console_lines_next_index + 0xFD;
                }
            }
            else
            {
                Console_lines_scroll_index--;
                if (Console_lines_scroll_index < 0)
                {
                    Console_lines_scroll_index = Console_num_lines - 1;
                }
            }
            break;
        case CONSOLE_KEY_PAGE_DOWN:
            if (Console_lines_scroll_index > -1)
            {
                Console_lines_scroll_index++;
                if (Console_lines_scroll_index > Console_num_lines - 1)
                {
                    Console_lines_scroll_index = 0;
                }
                if (Console_lines_scroll_index == Console_lines_next_index)
                {
                    Console_lines_scroll_index = -1;
                }
            }
            break;
        case CONSOLE_KEY_END:
            if (Console_lines_scroll_index == -1)
            {
                Command_line_xoffset = (unsigned int)strlen(Command_line_current);
            }
            else
            {
                Console_lines_scroll_index = -1;
            }
            break;
        case CONSOLE_KEY_HOME:
            Command_line_xoffset = 0;
            break;
        case CONSOLE_KEY_LEFT:
            if (Command_line_xoffset != 0)
            {
                do
                {
                    Command_line_xoffset--;
                } while (Command_line_xoffset != 0 && console_utf8_continuation(Command_line_current[Command_line_xoffset]));
            }
            console_reset_cursor_blink();
            break;
        case CONSOLE_KEY_RIGHT:
        {
            const unsigned int len = (unsigned int)strlen(Command_line_current);
            if (Command_line_xoffset < len)
            {
                do
                {
                    Command_line_xoffset++;
                } while (Command_line_xoffset < len && console_utf8_continuation(Command_line_current[Command_line_xoffset]));
            }
            console_reset_cursor_blink();
            break;
        }
        case CONSOLE_KEY_UP:
            console_history_up(history_last);
            break;
        case CONSOLE_KEY_DOWN:
            console_history_down(history_last);
            break;
        case CONSOLE_KEY_F3:
            // The last command again.
            if (history_last > -1)
            {
                console_recall_history(history_last);
                in_find_mode = false;
            }
            break;
        default:
            console_insert_text(last_key);
            break;
        }
        if (Console_dosbox)
        {
            strcpy(Console_line_current.text, Command_line_current);
        }
        else
        {
            sprintf(Console_line_current.text, "] %s", Command_line_current);
        }
        history_last = Command_history_last;
        previous_key = last_key;
        key = key_buf_pop();
    }
}

// 0x005CF7E0 (:1991)
void console_do_frame_process(bool dosbox)
{
    Console_dosbox = dosbox;
    if (!Console_inited)
    {
        console_init(nullptr);
    }
    if (Console_dosbox)
    {
        console_process_dos();
        return;
    }
    if (Console_active)
    {
        console_process_gr();
    }
}

// 0x005AF4F0 (:2016)
void console_do_frame_render()
{
    if (Console_inited && !Console_dosbox)
    {
        console_render_gr();
        console_render_chat();
    }
}

// 0x005CF820 (:2046)
void console_do_frame(bool dosbox)
{
    console_do_frame_process(dosbox);
    console_do_frame_render();
}

// 0x005AF510 (:2086)
// The next argument (Console_arg in lower case, Console_arg_org as typed) and its type bits: 1 none, 2 a word or
// string (quotes removed), 8 an integer, 0x10 a number, 0x20 hexadecimal (0x...), 0x40 true/on/non-zero, 0x80
// false/off/zero, 0x100 '+', 0x200 '-', 0x400 ','. Returns whether it has one of the type bits; a required argument
// that does not longjmps out of console_do_command.
bool console_get_arg(unsigned int type, int optional)
{
    scanner_get_token();
    Console_command_line = Scanner_bufferp;
    strcpy(Console_arg_org, Scanner_token_string);
    strcpy(Console_arg, Scanner_word_string);
    if (Console_debug_on != 0)
    {
        sprintf(Console_print_buffer, "next arg is '%s', was originally '%s'\n", Console_arg, Console_arg_org);
        console_output(Console_print_buffer, nullptr, false);
        console_output("Rest of the command line is ", nullptr, false);
        console_output(Console_command_line, nullptr, false);
        console_output("\n", nullptr, false);
    }
    if (Scanner_token == NO_TOKEN)
    {
        Console_arg_type = 1;
    }
    else if (Scanner_token == IDENTIFIER)
    {
        Console_arg_type = 2;
    }
    else
    {
        Console_arg_type = (Scanner_token == STRING) ? 4 : 2;
    }
    if ((Console_arg_type & 4) != 0)
    {
        const size_t len = strlen(Console_arg);
        memmove(Console_arg, Console_arg + 1, len);
        Console_arg[len - 2] = '\0';
        Console_arg_type = 2;
    }
    if ((Console_arg_type & 2) != 0)
    {
        const size_t len = strlen(Console_arg);
        size_t digits = 0;
        for (size_t i = 0; i < len; i++)
        {
            if (Scanner_char_table[(unsigned char)Console_arg[i]] == DIGIT)
            {
                digits++;
            }
        }
        if (digits == len)
        {
            Console_arg_type |= 0x10;
            Console_arg_float = (float)atof(Console_arg);
            if (strchr(Console_arg, '.') == nullptr)
            {
                Console_arg_type |= 8;
                Console_arg_int = atoi(Console_arg);
            }
        }
        else if (Console_arg[0] == '0' && Console_arg[1] == 'x')
        {
            char* end = nullptr;
            const unsigned long value = strtoul(Console_arg, &end, 0);
            if (*end == '\0')
            {
                Console_arg_type |= 0x28;
                Console_arg_int = (int)value;
            }
        }
        if (Console_debug_on != 0)
        {
            if ((Console_arg_type & 0x10) != 0)
            {
                sprintf(Console_print_buffer, "Found float number! %f\n", (double)Console_arg_float);
                console_output(Console_print_buffer, nullptr, false);
            }
            if ((Console_arg_type & 8) != 0)
            {
                sprintf(Console_print_buffer, "Found int32 number! %d\n", Console_arg_int);
                console_output(Console_print_buffer, nullptr, false);
            }
            if ((Console_arg_type & 0x20) != 0)
            {
                sprintf(Console_print_buffer, "Found hex number! %x\n", Console_arg_int);
                console_output(Console_print_buffer, nullptr, false);
            }
        }
        if (keen::compareStringNoCase(Console_arg, "on") == 0)
        {
            Console_arg_type |= 0x40;
        }
        if (keen::compareStringNoCase(Console_arg, "true") == 0)
        {
            Console_arg_type |= 0x40;
        }
        if (keen::compareStringNoCase(Console_arg, "off") == 0)
        {
            Console_arg_type |= 0x80;
        }
        if (keen::compareStringNoCase(Console_arg, "false") == 0)
        {
            Console_arg_type |= 0x80;
        }
        if (keen::compareStringNoCase(Console_arg, "+") == 0)
        {
            Console_arg_type |= 0x100;
        }
        if (keen::compareStringNoCase(Console_arg, "-") == 0)
        {
            Console_arg_type |= 0x200;
        }
        if (keen::compareStringNoCase(Console_arg, ",") == 0)
        {
            Console_arg_type |= 0x400;
        }
    }
    if ((Console_arg_type & 8) != 0)
    {
        Console_arg_type |= Console_arg_int == 0 ? 0x80 : 0x40;
    }
    if ((type & Console_arg_type) != 0)
    {
        return true;
    }
    if (optional != 0)
    {
        return false;
    }
    if ((Console_arg_type & 1) == 0 || (type & 1) != 0)
    {
        sprintf(Console_print_buffer, "Error: '%s' invalid type\n", Console_arg);
        console_output(Console_print_buffer, nullptr, false);
    }
    else
    {
        console_output("Error: Not enough parameters.\n", nullptr, false);
    }
    longjmp(Console_bad_arg, 1);
}

// 0x005AF950 (:2222)
// Closing returns to the chat mode and forgets the keys (they were typed into the console).
void console_deactivate()
{
    Console_window_moving = true;
    Console_mode = VCM_CHAT;
    Console_active = false;
    console_flush_keys();
}

// 0x0058CAC0 (:2233)
void console_activate()
{
    Console_active = true;
    Console_window_moving = true;
}

// 0x0058CAD0 (:2241)
bool console_is_active()
{
    return Console_active;
}

// 0x0058CAE0 (:2248)
bool console_is_moving()
{
    return Console_window_moving;
}

// 0x0058CAF0 (:2253)
int console_get_bottom_y()
{
    return (int)((float)Config.screen_height * console_height_pct()) + Console_y;
}

// 0x005CD150 (:2265)
// Once: the scanner, the callbacks, an empty scrollback, the "] " input line, the history file; chat mode.
void console_init(vconsole_config* init_config)
{
    if (Console_inited)
    {
        return;
    }
    scanner_init();
    if (init_config != nullptr)
    {
        Config = *init_config;
    }
    for (int i = 0; i < Console_num_lines; i++)
    {
        Console_lines[i].text[0] = '\0';
    }
    Console_cursor_timestamp.set(Console_cursor_blink_ms);
    strcpy(Console_line_current.text, "] ");
    Console_line_current.clr = Console_default_text_color;
    Console_lines_next_index = 0;
    Console_lines_scroll_index = -1;
    Console_window_moving = false;
    Console_cursor_visible = true;
    load_console_history();
    Console_inited = true;
    atexit(console_shutdown);
    Console_mode = VCM_CHAT;
}

// 0x005D02D0 (:2318)
// Unused (command object stripped).
void unused_dcf_script()
{
    if (Console_command != 0)
    {
        console_get_arg(2, 0);
        if (Console_arg[0] == '\0')
        {
            Console_help = 1;
            console_output("Usage: script <scriptfilename>\n", nullptr, false);
            return;
        }
        console_run_script(Console_arg);
    }
    if (Console_help != 0)
    {
        console_output("Usage: script <scriptfilename>\n", nullptr, false);
    }
}

// 0x005CF880 (:2339)
// "<name>.txt", else "<name>.script", one command per line; // lines and /* */ blocks are skipped. The file is opened
// even while cf_open is locked. A failing line is reported with its number (counting the lines that ran).
void console_run_script(const char* script_filename)
{
    bool in_comment = false;
    parse p;
    char filename[256];
    cf_add_ext(filename, sizeof(filename), script_filename, ".txt", false);
    if (Cfile_vdir_enabled)
    {
        vdir_refresh_file_size(filename);
    }
    const bool was_locked = Cf_open_locked;
    if (Cf_open_locked)
    {
        Cf_open_locked = false;
    }
    int result = p.open(filename);
    if (was_locked)
    {
        Cf_open_locked = true;
    }
    if (result < 0)
    {
        cf_add_ext(filename, sizeof(filename), script_filename, ".script", false);
        if (Cfile_vdir_enabled)
        {
            vdir_refresh_file_size(filename);
        }
        if (was_locked)
        {
            Cf_open_locked = false;
        }
        result = p.open(filename);
        if (was_locked)
        {
            Cf_open_locked = true;
        }
        if (result < 0)
        {
            sprintf(Console_print_buffer, "Couldn't find script file '%s'\n", filename);
            console_output(Console_print_buffer, nullptr, false);
            return;
        }
    }
    int line_num = 1;
    char line[256];
    while (p.parse_line(line, sizeof(line)))
    {
        str_trim(line, line, sizeof(line));
        if (line[0] == '/' && line[1] == '/')
        {
            continue;
        }
        if (strstr(line, "/*") != nullptr && !in_comment)
        {
            in_comment = true;
            continue;
        }
        if (strstr(line, "*/") != nullptr)
        {
            if (in_comment)
            {
                in_comment = false;
                continue;
            }
        }
        else if (in_comment)
        {
            continue;
        }
        if (line[0] != '\0' && console_do_command(line) != 0)
        {
            sprintf(Console_print_buffer, "Error processing script line %d: '%s'\n", line_num, line);
            console_output(Console_print_buffer, nullptr, false);
        }
        line_num++;
    }
}

// 0x005AF9A0 (:2416)
void dc_printf(const char* format, ...)
{
    char text[2048];
    memset(text, 0, sizeof(text));
    va_list args;
    va_start(args, format);
    vsprintf(text, format, args);
    va_end(args);
    console_output(text, nullptr, false);
}

// 0x005C5440 (:2437)
dcf_type console_get_type(const char* command)
{
    const console_command* c = console_command_get_by_name(command);
    return c != nullptr ? c->type : DCF_TYPE_NONE;
}

// 0x005C54B0 (:2458)
// Returns whether the command was enabled before.
bool console_command_enable(const char* command, bool enable)
{
    console_command* c = console_command_get_by_name(command);
    if (c == nullptr)
    {
        return false;
    }
    const bool was_enabled = c->enabled;
    c->enabled = enable;
    return was_enabled;
}

// 0x005C5520 (:2482)
console_command* console_command_get_by_name(const char* command)
{
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        if (keen::compareStringNoCase(command, e->def->name) == 0)
        {
            return e->def;
        }
    }
    return nullptr;
}

// 0x005C5590 (:2506)
// The value of a variable command (its function asked in status mode). A missing command reads as DCF_TYPE_FUNC.
// Notes (decompiled behaviour, kept as is): a vector's x and y are converted from integers.
dcf_type console_get_variable_value(const char* command, console_variable_value* value)
{
    console_command* c = console_command_get_by_name(command);
    if (c == nullptr)
    {
        return DCF_TYPE_FUNC;
    }
    Console_status = 1;
    Console_command = 0;
    switch (c->type)
    {
    case DCF_TYPE_BOOL:
        value->val.b = c->func.bfunc();
        break;
    case DCF_TYPE_INT:
        value->val.i = c->func.ifunc();
        break;
    case DCF_TYPE_FLOAT:
        value->val.f = c->func.ffunc();
        break;
    case DCF_TYPE_VECTOR3:
    {
        const vector v = c->func.v3func();
        const int* raw = (const int*)&v;
        value->v3val.x = (float)raw[0];
        value->v3val.y = (float)raw[1];
        value->v3val.z = v.z;
        break;
    }
    default:
        break;
    }
    return c->type;
}

// 0x0058CB30 (:2553)
// The last needle at or before end (not before begin), null when there is none.
char* str_back_find_char(char* begin, char* end, char needle)
{
    for (; begin < end; end--)
    {
        if (*end == needle)
        {
            return end;
        }
    }
    return *end == needle ? end : nullptr;
}

// 0x0058CB50 (:2578)
// Copies source into buffer; a longer source is cut at the last of the delimiters (tried in order) within the buffer,
// else at the buffer's end with a '-'. Returns where the rest starts.
char* strncpy_intelligent(char* buffer, int buffer_size, const char* delims, char* source)
{
    const unsigned int len = (unsigned int)strlen(source);
    if (len < (unsigned int)buffer_size)
    {
        strcpy(buffer, source);
        return source + len;
    }
    for (;;)
    {
        char* cut = str_back_find_char(source, source + buffer_size - 1, *delims);
        delims++;
        if (cut != nullptr)
        {
            if (*delims != '\0')
            {
                strncpy(buffer, source, cut - source);
                buffer[cut - source] = '\0';
                return cut;
            }
            break;
        }
        if (*delims == '\0')
        {
            break;
        }
    }
    strncpy(buffer, source, buffer_size - 2);
    buffer[buffer_size - 2] = '-';
    buffer[buffer_size - 1] = '\0';
    return source + buffer_size - 2;
}

// 0x0058CC10 (:2613)
// The case-insensitive position of needle in haystack (a rolling sum of lower-case characters picks candidates),
// -1 when it is not there.
int find_substring(const char* needle, const char* haystack)
{
    const unsigned int needle_len = (unsigned int)strlen(needle);
    const unsigned int haystack_len = (unsigned int)strlen(haystack);
    if (needle_len > haystack_len)
    {
        return -1;
    }
    int haystack_sum = 0;
    int needle_sum = 0;
    for (unsigned int i = 0; i < needle_len; i++)
    {
        haystack_sum += tolower(haystack[i]);
        needle_sum += tolower(needle[i]);
    }
    for (unsigned int pos = 0; pos <= haystack_len - needle_len; pos++)
    {
        if (haystack_sum == needle_sum)
        {
            unsigned int i = 0;
            while (i < needle_len && tolower(haystack[i + pos]) == tolower(needle[i]))
            {
                i++;
            }
            if (i >= needle_len)
            {
                return (int)pos;
            }
        }
        haystack_sum = haystack_sum - tolower(haystack[pos]) + tolower(haystack[pos + needle_len]);
    }
    return -1;
}

// 0x005C56A0 (:2662)
// Unused (command object stripped). Lists the commands whose name or help contains up to two search words, combined
// with "and", "or" or "not"; at most 50 results, long ones wrapped.
void unused_dcf_search()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_printf("Usage: search <one or more words>\nThis command will allow you to search through the list of available console commands,\noutputting all commands containing any of the search terms specified either in the\ncommand itself or in the command's help text.");
        }
        return;
    }
    console_get_arg(2, 0);
    int op = 0;
    int num_results = 0;
    char word1[128];
    char word2[128];
    word1[0] = '\0';
    word2[0] = '\0';
    int args = 0;
    do
    {
        args++;
        if (keen::compareStringNoCase(Console_arg, "and") == 0)
        {
            op = 0;
        }
        else if (keen::compareStringNoCase(Console_arg, "or") == 0)
        {
            op = 1;
        }
        else if (keen::compareStringNoCase(Console_arg, "not") == 0)
        {
            if (op == 0)
            {
                op = 2;
            }
            else if (op == 1)
            {
                op = 3;
            }
        }
        else
        {
            keen::formatString(word1[0] == '\0' ? word1 : word2, 0x80, Console_arg);
        }
    } while (console_get_arg(2, 1) && args < 4);
    // Magic number: 50 results.
    console_command* results[50];
    for (dictionary<console_command>::entry* e = dictionary<console_command>::get_first(); e != nullptr; e = dictionary<console_command>::get_next(e))
    {
        console_command* command = e->def;
        const bool first = find_substring(word1, command->name) != -1 || find_substring(word1, command->help) != -1;
        bool second = true;
        if (word2[0] != '\0' && find_substring(word2, command->name) == -1 && find_substring(word2, command->help) == -1)
        {
            second = false;
        }
        bool take = false;
        switch (op)
        {
        case 0:
            take = first && second;
            break;
        case 1:
            take = first || second;
            break;
        case 2:
            take = first && !second;
            break;
        case 3:
            take = first || !second;
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (take)
        {
            results[num_results++] = command;
            if (num_results > 49)
            {
                break;
            }
        }
    }
    int i = 0;
    for (; i < num_results && i < 50; i++)
    {
        char line[1024];
        sprintf(line, "%s: %s", results[i]->name, results[i]->help);
        // Magic number: 91 characters fit a console line.
        if (strlen(line) < 0x5B)
        {
            console_output(line, nullptr, false);
            continue;
        }
        // Continuation lines are indented by three spaces.
        const char indent[4] = "   ";
        const int indent_len = (int)strlen(indent);
        char out[92];
        char* source = line;
        char* dest = out;
        int size = 0x5A;
        do
        {
            source = strncpy_intelligent(dest, size, " _", source);
            console_output(out, nullptr, false);
            strcpy(out, indent);
            if (*source == ' ')
            {
                source++;
            }
            size = 0x5A - indent_len;
            dest = out + indent_len;
        } while (*source != '\0');
    }
    if (num_results != 0)
    {
        if (i == 50)
        {
            console_output("Max search results returned, narrow your search...", nullptr, false);
        }
        return;
    }
    console_output("No matches found, try again...", nullptr, false);
}

// 0x005AFA10 (:2812)
// A bool variable command: no argument toggles, true/on/non-zero or false/off/zero sets; prints the value when it
// toggled or in status mode, the usage in help mode or after a bad argument.
bool console_command_bool(const char* command_name, const char* bool_variable_name, bool bool_variable)
{
    bool print_value = false;
    bool print_usage = false;
    if (Console_command != 0)
    {
        if (console_get_arg(0xC1, 1))
        {
            if ((Console_arg_type & 0x40) != 0)
            {
                bool_variable = true;
            }
            else if ((Console_arg_type & 0x80) != 0)
            {
                bool_variable = false;
            }
            else if ((Console_arg_type & 1) != 0)
            {
                bool_variable = !bool_variable;
                print_value = true;
            }
        }
        else
        {
            print_usage = true;
        }
    }
    if (Console_help != 0 || print_usage)
    {
        sprintf(Console_print_buffer, "Usage: %s [bool]\nSets/toggles %s.\n", command_name, bool_variable_name);
        console_output(Console_print_buffer, nullptr, false);
    }
    if (Console_status != 0 || print_value)
    {
        sprintf(Console_print_buffer, "%s is %s\n", command_name, bool_variable ? "TRUE" : "FALSE");
        console_output(Console_print_buffer, nullptr, false);
    }
    return bool_variable;
}

// 0x005AFB10 (:2845)
// A float variable command: a number sets it; no argument prints it.
float console_command_float(const char* command_name, const char* float_variable_name, float& float_variable)
{
    bool print_value = false;
    bool print_usage = false;
    if (Console_command != 0)
    {
        if (console_get_arg(0x10, 1))
        {
            float_variable = Console_arg_float;
        }
        else if ((Console_arg_type & 1) == 0)
        {
            print_usage = true;
        }
        else
        {
            print_value = true;
        }
    }
    if (Console_help != 0 || print_usage)
    {
        sprintf(Console_print_buffer, "Usage: %s [float]\nSets %s.\n", command_name, float_variable_name);
        console_output(Console_print_buffer, nullptr, false);
    }
    if (Console_status == 0 && !print_value)
    {
        return float_variable;
    }
    sprintf(Console_print_buffer, "%s is %f\n", command_name, (double)float_variable);
    console_output(Console_print_buffer, nullptr, false);
    return float_variable;
}

// 0x005AFC10 (:2875)
// An int variable command: an integer sets it; no argument prints it.
int console_command_int(const char* command_name, const char* int_variable_name, int int_variable)
{
    bool print_value = false;
    bool print_usage = false;
    if (Console_command != 0)
    {
        if (console_get_arg(8, 1))
        {
            int_variable = Console_arg_int;
        }
        else if ((Console_arg_type & 1) == 0)
        {
            print_usage = true;
        }
        else
        {
            print_value = true;
        }
    }
    if (Console_help != 0 || print_usage)
    {
        sprintf(Console_print_buffer, "Usage: %s [int]\nSets %s.\n", command_name, int_variable_name);
        console_output(Console_print_buffer, nullptr, false);
    }
    if (Console_status != 0 || print_value)
    {
        sprintf(Console_print_buffer, "%s is %d\n", command_name, int_variable);
        console_output(Console_print_buffer, nullptr, false);
    }
    return int_variable;
}

// 0x0058CD30 (:2905)
// Only the chat modes can be set (anything else is the chat mode).
void console_set_console_mode(vconsole_mode new_mode)
{
    Console_mode = new_mode;
    if (new_mode != VCM_CHAT && new_mode != VCM_TEAMCHAT)
    {
        Console_mode = VCM_CHAT;
    }
}

// 0x0058CD50 (:2921)
vconsole_mode console_get_console_mode()
{
    return Console_mode;
}

// 0x0058CD60 (:2926)
bool console_is_in_chat_mode()
{
    return Console_mode == VCM_CHAT || Console_mode == VCM_TEAMCHAT;
}

// The chat input: the prompt in the input line, the keys forgotten and the next three frames' keys swallowed (the
// key that opened it), and the chat shown for 10 s.
static void console_activate_chat(const char* prompt, vconsole_mode mode)
{
    strcpy(Command_line_current, prompt);
    Command_line_xoffset = (unsigned int)strlen(Command_line_current);
    console_reset_cursor_blink();
    sprintf(Console_line_current.text, "] %s", Command_line_current);
    Console_mode = mode;
    console_flush_keys();
    Chat_timer.set(Chat_line_time_ms);
    // Magic number: three frames.
    Swallow_input_num_frames = 3;
}

// 0x005AFCF0 (:2935)
void console_activate_chat_say()
{
    console_activate_chat("(say) ", VCM_CHAT);
}

// 0x005AFDD0 (:2954)
void console_activate_chat_teamsay()
{
    console_activate_chat("(team say) ", VCM_TEAMCHAT);
}

// 0x005AFEB0 (:2974)
// The width of the name with its "(TEAM)  " or ":  " suffix.
int get_prefix_render_width(const char* name, bool team)
{
    color black(0, 0, 0, 0);
    int w = 0;
    int h = 0;
    Config.draw_str(name, 0, 0, -1, w, h, black);
    const int name_w = w;
    Config.draw_str(team ? "(TEAM)  " : ":  ", 0, 0, -1, w, h, black);
    return w + name_w;
}

// 0x005AFF50 (:2992)
// How many characters of text fit max_width (each measured alone; the whole prefix measured once more and scaled
// when it does not fit as a whole).
int get_chat_num_character_in_line(const char* text, int max_width)
{
    color black(0, 0, 0, 0);
    int fitting_width = 0;
    int sum = 0;
    unsigned int count = 0;
    char one[2] = {0, 0};
    int w = 0;
    int h = 0;
    const unsigned int len = (unsigned int)strlen(text);
    for (; count < len; count++)
    {
        one[0] = text[count];
        Config.draw_str(one, 0, 0, 1, w, h, black);
        sum += w;
        if (max_width < sum)
        {
            break;
        }
        fitting_width = sum;
    }
    int whole_w = 0;
    Config.draw_str(text, 0, 0, (int)count, whole_w, h, black);
    if (whole_w <= max_width && count == strlen(text))
    {
        return (int)count;
    }
    // Rounded to nearest (x87 FRNDINT).
    return (int)lrintf((float)count * ((float)fitting_width / (float)whole_w));
}

// 0x005B0060 (:3022)
// A chat line (also printed to the console as "[(TEAM) ]name: text" when asked); the chat shows for 10 s.
void console_add_textchat_line(const char* player_name, const char* text_line, bool team_message, bool wrapped, color team_color, bool print_to_console)
{
    const unsigned int len = (unsigned int)strlen(text_line);
    if (len == 0)
    {
        return;
    }
    if (print_to_console)
    {
        console_chat_printf(team_message ? "(TEAM) %s: %s" : "%s: %s", player_name, text_line);
    }
    chat_console_line& line = Textchat_lines[Textchat_lines_next_index];
    const unsigned int count = len & 0xFFFF;
    strncpy(line.text, text_line, count);
    line.text[count] = '\0';
    strcpy(line.player_name, player_name);
    line.team_message = team_message;
    line.wrapped_line = wrapped;
    line.clr = team_color;
    Textchat_lines_next_index++;
    if (Textchat_lines_next_index > Console_num_lines - 1)
    {
        Textchat_lines_next_index = 0;
        Textchat_console_full = true;
    }
    Chat_timer.set(Chat_line_time_ms);
}

// 0x0058CD80 (:3084)
bool console_is_chat_window_enabled()
{
    return Chat_window_enabled;
}

// 0x0058CD90 (:3093)
void console_disable_chat_window()
{
    Chat_window_enabled = false;
    Console_mode = VCM_CHAT;
}

// 0x0058CDB0 (:3101)
// An empty chat at the given position.
void console_enable_chat_window(vchat_position chat_position)
{
    Chat_position = chat_position;
    Chat_window_enabled = true;
    Textchat_console_full = false;
    for (int i = 0; i < Console_num_lines; i++)
    {
        Textchat_lines[i].text[0] = '\0';
        Textchat_lines[i].player_name[0] = '\0';
        Textchat_lines[i].team_message = false;
        Textchat_lines[i].wrapped_line = false;
    }
    Chat_timer.invalidate();
    Textchat_lines_next_index = 0;
    Textchat_lines_scroll_index = -1;
}

// The scroll buttons' region in the 640x480 (below 1280 pixels wide) or 1280x720 coordinates of the mouse.
static void console_scale_region(vector& top_left, vector& bottom_right)
{
    // Magic numbers: 1280 pixels, the 640x480 and 1280x720 interface resolutions.
    float w = 1280.0f;
    float h = 720.0f;
    if (Config.screen_width < 0x500)
    {
        w = 640.0f;
        h = 480.0f;
    }
    top_left.x = (w / (float)Config.screen_width) * top_left.x;
    top_left.y = (h / (float)Config.screen_height) * top_left.y;
    bottom_right.x = (w / (float)Config.screen_width) * bottom_right.x;
    bottom_right.y = (h / (float)Config.screen_height) * bottom_right.y;
}

// The chat window's y as the scroll regions compute it (double precision).
static int chat_window_y_d()
{
    if ((int)Chat_position < 0 || (int)Chat_position > 5)
    {
        return 0;
    }
    return (int)((double)Config.screen_height * (double)0.926f);
}

// 0x0058CE00 (:3136)
void console_get_scrollup_region(vector& top_left, vector& bottom_right)
{
    top_left.x = 7.0f;
    top_left.y = (float)(chat_window_y_d() + 2);
    top_left.z = 0.0f;
    bottom_right.x = 17.0f;
    bottom_right.z = 0.0f;
    bottom_right.y = (float)(chat_window_y_d() + 12);
    console_scale_region(top_left, bottom_right);
}

// 0x0058CFC0 (:3163)
void console_get_scrolldown_region(vector& top_left, vector& bottom_right)
{
    top_left.x = 7.0f;
    top_left.y = (float)((int)(((double)Config.screen_height * 54.0) / 768.0) - 12 + chat_window_y_d());
    top_left.z = 0.0f;
    bottom_right.x = 15.0f;
    bottom_right.y = (float)(get_chat_window_h() - 2 + get_chat_window_y());
    bottom_right.z = 0.0f;
    console_scale_region(top_left, bottom_right);
}

// 0x0058D1B0 (:3187)
void console_scrollup_mouse_over(bool over)
{
    Textchat_mouse_over_up = over;
}

// 0x0058D1C0 (:3196)
void console_scrolldown_mouse_over(bool over)
{
    Textchat_mouse_over_down = over;
}

// 0x0058D1D0 (:3205)
void console_scrollup_pressed()
{
    if (Textchat_lines_scroll_index == -1)
    {
        Textchat_lines_scroll_index = Textchat_lines_next_index - 1;
    }
    Textchat_lines_scroll_index--;
    if (Textchat_console_full)
    {
        if (Textchat_lines_scroll_index < 0)
        {
            Textchat_lines_scroll_index = Console_num_lines - 1;
        }
    }
    else if (Textchat_lines_scroll_index < 0)
    {
        Textchat_lines_scroll_index = 0;
    }
}

// 0x0058D210 (:3226)
// Notes (decompiled behaviour, kept as is): the wrap compares with 256 (> 0x100), not 255.
void console_scrolldown_pressed()
{
    if (Textchat_lines_scroll_index == -1)
    {
        return;
    }
    if (Textchat_lines_next_index - 2 <= Textchat_lines_scroll_index)
    {
        Textchat_lines_scroll_index = -1;
        return;
    }
    Textchat_lines_scroll_index++;
    if (Textchat_lines_scroll_index > 0x100)
    {
        Textchat_lines_scroll_index = 0;
    }
}

// 0x0058D250 (:3244)
void console_disable_chat_render(bool disable)
{
    Chat_rendering_disabled = disable;
}

// 0x0058D260 (:3253)
vchat_position get_console_chat_window_pos()
{
    return Chat_position;
}

// 0x0058D270 (:3258)
void console_change_chat_window_pos(vchat_position chat_position)
{
    Chat_position = chat_position;
}
