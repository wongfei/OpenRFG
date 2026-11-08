// Original: volition/rfg/code/interface/vint/rfg_lua_actions.cpp
//
// UI.md UI3: whole. The game's natives for the documents' Lua scripts, registered as globals by
// rfg_lua_register_functions (vint_lua_init). Arguments are read with macros_v2.h's cursor helpers from the first of
// the last N stack values. The single player natives (mission briefing and complete, weapon cabinet, district
// liberation, upgrades, credits) call into code that is a stand-in outside its tier.

#include "volition/rfg/code/interface/vint/rfg_lua_actions.h"

#include "keen/base/stringencoding.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/animation/vint_object_tween.h"
#include "volition/interface_lib/code/objects/noninteractive/vint_element_bitmap.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/resources/vint_resource_bitmap.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/libmodules/lua/lua_1_21/lua.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/lua_lib/code/lua_thread.h"
#include "volition/lua_lib/code/macros_v2.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/hud/hud_ui_mission_complete.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/interface/credits.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/vfont.h"

#include <stdlib.h>

// ---- file-static globals
bool Display_character = true; // 0x0166316F (file static in the binary; external since UI4, main_menu.cpp)
// A static of the interface unity build in rfg.exe (ui_init_variables and ui_render update it; main_menu.cpp reads
// it): external here, declared where ui_document_manager.cpp uses it.
vector2i Screen_size(1280, 720); // 0x0166716C

// The position of the first of the last num_args stack values (the natives' argument cursor).
static int first_arg_pos(lua_State* lua_state, int num_args)
{
    const int top = lua_gettop(lua_state);
    return top > num_args ? num_args : top;
}

// A tween's callback variants (protected members of vint_object_tween), as the inlined clears reach them.
// Magic numbers: m_start_callback at 0x170, m_end_callback at 0x190.
static vint_variant& tween_callback(vint_object_base* tween, unsigned int offset)
{
    return *(vint_variant*)((unsigned char*)tween + offset);
}

// 0x008A1D30 (:106)
// The document of the running script thread.
static vint_document* get_rfg_doc_from_context()
{
    lua_thread* thread = lua_get_current_thread();
    if (thread == nullptr)
    {
        return nullptr;
    }
    return vint_document::find(thread->context);
}

// 0x008CCF00 (:127)
static int menu_tween_on_select_end(lua_State* lua_state)
{
    lua_gettop(lua_state);
    unsigned int vint_doc_handle = 0;
    vint_document* doc = get_rfg_doc_from_context();
    if (doc != nullptr)
    {
        vint_doc_handle = doc->get_handle();
    }
    ui_menu_tween_on_select_end(vint_doc_handle);
    return lua_add_return<int>(0, lua_state);
}

// 0x008CCF50 (:143)
static int menu_tween_fade_in_end(lua_State* lua_state)
{
    lua_gettop(lua_state);
    unsigned int vint_doc_handle = 0;
    vint_document* doc = get_rfg_doc_from_context();
    if (doc != nullptr)
    {
        vint_doc_handle = doc->get_handle();
    }
    ui_menu_tween_fade_in_end(vint_doc_handle);
    return lua_add_return<int>(0, lua_state);
}

// 0x008B9BD0 (:156)
// remove_tween_start_callback(tween handle): clears the tween's start callback.
static int remove_tween_start_callback(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const unsigned int handle = lua_get_arg<unsigned int>(lua_state, pos, true, 0);
    vint_document* doc = get_rfg_doc_from_context();
    if (doc != nullptr)
    {
        vint_object_base* obj = vint_object_base::find_object(handle, doc);
        if (obj != nullptr && obj->get_type() == vint_object_tween::Object_type)
        {
            tween_callback(obj, 0x170).zero();
        }
    }
    return lua_add_return<int>(0, lua_state);
}

// 0x008B9C70 (:173)
// remove_tween_end_callback(tween handle): clears the tween's end callback.
static int remove_tween_end_callback(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const unsigned int handle = lua_get_arg<unsigned int>(lua_state, pos, true, 0);
    vint_document* doc = get_rfg_doc_from_context();
    if (doc != nullptr)
    {
        vint_object_base* obj = vint_object_base::find_object(handle, doc);
        if (obj != nullptr && obj->get_type() == vint_object_tween::Object_type)
        {
            tween_callback(obj, 0x190).zero();
        }
    }
    return lua_add_return<int>(0, lua_state);
}

// 0x008B9D10 (:191)
// rfg_rand(min, max): the game's RNG in [min, max], 0 when the range is empty.
static int rfg_rand(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const int minimum = lua_get_arg<int>(lua_state, pos, true, 0);
    const int maximum = lua_get_arg<int>(lua_state, pos, true, 0);
    int value = 0;
    if (maximum != minimum && maximum - minimum >= 0)
    {
        value = int_rand_range(minimum, maximum);
    }
    return lua_add_return<int>(value, lua_state);
}

// 0x008B9D90 (:207)
static int rfg_get_screen_dim(lua_State* lua_state)
{
    lua_gettop(lua_state);
    lua_add_return<int>(Screen_size.x, lua_state);
    lua_add_return<int>(Screen_size.y, lua_state);
    return 2;
}

// 0x008B9DF0 (:219)
// The current resolution mode's size.
static int rfg_get_ui_doc_res(lua_State* lua_state)
{
    lua_gettop(lua_state);
    int hres;
    int vres;
    vint_project_get_resolution_mode(hres, vres, vint_project_get_resolution_mode());
    lua_add_return<int>(hres, lua_state);
    lua_add_return<int>(vres, lua_state);
    return 2;
}

// 0x008B9E50 (:230)
// rfg_play_audio(cue name): plays the 2D cue when it exists. Original: returns no value when it plays, 0 when the
// name is missing.
static int rfg_play_audio(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const char* cue_name = lua_get_arg<const char*>(lua_state, pos, true, nullptr);
    if (cue_name == nullptr)
    {
        return lua_add_return<int>(0, lua_state);
    }
    int cue_id;
    audiolib_cue_get_id(cue_name, &cue_id);
    audiolib_cue_info info;
    if (audiolib_cue_get_info(cue_id, &info) == AUDIOLIB_ERR_NONE)
    {
        audiolib_cue_play(cue_id, nullptr, nullptr);
    }
    return 0;
}

// 0x008B9F10 (:253)
// rfg_play_video_mb(play): pauses the mission briefing's video when false.
static int rfg_play_video_mb(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const bool play = lua_get_arg<bool>(lua_state, pos, true, false);
    hud_ui_mission_briefing_pause(!play, true);
    return 0;
}

// 0x008B9F50 (:264)
static int rfg_set_video_volume(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const int video_handle = lua_get_arg<int>(lua_state, pos, true, 0);
    const float volume = lua_get_arg<float>(lua_state, pos, true, 0.0f);
    video_player_set_volume(video_handle, volume);
    return 0;
}

// 0x008B9FB0 (:276)
static int rfg_mission_briefing_done(lua_State* lua_state)
{
    lua_gettop(lua_state);
    hud_ui_mission_briefing_done();
    return 0;
}

// 0x008B9FD0 (:284)
// The script's page is 1-based.
static int rfg_start_mission_complete(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const int page = lua_get_arg<int>(lua_state, pos, true, 0);
    hud_ui_mission_complete_start_tally(page - 1);
    return 0;
}

// 0x008BA010 (:293)
static int rfg_start_activity_complete(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const int page = lua_get_arg<int>(lua_state, pos, true, 0);
    hud_ui_activity_complete_start_tally(page - 1);
    return 0;
}

// 0x008BA050 (:312)
// rfg_get_backpack_icon_name(name or type): the backpack's icon, 0 when there is none.
static int rfg_get_backpack_icon_name(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    if (pos > 0)
    {
        const multi_backpack_info* info = nullptr;
        const int type = lua_type(lua_state, -pos);
        if (type == LUA_TSTRING)
        {
            info = multi_game_backpack_get_info(lua_tostring(lua_state, -pos));
        }
        else if (type == LUA_TNUMBER)
        {
            info = multi_game_backpack_get_info(lua_get_arg<int>(lua_state, pos, true, 0));
        }
        else
        {
            return lua_add_return<int>(0, lua_state);
        }
        if (info != nullptr && info->icon_name != nullptr)
        {
            return lua_add_return<const char*>(info->icon_name, lua_state);
        }
    }
    return lua_add_return<int>(0, lua_state);
}

// 0x008CCFA0 (:338)
// rfg_get_string_width(text, font): the UTF-8 text's width in the font.
static int rfg_get_string_width(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 2);
    const char* text = lua_get_arg<const char*>(lua_state, pos, true, nullptr);
    const int fontnum = lua_get_arg<int>(lua_state, pos, true, 0);
    int width = 0;
    if (text != nullptr)
    {
        wchar_t wide[512];
        // Magic number: 0x400, the buffer's bytes.
        keen::convertUTF8ToUTF16((uint16*)wide, 0x400, text);
        width = ui_get_usage_character_width(wide, fontnum);
    }
    return lua_add_return<int>(width, lua_state);
}

// 0x008BA0F0 (:353)
// rfg_get_font_height(font): the height of "Hello" in the font.
static int rfg_get_font_height(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    const int fontnum = lua_get_arg<int>(lua_state, pos, true, 0);
    wchar_t wide[512];
    keen::convertUTF8ToUTF16((uint16*)wide, 0x400, "Hello");
    return lua_add_return<int>(ui_get_usage_character_height(wide, fontnum), lua_state);
}

// 0x008BA190 (:365)
// rfg_tonumber(text): atof as a float.
static int rfg_tonumber(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    float value = 0.0f;
    const char* text = lua_get_arg<const char*>(lua_state, pos, true, nullptr);
    if (text != nullptr)
    {
        value = (float)atof(text);
    }
    return lua_add_return<float>(value, lua_state);
}

// 0x008BA200 (:377)
// The current gameplay mode's internal name in multiplayer, else 0.
static int rfg_get_multiplayer_mode_name(lua_State* lua_state)
{
    lua_gettop(lua_state);
    if (Multiplayer)
    {
        const char* name = multi_gameplay_mode_get_internal_name(multi_gameplay_mode_get());
        if (name != nullptr)
        {
            return lua_add_return<const char*>(name, lua_state);
        }
    }
    return lua_add_return<int>(0, lua_state);
}

// 0x008BA260 (:395)
// rfg_get_mp_team_color(team name or number): the team's icon color as three 0..1 values, white otherwise.
static int rfg_get_mp_team_color(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    if (pos > 0)
    {
        human_teams team = HUMAN_TEAM_NONE;
        const int type = lua_type(lua_state, -pos);
        bool valid = true;
        if (type == LUA_TSTRING)
        {
            team = human_team_from_name(lua_tostring(lua_state, -pos));
        }
        else if (type == LUA_TNUMBER)
        {
            team = lua_get_arg<human_teams>(lua_state, pos, true, HUMAN_TEAM_NONE);
        }
        else
        {
            valid = false;
        }
        if (valid && team != HUMAN_TEAM_NONE)
        {
            color clr = color::WHITE;
            multi_game_render_get_team_color(clr, MTCT_ICON, team);
            lua_add_return<float>((float)clr.red / 255.0f, lua_state);
            lua_add_return<float>((float)clr.green / 255.0f, lua_state);
            lua_add_return<float>((float)clr.blue / 255.0f, lua_state);
            return 3;
        }
    }
    lua_add_return<float>(1.0f, lua_state);
    lua_add_return<float>(1.0f, lua_state);
    lua_add_return<float>(1.0f, lua_state);
    return 3;
}

// 0x008BA3C0 (:431)
static int rfg_is_multiplayer(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<bool>(Multiplayer, lua_state);
}

// 0x008BA3F0 (:438)
static int rfg_wepon_cab_tween_end(lua_State* lua_state)
{
    lua_gettop(lua_state);
    hud_ui_weapon_cabinet_tween_end_cb();
    return 0;
}

// 0x008BA410 (:445)
static int rfg_dist_lib_tween_end(lua_State* lua_state)
{
    lua_gettop(lua_state);
    hud_ui_district_liberation_tween_done();
    return 0;
}

// 0x008BA430 (:452)
static int rfg_credits_tween_is_finished(lua_State* lua_state)
{
    lua_gettop(lua_state);
    credits_set_up_credits_page();
    return 0;
}

// 0x008BA450 (:457)
static int rfg_get_max_charges(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<int>(upgrade_get_max_charges(), lua_state);
}

// 0x008BA480 (:465)
static int rfg_hide_main_menu_character(lua_State* lua_state)
{
    lua_gettop(lua_state);
    Display_character = false;
    return 0;
}

// 0x008BA4A0 (:470)
static int rfg_enable_button_input(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    hud_enable_button_input(lua_get_arg<bool>(lua_state, pos, true, false));
    return 0;
}

// 0x008BA4E0 (:478)
static int rfg_morale_disabled(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<bool>(dlc_get_morale_disabled(), lua_state);
}

// 0x008BA510 (:485)
static int rfg_morale_disabled_load_game(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<bool>(dlc_get_morale_disabled_load_game(), lua_state);
}

// 0x008BA540 (:492)
static int rfg_salvage_disabled(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<bool>(dlc_get_salvage_disabled(), lua_state);
}

// 0x008BA570 (:499)
static int rfg_tooltips_disabled(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<bool>(dlc_get_tooltips_disabled(), lua_state);
}

// 0x008BA5A0 (:506)
static int rfg_get_language(lua_State* lua_state)
{
    lua_gettop(lua_state);
    return lua_add_return<int>((int)Current_language, lua_state);
}

// 0x008BA5D0 (:513)
// Whether the body font is right to left.
static int rfg_get_language_right_to_left(lua_State* lua_state)
{
    lua_gettop(lua_state);
    bool reversed = false;
    if (Body_font != -1)
    {
        font* f = font_get_font(Body_font);
        if (f != nullptr && f->is_reversed != 0)
        {
            reversed = true;
        }
    }
    return lua_add_return<bool>(reversed, lua_state);
}

// 0x008BA620 (:526)
// rfg_get_bitmap_dimensions(bitmap element handle): the image's size and scale (4 values), nothing otherwise.
static int rfg_get_bitmap_dimensions(lua_State* lua_state)
{
    int pos = first_arg_pos(lua_state, 1);
    unsigned int handle = 0;
    if (pos > 0 && lua_type(lua_state, -pos) == LUA_TNUMBER)
    {
        handle = lua_get_arg<unsigned int>(lua_state, pos, true, 0);
    }
    vint_object_base* obj = vint_object_base::find_object(handle, nullptr);
    if (obj == nullptr || obj->get_type() != vint_element_bitmap::Object_type)
    {
        return 0;
    }
    vint_variant image;
    static_cast<vint_element_bitmap*>(obj)->get_image(image, 0);
    vint_resource_bitmap* bitmap;
    if (image.get_type() == VINT_PROP_TYPE_BITMAP && (bitmap = (vint_resource_bitmap*)image) != nullptr)
    {
        vector2i size;
        vector2 scale;
        bitmap->get_dimensions(size, scale);
        lua_add_return<int>(size.x, lua_state);
        lua_add_return<int>(size.y, lua_state);
        lua_add_return<float>(scale.x, lua_state);
        lua_add_return<float>(scale.y, lua_state);
        return 4;
    }
    return 0;
}

// 0x008CD060 (:580)
// The 33 natives as globals.
void rfg_lua_register_functions(lua_State* lua_state)
{
    struct native
    {
        const char* name;
        lua_CFunction func;
    };
    const native natives[] = {
        {"menu_tween_on_select_end", menu_tween_on_select_end},
        {"menu_tween_fade_in_end", menu_tween_fade_in_end},
        {"remove_tween_start_callback", remove_tween_start_callback},
        {"remove_tween_end_callback", remove_tween_end_callback},
        {"rfg_rand", rfg_rand},
        {"rfg_get_screen_dim", rfg_get_screen_dim},
        {"rfg_get_ui_doc_res", rfg_get_ui_doc_res},
        {"rfg_play_audio", rfg_play_audio},
        {"rfg_play_video_mb", rfg_play_video_mb},
        {"rfg_set_video_volume", rfg_set_video_volume},
        {"rfg_mission_briefing_done", rfg_mission_briefing_done},
        {"rfg_start_mission_complete", rfg_start_mission_complete},
        {"rfg_start_activity_complete", rfg_start_activity_complete},
        {"rfg_get_backpack_icon_name", rfg_get_backpack_icon_name},
        {"rfg_get_string_width", rfg_get_string_width},
        {"rfg_get_font_height", rfg_get_font_height},
        {"rfg_tonumber", rfg_tonumber},
        {"rfg_get_multiplayer_mode_name", rfg_get_multiplayer_mode_name},
        {"rfg_get_mp_team_color", rfg_get_mp_team_color},
        {"rfg_is_multiplayer", rfg_is_multiplayer},
        {"rfg_wepon_cab_tween_end", rfg_wepon_cab_tween_end},
        {"rfg_dist_lib_tween_end", rfg_dist_lib_tween_end},
        {"rfg_credits_tween_is_finished", rfg_credits_tween_is_finished},
        {"rfg_get_max_charges", rfg_get_max_charges},
        {"rfg_hide_main_menu_character", rfg_hide_main_menu_character},
        {"rfg_enable_button_input", rfg_enable_button_input},
        {"rfg_morale_disabled", rfg_morale_disabled},
        {"rfg_morale_disabled_load_game", rfg_morale_disabled_load_game},
        {"rfg_salvage_disabled", rfg_salvage_disabled},
        {"rfg_tooltips_disabled", rfg_tooltips_disabled},
        {"rfg_get_language", rfg_get_language},
        {"rfg_get_bitmap_dimensions", rfg_get_bitmap_dimensions},
        {"rfg_get_language_right_to_left", rfg_get_language_right_to_left},
    };
    for (unsigned int i = 0; i < sizeof(natives) / sizeof(natives[0]); i++)
    {
        lua_pushstring(lua_state, natives[i].name);
        lua_pushcclosure(lua_state, natives[i].func, 0);
        lua_settable(lua_state, LUA_GLOBALSINDEX);
    }
}

// 0x008A1D50 (:629)
void rfg_lua_actions_cleanup()
{
}
