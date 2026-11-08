// Original: volition/rfg/code/hud/hud_message.cpp
//
// HUD messages (stub_impl.md S7): the old text list and the entry points the game uses, which forward to the vint message
// set of ui_document_hud.cpp. Nothing adds to the old list in the shipped game, so its render draws nothing.

#include "volition/rfg/code/hud/hud_message.h"

#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_internal.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/vfont.h"

// ---- file-private types (rfg.pdb)
// size 0x5C
struct hud_message_minor_info
{

    char str[81]; // 0x00
    int str_width; // 0x54
    timestamp duration; // 0x58
};
static_assert(sizeof(hud_message_minor_info) == 0x5C, "hud_message_minor_info");

// ---- file-static globals
static hud_message_minor_info Hud_message_minor; // 0x01661788
static hud_message* Hud_message_list; // 0x02B9DC80
static hud_message* Hud_message_free_list; // 0x02B9DC84
static hud_message* Hud_message_pending_list; // 0x02B9DC88
static int Font_message_height; // 0x02B9DC8C
static int First_message_y_position; // 0x02B9DC90
static hud_message Hud_messages[4]; // 0x02BE4F50

// 0x008448A0
hud_message::hud_message()
{
}

// 0x008448C0
static void free_all_hud_messages()
{
    Hud_message_minor.str[0] = '\0';
    Hud_message_list = nullptr;
    Hud_message_pending_list = nullptr;
    Hud_message_free_list = Hud_messages;
    for (int i = 0; i < 4; i++)
    {
        Hud_messages[i].next = &Hud_messages[(i + 1) % 4];
        Hud_messages[i].prev = &Hud_messages[(i + 3) % 4];
    }
}

// 0x00844920
void hud_message_system_init()
{
    free_all_hud_messages();
    const float res_scale = X_res_multiplier - Y_res_multiplier < 0.0f ? X_res_multiplier : Y_res_multiplier;
    Font_message_height = fl_round((float)(font_get_render_height(General_font) + 2) * res_scale);
    // Magic numbers: the list's bottom is 390 pixels (at 1280x720) down, 5 messages high.
    const float res_scale_y = X_res_multiplier - Y_res_multiplier >= 0.0f ? Y_res_multiplier : X_res_multiplier;
    First_message_y_position = fl_round(res_scale_y * 390.0f) + Font_message_height * -5;
}

// 0x00844A40
void hud_message_level_init_post()
{
    free_all_hud_messages();
}

// 0x00844AA0
void hud_message_process()
{
}

// 0x008559F0
void hud_message_render()
{
    hud_message_minor_info minor = Hud_message_minor;
    if (Hud_message_minor.str[0] != '\0')
    {
        // Magic number: fades out over the last half second.
        float alpha = (float)minor.duration.time_until() * 0.001f * 2.0f;
        if (alpha - 0.0f < 0.0f)
        {
            alpha = 0.0f;
        }
        if (alpha - 1.0f >= 0.0f)
        {
            alpha = 1.0f;
        }
        if ((int)(alpha * 255.0f) != 0)
        {
            gr_set_color(200, 200, 200, (int)(alpha * 255.0f));
            const int width = rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
            const int height = rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
            gr_string((int)((double)width * 0.75) - minor.str_width / 2, (int)((double)height * 0.800000011920929), minor.str, General_font, GM_FILTER);
        }
    }
    int line = 0;
    hud_message* message = Hud_message_list;
    while (message != nullptr)
    {
        const float time_left = (float)message->duration.time_until() * 0.001f;
        float alpha = time_left * 2.0f;
        if (alpha - 0.0f < 0.0f)
        {
            alpha = 0.0f;
        }
        if (alpha - 1.0f >= 0.0f)
        {
            alpha = 1.0f;
        }
        gr_set_color(200, 200, 200, (int)(alpha * 255.0f));
        // Magic number: a new message shrinks to its size during its first 0.2 s.
        float pop = 1.0f;
        const float pop_start = (float)message->original_duration - 0.2f;
        if (pop_start < time_left)
        {
            pop = (time_left - pop_start) / 0.2f + 1.0f;
        }
        const float res_scale = X_res_multiplier - Y_res_multiplier >= 0.0f ? Y_res_multiplier : X_res_multiplier;
        gr_string_scale(0x8000, Font_message_height * line + First_message_y_position, message->str, res_scale * pop, General_font, GM_FILTER);
        line++;
        if (line >= 5)
        {
            return;
        }
        message = message->next != Hud_message_list ? message->next : nullptr;
    }
}

// 0x00855CE0
hud_message_handle hud_message_add(const char* str, float duration, void* data1, void* data2, int* sound_effect)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_add_primary_message(text, duration, data1, data2, sound_effect);
}

// 0x00844AB0
hud_message_handle hud_message_add(const wchar_t* str, float duration, void* data1, void* data2, int* sound_effect)
{
    return ui_add_primary_message(str, duration, data1, data2, sound_effect);
}

// 0x00855D60
hud_message_handle hud_message_add_minor(const char* str, float display_time, bool is_special)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_add_secondary_message(text, display_time, is_special, false);
}

// 0x00844AE0
hud_message_handle hud_message_add_minor(const wchar_t* str, float display_time, bool is_special)
{
    return ui_add_secondary_message(str, display_time, is_special, false);
}

// 0x00844B10
void hud_message_get_icon_with_localized_text(wchar_t* out_string, int string_size, const wchar_t* icon_string, const char* unlocalized_string)
{
    const wchar_t* text = localization_get_display_string_req(unlocalized_string, nullptr);
    if (icon_string != nullptr)
    {
        _snprintf_safe(out_string, string_size, L"[image:%ls] %ls", icon_string, text);
        return;
    }
    _snprintf_safe(out_string, string_size, L"%ls", text);
}

// 0x00855DD0
hud_message_handle hud_message_add_user(const char* str, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_add_user_message(text, type, position_x, position_y, outlined, lifespan);
}

// 0x00844B60
hud_message_handle hud_message_add_user(const wchar_t* str, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan)
{
    return ui_add_user_message(str, type, position_x, position_y, outlined, lifespan);
}

// 0x00844BA0
void hud_message_remove(hud_message_handle handle)
{
    ui_remove_primary_message(handle);
}

// 0x00844BB0
void hud_message_remove_user(hud_message_handle handle)
{
    ui_remove_user_message(handle);
}

// 0x00844BC0
void hud_message_remove_minor(hud_message_handle handle)
{
    ui_remove_secondary_message(handle);
}

// 0x00844BD0
void* hud_message_get_data1(hud_message_handle handle)
{
    return ui_primary_message_get_data1(handle);
}

// 0x00844BE0
void* hud_message_get_data2(hud_message_handle handle)
{
    return ui_primary_message_get_data2(handle);
}

// 0x00855E60
hud_message_handle hud_message_change_message(hud_message_handle handle, const char* str, float duration, void* data1, void* data2)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_change_primary_message(handle, text, duration, data1, data2);
}

// 0x00844BF0
hud_message_handle hud_message_change_message(hud_message_handle handle, const wchar_t* str, float duration, void* data1, void* data2)
{
    return ui_change_primary_message(handle, str, duration, data1, data2);
}

// 0x00855EE0
hud_message_handle hud_message_change_minor_message(hud_message_handle handle, const char* str, float duration)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_change_secondary_message(handle, text, duration);
}

// 0x00844C20
hud_message_handle hud_message_change_minor_message(hud_message_handle handle, const wchar_t* str, float duration)
{
    return ui_change_secondary_message(handle, str, duration);
}

// 0x00855F50
hud_message_handle hud_message_change_user(hud_message_handle handle, const char* str)
{
    wchar_t text[0x400];
    copy_to_wide(text, str, 0x400);
    return ui_change_user_message(handle, text);
}

// 0x00844C40
hud_message_handle hud_message_change_user(hud_message_handle handle, const wchar_t* str)
{
    return ui_change_user_message(handle, str);
}
