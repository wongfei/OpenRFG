// Original: volition/rfg/code/cutscene/cutscene_options.cpp
//
// Shot options: parsed from each shot's table and dispatched by name at the start of the shot. Retail keeps only the two
// Bink options: the other 21 handlers have empty bodies in rfg.exe.

#include "volition/rfg/code/cutscene/cutscene_options.h"

#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// size 0x8
struct cs_option_function_table_entry
{
    const char* function_name; // 0x00
    void (*function)(const cs_shot_option&); // 0x04
};

// 0x00706AE0 (:111)
// LTCG: option_type_name in EDI.
static e_cs_option_type option_type_from_string(const char* const option_type_name)
{
    if (option_type_name == nullptr)
    {
        return CSOPT_NONE;
    }
    if (v_stricmp(option_type_name, "OptBool") == 0)
    {
        return CSOPT_BOOL;
    }
    if (v_stricmp(option_type_name, "OptFloat") == 0)
    {
        return CSOPT_FLOAT;
    }
    if (v_stricmp(option_type_name, "OptString") == 0)
    {
        return CSOPT_STRING;
    }
    if (v_stricmp(option_type_name, "OptColor") == 0)
    {
        return CSOPT_COLOR;
    }
    if (v_stricmp(option_type_name, "Opt3State") == 0)
    {
        return CSOPT_3STATE;
    }
    // Deviation: a stack buffer where rfg.exe prints to its thread-local one.
    char message[256];
    _snprintf_safe(message, sizeof(message), "cutscene: %s is not a valid option type.\n", option_type_name);
    keen::debug::nativePrintArgs(message);
    return CSOPT_NONE;
}

// 0x00706B90 (:143)
static void cs_option_spawn_peds(const cs_shot_option&)
{
}

// 0x00706BA0 (:161)
static void cs_option_spawn_cars(const cs_shot_option&)
{
}

// 0x00706BB0 (:178)
static void cs_option_level_lights(const cs_shot_option&)
{
}

// 0x00706BC0 (:191)
static void cs_option_tree_shadows(const cs_shot_option&)
{
}

// 0x00706BD0 (:207)
static void cs_option_character_shadows(const cs_shot_option&)
{
}

// 0x00706BE0 (:218)
static void cs_option_camera_movement(const cs_shot_option&)
{
}

// 0x00706BF0 (:225)
static void cs_option_disable_trees(const cs_shot_option&)
{
}

// 0x00706C00 (:237)
static void cs_option_pvs_enable(const cs_shot_option&)
{
}

// 0x00706C10 (:250)
static void cs_option_no_render_al(const cs_shot_option&)
{
}

// 0x00706C20 (:259)
static void cs_option_shader_lod_highest(const cs_shot_option&)
{
}

// 0x00706C30 (:266)
static void cs_option_shader_lod_restore(const cs_shot_option&)
{
}

// 0x00706C40 (:278)
static void cs_option_remove_spawns(const cs_shot_option&)
{
}

// 0x00706C50 (:295)
static void cs_option_far_clip(const cs_shot_option&)
{
}

// 0x00706C60 (:309)
static void cs_option_shot_fade_in(const cs_shot_option&)
{
}

// 0x00706C70 (:328)
static void cs_option_shot_fade_out(const cs_shot_option&)
{
}

// 0x00706C80 (:342)
static void cs_option_letterbox_fade_in(const cs_shot_option&)
{
}

// 0x00706C90 (:356)
static void cs_option_letterbox_fade_out(const cs_shot_option&)
{
}

// 0x00706CA0 (:370)
static void cs_option_framebuffer_tint(const cs_shot_option&)
{
}

// 0x00706CB0 (:401)
static void cs_option_force_tod_lights(const cs_shot_option&)
{
}

// 0x00706CC0 (:421)
static void cs_option_vint_shot_function(const cs_shot_option&)
{
}

// 0x00706CD0 (:440)
static void cs_option_hide_player_component(const cs_shot_option&)
{
}

// 0x00706CE0 (:458)
static void cs_option_bink_play_filename(const cs_shot_option& opt)
{
    if (opt.szParam1 != nullptr)
    {
        if (Current_cutscene->bink_movie_handle != 0)
        {
            video_player_stop(Current_cutscene->bink_movie_handle);
        }
        // rfg.exe inlines these two lines of cutscene_start_bink_movie.
        cutscene_start_bink_movie(opt.szParam1);
    }
}

// 0x00706D30 (:473)
static void cs_option_bink_material(const cs_shot_option& opt)
{
    if (opt.szParam1 != nullptr)
    {
        cutscene_set_bink_material(opt.szParam1);
    }
}

// 0x012D64E0. The names are the ones the tables use, typos and colons included.
static const cs_option_function_table_entry Option_function_table[23] = {
    {"TOD character shadows", cs_option_character_shadows},
    {"level lights", cs_option_level_lights},
    {"Tree Shadows", cs_option_tree_shadows},
    {"spawning cars", cs_option_spawn_cars},
    {"spawning peds", cs_option_spawn_peds},
    {"Camera moves with target", cs_option_camera_movement},
    {"Shader LOD Force Highest", cs_option_shader_lod_highest},
    {"Shader LOD Restore", cs_option_shader_lod_restore},
    {"PVS Enable", cs_option_pvs_enable},
    {"City No Render AL", cs_option_no_render_al},
    {"Spawning Remove All", cs_option_remove_spawns},
    {"Disable Trees", cs_option_disable_trees},
    {"Far Clip", cs_option_far_clip},
    {"Shot Fade In:", cs_option_shot_fade_in},
    {"Shot Fade Out:", cs_option_shot_fade_out},
    {"Letterbox Fade In", cs_option_letterbox_fade_in},
    {"Letterbox Fade Out", cs_option_letterbox_fade_out},
    {"Framebuffer Tint", cs_option_framebuffer_tint},
    {"Force TOD Lights", cs_option_force_tod_lights},
    {"Vint Shot Function", cs_option_vint_shot_function},
    {"Hide Player Compnt.", cs_option_hide_player_component},
    {"Bink Play Filename", cs_option_bink_play_filename},
    {"Bink Play Material", cs_option_bink_material},
};

// 0x00706D50 (:526)
bool cs_shot_option::operator==(const char* const _param) const
{
    return _param != nullptr && v_stricmp(option_name, _param) == 0;
}

// 0x00706D80
cs_shot_option::cs_shot_option()
{
}

// 0x0070D790 (:543)
void cutscene_parse_shot_options(xml_element* const shot_xml, cutscene_shot& shot)
{
    xml_element* const options_xml = xml_table_find_opt(shot_xml, "Options");
    cutscene_initialize_array_from_xml(shot.m_options, options_xml, "Option", Cutscene_mempool);
    // Original quirk kept: the one option on the stack is never cleared, so a field its type does not read keeps what the
    // option before left there, or stack garbage. Only the handlers read these, and the two real ones check szParam1.
    cs_shot_option option;
    for (xml_element* option_xml = xml_table_find_opt(options_xml, "Option"); option_xml != nullptr;
         option_xml = xml_table_find_next(options_xml, option_xml, "Option"))
    {
        option.option_name = str_new_copy(xml_table_get_req_string_ref(option_xml, "Name"), Cutscene_mempool);
        const char* const type_name = xml_table_get_req_string_ref(option_xml, "Type");
        option.option_type = option_type_from_string(type_name);
        switch (option.option_type)
        {
        case CSOPT_NONE:
            break;
        case CSOPT_BOOL:
            xml_table_get_req_bool(option.bParam1, option_xml, "Enabled");
            break;
        case CSOPT_FLOAT:
            xml_table_get_req_float(option.flParam1, option_xml, "Float");
            break;
        case CSOPT_STRING:
        {
            const char* const string_value = xml_table_get_opt_string_ref(option_xml, "String");
            option.szParam1 = string_value != nullptr ? str_new_copy(string_value, Cutscene_mempool) : nullptr;
            break;
        }
        case CSOPT_COLOR:
        {
            unsigned int red;
            unsigned int green;
            unsigned int blue;
            xml_table_get_req_uint(red, option_xml, "ColorRed");
            xml_table_get_req_uint(green, option_xml, "ColorGreen");
            xml_table_get_req_uint(blue, option_xml, "ColorBlue");
            option.Color = color((unsigned char)red, (unsigned char)green, (unsigned char)blue, 0);
            xml_table_get_req_float(option.flParam1, option_xml, "ColorInt");
            xml_table_get_req_float(option.flParam2, option_xml, "ColorSat");
            break;
        }
        case CSOPT_3STATE:
            xml_table_get_req_int(option.nParam1, option_xml, "3State");
            break;
        default:
        {
            // Deviation: a stack buffer where rfg.exe prints to its thread-local one.
            char message[256];
            _snprintf_safe(message, sizeof(message), "cutscene: %s is not a valid option type.\n", type_name);
            keen::debug::nativePrintArgs(message);
            break;
        }
        }
        shot.m_options.add(option);
    }
}

// 0x00709EC0 (:625)
void cutscene_set_options_for_shot(const cutscene_shot& shot)
{
    for (int i = 0; i < shot.m_options.size(); i++)
    {
        const cs_shot_option& option = shot.m_options[i];
        if (option.option_type == CSOPT_NONE)
        {
            continue;
        }
        for (const cs_option_function_table_entry& entry : Option_function_table)
        {
            if (entry.function_name != nullptr && v_stricmp(option.option_name, entry.function_name) == 0)
            {
                entry.function(option);
                break;
            }
        }
    }
}
