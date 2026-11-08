// Original: volition/rfg/code/interface/ui_document_hud_satellite.cpp
//
// The satellite view module document (stub_impl.md S7, single player): rfg_ui_hud_avm's reticule, ammunition, hints,
// header and target data from the satellite camera, with the view's tint and saturation while it is up.
//
// Original bugs kept (commented): a mission's description and caption are used as format strings, and the header sends
// an uninitialized buffer when the mission has no description; the hint item's show flag is set twice.

#include "volition/rfg/code/interface/ui_document_hud_satellite.h"

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "keen/base/string.hpp"

// ---- file-static globals
static const char* cDocname_uisat = {"rfg_ui_hud_avm"}; // 0x01664A4C
static int Doc_handle_uisat = -1; // 0x01664A50
// Original: the data item handles start at 16, not at an invalid handle.
static int AVMReticuleUpdate_handle = 16; // 0x01664A54
static int AVMAmmoUpdate_handle = 16; // 0x01664A58
static int AVMSetHints_handle = 16; // 0x01664A5C
static int AVMHeaderUpdate_handle = 16; // 0x01664A60
static int AVMTickUpdate_handle = 16; // 0x01664A64
static int AVMTargetUpdate_handle = 16; // 0x01664A68
static int AVMDisconnect_handle = 16; // 0x01664A6C
static const char* const UI_strings[6] = { // 0x01664A70
    "",
    "HM_SAT_ACQUIRING_TARGET",
    "HM_SAT_CONVOY_TAKING_FIRE",
    "HM_SAT_READY",
    "HM_SAT_RELOADING",
    "HM_SAT_CANNOT_FIRE",
};
static float Satellite_cam_saturation = 0.1f; // 0x01664A88
static float Satellite_cam_tint_r = 0.4f; // 0x01664A8C
static float Satellite_cam_tint_b = 0.4f; // 0x01664A90
static float Satellite_cam_tint_g = 0.4f; // 0x01664A94
static timestamp Fire_taken_ago; // 0x01667160
static timestamp Satellite_temp_message_timer; // 0x01667164
static bool Satellite_old_fog_enabled; // 0x02BE9E1F
static bool Satellite_old_hud_enabled; // 0x02BF2225
static char Satellite_temp_message_string[32]; // 0x02C00EDC
static bool Assets_loaded_uisat; // 0x02C08066
static bool Active_uisat; // 0x02C08067
static bool Data_items_created_uisat; // 0x02C08068

// Port helper: "[image:<the action's button image>]", as rfg.exe inlines it.
static void hud_ui_satellite_button_image(char* text, int text_size, control_button_action action)
{
    char image_name[32];
    controls_get_button_image_name(image_name, sizeof(image_name), action, Player_controller_index);
    keen::formatString(text, text_size, "[image:%s]", image_name);
}

// Port helper: the view's tint, as rfg.exe inlines it.
static void hud_ui_satellite_set_tint()
{
    rl_color_float tint;
    tint.red = Satellite_cam_tint_r;
    tint.green = Satellite_cam_tint_g;
    tint.blue = Satellite_cam_tint_b;
    tint.alpha = 1.0f;
    fb_color_correct_modulate_tint(tint);
}

// 0x008D92D0 (:99)
void hud_ui_satellite_create_data_items()
{
    vint_data_item* item = vint_data_item::create_new("AVMReticuleUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[10] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 10);
        AVMReticuleUpdate_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMReticuleUpdate_handle);
    }
    item = vint_data_item::create_new("AVMAmmoUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 2);
        AVMAmmoUpdate_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMAmmoUpdate_handle);
    }
    item = vint_data_item::create_new("AVMSetHints");
    if (item != nullptr)
    {
        const vint_variant_type types[9] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 9);
        AVMSetHints_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMSetHints_handle);
        char image[128];
        hud_ui_satellite_button_image(image, sizeof(image), CBA_SAT_RETURN);
        item->set_element(0, vint_variant((const char*)image));
        item->set_element(1, vint_variant("HM_SAT_BTN_TARGET"));
        int index = 2;
        if (mission_get_mode() == MISSION_MODE_ANTS_VS_MAG)
        {
            hud_ui_satellite_button_image(image, sizeof(image), CBA_SAT_HOLD);
            item->set_element(2, vint_variant((const char*)image));
            item->set_element(3, vint_variant("HM_SAT_BTN_HOLD"));
            index = 4;
        }
        hud_ui_satellite_button_image(image, sizeof(image), CBA_SAT_FIRE);
        item->set_element(index, vint_variant((const char*)image));
        item->set_element(index + 1, vint_variant("HM_SAT_BTN_FIRE"));
        item->set_element(index + 2, vint_variant((const char*)nullptr));
        item->set_element(index + 3, vint_variant(""));
        if (mission_get_mode() != MISSION_MODE_ANTS_VS_MAG)
        {
            item->set_element(index + 4, vint_variant((const char*)nullptr));
            item->set_element(index + 5, vint_variant(""));
        }
        item->set_element(index + 6, vint_variant(false));
    }
    item = vint_data_item::create_new("AVMHeaderUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 2);
        AVMHeaderUpdate_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMHeaderUpdate_handle);
    }
    item = vint_data_item::create_new("AVMRangeUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 2);
        AVMTickUpdate_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMTickUpdate_handle);
    }
    item = vint_data_item::create_new("AVMTargetUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 3);
        AVMTargetUpdate_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMTargetUpdate_handle);
    }
    item = vint_data_item::create_new("AVMDisconnect");
    if (item != nullptr)
    {
        const vint_variant_type types[1] = {VINT_PROP_TYPE_BOOL};
        item->set_template(types, 1);
        AVMDisconnect_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uisat, AVMDisconnect_handle);
    }
}

// Port helper: the document's 3D group offset from its logo in the current resolution (zero without them), as rfg.exe
// inlines it.
static vector2 hud_ui_satellite_3d_offset()
{
    vector2 result;
    result.zero();
    vint_document* document = nullptr;
    const vint_doc_info* info = ui_get_doc_info(Doc_handle_uisat);
    if (info != nullptr)
    {
        document = info->m_doc;
    }
    vint_object_base* logo = vint_object_base::find_object("boot_logo1", document);
    vint_object_base* group = vint_object_base::find_object("boot_3d_group", document);
    if (logo == nullptr || group == nullptr || !logo->is_type(vint_element_base::Object_type) || !group->is_type(vint_element_base::Object_type))
    {
        return result;
    }
    vint_res_offset logo_x = VINT_RES_OFFSET_CENTER;
    vint_res_offset logo_y = VINT_RES_OFFSET_CENTER;
    vint_res_scale scale_x = VINT_RES_SCALE_MAINTAIN_RATIO;
    vint_res_scale scale_y = VINT_RES_SCALE_MAINTAIN_RATIO;
    ((vint_element_base*)logo)->get_res_offsets_and_scales_inherited(logo_x, logo_y, scale_x, scale_y);
    vint_res_offset group_x = VINT_RES_OFFSET_CENTER;
    vint_res_offset group_y = VINT_RES_OFFSET_CENTER;
    ((vint_element_base*)group)->get_res_offsets_and_scales_inherited(group_x, group_y, scale_x, scale_y);
    const vector2 logo_offset = vint_project_get_offset(logo_x, logo_y);
    const vector2 group_offset = vint_project_get_offset(group_x, group_y);
    result.x = group_offset.x - logo_offset.x;
    result.y = group_offset.y - logo_offset.y;
    return result;
}

// 0x008D97B0 (:277)
void hud_ui_satellite_process_info()
{
    camera_satellite_hud_data hud;
    camera_satellite_get_hud_data(hud);
    if (hud.shutting_down)
    {
        vint_data_item* item = vint_data_item::find(AVMDisconnect_handle);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(true));
        }
        return;
    }
    vint_data_item* item = vint_data_item::find(AVMReticuleUpdate_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(hud.enemy_target_reticule));
        const char* text = UI_strings[0];
        bool flash = false;
        if (Satellite_temp_message_timer.valid() && !Satellite_temp_message_timer.elapsed())
        {
            text = Satellite_temp_message_string;
            flash = true;
        }
        else if (Fire_taken_ago.valid() && !Fire_taken_ago.elapsed())
        {
            text = UI_strings[2];
            flash = true;
        }
        else if (hud.acquiring_target)
        {
            text = UI_strings[1];
        }
        item->set_element(1, vint_variant(text));
        item->set_element(2, vint_variant(hud.can_fire));
        item->set_element(3, vint_variant(flash));
        item->set_element(4, vint_variant((const char*)nullptr));
        if (input_get_gyro_enabled(GYRO_MODE_SATELLITE) && !game_is_paused())
        {
            item->set_element(5, vint_variant(true));
            // Magic number: 25 pixels of reticule per unit of gyro.
            item->set_element(6, vint_variant((float)((double)camera_satellite_get_gyro_heading() * 25.0)));
            item->set_element(7, vint_variant((float)((double)-camera_satellite_get_gyro_pitch() * 25.0)));
        }
        else
        {
            item->set_element(5, vint_variant(false));
            item->set_element(6, vint_variant(0.0f));
            item->set_element(7, vint_variant(0.0f));
        }
        const vector2 offset = hud_ui_satellite_3d_offset(); // inlined
        item->set_element(8, vint_variant(offset.x));
        item->set_element(9, vint_variant(offset.y));
    }
    item = vint_data_item::find(AVMAmmoUpdate_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(""));
        int status = 3;
        if (!hud.can_fire)
        {
            status = 5;
        }
        else if (hud.reloading)
        {
            status = 4;
        }
        item->set_element(1, vint_variant(UI_strings[status]));
    }
    item = vint_data_item::find(AVMHeaderUpdate_handle);
    if (item != nullptr && mission_is_active())
    {
        char description[64]; // Original bug kept: not initialized, and sent as it is without a description.
        char caption[64];
        const char* mission_description = mission_get_description();
        if (mission_description != nullptr)
        {
            keen::formatString(description, sizeof(description), mission_description); // Original: as a format string.
        }
        // rfg.exe reads Activity_set.m_mission_caption (0x01667014, ui_document_hud_missions.cpp's) inline.
        keen::formatString(caption, sizeof(caption), hud_ui_missions_get_objective()); // Original: as a format string.
        item->set_element(0, vint_variant((const char*)description));
        item->set_element(1, vint_variant((const char*)caption));
    }
    item = vint_data_item::find(AVMSetHints_handle);
    if (item != nullptr && mission_is_active())
    {
        // Original: set true and then to its value.
        item->set_element(8, vint_variant(true));
        item->set_element(8, vint_variant(hud.show_hints));
    }
    item = vint_data_item::find(AVMTickUpdate_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(hud.ticker_pct.x));
        item->set_element(1, vint_variant(hud.ticker_pct.y));
    }
}

// 0x008987D0 (:412)
void hud_ui_satellite_process_target()
{
    bool show;
    vector2 fire_pos;
    if (!camera_satellite_get_hud_target(show, fire_pos))
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(AVMTargetUpdate_handle);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant(show));
        item->set_element(1, vint_variant(fire_pos.x));
        item->set_element(2, vint_variant(fire_pos.y));
        item->force_immediate_update();
    }
}

// 0x00916EE0 (:454)
void hud_ui_satellite_enter()
{
    Assets_loaded_uisat = false;
    Data_items_created_uisat = false;
    Active_uisat = false;
    // Magic number: half a second's fade.
    Doc_handle_uisat = ui_load_document(cDocname_uisat, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Assets_loaded_uisat = false;
    Active_uisat = true;
    hud_ui_satellite_set_tint(); // inlined
    fb_color_correct_modulate_saturation(Satellite_cam_saturation);
    Satellite_old_fog_enabled = game_render_get_fog_enabled();
    game_render_set_fog_enabled(false);
    Satellite_old_hud_enabled = hud_is_hidden();
    hud_hide(true);
    Satellite_temp_message_string[0] = '\0';
    Satellite_temp_message_timer.invalidate();
}

// 0x00904BB0 (:485)
void hud_ui_satellite_exit()
{
    ui_unload_document(Doc_handle_uisat, DFOT_NONE, 0.5f);
    Doc_handle_uisat = -1;
    Data_items_created_uisat = false;
    Active_uisat = false;
    Assets_loaded_uisat = false;
    rl_color_float tint;
    tint.red = 1.0f;
    tint.green = 1.0f;
    tint.blue = 1.0f;
    tint.alpha = 1.0f;
    fb_color_correct_modulate_tint(tint);
    fb_color_correct_modulate_saturation(1.0f);
    game_render_set_fog_enabled(Satellite_old_fog_enabled);
    hud_hide(Satellite_old_hud_enabled);
}

// 0x00910BD0 (:510)
void hud_ui_satellite_load_items(bool assets_only)
{
    if (Active_uisat)
    {
        return;
    }
    if (assets_only)
    {
        ui_reload_doc_assets(Doc_handle_uisat);
    }
    else
    {
        Doc_handle_uisat = ui_load_document(cDocname_uisat, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    }
    Assets_loaded_uisat = false;
    Active_uisat = true;
}

// 0x008F5680 (:528)
void hud_ui_satellite_unload_items(bool assets_only)
{
    if (Active_uisat && assets_only)
    {
        ui_unload_doc_assets(Doc_handle_uisat);
        Active_uisat = false;
        Assets_loaded_uisat = false;
        return;
    }
    if (!Active_uisat && assets_only)
    {
        return;
    }
    // Original: without assets_only the document goes whether it is active or not.
    ui_unload_document(Doc_handle_uisat, DFOT_NONE, 0.5f);
    Data_items_created_uisat = false;
    Active_uisat = false;
    Assets_loaded_uisat = false;
    Doc_handle_uisat = -1;
}

// 0x008F5700 (:549)
void hud_ui_satellite_process()
{
    if (!Assets_loaded_uisat)
    {
        switch (ui_get_doc_asset_state(Doc_handle_uisat))
        {
        case ST_UNLOADED:
            VLIB_CRITICAL_FAILURE();
            // fall through
        case ST_ERROR:
            ui_unload_document(Doc_handle_uisat, DFOT_NONE, 0.5f);
            Doc_handle_uisat = -1;
            break;
        case ST_LOADING:
            Assets_loaded_uisat = false;
            break;
        case ST_LOADED:
            Assets_loaded_uisat = true;
            break;
        default:
            VLIB_CRITICAL_FAILURE();
            break;
        }
        if (!Assets_loaded_uisat)
        {
            hud_ui_satellite_set_tint(); // inlined
            fb_color_correct_modulate_saturation(Satellite_cam_saturation);
            return;
        }
        if (!Data_items_created_uisat)
        {
            Data_items_created_uisat = true;
            hud_ui_satellite_create_data_items();
        }
    }
    hud_ui_satellite_process_info();
    ui_add_for_processing(Doc_handle_uisat);
    hud_ui_satellite_set_tint(); // inlined
    fb_color_correct_modulate_saturation(Satellite_cam_saturation);
}

// 0x008C6630 (:598)
void hud_ui_satellite_get_doc_handles(farray<int,16>& handles)
{
    handles.add(Doc_handle_uisat);
}

// 0x008D9E50 (:604)
void hud_ui_satellite_render()
{
    hud_ui_satellite_process_target();
    if (Assets_loaded_uisat)
    {
        ui_add_for_rendering(Doc_handle_uisat);
    }
}

// 0x008988A0 (:612)
void hud_ui_satellite_report_fire_taken(unsigned int veh_handle)
{
    if (veh_handle == camera_satellite_get_target())
    {
        // Magic number: the taking fire message stays 1.5 s.
        Fire_taken_ago.set(1500);
    }
}

// 0x008988C0 (:621)
void hud_ui_satellite_set_temp_message(const char* const msg, int display_ms)
{
    strncpy_safe(Satellite_temp_message_string, msg, 0x20);
    Satellite_temp_message_timer.set(display_ms);
}
