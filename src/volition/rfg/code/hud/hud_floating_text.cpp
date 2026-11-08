// Original: volition/rfg/code/hud/hud_floating_text.cpp
//
// Floating text (stub_impl.md S7): strings drifting up and fading out, in two circular lists.

#include "volition/rfg/code/hud/hud_floating_text.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"

// ---- file-private types (rfg.pdb)
// size 0x3C
struct floating_string
{

    float x; // 0x00
    float y; // 0x04
    char str[32]; // 0x08
    int font_id; // 0x28
    color rgba; // 0x2C
    float alpha; // 0x30
    floating_string* next; // 0x34
    floating_string* prev; // 0x38
};
static_assert(sizeof(floating_string) == 0x3C, "floating_string");

// ---- file-static globals
static floating_string* Floating_string_list; // 0x02B9DC74
static floating_string* Floating_string_free_list; // 0x02B9DC78
static floating_string Floating_strings[8]; // 0x02BD9148

// 0x00842E60
static void free_all_floating_strings()
{
    Floating_string_free_list = Floating_strings;
    Floating_string_list = nullptr;
    for (int i = 0; i < 8; i++)
    {
        Floating_strings[i].next = &Floating_strings[(i + 1) % 8];
        Floating_strings[i].prev = &Floating_strings[(i + 7) % 8];
    }
}

// 0x00842F00
void hud_floating_text_system_init()
{
    free_all_floating_strings();
    for (floating_string& floating : Floating_strings)
    {
        floating.str[0x1F] = '\0';
    }
}

// 0x00842F30
void hud_floating_text_level_init_post()
{
    free_all_floating_strings();
}

// 0x00842F40
void hud_floating_text_render()
{
    floating_string* floating = Floating_string_list;
    if (floating == nullptr)
    {
        return;
    }
    do
    {
        gr_set_color(floating->rgba);
        gr_string((int)floating->x, (int)floating->y, floating->str, floating->font_id, GM_FILTER);
        floating = floating->next;
    } while (floating != Floating_string_list);
}

// 0x00842F90
void hud_floating_text_add(float x, float y, const char* str, int r, int g, int b, int font_id)
{
    floating_string* floating = Floating_string_free_list;
    if (floating == nullptr)
    {
        return;
    }
    // Take it from the free list.
    if (floating->next == floating)
    {
        Floating_string_free_list = nullptr;
    }
    else
    {
        Floating_string_free_list = floating->next;
        if (floating->prev != nullptr)
        {
            floating->prev->next = floating->next;
        }
        if (floating->next != nullptr)
        {
            floating->next->prev = floating->prev;
        }
    }
    // Append it to the list.
    floating->prev = nullptr;
    floating->next = nullptr;
    if (Floating_string_list == nullptr)
    {
        floating->next = floating;
        floating->prev = floating;
        Floating_string_list = floating;
    }
    else
    {
        floating->prev = Floating_string_list->prev;
        floating->next = Floating_string_list;
        Floating_string_list->prev->next = floating;
        Floating_string_list->prev = floating;
    }
    floating->x = x;
    floating->y = y;
    strncpy_safe(floating->str, str, 0x1F);
    floating->rgba.red = (unsigned char)r;
    floating->rgba.green = (unsigned char)g;
    floating->rgba.blue = (unsigned char)b;
    floating->rgba.alpha = 0xFF;
    floating->alpha = 1.0f;
    floating->font_id = font_id;
}

// 0x00843070
void hud_floating_text_add_centered(float cx, float cy, const char* str, int r, int g, int b, int font_id)
{
    int w, h;
    gr_get_string_size(w, h, str, -1, font_id);
    hud_floating_text_add(cx - (float)(w >> 1), cy - (float)(h >> 1), str, r, g, b, font_id);
}

// 0x00843110
void hud_floating_text_process()
{
    floating_string* floating = Floating_string_list;
    while (floating != nullptr)
    {
        floating_string* next = floating->next;
        // Magic numbers: the text fades out in 1.5 s and drifts up 25 pixels per second.
        const float alpha = floating->alpha - Frametime_uncompressed * 0.6666667f;
        floating_string* following = next != Floating_string_list ? next : nullptr;
        floating->alpha = alpha;
        if (alpha > 0.0f)
        {
            floating->rgba.alpha = (unsigned char)(int)(alpha * 255.0f);
            floating->y = floating->y - Frametime_uncompressed * 25.0f;
        }
        else
        {
            if (Floating_string_list == floating && (Floating_string_list = next, next == floating))
            {
                Floating_string_list = nullptr;
            }
            else
            {
                if (floating->prev != nullptr)
                {
                    floating->prev->next = next;
                }
                if (floating->next != nullptr)
                {
                    floating->next->prev = floating->prev;
                }
            }
            floating->prev = nullptr;
            floating->next = nullptr;
            if (Floating_string_free_list == nullptr)
            {
                floating->next = floating;
                floating->prev = floating;
                Floating_string_free_list = floating;
            }
            else
            {
                floating->prev = Floating_string_free_list->prev;
                floating->next = Floating_string_free_list;
                Floating_string_free_list->prev->next = floating;
                Floating_string_free_list->prev = floating;
            }
        }
        floating = following;
    }
}
