// Original: volition/rfg/code/hud/hud_ui_handbook.cpp
//
// The guerrilla handbook screen: a tree of pages and entries from guerrilla_handbook.xtbl and the tool tips, with images,
// videos and voice lines per entry, and the save section of what is unlocked and read.
//
// The save section matches rfg.exe's format: count, Unlock_counter and Selected_page as bytes, then each node's name crc,
// viewed flag and unlock number. Records of names the table does not have are dropped on load.

#include "volition/rfg/code/hud/hud_ui_handbook.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Magic numbers: the eight pages in Handbook_page_names, the rows on the screen, the tree's depth.
static const int Num_handbook_pages = 8;
static const int Num_handbook_rows = 14;
static const int Handbook_max_depth = 5;
// Pages rfg.exe finds by position.
static const int Handbook_page_tool_tips = 5;
static const int Handbook_page_bftp = 6;
static const int Handbook_page_cinematics = 7;

// size 0x8
struct voice_info
{
    voice_line_handle current_voice_line; // 0x00
    int audio_id; // 0x04
};

// ---- file-static globals
static const char* Handbook_page_names[Num_handbook_pages] = {"MENU_CORE_GAMEPLAY", "MENU_GUERRILLA_ACTIONS", "MENU_COMBAT", "MENU_VEHICLES", "MENU_BACKGROUND", "MENU_TOOL_TIPS", "MENU_BFTP", "MENU_CINEMATICS"}; // 0x01660FF4
static const char* Handbook_page_data_names[Num_handbook_pages] = {"GameplayData", "ActionsData", "CombatData", "VehicleData", "BackgroundData", "ToolTipsData", "BftpData", "CutsceneData"}; // 0x01661014
static const char* cDocnameHandbook = "rfg_ui_handbook"; // 0x01661034
static const char* cPersona_name = "HANDBOOK"; // 0x01661038
static const char* cEmptystringHandbook = ""; // 0x0166103C
static const char* cTitle = "MENU_HANDBOOK"; // 0x01661040
static const char* cBackHandbook = "MENU_BACK"; // 0x01661048
static const char* cNew_string = "MENU_NEW"; // 0x0166104C
static const wchar_t* cTab = L"     "; // 0x01661050
static const char* cUI_bink_video = "game_video"; // 0x01661054
static int Doc_handle_handbook = -1; // 0x01661058
static int Input_id_handbook = -1; // 0x0166105C
static int Data_header_handle_handbook = -1; // 0x01661060
static int Data_hints_handle_handbook = -1; // 0x01661064
static int Data_row_group_handle = -1; // 0x01661068
static int Data_row_tab_handle = -1; // 0x0166106C
static const char* kRowGroup = "row_group"; // 0x01662228
static const char* kPageHeaders[Num_handbook_pages] = {"tab_1", "tab_2", "tab_3", "tab_4", "tab_5", "tab_6", "tab_7", "tab_8"}; // 0x0166222C
static const char* kLeftArrowGroup = "page_left_arrow_main"; // 0x0166224C
static const char* kRightArrowGroup = "page_right_arrow_main"; // 0x01662250
static const char* kHintBack = "hint_1_group"; // 0x01662254
static const char* kScrollBarOutline = "black_scrollfill"; // 0x01662258
static handbook_node Handbook_entry_tree; // 0x02B95604
static int Data_page_handles[Num_handbook_pages]; // 0x02B95860
static int Data_row_handles[Num_handbook_rows]; // 0x02B95880
static voice_info Voice_data; // 0x02B958B8
static char* Handbook_string_pool_data; // 0x02B9DCC0
static bool ExitHandbook; // 0x02B9DCC4
static bool LoadedHandbook; // 0x02B9DCC5
static bool Requested_bink; // 0x02B9DCC6
static bool Load_image_next_frame_handbook; // 0x02B9DCC7
static unsigned int Control_id_handbook; // 0x02B9DCC8
static int Selected_page; // 0x02B9DCCC
static int Display_row_start_index; // 0x02B9DCD0
static int Selected_row; // 0x02B9DCD4
static int Selected_page_display_start_time; // 0x02B9DCD8
static int Unlock_counter; // 0x02B9DCDC
static int Num_unlocked_bftp_entries; // 0x02B9DCE0
static int Num_bftp_entries; // 0x02B9DCE4
static const char* Streamed_image_name; // 0x02B9DCE8
static const char* Image_to_stream_name; // 0x02B9DCEC
static mempool_base* Bink_gpu_mempool; // 0x02B9DCF0
static mempool_base* Bink_cpu_mempool; // 0x02B9DCF4
static bool Blur_called; // 0x02B9DCF8
static bool Video_stopped; // 0x02B9DCF9
static bool Performed_exit; // 0x02B9DCFA
static bool Trigger_cutscene; // 0x02B9DCFB
static bool Playing_cutscene; // 0x02B9DCFC
static bool Push_state_handbook; // 0x02B9DCFD
static bool Mouse_double_clicked; // 0x02B9DCFE
static int time_we_moved_onto_an_entry; // 0x02B9DD00
static handbook_video_info Bink_info; // 0x02B9DD80
static object_pool<handbook_node, 176, 1> Handbook_node_pool(&handbook_node::reset, nullptr, 0); // 0x02BCE0F0
static string_pool Handbook_string_pool("Handbook String Pool"); // 0x02BD8070
static char bftp_count[6]; // 0x02BE5918

// 0x00847240 (:101)
void handbook_video_info::reset()
{
    filename = nullptr;
    line_info = nullptr;
    state = BINK_STATE_STOPPED;
    handle = 0;
    num_frames = 0;
    position.x = 0;
    position.y = 0;
    size.x = 0;
    size.y = 0;
    alpha = 1.0f;
    handbook_node = nullptr;
    curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    playing_audio_line = false;
    which_line_index = 0;
    mempool_mark = 0;
    mempool_mark_gpu = 0;
}

// 0x00847290
handbook_video_info::handbook_video_info()
{
    reset();
}

// The page at a position in the tree, or null. rfg.exe inlines this walk everywhere.
static handbook_node* hud_ui_handbook_nth_page(int index)
{
    handbook_node* page = Handbook_entry_tree.child;
    for (int i = 0; i < index && page != nullptr; i++)
    {
        page = page->next;
    }
    return page;
}

// One step of the depth-first walk rfg.exe inlines in find_node, get_child and level_init_pre.
static void hud_ui_handbook_walk_step(handbook_node** stack, int& depth)
{
    handbook_node* node = stack[depth];
    if (node->child != nullptr)
    {
        if (depth < Handbook_max_depth - 1)
        {
            stack[depth + 1] = node->child;
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        depth++;
    }
    else if (node->next != nullptr)
    {
        stack[depth] = node->next;
    }
    else
    {
        while (depth > 0 && stack[depth]->next == nullptr)
        {
            depth--;
        }
        stack[depth] = stack[depth]->next;
    }
}

// 0x00868880 (:248)
void unused_dcf_handbook_unlock_all()
{
    for (handbook_node* node = Handbook_node_pool.get_first(0); node != nullptr; node = Handbook_node_pool.get_next(node))
    {
        if (node->unlock_number < 0)
        {
            Unlock_counter++;
            node->unlock_number = Unlock_counter;
        }
    }
}

// 0x00857040 (:259)
void unused_dcf_handbook_unlock()
{
    if (Console_command != 0)
    {
        console_get_arg(2, 0); // Magic number: the string argument type, as the asm passes.
        hud_ui_handbook_unlock_entry(crc_stri(Console_arg_org, 0));
    }
}

// 0x00857070 (:268)
void unused_dcf_handbook_unlock_bftp()
{
    if (Console_command == 0)
    {
        return;
    }
    console_get_arg(8, 0); // Magic number: the integer argument type, as the asm passes.
    if (!Multiplayer && World.dlc_bundle_id == -1)
    {
        handbook_node* node = hud_ui_handbook_find_node(Console_arg_int);
        if (node != nullptr && node->unlock_number < 0)
        {
            Num_unlocked_bftp_entries++;
            Unlock_counter++;
            node->unlock_number = Unlock_counter;
            hud_ui_handbook_set_page_as_newest(node);
        }
    }
}

// 0x008472A0 (:283)
void hud_ui_handbook_add_node_to_list(handbook_node* root, handbook_node* new_node)
{
    handbook_node* last = root->child;
    if (last == nullptr)
    {
        root->child = new_node;
        return;
    }
    while (last->next != nullptr)
    {
        last = last->next;
    }
    last->next = new_node;
}

// 0x00868930 (:304)
handbook_node* hud_ui_handbook_find_dir_node(handbook_node* root, const char* dir_name)
{
    if (dir_name == nullptr || *dir_name == '\0')
    {
        return root;
    }
    char name[128];
    strncpy(name, dir_name, 0x80);
    const char* rest = nullptr;
    char* colon = strstr(name, ":");
    if (colon != nullptr)
    {
        *colon = '\0';
        rest = colon + 1;
    }
    handbook_node* node = root->child;
    while (node != nullptr && (node->flags.selectable || v_stricmp(node->name, name) != 0))
    {
        node = node->next;
    }
    if (node == nullptr)
    {
        node = Handbook_node_pool.alloc(0);
        if (node == nullptr)
        {
            return nullptr;
        }
        node->name = Handbook_string_pool.add_unique(name);
        node->flags.selectable = 0;
        node->flags.viewed = 1;
        node->unlock_number = 0;
        hud_ui_handbook_add_node_to_list(root, node);
    }
    return hud_ui_handbook_find_dir_node(node, rest);
}

// 0x008472E0 (:360)
// A bubble sort of the children, highest unlock number first.
void hud_ui_handbook_sort_children(handbook_node* root)
{
    if (root == nullptr)
    {
        return;
    }
    bool sorted;
    do
    {
        handbook_node* current = root->child;
        handbook_node* previous = nullptr;
        sorted = true;
        if (current == nullptr)
        {
            return;
        }
        do
        {
            handbook_node* next = current->next;
            handbook_node* kept = current;
            if (next != nullptr && current->unlock_number < next->unlock_number)
            {
                sorted = false;
                if (previous == nullptr)
                {
                    root->child = next;
                }
                else
                {
                    previous->next = next;
                }
                handbook_node* after = next->next;
                next->next = current;
                current->next = after;
                kept = next;
            }
            current = kept->next;
            previous = kept;
        } while (current != nullptr);
    } while (!sorted);
}

// 0x00847340 (:403)
int hud_ui_handbook_count_children(handbook_node* root, int cur_count)
{
    if (root == nullptr)
    {
        return 0;
    }
    for (handbook_node* child = root->child; child != nullptr; child = child->next)
    {
        cur_count += 1 + hud_ui_handbook_count_children(child, 0);
    }
    return cur_count;
}

// 0x00847380 (:423)
// Original bug, kept: only the direct children are tested for being unlocked; their descendants always count.
int hud_ui_handbook_count_viewable_children(handbook_node* root, int cur_count)
{
    if (root == nullptr)
    {
        return 0;
    }
    for (handbook_node* child = root->child; child != nullptr; child = child->next)
    {
        cur_count += hud_ui_handbook_count_children(child, 0);
        if (child->unlock_number >= 0)
        {
            cur_count++;
        }
    }
    return cur_count;
}

// 0x008473D0 (:445)
handbook_node* hud_ui_handbook_find_page(int page_index, bool count_invisible)
{
    int count = 0;
    for (handbook_node* page = Handbook_entry_tree.child; page != nullptr; page = page->next)
    {
        if (count_invisible || hud_ui_handbook_count_viewable_children(page, 0) > 0)
        {
            count++;
        }
        if (page_index < count)
        {
            return page;
        }
    }
    return nullptr;
}

// 0x00847450 (:466)
int hud_ui_handbook_count_viewable_pages()
{
    int count = 0;
    handbook_node* page = Handbook_entry_tree.child;
    for (int i = 0; i < Num_handbook_pages && page != nullptr; i++, page = page->next)
    {
        if (hud_ui_handbook_count_viewable_children(page, 0) > 0)
        {
            count++;
        }
    }
    return count;
}

// 0x008474D0 (:487)
void hud_ui_handbook_set_page_as_newest(handbook_node* find_me)
{
    handbook_node* page = Handbook_entry_tree.child;
    for (; page != nullptr; page = page->next)
    {
        handbook_node* child = page->child;
        while (child != nullptr && child != find_me)
        {
            child = child->next;
        }
        if (child != nullptr)
        {
            break;
        }
    }
    if (page == nullptr)
    {
        return;
    }
    if (page != hud_ui_handbook_nth_page(Handbook_page_tool_tips) && page != hud_ui_handbook_nth_page(Handbook_page_cinematics))
    {
        const int num_pages = hud_ui_handbook_count_viewable_pages();
        for (int i = 0; i < num_pages; i++)
        {
            if (hud_ui_handbook_find_page(i, false) == page)
            {
                Selected_page = i;
                break;
            }
        }
        // Magic number: 5 seconds on the screen.
        ui_add_secondary_message(localization_get_display_string_req("HM_NEW_HANDBOOK", nullptr), 5.0f, true, false);
        audiolib_cue_play(Ga_cue_ids.general_handbook_pop_up, nullptr, nullptr);
    }
    if (page == hud_ui_handbook_nth_page(Handbook_page_cinematics))
    {
        find_me->flags.viewed = 1;
    }
    hud_ui_handbook_sort_children(page);
    Display_row_start_index = 0;
    Selected_row = 0;
}

// 0x00847610 (:553)
bool hud_ui_handbook_child_node_is_unread(handbook_node* root)
{
    if (root == nullptr)
    {
        return false;
    }
    for (handbook_node* child = root->child; child != nullptr; child = child->next)
    {
        if ((child->flags.selectable && !child->flags.viewed && child->unlock_number >= 0) || hud_ui_handbook_child_node_is_unread(child))
        {
            return true;
        }
    }
    return false;
}

// 0x00847650 (:580)
handbook_node* hud_ui_handbook_get_child(handbook_node* root, int index, int& depth, bool count_locked)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    handbook_node* stack[Handbook_max_depth];
    depth = 0;
    stack[0] = root->child;
    while (depth < Handbook_max_depth)
    {
        handbook_node* node = stack[depth];
        if (node == nullptr)
        {
            break;
        }
        if (count_locked || node->unlock_number >= 0)
        {
            index--;
        }
        if (index < 0)
        {
            break;
        }
        hud_ui_handbook_walk_step(stack, depth);
    }
    return depth >= 0 && depth < Handbook_max_depth ? stack[depth] : nullptr;
}

// 0x00847740 (:636)
handbook_node* hud_ui_handbook_find_node(unsigned int name_crc)
{
    handbook_node* stack[Handbook_max_depth];
    int depth = 0;
    stack[0] = Handbook_entry_tree.child;
    while (depth < Handbook_max_depth)
    {
        handbook_node* node = stack[depth];
        if (node == nullptr || node->name_crc == name_crc)
        {
            return node;
        }
        hud_ui_handbook_walk_step(stack, depth);
    }
    return nullptr;
}

// 0x008477E0 (:676)
handbook_node* hud_ui_handbook_find_node(int id)
{
    handbook_node* stack[Handbook_max_depth];
    int depth = 0;
    stack[0] = Handbook_entry_tree.child;
    while (depth < Handbook_max_depth)
    {
        handbook_node* node = stack[depth];
        if (node == nullptr || node->index == id)
        {
            return node;
        }
        hud_ui_handbook_walk_step(stack, depth);
    }
    return nullptr;
}

// 0x00847880 (:717)
bool hud_ui_handbook_parse_entry(xml_element* xml_root, const char* parent_name, handbook_node* node)
{
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr || element->text == nullptr)
    {
        return false;
    }
    node->name_crc = crc_stri(element->text, 0);
    if (parent_name != nullptr && v_stricmp(parent_name, Handbook_page_names[Handbook_page_bftp]) == 0)
    {
        Num_bftp_entries++;
        node->index = atoi(element->text);
    }
    element = xml_find(xml_root, "Title");
    if (element == nullptr || element->text == nullptr)
    {
        return false;
    }
    node->name = Handbook_string_pool.add_unique(element->text);
    if (node->name == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "Description");
    node->description_crc = element != nullptr && element->text != nullptr ? localization_get_id_hash(element->text) : 0;
    element = xml_find(xml_root, "Info");
    if (element != nullptr && element->text != nullptr)
    {
        node->info = Handbook_string_pool.add_unique(element->text);
    }
    element = xml_find(xml_root, "Image");
    if (element != nullptr)
    {
        element = xml_find(element, "Filename");
        if (element != nullptr && element->text != nullptr)
        {
            char filename[64];
            cf_drop_ext(filename, 0x40, element->text);
            node->filename = Handbook_string_pool.add_unique(filename);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        node->flags.is_video = 0;
    }
    element = xml_find(xml_root, "Video");
    if (element != nullptr)
    {
        element = xml_find(element, "Filename");
        if (element != nullptr && element->text != nullptr)
        {
            node->filename = Handbook_string_pool.add_unique(element->text);
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        node->flags.is_video = 1;
    }
    element = xml_find(xml_root, "Voice_Line");
    if (element != nullptr)
    {
        node->voice_line = voice_line_handle_from_name(element->text);
    }
    element = xml_find(xml_root, "Unlocked");
    if (element == nullptr || element->text == nullptr)
    {
        return false;
    }
    if (v_stricmp(element->text, "True") == 0)
    {
        node->flags.default_viewed = 1;
        node->flags.default_unlocked = 1;
        // These two entries are unlocked but still show as new.
        if (v_stricmp(node->name, "Introduction") == 0 || v_stricmp(node->name, "Salvage_handbook") == 0)
        {
            node->flags.default_viewed = 0;
        }
    }
    else
    {
        node->flags.default_viewed = 0;
        node->flags.default_unlocked = 0;
    }
    node->flags.selectable = 1;
    return true;
}

// 0x00876FF0 (:815)
void hud_ui_handbook_load_table_data()
{
    xml_element* root = xml_table_parse("guerrilla_handbook.xtbl");
    for (xml_element* entry = xml_find(root, "Handbook_Entry"); entry != nullptr; entry = xml_find_next(root, entry, "Handbook_Entry"))
    {
        xml_element* category = xml_find(entry, "_Editor");
        if (category != nullptr)
        {
            category = xml_find(category, "Category");
        }
        if (category == nullptr || category->text == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        handbook_node* parent = hud_ui_handbook_find_dir_node(&Handbook_entry_tree, category->text);
        if (parent == nullptr)
        {
            continue;
        }
        handbook_node* node = Handbook_node_pool.alloc(0);
        if (node == nullptr)
        {
            // Original bug, kept: a full pool returns without closing the table.
            return;
        }
        if (hud_ui_handbook_parse_entry(entry, parent->name, node))
        {
            hud_ui_handbook_add_node_to_list(parent, node);
        }
        else
        {
            Handbook_node_pool.free(node);
        }
    }
    xml_table_close(root);
}

// 0x00868A60 (:867)
void hud_ui_handbook_load_tool_tips_data()
{
    handbook_node* page = hud_ui_handbook_nth_page(Handbook_page_tool_tips);
    if (page == nullptr)
    {
        return;
    }
    for (const tool_tip_base_data* tip = ui_tool_tips_get_first_tip(); tip != nullptr; tip = ui_tool_tips_get_next_tip(tip))
    {
        if (tip->flags.save_to_profile)
        {
            continue;
        }
        handbook_node* node = Handbook_node_pool.alloc(0);
        if (node == nullptr)
        {
            return;
        }
        node->reset();
        node->name = tip->title;
        node->name_crc = tip->name_crc.get_crc();
        node->description_crc = tip->description_crc;
        node->info = tip->icon;
        node->flags.selectable = 1;
        node->flags.default_unlocked = 0;
        node->flags.default_viewed = 1;
        hud_ui_handbook_add_node_to_list(page, node);
    }
}

// 0x00847B10 (:903)
static void hud_ui_init_audio_info()
{
}

// 0x00847B20 (:962)
void hud_ui_handbook_pause_video(bool pause)
{
    if (Bink_info.handle == 0)
    {
        return;
    }
    if (pause)
    {
        video_player_pause(Bink_info.handle);
        Bink_info.state = BINK_STATE_PAUSED;
    }
    else
    {
        video_player_unpause(Bink_info.handle);
        Bink_info.state = BINK_STATE_PLAYING;
    }
}

// 0x00847B60 (:976)
static void hud_ui_handbook_stop_video_lines()
{
    if (!Bink_info.playing_audio_line)
    {
        return;
    }
    if (human_voice_persona_from_handle(Bink_info.curr_persona_handle) != nullptr)
    {
        dialogue_manager_cancel(Bink_info.curr_vl_handle, true);
    }
    Bink_info.playing_audio_line = false;
    Bink_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
    Bink_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
}

// 0x008570D0 (:994)
static void hud_ui_handbook_clear_all_play_trigger_flags()
{
    if (Bink_info.line_info == nullptr)
    {
        return;
    }
    for (int i = 0; i < Bink_info.line_info->lines.size(); i++)
    {
        Bink_info.line_info->lines[i].has_triggered = false;
    }
}

// 0x00857110 (:1007)
void hud_ui_handbook_stop_video()
{
    if (Bink_info.handle != 0)
    {
        video_player_stop(Bink_info.handle);
    }
    hud_ui_handbook_stop_video_lines();
    hud_ui_handbook_clear_all_play_trigger_flags();
}

// 0x00847BB0 (:1018)
static bool hud_ui_handbook_is_video_line_still_playing()
{
    if (Bink_info.playing_audio_line && Bink_info.curr_vl_handle != INVALID_VOICE_LINE_HANDLE && Bink_info.curr_persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        Bink_info.playing_audio_line = dialogue_manager_is_dialogue_playing(Bink_info.curr_vl_handle);
        if (!Bink_info.playing_audio_line)
        {
            dialogue_manager_cancel(Bink_info.curr_vl_handle, true);
        }
    }
    return Bink_info.playing_audio_line;
}

// 0x008571A0 (:1040)
void hud_ui_handbook_trigger_video_line(int which_line)
{
    if (Bink_info.line_info == nullptr || Bink_info.which_line_index < 0 || Bink_info.which_line_index >= Bink_info.line_info->lines.size())
    {
        return;
    }
    const mission_briefing_line& line = Bink_info.line_info->lines[which_line];
    Bink_info.curr_vl_handle = line.vl_handle;
    Bink_info.curr_persona_handle = line.persona_handle;
    const human_voice_persona* persona = human_voice_persona_from_handle(Bink_info.curr_persona_handle);
    if (persona != nullptr)
    {
        Bink_info.playing_audio_line = dialogue_manager_start_dialogue(Bink_info.curr_vl_handle, persona, DIALOGUE_PRIORITY_BRIEFING, INVALID_PA_HANDLE, nullptr);
        if (!Bink_info.playing_audio_line)
        {
            Bink_info.curr_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
            Bink_info.curr_vl_handle = INVALID_VOICE_LINE_HANDLE;
        }
    }
}

// 0x00857220 (:1070)
static void hud_ui_handbook_determine_audio_and_video_status(int curr_video_frame)
{
    if (Bink_info.line_info == nullptr)
    {
        return;
    }
    farray<mission_briefing_line,40>& lines = Bink_info.line_info->lines;
    const int count = lines.size();
    if (count <= 0)
    {
        return;
    }
    const bool line_playing = hud_ui_handbook_is_video_line_still_playing();
    int which_line = -1;
    bool must_pause = false;
    for (int i = 0; i < count; i++)
    {
        if (!lines[i].has_triggered && lines[i].start_frame <= curr_video_frame)
        {
            if (which_line == -1)
            {
                which_line = i;
            }
            if (lines[i].pause_if_prev_not_done && line_playing)
            {
                must_pause = true;
            }
        }
    }
    if (!line_playing && which_line != -1)
    {
        if (!must_pause && Bink_info.handle != 0)
        {
            video_player_unpause(Bink_info.handle);
            Bink_info.state = BINK_STATE_PLAYING;
        }
        Bink_info.which_line_index = which_line;
        lines[which_line].has_triggered = true;
        hud_ui_handbook_trigger_video_line(which_line);
    }
    if (must_pause && Bink_info.handle != 0)
    {
        video_player_pause(Bink_info.handle);
        Bink_info.state = BINK_STATE_PAUSED;
    }
}

// 0x00847C00 (:1138)
static void hud_ui_handbook_video_stop_callback(int handle)
{
    (void)handle;
    if (Bink_info.handbook_node != nullptr)
    {
        Bink_info.handbook_node->flags.video_is_playing = 0;
    }
    if (Bink_cpu_mempool == nullptr)
    {
        keen::debug::nativePrintArgs("Video player is requesting us to clean-up memory, but our mempool is no longer valid.  This is really bad");
        return;
    }
    Bink_info.handle = 0;
    Bink_info.state = BINK_STATE_STOPPED;
    // Magic number: 0x40 is the pool's split flag.
    if ((Bink_gpu_mempool->get_flags() & 0x40) != 0)
    {
        ((split_mempool*)Bink_gpu_mempool)->restore_to_mark_gpu(Bink_info.mempool_mark_gpu);
    }
    if ((Bink_cpu_mempool->get_flags() & 0x40) != 0)
    {
        ((split_mempool*)Bink_cpu_mempool)->restore_to_mark_cpu(Bink_info.mempool_mark);
    }
    else
    {
        Bink_cpu_mempool->restore_to_mark(Bink_info.mempool_mark);
    }
    Bink_info.mempool_mark = 0;
    Bink_info.mempool_mark_gpu = 0;
}

// 0x00847C90 (:1172)
static void humb_obtain_bink_screen_info()
{
    vint_document* doc = ui_get_vint_document(Doc_handle_handbook);
    if (doc == nullptr)
    {
        return;
    }
    vint_element_base* element = (vint_element_base*)vint_object_base::find_object(cUI_bink_video, doc);
    if (element == nullptr)
    {
        return;
    }
    vint_variant handle;
    handle = Bink_info.handle;
    element->set_property_value("vid_id_handle", handle, 0);
    vector2 anchor;
    element->get_global_anchor(anchor);
    vint_variant size_value;
    element->get_property_value("screen_size", size_value, 0);
    vector2 size(0.0f, 0.0f);
    if (size_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
    {
        size = (vector2)size_value;
    }
    vint_variant alpha_value;
    element->get_property_value("alpha", alpha_value, 0);
    const float alpha = (float)alpha_value;
    vint_variant offset_value;
    element->get_offset(offset_value, 0);
    vector2 element_offset(0.0f, 0.0f);
    if (offset_value.get_type() == VINT_PROP_TYPE_VECTOR2F)
    {
        element_offset = (vector2)offset_value;
    }
    vint_variant auto_offset;
    element->get_auto_offset(auto_offset, 0);
    const double dx = (double)element_offset.x * (double)size.x;
    const double dy = (double)element_offset.y * (double)size.y;
    // Magic numbers: the auto offset's nine values, by the signs the asm applies (the same table as the briefing's).
    switch ((int)auto_offset)
    {
    case 1:
        anchor.x = (float)(dx + (double)anchor.x);
        anchor.y = (float)(dy + (double)anchor.y);
        break;
    case 2:
        anchor.x = (float)(dx + (double)anchor.x);
        break;
    case 3:
        anchor.y = (float)(dy + (double)anchor.y);
        break;
    case 4:
        anchor.x = (float)((double)anchor.x - dx);
        break;
    case 5:
        anchor.y = (float)((double)anchor.y - dy);
        break;
    case 7:
        anchor.x = (float)((double)anchor.x - dx);
        anchor.y = (float)(dy + (double)anchor.y);
        break;
    case 8:
        anchor.x = (float)((double)anchor.x - dx);
        anchor.y = (float)((double)anchor.y - dy);
        break;
    case 9:
        anchor.x = (float)(dx + (double)anchor.x);
        anchor.y = (float)((double)anchor.y - dy);
        break;
    default:
        break;
    }
    const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
    const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
    Bink_info.position.y = fl_round((float)((double)scale.y * (double)anchor.y + (double)offset.y));
    Bink_info.position.x = fl_round((float)((double)scale.x * (double)anchor.x + (double)offset.x));
    Bink_info.size.y = fl_round((float)((double)size.y * (double)scale.y));
    Bink_info.size.x = fl_round((float)((double)size.x * (double)scale.x));
    Bink_info.alpha = alpha;
}

// 0x00857310 (:1254)
static void hud_ui_handbook_process_video()
{
    humb_obtain_bink_screen_info();
    switch (Bink_info.state)
    {
    case BINK_STATE_STOPPED:
        break;
    case BINK_STATE_PLAY:
    {
        if (Bink_info.handle != 0)
        {
            hud_ui_handbook_stop_video();
        }
        if (Bink_gpu_mempool == nullptr || Bink_cpu_mempool == nullptr)
        {
            break;
        }
        // Magic number: 0x40 is the pool's split flag.
        Bink_info.mempool_mark_gpu = (Bink_gpu_mempool->get_flags() & 0x40) != 0 ? ((split_mempool*)Bink_gpu_mempool)->mark_gpu() : 0;
        Bink_info.mempool_mark = Bink_cpu_mempool->mark();
        float scalar = 1.0f;
        audiolib_system_get_volume_scalar(4, &scalar); // Magic number: the volume category the asm passes.
        // Original bugs, kept: a video with no line info reads through null here, and each play scales the table's stored
        // volume again.
        Bink_info.line_info->bink_video_volume = Bink_info.line_info->bink_video_volume * scalar;
        Bink_info.handle = video_player_play(Bink_info.filename, VPM_ONCE, Bink_info.position.x, Bink_info.position.y, Bink_info.size.x, Bink_info.size.y, Bink_info.line_info->bink_video_volume, true, hud_ui_handbook_video_stop_callback, Bink_gpu_mempool, Bink_cpu_mempool, Bink_cpu_mempool, 0, false, false);
        Bink_info.num_frames = video_player_get_total_frames(Bink_info.handle);
        Bink_info.state = BINK_STATE_PLAYING;
        int depth;
        handbook_node* node = hud_ui_handbook_get_child(hud_ui_handbook_find_page(Selected_page, false), Selected_row + Display_row_start_index, depth, false);
        if (node != nullptr)
        {
            Bink_info.handbook_node = node;
            node->flags.video_is_playing = 1;
        }
        break;
    }
    case BINK_STATE_PLAYING:
    {
        const int frame = video_player_get_current_frame(Bink_info.handle);
        if (Bink_info.handle != 0)
        {
            video_player_resize(Bink_info.handle, Bink_info.position.x, Bink_info.position.y, Bink_info.size.x, Bink_info.size.y);
            hud_ui_handbook_determine_audio_and_video_status(frame);
        }
        // Magic number: within 10 frames of the end counts as watched.
        if (Bink_info.num_frames - 10 < frame && Bink_info.handbook_node != nullptr && Bink_info.handbook_node->flags.video_is_playing)
        {
            Bink_info.handbook_node->flags.video_watched = 1;
        }
        break;
    }
    case BINK_STATE_LOOPING_END:
        if (Bink_info.handle != 0 && Bink_info.num_frames - video_player_get_current_frame(Bink_info.handle) < 3)
        {
            video_player_goto_frame(Bink_info.handle, Bink_info.num_frames - Bink_info.line_info->loop_frames);
        }
        break;
    case BINK_STATE_PAUSED:
        if (Bink_info.handle != 0)
        {
            video_player_resize(Bink_info.handle, Bink_info.position.x, Bink_info.position.y, Bink_info.size.x, Bink_info.size.y);
            const int frame = video_player_get_current_frame(Bink_info.handle);
            if (Bink_info.num_frames - frame > 2)
            {
                hud_ui_handbook_determine_audio_and_video_status(frame);
            }
            else
            {
                video_player_set_volume(Bink_info.handle, 0.0f);
                video_player_goto_frame(Bink_info.handle, 0);
                Bink_info.state = BINK_STATE_LOOPING_END;
            }
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
}

// 0x00848140 (:1378)
static void handbook_do_exit(game_state new_state)
{
    if (Streamed_image_name != nullptr)
    {
        ui_release_image(Streamed_image_name);
        Streamed_image_name = nullptr;
    }
    ui_unload_document(Doc_handle_handbook, DFOT_NONE, 0.5f);
    ui_relinquish_control_id(Control_id_handbook);
    ui_relinquish_input_id(Input_id_handbook);
    if (new_state != GS_INGAME_OPTIONS)
    {
        hud_ui_reload_items();
    }
    if (Blur_called)
    {
        ui_set_screen_blur(false, true);
    }
    ingame_options_show_menu(true);
    audiolib_play_fade_out(Voice_data.audio_id, 150); // Magic number: 150 ms.
    Voice_data.audio_id = -1;
}

// 0x008481E0 (:1405)
void hud_ui_handbook_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("HeaderData");
    if (item != nullptr)
    {
        static const vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 3);
        Data_header_handle_handbook = (int)item->get_handle();
        ui_add_data_item(Doc_handle_handbook, Data_header_handle_handbook);
    }
}

// 0x00848260 (:1428)
void hud_ui_handbook_create_page_tab_data()
{
    vint_data_item* item = vint_data_item::create_new("PageTabData");
    if (item != nullptr)
    {
        static const vint_variant_type types[11] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 11);
        Data_row_tab_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_handbook, Data_row_tab_handle);
    }
}

// 0x00848300 (:1459)
void hud_ui_handbook_create_row_data()
{
    vint_data_group* group = vint_data_group::create_new("RowData");
    if (group != nullptr)
    {
        Data_row_group_handle = (int)group->get_handle();
        ui_add_data_group(Doc_handle_handbook, Data_row_group_handle);
    }
}

// 0x00848330 (:1472)
void hud_ui_handbook_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("HintData");
    if (item != nullptr)
    {
        static const vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 5);
        Data_hints_handle_handbook = (int)item->get_handle();
        ui_add_data_item(Doc_handle_handbook, Data_hints_handle_handbook);
    }
}

// 0x008483B0 (:1497)
void hud_ui_handbook_create_page_data()
{
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Handbook_page_data_names[i]);
        if (item == nullptr)
        {
            continue;
        }
        // Elements 3 to 5 differ by page: image, info text, video length. Type 0 is VINT_PROP_TYPE_NONE.
        vint_variant_type types[7] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_NONE, VINT_PROP_TYPE_NONE, VINT_PROP_TYPE_NONE, VINT_PROP_TYPE_BOOL};
        switch (i)
        {
        case 0:
            types[5] = VINT_PROP_TYPE_FLOAT;
            break;
        case 1:
        case 2:
            types[3] = VINT_PROP_TYPE_STRING;
            types[4] = VINT_PROP_TYPE_STRING;
            break;
        case 3:
        case 4:
        case 6:
            types[3] = VINT_PROP_TYPE_STRING;
            break;
        case 5:
            types[4] = VINT_PROP_TYPE_STRING;
            break;
        case 7:
            types[2] = VINT_PROP_TYPE_NONE;
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        item->set_template(types, 7);
        Data_page_handles[i] = (int)item->get_handle();
        ui_add_data_item(Doc_handle_handbook, Data_page_handles[i]);
    }
}

// 0x00848540 (:1612)
void hud_ui_handbook_create_data_items()
{
    hud_ui_handbook_create_header_data();
    hud_ui_handbook_create_page_tab_data();
    hud_ui_handbook_create_row_data();
    hud_ui_handbook_create_hints_data();
    hud_ui_handbook_create_page_data();
}

// 0x00848580 (:1625)
void handbook_entry_set_viewed(handbook_node* node, int curr_time, int start_time)
{
    if (!node->flags.is_video)
    {
        // Magic number: an entry counts as read after a second on it.
        if (curr_time - start_time > 1000)
        {
            node->flags.viewed = 1;
        }
    }
    else if (Bink_info.handbook_node != nullptr && Bink_info.handbook_node->flags.video_watched)
    {
        node->flags.viewed = 1;
    }
}

// 0x008485C0 (:1639)
void hud_ui_process_mouse_input()
{
    if (ui_mouse_process_hints(&kHintBack, 1, Doc_handle_handbook, true) > -1)
    {
        audiolib_cue_play(Ga_cue_ids.ui_sound_press_abort, nullptr, nullptr);
        ExitHandbook = true;
        return;
    }
    const bool double_clicked = mouse_double_clicked(0);
    if (!mouse_just_released(0) && !double_clicked)
    {
        return;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_handbook);
    if (doc == nullptr)
    {
        return;
    }
    const int num_pages = hud_ui_handbook_count_viewable_pages();
    if (ui_is_mouse_over_vint_object(kRightArrowGroup, doc))
    {
        Selected_page++;
        if (Selected_page >= num_pages)
        {
            Selected_page = 0;
        }
        audiolib_cue_play(Ga_cue_ids.ui_sound_scroll_right, nullptr, nullptr);
        return;
    }
    if (ui_is_mouse_over_vint_object(kLeftArrowGroup, doc))
    {
        Selected_page--;
        if (Selected_page < 0)
        {
            Selected_page = num_pages - 1;
        }
        audiolib_cue_play(Ga_cue_ids.ui_sound_scroll_left, nullptr, nullptr);
        return;
    }
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        if (ui_is_mouse_over_vint_object(kPageHeaders[i], doc))
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_scroll_right, nullptr, nullptr);
            if (Selected_page == i)
            {
                return;
            }
            Display_row_start_index = 0;
            Selected_row = 0;
            Selected_page = i;
        }
    }
    int num_rows = hud_ui_handbook_count_viewable_children(hud_ui_handbook_find_page(Selected_page, false), 0);
    if (num_rows > Num_handbook_rows)
    {
        num_rows = Num_handbook_rows;
    }
    for (int i = 0; i < num_rows; i++)
    {
        char name[128];
        ui_menu_get_cloned_name(name, 0x80, kRowGroup, i, false);
        vint_object_base* row = vint_object_base::find_object(name, doc);
        if (row == nullptr || !ui_is_mouse_over_vint_object(row) || !ui_vint_object_visible(row))
        {
            continue;
        }
        const int old_row = Selected_row;
        const int old_page = Selected_page;
        Selected_row = i;
        handbook_node* page = hud_ui_handbook_find_page(Selected_page, false);
        int depth;
        hud_ui_handbook_get_child(page, Display_row_start_index + i, depth, false);
        audiolib_cue_play(Ga_cue_ids.ui_sound_move_up, nullptr, nullptr);
        hud_ui_handbook_get_child(page, Selected_row + Display_row_start_index, depth, false);
        if (double_clicked)
        {
            Mouse_double_clicked = true;
        }
        page = hud_ui_handbook_find_page(old_page, false);
        if (old_page == 0)
        {
            return;
        }
        handbook_node* old_node = hud_ui_handbook_get_child(page, Display_row_start_index + old_row, depth, false);
        if (old_node != nullptr)
        {
            const int start_time = time_we_moved_onto_an_entry;
            handbook_entry_set_viewed(old_node, timer_get(TIMER_MILLISECONDS), start_time);
        }
        return;
    }
}

// 0x00848880 (:1779)
void hud_ui_handbook_process_scroll_bar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0) && !mouse_just_scrolled_wheel())
    {
        return;
    }
    const int count = hud_ui_handbook_count_viewable_children(hud_ui_handbook_find_page(Selected_page, false), 0);
    if (count <= Num_handbook_rows)
    {
        return;
    }
    const int max_start = count - Num_handbook_rows;
    vint_document* doc = ui_get_vint_document(Doc_handle_handbook);
    if (doc == nullptr)
    {
        return;
    }
    vint_object_base* outline = vint_object_base::find_object(kScrollBarOutline, doc);
    if (outline == nullptr || !ui_vint_object_visible(outline))
    {
        return;
    }
    if ((!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0)) || !ui_is_mouse_over_vint_object(outline))
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float height = 0.0f;
    ui_get_vint_obj_position(outline, x, y);
    ui_get_vint_obj_size(outline, x, height);
    // Double chain per the asm, rounded to float at each step.
    const float fraction = (float)(((double)Mouse_scaled_y - (double)y) / (double)height);
    const float position = (float)((double)fraction * (double)(float)count + 0.5);
    int start = (int)floor((double)position);
    int row = 0;
    if (start > max_start)
    {
        row = start - max_start - 1;
        start = max_start;
    }
    if (start != Display_row_start_index || row != Selected_row)
    {
        audiolib_cue_play(Ga_cue_ids.ui_sound_move_up, nullptr, nullptr);
        Display_row_start_index = start;
        Selected_row = row;
    }
}

// 0x008575F0 (:1849)
void hud_ui_handbook_process_input()
{
    Mouse_double_clicked = false;
    const int old_page = Selected_page;
    const int curr_time = timer_get(TIMER_MILLISECONDS);
    hud_ui_handbook_process_scroll_bar();
    hud_ui_process_mouse_input();
    if (ExitHandbook)
    {
        return;
    }
    handbook_node* page = hud_ui_handbook_find_page(Selected_page, false);
    int depth;
    handbook_node* current = hud_ui_handbook_get_child(page, Selected_row + Display_row_start_index, depth, false);
    const int num_pages = hud_ui_handbook_count_viewable_pages();
    const int count = hud_ui_handbook_count_viewable_children(page, 0);
    const int old_start = Display_row_start_index;
    const int old_row = Selected_row;
    // Magic numbers: ui button ids 10 accept, 11 back, 29 the second back, 1 up, 2 down, 17 and 19 the shoulder buttons.
    if (ui_get_button_info(10, Input_id_handbook) > 0.0f || Mouse_double_clicked)
    {
        if (page == hud_ui_handbook_nth_page(Handbook_page_cinematics))
        {
            Trigger_cutscene = true;
            audiolib_cue_play(Ga_cue_ids.ui_sound_press_select, nullptr, nullptr);
        }
    }
    else if (ui_get_button_info(2, Input_id_handbook) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f || (count > 0 && (current == nullptr || !current->flags.selectable || current->unlock_number < 0)))
    {
        handbook_node* node;
        do
        {
            if (Selected_row + Display_row_start_index < count - 1)
            {
                if (Selected_row + 1 < Num_handbook_rows)
                {
                    Selected_row++;
                }
                else
                {
                    Display_row_start_index++;
                }
            }
            else
            {
                Display_row_start_index = 0;
                Selected_row = 0;
            }
            node = hud_ui_handbook_get_child(page, Selected_row + Display_row_start_index, depth, false);
            if (node == nullptr)
            {
                break;
            }
            if (node->flags.selectable)
            {
                break;
            }
        } while (node != current);
        if (node != nullptr && node->flags.selectable && node != current)
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_move_down, nullptr, nullptr);
            if (current != nullptr)
            {
                handbook_entry_set_viewed(current, curr_time, time_we_moved_onto_an_entry);
            }
        }
        else
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_invalid, nullptr, nullptr);
        }
    }
    else if (ui_get_button_info(1, Input_id_handbook) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
    {
        handbook_node* node;
        do
        {
            if (Selected_row > 0)
            {
                Selected_row--;
            }
            else if (Display_row_start_index > 0)
            {
                Display_row_start_index--;
            }
            else
            {
                Selected_row = (count < Num_handbook_rows ? count : Num_handbook_rows) - 1;
                const int start = count - Selected_row - 1;
                Display_row_start_index = start > 0 ? start : 0;
            }
            node = hud_ui_handbook_get_child(page, Selected_row + Display_row_start_index, depth, false);
            if (node == nullptr)
            {
                break;
            }
            if (node->flags.selectable)
            {
                break;
            }
        } while (node != current);
        if (node != nullptr && node->flags.selectable && node != current)
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_move_up, nullptr, nullptr);
            if (current != nullptr)
            {
                handbook_entry_set_viewed(current, curr_time, time_we_moved_onto_an_entry);
            }
        }
        else
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_invalid, nullptr, nullptr);
        }
    }
    else if (ui_get_button_info(11, Input_id_handbook) > 0.0f || ui_get_button_info(29, Input_id_handbook) > 0.0f)
    {
        // Which button was pressed picks the sound; button 11 plays the select sound, as in rfg.exe.
        if (ui_get_button_info(11, Input_id_handbook) > 0.0f)
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_press_select, nullptr, nullptr);
        }
        else
        {
            audiolib_cue_play(Ga_cue_ids.ui_sound_press_abort, nullptr, nullptr);
        }
        if (current != nullptr)
        {
            handbook_entry_set_viewed(current, curr_time, time_we_moved_onto_an_entry);
        }
        ExitHandbook = true;
    }
    else if (ui_get_button_info(17, Input_id_handbook) > 0.0f)
    {
        audiolib_cue_play(Ga_cue_ids.ui_sound_bumper_left, nullptr, nullptr);
        Selected_page--;
        if (Selected_page < 0)
        {
            Selected_page = num_pages - 1;
        }
        Display_row_start_index = 0;
        Selected_row = 0;
        Selected_page_display_start_time = curr_time;
        if (current != nullptr)
        {
            handbook_entry_set_viewed(current, curr_time, time_we_moved_onto_an_entry);
        }
    }
    else if (ui_get_button_info(19, Input_id_handbook) > 0.0f)
    {
        audiolib_cue_play(Ga_cue_ids.ui_sound_bumper_right, nullptr, nullptr);
        Selected_page++;
        if (Selected_page >= num_pages)
        {
            Selected_page = 0;
        }
        Display_row_start_index = 0;
        Selected_row = 0;
        Selected_page_display_start_time = curr_time;
        if (current != nullptr)
        {
            handbook_entry_set_viewed(current, curr_time, time_we_moved_onto_an_entry);
        }
    }
    if (time_we_moved_onto_an_entry == 0 || old_page != Selected_page || old_row + old_start != Selected_row + Display_row_start_index)
    {
        time_we_moved_onto_an_entry = curr_time;
    }
}

// 0x00848A90 (:2055)
void hud_ui_handbook_process_header()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle_handbook);
    if (item == nullptr)
    {
        return;
    }
    int count = 0;
    handbook_node* page = hud_ui_handbook_find_page(Selected_page, false);
    if (page != nullptr)
    {
        count = hud_ui_handbook_count_viewable_children(page, 0);
    }
    item->set_element(0, vint_variant(cTitle));
    item->set_element(1, vint_variant(count > Num_handbook_rows));
    item->set_element(2, vint_variant((float)(Selected_row + Display_row_start_index) / (float)(count - 1)));
}

// 0x00848B90 (:2075)
void hud_ui_handbook_process_page_tab()
{
    vint_data_item* item = vint_data_item::find(Data_row_tab_handle);
    if (item == nullptr)
    {
        return;
    }
    const int num_pages = hud_ui_handbook_count_viewable_pages();
    const handbook_node* selected = hud_ui_handbook_find_page(Selected_page, false);
    item->set_element(0, vint_variant(selected != nullptr ? selected->name : ""));
    item->set_element(1, vint_variant(Selected_page + 1));
    item->set_element(2, vint_variant(num_pages));
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        bool unread = false;
        if (i < num_pages)
        {
            handbook_node* page = hud_ui_handbook_find_page(i, false);
            unread = page != nullptr && hud_ui_handbook_child_node_is_unread(page);
        }
        item->set_element(i + 3, vint_variant(unread));
    }
}

// 0x00848CC0 (:2109)
void hud_ui_handbook_process_rows()
{
    vint_data_group* group = vint_data_group::find(Data_row_group_handle);
    if (group == nullptr)
    {
        return;
    }
    handbook_node* page = hud_ui_handbook_find_page(Selected_page, false);
    const bool cinematics = page != nullptr && page == hud_ui_handbook_nth_page(Handbook_page_cinematics);
    int count = hud_ui_handbook_count_viewable_children(page, 0);
    int num_items = (int)group->get_num_data_items();
    if (count > Num_handbook_rows)
    {
        count = Num_handbook_rows;
    }
    if (count < num_items)
    {
        while (--num_items >= count)
        {
            group->remove_data_item(Data_row_handles[num_items]);
        }
    }
    else
    {
        for (; num_items < count; num_items++)
        {
            vint_data_item* row = group->add_data_item(nullptr);
            if (row != nullptr)
            {
                static const vint_variant_type types[6] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
                row->set_template(types, 6);
                Data_row_handles[num_items] = (int)row->get_handle();
            }
        }
    }
    for (int i = 0; i < (int)group->get_num_data_items(); i++)
    {
        vint_data_item* row = vint_data_item::find(Data_row_handles[i]);
        if (row == nullptr)
        {
            continue;
        }
        row->set_element(0, vint_variant(i + 1));
        const int index = Display_row_start_index + i;
        int depth;
        const handbook_node* node = hud_ui_handbook_get_child(page, index, depth, false);
        if (node == nullptr)
        {
            row->set_element(1, vint_variant(cEmptystringHandbook));
            row->set_element(2, vint_variant(i == Selected_row));
            row->set_element(3, vint_variant(false));
            row->set_element(4, vint_variant(index % 2 == 0));
            row->set_element(5, vint_variant(cEmptystringHandbook));
            continue;
        }
        // Magic numbers: the sizes of rfg.exe's buffers on the stack.
        wchar_t text[129];
        wchar_t converted[128];
        char encoded[384];
        text[0] = L'\0';
        for (int j = 0; j < depth; j++)
        {
            wcscat(text, cTab);
        }
        if (localization_does_tag_exist(node->name))
        {
            wcscat(text, localization_get_display_string_req(node->name, nullptr));
        }
        else
        {
            copy_to_wide(converted, node->name, 0x80);
            wcscat(text, converted);
        }
        vint_encode_wide_text(encoded, 0x180, text);
        row->set_element(1, vint_variant(encoded));
        row->set_element(2, vint_variant(i == Selected_row));
        row->set_element(3, vint_variant(cinematics && node->flags.selectable));
        row->set_element(4, vint_variant(index % 2 == 0));
        row->set_element(5, vint_variant(node->flags.viewed ? cEmptystringHandbook : cNew_string));
    }
}

// 0x008491B0 (:2218)
void hud_ui_handbook_proccess_hints()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle_handbook);
    if (item == nullptr)
    {
        return;
    }
    char button[64];
    char image[128];
    controls_get_button_image_name(button, 0x40, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(0, vint_variant(image));
    controls_get_button_image_name(button, 0x40, CBA_MENU_L2, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(1, vint_variant(image));
    controls_get_button_image_name(button, 0x40, CBA_MENU_R2, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(2, vint_variant(image));
    controls_get_button_image_name(button, 0x40, CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(3, vint_variant(image));
    item->set_element(4, vint_variant(cBackHandbook));
}

// 0x00868AF0 (:2247)
void hud_ui_handbook_process_page()
{
    handbook_node* selected_page = hud_ui_handbook_find_page(Selected_page, false);
    const int display_time = timer_get(TIMER_MILLISECONDS) - Selected_page_display_start_time;
    if (selected_page == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        vint_data_item* item = vint_data_item::find(Data_page_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        const char* title = cEmptystringHandbook;
        const char* image = nullptr;
        const char* info = cEmptystringHandbook;
        const char* video = nullptr;
        unsigned int description = 0;
        voice_line_handle line = INVALID_VOICE_LINE_HANDLE;
        bool visible = false;
        bool bftp_next = false;
        float video_length = 0.0f;
        if (hud_ui_handbook_nth_page(i) == selected_page)
        {
            visible = true;
            int depth;
            const handbook_node* node = hud_ui_handbook_get_child(hud_ui_handbook_find_page(Selected_page, false), Selected_row + Display_row_start_index, depth, false);
            if (node != nullptr)
            {
                title = node->name;
                description = node->description_crc;
                if (i == 0)
                {
                    if (node->flags.is_video)
                    {
                        video = node->filename;
                    }
                    else
                    {
                        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    }
                }
                else
                {
                    line = node->voice_line;
                    if (!node->flags.is_video)
                    {
                        image = node->filename;
                    }
                    else
                    {
                        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    }
                    if (i == Handbook_page_bftp)
                    {
                        sprintf(bftp_count, "%d/%d", Num_unlocked_bftp_entries, Num_bftp_entries - 1);
                        bftp_next = Selected_row + Display_row_start_index == Num_unlocked_bftp_entries;
                        info = bftp_count;
                    }
                    else
                    {
                        info = node->info;
                    }
                }
            }
            if (Bink_info.filename == video)
            {
                if (Bink_info.filename != nullptr && Bink_info.num_frames > 0)
                {
                    video_length = (float)Bink_info.num_frames / 29.0f; // Magic number: the frame rate rfg.exe assumes here.
                }
            }
            else
            {
                if (Bink_info.filename != nullptr)
                {
                    hud_ui_handbook_stop_video();
                    Bink_info.filename = nullptr;
                    Bink_info.line_info = nullptr;
                }
                if (video != nullptr && Bink_info.state == BINK_STATE_STOPPED)
                {
                    Bink_info.state = BINK_STATE_PLAY;
                    Bink_info.filename = video;
                    char name[128];
                    cf_drop_ext(name, 0x80, video);
                    Bink_info.line_info = hud_ui_mission_briefing_get_line_info(name);
                    video_length = 45.0f; // Magic number: seconds, until the real length is known.
                    Bink_info.num_frames = 0;
                }
            }
            human_voice_persona* persona = human_voice_persona_from_name(cPersona_name);
            voice_line_handle current_line = Voice_data.current_voice_line;
            if (persona != nullptr && Voice_data.current_voice_line != line)
            {
                audiolib_play_fade_out(Voice_data.audio_id, 150); // Magic number: 150 ms.
                Voice_data.audio_id = -1;
                current_line = line;
                if (line != INVALID_VOICE_LINE_HANDLE)
                {
                    unsigned short radio_id;
                    persona->persona_play_voice_line(line, ZERO_VECTOR, AUDIOLIB_CUE_PRIORITY_MEDIUM, Voice_data.audio_id, radio_id, nullptr, false, false, true, true, nullptr);
                }
            }
            Voice_data.current_voice_line = current_line;
            if (Bink_info.handle == 0 && Streamed_image_name != image && image != nullptr)
            {
                Image_to_stream_name = image;
            }
            if (Streamed_image_name == nullptr || Image_to_stream_name != nullptr)
            {
                image = nullptr;
            }
            else
            {
                const stream2_status_val status = ui_get_image_status(Streamed_image_name);
                if (status != ST_UNLOADED && status != ST_LOADED)
                {
                    image = cEmptystringHandbook;
                }
            }
        }
        // Magic number: no image for the page's first 100 ms.
        if (display_time < 100)
        {
            image = nullptr;
        }
        item->set_element(0, vint_variant(visible));
        item->set_element(1, vint_variant(title));
        item->set_element(2, vint_variant(description));
        item->set_element(3, vint_variant(image));
        item->set_element(4, vint_variant(info));
        if (video_length > 0.0f)
        {
            item->set_element(5, vint_variant(video_length));
        }
        item->set_element(6, vint_variant(bftp_next));
    }
}

// 0x00869010 (:2485)
void hud_ui_handbook_process_info()
{
    hud_ui_handbook_process_header();
    hud_ui_handbook_process_page_tab();
    hud_ui_handbook_process_rows();
    hud_ui_handbook_proccess_hints();
    hud_ui_handbook_process_page();
}

// 0x00857AB0 (:2500)
bool hud_ui_handbook_exit_notify(game_state new_state)
{
    ExitHandbook = true;
    if (new_state == GS_SHUTDOWN)
    {
        return true;
    }
    if (!Performed_exit)
    {
        handbook_do_exit(new_state);
        Performed_exit = true;
    }
    if (!Video_stopped)
    {
        if (Bink_info.handle != 0)
        {
            hud_ui_handbook_stop_video();
            Video_stopped = true;
            return false;
        }
    }
    else if (Bink_info.handle != 0)
    {
        return false;
    }
    if (new_state != GS_GAMEPLAY && new_state != GS_INGAME_OPTIONS && gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY))
    {
        return false;
    }
    if (Bink_info.handle == 0)
    {
        if (Bink_cpu_mempool != nullptr && Bink_cpu_mempool != Bink_gpu_mempool)
        {
            salloc_gameplay_cpu_unlock_async();
            Bink_cpu_mempool = nullptr;
        }
        if (Bink_gpu_mempool != nullptr)
        {
            salloc_gameplay_gpu_unlock_async();
            if (Bink_cpu_mempool == Bink_gpu_mempool)
            {
                Bink_cpu_mempool = nullptr;
            }
            Bink_gpu_mempool = nullptr;
        }
    }
    if (new_state != GS_GAMEPLAY && new_state != GS_INGAME_OPTIONS && Bink_gpu_mempool == nullptr && !salloc_gameplay_gpu_unlock_async_poll())
    {
        return false;
    }
    return true;
}

// 0x008770E0 (:2566)
void hud_ui_handbook_init()
{
    state_info info;
    info.enter = hud_ui_handbook_enter;
    info.exit = hud_ui_handbook_exit;
    info.exit_notify = hud_ui_handbook_exit_notify;
    info.process = hud_ui_handbook_process;
    info.handshake = hud_ui_handbook_handshake;
    info.render = hud_ui_handbook_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_INGAME_HANDBOOK_HUD, info);
    if (Handbook_string_pool_data == nullptr)
    {
        // Magic number: 5000 bytes of strings.
        Handbook_string_pool_data = (char*)Permanent_mempool.alloc(5000, 0);
        if (Handbook_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the handbook string table");
        }
        Handbook_string_pool.init(Handbook_string_pool_data, 5000, "Handbook String Pool", 0, 0);
        Handbook_string_pool.clear();
    }
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        hud_ui_handbook_find_dir_node(&Handbook_entry_tree, Handbook_page_names[i]);
    }
    hud_ui_handbook_load_table_data();
    hud_ui_handbook_load_tool_tips_data();
}

// 0x008493A0 (:2603)
void hud_ui_handbook_level_init_pre()
{
    handbook_node* stack[Handbook_max_depth];
    int depth = 0;
    stack[0] = Handbook_entry_tree.child;
    while (depth < Handbook_max_depth && stack[depth] != nullptr)
    {
        handbook_node* node = stack[depth];
        node->unlock_number = node->flags.default_unlocked ? 0 : -1;
        node->flags.viewed = node->flags.default_viewed;
        hud_ui_handbook_walk_step(stack, depth);
    }
    Selected_page = 0;
    Display_row_start_index = 0;
    Selected_row = 0;
    Num_unlocked_bftp_entries = 0;
}

// 0x00869030 (:2640)
void hud_ui_handbook_enter(game_state prev_state)
{
    ExitHandbook = false;
    LoadedHandbook = false;
    Requested_bink = false;
    Video_stopped = false;
    Performed_exit = false;
    Trigger_cutscene = false;
    Playing_cutscene = false;
    Voice_data.audio_id = -1;
    Input_id_handbook = -1;
    Bink_info.reset();
    Doc_handle_handbook = ui_load_document(cDocnameHandbook, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_UI_DOC, false);
    if (Doc_handle_handbook == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    if (prev_state != GS_INGAME_OPTIONS)
    {
        hud_ui_unload_items();
        ui_set_screen_blur(true, true);
    }
    Blur_called = prev_state != GS_INGAME_OPTIONS;
    ingame_options_show_menu(false);
    Control_id_handbook = ui_request_control_id();
    ui_wait_for_button_ups();
    humb_clear_all_play_trigger_flags();
    if (prev_state != GS_INGAME_OPTIONS)
    {
        return;
    }
    // From the options menu, the first page opens if it has something unread.
    handbook_node* first = Handbook_entry_tree.child;
    if (first == nullptr || !hud_ui_handbook_child_node_is_unread(first))
    {
        return;
    }
    const int num_pages = hud_ui_handbook_count_viewable_pages();
    for (int i = 0; i < num_pages; i++)
    {
        if (hud_ui_handbook_find_page(i, false) == first)
        {
            Display_row_start_index = 0;
            Selected_row = 0;
            Selected_page = i;
            return;
        }
    }
}

// 0x00849470 (:2714)
void hud_ui_handbook_exit(game_state new_state)
{
    (void)new_state;
}

// 0x00849480 (:2724)
void hud_ui_handbook_handshake(rl_renderer*)
{
    if (!ExitHandbook && Bink_gpu_mempool == nullptr)
    {
        if (!Requested_bink && game_is_paused() && !ui_get_screen_blurring())
        {
            Requested_bink = salloc_gameplay_gpu_lock_async();
            if (!Requested_bink)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                gameseq_pop_state();
                return;
            }
        }
        if (Requested_bink && Bink_gpu_mempool == nullptr)
        {
            Bink_gpu_mempool = salloc_gameplay_gpu_lock_async_poll();
            if (Bink_gpu_mempool != nullptr)
            {
                Bink_cpu_mempool = Bink_gpu_mempool;
            }
        }
    }
    if (Image_to_stream_name == nullptr)
    {
        return;
    }
    // A new image loads one frame after the old one is released.
    if (Load_image_next_frame_handbook)
    {
        ui_request_image(Image_to_stream_name);
        Streamed_image_name = Image_to_stream_name;
        Image_to_stream_name = nullptr;
        Load_image_next_frame_handbook = false;
        return;
    }
    if (Streamed_image_name != nullptr)
    {
        ui_release_image(Streamed_image_name);
        Streamed_image_name = nullptr;
    }
    Load_image_next_frame_handbook = true;
}

// 0x008691E0 (:2788)
void hud_ui_handbook_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (msgbox_active)
    {
        ui_process(Control_id_handbook, msgbox_active);
        return;
    }
    if (ExitHandbook)
    {
        if (!Video_stopped && Bink_info.handle != 0)
        {
            hud_ui_handbook_stop_video();
            Video_stopped = true;
        }
        if (Bink_info.handle == 0)
        {
            if (Bink_cpu_mempool != nullptr && Bink_cpu_mempool != Bink_gpu_mempool)
            {
                salloc_gameplay_cpu_unlock_async();
                Bink_cpu_mempool = nullptr;
            }
            if (Bink_gpu_mempool != nullptr)
            {
                salloc_gameplay_gpu_unlock_async();
                Bink_gpu_mempool = nullptr;
                Bink_cpu_mempool = nullptr;
            }
        }
        else if (Bink_gpu_mempool != nullptr)
        {
            ui_process(Control_id_handbook, msgbox_active);
            return;
        }
        if (salloc_gameplay_gpu_unlock_async_poll())
        {
            gameseq_pop_state();
        }
        ui_process(Control_id_handbook, msgbox_active);
        return;
    }
    if (!LoadedHandbook)
    {
        if (Bink_gpu_mempool == nullptr)
        {
            ui_process(Control_id_handbook, msgbox_active);
            return;
        }
        switch (ui_get_doc_asset_state(Doc_handle_handbook))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            ui_unload_document(Doc_handle_handbook, DFOT_NONE, 0.5f);
            Doc_handle_handbook = -1;
            ExitHandbook = true;
            break;
        case ST_ERROR:
            ui_unload_document(Doc_handle_handbook, DFOT_NONE, 0.5f);
            Doc_handle_handbook = -1;
            ExitHandbook = true;
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            LoadedHandbook = true;
            Input_id_handbook = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            hud_ui_handbook_create_data_items();
            hud_ui_handbook_process_info();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    if (LoadedHandbook)
    {
        if (Trigger_cutscene)
        {
            if (Bink_info.state == BINK_STATE_STOPPED && Bink_info.handle == 0)
            {
                int depth;
                const handbook_node* node = hud_ui_handbook_get_child(hud_ui_handbook_find_page(Selected_page, false), Selected_row + Display_row_start_index, depth, false);
                if (node != nullptr && Bink_gpu_mempool != nullptr && Bink_cpu_mempool != nullptr)
                {
                    video_cutscene_play(node->info, 0xFFFFFFFF, nullptr, true, false, Bink_gpu_mempool, Bink_cpu_mempool, nullptr);
                    ui_fade_to_black_instant_fade_out();
                    Playing_cutscene = true;
                }
                Trigger_cutscene = false;
            }
        }
        else if (Playing_cutscene)
        {
            if (gameseq_get_state() == GS_INGAME_HANDBOOK_HUD)
            {
                Playing_cutscene = false;
                ui_fade_to_black_instant_fade_in();
            }
        }
        else
        {
            hud_ui_handbook_process_input();
        }
        hud_ui_handbook_process_info();
        hud_ui_handbook_process_video();
        ui_add_for_processing(Doc_handle_handbook);
    }
    ui_process(Control_id_handbook, msgbox_active);
}

// 0x00849560 (:2919)
void hud_ui_handbook_render(rl_renderer*)
{
    if (LoadedHandbook)
    {
        ui_add_for_rendering(Doc_handle_handbook);
    }
    ui_render(Control_id_handbook, false);
}

// 0x00849590 (:2929)
bool hud_ui_handbook_show()
{
    if (Push_state_handbook)
    {
        return false;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && game_is_paused() && gameseq_get_state() == GS_INGAME_OPTIONS)
    {
        gameseq_push_state(GS_INGAME_HANDBOOK_HUD, true, false);
        return true;
    }
    Push_state_handbook = true;
    return true;
}

// 0x008495E0 (:2944)
void hud_ui_handbook_level_shutdown()
{
    Push_state_handbook = false;
}

// 0x008495F0 (:2949)
void hud_ui_handbook_do_frame()
{
    if (Push_state_handbook && !game_is_paused() && gameseq_get_pending_state() == GS_NONE)
    {
        gameseq_push_state(GS_INGAME_HANDBOOK_HUD, true, false);
        Push_state_handbook = false;
    }
}

// 0x00869470 (:2959)
void hud_ui_handbook_save(cfile* save_file)
{
    if (save_file == nullptr)
    {
        return;
    }
    // Magic number: 176, the node pool's size.
    struct record
    {
        unsigned int name_crc;
        bool viewed;
        signed char unlock_number;
    };
    record records[176];
    memset(records, 0, sizeof(records));
    unsigned char count = 0;
    for (handbook_node* node = Handbook_node_pool.get_first(0); node != nullptr; node = Handbook_node_pool.get_next(node))
    {
        records[count].name_crc = node->name_crc;
        records[count].viewed = node->flags.viewed == 1;
        records[count].unlock_number = (signed char)node->unlock_number;
        count++;
    }
    cf_write_uint8(count, save_file);
    cf_write_uint8((unsigned char)Unlock_counter, save_file);
    cf_write_uint8((unsigned char)Selected_page, save_file);
    for (int i = 0; i < count; i++)
    {
        cf_write_uint32(records[i].name_crc, save_file);
        cf_write_bool(records[i].viewed, save_file);
        cf_write_int8(records[i].unlock_number, save_file);
    }
}

// 0x00849630 (:2991)
// Original: a count over the pool's size leaves the rest unread, and the save's section check then fails the load.
void hud_ui_handbook_load(cfile* save_file)
{
    if (save_file == nullptr)
    {
        return;
    }
    const unsigned int count = cf_read_uint8(save_file);
    if (count > 176) // Magic number: the node pool's size.
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    Unlock_counter = cf_read_uint8(save_file);
    Selected_page = cf_read_uint8(save_file);
    for (unsigned int i = 0; i < count; i++)
    {
        const unsigned int name_crc = cf_read_uint32(save_file);
        const bool viewed = cf_read_uint8(save_file) != 0;
        const signed char unlock_number = cf_read_int8(save_file);
        handbook_node* node = hud_ui_handbook_find_node(name_crc);
        if (node != nullptr)
        {
            node->unlock_number = unlock_number;
            node->flags.viewed = viewed ? 1 : 0;
        }
    }
    for (int i = 0; i < Num_handbook_pages; i++)
    {
        handbook_node* page = hud_ui_handbook_nth_page(i);
        if (page != nullptr)
        {
            hud_ui_handbook_sort_children(page);
            if (i == Handbook_page_bftp)
            {
                Num_unlocked_bftp_entries = hud_ui_handbook_count_viewable_children(page, 0) - 1;
            }
        }
    }
}

// 0x00849720 (:3036)
void hud_ui_handbook_unlock_entry(unsigned int name_crc)
{
    if (Multiplayer || wrecking_crew_is_active() || World.dlc_bundle_id != -1)
    {
        return;
    }
    handbook_node* node = hud_ui_handbook_find_node(name_crc);
    if (node != nullptr && node->unlock_number < 0)
    {
        Unlock_counter++;
        node->unlock_number = Unlock_counter;
        hud_ui_handbook_set_page_as_newest(node);
    }
}

// 0x00849770 (:3055)
void hud_ui_handbook_unlock_entry(const char* name)
{
    if (name != nullptr)
    {
        hud_ui_handbook_unlock_entry(crc_stri(name, 0));
    }
}

// 0x00849790 (:3064)
void hud_ui_handbook_unlock_bftp_entry(int index)
{
    if (Multiplayer || World.dlc_bundle_id != -1)
    {
        return;
    }
    handbook_node* node = hud_ui_handbook_find_node(index);
    if (node != nullptr && node->unlock_number < 0)
    {
        Unlock_counter++;
        Num_unlocked_bftp_entries++;
        node->unlock_number = Unlock_counter;
        hud_ui_handbook_set_page_as_newest(node);
    }
}
