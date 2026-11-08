// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.cpp
//
// The lifetime statistics (GS_MULTIPLAYER_STATS, UI.md UI5) on rfg_ui_mp_stats: pages of rows (mp_stats_page_layout.xtbl,
// rows sorted by their sort name, bagman rows only with the DLC) of four cells each (mp_stats_page_elements.xtbl: a
// tracked stat, a ratio of two, a hidden challenge's requirement or progress), 16 rows a screen; paging past the last
// page goes on to the experience tally. Reached from the system link (and live) menus' statistics entry.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
struct mp_stats_cell;

// size 0x18 (rfg.pdb; named only through mp_stats_page)
struct mp_stats_row
{
    char* sorting_name; // 0x00
    char* display_name; // 0x04
    mp_stats_cell* column[4]; // 0x08
};
static_assert(sizeof(mp_stats_row) == 0x18, "mp_stats_row");

// mp_menu_exit_actions_stats, mp_menu_states_stats and mp_match_stats_shell_info are in ui_document_mp_stats.h since
// UI5 (the experience tally uses them).

enum mp_stats_cell_derived_display_type
{
    MPSCDDT_FLOATING = 0,
    MPSCDDT_PERCENT = 1,
};

enum mp_stats_cell_types
{
    MPSCT_EMPTY = 0,
    MPSCT_DIRECT = 1,
    MPSCT_DERIVED_RATIO = 2,
    MPSCT_HIDDEN_CHALLENGE_REQUIREMENT = 3,
    MPSCT_HIDDEN_CHALLENGE_PROGRESS = 4,
    NUM_MP_STATS_CELL_TYPES = 5,
};

// size 0x1C
struct mp_stats_cell
{
    // size 0x4
    struct cell_type_direct
    {
        tracked_stats_indices stat_id; // 0x00
    };

    // size 0x14
    struct cell_type_derived
    {
        tracked_stats_indices numerator; // 0x00
        float numerator_multiplier; // 0x04
        tracked_stats_indices denominator; // 0x08
        float denominator_multiplier; // 0x0C
        mp_stats_cell_derived_display_type display_type; // 0x10
    };

    // size 0x4
    struct cell_type_hidden_challenge
    {
        const hidden_challenge* challenge_p; // 0x00
    };

    // size 0x14
    union cell_data
    {
        cell_type_direct direct; // 0x00
        cell_type_derived derived; // 0x00
        cell_type_hidden_challenge challenge; // 0x00
    };

    mp_stats_cell_types cells_type; // 0x00
    cell_data data; // 0x04
    char* name; // 0x18
};
static_assert(sizeof(mp_stats_cell) == 0x1C, "mp_stats_cell");

// size 0x24
struct mp_stats_page
{
    char* name; // 0x00
    char* display_name; // 0x04
    char* stat_column_name; // 0x08
    char* column_names[4]; // 0x0C
    mp_stats_row* rows; // 0x1C
    int num_rows; // 0x20
};
static_assert(sizeof(mp_stats_page) == 0x24, "mp_stats_page");

// ---- file-static globals
static const char* Ui_mp_shell_name_stats = {"rfg_ui_mp_stats"}; // 0x01663C38
static const char* Menu_data_item_names_stats[7] = {"StatsHighlightIndex", "StatsHintInfo", "StatsTitles", "StatsHeaderInfo", "StatsModeInfo", "StatsScoreInfo", "StatsSelectionProps"}; // 0x01663C3C
static const char* Mode_colon_string_stats = {"MENU_MODE_COLON"}; // 0x01663C58
static const char* Map_colon_string_stats = {"MENU_MAP_COLON"}; // 0x01663C5C
static const char* Back_string_stats = {"MENU_BACK"}; // 0x01663C60
static const char* Cycle_pages_string_stats = {"MENU_CYCLE_PAGES"}; // 0x01663C64
static const char* Cycle_leaderboard_string_stats = {"MENU_CYCLE_STATS"}; // 0x01663C68
static const char* Page_of_page_string = {"MENU_PAGE_OF_PAGE"}; // 0x01663C70
static int Msgbox_handle_stats = -1; // 0x01663C74
static mp_stats_hint_choices Current_mouse_hint_choice_stats = MPS_HINT_INVALID; // 0x01663C78, initial data ffffffff
mp_match_stats_shell_info Mp_match_stats_info; // 0x01666A20 (shared with the experience tally, UI5)
// int mp_stats_hint_config[7] (0x02BF214C) is used here too; placed in ui_message_box.cpp.
static int Match_stats_selection; // 0x02C03678
static mp_stats_cell* Cells; // 0x02C0367C
static int Num_cells; // 0x02C03680
static mp_stats_page* Pages; // 0x02C03684
static int Num_pages; // 0x02C03688
static int Current_page_index; // 0x02C0368C
static int Current_row_offset; // 0x02C03690
static int mp_stats_num_hints; // 0x02C03694
// The shell's object names (header constants in rfg.exe, this file's copies).
static const char* kHintGroup5 = {"hint_5_group"}; // 0x01668678
static const char* kHintGroup4 = {"hint_4_group"}; // 0x0166867C
static const char* kHintGroup2 = {"hint_2_group"}; // 0x01668680
static const char* kHintGroup1 = {"hint_1_group"}; // 0x01668684
static const char* kHintGroup3 = {"hint_3_group"}; // 0x01668688
static const char* kGroup_name = {"row_group"}; // 0x01668674

// ---- internal functions

// A pool copy of a string (the inlined strlen + alloc + strcpy).
static char* stats_pool_string(mempool_base& pool, const char* text)
{
    char* copy = (char*)pool.alloc((unsigned int)strlen(text) + 1, 0);
    strcpy(copy, text);
    return copy;
}

// 0x0088D950
static int next_page(int current_page)
{
    int page = current_page + 1;
    if (page >= Num_pages)
    {
        page = 0;
    }
    return page;
}

// 0x0088D960
static int prev_page(int current_page)
{
    int page = current_page - 1;
    if (page < 0)
    {
        page = Num_pages - 1;
    }
    return page;
}

// 0x0088D970
// Magic number: 16 rows a screen.
static int next_offset(int current_offset)
{
    int offset = current_offset + 16;
    if (offset >= Pages[Current_page_index].num_rows)
    {
        offset = 0;
    }
    return offset;
}

// 0x0088D990
static int prev_offset(int current_offset)
{
    int offset;
    if (current_offset < 1)
    {
        // The last screen's first row.
        offset = ((Pages[Current_page_index].num_rows - 1) / 16) * 16;
    }
    else
    {
        offset = current_offset - 16;
    }
    if (offset < 0)
    {
        offset = 0;
    }
    return offset;
}

// 0x0088D9C0
// mp_stats_page_elements.xtbl: each element's name and type (simple stat, derived ratio, challenge progress or
// requirement, empty). Original: an element of an unknown type leaves its cell unfilled and the next element takes it.
static void parse_cells(mempool_base& pool)
{
    xml_element* root = xml_table_parse("mp_stats_page_elements.xtbl");
    Num_cells = xml_table_count(root, "element");
    Cells = (mp_stats_cell*)pool.alloc((unsigned int)Num_cells * sizeof(mp_stats_cell), 0);
    int index = 0;
    for (xml_element* element = xml_find(root, "element"); element != nullptr; element = xml_find_next(root, element, "element"))
    {
        mp_stats_cell& cell = Cells[index];
        cell.name = stats_pool_string(pool, xml_table_get_req_string_ref(element, "Name"));
        xml_element* type = xml_table_find_req(element, "Type");
        xml_element* simple = xml_table_find_opt(type, "Simple_Stat");
        xml_element* ratio = xml_table_find_opt(type, "Derived_Ratio");
        xml_element* progress = xml_table_find_opt(type, "Challenge_Progress");
        xml_element* requirement = xml_table_find_opt(type, "Challenge_Requirement");
        xml_element* empty = xml_table_find_opt(type, "Empty");
        if (simple != nullptr)
        {
            cell.cells_type = MPSCT_DIRECT;
            cell.data.direct.stat_id = stats_get_by_name(xml_table_get_req_string_ref(simple, "Stat"));
        }
        else if (ratio != nullptr)
        {
            cell.cells_type = MPSCT_DERIVED_RATIO;
            cell.data.derived.numerator = stats_get_by_name(xml_table_get_req_string_ref(ratio, "Numerator"));
            xml_table_get_req_float(cell.data.derived.numerator_multiplier, ratio, "Numerator_Multiplier");
            cell.data.derived.denominator = stats_get_by_name(xml_table_get_req_string_ref(ratio, "Denominator"));
            xml_table_get_req_float(cell.data.derived.denominator_multiplier, ratio, "Denominator_Multiplier");
            const char* display = xml_table_get_req_string_ref(ratio, "Display_As");
            if (v_stricmp(display, "Floating") == 0)
            {
                cell.data.derived.display_type = MPSCDDT_FLOATING;
            }
            else if (v_stricmp(display, "Percentage") == 0)
            {
                cell.data.derived.display_type = MPSCDDT_PERCENT;
            }
            else
            {
                // Original: an unknown display type leaves the cell to the next element (the index does not advance).
                continue;
            }
        }
        else if (progress != nullptr)
        {
            cell.cells_type = MPSCT_HIDDEN_CHALLENGE_PROGRESS;
            cell.data.challenge.challenge_p = stats_get_hidden_challenge_by_display_name(xml_table_get_req_string_ref(progress, "Challenge"));
        }
        else if (requirement != nullptr)
        {
            cell.cells_type = MPSCT_HIDDEN_CHALLENGE_REQUIREMENT;
            cell.data.challenge.challenge_p = stats_get_hidden_challenge_by_display_name(xml_table_get_req_string_ref(requirement, "Challenge"));
        }
        else if (empty != nullptr)
        {
            cell.cells_type = MPSCT_EMPTY;
        }
        else
        {
            continue;
        }
        index++;
    }
    xml_table_close(root);
}

// 0x0088DC80
static mp_stats_cell* find_cell_by_name(const char* name)
{
    for (int i = 0; i < Num_cells; i++)
    {
        if (v_stricmp(Cells[i].name, name) == 0)
        {
            return &Cells[i];
        }
    }
    return nullptr;
}

// 0x0088DCE0
// mp_stats_page_layout.xtbl: the pages with their names, column titles and rows (cells by name; bagman rows without
// the DLC are dropped), each page's rows sorted by sort name (a selection sort).
static void parse_pages(mempool_base& pool)
{
    xml_element* root = xml_table_parse("mp_stats_page_layout.xtbl");
    Num_pages = xml_table_count(root, "stats_page");
    Pages = (mp_stats_page*)pool.alloc((unsigned int)Num_pages * sizeof(mp_stats_page), 0);
    int page_index = 0;
    for (xml_element* element = xml_find(root, "stats_page"); element != nullptr; element = xml_find_next(root, element, "stats_page"))
    {
        mp_stats_page& page = Pages[page_index];
        page.name = stats_pool_string(pool, xml_table_get_req_string_ref(element, "Name"));
        page.display_name = stats_pool_string(pool, xml_table_get_req_string_ref(element, "display_name"));
        page.stat_column_name = stats_pool_string(pool, xml_table_get_req_string_ref(element, "stat_column_name"));
        page.column_names[0] = stats_pool_string(pool, xml_table_get_req_string_ref(element, "column1_name"));
        page.column_names[1] = stats_pool_string(pool, xml_table_get_req_string_ref(element, "column2_name"));
        page.column_names[2] = stats_pool_string(pool, xml_table_get_req_string_ref(element, "column3_name"));
        page.column_names[3] = stats_pool_string(pool, xml_table_get_req_string_ref(element, "column4_name"));
        xml_element* rows = xml_table_find_req(element, "rows");
        page.num_rows = xml_table_count(rows, "row");
        page.rows = (mp_stats_row*)pool.alloc((unsigned int)page.num_rows * sizeof(mp_stats_row), 0);
        int row_index = 0;
        for (xml_element* row_element = xml_find(rows, "row"); row_element != nullptr; row_element = xml_find_next(rows, row_element, "row"))
        {
            mp_stats_row& row = page.rows[row_index];
            row.sorting_name = stats_pool_string(pool, xml_table_get_req_string_ref(row_element, "sort_name"));
            row.display_name = stats_pool_string(pool, xml_table_get_req_string_ref(row_element, "display_name"));
            row.column[0] = find_cell_by_name(xml_table_get_req_string_ref(row_element, "column1"));
            row.column[1] = find_cell_by_name(xml_table_get_req_string_ref(row_element, "column2"));
            row.column[2] = find_cell_by_name(xml_table_get_req_string_ref(row_element, "column3"));
            row.column[3] = find_cell_by_name(xml_table_get_req_string_ref(row_element, "column4"));
            bool bagman_locked;
            xml_table_get_req_bool(bagman_locked, row_element, "bagman_locked");
            if (!bagman_locked || Bagman_dlc_unlocked)
            {
                row_index++;
            }
            else
            {
                page.num_rows--;
            }
        }
        for (int i = 0; i < page.num_rows; i++)
        {
            int smallest = i;
            for (int j = i + 1; j < page.num_rows; j++)
            {
                if (strcmp(page.rows[j].sorting_name, page.rows[smallest].sorting_name) < 0)
                {
                    smallest = j;
                }
            }
            if (smallest != i)
            {
                const mp_stats_row swap = page.rows[smallest];
                page.rows[smallest] = page.rows[i];
                page.rows[i] = swap;
            }
        }
        page_index++;
    }
    // Original bug kept: rfg.exe selects the pages by name too, but its swap copies the page one past the last onto
    // itself, so the pages stay in the table's order (the copy past the end is not reproduced).
    xml_table_close(root);
}

// 0x0088E470
// A row with a hidden challenge requirement shows its name only once the challenge is half done (half its stats at
// the required value, or for an "any" challenge one stat at half of it); else "hidden challenge".
static bool show_row_name(mp_stats_row* rowp)
{
    int column = 0;
    while (rowp->column[column]->cells_type != MPSCT_HIDDEN_CHALLENGE_REQUIREMENT)
    {
        column++;
        if (column > 3)
        {
            return true;
        }
    }
    const hidden_challenge* challenge = rowp->column[column]->data.challenge.challenge_p;
    if (challenge->award_condition == 0)
    {
        if (challenge->num_stats < 1)
        {
            return false;
        }
        for (int i = 0; stats_get_stat_value_fl(challenge->stats[i]) * 2.0f < challenge->required_value;)
        {
            i++;
            if (i >= challenge->num_stats)
            {
                return false;
            }
        }
    }
    else
    {
        int complete = 0;
        for (int i = 0; i < challenge->num_stats; i++)
        {
            if (challenge->required_value <= stats_get_stat_value_fl(challenge->stats[i]))
            {
                complete++;
            }
        }
        // Magic number: half the stats.
        if ((float)complete * 2.0f < (float)challenge->num_stats)
        {
            return false;
        }
    }
    return true;
}

static void stats_load_global_data_items();
static void stats_unload_ui_information();

// 0x008F33B0
static void stats_do_state_change()
{
    do
    {
        Mp_match_stats_info.state = Mp_match_stats_info.pending_state;
        if (Mp_match_stats_info.pending_state == MMS_ST_LOADED)
        {
            stats_load_global_data_items();
            vint_data_group* group = vint_data_group::create_new("StatsSetData");
            if (group != nullptr)
            {
                Mp_match_stats_info.data_group_handle = group->get_handle();
                ui_add_data_group(Mp_match_stats_info.doc_handle, Mp_match_stats_info.data_group_handle);
            }
        }
        else
        {
            if (Mp_match_stats_info.pending_state != MMS_ST_UNLOADING)
            {
                return;
            }
            stats_unload_ui_information();
            if (Msgbox_handle_stats != -1)
            {
                ui_hide_msgbox(Msgbox_handle_stats, false);
                Msgbox_handle_stats = -1;
            }
            if (Mp_match_stats_info.exit_action == MMEA_ST_POP)
            {
                if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
                {
                    gameseq_pop_state();
                }
                gameseq_pop_state();
            }
            else if (Mp_match_stats_info.exit_action == MMEA_ST_PUSH)
            {
                gameseq_push_state(Mp_match_stats_info.exit_push_target, true, true);
            }
            Mp_match_stats_info.pending_state = MMS_ST_UNLOADED;
        }
    } while (Mp_match_stats_info.state != Mp_match_stats_info.pending_state);
}

// 0x0088E570
// LTCG: new_state in EAX.
static void stats_set_state(mp_menu_states_stats new_state)
{
    Mp_match_stats_info.pending_state = new_state;
}

// 0x0088E580
static void stats_init_state()
{
    Mp_match_stats_info.state = MMS_ST_UNLOADED;
    Mp_match_stats_info.pending_state = MMS_ST_UNLOADED;
    Mp_match_stats_info.prev_state = MMS_ST_UNLOADED;
    Mp_match_stats_info.exit_action = MMEA_ST_POP;
    Mp_match_stats_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AE530
static void stats_init_info()
{
    Mp_match_stats_info.group_item_handles.clear();
    for (int i = 0; i < 7; i++)
    {
        Mp_match_stats_info.data_item_handles[i] = -1;
    }
    Mp_match_stats_info.data_group_handle = -1;
}

// 0x008D43A0
// The seven data items (highlight index, hints, titles, column headers, a hidden mode/map item, a hidden score item, a
// hidden selection item).
static void stats_load_global_data_items()
{
    unsigned int handle = 0;
    for (int i = 0; i < 7; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Menu_data_item_names_stats[i]);
        if (item != nullptr)
        {
            switch (i)
            {
            case 0:
            {
                vint_variant_type types[1] = {VINT_PROP_TYPE_INT};
                item->set_template(types, 1);
                break;
            }
            case 2:
            {
                vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
                item->set_template(types, 3);
                break;
            }
            case 3:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                break;
            }
            case 4:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                item->set_element(0, vint_variant(false));
                item->set_element(1, vint_variant(Mode_colon_string_stats));
                item->set_element(2, vint_variant("MODE NAME"));
                item->set_element(3, vint_variant(Map_colon_string_stats));
                item->set_element(4, vint_variant("LEVEL NAME"));
                break;
            }
            case 5:
            {
                vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
                item->set_template(types, 3);
                item->set_element(0, vint_variant(false));
                break;
            }
            case 6:
            {
                vint_variant_type types[1] = {VINT_PROP_TYPE_BOOL};
                item->set_template(types, 1);
                item->set_element(0, vint_variant(false));
                break;
            }
            default:
                break;
            }
            handle = item->get_handle();
        }
        // Original: a failed create stores (and adds) the previous item's handle (0 for the first).
        Mp_match_stats_info.data_item_handles[i] = (int)handle;
        ui_add_data_item(Mp_match_stats_info.doc_handle, (int)handle);
    }
}

// 0x008C1BD0
// A row (the results' row layout): {name, 4 ints, 7 bools, 4 strings}.
static void stats_add_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_stats_info.data_group_handle);
    vint_data_item* item = group->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[16] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT,
                                   VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL,
                                   VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL,
                                   VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
    item->set_template(types, 16);
    Mp_match_stats_info.group_item_handles.add((int)item->get_handle());
}

// 0x008AE570
static void stats_remove_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_stats_info.data_group_handle);
    const int last = Mp_match_stats_info.group_item_handles.size() - 1;
    group->remove_data_item((unsigned int)Mp_match_stats_info.group_item_handles[last]);
    Mp_match_stats_info.group_item_handles.remove(last);
}

// 0x008D46E0
static void stats_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("StatsSetData");
    if (group != nullptr)
    {
        Mp_match_stats_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_match_stats_info.doc_handle, Mp_match_stats_info.data_group_handle);
    }
}

// 0x008D4710
static void stats_load_ui_information()
{
    stats_load_global_data_items();
    stats_load_data_groups();
}

// 0x008E4DD0
// LTCG: the document handle in EDI.
static void stats_unload_ui_information()
{
    vint_doc_info* info = ui_get_doc_info(Mp_match_stats_info.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    stats_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008E4E70
// Back, the page cycling hints and, with more than 16 rows, "page x/y" of the rows.
static void stats_process_data_item_hint_text(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_stats_info.data_item_handles[1]);
    if (vdi == nullptr)
    {
        return;
    }
    for (int i = 1; i < 7; i++)
    {
        mp_stats_hint_config[i] = -1;
    }
    Num_hints_added = 0;
    mp_stats_hint_config[0] = MPS_HINT_ABORT;
    mp_stats_num_hints = 1;
    Hint_data_item = vdi;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_stats, nullptr), CBA_MENU_ABORT);
    mp_stats_hint_config[mp_stats_num_hints++] = MPS_HINT_CYCLE_LEADERBOARD_LEFT;
    multi_player_menu_system_add_hint(localization_get_display_string_req("", nullptr), CBA_MENU_L2);
    mp_stats_hint_config[mp_stats_num_hints++] = MPS_HINT_CYCLE_LEADERBOARD_RIGHT;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Cycle_leaderboard_string_stats, nullptr), CBA_MENU_R2);
    // Magic number: 16 rows a screen.
    if (gameseq_get_state() == GS_MULTIPLAYER_STATS && Pages[Current_page_index].num_rows > 16)
    {
        const wchar_t* page_of_page = localization_get_display_string_req(Page_of_page_string, nullptr);
        wchar_t format[128];
        wchar_t text[128];
        // "<cycle pages> (<page x of y>)", the page counts filled in after.
        _snprintf_safe(format, 0x80, "%ls (%ls)", L"%ls", page_of_page != nullptr ? page_of_page : L"%d/%d");
        sprintf(text, format, localization_get_display_string_req(Cycle_pages_string_stats, nullptr), Current_row_offset / 16 + 1, (Pages[Current_page_index].num_rows - 1) / 16 + 1);
        mp_stats_hint_config[mp_stats_num_hints++] = MPS_HINT_CYCLE_PAGE_LEFT;
        multi_player_menu_system_add_hint(localization_get_display_string_req("", nullptr), CBA_MENU_LEFT);
        mp_stats_hint_config[mp_stats_num_hints++] = MPS_HINT_CYCLE_PAGE_RIGHT;
        multi_player_menu_system_add_hint(text, CBA_MENU_RIGHT);
    }
    multi_player_menu_system_end_hints();
}

// 0x008E5100
// The page's title (the tally's title under the tally), and "1/1" under the tally.
static void stats_process_data_item_titles(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_stats_info.data_item_handles[2]);
    if (vdi == nullptr)
    {
        return;
    }
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        vdi->set_element(0, vint_variant("MENU_STATS_PAGE_1_TITLE"));
        vdi->set_element(1, vint_variant(false));
    }
    else
    {
        vdi->set_element(0, vint_variant((const char*)Pages[Current_page_index].display_name));
        vdi->set_element(1, vint_variant(true));
    }
    const wchar_t* format = localization_get_display_string_req(Page_of_page_string, nullptr);
    if (format == nullptr)
    {
        format = L"%d/%d";
    }
    wchar_t text[128];
    memset(text, 0, sizeof(text));
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        sprintf(text, format, 1, 1);
    }
    char encoded[384];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(2, vint_variant((const char*)encoded));
}

// 0x0088E5B0
static void stats_process_data_item_info_text(float dtime)
{
    vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_match_stats_info.data_item_handles[3]);
    if (vdi == nullptr)
    {
        return;
    }
    vdi->set_element(0, vint_variant((const char*)Pages[Current_page_index].stat_column_name));
    for (int i = 0; i < 4; i++)
    {
        vdi->set_element(i + 1, vint_variant((const char*)Pages[Current_page_index].column_names[i]));
    }
}

// A tracked stat's value as text ("%d", or "%.2f" for the floating types); false for an unknown type.
static bool stats_format_stat(char* text, const tracked_stat* stat)
{
    switch (stat->value_type)
    {
    case TRACKED_STAT_INT8:
        sprintf(text, "%d", (int)stat->value.integer8);
        return true;
    case TRACKED_STAT_INT16:
        sprintf(text, "%d", (int)stat->value.integer16);
        return true;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        sprintf(text, "%d", stat->value.integer);
        return true;
    case TRACKED_STAT_FLOAT:
    case TRACKED_STAT_DISTANCE:
    case TRACKED_STAT_TIME:
    case TRACKED_STAT_MONEY:
    case TRACKED_STAT_PERCENT:
        sprintf(text, "%.2f", (double)stat->value.floating);
        return true;
    default:
        return false;
    }
}

// A tracked stat's value as a float (0 for an unknown type).
static float stats_stat_as_float(const tracked_stat* stat)
{
    switch (stat->value_type)
    {
    case TRACKED_STAT_INT8:
        return (float)(int)stat->value.integer8;
    case TRACKED_STAT_INT16:
        return (float)(int)stat->value.integer16;
    case TRACKED_STAT_INT:
    case TRACKED_STAT_PERCENT_COMPLETE:
    case TRACKED_STAT_MISSIONS_COMPLETED:
        return (float)stat->value.integer;
    case TRACKED_STAT_FLOAT:
    case TRACKED_STAT_DISTANCE:
    case TRACKED_STAT_TIME:
    case TRACKED_STAT_MONEY:
    case TRACKED_STAT_PERCENT:
        return stat->value.floating;
    default:
        return 0.0f;
    }
}

// 0x008C1CD0
// The screen's rows (16 at most from the row offset): the row's name (or "hidden challenge") and its four cells.
static void stats_process_data_group_menu_options(float dtime)
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_match_stats_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int existing = (int)group->get_num_data_items();
    int rows = Pages[Current_page_index].num_rows - Current_row_offset;
    // Magic number: 16 rows a screen.
    if (rows > 16)
    {
        rows = 16;
    }
    for (int i = existing; i < rows; i++)
    {
        stats_add_menu_option_data_item();
    }
    const int total_rows = existing < rows ? rows : existing;
    char text[68];
    for (int row = 0; row < rows; row++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_stats_info.group_item_handles[row]);
        if (item == nullptr)
        {
            continue;
        }
        mp_stats_row& stats_row = Pages[Current_page_index].rows[Current_row_offset + row];
        item->set_element(0, vint_variant(show_row_name(&stats_row) ? (const char*)stats_row.display_name : "STATS_HIDDEN_CHALLENGE"));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant(-1));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(-1));
        item->set_element(5, vint_variant(false));
        item->set_element(6, vint_variant(false));
        item->set_element(7, vint_variant(false));
        item->set_element(8, vint_variant(true));
        item->set_element(9, vint_variant(row == Match_stats_selection));
        item->set_element(10, vint_variant(false));
        item->set_element(11, vint_variant(false));
        for (int column = 0; column < 4; column++)
        {
            const mp_stats_cell* cell = stats_row.column[column];
            bool filled = false;
            switch (cell->cells_type)
            {
            case MPSCT_DIRECT:
            {
                const tracked_stat* stat = nullptr;
                stats_get_stat(stat, cell->data.direct.stat_id);
                filled = stat != nullptr && stats_format_stat(text, stat);
                break;
            }
            case MPSCT_DERIVED_RATIO:
            {
                const tracked_stat* denominator_stat = nullptr;
                stats_get_stat(denominator_stat, cell->data.derived.denominator);
                float denominator = denominator_stat != nullptr ? stats_stat_as_float(denominator_stat) : 0.0f;
                denominator = cell->data.derived.denominator_multiplier * denominator;
                const tracked_stat* numerator_stat = nullptr;
                stats_get_stat(numerator_stat, cell->data.derived.numerator);
                const float numerator = numerator_stat != nullptr ? stats_stat_as_float(numerator_stat) : 0.0f;
                float ratio = 0.0f;
                if (denominator > 0.0f)
                {
                    ratio = (cell->data.derived.numerator_multiplier * numerator) / denominator;
                }
                if (cell->data.derived.display_type == MPSCDDT_FLOATING)
                {
                    sprintf(text, "%.2f", (double)ratio);
                    filled = true;
                }
                else if (cell->data.derived.display_type == MPSCDDT_PERCENT)
                {
                    sprintf(text, "%.0f", (double)ratio * 100.0);
                    filled = true;
                }
                break;
            }
            case MPSCT_HIDDEN_CHALLENGE_REQUIREMENT:
            {
                const hidden_challenge* challenge = cell->data.challenge.challenge_p;
                if (challenge->award_condition == 0)
                {
                    sprintf(text, "%.0f", (double)challenge->required_value);
                    filled = true;
                }
                else if (challenge->award_condition == 1)
                {
                    sprintf(text, "%d", challenge->num_stats);
                    filled = true;
                }
                break;
            }
            case MPSCT_HIDDEN_CHALLENGE_PROGRESS:
            {
                const hidden_challenge* challenge = cell->data.challenge.challenge_p;
                if (challenge->display_type == HCDT_NUM_COMPLETE)
                {
                    sprintf(text, "%d", stats_get_hidden_challenge_num_complete_stats(challenge));
                    filled = true;
                }
                else if (challenge->display_type == HCDT_MOST_COMPLETE)
                {
                    const tracked_stat* stat = stats_get_hidden_challenge_most_complete_stat(challenge);
                    filled = stat != nullptr && stats_format_stat(text, stat);
                }
                break;
            }
            default:
                break;
            }
            if (!filled)
            {
                text[0] = '\0';
            }
            item->set_element(column + 12, vint_variant((const char*)text));
        }
    }
    for (int i = rows; i < total_rows; i++)
    {
        stats_remove_menu_option_data_item();
    }
}

// 0x008E52C0
static void stats_process_data_items()
{
    stats_process_data_item_hint_text(0.0f);
    stats_process_data_item_titles(0.0f);
    stats_process_data_item_info_text(0.0f);
    stats_process_data_group_menu_options(0.0f);
}

// 0x008F3480
static void stats_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_match_stats_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_match_stats_info.pending_state = MMS_ST_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Mp_match_stats_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_match_stats_info.doc_handle = -1;
        Mp_match_stats_info.pending_state = MMS_ST_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008D4740 (:1321) and 0x008D4960 (:1381, identical)
// The hints under the mouse (1-3: back and the stat cycling, 4-5: the page cycling); a click on a row selects it.
static mp_stats_hint_choices stats_mouse(int input_id)
{
    const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
    if (input_id != top)
    {
        return MPS_HINT_INVALID;
    }
    const char* first[3] = {kHintGroup1, kHintGroup2, kHintGroup3};
    const char* second[2] = {kHintGroup4, kHintGroup5};
    const int hint = ui_mouse_process_hints(first, 3, Mp_match_stats_info.doc_handle, true);
    const int page_hint = ui_mouse_process_hints(second, 2, Mp_match_stats_info.doc_handle, true);
    if (hint != -1)
    {
        return (mp_stats_hint_choices)mp_stats_hint_config[hint];
    }
    if (page_hint != -1)
    {
        return (mp_stats_hint_choices)mp_stats_hint_config[page_hint + 3];
    }
    if (!mouse_just_released(0) || (Utility_doc_handle != -1 && ui_is_msgbox_active(-2)))
    {
        return MPS_HINT_INVALID;
    }
    vint_document* doc = ui_get_vint_document(Mp_match_stats_info.doc_handle);
    if (doc == nullptr)
    {
        return MPS_HINT_INVALID;
    }
    char name[128];
    for (int i = 0; i < Mp_match_stats_info.group_item_handles.size(); i++)
    {
        if (i == 0)
        {
            keen::formatString(name, sizeof(name), "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, sizeof(name), "%s%d", kGroup_name, i + 1);
        }
        if (ui_is_mouse_over_vint_object(name, doc))
        {
            multi_player_menu_system_set_menu_selection(i, Match_stats_selection, Mp_match_stats_info.group_item_handles.size());
            return MPS_HINT_INVALID;
        }
    }
    return MPS_HINT_INVALID;
}

// 0x008D4740 (:1321)
mp_stats_hint_choices stats_internal_process_mouse(int input_id)
{
    return stats_mouse(input_id);
}

// 0x008D4940
static void stats_process_mouse()
{
    Current_mouse_hint_choice_stats = stats_internal_process_mouse(Mp_match_stats_info.input_id);
}

// 0x008D4960 (:1381)
mp_stats_hint_choices mp_stats_external_process_mouse(int input_id)
{
    return stats_mouse(input_id);
}

// A Button_info button pressed (and not waiting for its release) with the statistics' input focus.
static bool stats_button_pressed(int button)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    bool focus = Mp_match_stats_info.input_id == -1 && lock == 0;
    if (!focus)
    {
        const int top = Current_input_lock_id != -1 ? Current_input_lock_id : (Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id);
        focus = Mp_match_stats_info.input_id == top;
    }
    return focus && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x008E52E0
// Buttons (Button_info indices): 1/5 and the wheel up, 2/6 and the wheel down move the selection; 0x13/0x11 the
// pages; 4/3 the row screens; 11 back (not with 10 held).
static void stats_process_loaded()
{
    stats_process_data_items();
    const bool select = stats_button_pressed(10);
    bool up = stats_button_pressed(1) || stats_button_pressed(5);
    bool down = stats_button_pressed(2) || stats_button_pressed(6);
    const bool back = stats_button_pressed(0xB);
    bool screen_right = stats_button_pressed(4);
    bool screen_left = stats_button_pressed(3);
    bool page_right = stats_button_pressed(0x13);
    bool page_left = stats_button_pressed(0x11);
    Current_mouse_hint_choice_stats = stats_internal_process_mouse(Mp_match_stats_info.input_id);
    page_right = page_right || Current_mouse_hint_choice_stats == MPS_HINT_CYCLE_LEADERBOARD_RIGHT;
    page_left = page_left || Current_mouse_hint_choice_stats == MPS_HINT_CYCLE_LEADERBOARD_LEFT;
    screen_right = screen_right || Current_mouse_hint_choice_stats == MPS_HINT_CYCLE_PAGE_RIGHT;
    screen_left = screen_left || Current_mouse_hint_choice_stats == MPS_HINT_CYCLE_PAGE_LEFT;
    int dx;
    int dy;
    int wheel;
    mouse_get_deltas(dx, dy, wheel);
    up = up || wheel > 0;
    down = down || wheel < 0;
    int sound = Audio_info[11];
    bool play = true;
    if (up)
    {
        int selection = Match_stats_selection - 1;
        if (selection < 0)
        {
            selection = Mp_match_stats_info.group_item_handles.size() - 1;
        }
        if (selection != Match_stats_selection)
        {
            sound = Audio_info[1];
        }
        Match_stats_selection = selection;
    }
    else if (down)
    {
        // Original: never wraps here (the modulo below does).
        Match_stats_selection++;
        sound = Audio_info[0];
    }
    else if (page_right)
    {
        ui_document_mp_stats_next_page();
        sound = Audio_info[10];
    }
    else if (page_left)
    {
        ui_document_mp_stats_prev_page();
        sound = Audio_info[9];
    }
    else if (screen_right || screen_left)
    {
        const int offset = screen_right ? next_offset(Current_row_offset) : prev_offset(Current_row_offset);
        sound = screen_right ? Audio_info[10] : Audio_info[9];
        play = offset != Current_row_offset;
        Current_row_offset = offset;
    }
    else
    {
        play = false;
    }
    if (play)
    {
        audiolib_cue_play(sound, nullptr, nullptr);
    }
    if (Mp_match_stats_info.group_item_handles.size() < 1)
    {
        Match_stats_selection = -1;
    }
    else
    {
        Match_stats_selection = Match_stats_selection % Mp_match_stats_info.group_item_handles.size();
    }
    vint_data_item* highlight = vint_data_item::find((unsigned int)Mp_match_stats_info.data_item_handles[0]);
    if (highlight != nullptr)
    {
        highlight->set_element(0, vint_variant(Match_stats_selection));
    }
    if (!select && (back || Current_mouse_hint_choice_stats == MPS_HINT_ABORT))
    {
        ui_document_mp_stats_ui_pop();
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
}

// ---- external interface

// 0x009173A0 (:1521)
void ui_document_mp_stats_init()
{
    state_info info;
    info.enter = ui_document_mp_stats_enter;
    info.exit = ui_document_mp_stats_exit;
    info.process = ui_document_mp_stats_process;
    info.handshake = ui_document_mp_stats_handshake;
    info.render = ui_document_mp_stats_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_STATS, info);
}

// 0x0090E4D0 (:1538)
void ui_document_mp_stats_enter(game_state prev_state)
{
    stats_init_state();
    stats_init_info();
    // Magic number: a 0.5 s fade in.
    Mp_match_stats_info.doc_handle = ui_load_document(Ui_mp_shell_name_stats, DFIT_NONE, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Mp_match_stats_info.control_id = ui_request_control_id();
    Mp_match_stats_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
}

// 0x008F3520 (:1565)
void ui_document_mp_stats_exit(game_state new_state)
{
    stats_unload_ui_information();
    ui_relinquish_control_id(Mp_match_stats_info.control_id);
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Mp_match_stats_info.doc_handle, DFOT_NONE, 0.5f);
    Mp_match_stats_info.doc_handle = -1;
    ui_relinquish_input_id(Mp_match_stats_info.input_id);
    multi_game_lobby_clear_pushed_to_game();
}

// 0x009155B0 (:1584)
void ui_document_mp_stats_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Mp_match_stats_info.doc_handle);
    switch (Mp_match_stats_info.state)
    {
    case MMS_ST_LOADING:
        stats_process_loading();
        break;
    case MMS_ST_LOADED:
        stats_process_loaded();
        break;
    case MMS_ST_UNLOADED:
        stats_set_state(MMS_ST_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_match_stats_info.control_id, false);
}

// 0x008F35B0 (:1615)
void ui_document_mp_stats_handshake(rl_renderer*)
{
    Mp_match_stats_info.prev_state = Mp_match_stats_info.state;
    if (Mp_match_stats_info.pending_state != Mp_match_stats_info.state)
    {
        stats_do_state_change();
    }
}

// 0x008FC870 (:1623)
void ui_document_mp_stats_render(rl_renderer*)
{
    if (Mp_match_stats_info.doc_handle == -1 || Mp_match_stats_info.control_id == 0)
    {
        return;
    }
    if (Mp_match_stats_info.state == MMS_ST_LOADED && Mp_match_stats_info.prev_state == MMS_ST_LOADED)
    {
        ui_add_for_rendering(Mp_match_stats_info.doc_handle);
    }
    ui_render(Mp_match_stats_info.control_id, false);
}

// 0x0088E660 (:1640)
void ui_document_mp_stats_multi_init(mempool_base& pool)
{
    parse_cells(pool);
    parse_pages(pool);
}

// 0x0088E680 (:1646)
// The next page; past the last, the experience tally (from the tally, back to the first page).
void ui_document_mp_stats_next_page()
{
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        gameseq_pop_state();
        Current_page_index = 0;
        Current_row_offset = 0;
        return;
    }
    if (next_page(Current_page_index) < Current_page_index)
    {
        gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
        return;
    }
    Current_page_index = next_page(Current_page_index);
    Current_row_offset = 0;
}

// 0x0088E6F0 (:1662)
void ui_document_mp_stats_prev_page()
{
    if (gameseq_get_state() == GS_MULTIPLAYER_XP_TALLY)
    {
        gameseq_pop_state();
        Current_page_index = Num_pages - 1;
        Current_row_offset = 0;
        return;
    }
    if (Current_page_index < prev_page(Current_page_index))
    {
        gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
        return;
    }
    Current_page_index = prev_page(Current_page_index);
    Current_row_offset = 0;
}

// 0x0088E760 (:1678)
void ui_document_mp_stats_ui_pop()
{
    Mp_match_stats_info.exit_action = MMEA_ST_POP;
    Mp_match_stats_info.pending_state = MMS_ST_UNLOADING;
}
