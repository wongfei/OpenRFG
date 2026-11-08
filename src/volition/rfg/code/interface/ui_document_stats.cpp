// Original: volition/rfg/code/interface/ui_document_stats.cpp
//
// The statistics screen (rfg_ui_stat_screen): four categories of tracked stats on one page, the achievements on the other.

#include "volition/rfg/code/interface/ui_document_stats.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0xC
struct ui_stats_info
{
    const char* name; // 0x00
    bool indent; // 0x04
    tracked_stats_indices stat; // 0x08
};
static_assert(sizeof(ui_stats_info) == 0xC, "ui_stats_info");

// Magic numbers: the rows the document has per page, and the sizes of the tables.
static const int Max_stat_rows = 17;
static const int Max_achievement_rows = 4;
static const int Num_destruction_stats = 12;
static const int Num_death_stats = 9;
static const int Num_time_stats = 11;
static const int Num_weapon_stats = 23;
// Magic number: the weapons category.
static const int Weapon_category = 2;

// ---- file-static globals
static const ui_stats_info Stat_destruction_stats_info[Num_destruction_stats] = // 0x01301280
{
    {"MENU_TOTAL_DESTRUCTION", false, (tracked_stats_indices)0x45},
    {"MENU_TOTAL_BUILDING_DESTRUCTION", true, (tracked_stats_indices)0x46},
    {"MENU_TOTAL_VEHICLE_DESTRUCTION", true, (tracked_stats_indices)0x47},
    {"MENU_TOTAL_PEOPLE_DESTRUCTION", true, (tracked_stats_indices)0x48},
    {"MENU_NUM_BLDGS_DESTROYED", false, (tracked_stats_indices)0x49},
    {"MENU_NUM_EDF_BLDGS_DESTROYED", false, (tracked_stats_indices)0x3F},
    {"MENU_NUM_VEHICLES_DESTROYED", false, (tracked_stats_indices)0x4F},
    {"MENU_NUM_CIVILIAN_WALKERS_DESTROYED", true, (tracked_stats_indices)0x4E},
    {"MENU_NUM_CIVILIAN_WHEELED_DESTROYED", true, (tracked_stats_indices)0x4D},
    {"MENU_NUM_EDF_WHEELED_DESTROYED", true, (tracked_stats_indices)0x4B},
    {"MENU_NUM_EDF_FLYERS_DESTROYED", true, (tracked_stats_indices)0x40},
    {"MENU_NUM_EDF_TANKS_DESTROYED", true, (tracked_stats_indices)0x4C},
};
static const ui_stats_info Stat_death_stats_info[Num_death_stats] = // 0x01301310
{
    {"MENU_PLAYER_DEATH_TOTAL", false, (tracked_stats_indices)0x53},
    {"MENU_PLAYER_GUNNED_DOWN", true, (tracked_stats_indices)0x50},
    {"MENU_PLAYER_BLOWN_UP", true, (tracked_stats_indices)0x51},
    {"MENU_PLAYER_CRUSHED", true, (tracked_stats_indices)0x52},
    {"MENU_EDF_DEATH_TOTAL", false, (tracked_stats_indices)0x57},
    {"MENU_EDF_GUNNED_DOWN", true, (tracked_stats_indices)0x54},
    {"MENU_EDF_BLOWN_UP", true, (tracked_stats_indices)0x55},
    {"MENU_EDF_CRUSHED", true, (tracked_stats_indices)0x56},
    {"MENU_TOTAL_COLLATERAL_DAMAGE", false, (tracked_stats_indices)0x42},
};
static const ui_stats_info Stat_time_stats_info[Num_time_stats] = // 0x01301380
{
    {"MENU_TOTAL_TIME_PLAYED", false, (tracked_stats_indices)0x58},
    {"MENU_KILOMETERS_WALKED", false, (tracked_stats_indices)0x60},
    {"MENU_KILOMETERS_DRIVEN", false, (tracked_stats_indices)0x61},
    {"MENU_TIME_SPENT_HEADER", false, STATS_INVALID},
    {"MENU_TIME_IN_YELLOW_ALERT", true, (tracked_stats_indices)0x59},
    {"MENU_TIME_IN_ORANGE_ALERT", true, (tracked_stats_indices)0x5A},
    {"MENU_TIME_IN_RED_ALERT", true, (tracked_stats_indices)0x5B},
    {"MENU_TIME_SPENT_WALKING", true, (tracked_stats_indices)0x5C},
    {"MENU_TIME_SPENT_DRIVING", true, (tracked_stats_indices)0x5D},
    {"MENU_TIME_IN_MISSIONS", true, (tracked_stats_indices)0x5E},
    {"MENU_TIME_IN_ACTIVITIES", true, (tracked_stats_indices)0x5F},
};
static const ui_stats_info Stat_weapon_stats_info[Num_weapon_stats] = // 0x01301410
{
    {"MENU_FAVORITE_WEAPON", false, STATS_SP_TOTAL_WEAPON_KILLS_MOST},
    {"MENU_LEAST_FAVORITE_WEAPON", false, STATS_SP_TOTAL_WEAPON_KILLS_LEAST},
    {"MENU_GUERRILLA_TECH_HEADER", false, STATS_INVALID},
    {"WEP_SLEDGEHAMMER", true, STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER},
    {"WEP_CHARGE_PLACER", true, STATS_SP_TOTAL_WEAPON_KILLS_REMOTE_CHARGES},
    {"WEP_MINE_PLACER", true, STATS_SP_TOTAL_WEAPON_KILLS_PROXIMITY_MINES},
    {"WEP_THERMOBARIC_ROCKETS", true, STATS_SP_TOTAL_WEAPON_KILLS_THERMOBARIC_ROCKETS},
    {"WEP_RPG_LAUNCHER", true, STATS_SP_TOTAL_WEAPON_KILLS_ROCKET_LAUNCHER},
    {"WEP_GRINDER", true, STATS_SP_TOTAL_WEAPON_KILLS_GRINDER},
    {"WEP_ARC_WELDER", true, STATS_SP_TOTAL_WEAPON_KILLS_ARC_WELDER},
    {"WEP_NANO_RIFLE", true, STATS_SP_TOTAL_WEAPON_KILLS_NANO_RIFLE},
    {"MENU_EDF_TECH_HEADER", false, STATS_INVALID},
    {"WEP_EDF_PISTOL", true, STATS_SP_TOTAL_WEAPON_KILLS_PISTOL},
    {"WEP_ASSAULT_RIFLE", true, STATS_SP_TOTAL_WEAPON_KILLS_M16},
    {"WEP_ENFORCER", true, STATS_SP_TOTAL_WEAPON_KILLS_ENFORCER},
    {"MENU_GH_PEACEKEEPER", true, STATS_SP_TOTAL_WEAPON_KILLS_AUTOBLASTER},
    {"MENU_GH_GAUSS", true, STATS_SP_TOTAL_WEAPON_KILLS_GAUSS_RIFLE},
    {"WEP_SNIPER_RIFLE", true, STATS_SP_TOTAL_WEAPON_KILLS_SNIPER_RIFLE},
    {"WEP_RAIL_DRIVER", true, STATS_SP_TOTAL_WEAPON_KILLS_RAIL_DRIVER},
    {"WEP_SINGULARITY_BOMB", true, STATS_SP_TOTAL_WEAPON_KILLS_SINGULARITY_BOMB},
    {"MENU_MARAUDER_TECH_HEADER", false, STATS_INVALID},
    {"WEP_SHOTGUN", true, STATS_SP_TOTAL_WEAPON_KILLS_SHOTGUN},
    {"WEP_GUTTER", true, STATS_SP_TOTAL_WEAPON_KILLS_GUTTER},
};
static const char* Stat_screen_page_titles[2] = {"MENU_STATS_PAGE", "MENU_STATS_ACHIEVEMENTS"}; // 0x01664EEC
static const char* Stat_gameplay_category_titles[4] = {"MENU_DESTRUCTION_STATS", "MENU_DEATH_KILLS_STATS", "MENU_WEAPON_STATS", "MENU_TIME_STATS"}; // 0x01664EF4
static int Data_header_handle_uistats = 16; // 0x01664F04
static int Data_row_group_handle_uistats = 16; // 0x01664F08
static int Data_stat_row_group_handle = 16; // 0x01664F0C
static int Page_tab_handle_uistats = 16; // 0x01664F10
static int Toggle_data_handle = 16; // 0x01664F14
static int Hint_data_handle = 16; // 0x01664F18
static int Input_id_uistats = -1; // 0x01664F1C
static int Doc_handle_uistats = -1; // 0x01664F20
static stat_screen_pages Current_page = SSP_NUM_PAGES; // 0x01664F24
static int Current_gameplay_category = 4; // 0x01664F28
static int Hint_clicked = -1; // 0x01664F2C
static const char* kPageHeaders[2] = {"page1", "page2"}; // 0x016685B4
static const char* kHintImage = "hint_button_1"; // 0x016685BC
static const char* kHintText = "hints_text_1"; // 0x016685C0
static const char* kScrollBarOutline = "black_scrollfill"; // 0x016685C4
static int Data_stat_row_handles[Max_stat_rows]; // 0x02C02B50
static int Data_row_handles_uistats[Max_achievement_rows]; // 0x02C02CBC
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp's; true while the game runs under the menu
static bool Exit_uistats; // 0x02C08136
static bool Loaded_uistats; // 0x02C08137
static unsigned int Control_id_uistats; // 0x02C08144
static int Num_stats_to_display; // 0x02C08148
static int Num_stats_displayed; // 0x02C0814C
static int Display_start_index_uistats; // 0x02C08150
static int Weapon_stats_display_index; // 0x02C08154
static int Selected_row_uistats; // 0x02C08158
static bool Left_arrow_clicked_uistats; // 0x02C0815C
static bool Right_arrow_clicked_uistats; // 0x02C0815D

// 0x0089E4E0 (:258)
void ui_stats_clean_up_stats_page()
{
    vint_data_group* group = vint_data_group::find(Data_stat_row_group_handle);
    if (group != nullptr)
    {
        for (int i = (int)group->get_num_data_items() - 1; i >= 0; i--)
        {
            group->remove_data_item(Data_stat_row_handles[i]);
        }
    }
}

// 0x0089E520 (:271)
void ui_stats_clean_up_achievements_page()
{
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_uistats);
    if (group != nullptr)
    {
        for (int i = (int)group->get_num_data_items() - 1; i >= 0; i--)
        {
            group->remove_data_item(Data_row_handles_uistats[i]);
        }
    }
}

// 0x0089E560 (:284)
void ui_stats_set_up_new_page(stat_screen_pages new_page)
{
    if ((int)new_page >= 2)
    {
        return;
    }
    if (Current_page == SSP_STATS_PAGE)
    {
        ui_stats_clean_up_stats_page();
    }
    else if (Current_page == SSP_ACHIEVEMENTS)
    {
        ui_stats_clean_up_achievements_page();
    }
    Display_start_index_uistats = 0;
    Selected_row_uistats = 0;
    Current_page = new_page;
    if (new_page == SSP_STATS_PAGE)
    {
        Current_gameplay_category = 0;
        Num_stats_to_display = Num_destruction_stats;
    }
    else if (new_page == SSP_ACHIEVEMENTS)
    {
        Num_stats_to_display = stats_get_num_achievements();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x008C9A10 (:323)
void ui_stats_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("HeaderData");
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 3);
        Data_header_handle_uistats = item->get_handle();
        ui_add_data_item(Doc_handle_uistats, Data_header_handle_uistats);
    }
}

// 0x008C9A90 (:343)
void ui_stats_create_tab_data()
{
    vint_data_item* item = vint_data_item::create_new("PageTabData");
    if (item != nullptr)
    {
        const vint_variant_type types[6] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 6);
        Page_tab_handle_uistats = item->get_handle();
        ui_add_data_item(Doc_handle_uistats, Page_tab_handle_uistats);
    }
}

// 0x008C9B20 (:367)
void ui_stats_create_toggle_data()
{
    vint_data_item* item = vint_data_item::create_new("ToggleData");
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        Toggle_data_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uistats, Toggle_data_handle);
    }
}

// 0x008C9BA0 (:387)
void ui_stats_create_hint_data()
{
    vint_data_item* item = vint_data_item::create_new("HintData");
    if (item != nullptr)
    {
        const vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 5);
        Hint_data_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uistats, Hint_data_handle);
    }
}

// 0x008C9C20 (:409)
void ui_stats_create_row_data()
{
    vint_data_group* group = vint_data_group::create_new("RowData");
    if (group != nullptr)
    {
        Data_row_group_handle_uistats = group->get_handle();
        ui_add_data_group(Doc_handle_uistats, Data_row_group_handle_uistats);
    }
}

// 0x008C9C50 (:422)
void ui_stats_create_stat_row_data()
{
    vint_data_group* group = vint_data_group::create_new("StatRowData");
    if (group != nullptr)
    {
        Data_stat_row_group_handle = group->get_handle();
        ui_add_data_group(Doc_handle_uistats, Data_stat_row_group_handle);
    }
}

// 0x008C9C80 (:435)
void ui_stats_create_data_items()
{
    ui_stats_create_header_data();
    ui_stats_create_tab_data();
    ui_stats_create_toggle_data();
    ui_stats_create_hint_data();
    ui_stats_create_row_data();
    ui_stats_create_stat_row_data();
}

// 0x0089E5F0 (:446)
void ui_stats_process_header_data()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle_uistats);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant("MENU_STATISTICS"));
    float scroll = 0.0f;
    if ((int)Current_page < 1)
    {
        if (Num_stats_displayed <= Max_stat_rows)
        {
            item->set_element(1, vint_variant(false));
        }
        else
        {
            item->set_element(1, vint_variant(true));
            const int first = Current_gameplay_category == Weapon_category ? Weapon_stats_display_index : Display_start_index_uistats;
            scroll = (float)first / (float)(Num_stats_displayed - Max_stat_rows);
        }
    }
    else
    {
        item->set_element(1, vint_variant(Num_stats_to_display > Max_achievement_rows));
        scroll = (float)Display_start_index_uistats / (float)(Num_stats_to_display - Max_achievement_rows);
    }
    item->set_element(2, vint_variant(scroll));
}

// 0x0089E750 (:474)
void ui_stats_process_tab_data()
{
    vint_data_item* item = vint_data_item::find(Page_tab_handle_uistats);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Stat_screen_page_titles[0]));
        item->set_element(1, vint_variant(Stat_screen_page_titles[1]));
        item->set_element(2, vint_variant(""));
        item->set_element(3, vint_variant((int)Current_page > 1 ? 1 : (int)Current_page + 1));
        item->set_element(4, vint_variant((int)Current_page > 0));
        item->set_element(5, vint_variant((int)Current_page < 1));
    }
}

// 0x0089E8A0 (:491)
void ui_stats_process_toggle_data()
{
    vint_data_item* item = vint_data_item::find(Toggle_data_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(Current_page == SSP_STATS_PAGE));
        item->set_element(1, vint_variant("MENU_CATEGORY_LABEL"));
        if (Current_gameplay_category < 4)
        {
            item->set_element(2, vint_variant(Stat_gameplay_category_titles[Current_gameplay_category]));
        }
    }
}

// 0x0089E960 (:505)
void ui_stats_process_hint_data()
{
    vint_data_item* item = vint_data_item::find(Hint_data_handle);
    if (item == nullptr)
    {
        return;
    }
    static const control_button_action actions[4] = {CBA_MENU_SELECT, CBA_MENU_L2, CBA_MENU_R2, CBA_MENU_ABORT};
    for (int i = 0; i < 4; i++)
    {
        char image[64];
        char text[128];
        controls_get_button_image_name(image, 0x40, actions[i], Player_controller_index);
        keen::formatString(text, 0x80, "[image:%s]", image);
        item->set_element(i, vint_variant((const char*)text));
    }
    item->set_element(4, vint_variant("MENU_BACK"));
}

// A row of the weapons category past the first three shows only if the stat has a value. rfg.exe inlines this twice.
static bool ui_stats_stat_has_value(const tracked_stat* stat)
{
    if (stats_stat_is_int_value(stat))
    {
        return stats_get_stat_value(stat) > 0;
    }
    return stats_get_stat_value_fl(stat) > 0.0f;
}

// 0x0089EB40 (:534)
void ui_stats_process_stats()
{
    stats_update_destruction_totals();
    Num_stats_displayed = 0;
    const ui_stats_info* table;
    switch (Current_gameplay_category)
    {
    case 0:
        table = Stat_destruction_stats_info;
        Num_stats_displayed = Num_destruction_stats;
        break;
    case 1:
        table = Stat_death_stats_info;
        Num_stats_displayed = Num_death_stats;
        break;
    case Weapon_category:
        table = Stat_weapon_stats_info;
        for (int i = 0; i < Num_weapon_stats; i++)
        {
            const tracked_stats_indices index = table[i].stat;
            bool show = true;
            // Magic number: 0x27F, the number of tracked stats.
            if (index != STATS_INVALID && (int)index <= 0x27E)
            {
                const tracked_stat* stat = nullptr;
                stats_get_stat(stat, index);
                show = i < 3 || ui_stats_stat_has_value(stat);
            }
            if (show)
            {
                if (i == Display_start_index_uistats)
                {
                    Weapon_stats_display_index = Num_stats_displayed;
                }
                Num_stats_displayed++;
            }
        }
        break;
    case 3:
        table = Stat_time_stats_info;
        Num_stats_displayed = Num_time_stats;
        break;
    default:
        return;
    }
    vint_data_group* group = vint_data_group::find(Data_stat_row_group_handle);
    if (group == nullptr || Num_stats_to_display <= 0)
    {
        return;
    }
    int num_items = (int)group->get_num_data_items();
    const int rows_wanted = Num_stats_displayed > Max_stat_rows ? Max_stat_rows : Num_stats_displayed;
    for (int i = num_items - 1; i >= rows_wanted; i--)
    {
        group->remove_data_item(Data_stat_row_handles[i]);
    }
    for (int i = num_items; i < rows_wanted; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            const vint_variant_type types[5] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
            item->set_template(types, 5);
            Data_stat_row_handles[i] = item->get_handle();
        }
    }
    num_items = (int)group->get_num_data_items();
    int start = 0;
    int limit = num_items;
    if (Current_gameplay_category == Weapon_category)
    {
        limit = Num_weapon_stats;
        start = Weapon_stats_display_index;
    }
    int row = 0;
    for (int i = start; i < limit; i++)
    {
        if (row >= num_items)
        {
            continue;
        }
        vint_data_item* item = vint_data_item::find(Data_stat_row_handles[row]);
        if (item == nullptr)
        {
            continue;
        }
        const bool weapons = Current_gameplay_category == Weapon_category;
        const int index = weapons ? i : i + Display_start_index_uistats;
        char value[64];
        memset(value, 0, sizeof(value));
        // Magic numbers: rows 2, 11 and 20 of the weapons table are headers, above a column of kills.
        if (weapons && (index == 11 || index == 2 || index == 20))
        {
            strcpy(value, "MENU_KILLS");
        }
        else
        {
            const ui_stats_info& info = table[index];
            if (info.stat != STATS_INVALID && (int)info.stat <= 0x27E)
            {
                const tracked_stat* stat = nullptr;
                stats_get_stat(stat, info.stat);
                if (weapons && index >= 3 && index != 11 && index != 20 && !ui_stats_stat_has_value(stat))
                {
                    continue;
                }
                if (stat != nullptr)
                {
                    if (info.stat == STATS_SP_TOTAL_WEAPON_KILLS_LEAST || info.stat == STATS_SP_TOTAL_WEAPON_KILLS_MOST)
                    {
                        const char* name = info.stat == STATS_SP_TOTAL_WEAPON_KILLS_LEAST ? stats_get_worst_weapon_name() : stats_get_best_weapon_name();
                        if (name == nullptr)
                        {
                            name = "MENU_NONE";
                        }
                        // Original bug, kept: the weapon's name is passed as the format string.
                        _snprintf_safe(value, 0x40, name);
                    }
                    else
                    {
                        stats_get_string_value(value, 0x40, *stat);
                    }
                }
            }
        }
        row++;
        item->set_element(0, vint_variant(row));
        item->set_element(1, vint_variant(table[index].name));
        item->set_element(2, vint_variant((const char*)value));
        item->set_element(3, vint_variant(table[index].indent ? 20 : 0)); // Magic number: the indent in pixels.
        item->set_element(4, vint_variant(false));
    }
}

// 0x0089EFF0 (:705)
void ui_stats_process_achievements()
{
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_uistats);
    if (group == nullptr || Num_stats_to_display <= 0)
    {
        return;
    }
    int num_items = (int)group->get_num_data_items();
    const int rows_wanted = Num_stats_to_display > Max_achievement_rows ? Max_achievement_rows : Num_stats_to_display;
    for (int i = num_items - 1; i >= rows_wanted; i--)
    {
        group->remove_data_item(Data_row_handles_uistats[i]);
    }
    for (int i = num_items; i < rows_wanted; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            const vint_variant_type types[8] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_BOOL};
            item->set_template(types, 8);
            Data_row_handles_uistats[i] = item->get_handle();
        }
    }
    num_items = (int)group->get_num_data_items();
    char name_text[384];
    char desc_text[384];
    char progress_text[64];
    name_text[0] = '\0';
    desc_text[0] = '\0';
    for (int i = 0; i < num_items; i++)
    {
        vint_data_item* item = vint_data_item::find(Data_row_handles_uistats[i]);
        if (item == nullptr)
        {
            continue;
        }
        const bool selected = Selected_row_uistats == i;
        const achievement* ach = stats_get_achievement(Display_start_index_uistats + i);
        vint_encode_wide_text(name_text, 0x180, ach->display_name);
        vint_encode_wide_text(desc_text, 0x180, ach->display_desc);
        const float progress = stats_get_achievement_progress(progress_text, ach->achievement_id);
        bool achieved = false;
        if (multi_core_platform_is_signed_into_profile(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0))
        {
            achieved = ach->has_been_achieved;
        }
        if (progress >= 1.0f)
        {
            achieved = true;
            keen::formatString(progress_text, 0x40, "MENU_ACHIEVEMENT_COMPLETE");
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(ach->img_name));
        item->set_element(2, vint_variant(achieved));
        item->set_element(3, vint_variant((const char*)name_text));
        item->set_element(4, vint_variant((const char*)desc_text));
        item->set_element(5, vint_variant((const char*)progress_text));
        item->set_element(6, vint_variant(achieved ? 1.0f : progress));
        item->set_element(7, vint_variant(selected));
    }
}

// 0x0089F390 (:794)
void ui_stats_process_data_items()
{
    ui_stats_process_tab_data();
    ui_stats_process_toggle_data();
    ui_stats_process_hint_data();
    if (Current_page == SSP_STATS_PAGE)
    {
        ui_stats_process_stats();
    }
    else if (Current_page == SSP_ACHIEVEMENTS)
    {
        ui_stats_process_achievements();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    ui_stats_process_header_data();
}

// 0x008C9CF0 (:815)
static bool stats_check_page_tab_click()
{
    vint_document* doc = ui_get_vint_document(Doc_handle_uistats);
    if (doc == nullptr)
    {
        return false;
    }
    for (int page = SSP_STATS_PAGE; page < SSP_NUM_PAGES; page++)
    {
        vint_object_base* obj = vint_object_base::find_object(kPageHeaders[page], doc);
        if (ui_is_mouse_over_vint_object(obj))
        {
            static const unsigned int crc_alpha = crc_str("alpha", 0); // 0x02CA14C0
            if (obj != nullptr)
            {
                obj->set_property_value(crc_alpha, vint_variant(1.0f), 0);
            }
            if (mouse_just_released(0))
            {
                audiolib_cue_play(Audio_info[9], nullptr, nullptr);
                if (Current_page != page)
                {
                    ui_stats_set_up_new_page((stat_screen_pages)page);
                }
                return true;
            }
        }
        else if (Current_page != page)
        {
            ui_pc_set_object_opacity(obj, 0.5f);
        }
    }
    return false;
}

// 0x008C9E10 (:864)
static bool stats_check_hints()
{
    if (!ui_mouse_over_hint_combo(kHintImage, kHintText, Doc_handle_uistats, true))
    {
        return false;
    }
    Hint_clicked = 0;
    return true;
}

// 0x008C9E40 (:879)
static void stats_process_scroll_bar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0) && !mouse_just_scrolled_wheel())
    {
        return;
    }
    const int count = Current_gameplay_category == Weapon_category ? Num_stats_displayed : Num_stats_to_display;
    if (count <= Max_achievement_rows)
    {
        return;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_uistats);
    if (doc == nullptr)
    {
        return;
    }
    vint_object_base* obj = vint_object_base::find_object(kScrollBarOutline, doc);
    if (obj == nullptr || !ui_vint_object_visible(obj) || !mouse_is_pressed(0) || !ui_is_mouse_over_vint_object(obj))
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float h = 0.0f;
    ui_get_vint_obj_position(obj, x, y);
    ui_get_vint_obj_size(obj, x, h);
    const int first = (int)floor((double)(((Mouse_scaled_y - y) / h) * (float)(count - Max_achievement_rows) + 0.5f));
    if (first != Display_start_index_uistats)
    {
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
        Display_start_index_uistats = first;
    }
}

// 0x008CA020 (:932)
static bool stats_process_achievement_row_click()
{
    if (!mouse_just_released(0))
    {
        return false;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_uistats);
    if (ui_is_mouse_over_vint_object(vint_object_base::find_object("row_main", doc)))
    {
        Selected_row_uistats = 0;
        return true;
    }
    for (int row = 1; row < Max_achievement_rows; row++)
    {
        char name[64];
        keen::formatString(name, 0x40, "%s%d", "row_group", row);
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, doc)))
        {
            Selected_row_uistats = row;
            return true;
        }
    }
    return false;
}

// 0x008CA130 (:958)
static bool stats_process_stats_arrow_click()
{
    if (!mouse_just_released(0))
    {
        return false;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_uistats);
    if (ui_is_mouse_over_vint_object(vint_object_base::find_object("toggle_arrow_left", doc)))
    {
        Left_arrow_clicked_uistats = true;
        return true;
    }
    if (ui_is_mouse_over_vint_object(vint_object_base::find_object("toggle_arrow_right", doc)))
    {
        Right_arrow_clicked_uistats = true;
    }
    return Left_arrow_clicked_uistats || Right_arrow_clicked_uistats;
}

// 0x008CA1C0 (:972)
void ui_stats_process_mouse()
{
    Hint_clicked = -1;
    Right_arrow_clicked_uistats = false;
    Left_arrow_clicked_uistats = false;
    if (stats_check_page_tab_click() || stats_check_hints())
    {
        return;
    }
    if (Current_page == SSP_STATS_PAGE)
    {
        stats_process_stats_arrow_click();
    }
    else if (Current_page == SSP_ACHIEVEMENTS)
    {
        stats_process_scroll_bar();
        stats_process_achievement_row_click();
    }
}

// The row count of a category. rfg.exe inlines this twice.
static void ui_stats_set_category_rows()
{
    switch (Current_gameplay_category)
    {
    case 0:
        Display_start_index_uistats = 0;
        Num_stats_to_display = Num_destruction_stats;
        break;
    case 1:
        Display_start_index_uistats = 0;
        Num_stats_to_display = Num_death_stats;
        break;
    case Weapon_category:
        Display_start_index_uistats = 0;
        Num_stats_to_display = Num_weapon_stats;
        break;
    case 3:
        Display_start_index_uistats = 0;
        Num_stats_to_display = Num_time_stats;
        break;
    default:
        break;
    }
}

// 0x008CA230 (:1006)
void ui_stats_process_input()
{
    ui_stats_process_mouse();
    const int visible_rows = (int)Current_page > 0 ? Max_achievement_rows : Max_stat_rows;
    // Magic numbers: ui button 11 goes back; 5 and 1 go up; 6 and 2 down; 4 and 3 change the category; 17 and 19 the page.
    if (ui_get_button_info(11, Input_id_uistats) > 0.0f || Hint_clicked == 0)
    {
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        Exit_uistats = true;
        return;
    }
    if (ui_get_button_info(5, Input_id_uistats) > 0.0f || ui_get_button_info(1, Input_id_uistats) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
    {
        if ((int)Current_page < 1)
        {
            const int count = Current_gameplay_category == Weapon_category ? Num_stats_displayed : Num_stats_to_display;
            if (Display_start_index_uistats < 1)
            {
                if (visible_rows < count)
                {
                    Display_start_index_uistats = count - visible_rows;
                    return;
                }
                audiolib_cue_play(Audio_info[11], nullptr, nullptr);
                return;
            }
        }
        else
        {
            if (Selected_row_uistats > 0)
            {
                audiolib_cue_play(Audio_info[1], nullptr, nullptr);
                Selected_row_uistats--;
                return;
            }
            if (Display_start_index_uistats < 1)
            {
                if (Num_stats_to_display <= 0)
                {
                    audiolib_cue_play(Audio_info[11], nullptr, nullptr);
                    return;
                }
                audiolib_cue_play(Audio_info[1], nullptr, nullptr);
                if (Num_stats_to_display <= visible_rows)
                {
                    Selected_row_uistats = Num_stats_to_display - 1;
                    return;
                }
                Selected_row_uistats = visible_rows - 1;
                Display_start_index_uistats = Num_stats_to_display - visible_rows;
                return;
            }
        }
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
        Display_start_index_uistats--;
        return;
    }
    if (ui_get_button_info(6, Input_id_uistats) > 0.0f || ui_get_button_info(2, Input_id_uistats) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f)
    {
        if ((int)Current_page < 1)
        {
            const int count = Current_gameplay_category == Weapon_category ? Num_stats_displayed : Num_stats_to_display;
            if (Display_start_index_uistats + visible_rows < count)
            {
                audiolib_cue_play(Audio_info[0], nullptr, nullptr);
                Display_start_index_uistats++;
                return;
            }
            if (visible_rows < Num_stats_to_display)
            {
                Display_start_index_uistats = 0;
                return;
            }
        }
        else
        {
            if (Selected_row_uistats < Max_achievement_rows - 1)
            {
                audiolib_cue_play(Audio_info[0], nullptr, nullptr);
                Selected_row_uistats++;
                return;
            }
            if (Display_start_index_uistats + visible_rows < Num_stats_to_display)
            {
                audiolib_cue_play(Audio_info[0], nullptr, nullptr);
                Display_start_index_uistats++;
                return;
            }
            if (Num_stats_to_display > 0)
            {
                audiolib_cue_play(Audio_info[0], nullptr, nullptr);
                if (visible_rows < Num_stats_to_display)
                {
                    Selected_row_uistats = 0;
                    Display_start_index_uistats = 0;
                    return;
                }
                Selected_row_uistats = 0;
                return;
            }
        }
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        return;
    }
    const stat_screen_pages page = Current_page;
    if ((ui_get_button_info(4, Input_id_uistats) > 0.0f || Right_arrow_clicked_uistats) && page == SSP_STATS_PAGE)
    {
        Current_gameplay_category = Current_gameplay_category < 3 ? Current_gameplay_category + 1 : 0;
        audiolib_cue_play(Audio_info[2], nullptr, nullptr);
        ui_stats_set_category_rows();
        return;
    }
    if ((ui_get_button_info(3, Input_id_uistats) > 0.0f || Left_arrow_clicked_uistats) && page == SSP_STATS_PAGE)
    {
        Current_gameplay_category = Current_gameplay_category < 1 ? 3 : Current_gameplay_category - 1;
        audiolib_cue_play(Audio_info[3], nullptr, nullptr);
        ui_stats_set_category_rows();
        return;
    }
    if (ui_get_button_info(17, Input_id_uistats) > 0.0f)
    {
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
        ui_stats_set_up_new_page((int)page < 1 ? SSP_ACHIEVEMENTS : (stat_screen_pages)((int)Current_page - 1));
        return;
    }
    if (ui_get_button_info(19, Input_id_uistats) > 0.0f)
    {
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        ui_stats_set_up_new_page((int)page > 0 ? SSP_STATS_PAGE : (stat_screen_pages)((int)Current_page + 1));
    }
}

// 0x009170C0 (:1175)
void ui_stats_init()
{
    state_info info;
    info.enter = ui_stats_enter;
    info.exit = ui_stats_exit;
    info.process = ui_stats_process;
    info.handshake = ui_stats_handshake;
    info.render = ui_stats_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_STATS_SCREEN, info);
}

// 0x00908500 (:1188)
void ui_stats_enter(game_state prev_state)
{
    (void)prev_state;
    Control_id_uistats = 0;
    Input_id_uistats = -1;
    Exit_uistats = false;
    Loaded_uistats = false;
    Doc_handle_uistats = ui_load_document("rfg_ui_stat_screen", DFIT_NONE, 0.0f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_uistats == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Input_id_uistats = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Control_id_uistats = ui_request_control_id();
    ui_stats_set_up_new_page(SSP_STATS_PAGE);
    ui_wait_for_button_ups();
    if (Active)
    {
        ingame_options_show_menu(false);
    }
    else
    {
        main_menu_show_menu(false);
    }
}

// 0x008EF610 (:1225)
void ui_stats_exit(game_state prev_state)
{
    (void)prev_state;
    ui_unload_document(Doc_handle_uistats, DFOT_NONE, 0.5f);
    Doc_handle_uistats = -1;
    ui_relinquish_control_id(Control_id_uistats);
    ui_relinquish_input_id(Input_id_uistats);
    Exit_uistats = false;
    Loaded_uistats = false;
    Num_stats_to_display = 0;
    Display_start_index_uistats = 0;
    Selected_row_uistats = 0;
    Current_page = SSP_NUM_PAGES;
    Current_gameplay_category = 4; // Magic number: no category.
    if (Active)
    {
        ingame_options_show_menu(true);
    }
    else
    {
        main_menu_show_menu(true);
    }
}

// 0x0089F3E0 (:1252)
void ui_stats_handshake(rl_renderer*)
{
}

// 0x009124D0 (:1256)
void ui_stats_process()
{
    if (Exit_uistats)
    {
        gameseq_pop_state();
    }
    else if (Loaded_uistats)
    {
        ui_stats_process_input();
        ui_stats_process_data_items();
        ui_add_for_processing(Doc_handle_uistats);
    }
    else
    {
        switch (ui_get_doc_asset_state(Doc_handle_uistats))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            ui_unload_document(Doc_handle_uistats, DFOT_NONE, 0.5f);
            Doc_handle_uistats = -1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Loaded_uistats = true;
            ui_stats_create_data_items();
            ui_stats_process_data_items();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    ui_process(Control_id_uistats, false);
}

// 0x008F6B20 (:1298)
void ui_stats_render(rl_renderer*)
{
    if (Loaded_uistats)
    {
        ui_add_for_rendering(Doc_handle_uistats);
    }
    ui_render(Control_id_uistats, false);
}

// 0x0089F3F0 (:1307)
void ui_stats_show()
{
    gameseq_push_state(GS_STATS_SCREEN, true, false);
}
