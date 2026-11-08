// Original: volition/rfg/code/interface/ui_document_hud_sniper.cpp
//
// The sniper scope module document (stub_impl.md S7): ammunition counts and the target under the crosshair.

#include "volition/rfg/code/interface/ui_document_hud_sniper.h"

#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/vector3.h"

// ---- file-static globals
static const char* cDocname_uisni = {"rfg_ui_sniper"}; // 0x01664A98
static int Doc_handle_uisni = -1; // 0x01664A9C
static int Data_item_handle_uisni = -1; // 0x01664AA0
static bool Visible_uisni; // 0x02C08069
static bool Assets_loaded_uisni; // 0x02C0806A
static bool Active_uisni; // 0x02C0806B
static bool Data_items_created_uisni; // 0x02C0806C

// 0x008D9E70
void hud_ui_sniper_create_data_items()
{
    vint_data_item* item = vint_data_item::create_new("SniperUpdate");
    if (item != nullptr)
    {
        const vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
        item->set_template(types, 3);
        Data_item_handle_uisni = item->get_handle();
        ui_add_data_item(Doc_handle_uisni, Data_item_handle_uisni);
    }
}

// 0x008C6670
void hud_ui_sniper_process_info()
{
    vint_data_item* item = vint_data_item::find(Data_item_handle_uisni);
    if (item == nullptr)
    {
        return;
    }
    int in_magazine = 0;
    int in_reserve = 0;
    int target = 0;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr)
        {
            in_magazine = wp->get_rounds_in_magazine();
            // Magic number: -1 is shown as unlimited.
            in_reserve = wp->has_unlimited_ammo() ? -1 : wp->get_rounds_in_reserve();
            if (vec_dist(pp->aim_pos, pp->pos) <= wp->w_info->red_range)
            {
                target = hud_ui_weapons_determine_object_targeted(pp);
            }
        }
    }
    item->set_element(0, vint_variant(in_magazine));
    item->set_element(1, vint_variant(in_reserve));
    item->set_element(2, vint_variant(target));
}

// 0x00910C40
void hud_ui_sniper_load_items(bool assets_only)
{
    if (!Active_uisni)
    {
        if (assets_only)
        {
            ui_reload_doc_assets(Doc_handle_uisni);
        }
        else
        {
            Doc_handle_uisni = ui_load_document(cDocname_uisni, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        }
        Assets_loaded_uisni = false;
        Active_uisni = true;
    }
}

// 0x008F5880
void hud_ui_sniper_unload_items(bool assets_only)
{
    if (assets_only)
    {
        if (Active_uisni)
        {
            ui_unload_doc_assets(Doc_handle_uisni);
            Active_uisni = false;
            Assets_loaded_uisni = false;
        }
        return;
    }
    ui_unload_document(Doc_handle_uisni, DFOT_NONE, 0.5f);
    Data_items_created_uisni = false;
    Active_uisni = false;
    Assets_loaded_uisni = false;
    Doc_handle_uisni = -1;
}

// 0x00910CB0
void hud_ui_sniper_enter()
{
    Assets_loaded_uisni = false;
    Data_items_created_uisni = false;
    Active_uisni = false;
    Visible_uisni = false;
    Doc_handle_uisni = ui_load_document(cDocname_uisni, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Assets_loaded_uisni = false;
    Active_uisni = true;
}

// 0x008F5900
void hud_ui_sniper_exit()
{
    if (Visible_uisni)
    {
        if (!Multiplayer)
        {
            hud_hide(false);
        }
        Visible_uisni = false;
    }
    ui_unload_document(Doc_handle_uisni, DFOT_NONE, 0.5f);
    Data_items_created_uisni = false;
    Active_uisni = false;
    Assets_loaded_uisni = false;
    Doc_handle_uisni = -1;
}

// 0x008F5960
void hud_ui_sniper_process()
{
    if (!Assets_loaded_uisni)
    {
        if (!Active_uisni)
        {
            return;
        }
        switch (ui_get_doc_asset_state(Doc_handle_uisni))
        {
            case ST_UNLOADED:
            case ST_ERROR:
                ui_unload_document(Doc_handle_uisni, DFOT_NONE, 0.5f);
                Doc_handle_uisni = -1;
                Active_uisni = false;
                break;
            case ST_LOADING:
                Assets_loaded_uisni = false;
                return;
            case ST_LOADED:
                Assets_loaded_uisni = true;
                break;
            default:
                break;
        }
        if (!Assets_loaded_uisni)
        {
            return;
        }
        if (!Data_items_created_uisni)
        {
            Data_items_created_uisni = true;
            hud_ui_sniper_create_data_items();
        }
    }
    if (Visible_uisni)
    {
        if (Data_items_created_uisni)
        {
            hud_ui_sniper_process_info();
        }
        ui_add_for_processing(Doc_handle_uisni);
    }
}

// 0x008D9EE0
void hud_ui_sniper_render()
{
    if (Assets_loaded_uisni && Visible_uisni)
    {
        ui_add_for_rendering(Doc_handle_uisni);
    }
}

// 0x008988F0
void hud_ui_sniper_set_visible(bool visible)
{
    if (Visible_uisni != visible)
    {
        if (!Multiplayer)
        {
            hud_hide(visible);
        }
        Visible_uisni = visible;
    }
}

// 0x00898930
bool hud_ui_sniper_is_visible()
{
    return Visible_uisni;
}
