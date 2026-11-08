// Original: volition/rfg/code/interface/ui_document_hud_tool_tips.cpp
//
// Tool tips (stub_impl.md S7): tool_tips.xtbl, the queue of four, the help document, the times each one was shown (in the
// save game; the tips marked Save_to_Profile in the player profile).

#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"

#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <string.h>
#include <wchar.h>

// ---- file-static globals
static const char TOOL_TIPS_FILE_NAME[15] = "tool_tips.xtbl"; // 0x0130104C
static const char TOOL_TIPS_ROOT_DATA_TAG[10] = "Tool_Tips"; // 0x0130105C
static const char TOOL_TIPS_NAME_TAG[5] = "Name"; // 0x01301068
static const char TOOL_TIPS_TITLE_TAG[6] = "Title"; // 0x01301070
static const char TOOL_TIPS_DESCRIPTION_TAG[12] = "Description"; // 0x01301078
static const char TOOL_TIPS_ICON_TAG[5] = "Icon"; // 0x01301084
static const char TOOL_TIPS_ANIMATION_TAG[10] = "Animation"; // 0x0130108C
static const char TOOL_TIPS_SCREEN_LOCATION_TAG[11] = "Screen_Loc"; // 0x01301098
static const char TOOL_TIPS_ARROW_LOCATION_TAG[10] = "Arrow_Loc"; // 0x013010A4
static const char TOOL_TIPS_PLAY_TIMES_TAG[11] = "Play_Times"; // 0x013010B0
static const char TOOL_TIPS_DURATION_TAG[9] = "Duration"; // 0x013010BC
static const char TOOL_TIPS_PAUSE_GAME_TAG[6] = "Pause"; // 0x013010C8
static const char TOOL_TIPS_RUN_PAUSED_TAG[11] = "Run_Paused"; // 0x013010D0
static const char TOOL_TIPS_MINOR_TIP_TAG[10] = "Minor_tip"; // 0x013010DC
static const char TOOL_TIPS_SAVE_TO_PROFILE_TAG[16] = "Save_to_Profile"; // 0x013010E8
static const char TOOL_TIPS_ALWAYS_SHOW_TAG[12] = "Always_Show"; // 0x013010F8
static const char TOOL_TIPS_STRING_POOL_NAME[22] = "Tool Tips String Pool"; // 0x01301104
static const char* TOOL_TIPS_LOCATION_NAMES[6] = {"North East", "North West", "South East", "South West", "Center", "None"}; // 0x01664AA4
static int Tool_tip_input_id = -1; // 0x01664ABC
static const char* cDocname_uitips = {"rfg_ui_hud_help"}; // 0x01664AC0
static const char* cEmptystring_uitips = {""}; // 0x01664AC4
int Doc_handle_uitips = -1; // 0x01664AC8
static int Data_handle = -1; // 0x01664ACC
unsigned char Tool_tips_times_displayed[128]; // 0x02BF4490 (external since UI6: the game options reset it)
bool Active; // 0x02C03754
// The tool tips option (the game options set it; UI.md UI6).
bool Tool_tips_enabled = true; // 0x01664A49
static char* Tool_tips_string_pool_data; // 0x02C08070
unsigned int Current_tool_tip; // 0x02C08074
static int Dialogue_just_finished; // 0x02C08078
static object_pool<tool_tip_base_data,128,1> Tool_tips_data; // 0x02C08420
tool_tip_instance Tool_tip_queue[4]; // 0x02C108D0
static string_pool Tool_tips_string_pool(TOOL_TIPS_STRING_POOL_NAME); // 0x02C520D0
static bool exit_was_down; // the resume button was seen down (tool_tips_queue_process)

// 0x008EB700
void unused_dcf_display_tool_tip()
{
    if (Console_command != 0)
    {
        if (console_get_arg(2, 1))
        {
            ui_tool_tips_add_tip(checksum_stri(Console_arg, 0), 0, nullptr);
            return;
        }
        console_output("Enter the name of the tool tip to display.", nullptr, false);
    }
}

// 0x008B2B40
void unused_dcf_reset_tool_tips()
{
    if (Console_command != 0)
    {
        memset(Tool_tips_times_displayed, 0, 0x80);
    }
}

// 0x00898950
event_result tool_tips_report_dialog_line_finished(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() == EVENT_DIALOGUE_FINISHED)
    {
        if (!((const dialogue_event&)in_event).is_start)
        {
            Dialogue_just_finished = 1;
        }
        return EVENT_OK;
    }
    return EVENT_IGNORED;
}

// 0x00898980
tool_tip_location get_tip_location_from_name(const char* name)
{
    for (int i = TOOL_TIP_LOCATION_NE; i < NUM_TOOL_TIP_LOCATIONS; i++)
    {
        if (v_stricmp(TOOL_TIPS_LOCATION_NAMES[i], name) == 0)
        {
            return (tool_tip_location)i;
        }
    }
    return INVALID_TOOL_TIP_LOCATION;
}

// 0x008C67A0
tool_tip_base_data* find_tool_tip(const checksum_stri& name_crc)
{
    if (name_crc == checksum_stri::INVALID)
    {
        return nullptr;
    }
    Tool_tips_data.thread_access.enter();
    for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr; tip = Tool_tips_data.get_next(tip))
    {
        if (tip->name_crc == name_crc)
        {
            Tool_tips_data.thread_access.leave();
            return tip;
        }
    }
    Tool_tips_data.thread_access.leave();
    return nullptr;
}

// 0x008989C0
tool_tip_instance* get_available_tool_tip_slot()
{
    unsigned int slot = Current_tool_tip;
    for (int i = 0; i < 4; i++)
    {
        if ((int)slot > 3)
        {
            slot = 0;
        }
        if (Tool_tip_queue[slot].base_tool_tip == nullptr)
        {
            return &Tool_tip_queue[slot];
        }
        slot++;
    }
    return nullptr;
}

// 0x008EB750
void load_tool_tips_data()
{
    if (Tool_tips_string_pool_data == nullptr)
    {
        // Magic number: the string pool's 6 KB.
        Tool_tips_string_pool_data = (char*)Permanent_mempool.alloc(0x1800, 0);
        Tool_tips_string_pool.init(Tool_tips_string_pool_data, 0x1800, TOOL_TIPS_STRING_POOL_NAME, 0, 0);
        Tool_tips_string_pool.clear();
        Tool_tips_data.thread_access.enter();
        for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr;)
        {
            tip = Tool_tips_data.free(tip);
        }
        Tool_tips_data.thread_access.leave();
    }
    xml_element* root = xml_table_parse(TOOL_TIPS_FILE_NAME);
    for (xml_element* element = xml_find(root, TOOL_TIPS_ROOT_DATA_TAG); element != nullptr; element = xml_find_next(root, element, TOOL_TIPS_ROOT_DATA_TAG))
    {
        tool_tip_base_data* tip = find_tool_tip(checksum_stri(xml_find(element, TOOL_TIPS_NAME_TAG)->text, 0));
        if (tip == nullptr)
        {
            tip = Tool_tips_data.alloc(0);
        }
        if (tip != nullptr)
        {
            tip->reset();
            if (!tip->load(element))
            {
                tip->reset();
            }
        }
    }
    xml_table_close(root);
}

// 0x008D9F00
void tool_tips_queue_process(float dtime)
{
    const unsigned int current = Current_tool_tip;
    tool_tip_instance* instance = &Tool_tip_queue[current];
    if (instance->base_tool_tip == nullptr)
    {
        return;
    }
    player* pp = player_get_local_player();
    const unsigned int player_handle = pp != nullptr ? pp->get_handle() : 0xFFFFFFFF;
    const tool_tip_base_data::tip_flags flags = instance->base_tool_tip->flags;
    if (!instance->running)
    {
        exit_was_down = false;
        const bool pause_ok = !game_is_paused() || flags.run_paused;
        const bool state_ok = (gameseq_get_pending_state() == GS_NONE && gameseq_get_state() == GS_GAMEPLAY) || flags.run_paused;
        const bool dialogue_ok = dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0 || Dialogue_just_finished != 0 || flags.run_paused || !flags.pause_game;
        // Original bug kept: the player is not checked here.
        const bool scope_ok = !player_is_scoped(pp) && !(pp->previous_zoom_state == PZS_SCOPE && camera_view_current_transition_time_left() != 0);
        if (pause_ok && state_ok && dialogue_ok && scope_ok && instance->delay_before_displaying.elapsed() && (Utility_doc_handle == -1 || Message_box_info_set.msgbox_info_pool.get_count(0) == 0) && !human_is_dead(player_handle))
        {
            tool_tip_base_data* tip = instance->base_tool_tip;
            if (tip->flags.minor_tip)
            {
                hud_message_add_minor(localization_get_display_string_opt(tip->description_crc), tip->duration, false);
                hud_ui_handbook_unlock_entry(tip->name_crc.get_crc());
                Current_tool_tip++;
                instance->reset();
                if (Current_tool_tip > 3)
                {
                    Current_tool_tip = 0;
                }
                return;
            }
            instance->running = true;
            player* local = player_get_local_player();
            const unsigned int source_handle = local != nullptr ? local->get_handle() : 0xFFFFFFFF;
            if (tip->flags.pause_game)
            {
                ovl_inventory_close();
                if (game_is_paused())
                {
                    Tool_tip_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, true);
                }
                else
                {
                    game_pause(false);
                }
                // Magic number: a paused tip takes a second before the resume button works.
                instance->time_left = 1.0f;
            }
            else
            {
                instance->time_left = tip->duration;
            }
            gameaudio_play_cross_linked_sound(Ga_cue_ids.general_tooltip_pop_up, source_handle, nullptr, nullptr);
        }
    }
    else if (instance->time_left <= 0.0f || pp == nullptr || pp->is_killed())
    {
        if (flags.pause_game)
        {
            if (controls_is_button_down(CBA_TOOLTIP_RESUME, Player_controller_index))
            {
                exit_was_down = true;
                return;
            }
            if (!exit_was_down)
            {
                return;
            }
            if (Tool_tip_input_id == -1)
            {
                if (game_is_paused())
                {
                    game_unpause();
                    gameaudio_play_cross_linked_sound(Ga_cue_ids.ui_sound_press_select, player_handle, nullptr, nullptr);
                }
            }
            else
            {
                ui_relinquish_input_id(Tool_tip_input_id);
                Tool_tip_input_id = -1;
                gameaudio_play_cross_linked_sound(Ga_cue_ids.ui_sound_press_select, player_handle, nullptr, nullptr);
            }
        }
        hud_ui_handbook_unlock_entry(instance->base_tool_tip->name_crc.get_crc());
        Current_tool_tip++;
        instance->reset();
        if (Current_tool_tip > 3)
        {
            Current_tool_tip = 0;
        }
    }
    else if (!game_is_paused() || flags.pause_game || flags.run_paused)
    {
        instance->time_left = instance->time_left - dtime;
    }
}

// 0x00898A00
void tool_tip_base_data::reset()
{
    name_crc = checksum_stri::INVALID;
    title = nullptr;
    description_crc = 0;
    icon = nullptr;
    animation = nullptr;
    screen_location = INVALID_TOOL_TIP_LOCATION;
    arrow_location = INVALID_TOOL_TIP_LOCATION;
    duration = 0.0f;
    num_times_to_play = 0;
    memset(&flags, 0, sizeof(flags));
}

// 0x008DA270
bool tool_tip_base_data::load(xml_element* xml_root)
{
    xml_element* element = xml_find(xml_root, TOOL_TIPS_NAME_TAG);
    if (element == nullptr || element->text == nullptr)
    {
        return false;
    }
    name_crc = checksum_stri(element->text, 0);
    element = xml_find(xml_root, TOOL_TIPS_TITLE_TAG);
    if (element == nullptr)
    {
        return false;
    }
    title = Tool_tips_string_pool.add_unique(element->text);
    if (title == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, TOOL_TIPS_DESCRIPTION_TAG);
    if (element == nullptr)
    {
        return false;
    }
    description_crc = localization_get_id_hash(element->text);
    char lower[0x100];
    element = xml_find(xml_root, TOOL_TIPS_ICON_TAG);
    if (element != nullptr)
    {
        str_copy_to_lower(lower, 0x100, element->text);
        icon = Tool_tips_string_pool.add_unique(lower);
    }
    element = xml_find(xml_root, TOOL_TIPS_ANIMATION_TAG);
    if (element != nullptr)
    {
        str_copy_to_lower(lower, 0x100, element->text);
        animation = Tool_tips_string_pool.add_unique(lower);
    }
    element = xml_find(xml_root, TOOL_TIPS_SCREEN_LOCATION_TAG);
    if (element != nullptr)
    {
        screen_location = get_tip_location_from_name(element->text);
    }
    if (screen_location == INVALID_TOOL_TIP_LOCATION)
    {
        screen_location = TOOL_TIP_LOCATION_C;
    }
    element = xml_find(xml_root, TOOL_TIPS_ARROW_LOCATION_TAG);
    if (element != nullptr)
    {
        arrow_location = get_tip_location_from_name(element->text);
    }
    if (arrow_location == INVALID_TOOL_TIP_LOCATION)
    {
        arrow_location = TOOL_TIP_LOCATION_C;
    }
    num_times_to_play = xml_get_int(xml_find(xml_root, TOOL_TIPS_PLAY_TIMES_TAG), 1);
    // Magic number: the default of 5 s; the table is in milliseconds.
    duration = (float)xml_get_int(xml_find(xml_root, TOOL_TIPS_DURATION_TAG), 5000) / 1000.0f;
    flags.pause_game = xml_get_bool(xml_find(xml_root, TOOL_TIPS_PAUSE_GAME_TAG), false);
    flags.run_paused = xml_get_bool(xml_find(xml_root, TOOL_TIPS_RUN_PAUSED_TAG), false);
    flags.minor_tip = xml_get_bool(xml_find(xml_root, TOOL_TIPS_MINOR_TIP_TAG), false);
    flags.save_to_profile = xml_get_bool(xml_find(xml_root, TOOL_TIPS_SAVE_TO_PROFILE_TAG), false);
    flags.always_show = xml_get_bool(xml_find(xml_root, TOOL_TIPS_ALWAYS_SHOW_TAG), false);
    return true;
}

// 0x00910D10
void ui_tool_tips_doc_init()
{
    const bool locked = cf_is_locked();
    if (locked)
    {
        cf_open_lock(false);
    }
    Doc_handle_uitips = ui_load_document(cDocname_uitips, DFIT_NONE, 0.0f, DLT_PERMANENTLY_LOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    vint_data_item* item = vint_data_item::create_new("HelpUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[10] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 10);
        Data_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uitips, Data_handle);
    }
    if (locked)
    {
        cf_open_lock(true);
    }
}

// 0x008EB900
void ui_tool_tip_process_info()
{
    vint_data_item* item = vint_data_item::find(Data_handle);
    if (item == nullptr)
    {
        return;
    }
    tool_tip_instance* instance = &Tool_tip_queue[Current_tool_tip];
    tool_tip_base_data* tip = instance->base_tool_tip;
    if (tip == nullptr || !instance->running)
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(0u));
        item->set_element(2, vint_variant(0u));
        item->set_element(3, vint_variant(cEmptystring_uitips));
        item->set_element(4, vint_variant(cEmptystring_uitips));
        item->set_element(5, vint_variant(1));
        item->set_element(6, vint_variant(1));
        item->set_element(7, vint_variant(0.0f));
        item->set_element(8, vint_variant(cEmptystring_uitips));
        item->set_element(9, vint_variant(false));
        return;
    }
    const char* icon = tip->icon != nullptr ? tip->icon : cEmptystring_uitips;
    if (instance->override_icon[0] != '\0')
    {
        icon = instance->override_icon;
    }
    // Original bug kept: the duration is already in seconds here, divided again.
    float duration = tip->duration / 1000.0f;
    const char* animation = tip->animation != nullptr ? tip->animation : cEmptystring_uitips;
    bool show_resume = false;
    if (tip->flags.pause_game)
    {
        duration = -1.0f;
        show_resume = instance->time_left <= 0.0f;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant(localization_get_id_hash(tip->title)));
    item->set_element(2, vint_variant(tip->description_crc));
    item->set_element(3, vint_variant(icon));
    item->set_element(4, vint_variant(animation));
    // The document's locations count from 1.
    item->set_element(5, vint_variant((int)tip->screen_location + 1));
    item->set_element(6, vint_variant((int)tip->arrow_location + 1));
    item->set_element(7, vint_variant(duration));
    char encoded[0x180];
    const char* resume_text = cEmptystring_uitips;
    if (show_resume)
    {
        // The press start text with the start button replaced by the tool tip's resume button.
        wchar_t press_start[0x80];
        wchar_t replaced[0x80];
        wcsncpy(press_start, localization_get_display_string_req("HM_PRESS_START", nullptr), 0x80);
        const wchar_t* src = press_start;
        wchar_t* dst = replaced;
        // Original bug kept: the loop counts source characters but a replacement skips 19 of them at one count, so it
        // copies on past the terminator (the zeros wcsncpy left) for as many characters as were skipped.
        for (int i = 0; i < (int)wcslen(press_start); i++)
        {
            if (_v_strnicmp(src, L"[button:menu_start]", 0x13) == 0)
            {
                memcpy(dst, L"[button:tooltip_resume]", 23 * sizeof(wchar_t));
                dst += 23;
                src += 0x13;
            }
            else
            {
                *dst++ = *src++;
            }
        }
        *dst = L'\0';
        vint_encode_wide_text(encoded, 0x180, replaced);
        resume_text = encoded;
    }
    item->set_element(8, vint_variant(resume_text));
    item->set_element(9, vint_variant((bool)tip->flags.pause_game));
}

// 0x00910E00
void ui_tool_tips_init()
{
    ui_tool_tips_doc_init();
}

// 0x008EBE10
void ui_tool_tips_init_2()
{
    memset(Tool_tips_times_displayed, 0, 0x80);
    const bool locked = cf_is_locked();
    if (locked)
    {
        cf_open_lock(false);
    }
    load_tool_tips_data();
    if (locked)
    {
        cf_open_lock(true);
    }
}

// 0x008DA4F0
void ui_tool_tips_add_tip(const char* name, const int delay_ms, const char* icon_image)
{
    if (name != nullptr)
    {
        ui_tool_tips_add_tip(checksum_stri(name, 0), delay_ms, icon_image);
    }
}

// 0x008C6820
void ui_tool_tips_add_tip(const checksum_stri& name_crc, const int delay_ms, const char* icon_image)
{
    if (name_crc == checksum_stri::INVALID)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->is_killed())
    {
        return;
    }
    tool_tip_base_data* tip = find_tool_tip(name_crc);
    if (tip == nullptr || dlc_get_tooltips_disabled())
    {
        return;
    }
    if (!Tool_tips_enabled && !tip->flags.always_show)
    {
        if (tip->num_times_to_play != 0)
        {
            hud_ui_handbook_unlock_entry(tip->name_crc.get_crc());
        }
        return;
    }
    // In multiplayer and wrecking crew only the tips kept in the profile show.
    if ((Multiplayer || wrecking_crew_is_active()) && !tip->flags.save_to_profile)
    {
        return;
    }
    tool_tip_instance* instance = get_available_tool_tip_slot();
    if (instance == nullptr)
    {
        return;
    }
    instance->base_tool_tip = tip;
    const int index = Tool_tips_data.get_index(tip);
    if (tip->num_times_to_play > -1 && tip->num_times_to_play <= (int)Tool_tips_times_displayed[index])
    {
        instance->reset();
        return;
    }
    Tool_tips_times_displayed[index]++;
    if (icon_image != nullptr)
    {
        strncpy(instance->override_icon, icon_image, 0x20);
    }
    if (delay_ms != 0 && instance->base_tool_tip->flags.run_paused)
    {
        // A delay does not count while paused.
        instance->delay_before_displaying.set(0);
    }
    else
    {
        instance->delay_before_displaying.set(delay_ms);
    }
    instance->running = false;
}

// 0x008B2B60
void ui_tool_tips_remove_tip(const char* name)
{
    if (name != nullptr)
    {
        ui_tool_tips_remove_tip(checksum_stri(name, 0));
    }
}

// 0x00898A30
void ui_tool_tips_remove_tip(const checksum_stri& name_crc)
{
    if (name_crc == checksum_stri::INVALID)
    {
        return;
    }
    unsigned int slot = Current_tool_tip;
    for (int i = 0; ; i++)
    {
        if (Tool_tip_queue[slot].base_tool_tip != nullptr && Tool_tip_queue[slot].base_tool_tip->name_crc == name_crc)
        {
            break;
        }
        slot++;
        if ((int)slot > 3)
        {
            slot = 0;
        }
        if (i >= 3)
        {
            return;
        }
    }
    if (Tool_tip_queue[slot].base_tool_tip->flags.pause_game && game_is_paused())
    {
        game_unpause();
    }
    if (slot == Current_tool_tip)
    {
        Tool_tip_queue[slot].time_left = 0.0f;
        return;
    }
    Tool_tip_queue[slot].reset();
    // Closes the gap: the slots after it move down, up to the one before the current.
    int last = (int)Current_tool_tip - 1;
    if (last < 0)
    {
        last = 3;
    }
    while ((int)slot != last)
    {
        unsigned int next = slot + 1;
        if ((int)next > 3)
        {
            next = 0;
        }
        Tool_tip_queue[slot] = Tool_tip_queue[next];
        slot = next;
    }
}

// 0x008B2B80
bool ui_tool_tips_tip_is_queued(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }
    return ui_tool_tips_tip_is_queued(checksum_stri(name, 0));
}

// 0x00898B50
bool ui_tool_tips_tip_is_queued(const checksum_stri& name_crc)
{
    if (name_crc != checksum_stri::INVALID)
    {
        for (const tool_tip_instance& instance : Tool_tip_queue)
        {
            if (instance.base_tool_tip != nullptr && instance.base_tool_tip->name_crc == name_crc)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x008DA520
bool ui_tool_tips_tip_has_been_played(const char* name, bool no_more_plays_remaining)
{
    if (name == nullptr)
    {
        return false;
    }
    return ui_tool_tips_tip_has_been_played(checksum_stri(name, 0), no_more_plays_remaining);
}

// 0x008C6980
bool ui_tool_tips_tip_has_been_played(const checksum_stri& name_crc, bool no_more_plays_remaining)
{
    tool_tip_base_data* tip = find_tool_tip(name_crc);
    if (tip != nullptr)
    {
        const int played = Tool_tips_times_displayed[Tool_tips_data.get_index(tip)];
        if ((played != 0 && !no_more_plays_remaining) || (tip->num_times_to_play <= played && no_more_plays_remaining))
        {
            return true;
        }
    }
    return false;
}

// 0x00898B80
bool ui_tool_tips_tips_are_enabled()
{
    return !dlc_get_tooltips_disabled() && Tool_tips_enabled;
}

// A running tip that holds the game is processed and drawn while paused.
static bool tool_tips_doc_runs()
{
    const tool_tip_instance& instance = Tool_tip_queue[Current_tool_tip];
    return !game_is_paused() || (instance.running && instance.base_tool_tip != nullptr && (instance.base_tool_tip->flags.pause_game || instance.base_tool_tip->flags.run_paused));
}

// 0x008EBFE0
void ui_tool_tips_process(float dtime)
{
    if (!Active && game_save_get_state() == GSS_NONE)
    {
        tool_tips_queue_process(dtime);
        ui_tool_tip_process_info();
        vint_doc_info* info = ui_get_doc_info(Doc_handle_uitips);
        if (info != nullptr && info->m_doc != nullptr && tool_tips_doc_runs())
        {
            info->m_doc->process(dtime);
        }
    }
    if (Dialogue_just_finished != 0)
    {
        Dialogue_just_finished = 0;
    }
}

// 0x008DA590
void ui_tool_tips_render()
{
    if (Active)
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_uitips);
    if (info != nullptr && info->m_doc != nullptr && tool_tips_doc_runs())
    {
        info->m_doc->render();
    }
}

// 0x00898BA0
void ui_tool_tips_reset_data()
{
    memset(Tool_tips_times_displayed, 0, 0x80);
}

// 0x008DA5F0
void ui_tool_tips_level_init_pre()
{
    notify_info info;
    info.data = nullptr;
    info.callback = tool_tips_report_dialog_line_finished;
    info.type = EVENT_DIALOGUE_FINISHED;
    gem_register_for_event(info);
    ui_tool_tips_system_load_from_profile();
}

// 0x00898BC0
void ui_tool_tips_level_shutdown()
{
    memset(Tool_tips_times_displayed, 0, 0x80);
    for (tool_tip_instance& instance : Tool_tip_queue)
    {
        instance.reset();
    }
    Current_tool_tip = 0;
}

// 0x008F5A70
void ui_tool_tips_shutdown()
{
    ui_tool_tips_level_shutdown();
    if (Doc_handle_uitips != -1)
    {
        ui_unload_document(Doc_handle_uitips, DFOT_NONE, 0.5f);
        Doc_handle_uitips = -1;
    }
}

// 0x008C69E0
void ui_tool_tips_system_save(cfile* save_file)
{
    // Magic number: the 128 tips of the pool.
    cf_write_uint8(0x80, save_file);
    int written = 0;
    Tool_tips_data.thread_access.enter();
    for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr; tip = Tool_tips_data.get_next(tip))
    {
        if (!tip->flags.save_to_profile)
        {
            cf_write_uint8(Tool_tips_times_displayed[Tool_tips_data.get_index(tip)], save_file);
            written++;
        }
    }
    Tool_tips_data.thread_access.leave();
    for (; written < 0x80; written++)
    {
        cf_write_uint8(0, save_file);
    }
}

// 0x008C6AD0
void ui_tool_tips_system_load(cfile* save_file)
{
    if (cf_read_uint8(save_file) != 0x80)
    {
        return;
    }
    int read = 0;
    Tool_tips_data.thread_access.enter();
    for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr; tip = Tool_tips_data.get_next(tip))
    {
        if (!tip->flags.save_to_profile)
        {
            Tool_tips_times_displayed[Tool_tips_data.get_index(tip)] = cf_read_uint8(save_file);
            read++;
        }
    }
    Tool_tips_data.thread_access.leave();
    for (; read < 0x80; read++)
    {
        cf_read_uint8(save_file);
    }
}

// 0x008C6BC0
void ui_tool_tips_system_save_to_profile()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return;
    }
    player_profile_data* profile = player_profile_get("tooltips", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
    if (profile == nullptr)
    {
        return;
    }
    int count = 0;
    Tool_tips_data.thread_access.enter();
    for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr; tip = Tool_tips_data.get_next(tip))
    {
        if (tip->flags.save_to_profile)
        {
            profile->tool_tip_info[count] = Tool_tips_times_displayed[Tool_tips_data.get_index(tip)];
            count++;
        }
    }
    Tool_tips_data.thread_access.leave();
    if (count < 0x80)
    {
        memset(profile->tool_tip_info + count, 0, 0x80 - count);
    }
}

// 0x008C6CA0
void ui_tool_tips_system_load_from_profile()
{
    player_profile_data* profile = player_profile_get("tooltips", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
    if (profile == nullptr)
    {
        return;
    }
    int count = 0;
    Tool_tips_data.thread_access.enter();
    for (tool_tip_base_data* tip = Tool_tips_data.get_first(0); tip != nullptr; tip = Tool_tips_data.get_next(tip))
    {
        if (tip->flags.save_to_profile)
        {
            Tool_tips_times_displayed[Tool_tips_data.get_index(tip)] = profile->tool_tip_info[count];
            count++;
        }
    }
    Tool_tips_data.thread_access.leave();
}

// 0x00898C50
bool ui_tool_tips_is_active()
{
    return Tool_tip_queue[Current_tool_tip].running;
}

// 0x008B2BC0
const tool_tip_base_data* ui_tool_tips_get_first_tip()
{
    return Tool_tips_data.get_first(0);
}

// 0x008C6D80
const tool_tip_base_data* ui_tool_tips_get_next_tip(const tool_tip_base_data* current_tip)
{
    return Tool_tips_data.get_next(current_tip);
}

// 0x00888510
void tool_tip_instance::reset()
{
    base_tool_tip = nullptr;
    override_icon[0] = '\0';
    time_left = -1.0f;
    running = false;
}

// 0x00888530
tool_tip_instance::tool_tip_instance()
{
    reset();
}

// 0x008AAC00
tool_tip_base_data::tool_tip_base_data()
{
    reset();
}
