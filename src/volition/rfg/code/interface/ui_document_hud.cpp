// Original: volition/rfg/code/interface/ui_document_hud.cpp
//
// The HUD documents (stub_impl.md S7): rfg_ui_hud_health, rfg_ui_hud (not in multiplayer and wrecking crew) and
// rfg_ui_messages, their data items, the three message pools (major, minor, permanent/user), the radio messages with
// their three-part sound, the item pickups, the swap prompt and the module documents (satellite, rail driver, sniper).

#include "volition/rfg/code/interface/ui_document_hud.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/animation/vint_object_animation.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_group.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_text.h"
#include "volition/interface_lib/code/resources/vint_resource_font.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_heartbeat_effect.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_internal.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_satellite.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/vfont.h"

#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// size 0x4
union swap_info_handle
{
    unsigned int w_info_handle; // 0x00
    int backpack_type; // 0x00
};
static_assert(sizeof(swap_info_handle) == 0x4, "swap_info_handle");

// size 0x14
struct swap_info_set
{

    bool m_show_swap_menu; // 0x00
    bool m_weapon_swap; // 0x01
    swap_info_handle m_primary_slot; // 0x04
    swap_info_handle m_secondary_slot; // 0x08
    vector2 m_screen_pos; // 0x0C
};
static_assert(sizeof(swap_info_set) == 0x14, "swap_info_set");

// The radio message's states (m_state). Magic numbers of rfg.exe, no enum in rfg.pdb.
enum
{
    RMS_NONE = 0,
    RMS_START_INTRO = 1,
    RMS_INTRO = 2,
    RMS_START_MESSAGE = 3,
    RMS_MESSAGE = 4,
    RMS_START_OUTRO = 5,
    RMS_OUTRO = 6,
    RMS_DONE = 7,
};

// ---- file-static globals
static const char* Hud_doc_names[3] = {"rfg_ui_hud_health", "rfg_ui_hud", "rfg_ui_messages"}; // 0x016642CC
static float Radio_persona_freq[19] = {0.0f, 641.099976f, 327.5f, 999.900024f, 145.699997f, 354.299988f, 374.700012f, 248.300003f, 678.299988f, 783.299988f, 269.100006f, 269.100006f, 269.100006f, 974.299988f, 741.900024f, 432.700012f, 634.700012f, 374.100006f, 427.5f}; // 0x016642D8
// The .data defaults (English); the localized names from Radio_persona_display_names_source replace them.
static char Radio_persona_display_names[19][64] = {"MENU_RADIO_MESSAGE", "VASHA", "CARMEN", "COOT", "DAN", "GALLO",
    "JENKINS", "RF COMMANDER", "GENERAL ROTH", "ADMIRAL KOBEL", "SAMANYA", "SAMANYA", "SAMANYA", "OFFICER", "BROGA",
    "KEPLER", "GUNNARSEN", "COMLINK", "MASON"}; // 0x01664328
static const char* Radio_persona_display_names_source[19] = {"MENU_RADIO_MESSAGE", "HM_RADIO_NAME_VASHA", "HM_RADIO_NAME_CARMEN", "HM_RADIO_NAME_COOT", "HM_RADIO_NAME_DAN", "HM_RADIO_NAME_GALLO", "HM_RADIO_NAME_JENKINS", "HM_RADIO_NAME_RFC", "HM_RADIO_NAME_GENERAL_ROTH", "HM_RADIO_NAME_ADMIRAL_KOBEL", "HM_RADIO_NAME_SAMANYA", "HM_RADIO_NAME_SAMANYA", "HM_RADIO_NAME_SAMANYA", "HM_RADIO_NAME_OFFICER", "HM_RADIO_NAME_BROGA", "HM_RADIO_NAME_KEPLER", "HM_RADIO_NAME_GUNNARSEN", "HM_RADIO_NAME_COMLINK", "HM_RADIO_NAME_MASON"}; // 0x016647E8
static const char* cUI_Hud_Group = {"hud_group"}; // 0x01664834
static const char* cUI_Radio_Group = {"radio_main"}; // 0x01664838
static const char* cUI_Visible = {"visible"}; // 0x0166483C
static const wchar_t* cEmptyString = {L""}; // 0x01664840
static const char* cBlankString = {" "}; // 0x01664848
static const char* cProp_Scale = {"scale"}; // 0x0166484C
static const char* cProp_screen_size_uihud = {"screen_size"}; // 0x01664850
static const char* cGroupMajor = {"usage_text_major_group1"}; // 0x01664854
static const char* cGroupMinor = {"usage_text_minor_group1"}; // 0x01664858
static const char* cGroupPerm = {"usage_text_perm_group1"}; // 0x0166485C
static const char* cTextMajor = {"usage_text_major1"}; // 0x01664860
static const char* cTextMinor = {"usage_text_minor1"}; // 0x01664864
static const char* cTextPerm = {"usage_text_perm1"}; // 0x01664868
static const char* cAnimInMinor = {"usage_text_minor_anim_in"}; // 0x0166486C
static const char* cAnimInMinor2 = {"usage_text_minor_anim_in_2"}; // 0x01664870
static const char* cAnimMinor = {"usage_text_minor_anim"}; // 0x01664874
static const char* cAnimMinor2 = {"usage_text_minor_anim_2"}; // 0x01664878
static const char* cAnimOutMinor = {"usage_text_minor_anim_out"}; // 0x0166487C
static const char* cAnimOutMinor2 = {"usage_text_minor_anim_out_2"}; // 0x01664880
static const char* cBgMinor2 = {"minor_line2"}; // 0x01664884
static const char* cFont = {"font"}; // 0x01664888
static const char* cFullmsg = {"MENU_FULL"}; // 0x0166488C
static bool Enabled = true; // 0x01664894
static int Doc_handles[3] = {-1, -1, -1}; // 0x01664898
static int Data_handles_uihud[15] = {16, 16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 0x016648A4
static float Minimum_perm_message_y = 0.8f; // 0x016648E0
static int Control_current_value = -1; // 0x016648E4
static int Control_delta_value = -1; // 0x016648E8
static int Control_old_value = -1; // 0x016648EC
static float Control_current_percent = -1.0f; // 0x016648F0
static float Control_delta_percent = -1.0f; // 0x016648F4
static unsigned int Control_district_handle = 0xFFFFFFFF; // 0x016648F8
static float Morale_current_percent = -1.0f; // 0x016648FC
static float Morale_old_percent = -1.0f; // 0x01664900
static float Morale_change = -1.0f; // 0x01664904
static unsigned int Hud_message_counter = 0xFFFFFFFF; // 0x01664908
static float Previous_health = 1.0f; // 0x0166490C
static char Encoded_Subtitle_Persona_Name[64]; // 0x02BF2190
static wchar_t Subtitle_Persona_Name[64]; // 0x02BF5368
extern bool Radio_persona_names_fixed_up; // 0x02C07FEE (localization_shared.cpp)
static bool Unloaded; // 0x02C07FEF
static const char* cAnimInMajor; // 0x02C07FF0
static const char* cAnimInMajor2; // 0x02C07FF4
static const char* cAnimMajor; // 0x02C07FF8
static const char* cAnimMajor2; // 0x02C07FFC
static const char* cAnimOutMajor; // 0x02C08000
static const char* cAnimOutMajor2; // 0x02C08004
static const char* cBgMajor2; // 0x02C08008
static bool Active_uihud; // 0x02C0800C
static bool Data_items_created_uihud; // 0x02C0800D
static bool Cant_load; // 0x02C0800E
static bool Control_value_updated; // 0x02C0800F
static hud_modules Current_module; // 0x02C08010
static unsigned int Control_id_uihud; // 0x02C08014
static int Control_num_missions; // 0x02C08018
static int Control_missions_left; // 0x02C0801C
static int Control_missions_locked; // 0x02C08020
static int Control_missions_unlocked; // 0x02C08024
static bool Control_mission_complete; // 0x02C08028
static bool Morale_percent_updated; // 0x02C08029
static bool In_radiation; // 0x02C0802A
static unsigned int Control_morale_district_hash; // 0x02C0802C
static float User_message_base_scale; // 0x02C08030
static const char* Current_radio_icon_name; // 0x02C08034
static bool Unloaded_assets_only; // 0x02C082DC
static radio_msg_info_set Radio_set; // 0x02C09AE0
static swap_info_set Swap_menu_set; // 0x02C3D10C
static hud_ui_message_set Ui_message_set; // 0x02C43CF0
static item_info_set Item_pickup_set; // 0x02C53FE8
// 0x02CA1470: its dynamic initializer (0x0106E790).
static unsigned int Radio_persona_lookup_crc[19] = {crc_str("", 0), crc_str("VASHA", 0), crc_str("CARMEN", 0), crc_str("COOT", 0), crc_str("DAN", 0), crc_str("GALLO", 0), crc_str("JORGE", 0), crc_str("RFC", 0), crc_str("GENERAL", 0), crc_str("ADMIRAL", 0), crc_str("TECH", 0), crc_str("SAFE", 0), crc_str("TIMED", 0), crc_str("OFFICER", 0), crc_str("BROGA", 0), crc_str("DRIVER", 0), crc_str("GUNNAR", 0), crc_str("COMLINK", 0), crc_str("PCM01", 0)};
static cs_wrapper radio_message_cs(0); // guards Radio_set; the radio audio callbacks come from the audio thread

// The ids of radio messages: the pool index in the low 4 bits, a 12-bit rolling count above it.
static unsigned short radio_message_id(const radio_message_info* rmi)
{
    return (unsigned short)((Radio_set.m_radio_info_pool.get_index(rmi) & 0xF) | (rmi->m_rolling_count << 4));
}

// A message element's virtuals, called directly by rfg.exe (vtbl+0x38, +0x48, +0x60, +0x68, +0xD8, +0xE0, +0xE8, +0x120).
static void set_visible(vint_object_base* object, bool visible)
{
    ((vint_element_base*)object)->set_visible(vint_variant(visible), 0);
}

static void set_alpha(vint_object_base* object, float alpha)
{
    ((vint_element_base*)object)->set_alpha(vint_variant(alpha), 0);
}

// 0x00894120
void radio_message_info::clear()
{
    const short rolling_count = m_rolling_count;
    memset(this, 0, sizeof(radio_message_info));
    m_dirty = true;
    m_msg_sound_cue_id = -1;
    m_intro_sound_play_id = -1;
    m_msg_sound_play_id = -1;
    m_outro_sound_play_id = -1;
    // Magic number: 12 bits of the id; 0 stays the invalid id.
    const short next = (short)((rolling_count + 1) & 0xFFF);
    m_char_uncover_time = 0.06f;
    m_state = RMS_NONE;
    m_rolling_count = next;
    if (next == 0)
    {
        m_rolling_count = 1;
    }
}

// 0x00894190
void radio_message_info::drop()
{
    short next = (short)((m_rolling_count + 1) & 0xFFF);
    if (next == 0)
    {
        next = 1;
    }
    memset(this, 0, sizeof(radio_message_info));
    m_rolling_count = next;
    clear();
}

// 0x008941E0
void message_info::clear()
{
    m_width = 0;
    m_data1 = nullptr;
    m_data2 = nullptr;
    m_audio_cue = nullptr;
    m_running_time = 0.0f;
    m_dirty = true;
    // Magic number: one frame at 30 Hz.
    m_display_time = 0.033333335f;
    m_handle = INVALID_HUD_MESSAGE_HANDLE;
    _snprintf_safe(m_text, 0x200, cEmptyString);
}

// 0x008E8100
void unused_dcf_add_hud_message()
{
    if (Console_command != 0)
    {
        if (console_get_arg(2, 1))
        {
            wchar_t text[0x100];
            copy_to_wide(text, Console_arg, 0x100);
            ui_add_primary_message(text, 10.0f, nullptr, nullptr, nullptr);
        }
        console_output("Enter a string to be displayed on the HUD.", nullptr, false);
    }
}

// 0x008E81A0
void unused_dcf_add_secondary_hud_message()
{
    if (Console_command != 0)
    {
        if (console_get_arg(2, 1))
        {
            wchar_t text[0x100];
            copy_to_wide(text, Console_arg, 0x100);
            ui_add_secondary_message(text, 10.0f, false, false);
        }
        console_output("Enter a string to be displayed on the HUD as a secondary message.", nullptr, false);
    }
}

// 0x008E8240
void unused_dcf_add_secondary_hud_message_special()
{
    if (Console_command != 0)
    {
        if (console_get_arg(2, 1))
        {
            wchar_t text[0x100];
            copy_to_wide(text, Console_arg, 0x100);
            ui_add_secondary_message(text, 10.0f, true, false);
        }
        console_output("Enter a string to be displayed on the HUD as a secondary message.", nullptr, false);
    }
}

// 0x008940E0
void unused_dcf_morale_test_mission_unlock()
{
    Control_value_updated = true;
    Control_num_missions = 4;
    Control_missions_left = 3;
    Control_missions_locked = 2;
    Control_missions_unlocked = 1;
    Control_mission_complete = false;
}

// 0x008B0D70
radio_message_info* RadioMsgID_to_RadioMsgInfo(unsigned short id)
{
    if (id != 0 && (unsigned int)(id & 0xF) < Radio_set.m_radio_info_pool.get_capacity())
    {
        radio_message_info* rmi = Radio_set.m_radio_info_pool.get_entry(id & 0xF);
        if (rmi != nullptr && rmi->m_rolling_count == (short)(id >> 4))
        {
            return rmi;
        }
    }
    return nullptr;
}

// 0x00894240
static void ui_init_radio_msg_info_set()
{
}

// 0x00894250
static void ui_init_hud_ui_message_set()
{
    Ui_message_set.m_msg_doc = nullptr;
    Ui_message_set.m_primary_font = -1;
    Ui_message_set.m_secondary_font = -1;
    Ui_message_set.m_user_font = -1;
    Ui_message_set.m_primary_scale.x = 1.0f;
    Ui_message_set.m_primary_scale.y = 1.0f;
    Ui_message_set.m_secondary_scale.x = 1.0f;
    Ui_message_set.m_secondary_scale.y = 1.0f;
    Ui_message_set.m_user_scale.x = 1.0f;
    Ui_message_set.m_user_scale.y = 1.0f;
}

template<int N>
static void free_all_messages(object_pool<message_info, N, 1>& pool)
{
    pool.thread_access.enter();
    for (message_info* message = pool.get_first(0); message != nullptr;)
    {
        message = pool.free(message);
    }
    pool.thread_access.leave();
}

// 0x008E82E0
static void ui_init_use_messages()
{
    free_all_messages(Ui_message_set.m_primary_pool);
    free_all_messages(Ui_message_set.m_secondary_pool);
    free_all_messages(Ui_message_set.m_user_pool);
}

// 0x008942B0
static void ui_init_swap_menu_set()
{
    Swap_menu_set.m_show_swap_menu = false;
    Swap_menu_set.m_weapon_swap = true;
    Swap_menu_set.m_primary_slot.w_info_handle = 0xFFFFFFFF;
    Swap_menu_set.m_secondary_slot.w_info_handle = 0xFFFFFFFF;
    Swap_menu_set.m_screen_pos.x = 0.0f;
    Swap_menu_set.m_screen_pos.y = 0.0f;
}

// 0x008942E0
static void ui_init_my_fonts()
{
    if (Ui_message_set.m_msg_doc == nullptr)
    {
        return;
    }
    Ui_message_set.m_user_font = -1;
    Ui_message_set.m_secondary_font = -1;
    Ui_message_set.m_primary_font = -1;
    vint_object_base* major = vint_object_base::find_object(cTextMajor, Ui_message_set.m_msg_doc);
    vint_object_base* minor = vint_object_base::find_object(cTextMinor, Ui_message_set.m_msg_doc);
    vint_object_base* perm = vint_object_base::find_object(cTextPerm, Ui_message_set.m_msg_doc);
    if (major != nullptr)
    {
        vint_variant font;
        major->get_property_value(cFont, font, 0);
        vint_resource_font* resource = vint_resource_font::find((unsigned int)font);
        if (resource != nullptr)
        {
            Ui_message_set.m_primary_font = resource->get_title_handle();
        }
        vint_variant scale;
        ((vint_element_text*)major)->get_text_scale(scale, 0);
        Ui_message_set.m_primary_scale = scale.get_type() == VINT_PROP_TYPE_VECTOR2F ? (vector2)scale : vector2(0.0f, 0.0f);
    }
    if (minor != nullptr)
    {
        vint_variant font;
        minor->get_property_value(cFont, font, 0);
        vint_resource_font* resource = vint_resource_font::find((unsigned int)font);
        if (resource != nullptr)
        {
            Ui_message_set.m_secondary_font = resource->get_title_handle();
        }
        vint_variant scale;
        ((vint_element_text*)minor)->get_text_scale(scale, 0);
        Ui_message_set.m_secondary_scale = scale.get_type() == VINT_PROP_TYPE_VECTOR2F ? (vector2)scale : vector2(0.0f, 0.0f);
    }
    if (perm != nullptr)
    {
        vint_variant font;
        perm->get_property_value(cFont, font, 0);
        vint_resource_font* resource = vint_resource_font::find((unsigned int)font);
        if (resource != nullptr)
        {
            Ui_message_set.m_user_font = resource->get_title_handle();
        }
        Ui_message_set.m_user_scale = Ui_message_set.m_primary_scale;
    }
}

// 0x00894520
void ui_document_hud_clear_persona_language_prebuild_flag()
{
    Radio_persona_names_fixed_up = false;
}

// 0x008E83B0
void ui_hud_fix_up_persona_names()
{
    if (Radio_persona_names_fixed_up)
    {
        return;
    }
    for (int i = 1; i < 19; i++)
    {
        wchar_t name[64] = {};
        // A reversed (right to left) font starts the text with a carriage return.
        int start = 0;
        if (Body_font != -1)
        {
            const font* body = font_get_font(Body_font);
            if (body != nullptr && body->is_reversed != 0)
            {
                name[0] = L'\r';
                start = 1;
            }
        }
        const char* source = Radio_persona_display_names_source[i];
        if (source != nullptr)
        {
            const wchar_t* localized = localization_get_display_string_opt(source);
            if (localized == nullptr)
            {
                copy_to_wide(name + start, source, 0x40 - start);
            }
            else
            {
                wcsncpy(name + start, localized, 0x40 - start);
            }
        }
        vint_encode_wide_text(Radio_persona_display_names[i], 0x40, name);
    }
    Radio_persona_names_fixed_up = true;
}

// 0x008D7390
float hud_ui_get_radio_persona_radio_freq(const char* persona_name)
{
    if (persona_name != nullptr)
    {
        const unsigned int crc = crc_str(persona_name, 0);
        for (int i = 1; i < 19; i++)
        {
            if (Radio_persona_lookup_crc[i] == crc)
            {
                return Radio_persona_freq[i];
            }
        }
        if (localization_get_display_string_opt(persona_name) != nullptr)
        {
            // Magic number: any other localized persona gets a frequency from its name.
            return (float)(crc % 1000) / 10.0f;
        }
    }
    return Radio_persona_freq[0];
}

// 0x008E8520
const char* hud_ui_get_radio_persona_name(const char* persona_name, bool* found)
{
    if (!Radio_persona_names_fixed_up)
    {
        ui_hud_fix_up_persona_names();
    }
    if (found != nullptr)
    {
        *found = false;
    }
    if (persona_name == nullptr)
    {
        return Radio_persona_display_names[0];
    }
    const unsigned int crc = crc_str(persona_name, 0);
    for (int i = 1; i < 19; i++)
    {
        if (Radio_persona_lookup_crc[i] == crc)
        {
            if (found != nullptr)
            {
                *found = true;
            }
            return Radio_persona_display_names[i];
        }
    }
    const wchar_t* localized = localization_get_display_string_opt(persona_name);
    if (localized == nullptr)
    {
        return Radio_persona_display_names[0];
    }
    if (found != nullptr)
    {
        *found = true;
    }
    wcsncpy(Subtitle_Persona_Name, localized, 0x40);
    vint_encode_wide_text(Encoded_Subtitle_Persona_Name, 0x40, Subtitle_Persona_Name);
    return Encoded_Subtitle_Persona_Name;
}

// 0x008E8640
void hud_ui_get_subtitle_display_text(unsigned int text_crc, const char* persona_name, bool show_name, char* output_string, int output_size)
{
    if (output_string == nullptr || output_size < 1)
    {
        return;
    }
    char name_text[0x140] = {};
    wchar_t text[0x140] = {};
    output_string[0] = '\0';
    if (show_name && persona_name != nullptr && strlen(persona_name) > 1)
    {
        // The colon between the name and the line; the carriage returns keep it left to right.
        wchar_t separator[0x10];
        copy_to_wide(separator, "\r:\r  \r", 0x10);
        bool found = false;
        const char* display_name = hud_ui_get_radio_persona_name(persona_name, &found);
        if (found && display_name != nullptr)
        {
            vint_decode_wide_text(text, 0x280, display_name);
            wcsncat(text, separator, 0x280);
        }
        // Magic number: 0x80 starts vint's encoded wide text.
        else if (!found && (unsigned char)persona_name[0] == 0x80)
        {
            vint_decode_wide_text(text, 0x280, persona_name);
            wcsncat(text, separator, 0x280);
        }
        else
        {
            strncpy(name_text, persona_name, 0x140);
            strncat(name_text, ": ", 2);
            copy_to_wide(text, name_text, 0x280);
        }
    }
    const wchar_t* line = text_crc != 0 && !s_localization_is_loading ? localization_get_display_string_opt(text_crc) : nullptr;
    if (line != nullptr)
    {
        wcsncat(text, line, 0x280);
    }
    else
    {
        text[0] = L'\0';
    }
    vint_encode_wide_text(output_string, output_size, text);
}

// 0x008D7440
static void hud_ui_show_paused_elements(bool show)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handles[HDI_BASE]);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    vint_variant visible(show);
    vint_object_base* group = vint_object_base::find_object(cUI_Hud_Group, info->m_doc);
    if (group != nullptr)
    {
        group->set_property_value(cUI_Visible, visible, 0);
    }
    if (Radio_set.m_radio_info_pool.get_first(0) != nullptr)
    {
        vint_object_base* radio = vint_object_base::find_object(cUI_Radio_Group, info->m_doc);
        if (radio != nullptr)
        {
            radio->set_property_value(cUI_Visible, visible, 0);
        }
    }
}

// The primary and secondary loops of ui_setup_primary_secondary_text_names, the same code twice in rfg.exe.
template<int N>
static void setup_text_names(object_pool<message_info, N, 1>& pool, const char* group_name, const char* text_name,
    const char* in_name, const char* in2_name, const char* anim_name, const char* anim2_name, const char* out_name, const char* out2_name, const char* bg2_name)
{
    vint_document* doc = Ui_message_set.m_msg_doc;
    vint_object_base* group = vint_object_base::find_object(group_name, doc);
    vint_object_base* text = vint_object_base::find_object(text_name, doc);
    vint_object_animation* in = in_name != nullptr ? (vint_object_animation*)vint_object_base::find_object(in_name, doc) : nullptr;
    vint_object_animation* in2 = in2_name != nullptr ? (vint_object_animation*)vint_object_base::find_object(in2_name, doc) : nullptr;
    vint_object_base* anim = anim_name != nullptr ? vint_object_base::find_object(anim_name, doc) : nullptr;
    vint_object_base* anim2 = anim2_name != nullptr ? vint_object_base::find_object(anim2_name, doc) : nullptr;
    vint_object_animation* out = out_name != nullptr ? (vint_object_animation*)vint_object_base::find_object(out_name, doc) : nullptr;
    vint_object_animation* out2 = out2_name != nullptr ? (vint_object_animation*)vint_object_base::find_object(out2_name, doc) : nullptr;
    vint_object_base* bg2 = bg2_name != nullptr ? vint_object_base::find_object(bg2_name, doc) : nullptr;
    for (int i = 0; i < (int)pool.get_capacity(); i++)
    {
        message_info* message = pool.get_entry(i);
        // Original bug kept: the group and the text are not checked.
        message->m_obj_handle = group->get_handle();
        message->m_text_obj_handle = text->get_handle();
        if (in == nullptr)
        {
            message->m_fadein_handle = 0;
            message->m_fadein_time = 0.0f;
        }
        else
        {
            message->m_fadein_handle = in->get_handle();
            message->m_fadein_time = in->get_duration();
        }
        if (in2 == nullptr)
        {
            message->m_fadein2_handle = 0;
            message->m_fadein2_time = 0.0f;
        }
        else
        {
            message->m_fadein2_handle = in2->get_handle();
            message->m_fadein2_time = in2->get_duration();
        }
        message->m_anim_handle = anim != nullptr ? anim->get_handle() : 0;
        message->m_anim2_handle = anim2 != nullptr ? anim2->get_handle() : 0;
        if (out == nullptr)
        {
            message->m_fadeout_handle = 0;
            message->m_fadeout_time = 0.0f;
        }
        else
        {
            message->m_fadeout_handle = out->get_handle();
            message->m_fadeout_time = out->get_duration();
        }
        if (out2 == nullptr)
        {
            message->m_fadeout2_handle = 0;
            // Original bug kept: clears the first fade out's time, not the second's.
            message->m_fadeout_time = 0.0f;
        }
        else
        {
            message->m_fadeout2_handle = out2->get_handle();
            message->m_fadeout2_time = out2->get_duration();
        }
        message->m_bg2_handle = bg2 != nullptr ? bg2->get_handle() : 0;
        vint_variant size;
        text->get_property_value("screen_size", size, 0);
        const vector2 screen_size = size.get_type() == VINT_PROP_TYPE_VECTOR2F ? (vector2)size : vector2(0.0f, 0.0f);
        message->m_base_width = (int)screen_size.x;
        // Magic number: the second line's limit, compared with the text's width in ui_process_primary_messages.
        message->m_base_height = (int)(screen_size.y * 1.5f);
    }
}

// 0x008B0DB0
static void ui_setup_primary_secondary_text_names()
{
    // The major names are null in the shipped game (cAnimInMajor ... have no initializer): those messages have no
    // animations and appear at once.
    setup_text_names(Ui_message_set.m_primary_pool, cGroupMajor, cTextMajor, cAnimInMajor, cAnimInMajor2, cAnimMajor, cAnimMajor2, cAnimOutMajor, cAnimOutMajor2, cBgMajor2);
    setup_text_names(Ui_message_set.m_secondary_pool, cGroupMinor, cTextMinor, cAnimInMinor, cAnimInMinor2, cAnimMinor, cAnimMinor2, cAnimOutMinor, cAnimOutMinor2, cBgMinor2);

    vint_document* doc = Ui_message_set.m_msg_doc;
    vint_object_base* group = vint_object_base::find_object(cGroupPerm, doc);
    vint_object_base* text = vint_object_base::find_object("usage_text_perm1", doc);
    vint_variant scale;
    text->get_property_value("text_scale", scale, 0);
    User_message_base_scale = scale.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)scale).x : 0.0f;
    // The first user message uses the document's group, the others clones of it.
    for (int i = 0; i < (int)Ui_message_set.m_user_pool.get_capacity(); i++)
    {
        message_info* message = Ui_message_set.m_user_pool.get_entry(i);
        vint_object_base* copy = group;
        if (i != 0 && group != nullptr)
        {
            copy = group->clone(true, group->get_parent_object());
        }
        if (copy != nullptr)
        {
            vint_object_base* child = copy->find_child(crc_stri(cTextPerm, 0));
            message->m_obj_handle = copy->get_handle();
            message->m_text_obj_handle = child->get_handle();
        }
        else
        {
            message->m_obj_handle = 0;
            message->m_text_obj_handle = 0;
        }
    }
}

// 0x008B13C0 (font_handle is not used)
static void ui_setup_text_handle_information(object_pool_server<message_info>& pool_server, int font_handle)
{
    for (int i = 0; i < (int)pool_server.get_capacity(); i++)
    {
        message_info* message = pool_server.get_entry(i);
        vint_object_base* group = vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (group != nullptr)
        {
            vint_variant scale;
            group->get_property_value(cProp_Scale, scale, 0);
            message->m_obj_scale = scale.get_type() == VINT_PROP_TYPE_VECTOR2F ? (vector2)scale : vector2(0.0f, 0.0f);
            // In use (a category), the group shows.
            set_visible(group, (signed char)pool_server.get_category(i) > -1);
            vint_variant size;
            group->get_property_value(cProp_screen_size_uihud, size, 0);
            const vector2 screen_size = size.get_type() == VINT_PROP_TYPE_VECTOR2F ? (vector2)size : vector2(0.0f, 0.0f);
            message->m_width = (int)screen_size.x;
            message->m_height = (int)screen_size.y;
            if (vint_object_base::find_object(message->m_text_obj_handle, Ui_message_set.m_msg_doc) != nullptr)
            {
                message->m_text[0] = L'\0';
            }
        }
    }
}

// 0x008B1540
static void ui_setup_primary_secondary_text_handles()
{
    ui_setup_primary_secondary_text_names();
    if (Ui_message_set.m_msg_doc != nullptr)
    {
        ui_setup_text_handle_information(Ui_message_set.m_primary_pool, Ui_message_set.m_primary_font);
        ui_setup_text_handle_information(Ui_message_set.m_secondary_pool, Ui_message_set.m_secondary_font);
        ui_setup_text_handle_information(Ui_message_set.m_user_pool, Ui_message_set.m_user_font);
    }
}

template<int N>
static void clear_active_messages(object_pool<message_info, N, 1>& pool)
{
    pool.thread_access.enter();
    message_info* message = pool.get_first(0);
    while (message != nullptr)
    {
        message_info* next = pool.get_next(message);
        vint_object_base* group = vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (group != nullptr)
        {
            set_visible(group, false);
        }
        pool.free(message);
        message = next;
    }
    pool.thread_access.leave();
}

// 0x008D7500
static void ui_clear_active_primary_messages()
{
    clear_active_messages(Ui_message_set.m_primary_pool);
}

// 0x008D75F0
static void ui_clear_active_secondary_messages()
{
    clear_active_messages(Ui_message_set.m_secondary_pool);
}

// 0x008D76E0
static void ui_clear_active_user_messages()
{
    clear_active_messages(Ui_message_set.m_user_pool);
}

// 0x00894530
static void ui_clear_item_pickup_messages()
{
    Item_pickup_set.num_icons = 0;
    Item_pickup_set.prev_num_icons = 0;
    Item_pickup_set.updated = false;
    Item_pickup_set.icon_info[0].data_item_handle = 0;
    Item_pickup_set.icon_info[1].data_item_handle = 0;
    Item_pickup_set.icon_info[2].data_item_handle = 0;
}

// 0x008D77D0
static void ui_activate_use_messages()
{
    vint_doc_info* info = ui_get_doc_info(Doc_handles[HDI_MESSAGES]);
    Ui_message_set.m_msg_doc = info != nullptr ? info->m_doc : nullptr;
    ui_init_my_fonts();
    ui_setup_primary_secondary_text_handles();
}

// 0x00894560
static void ui_deactivate_use_messages()
{
    Ui_message_set.m_user_font = -1;
    Ui_message_set.m_secondary_font = -1;
    Ui_message_set.m_primary_font = -1;
    Ui_message_set.m_msg_doc = nullptr;
}

// 0x008B1570
static void radio_msg_audio_callback_intro_done(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data)
{
    if (cue_cb_data != nullptr && cue_cb_data->user_data != nullptr)
    {
        radio_message_info* rmi = RadioMsgID_to_RadioMsgInfo((unsigned short)(unsigned int)cue_cb_data->user_data);
        if (rmi != nullptr)
        {
            rmi->m_state = RMS_START_MESSAGE;
            rmi->m_intro_sound_play_id = -1;
        }
    }
}

// 0x008B15D0
static void radio_msg_audio_callback_msg_done(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data)
{
    if (cue_cb_data != nullptr && cue_cb_data->user_data != nullptr)
    {
        radio_message_info* rmi = RadioMsgID_to_RadioMsgInfo((unsigned short)(unsigned int)cue_cb_data->user_data);
        if (rmi != nullptr)
        {
            // A subtitle has no outro.
            rmi->m_state = (char)(rmi->m_display_as_subtitle ? RMS_DONE : RMS_START_OUTRO);
            rmi->m_msg_sound_play_id = -1;
        }
    }
}

// 0x008B1630
static void radio_msg_audio_callback_outro_done(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data)
{
    if (cue_cb_data != nullptr && cue_cb_data->user_data != nullptr)
    {
        radio_message_info* rmi = RadioMsgID_to_RadioMsgInfo((unsigned short)(unsigned int)cue_cb_data->user_data);
        if (rmi != nullptr)
        {
            rmi->m_state = RMS_DONE;
            rmi->m_outro_sound_play_id = -1;
        }
    }
}

// 0x00894580 (LTCG: text in ECX, alpha in XMM0)
static void set_vtext_alpha(vint_element_text* text, float alpha)
{
    if (text != nullptr)
    {
        text->set_alpha(vint_variant(alpha), 0);
    }
}

// 0x008945C0 (LTCG: text in ECX, text_tag in EAX)
static void set_vtext_text(vint_element_text* text, const char* text_tag)
{
    if (text != nullptr)
    {
        // cEmptyString is the wide empty string; the first byte is 0 either way.
        text->set_text_tag(vint_variant(text_tag != nullptr ? text_tag : (const char*)cEmptyString), 0);
    }
}

// 0x00894600 (LTCG: text in ECX, text_scale in EAX)
static void set_vtext_text_scale(vint_element_text* text, const vector2& text_scale)
{
    if (text != nullptr)
    {
        text->set_text_scale(vint_variant(text_scale), 0);
    }
}

// 0x00894650 (LTCG: group in ECX, visible in AL)
static void set_vgroup_visibility(vint_element_group* group, bool visible)
{
    if (group != nullptr)
    {
        group->set_visible(vint_variant(visible), 0);
    }
}

// 0x00894690 (LTCG: group in ECX, alpha in XMM0)
static void set_vgroup_alpha(vint_element_group* group, float alpha)
{
    if (group != nullptr)
    {
        group->set_alpha(vint_variant(alpha), 0);
    }
}

// 0x008946D0 (LTCG: group in ECX, posn in EAX)
static void set_vgroup_posn(vint_element_group* group, const vector2& posn)
{
    if (group != nullptr)
    {
        group->set_anchor(vint_variant(posn), 0);
    }
}

// 0x00894720
static vint_data_item* hud_ui_add_health_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("HealthUpdate");
    doc_handle = HDI_HEALTH;
    if (item != nullptr)
    {
        const vint_variant_type types[6] = {VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 6);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x008947A0
static vint_data_item* hud_ui_add_morale_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("MoraleUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[16] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_INT,
            VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_UINT};
        item->set_template(types, 16);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00894840
static vint_data_item* hud_ui_add_radio_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("RadioUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[11] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_INT};
        item->set_template(types, 11);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x008948D0
static vint_data_item* hud_ui_add_alert_level_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("AlertLevel");
    if (item != nullptr)
    {
        const vint_variant_type types[5] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING};
        item->set_template(types, 5);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00894940
static vint_data_item* hud_ui_add_player_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("PlayerData");
    if (item != nullptr)
    {
        const vint_variant_type type = VINT_PROP_TYPE_FLOAT;
        item->set_template(&type, 1);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00894980
static vint_data_item* hud_ui_add_swap_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("SwapData");
    doc_handle = HDI_MESSAGES;
    if (item != nullptr)
    {
        const vint_variant_type types[5] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 5);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00894A00
static vint_data_item* hud_ui_add_use_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("UseData");
    doc_handle = HDI_MESSAGES;
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x008D7830
static void hud_ui_create_data_items()
{
    // Magic number: the first 7 entries of Data_handles_uihud are items, the 8th the group.
    for (int i = 0; i < 7; i++)
    {
        vint_data_item* item = nullptr;
        int data_handle = -1;
        int doc = HDI_BASE;
        switch (i)
        {
            case 0: item = hud_ui_add_health_data_items_info(data_handle, doc); break;
            case 1: item = hud_ui_add_morale_data_items_info(data_handle, doc); break;
            case 2: item = hud_ui_add_radio_data_items_info(data_handle, doc); break;
            case 3: item = hud_ui_add_alert_level_data_items_info(data_handle, doc); break;
            case 4: item = hud_ui_add_player_data_items_info(data_handle, doc); break;
            case 5: item = hud_ui_add_swap_data_items_info(data_handle, doc); break;
            case 6: item = hud_ui_add_use_data_items_info(data_handle, doc); break;
        }
        Data_handles_uihud[i] = data_handle;
        if (!ui_add_data_item(Doc_handles[doc], data_handle))
        {
            if (item != nullptr)
            {
                item->destroy();
            }
            Data_handles_uihud[i] = -1;
        }
    }
}

// 0x00894A70
static vint_data_group* hud_ui_add_item_data_info(int& data_handle, int& doc_handle)
{
    vint_data_group* group = vint_data_group::create_new("AmmoData");
    doc_handle = HDI_MESSAGES;
    if (group != nullptr)
    {
        data_handle = group->get_handle();
    }
    return group;
}

// 0x008D7980
static void hud_ui_create_data_groups()
{
    int data_handle = -1;
    int doc = HDI_BASE;
    vint_data_group* group = hud_ui_add_item_data_info(data_handle, doc);
    Data_handles_uihud[7] = data_handle;
    if (!ui_add_data_group(Doc_handles[doc], data_handle))
    {
        if (group != nullptr)
        {
            group->destroy();
        }
        Data_handles_uihud[7] = -1;
    }
}

// The start times of a message's three animations (ui_process_primary_messages, ui_process_secondary_messages).
static void set_message_start_times(const message_info* message, vint_object_base* fadein, vint_object_base* anim, vint_object_base* fadeout, float fadein_time, float fadeout_time, bool first_frame)
{
    float start = Ui_message_set.m_msg_doc->get_time_index();
    if (first_frame)
    {
        if (fadein != nullptr)
        {
            fadein->set_property_value("start_time", vint_variant(start), 0);
        }
        if (anim != nullptr)
        {
            start += fadein_time;
            anim->set_property_value("start_time", vint_variant(start), 0);
        }
    }
    if (fadeout != nullptr)
    {
        // Magic number: the half second all display times have on top (ui_add_primary_message_common).
        start += (float)((double)message->m_display_time - (double)message->m_running_time - (double)fadeout_time - 0.5);
        fadeout->set_property_value("start_time", vint_variant(start), 0);
    }
}

static void set_message_tween_states(const message_info* message)
{
    const vint_tween_state first = message->m_use_secondary_anim ? VINT_TWEEN_STATE_DISABLED : VINT_TWEEN_STATE_WAITING;
    const vint_tween_state second = message->m_use_secondary_anim ? VINT_TWEEN_STATE_WAITING : VINT_TWEEN_STATE_DISABLED;
    vint_object_animation_set_child_tween_state(message->m_fadein_handle, first, Ui_message_set.m_msg_doc);
    vint_object_animation_set_child_tween_state(message->m_fadeout_handle, first, Ui_message_set.m_msg_doc);
    vint_object_animation_set_child_tween_state(message->m_anim_handle, first, Ui_message_set.m_msg_doc);
    vint_object_animation_set_child_tween_state(message->m_fadein2_handle, second, Ui_message_set.m_msg_doc);
    vint_object_animation_set_child_tween_state(message->m_fadeout2_handle, second, Ui_message_set.m_msg_doc);
    vint_object_animation_set_child_tween_state(message->m_anim2_handle, second, Ui_message_set.m_msg_doc);
}

// 0x008E8890
void ui_process_primary_messages(float dtime)
{
    if (Ui_message_set.m_msg_doc == nullptr)
    {
        return;
    }
    message_info* message = Ui_message_set.m_primary_pool.get_first(0);
    if (message == nullptr)
    {
        return;
    }
    vint_document* doc = Ui_message_set.m_msg_doc;
    vint_element_group* group = (vint_element_group*)vint_object_base::find_object(message->m_obj_handle, doc);
    vint_element_group* bg2 = (vint_element_group*)vint_object_base::find_object(message->m_bg2_handle, doc);
    vint_object_base* fadein;
    vint_object_base* fadeout;
    vint_object_base* anim;
    float fadein_time;
    float fadeout_time;
    if (!message->m_use_secondary_anim)
    {
        fadein = vint_object_base::find_object(message->m_fadein_handle, doc);
        fadeout = vint_object_base::find_object(message->m_fadeout_handle, doc);
        anim = vint_object_base::find_object(message->m_anim_handle, doc);
        fadein_time = message->m_fadein_time;
        fadeout_time = message->m_fadeout_time;
    }
    else
    {
        fadein = vint_object_base::find_object(message->m_fadein2_handle, doc);
        fadeout = vint_object_base::find_object(message->m_fadeout2_handle, doc);
        anim = vint_object_base::find_object(message->m_anim2_handle, doc);
        fadein_time = message->m_fadein2_time;
        fadeout_time = message->m_fadeout2_time;
    }
    if (game_is_paused())
    {
        if (group != nullptr)
        {
            group->set_visible(vint_variant(false), 0);
        }
        return;
    }
    const bool first_frame = message->m_running_time <= 0.0f;
    message->m_running_time = message->m_running_time + dtime;
    if (message->m_display_time < message->m_running_time)
    {
        Ui_message_set.m_primary_pool.free(message);
        set_vgroup_visibility(group, false);
        return;
    }
    if (message->m_dirty)
    {
        vint_element_text* text = (vint_element_text*)vint_object_base::find_object(message->m_text_obj_handle, doc);
        if (text != nullptr)
        {
            if (message->m_text[0] == L'\0')
            {
                text->set_text_tag_crc(vint_variant(message->m_crc), 0);
            }
            else
            {
                char ascii[0x200];
                copy_to_ascii(ascii, message->m_text, 0x200);
                const wchar_t* localized = localization_get_display_string_req(ascii, message->m_text);
                char encoded[0x300];
                vint_encode_wide_text(encoded, 0x300, localized);
                set_vtext_text(text, encoded);
                if (first_frame && fadein == nullptr)
                {
                    // Without an animation to bring them in.
                    set_vgroup_alpha(group, 1.0f);
                    text->set_alpha(vint_variant(1.0f), 0);
                }
            }
            if (bg2 != nullptr)
            {
                vint_variant size;
                text->get_property_value("screen_size", size, 0);
                const float width = size.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)size).x : 0.0f;
                set_vgroup_visibility(bg2, (int)width > message->m_base_height);
            }
        }
    }
    if (first_frame)
    {
        audiolib_cue_play(message->m_audio_cue != nullptr ? *message->m_audio_cue : Ga_cue_ids.general_other_pop_up, nullptr, nullptr);
        if (Ui_message_set.m_msg_doc != nullptr)
        {
            set_message_start_times(message, fadein, anim, fadeout, fadein_time, fadeout_time, true);
        }
        set_message_tween_states(message);
    }
    else
    {
        set_message_start_times(message, fadein, anim, fadeout, fadein_time, fadeout_time, false);
    }
    set_vgroup_visibility(group, true);
    message->m_dirty = false;
    if (Multiplayer && hud_ui_multiplayer_splash_screen_is_active())
    {
        set_vgroup_visibility(group, false);
    }
}

// 0x008E8E50
void ui_process_secondary_messages(float dtime)
{
    if (Ui_message_set.m_msg_doc == nullptr)
    {
        return;
    }
    message_info* message = Ui_message_set.m_secondary_pool.get_first(0);
    if (message == nullptr)
    {
        return;
    }
    vint_document* doc = Ui_message_set.m_msg_doc;
    vint_element_group* group = (vint_element_group*)vint_object_base::find_object(message->m_obj_handle, doc);
    vint_element_group* bg2 = (vint_element_group*)vint_object_base::find_object(message->m_bg2_handle, doc);
    vint_object_base* fadein;
    vint_object_base* fadeout;
    vint_object_base* anim;
    float fadein_time;
    float fadeout_time;
    if (!message->m_use_secondary_anim)
    {
        fadein = vint_object_base::find_object(message->m_fadein_handle, doc);
        fadeout = vint_object_base::find_object(message->m_fadeout_handle, doc);
        anim = vint_object_base::find_object(message->m_anim_handle, doc);
        fadein_time = message->m_fadein_time;
        fadeout_time = message->m_fadeout_time;
    }
    else
    {
        fadein = vint_object_base::find_object(message->m_fadein2_handle, doc);
        fadeout = vint_object_base::find_object(message->m_fadeout2_handle, doc);
        anim = vint_object_base::find_object(message->m_anim2_handle, doc);
        fadein_time = message->m_fadein2_time;
        fadeout_time = message->m_fadeout2_time;
    }
    if (game_is_paused())
    {
        if (group != nullptr)
        {
            group->set_visible(vint_variant(false), 0);
        }
        return;
    }
    const bool first_frame = message->m_running_time <= 0.0f;
    message->m_running_time = message->m_running_time + dtime;
    if (message->m_display_time < message->m_running_time)
    {
        Ui_message_set.m_secondary_pool.free(message);
        set_vgroup_visibility(group, false);
        return;
    }
    if (message->m_dirty)
    {
        vint_element_text* text = (vint_element_text*)vint_object_base::find_object(message->m_text_obj_handle, doc);
        if (text != nullptr)
        {
            if (message->m_text[0] == L'\0')
            {
                text->set_text_tag_crc(vint_variant(message->m_crc), 0);
            }
            else
            {
                char ascii[0x200];
                copy_to_ascii(ascii, message->m_text, 0x200);
                const wchar_t* localized = localization_get_display_string_req(ascii, message->m_text);
                char encoded[0x300];
                vint_encode_wide_text(encoded, 0x300, localized);
                set_vtext_text(text, encoded);
            }
            if (bg2 != nullptr)
            {
                vint_variant size;
                text->get_property_value("screen_size", size, 0);
                const float width = size.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)size).x : 0.0f;
                set_vgroup_visibility(bg2, (int)width > message->m_base_height);
            }
        }
    }
    if (first_frame)
    {
        audiolib_cue_play(message->m_audio_cue != nullptr ? *message->m_audio_cue : Ga_cue_ids.general_other_pop_up, nullptr, nullptr);
        if (Ui_message_set.m_msg_doc != nullptr)
        {
            set_message_start_times(message, fadein, anim, fadeout, fadein_time, fadeout_time, true);
        }
        set_message_tween_states(message);
        if (!message->m_use_secondary_anim)
        {
            vint_object_base* text = vint_object_base::find_object("usage_text_minor1", Ui_message_set.m_msg_doc);
            // Original bug kept: the text is not checked.
            vint_object_set_property(text->get_handle(), "alpha", vint_variant(1.0f));
        }
    }
    else
    {
        set_message_start_times(message, fadein, anim, fadeout, fadein_time, fadeout_time, false);
    }
    set_vgroup_visibility(group, true);
    message->m_dirty = false;
    if (Multiplayer && hud_ui_multiplayer_splash_screen_is_active())
    {
        set_vgroup_visibility(group, false);
    }
}

// 0x008D7A10
void ui_process_user_messages(float dtime)
{
    if (Ui_message_set.m_msg_doc == nullptr)
    {
        return;
    }
    int lines[NUM_HUD_USER_MESSAGE_TYPES] = {};
    // Magic number: the pool's size. The first message has the largest depth.
    int depth = 11;
    Ui_message_set.m_user_pool.thread_access.enter();
    message_info* message = Ui_message_set.m_user_pool.get_first(0);
    while (message != nullptr)
    {
        message_info* next = Ui_message_set.m_user_pool.get_next(message);
        vint_element_group* group = (vint_element_group*)vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (game_is_paused())
        {
            if (group != nullptr)
            {
                group->set_visible(vint_variant(false), 0);
            }
        }
        else
        {
            float y = message->m_position.y;
            if (!Multiplayer && y - Minimum_perm_message_y >= 0.0f)
            {
                y = Minimum_perm_message_y;
            }
            const hud_user_message_types type = message->m_user_message_type;
            // Magic numbers: the position is a fraction of 1280x720; each type stacks its own lines.
            const vector2 position(message->m_position.x * 1280.0f, (float)(message->m_height * lines[type]) + y * 720.0f);
            if (type != HUMT_OTHER)
            {
                lines[type]++;
            }
            if (message->m_dirty)
            {
                vint_element_text* text = (vint_element_text*)vint_object_base::find_object(message->m_text_obj_handle, Ui_message_set.m_msg_doc);
                if (text != nullptr)
                {
                    text->set_text_scale(vint_variant(vector2(User_message_base_scale, User_message_base_scale)), 0);
                }
                char encoded[0x300];
                vint_encode_wide_text(encoded, 0x300, message->m_text);
                if (text != nullptr)
                {
                    text->set_text_tag(vint_variant((const char*)encoded), 0);
                }
                // Original bug kept: the text is used below without a check.
                vint_variant size;
                text->get_property_value("screen_size", size, 0);
                const float width = size.get_type() == VINT_PROP_TYPE_VECTOR2F ? ((vector2)size).x : 0.0f;
                // Magic number: wider than 624 pixels shrinks horizontally to fit.
                float scale_x = User_message_base_scale;
                if (width > 624.0f)
                {
                    scale_x = (User_message_base_scale * 624.0f) / width;
                }
                text->set_text_scale(vint_variant(vector2(scale_x, User_message_base_scale)), 0);
                text->set_line_frame_enable(vint_variant(message->m_outlined), 0);
            }
            if (group != nullptr)
            {
                group->set_anchor(vint_variant(position), 0);
                group->set_alpha(vint_variant(1.0f), 0);
                group->set_visible(vint_variant(true), 0);
                group->set_depth(vint_variant(depth), 0);
            }
            message->m_dirty = false;
        }
        message->m_running_time = message->m_running_time + dtime;
        if (message->m_display_time >= 0.0f && message->m_display_time <= message->m_running_time)
        {
            if (group != nullptr)
            {
                group->set_visible(vint_variant(false), 0);
            }
            Ui_message_set.m_user_pool.free(message);
        }
        depth--;
        message = next;
    }
    Ui_message_set.m_user_pool.thread_access.leave();
}

// 0x008B1690
static void hud_ui_process_health_update(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[0]);
    if (item == nullptr)
    {
        return;
    }
    player* pp = player_get_local_player();
    bool on_fire = false;
    if (pp == nullptr || !hud_damage_indicators_enabled())
    {
        item->set_element(0, vint_variant(1.0f));
        item->set_element(1, vint_variant(false));
        item->set_element(2, vint_variant(pp == nullptr || !pp->is_killed() ? 1.0f : 0.0f));
        item->set_element(3, vint_variant(false));
        item->set_element(4, vint_variant(0));
    }
    else
    {
        int upgrade = player_get_meta_data()->get_upgrade_value(0x1A); // Magic number: the health upgrade (26)
        // Magic number: the DLC bundle and wrecking crew show the second upgrade level.
        if (World.dlc_bundle_id == 1)
        {
            upgrade = 2;
        }
        if (wrecking_crew_is_active())
        {
            upgrade = 2;
        }
        float health = pp->hit_points / (float)pp->max_hit_points;
        bool in_vehicle = false;
        if (pp->pflags.in_radiation == 0)
        {
            if (In_radiation)
            {
                In_radiation = false;
            }
            else
            {
                vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
                if (vp != nullptr)
                {
                    health = 1.0f - vp->get_vehicle_damage();
                    on_fire = vp->m_flags.m_on_fire != 0;
                    in_vehicle = true;
                }
            }
        }
        else
        {
            In_radiation = true;
        }
        Previous_health = health;
        item->set_element(0, vint_variant(heartbeat_get_ring_alpha()));
        item->set_element(1, vint_variant(In_radiation));
        if (pp->is_killed())
        {
            health = 0.0f;
        }
        item->set_element(2, vint_variant(health));
        item->set_element(3, vint_variant(in_vehicle));
        item->set_element(4, vint_variant(upgrade));
    }
    item->set_element(5, vint_variant(on_fire));
}

// 0x00894AA0
static void hud_ui_process_morale_info(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[1]);
    if (item == nullptr)
    {
        return;
    }
    if (!Control_value_updated)
    {
        item->set_element(0, vint_variant(0));
        item->set_element(1, vint_variant(0));
        item->set_element(2, vint_variant(0.0f));
        item->set_element(3, vint_variant(0.0f));
        item->set_element(7, vint_variant(0));
        item->set_element(8, vint_variant(false));
        item->set_element(9, vint_variant(0));
        item->set_element(10, vint_variant(0));
        item->set_element(11, vint_variant(0));
        item->set_element(12, vint_variant(false));
        item->set_element(13, vint_variant(false));
    }
    else
    {
        item->set_element(0, vint_variant(Control_current_value));
        item->set_element(1, vint_variant(Control_delta_value));
        item->set_element(2, vint_variant(Control_current_percent));
        item->set_element(3, vint_variant(Control_delta_percent));
        item->set_element(7, vint_variant(Control_num_missions - Control_missions_left));
        item->set_element(8, vint_variant(Control_mission_complete));
        item->set_element(9, vint_variant(Control_num_missions));
        item->set_element(10, vint_variant(Control_missions_locked));
        item->set_element(11, vint_variant(Control_missions_unlocked));
        bool mission_here = false;
        if (mission_is_active())
        {
            mission_info* info = mission_info_get(mission_get_current_mission());
            if (info != nullptr && get_district_by_name(info->district_name) == Control_district_handle)
            {
                mission_here = true;
            }
        }
        item->set_element(12, vint_variant(mission_here));
        item->set_element(13, vint_variant(true));
        Control_value_updated = false;
    }
    if (!Morale_percent_updated)
    {
        item->set_element(4, vint_variant(0.0f));
        item->set_element(5, vint_variant(0.0f));
        item->set_element(14, vint_variant(false));
    }
    else
    {
        item->set_element(4, vint_variant(Morale_current_percent));
        item->set_element(5, vint_variant(Morale_old_percent));
        item->set_element(6, vint_variant(Morale_change));
        item->set_element(14, vint_variant(true));
        Morale_percent_updated = false;
    }
    item->set_element(15, vint_variant(Control_morale_district_hash));
}

// 0x00894FE0
static void hud_ui_process_swap_info(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[5]);
    if (item == nullptr || player_get_local_player() == nullptr)
    {
        return;
    }
    const char* primary_icon = nullptr;
    const char* secondary_icon = nullptr;
    if (Swap_menu_set.m_weapon_swap)
    {
        const weapon_info* primary = weapons_get_weapon_info(Swap_menu_set.m_primary_slot.w_info_handle);
        if (primary != nullptr)
        {
            primary_icon = primary->icon_name;
        }
        const weapon_info* secondary = weapons_get_weapon_info(Swap_menu_set.m_secondary_slot.w_info_handle);
        if (secondary != nullptr)
        {
            secondary_icon = secondary->icon_name;
        }
    }
    else
    {
        const multi_backpack_info* primary = multi_game_backpack_get_info(Swap_menu_set.m_primary_slot.backpack_type);
        if (primary != nullptr)
        {
            primary_icon = primary->icon_name;
        }
        const multi_backpack_info* secondary = multi_game_backpack_get_info(Swap_menu_set.m_secondary_slot.backpack_type);
        if (secondary != nullptr)
        {
            secondary_icon = secondary->icon_name;
        }
    }
    item->set_element(0, vint_variant(Swap_menu_set.m_show_swap_menu));
    item->set_element(1, vint_variant(Swap_menu_set.m_weapon_swap));
    item->set_element(2, vint_variant(primary_icon));
    item->set_element(3, vint_variant(secondary_icon));
    char button[0x80];
    char image[0x80];
    controls_get_button_image_name(button, 0x80, control_gameplay_get_action_button(), Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(4, vint_variant((const char*)image));
}

// 0x008E9410
static void hud_ui_process_use_action_info(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[6]);
    if (item == nullptr)
    {
        return;
    }
    bool show = false;
    int icon = USE_OBJECT_ICON_DEFAULT;
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->action_object.action_callback != nullptr)
    {
        icon = pp->action_object.action_icon;
        const bool shown = (bool)item->get_element(0);
        if (pp->action_object.action_type == GENERAL_ACTION_UPGRADE && !shown)
        {
            ui_tool_tips_add_tip("upgrades", 100, nullptr);
        }
        // The ammo message covers the prompt.
        bool ammo_message = false;
        vint_doc_info* info = ui_get_doc_info(Doc_handles[HDI_MESSAGES]);
        if (info != nullptr && info->m_doc != nullptr)
        {
            vint_object_base* object = vint_object_base::find_object("message_ammo", info->m_doc);
            if (object != nullptr)
            {
                vint_variant visible;
                object->get_property_value("visible", visible, 0);
                vint_variant alpha;
                object->get_property_value("alpha", alpha, 0);
                ammo_message = (bool)visible && (float)alpha >= 0.1f;
            }
        }
        // Magic number: 10 = USE_OBJECT_ICON_UPGRADE, one past the shown icons counted from USE_OBJECT_ICON_DEFAULT
        // (compared unsigned: USE_OBJECT_ICON_SWAP is out of range).
        if ((unsigned int)(icon - USE_OBJECT_ICON_DEFAULT) < 10)
        {
            if ((pp->action_object.action_type != GENERAL_ACTION_UPGRADE || player_start_upgrades(player_get_local_player(), false)) && !human_is_in_cover(pp))
            {
                show = !ammo_message;
            }
        }
        else
        {
            icon = USE_OBJECT_ICON_DEFAULT;
        }
    }
    item->set_element(0, vint_variant(show));
    item->set_element(1, vint_variant(icon));
    char button[0x80];
    char image[0x80];
    controls_get_button_image_name(button, 0x80, control_gameplay_get_action_button(), Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(2, vint_variant((const char*)image));
}

// 0x008B19C0
static void hud_ui_process_use_action_info_paused(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[6]);
    if (item != nullptr && Tool_tip_queue[Current_tool_tip].running)
    {
        item->set_element(0, vint_variant(false));
    }
}

// 0x008951B0
static void hud_ui_process_item_pickup_info(float dtime)
{
    vint_data_group* group = vint_data_group::find(Data_handles_uihud[7]);
    int prev_num = Item_pickup_set.prev_num_icons;
    if (group != nullptr && Item_pickup_set.updated)
    {
        Item_pickup_set.updated = false;
        const int num = Item_pickup_set.num_icons;
        if (num > -1 && num < 4 && prev_num > -1 && prev_num < 4)
        {
            for (int i = prev_num - 1; i >= num; i--)
            {
                vint_data_item* item = vint_data_item::find(Item_pickup_set.icon_info[i].data_item_handle);
                if (item != nullptr)
                {
                    item->destroy();
                }
                Item_pickup_set.icon_info[i].data_item_handle = 0;
            }
            for (int i = prev_num; i < num; i++)
            {
                vint_data_item* item = group->add_data_item(nullptr);
                if (item == nullptr)
                {
                    Item_pickup_set.icon_info[i].data_item_handle = 0;
                }
                else
                {
                    const vint_variant_type types[6] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT};
                    item->set_template(types, 6);
                    Item_pickup_set.icon_info[i].data_item_handle = item->get_handle();
                }
            }
            for (int i = 0; i < num; i++)
            {
                item_icon_info& icon = Item_pickup_set.icon_info[i];
                vint_data_item* item = vint_data_item::find(icon.data_item_handle);
                if (item != nullptr)
                {
                    item->set_element(0, vint_variant(i + 1));
                    item->set_element(1, vint_variant(num));
                    item->set_element(2, vint_variant(icon.full));
                    item->set_element(3, vint_variant((const char*)icon.item_name));
                    item->set_element(4, vint_variant((const char*)icon.item_string));
                    item->set_element(5, vint_variant(icon.is_salvage ? icon.amount_added : 0u));
                    item->set_updated();
                }
            }
        }
        prev_num = Item_pickup_set.num_icons;
    }
    Item_pickup_set.prev_num_icons = prev_num;
}

// 0x008E9660
static void hud_ui_clear_all_radio_messages()
{
    radio_message_cs.enter();
    Radio_set.m_radio_info_pool.thread_access.enter();
    for (radio_message_info* rmi = Radio_set.m_radio_info_pool.get_first(0); rmi != nullptr;)
    {
        rmi = Radio_set.m_radio_info_pool.free(rmi);
    }
    Radio_set.m_radio_info_pool.thread_access.leave();
    radio_message_cs.leave();
}

// 0x00895450 (LTCG: rmi in ESI)
static void hud_ui_cancel_radio_message_sounds(radio_message_info* rmi)
{
    if (rmi != nullptr)
    {
        audiolib_play_stop(rmi->m_intro_sound_play_id);
        audiolib_play_stop(rmi->m_msg_sound_play_id);
        // Original bug kept: the outro is unpaused, not stopped, so it plays after a cancel.
        audiolib_play_set_pause(rmi->m_outro_sound_play_id, false);
    }
}

// 0x008D7F00 (LTCG: rmi in ESI)
static void hud_ui_cancel_one_radio_message(radio_message_info* rmi)
{
    radio_message_cs.enter();
    if (rmi != nullptr)
    {
        hud_ui_cancel_radio_message_sounds(rmi);
        Radio_set.m_radio_info_pool.free(rmi);
    }
    radio_message_cs.leave();
}

// 0x008E96C0
void hud_ui_cancel_radio_messages()
{
    radio_message_cs.enter();
    Radio_set.m_radio_info_pool.thread_access.enter();
    radio_message_info* rmi = Radio_set.m_radio_info_pool.get_first(0);
    while (rmi != nullptr)
    {
        radio_message_info* next = Radio_set.m_radio_info_pool.get_next(rmi);
        hud_ui_cancel_one_radio_message(rmi);
        rmi = next;
    }
    Radio_set.m_radio_info_pool.thread_access.leave();
    hud_ui_clear_all_radio_messages();
    radio_message_cs.leave();
}

// 0x008B1A20
static void hud_ui_unpause_radio_messages()
{
    radio_message_cs.enter();
    radio_message_info* rmi = Radio_set.m_radio_info_pool.get_first(0);
    if (rmi == nullptr || !rmi->m_paused)
    {
        radio_message_cs.leave();
        return;
    }
    int play_id = -1;
    switch (rmi->m_last_state)
    {
        case RMS_INTRO:
            if (audiolib_play_get_state(rmi->m_intro_sound_play_id, nullptr) == AUDIOLIB_ERR_NONE)
            {
                play_id = rmi->m_intro_sound_play_id;
                break;
            }
            // fall through
        case RMS_START_MESSAGE:
        case RMS_MESSAGE:
            if (audiolib_play_get_state(rmi->m_msg_sound_play_id, nullptr) == AUDIOLIB_ERR_NONE)
            {
                play_id = rmi->m_msg_sound_play_id;
                break;
            }
            // fall through
        case RMS_START_OUTRO:
        case RMS_OUTRO:
        case RMS_DONE:
            play_id = rmi->m_outro_sound_play_id;
            break;
        default:
            rmi->m_paused = false;
            radio_message_cs.leave();
            return;
    }
    if (rmi->m_kill_me)
    {
        audiolib_play_stop(play_id);
    }
    else
    {
        audiolib_play_set_pause(play_id, false);
    }
    rmi->m_paused = false;
    radio_message_cs.leave();
}

// 0x008B1B70
static void hud_ui_pause_radio_messages()
{
    radio_message_cs.enter();
    radio_message_info* rmi = Radio_set.m_radio_info_pool.get_first(0);
    if (rmi != nullptr && !rmi->m_paused)
    {
        audiolib_play_set_pause(rmi->m_intro_sound_play_id, true);
        audiolib_play_set_pause(rmi->m_msg_sound_play_id, true);
        audiolib_play_set_pause(rmi->m_outro_sound_play_id, true);
        rmi->m_paused = true;
    }
    radio_message_cs.leave();
}

// 0x008C4F40
static radio_message_info* hud_ui_get_free_radio_info()
{
    radio_message_cs.enter();
    radio_message_info* rmi = Radio_set.m_radio_info_pool.alloc(0);
    radio_message_cs.leave();
    return rmi;
}

// 0x008B1BF0
void hud_ui_clear_current_radio_message(const unsigned short id)
{
    radio_message_cs.enter();
    bool cancel = true;
    radio_message_info* rmi;
    if (id == 0)
    {
        rmi = Radio_set.m_radio_info_pool.get_first(0);
    }
    else
    {
        rmi = RadioMsgID_to_RadioMsgInfo(id);
        if (rmi != nullptr && rmi->m_last_state == RMS_NONE)
        {
            cancel = false;
        }
    }
    if (rmi != nullptr && cancel)
    {
        rmi->m_kill_me = true;
        hud_ui_cancel_radio_message_sounds(rmi);
    }
    radio_message_cs.leave();
}

// The sound of the current part, keeping it playing: unpaused while it waits or loads; playing is fine, gone ends the part.
static void hud_ui_radio_keep_sound_playing(radio_message_info* rmi, int play_id, char state_when_gone)
{
    audiolib_play_state state;
    audiolib_play_get_state(play_id, &state);
    switch (state)
    {
        case AUDIOLIB_PLAY_STATE_PENDING_FREE_CHANNEL:
        case AUDIOLIB_PLAY_STATE_LOADING:
        case AUDIOLIB_PLAY_STATE_READY:
        case AUDIOLIB_PLAY_STATE_PAUSED:
            audiolib_play_set_pause(play_id, false);
            break;
        case AUDIOLIB_PLAY_STATE_PLAYING:
            if (play_id == rmi->m_msg_sound_play_id && rmi->m_human != nullptr)
            {
                vector head;
                human_get_head_pos(rmi->m_human, head);
                audiolib_play_set_3d_position(rmi->m_msg_sound_play_id, &head, nullptr, nullptr);
            }
            break;
        default:
            rmi->m_state = state_when_gone;
            break;
    }
}

// 0x008F4360
static void hud_ui_process_radio_messages(float dtime)
{
    radio_message_cs.enter();
    radio_message_info* rmi = Radio_set.m_radio_info_pool.get_first(0);
    if (rmi == nullptr)
    {
        vint_data_item* item = vint_data_item::find(Data_handles_uihud[2]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(false));
            item->set_element(3, vint_variant(0.0f));
            item->set_element(10, vint_variant(0));
        }
        radio_message_cs.leave();
        return;
    }
    rmi->m_last_state = rmi->m_state;
    rmi->m_radio_message_time_left = rmi->m_radio_message_time_left - dtime;
    switch (rmi->m_state)
    {
        case RMS_START_INTRO:
            rmi->m_start_time = timer_get(TIMER_MILLISECONDS);
            if (audiolib_play_get_state(rmi->m_intro_sound_play_id, nullptr) == AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = (char)(audiolib_play_set_pause(rmi->m_intro_sound_play_id, false) == AUDIOLIB_ERR_NONE ? RMS_INTRO : RMS_START_MESSAGE);
            }
            else
            {
                rmi->m_state = RMS_START_MESSAGE;
            }
            break;
        case RMS_INTRO:
            if (audiolib_play_get_state(rmi->m_intro_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = RMS_DONE;
            }
            else
            {
                hud_ui_radio_keep_sound_playing(rmi, rmi->m_intro_sound_play_id, RMS_START_MESSAGE);
            }
            break;
        case RMS_START_MESSAGE:
            if (audiolib_play_get_state(rmi->m_msg_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = RMS_DONE;
            }
            else
            {
                rmi->m_state = (char)(audiolib_play_set_pause(rmi->m_msg_sound_play_id, false) == AUDIOLIB_ERR_NONE ? RMS_MESSAGE : RMS_START_OUTRO);
            }
            break;
        case RMS_MESSAGE:
            if (audiolib_play_get_state(rmi->m_msg_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = RMS_DONE;
            }
            else
            {
                hud_ui_radio_keep_sound_playing(rmi, rmi->m_msg_sound_play_id, RMS_DONE);
            }
            break;
        case RMS_START_OUTRO:
            if (audiolib_play_get_state(rmi->m_outro_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = RMS_DONE;
            }
            else
            {
                audiolib_play_set_pause(rmi->m_outro_sound_play_id, false);
                rmi->m_state = RMS_OUTRO;
            }
            break;
        case RMS_OUTRO:
            if (audiolib_play_get_state(rmi->m_outro_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                rmi->m_state = RMS_DONE;
            }
            else
            {
                hud_ui_radio_keep_sound_playing(rmi, rmi->m_outro_sound_play_id, RMS_DONE);
            }
            break;
        case RMS_DONE:
            // Magic number: 0.1 s after the time left ran out, once the message's sound is gone.
            if (audiolib_play_get_state(rmi->m_msg_sound_play_id, nullptr) != AUDIOLIB_ERR_NONE && rmi->m_radio_message_time_left < -0.1f)
            {
                rmi->m_kill_me = true;
            }
            break;
    }

    if (rmi->m_kill_me)
    {
        rmi->m_state = RMS_DONE;
        rmi->m_radio_message_time_left = 0.0f;
        vint_data_item* item = vint_data_item::find(Data_handles_uihud[2]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(false));
            item->set_element(3, vint_variant(0.0f));
            item->set_element(10, vint_variant((int)radio_message_id(rmi)));
        }
        hud_ui_cancel_one_radio_message(rmi);
    }
    else if (game_is_paused())
    {
        vint_doc_info* info = ui_get_doc_info(Doc_handles[HDI_BASE]);
        if (info != nullptr && info->m_doc != nullptr)
        {
            info->m_doc->set_time_index_paused(true);
        }
        rmi->m_dirty = true;
    }
    else
    {
        vint_doc_info* info = ui_get_doc_info(Doc_handles[HDI_BASE]);
        if (info != nullptr && info->m_doc != nullptr && info->m_doc->is_time_index_paused())
        {
            info->m_doc->set_time_index_paused(false);
        }
        char name[0xC0] = {};
        if (rmi->m_dirty)
        {
            if (Hide_hud_ref_count < 1)
            {
                vint_data_item* item = vint_data_item::find(Data_handles_uihud[2]);
                if (item != nullptr)
                {
                    const char* message = rmi->m_radio_message;
                    unsigned int crc = rmi->m_crc;
                    char subtitle[0x400];
                    subtitle[0] = '\0';
                    if (rmi->m_display_as_subtitle)
                    {
                        crc = 0;
                        if (Subtitles_enabled)
                        {
                            hud_ui_get_subtitle_display_text(rmi->m_crc, rmi->m_display_name, true, subtitle, 0x400);
                            message = subtitle;
                        }
                    }
                    else if (rmi->m_display_name != nullptr)
                    {
                        // Original bug kept: no check against the 192 bytes of name.
                        memcpy(name, rmi->m_display_name, strlen(rmi->m_display_name));
                    }
                    rmi->m_char_uncover_time = rmi->m_char_uncover_time - dtime;
                    if (rmi->m_char_uncover_time <= 0.0f)
                    {
                        rmi->m_char_uncover_count++;
                        // Magic number: the seconds between two uncovered characters.
                        rmi->m_char_uncover_time = 0.06f;
                    }
                    const unsigned int freq_whole = (unsigned int)rmi->m_radio_freq;
                    // Magic numbers: one decimal of the frequency, 0.01 against rounding down.
                    const unsigned int freq_tenth = (unsigned int)(((rmi->m_radio_freq - (float)freq_whole) + 0.01) * 10.0);
                    item->set_element(0, vint_variant(true));
                    item->set_element(1, vint_variant((const char*)name));
                    item->set_element(2, vint_variant(message));
                    item->set_element(3, vint_variant(rmi->m_radio_message_time_left));
                    item->set_element(4, vint_variant((const char*)rmi->m_icon_name));
                    item->set_element(5, vint_variant(crc));
                    item->set_element(6, vint_variant(Subtitles_enabled));
                    item->set_element(7, vint_variant(!rmi->m_display_as_subtitle));
                    item->set_element(8, vint_variant(freq_whole));
                    item->set_element(9, vint_variant(freq_tenth));
                    item->set_element(10, vint_variant((int)radio_message_id(rmi)));
                }
            }
            else if (rmi->m_display_as_subtitle)
            {
                ui_add_subtitle(rmi->m_crc, rmi->m_display_name, (int)(rmi->m_radio_message_time_left * 1000.0f));
            }
            rmi->m_dirty = false;
        }
    }
    radio_message_cs.leave();
}

// 0x00895480
static void hud_ui_process_player_info(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[4]);
    if (item != nullptr)
    {
        const matrix orient = Cartographer.get_player_orientation(false);
        // Magic number: degrees per radian.
        float heading = orient.extract_heading() * 57.29583f;
        while (heading < 0.0f)
        {
            heading = heading + 360.0f;
        }
        item->set_element(0, vint_variant(heading));
    }
}

// 0x00895520
static void hud_ui_process_alert_level_info(float dtime)
{
    vint_data_item* item = vint_data_item::find(Data_handles_uihud[3]);
    if (item == nullptr)
    {
        return;
    }
    const char* name = cBlankString;
    district* dp = district_local_player();
    unsigned int hash = Control_morale_district_hash;
    if (dp != nullptr && Control_morale_district_hash == 0)
    {
        hash = dp->get_localized_name_hash();
        if (hash == 0)
        {
            name = dp->get_name();
        }
    }
    const gfm_alert_levels level = gsm_get_alert_level();
    const float percent = gsm_find_alert_value_percentage();
    const bool show = !Multiplayer;
    item->set_element(0, vint_variant(show));
    item->set_element(1, vint_variant(show));
    // Magic number: each of the 4 levels is a quarter of the meter.
    item->set_element(2, vint_variant((float)(int)level * 25.0f + percent * 25.0f));
    item->set_element(3, vint_variant(hash));
    item->set_element(4, vint_variant(name));
}

// 0x008F4BF0
static void hud_ui_process_data_items(float dtime)
{
    if (!Multiplayer && !wrecking_crew_is_active() && !cutscene_is_playing() && !cutscene_is_finishing())
    {
        hud_ui_process_morale_info(dtime);
        hud_ui_process_radio_messages(dtime);
        hud_ui_process_alert_level_info(dtime);
        hud_ui_process_player_info(dtime);
    }
    hud_ui_process_health_update(dtime);
    hud_ui_process_swap_info(dtime);
    hud_ui_process_use_action_info(dtime);
    hud_ui_process_item_pickup_info(dtime);
}

// 0x008B1CB0
static void hud_ui_process_data_items_paused(float dtime)
{
    hud_ui_process_use_action_info_paused(dtime);
    hud_ui_process_health_update(dtime);
}

// 0x00895680
void hud_ui_set_radio_message_icon(const char* icon_name)
{
    if (icon_name != nullptr)
    {
        Current_radio_icon_name = icon_name;
    }
}

// 0x00895690
void hud_ui_unset_radio_message_icon()
{
    Current_radio_icon_name = nullptr;
}

// 0x008E9810
radio_message_info* hud_ui_add_radio_message_common(float visible_time, int cue_id, unsigned char priority, bool player, bool display_as_subtitle, const char* persona_name, unsigned int crc, const char* msg, audiolib_result& alib_result, const vector* pos, bool play_intro, bool play_outro, human* hp)
{
    radio_message_info* rmi = hud_ui_get_free_radio_info();
    if (rmi == nullptr)
    {
        return nullptr;
    }
    if (Current_radio_icon_name != nullptr)
    {
        strncpy_safe(rmi->m_icon_name, Current_radio_icon_name, 0x40);
    }
    rmi->m_human = hp;
    rmi->m_msg_sound_cue_id = cue_id;
    rmi->m_radio_message_time_left = visible_time;
    int intro_cue;
    int outro_cue;
    if (player)
    {
        outro_cue = -1;
        intro_cue = Ga_cue_ids.player_radio_start;
    }
    else
    {
        outro_cue = Ga_cue_ids.radio_message_end;
        intro_cue = Ga_cue_ids.radio_message_start;
    }
    audiolib_cue_info cue_info;
    if (display_as_subtitle)
    {
        intro_cue = -1;
        outro_cue = -1;
    }
    else if (play_intro)
    {
        if (audiolib_cue_get_info(intro_cue, &cue_info) == AUDIOLIB_ERR_NONE)
        {
            rmi->m_radio_message_time_left = (float)cue_info.play_time[0] / 1000.0f + rmi->m_radio_message_time_left;
        }
    }
    else
    {
        intro_cue = -1;
    }
    if (audiolib_cue_get_info(rmi->m_msg_sound_cue_id, &cue_info) == AUDIOLIB_ERR_NONE)
    {
        rmi->m_radio_message_time_left = (float)cue_info.play_time[0] / 1000.0f + rmi->m_radio_message_time_left;
    }
    else
    {
        rmi->m_msg_sound_cue_id = -1;
    }
    // Magic number: at least a second on screen.
    if (rmi->m_radio_message_time_left < 1.0f)
    {
        rmi->m_radio_message_time_left = 1.0f;
    }
    audiolib_params params;
    // Magic numbers: audiolib_params flags as in rfg.exe (0x400: starts paused for the state machine above is the radio's own).
    params.flags = display_as_subtitle ? 6 : 0x436;
    params.callback_flags = 0x80;
    params.callback_data = (void*)(unsigned int)radio_message_id(rmi);
    params.priority = priority;
    audiolib_result result;
    if (play_intro)
    {
        params.callback_func = radio_msg_audio_callback_intro_done;
        rmi->m_intro_sound_play_id = audiolib_cue_play(intro_cue, &params, &result);
    }
    params.callback_func = radio_msg_audio_callback_msg_done;
    if (pos != nullptr)
    {
        params.pos = *pos;
        params.flags = params.flags | 1;
    }
    rmi->m_msg_sound_play_id = audiolib_cue_play(rmi->m_msg_sound_cue_id, &params, &result);
    alib_result = result;
    if (play_outro)
    {
        params.callback_func = radio_msg_audio_callback_outro_done;
        rmi->m_outro_sound_play_id = audiolib_cue_play(outro_cue, &params, &result);
    }
    bool found = true;
    const char* name = hud_ui_get_radio_persona_name(persona_name, &found);
    rmi->m_display_name = name;
    if (!found || name == nullptr)
    {
        rmi->m_display_name = "";
    }
    rmi->m_radio_freq = hud_ui_get_radio_persona_radio_freq(persona_name);
    rmi->m_crc = crc;
    if (msg == nullptr)
    {
        rmi->m_radio_message[0] = '\0';
    }
    else
    {
        strncpy_safe(rmi->m_radio_message, msg, 0x400);
    }
    rmi->m_display_as_subtitle = display_as_subtitle;
    rmi->m_state = RMS_START_INTRO;
    return rmi;
}

// 0x008E9B60
unsigned short hud_ui_add_radio_message(unsigned int crc, float visible_time, const int cue_id, unsigned char priority, bool player, bool display_as_subtitle, const char* persona_name, audiolib_result& alib_result, int& play_id, const vector* pos, bool play_intro, bool play_outro, human* hp)
{
    play_id = -1;
    if (crc != 0)
    {
        radio_message_info* rmi = hud_ui_add_radio_message_common(visible_time, cue_id, priority, player, display_as_subtitle, persona_name, crc, nullptr, alib_result, pos, play_intro, play_outro, hp);
        if (rmi != nullptr)
        {
            play_id = rmi->m_msg_sound_play_id;
            return radio_message_id(rmi);
        }
    }
    return 0;
}

// 0x008B1D10
bool hud_ui_is_radio_message_valid(unsigned short id, bool* visible, bool* radio_line)
{
    radio_message_info* rmi = id == 0 ? Radio_set.m_radio_info_pool.get_first(0) : RadioMsgID_to_RadioMsgInfo(id);
    if (rmi == nullptr)
    {
        return false;
    }
    if (visible != nullptr)
    {
        *visible = false;
    }
    if (radio_line != nullptr)
    {
        *radio_line = !rmi->m_display_as_subtitle;
    }
    if (rmi->m_state < RMS_START_INTRO || rmi->m_radio_message_time_left <= 0.0f)
    {
        return false;
    }
    if (visible != nullptr)
    {
        *visible = true;
    }
    return true;
}

// 0x008B1DC0
int hud_ui_radio_get_play_id_from_msg(unsigned short id)
{
    radio_message_info* rmi = RadioMsgID_to_RadioMsgInfo(id);
    return rmi != nullptr ? rmi->m_msg_sound_play_id : -1;
}

// 0x008B1E00
bool hud_ui_is_radio_message_active()
{
    return Radio_set.m_radio_info_pool.get_first(0) != nullptr;
}

// 0x008B1E30
int hud_ui_get_radio_message_remaining_time(unsigned short id)
{
    radio_message_info* rmi = id == 0 ? Radio_set.m_radio_info_pool.get_first(0) : RadioMsgID_to_RadioMsgInfo(id);
    if (rmi == nullptr)
    {
        return -1;
    }
    return (int)(rmi->m_radio_message_time_left * 1000.0f);
}

// A pool that is full gives up its oldest message.
template<int N>
static message_info* get_free_message(object_pool<message_info, N, 1>& pool)
{
    message_info* message = pool.alloc(0);
    if (message == nullptr)
    {
        message_info* oldest = pool.get_first(0);
        if (oldest != nullptr)
        {
            pool.free(oldest);
            message = pool.alloc(0);
        }
    }
    return message;
}

// 0x008D7F50
message_info* ui_get_free_primary_message()
{
    return get_free_message(Ui_message_set.m_primary_pool);
}

// 0x008D7FA0
message_info* ui_get_free_secondary_message()
{
    return get_free_message(Ui_message_set.m_secondary_pool);
}

// 0x008D7FF0
message_info* ui_get_free_user_message()
{
    return get_free_message(Ui_message_set.m_user_pool);
}

// 0x008C4F70
bool ui_primary_string_exists(unsigned int crc)
{
    Ui_message_set.m_primary_pool.thread_access.enter();
    for (message_info* message = Ui_message_set.m_primary_pool.get_first(0); message != nullptr; message = Ui_message_set.m_primary_pool.get_next(message))
    {
        if (message->m_crc == crc)
        {
            Ui_message_set.m_primary_pool.thread_access.leave();
            return true;
        }
    }
    Ui_message_set.m_primary_pool.thread_access.leave();
    return false;
}

// 0x008C4FF0
bool ui_primary_string_exists(const wchar_t* text)
{
    if (text == nullptr)
    {
        return false;
    }
    Ui_message_set.m_primary_pool.thread_access.enter();
    for (message_info* message = Ui_message_set.m_primary_pool.get_first(0); message != nullptr; message = Ui_message_set.m_primary_pool.get_next(message))
    {
        if (_v_stricmp(message->m_text, text) == 0)
        {
            Ui_message_set.m_primary_pool.thread_access.leave();
            return true;
        }
    }
    Ui_message_set.m_primary_pool.thread_access.leave();
    return false;
}

// 0x008C5070
bool ui_secondary_string_exists(const wchar_t* text)
{
    if (text == nullptr)
    {
        return false;
    }
    Ui_message_set.m_secondary_pool.thread_access.enter();
    for (message_info* message = Ui_message_set.m_secondary_pool.get_first(0); message != nullptr; message = Ui_message_set.m_secondary_pool.get_next(message))
    {
        if (_v_stricmp(message->m_text, text) == 0)
        {
            Ui_message_set.m_secondary_pool.thread_access.leave();
            return true;
        }
    }
    Ui_message_set.m_secondary_pool.thread_access.leave();
    return false;
}

// 0x008C50F0
bool ui_secondary_message_is_playing_fadeout(const wchar_t* text)
{
    if (text == nullptr)
    {
        return false;
    }
    Ui_message_set.m_secondary_pool.thread_access.enter();
    for (message_info* message = Ui_message_set.m_secondary_pool.get_first(0); message != nullptr; message = Ui_message_set.m_secondary_pool.get_next(message))
    {
        if (_v_stricmp(message->m_text, text) == 0)
        {
            const float fadeout = message->m_use_secondary_anim ? message->m_fadeout2_time : message->m_fadeout_time;
            if ((message->m_display_time - fadeout) - 0.5f < message->m_running_time)
            {
                Ui_message_set.m_secondary_pool.thread_access.leave();
                return true;
            }
        }
    }
    Ui_message_set.m_secondary_pool.thread_access.leave();
    return false;
}

// The next message handle: -1 stays invalid (three copies inlined in rfg.exe).
static hud_message_handle next_message_handle()
{
    Hud_message_counter++;
    if (Hud_message_counter == 0xFFFFFFFF)
    {
        Hud_message_counter = 0;
    }
    return (hud_message_handle)Hud_message_counter;
}

// 0x008D8040
static message_info* ui_add_primary_message_common(float display_time, void* data1, void* data2, int* sound)
{
    message_info* message = ui_get_free_primary_message();
    if (message == nullptr)
    {
        return nullptr;
    }
    message->m_data1 = data1;
    message->m_data2 = data2;
    message->m_audio_cue = sound;
    message->m_dirty = true;
    message->m_display_time = display_time;
    message->m_running_time = 0.0f;
    // Magic number: the default display time.
    if (display_time < 0.0f)
    {
        message->m_display_time = 3.0f;
    }
    message->m_display_time = message->m_fadeout_time + message->m_fadein_time + 0.5f + message->m_display_time;
    // Magic number: the line spacing.
    message->m_height = (int)((float)font_get_render_height(Ui_message_set.m_primary_font) * message->m_obj_scale.y * 1.35f);
    message->m_handle = next_message_handle();
    return message;
}

// 0x008D8170
hud_message_handle ui_add_primary_message(unsigned int crc, float display_time, void* data1, void* data2, int* sound)
{
    if (Ui_message_set.m_msg_doc == nullptr || ui_primary_string_exists(crc))
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message_info* message = ui_add_primary_message_common(display_time, data1, data2, sound);
    if (message == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message->m_crc = crc;
    message->m_text[0] = L'\0';
    return message->m_handle;
}

// 0x008D81E0
hud_message_handle ui_add_primary_message(const wchar_t* text, float display_time, void* data1, void* data2, int* sound)
{
    if (Ui_message_set.m_msg_doc == nullptr || text == nullptr || ui_primary_string_exists(text))
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message_info* message = ui_add_primary_message_common(display_time, data1, data2, sound);
    if (message == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    wcsncpy(message->m_text, text, 0x200);
    message->m_text[0x1FF] = L'\0';
    return message->m_handle;
}

// 0x008D8270
hud_message_handle ui_add_secondary_message(const wchar_t* text, float display_time, bool use_secondary_anim, bool force_redisplay)
{
    if (Ui_message_set.m_msg_doc == nullptr || text == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    if (!force_redisplay && ui_secondary_string_exists(text) && !ui_secondary_message_is_playing_fadeout(text))
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    if (camera_get_mode() == CAMERA_SATELLITE_MODE)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message_info* message = ui_get_free_secondary_message();
    if (message == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message->m_display_time = display_time;
    message->m_running_time = 0.0f;
    if (display_time < 0.0f)
    {
        message->m_display_time = 3.0f;
    }
    message->m_use_secondary_anim = use_secondary_anim;
    // Original bug kept: the secondary animation adds the first animation's times and the other way round.
    if (use_secondary_anim)
    {
        message->m_display_time = message->m_fadeout_time + message->m_fadein_time + 0.5f + message->m_display_time;
    }
    else
    {
        message->m_display_time = message->m_fadeout2_time + message->m_fadein2_time + 0.5f + message->m_display_time;
    }
    message->m_dirty = true;
    wcsncpy(message->m_text, text, 0x200);
    message->m_text[0x1FF] = L'\0';
    message->m_height = (int)((float)font_get_render_height(Ui_message_set.m_secondary_font) * message->m_obj_scale.y * 1.35f);
    message->m_handle = next_message_handle();
    return message->m_handle;
}

// 0x008D83D0
hud_message_handle ui_add_user_message(const wchar_t* text, hud_user_message_types type, float position_x, float position_y, bool outlined, float lifespan)
{
    if (Ui_message_set.m_msg_doc == nullptr || text == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message_info* message = ui_get_free_user_message();
    if (message == nullptr)
    {
        return INVALID_HUD_MESSAGE_HANDLE;
    }
    message->m_position.x = position_x;
    message->m_position.y = position_y;
    message->m_display_time = lifespan;
    message->m_user_message_type = type;
    message->m_running_time = 0.0f;
    message->m_outlined = outlined;
    message->m_dirty = true;
    // Original bug kept: the text is the format string.
    _snprintf_safe(message->m_text, 0x200, text);
    message->m_width = (int)((float)ui_get_usage_character_width(text, Ui_message_set.m_user_font) * message->m_obj_scale.x);
    message->m_height = (int)((float)ui_get_usage_character_height(text, Ui_message_set.m_user_font) * message->m_obj_scale.y * 1.35f);
    message->m_handle = next_message_handle();
    return message->m_handle;
}

// Finds a message by handle under the pool's lock; the lock stays held for the caller when found.
template<int N>
static message_info* find_message_locked(object_pool<message_info, N, 1>& pool, hud_message_handle handle)
{
    for (message_info* message = pool.get_first(0); message != nullptr; message = pool.get_next(message))
    {
        if (message->m_handle == handle)
        {
            return message;
        }
    }
    return nullptr;
}

// 0x008D84F0
void ui_remove_primary_message(hud_message_handle handle)
{
    object_pool<message_info,3,1>& pool = Ui_message_set.m_primary_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (group != nullptr)
        {
            set_alpha(group, 0.0f);
        }
        pool.free(message);
    }
    pool.thread_access.leave();
}

// 0x008D85B0
void ui_remove_secondary_message(hud_message_handle handle)
{
    object_pool<message_info,3,1>& pool = Ui_message_set.m_secondary_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (group != nullptr)
        {
            set_alpha(group, 0.0f);
        }
        pool.free(message);
    }
    pool.thread_access.leave();
}

// 0x008D8670
void ui_remove_user_message(hud_message_handle handle)
{
    object_pool<message_info,11,1>& pool = Ui_message_set.m_user_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object(message->m_obj_handle, Ui_message_set.m_msg_doc);
        if (group != nullptr)
        {
            set_visible(group, false);
        }
        pool.free(message);
    }
    pool.thread_access.leave();
}

// 0x008D8730
hud_message_handle ui_change_primary_message(hud_message_handle handle, const wchar_t* text, float display_time, void* data1, void* data2)
{
    hud_message_handle result = INVALID_HUD_MESSAGE_HANDLE;
    object_pool<message_info,3,1>& pool = Ui_message_set.m_primary_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        message->m_data1 = data1;
        message->m_data2 = data2;
        message->m_dirty = true;
        message->m_display_time = display_time;
        if (display_time < 0.0f)
        {
            message->m_display_time = 3.0f;
        }
        const float fadein = message->m_fadein_time;
        message->m_display_time = message->m_fadeout_time + fadein + 0.5f + message->m_display_time;
        // Back to the end of the fade in; never 0, which would count as the first frame.
        if (fadein <= message->m_running_time)
        {
            message->m_running_time = fadein;
            if (fadein <= 0.0f)
            {
                message->m_running_time = 0.1f;
            }
        }
        wcsncpy(message->m_text, text, 0x200);
        message->m_text[0x1FF] = L'\0';
        message->m_width = (int)((float)ui_get_usage_character_width(text, Ui_message_set.m_primary_font) * message->m_obj_scale.x);
        message->m_height = (int)((float)font_get_render_height(Ui_message_set.m_primary_font) * message->m_obj_scale.y);
        result = message->m_handle;
        pool.move(message, 0);
    }
    pool.thread_access.leave();
    return result;
}

// 0x008C51B0
hud_message_handle ui_change_secondary_message(hud_message_handle handle, const wchar_t* text, float display_time)
{
    hud_message_handle result = INVALID_HUD_MESSAGE_HANDLE;
    if (Ui_message_set.m_msg_doc == nullptr || text == nullptr)
    {
        return result;
    }
    object_pool<message_info,3,1>& pool = Ui_message_set.m_secondary_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        message->m_display_time = display_time;
        if (display_time < 0.0f)
        {
            message->m_display_time = 3.0f;
        }
        const float fadein = message->m_fadein_time;
        message->m_display_time = message->m_fadeout_time + fadein + 0.5f + message->m_display_time;
        if (fadein <= message->m_running_time)
        {
            message->m_running_time = fadein;
            if (fadein <= 0.0f)
            {
                message->m_running_time = 0.1f;
            }
        }
        message->m_dirty = true;
        wcsncpy(message->m_text, text, 0x200);
        message->m_text[0x1FF] = L'\0';
        message->m_height = (int)((float)font_get_render_height(Ui_message_set.m_secondary_font) * message->m_obj_scale.y * 1.35f);
        result = message->m_handle;
    }
    pool.thread_access.leave();
    return result;
}

// 0x008D88D0
hud_message_handle ui_change_user_message(hud_message_handle handle, const wchar_t* text)
{
    hud_message_handle result = INVALID_HUD_MESSAGE_HANDLE;
    object_pool<message_info,11,1>& pool = Ui_message_set.m_user_pool;
    pool.thread_access.enter();
    message_info* message = find_message_locked(pool, handle);
    if (message != nullptr)
    {
        message->m_running_time = 0.0f;
        message->m_dirty = true;
        // Original bug kept: the text is the format string.
        _snprintf_safe(message->m_text, 0x200, text);
        message->m_width = (int)((float)ui_get_usage_character_width(text, Ui_message_set.m_user_font) * message->m_obj_scale.x);
        message->m_height = (int)((float)font_get_render_height(Ui_message_set.m_user_font) * message->m_obj_scale.y);
        result = message->m_handle;
        pool.move(message, 0);
    }
    pool.thread_access.leave();
    return result;
}

// 0x008C5320
void* ui_primary_message_get_data1(hud_message_handle handle)
{
    void* data = nullptr;
    if (Ui_message_set.m_msg_doc != nullptr)
    {
        Ui_message_set.m_primary_pool.thread_access.enter();
        message_info* message = find_message_locked(Ui_message_set.m_primary_pool, handle);
        if (message != nullptr)
        {
            data = message->m_data1;
        }
        Ui_message_set.m_primary_pool.thread_access.leave();
    }
    return data;
}

// 0x008C53A0
void* ui_primary_message_get_data2(hud_message_handle handle)
{
    void* data = nullptr;
    if (Ui_message_set.m_msg_doc != nullptr)
    {
        Ui_message_set.m_primary_pool.thread_access.enter();
        message_info* message = find_message_locked(Ui_message_set.m_primary_pool, handle);
        if (message != nullptr)
        {
            data = message->m_data2;
        }
        Ui_message_set.m_primary_pool.thread_access.leave();
    }
    return data;
}

// 0x008956A0
int hud_ui_get_document_handle(hud_doc_info info)
{
    if (info < HDI_LAST)
    {
        return Doc_handles[info];
    }
    return -1;
}

// 0x009167A0
void hud_ui_reload_items()
{
    if (!Enabled)
    {
        return;
    }
    if (!Active_uihud)
    {
        for (int i = 0; i < HDI_LAST; i++)
        {
            if ((Multiplayer || wrecking_crew_is_active()) && i == HDI_BASE)
            {
                Doc_handles[HDI_BASE] = -1;
            }
            else if (!Ui_unload_assets_only)
            {
                const int handle = ui_load_document(Hud_doc_names[i], DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
                if (handle > -1)
                {
                    Doc_handles[i] = handle;
                }
            }
            else
            {
                ui_reload_doc_assets(Doc_handles[i]);
            }
        }
        if (Control_id_uihud == 0)
        {
            Control_id_uihud = ui_request_control_id();
        }
    }
    hud_ui_weapons_load_items(Ui_unload_assets_only);
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        hud_ui_missions_load_items(Ui_unload_assets_only);
    }
    if (Multiplayer)
    {
        hud_ui_multiplayer_load_items(Ui_unload_assets_only);
    }
    if (wrecking_crew_is_active())
    {
        hud_ui_wrecking_crew_load_items(Ui_unload_assets_only);
    }
    switch (Current_module)
    {
        case HUD_MODULE_NONE:
            break;
        case HUD_MODULE_SATELLITE:
            hud_ui_satellite_load_items(Ui_unload_assets_only);
            break;
        case HUD_MODULE_RAIL_DRIVER:
            hud_ui_rail_driver_load_items(Ui_unload_assets_only);
            break;
        case HUD_MODULE_SNIPER:
            hud_ui_sniper_load_items(Ui_unload_assets_only);
            break;
        default:
            break;
    }
    Active_uihud = true;
}

// 0x00904690
void hud_ui_unload_items()
{
    if (Active_uihud)
    {
        for (int handle : Doc_handles)
        {
            if (!Ui_unload_assets_only)
            {
                ui_unload_document(handle, DFOT_NONE, 0.5f);
            }
            else
            {
                ui_unload_doc_assets(handle);
            }
        }
        Unloaded_assets_only = Ui_unload_assets_only && Data_items_created_uihud;
        if (!Ui_unload_assets_only)
        {
            ui_relinquish_control_id(Control_id_uihud);
            Control_id_uihud = 0;
            ui_deactivate_use_messages();
            Data_items_created_uihud = false;
        }
        Unloaded = true;
        Active_uihud = false;
    }
    hud_ui_weapons_unload_items(Ui_unload_assets_only);
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        hud_ui_missions_unload_items(Ui_unload_assets_only);
    }
    if (Multiplayer)
    {
        hud_ui_multiplayer_unload_items(Ui_unload_assets_only);
    }
    if (wrecking_crew_is_active())
    {
        hud_ui_wrecking_crew_unload_items(Ui_unload_assets_only);
    }
    switch (Current_module)
    {
        case HUD_MODULE_NONE:
            break;
        case HUD_MODULE_SATELLITE:
            hud_ui_satellite_unload_items(Ui_unload_assets_only);
            break;
        case HUD_MODULE_RAIL_DRIVER:
            hud_ui_rail_driver_unload_items(Ui_unload_assets_only);
            break;
        case HUD_MODULE_SNIPER:
            hud_ui_sniper_unload_items(Ui_unload_assets_only);
            break;
        default:
            break;
    }
    Cant_load = false;
}

// 0x008956C0
bool hud_ui_is_loaded()
{
    return Cant_load || (!Unloaded && Data_items_created_uihud);
}

// 0x00918D30
bool hud_ui_set_loaded_module(hud_modules module)
{
    if (Current_module != module)
    {
        switch (Current_module)
        {
            case HUD_MODULE_SATELLITE: hud_ui_satellite_exit(); break;
            case HUD_MODULE_RAIL_DRIVER: hud_ui_rail_driver_exit(); break;
            case HUD_MODULE_SNIPER: hud_ui_sniper_exit(); break;
            default: break;
        }
    }
    if (Current_module == HUD_MODULE_SATELLITE && module == HUD_MODULE_NONE)
    {
        // Leaving the satellite view back to a scoped weapon brings its module back.
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            weapon* wp = pp->get_equipped_weapon();
            if (wp != nullptr && wp->w_info != nullptr)
            {
                const char* name = wp->w_info->name;
                if (v_stricmp(name, "rail_driver") == 0)
                {
                    module = HUD_MODULE_RAIL_DRIVER;
                    Current_module = HUD_MODULE_NONE;
                }
                else if (v_stricmp(name, "sniper_rifle") == 0)
                {
                    module = HUD_MODULE_SNIPER;
                    Current_module = HUD_MODULE_NONE;
                }
            }
        }
    }
    // rfg.exe's nested jump tables (0x00918E8C): entering from a module that shares nothing with the new one; the
    // satellite view keeps a loaded module running, and the same module is not entered twice.
    switch (module)
    {
        case HUD_MODULE_SATELLITE:
            if (Current_module != HUD_MODULE_SATELLITE)
            {
                hud_ui_satellite_enter();
            }
            break;
        case HUD_MODULE_RAIL_DRIVER:
            if (Current_module == HUD_MODULE_NONE || Current_module == HUD_MODULE_SNIPER)
            {
                hud_ui_rail_driver_enter();
            }
            break;
        case HUD_MODULE_SNIPER:
            if (Current_module == HUD_MODULE_NONE || Current_module == HUD_MODULE_RAIL_DRIVER)
            {
                hud_ui_sniper_enter();
            }
            break;
        default:
            break;
    }
    Current_module = module;
    return true;
}

// 0x008956F0
hud_modules hud_ui_get_loaded_module()
{
    return Current_module;
}

// 0x00918ED0
void hud_ui_enter()
{
    if (Multiplayer && multi_is_dedicated_server())
    {
        Show_minimap = false;
        Enabled = false;
        return;
    }
    Show_minimap = true;
    Enabled = true;
    ui_init_hud_ui_message_set();
    ui_init_radio_msg_info_set();
    ui_init_use_messages();
    ui_init_swap_menu_set();
    bool loaded = true;
    for (int i = 0; i < HDI_LAST; i++)
    {
        if ((Multiplayer || wrecking_crew_is_active()) && i == HDI_BASE)
        {
            Doc_handles[HDI_BASE] = -1;
        }
        else
        {
            const int handle = ui_load_document(Hud_doc_names[i], DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle < 0)
            {
                loaded = false;
            }
            else
            {
                Doc_handles[i] = handle;
            }
        }
    }
    if (loaded)
    {
        ui_clear_active_primary_messages();
        ui_clear_active_secondary_messages();
        ui_clear_active_user_messages();
        ui_clear_item_pickup_messages();
        hud_ui_cancel_radio_messages();
        if (!Multiplayer && !wrecking_crew_is_active())
        {
            hud_ui_missions_enter();
            hud_ui_missions_load_items(false);
        }
        if (Multiplayer)
        {
            hud_ui_multiplayer_enter();
        }
        if (wrecking_crew_is_active())
        {
            hud_ui_wrecking_crew_enter();
        }
        hud_ui_weapons_enter();
        hud_ui_weapons_load_items(false);
        Active_uihud = true;
    }
    Control_id_uihud = ui_request_control_id();
    Unloaded_assets_only = false;
    Data_items_created_uihud = false;
    Cant_load = false;
    Current_module = HUD_MODULE_NONE;
    Morale_percent_updated = false;
    Control_value_updated = false;
    In_radiation = false;
    Unloaded = true;
    Previous_health = 1.0f;
}

// 0x00916940
void hud_ui_process()
{
    if (!Enabled)
    {
        return;
    }
    if (Unloaded && Active_uihud)
    {
        bool loaded = true;
        bool error = false;
        for (int i = 0; i < HDI_LAST; i++)
        {
            if ((Multiplayer || wrecking_crew_is_active()) && i == HDI_BASE)
            {
                Doc_handles[HDI_BASE] = -1;
                continue;
            }
            switch (ui_get_doc_asset_state(Doc_handles[i]))
            {
                case ST_UNLOADED:
                case ST_ERROR:
                    error = true;
                    loaded = false;
                    break;
                case ST_LOADING:
                    loaded = false;
                    break;
                default:
                    break;
            }
        }
        if (error)
        {
            hud_ui_unload_items();
            Active_uihud = false;
            Cant_load = true;
        }
        if (loaded)
        {
            Unloaded = false;
            if (!Data_items_created_uihud)
            {
                ui_activate_use_messages();
                ui_clear_item_pickup_messages();
                hud_ui_create_data_items();
                hud_ui_create_data_groups();
                Data_items_created_uihud = true;
            }
            for (int handle : Doc_handles)
            {
                if (handle != -1)
                {
                    ui_fade_in_doc(handle, DFIT_ALPHA, 0.5f);
                }
            }
        }
    }
    if (Cant_load)
    {
        hud_ui_process_radio_messages(Frametime_real);
        return;
    }
    bool input_only = false;
    if ((!Unloaded && Active_uihud) || (Multiplayer && Data_items_created_uihud))
    {
        farray<int,16> handles;
        const bool paused = game_is_paused();
        district* dp = district_local_player();
        unsigned int district_handle = Control_district_handle;
        if (dp != nullptr)
        {
            district_handle = dp->get_handle();
            if (Control_district_handle != 0xFFFFFFFF && district_handle != Control_district_handle)
            {
                hud_ui_change_control(gfm_get_edf_control_display_value(dp), gfm_get_edf_control_percent(dp), 0, 0.0f, district_handle, false, 0);
            }
        }
        Control_district_handle = district_handle;
        switch (Current_module)
        {
            case HUD_MODULE_SATELLITE: hud_ui_satellite_process(); break;
            case HUD_MODULE_RAIL_DRIVER: hud_ui_rail_driver_process(); break;
            case HUD_MODULE_SNIPER: hud_ui_sniper_process(); break;
            default: break;
        }
        hud_ui_unpause_radio_messages();
        if (!Multiplayer && !wrecking_crew_is_active())
        {
            hud_ui_missions_process(Frametime_real);
        }
        if (Multiplayer)
        {
            hud_ui_multiplayer_process(Frametime_real);
        }
        if (wrecking_crew_is_active())
        {
            hud_ui_wrecking_crew_process(Frametime_real);
        }
        hud_ui_weapons_process(Frametime_real);
        if (paused)
        {
            hud_ui_process_data_items_paused(Frametime_real);
        }
        else
        {
            hud_ui_process_data_items(Frametime_real);
            ui_process_primary_messages(Frametime_real);
            ui_process_secondary_messages(Frametime_real);
            ui_process_user_messages(Frametime_real);
        }
        for (int handle : Doc_handles)
        {
            handles.add(handle);
        }
        ui_add_for_processing(handles);
        input_only = paused && !Multiplayer;
    }
    else
    {
        switch (Current_module)
        {
            case HUD_MODULE_SATELLITE: hud_ui_satellite_process(); break;
            case HUD_MODULE_RAIL_DRIVER: hud_ui_rail_driver_process(); break;
            case HUD_MODULE_SNIPER: hud_ui_sniper_process(); break;
            default: break;
        }
        hud_ui_pause_radio_messages();
    }
    ui_process(Control_id_uihud, input_only);
}

// 0x00904880
void hud_ui_render()
{
    if (Unloaded || !Enabled)
    {
        ui_fade_to_black_render();
        return;
    }
    farray<int,16> handles;
    for (int handle : Doc_handles)
    {
        handles.add(handle);
    }
    const bool hidden = hud_is_hidden();
    const bool menu_up = game_is_paused() && (!Tool_tip_queue[Current_tool_tip].running || (Utility_doc_handle != -1 && Message_box_info_set.msgbox_info_pool.get_count(0) != 0));
    const bool show = !menu_up && gameseq_get_state() != GS_INGAME_OPTIONS && gameseq_get_previous_state() != GS_INGAME_OPTIONS;
    switch (Current_module)
    {
        case HUD_MODULE_SATELLITE: hud_ui_satellite_render(); break;
        case HUD_MODULE_RAIL_DRIVER: hud_ui_rail_driver_render(); break;
        case HUD_MODULE_SNIPER: hud_ui_sniper_render(); break;
        default: break;
    }
    if (!hidden)
    {
        if (show)
        {
            hud_ui_show_paused_elements(true);
            ui_add_for_rendering(handles);
            if (!Multiplayer && !wrecking_crew_is_active())
            {
                hud_ui_missions_render();
            }
            if (Multiplayer)
            {
                hud_ui_multiplayer_render();
            }
            if (wrecking_crew_is_active())
            {
                hud_ui_wrecking_crew_render();
            }
            hud_ui_weapons_render();
            ui_render(Control_id_uihud, false);
            return;
        }
        const unsigned int top = Controller_slots.size() < 1 ? 1 : Controller_slots[Controller_slots.size() - 1];
        if (top == Control_id_uihud)
        {
            hud_ui_show_paused_elements(false);
            ui_render(Control_id_uihud, false);
        }
    }
    else
    {
        ui_render(Control_id_uihud, false);
    }
    // Hidden or behind a menu: the documents stay faded in, so they show at once when the HUD comes back.
    hud_ui_missions_get_doc_handles(handles); // rfg.exe inlines it
    hud_ui_weapons_get_doc_handles(handles);
    for (int i = 0; i < handles.size(); i++)
    {
        ui_fade_in_doc(handles[i], DFIT_ALPHA, 0.5f);
    }
}

// 0x00919200
void hud_ui_exit()
{
    if (!Enabled)
    {
        return;
    }
    hud_ui_cancel_radio_messages();
    for (int& handle : Doc_handles)
    {
        if (handle != -1)
        {
            ui_unload_document(handle, DFOT_NONE, 0.5f);
            handle = -1;
        }
    }
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        hud_ui_missions_unload_items(false);
    }
    if (Multiplayer)
    {
        hud_ui_multiplayer_unload_items(false);
    }
    if (wrecking_crew_is_active())
    {
        hud_ui_wrecking_crew_unload_items(false);
    }
    hud_ui_weapons_unload_items(false);
    hud_ui_set_loaded_module(HUD_MODULE_NONE);
    ui_deactivate_use_messages();
    ui_clear_active_primary_messages();
    ui_clear_active_secondary_messages();
    ui_clear_active_user_messages();
    ui_clear_item_pickup_messages();
    ui_relinquish_control_id(Control_id_uihud);
    Control_id_uihud = 0;
    hud_reset_damage_indicators();
    Hide_hud_ref_count = 0;
    Unloaded = true;
    Enabled = false;
}

// 0x00895700
void hud_ui_add_item_pickup_message(const item_pickup_info* pickup_info_list, int num_pickups)
{
    if ((unsigned int)num_pickups >= 4)
    {
        return;
    }
    // The same item again while the last one is showing adds up.
    const bool combine = Item_pickup_set.updated && num_pickups == 1 && Item_pickup_set.num_icons == 1 && pickup_info_list->item_name != nullptr && v_stricmp(Item_pickup_set.icon_info[0].item_name, pickup_info_list->item_name) == 0;
    int count = 0;
    for (int i = 0; i < num_pickups; i++)
    {
        const item_pickup_info& pickup = pickup_info_list[i];
        if (pickup.item_name == nullptr)
        {
            continue;
        }
        item_icon_info& icon = Item_pickup_set.icon_info[count];
        icon.full = pickup.num_picked_up == -1;
        strncpy(icon.item_name, pickup.item_name, 0x40);
        unsigned int amount = pickup.num_picked_up;
        if (combine)
        {
            amount += icon.amount_added;
        }
        if (strcmp(pickup.item_name, "ui_hud_weapon_icon_gutter") == 0)
        {
            icon.item_string[0] = '\0';
        }
        else if (icon.full)
        {
            _snprintf_safe(icon.item_string, 0x10, cFullmsg);
        }
        else
        {
            if (v_stricmp(pickup.item_name, "ui_hud_icon_salvage") == 0)
            {
                _snprintf_safe(icon.item_string, 0x10, "%i", pickup.total_val);
                // Magic number: 3 or more shows the amount.
                icon.is_salvage = pickup.num_picked_up >= 3;
            }
            else
            {
                _snprintf_safe(icon.item_string, 0x10, "+ %i", amount);
                icon.is_salvage = false;
            }
            icon.amount_added = amount;
        }
        count++;
    }
    if (count > 0)
    {
        Item_pickup_set.updated = true;
        Item_pickup_set.num_icons = count;
    }
}

// 0x00895870
void hud_ui_set_swap_menu_active(bool active)
{
    Swap_menu_set.m_show_swap_menu = active;
}

// 0x00895880
bool hud_ui_is_swap_menu_active()
{
    return Swap_menu_set.m_show_swap_menu;
}

// 0x008B1EA0
void hud_ui_set_swap_data(unsigned int primary, unsigned int secondary, bool weapon_swap)
{
    Swap_menu_set.m_weapon_swap = weapon_swap;
    if (primary == 0xFFFFFFFF)
    {
        return;
    }
    object* primary_obj = World.object_from_handle(primary);
    if (primary_obj == nullptr || primary_obj->get_obj_type() != OT_ITEM || secondary == 0xFFFFFFFF)
    {
        return;
    }
    if (!weapon_swap && primary_obj->get_sub_type() != OT_SUB_ITEM_MULTI_BACKPACK)
    {
        return;
    }
    object* secondary_obj = World.object_from_handle(secondary);
    if (secondary_obj == nullptr || secondary_obj->get_obj_type() != OT_ITEM)
    {
        return;
    }
    if (weapon_swap)
    {
        if (secondary_obj->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            // An item that stands for a weapon (a safehouse weapon) names its weapon.
            const inv_item_info* inv_info = ((item*)primary_obj)->info->safehouse_weapon;
            const weapon_info* w_info = inv_info != nullptr ? inv_info->w_info : ((weapon*)primary_obj)->w_info;
            Swap_menu_set.m_primary_slot.w_info_handle = weapons_get_weapon_info_handle(w_info);
            Swap_menu_set.m_secondary_slot.w_info_handle = weapons_get_weapon_info_handle(((weapon*)secondary_obj)->w_info);
        }
    }
    else if (secondary_obj->get_sub_type() == OT_SUB_ITEM_MULTI_BACKPACK)
    {
        Swap_menu_set.m_primary_slot.backpack_type = ((multi_object_backpack*)primary_obj)->get_backpack_info()->unique_id;
        Swap_menu_set.m_secondary_slot.backpack_type = ((multi_object_backpack*)secondary_obj)->get_backpack_info()->unique_id;
    }
}

// 0x008B1FC0
void hud_ui_change_morale(float new_morale_percent, float old_morale_percent, float morale_change, unsigned int district_handle)
{
    player* pp = player_get_local_player();
    district* dp = nullptr;
    if (district_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(district_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_DISTRICT)
        {
            dp = (district*)objp;
        }
    }
    if (dp == nullptr && pp != nullptr)
    {
        dp = district_get_from_pos(pp->pos);
    }
    if (dp != nullptr && !dp->get_disable_morale_change())
    {
        Control_morale_district_hash = dp->get_localized_name_hash();
        if (!Morale_percent_updated)
        {
            Morale_old_percent = old_morale_percent;
        }
        Morale_current_percent = new_morale_percent;
        Morale_change = morale_change;
        if (camera_get_mode() != CAMERA_SATELLITE_MODE)
        {
            Morale_percent_updated = true;
        }
    }
}

// 0x008B2060
void hud_ui_change_control(int new_control_value, float new_control_percent, int control_value_change, float delta_control_percent, unsigned int district_handle, bool mission_completed, int missions_unlocked)
{
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        // Unlocked missions in another district than the player's are not shown.
        district* here = district_get_from_pos(pp->pos);
        if (here != nullptr && here->get_handle() != district_handle && missions_unlocked > 0)
        {
            return;
        }
    }
    if (district_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(district_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_DISTRICT)
    {
        return;
    }
    district* dp = (district*)objp;
    if (dp->get_disable_control_change())
    {
        return;
    }
    const bool other_district = dp->get_localized_name_hash() != Control_morale_district_hash;
    Control_num_missions = 0;
    Control_missions_left = 0;
    Control_missions_locked = 0;
    Control_district_handle = dp->get_handle();
    Control_morale_district_hash = dp->get_localized_name_hash();
    missions_get_hud_info(Control_district_handle, Control_num_missions, Control_missions_left, Control_missions_locked);
    bool set_current = true;
    if (!Control_value_updated || other_district)
    {
        Control_delta_value = control_value_change;
        Control_delta_percent = delta_control_percent;
        Control_mission_complete = mission_completed;
        Control_missions_unlocked = missions_unlocked;
    }
    else
    {
        Control_mission_complete = Control_mission_complete || mission_completed;
        Control_missions_unlocked = Control_missions_unlocked + missions_unlocked;
        set_current = Control_current_value + control_value_change == new_control_value;
    }
    if (set_current)
    {
        Control_current_percent = new_control_percent;
        Control_current_value = new_control_value;
    }
    Control_value_updated = camera_get_mode() != CAMERA_SATELLITE_MODE;
    Control_old_value = new_control_value;
}

// 0x00895890
void hud_ui_convert_time_for_output(char* output, int output_size, int time_ms)
{
    const int minutes = time_ms / 60000;
    _snprintf_safe(output, output_size, "%d:%02d", minutes, (time_ms - minutes * 60000) / 1000);
}

// 0x008958E0
void hud_ui_convert_time_for_output(char* output, int output_size, float time_seconds)
{
    const int time_ms = (int)((double)time_seconds * 1000.0);
    const int minutes = time_ms / 60000;
    _snprintf_safe(output, output_size, "%d:%02d", minutes, (time_ms - minutes * 60000) / 1000);
}

// 0x00883610
item_icon_info::item_icon_info()
{
    full = false;
    item_string[0] = '\0';
    item_name[0] = '\0';
    amount_added = 0;
    data_item_handle = 0;
    is_salvage = false;
}

// 0x00883630
item_info_set::item_info_set()
{
    updated = false;
    num_icons = 0;
    prev_num_icons = 0;
}

// 0x00753D10
item_pickup_info::item_pickup_info(const char* name, int picked_up, int total)
{
    item_name = name;
    num_picked_up = picked_up;
    total_val = total;
}

// 0x008B0CE0
radio_message_info::radio_message_info()
{
    clear();
}

// 0x008B0CF0
message_info::message_info()
{
    clear();
}

// 0x008E80A0
radio_msg_info_set::radio_msg_info_set() : m_radio_info_pool(nullptr, &radio_message_info::clear, 0)
{
}

// 0x008C4F20
radio_msg_info_set::~radio_msg_info_set()
{
}

// 0x008E80C0
hud_ui_message_set::hud_ui_message_set()
    : m_primary_pool(nullptr, &message_info::clear, 0), m_secondary_pool(nullptr, &message_info::clear, 0), m_user_pool(nullptr, &message_info::clear, 0)
{
}

// 0x008C4F30
hud_ui_message_set::~hud_ui_message_set()
{
}
