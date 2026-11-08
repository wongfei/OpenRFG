// Original: volition/rfg/code/os/dc_menu.h
// Implementation: volition/rfg/code/os/dc_menu.cpp
//
// The debug menus: menus of items that run a console command or open another menu. The menu types are shared with
// activity_mission_menu.cpp, which pools them; placement in this header inferred.
#pragma once

// size 0xC
struct dc_menu_item
{
    char* text; // 0x00
    char* action; // 0x04, a console command
    char* goto_menu; // 0x08, the title of the menu to open
};

// size 0x208
struct dc_menu
{
    char* title_text; // 0x00
    int num_items; // 0x04
    dc_menu_item* items[128]; // 0x08
};

void dc_menu_system_init(); // 0x00AE7C70 (:453)
