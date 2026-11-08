// Original: volition/rfg/code/interface/ui_document_hud_rail_driver.cpp
//
// The rail driver module document (header comment in ui_document_hud_rail_driver.h): ammo, refire and target of the zoomed
// view, and the two boxes the x-ray effect draws in.

#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"

#include "volition/interface_lib/code/objects/vint_element_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector2.h"

#include "keen/base/trace.hpp"

// ---- globals
bool hud_ui_rail_driver_render_enabled = true; // 0x01664A48

// ---- file-static globals
static const char* cDocname_uirail = "rfg_ui_raildriver"; // 0x01664A38
static int Doc_handle_uirail = -1; // 0x01664A3C
static int Data_item_handle = -1; // 0x01664A40
static int Num_verts = 8; // 0x01664A44
static vector2 Verts[8]; // 0x01667120
static bool Visible; // 0x02C0804A
static bool Assets_loaded_uirail; // 0x02C0804B
static bool Active_uirail; // 0x02C08064
static bool Data_items_created_uirail; // 0x02C08065

// a * b + c with rfg.exe's double chain.
static float rail_scale_offset(float a, float b, float c)
{
    return (float)((double)c + (double)(float)((double)a * (double)b));
}

// 0x008D8DE0 (:79)
void hud_ui_rail_driver_calc_verts()
{
    if (Doc_handle_uirail != -1 && Assets_loaded_uirail && Active_uirail && Visible)
    {
        vint_document* doc = ui_get_vint_document(Doc_handle_uirail);
        vint_element_base* box1 = (vint_element_base*)vint_object_base::find_object("dummybox1", doc);
        doc = ui_get_vint_document(Doc_handle_uirail);
        vint_element_base* box2 = (vint_element_base*)vint_object_base::find_object("dummybox2", doc);
        if (box1 != nullptr && box2 != nullptr)
        {
            vector2 anchor1;
            vector2 anchor2;
            vint_variant size1;
            vint_variant size2;
            box1->get_global_anchor(anchor1);
            box1->get_screen_size(size1, 0);
            box2->get_global_anchor(anchor2);
            box2->get_screen_size(size2, 0);
            const vector2 extent1 = (vector2)size1;
            const vector2 extent2 = (vector2)size2;
            const vector2 scale = vint_project_get_scale(VINT_RES_SCALE_MAINTAIN_RATIO, VINT_RES_SCALE_MAINTAIN_RATIO);
            const vector2 offset = vint_project_get_offset(VINT_RES_OFFSET_CENTER, VINT_RES_OFFSET_CENTER);
            Verts[2].x = rail_scale_offset(anchor1.x, scale.x, offset.x);
            Verts[0].x = rail_scale_offset(anchor2.x, scale.x, offset.x);
            Verts[2].y = rail_scale_offset(anchor1.y, scale.y, offset.y);
            Verts[0].y = rail_scale_offset(anchor2.y, scale.y, offset.y);
            Verts[1].y = rail_scale_offset(extent2.y, scale.y, Verts[0].y);
            Verts[3].y = rail_scale_offset(extent1.y, scale.y, Verts[2].y);
            Verts[4].x = rail_scale_offset(extent1.x, scale.x, Verts[2].x);
            Verts[6].x = rail_scale_offset(extent2.x, scale.x, Verts[0].x);
            Num_verts = 8;
            Verts[1].x = Verts[0].x;
            Verts[3].x = Verts[2].x;
            Verts[4].y = Verts[2].y;
            Verts[5].x = Verts[4].x;
            Verts[5].y = Verts[3].y;
            Verts[6].y = Verts[0].y;
            Verts[7].x = Verts[6].x;
            Verts[7].y = Verts[1].y;
            return;
        }
    }
    const float width = (float)rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
    const float height = (float)rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
    Verts[0].x = 0.0f;
    Verts[0].y = 0.0f;
    Verts[1].x = 0.0f;
    Verts[1].y = height;
    Verts[2].x = width;
    Verts[2].y = 0.0f;
    Verts[3].x = width;
    Verts[3].y = height;
    Num_verts = 4;
}

// 0x008D9200 (:151)
void hud_ui_rail_driver_create_data_items()
{
    vint_data_item* item = vint_data_item::create_new("RailDriverUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[4] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_INT};
        item->set_template(types, 4);
        Data_item_handle = item->get_handle();
        ui_add_data_item(Doc_handle_uirail, Data_item_handle);
    }
}

// 0x008C6450 (:174)
void hud_ui_rail_driver_process_info()
{
    vint_data_item* item = vint_data_item::find(Data_item_handle);
    if (item == nullptr)
    {
        return;
    }
    int rounds = 0;
    int reserve = 0;
    float refire = 1.0f;
    int targeted = 0;
    player* pp = player_get_local_player();
    weapon* wp = pp != nullptr ? pp->get_equipped_weapon() : nullptr;
    if (wp != nullptr)
    {
        rounds = wp->get_rounds_in_magazine();
        reserve = wp->has_unlimited_ammo() ? -1 : wp->get_rounds_in_reserve(); // Magic number: -1, no count to show.
        const weapon_info* info = wp->w_info;
        if (info != nullptr)
        {
            const int since_fired = wp->time_since_fired();
            if (since_fired < 0)
            {
                refire = 1.0f;
            }
            else
            {
                // Magic number: the refire delay is in seconds, the time in milliseconds.
                refire = (float)since_fired / (info->default_refire_delay * 1000.0f);
                if (refire - 0.0f < 0.0f)
                {
                    refire = 0.0f;
                }
                if (refire - 1.0f >= 0.0f)
                {
                    refire = 1.0f;
                }
            }
            if (vec_dist(pp->aim_pos, pp->pos) <= info->red_range)
            {
                targeted = hud_ui_weapons_determine_object_targeted(pp);
            }
        }
    }
    item->set_element(0, vint_variant(rounds));
    item->set_element(1, vint_variant(reserve));
    item->set_element(2, vint_variant(refire));
    item->set_element(3, vint_variant(targeted));
}

// 0x00910B00 (:221)
void hud_ui_rail_driver_load_items(bool assets_only)
{
    if (Active_uirail)
    {
        return;
    }
    if (assets_only)
    {
        ui_reload_doc_assets(Doc_handle_uirail);
    }
    else
    {
        Doc_handle_uirail = ui_load_document(cDocname_uirail, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    }
    Assets_loaded_uirail = false;
    Active_uirail = true;
}

// 0x008F5430 (:238)
void hud_ui_rail_driver_unload_items(bool assets_only)
{
    if (Active_uirail && assets_only)
    {
        ui_unload_doc_assets(Doc_handle_uirail);
    }
    else
    {
        if (assets_only)
        {
            return;
        }
        // Original bug, kept: a full unload does not test Active_uirail, so a second one unloads handle -1.
        ui_unload_document(Doc_handle_uirail, DFOT_NONE, 0.5f);
        Doc_handle_uirail = -1;
        Data_items_created_uirail = false;
    }
    if (Visible)
    {
        xray_effect_stop(XET_RAIL_DRIVER);
    }
    Active_uirail = false;
    Assets_loaded_uirail = false;
}

// 0x00910B70 (:262)
void hud_ui_rail_driver_enter()
{
    Assets_loaded_uirail = false;
    Data_items_created_uirail = false;
    Active_uirail = false;
    Visible = false;
    hud_ui_rail_driver_load_items(false);
}

// 0x008F54B0 (:272)
void hud_ui_rail_driver_exit()
{
    hud_ui_rail_driver_set_visible(false);
    hud_ui_rail_driver_unload_items(false);
}

// 0x008F5540 (:279)
void hud_ui_rail_driver_process()
{
    if (!Assets_loaded_uirail)
    {
        if (!Active_uirail)
        {
            return;
        }
        switch (ui_get_doc_asset_state(Doc_handle_uirail))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            ui_unload_document(Doc_handle_uirail, DFOT_NONE, 0.5f);
            Doc_handle_uirail = -1;
            Active_uirail = false;
            break;
        case ST_LOADING:
            Assets_loaded_uirail = false;
            return;
        case ST_LOADED:
            Assets_loaded_uirail = true;
            if (Visible)
            {
                xray_effect_start(XET_RAIL_DRIVER);
            }
            else
            {
                xray_effect_stop(XET_RAIL_DRIVER);
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_uirail)
        {
            return;
        }
        if (!Data_items_created_uirail)
        {
            Data_items_created_uirail = true;
            hud_ui_rail_driver_create_data_items();
        }
        // Original bug, kept: the effect starts a second time in the frame the assets arrive.
        if (Visible)
        {
            xray_effect_start(XET_RAIL_DRIVER);
        }
    }
    if (Assets_loaded_uirail && Visible)
    {
        if (Data_items_created_uirail)
        {
            hud_ui_rail_driver_process_info();
        }
        ui_add_for_processing(Doc_handle_uirail);
    }
}

// 0x008D9280 (:339)
void hud_ui_rail_driver_render()
{
    if (hud_ui_rail_driver_render_enabled && Assets_loaded_uirail && Visible)
    {
        ui_add_for_rendering(Doc_handle_uirail);
    }
}

// 0x008D92B0 (:346)
vector2* hud_ui_rail_driver_get_xray_verts(int& num_verts)
{
    hud_ui_rail_driver_calc_verts();
    num_verts = Num_verts;
    return Verts;
}

// 0x00898740 (:354)
void hud_ui_rail_driver_set_visible(bool visible)
{
    if (Visible == visible)
    {
        return;
    }
    if (visible)
    {
        if (!Multiplayer)
        {
            hud_hide(true);
        }
        if (Assets_loaded_uirail)
        {
            xray_effect_start(XET_RAIL_DRIVER);
        }
    }
    else
    {
        if (!Multiplayer)
        {
            hud_hide(false);
        }
        if (Assets_loaded_uirail)
        {
            xray_effect_stop(XET_RAIL_DRIVER);
        }
    }
    Visible = visible;
}

// 0x008987B0 (:383)
bool hud_ui_rail_driver_is_visible()
{
    return Visible;
}
