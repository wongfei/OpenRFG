// Original: volition/rfg/code/os/dc_menu.cpp
//
// Partial: the menu table (debug_menu.tbl: "$Menu:" titles with "$Item:" entries that either run a console command
// or "$Goto:" another menu) and the "All Levels" menu built from the .bcy files (stub_impl.md S3). Retail ships no
// debug_menu.tbl, so dc_menu_system_init loads nothing. Scrolling, rendering and the menu loop (dc_menu_launch) are
// not ported.

#include "volition/rfg/code/os/dc_menu.h"

#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/parse.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

static dc_menu* Dc_menus[18]; // 0x035B5A70
static int Num_dc_menus; // 0x035B5F58
static bool Dc_menus_inited; // 0x035B5F60

// 0x00AE78A0 (:113)
// The string copied into Permanent_mempool.
static char* dc_strdup(const char* s)
{
    char* copy = (char*)Permanent_mempool.alloc((unsigned int)strlen(s) + 1, 1);
    strcpy(copy, s);
    return copy;
}

// 0x00AE78D0 (:125)
static void dc_menu_item_init(dc_menu_item* itp, const char* _text, const char* _action, const char* _goto_menu)
{
    itp->text = _text != nullptr ? dc_strdup(_text) : nullptr;
    itp->action = _action != nullptr ? dc_strdup(_action) : nullptr;
    itp->goto_menu = _goto_menu != nullptr ? dc_strdup(_goto_menu) : nullptr;
}

// 0x00AE79B0 (:144)
static void dc_menu_init(dc_menu* mp, const char* _title_text)
{
    mp->num_items = 0;
    mp->title_text = dc_strdup(_title_text);
}

// 0x00AE79F0 (:151)
// original: no check against the 128 items
static void dc_menu_add_selection(dc_menu* mp, const char* text, const char* action, const char* goto_menu)
{
    dc_menu_item* itp = (dc_menu_item*)Permanent_mempool.alloc(sizeof(dc_menu_item), 4);
    dc_menu_item_init(itp, text, action, goto_menu);
    mp->items[mp->num_items] = itp;
    mp->num_items++;
}

// 0x00AE7AE0 (:368)
// The "Levels" menu gets an "All Levels..." entry that opens a new "All Levels" menu with a "reload <name>" entry per
// .bcy file.
static void create_all_levels_menu()
{
    dc_menu* levels = nullptr;
    for (int i = 0; i < Num_dc_menus; i++)
    {
        if (v_stricmp("Levels", Dc_menus[i]->title_text) == 0)
        {
            levels = Dc_menus[i];
            break;
        }
    }
    if (levels == nullptr)
    {
        return;
    }
    dc_menu_add_selection(levels, "All Levels...", nullptr, "All Levels");
    dc_menu* all = (dc_menu*)Permanent_mempool.alloc(sizeof(dc_menu), 4);
    dc_menu_init(all, "All Levels");
    // original: no check against the 18 menus
    Dc_menus[Num_dc_menus] = all;
    Num_dc_menus++;
    const char* files[256];
    const unsigned int num_files = cf_find_ext(files, 256, "bcy");
    for (int i = 0; i < (int)num_files; i++)
    {
        char command[256];
        sprintf(command, "reload %s", files[i]);
        dc_menu_add_selection(all, files[i], command, nullptr);
    }
}

// 0x00AE7C70 (:453)
void dc_menu_system_init()
{
    parse p;
    if (Dc_menus_inited)
    {
        return;
    }
    Dc_menus_inited = true;
    Num_dc_menus = 0;
    if (p.open("debug_menu.tbl") != 0)
    {
        return;
    }
    while (p.parse_optional("$Menu:"))
    {
        char title[32];
        p.parse_string(title, sizeof(title), '"', '"');
        dc_menu* mp = (dc_menu*)Permanent_mempool.alloc(sizeof(dc_menu), 4);
        dc_menu_init(mp, title);
        Dc_menus[Num_dc_menus] = mp;
        Num_dc_menus++;
        while (p.parse_optional("$Item:"))
        {
            char text[128];
            char target[128];
            p.parse_string(text, sizeof(text), '"', '"');
            if (p.parse_optional("$Goto:"))
            {
                p.parse_string(target, sizeof(target), '"', '"');
                dc_menu_add_selection(mp, text, nullptr, target);
            }
            else
            {
                p.parse_string(target, sizeof(target), '"', '"');
                dc_menu_add_selection(mp, text, target, nullptr);
            }
        }
    }
    p.close();
    create_all_levels_menu();
}
