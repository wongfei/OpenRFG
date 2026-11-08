// Original: volition/rfg/code/hud/hud.cpp
//
// The non-vint HUD (stub_impl.md S7): the user interface table, the damage indicators, the frame's HUD render and the
// resolution independent bitmap helpers. Only the hit indicators, the respawn line and the two message/icon lists still
// draw anything in the shipped game; everything else was moved to the vint documents (ui_document_hud.cpp).

#include "volition/rfg/code/hud/hud.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_hudtimer.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/hud/hud_floating_text.h"
#include "volition/rfg/code/hud/hud_heartbeat_effect.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <stdio.h>

// ---- file-private types (rfg.pdb)
// size 0x10
struct hud_display_slot
{
    int x; // 0x00
    int y; // 0x04
    float scale; // 0x08
    float alpha; // 0x0C
};
static_assert(sizeof(hud_display_slot) == 0x10, "hud_display_slot");

// size 0x18
struct follower_slot
{

    timestamp visible_delay_timer; // 0x00
    timestamp bling_delay_timer; // 0x04
    hud_display_slot slot; // 0x08
};
static_assert(sizeof(follower_slot) == 0x18, "follower_slot");

// size 0x14
struct hit_indicator_data
{

    float alpha; // 0x00
    vector direction; // 0x04
    float scale; // 0x10
};
static_assert(sizeof(hit_indicator_data) == 0x14, "hit_indicator_data");

// size 0xC
struct number_bitmap
{
    const char* bitmap; // 0x00
    int id; // 0x04
    int width; // 0x08
};
static_assert(sizeof(number_bitmap) == 0xC, "number_bitmap");

// ---- globals
float X_res_multiplier; // 0x02B953E8
float Y_res_multiplier; // 0x02B95600
int Center_screen_x; // 0x02B953EC
int Center_screen_y; // 0x02B953F0

// ---- file-static globals
static bool Button_input_enabled = true; // 0x01660DC5
static bool Firearm_interface_rendering_enabled = true; // 0x01660DC6
static bool Pickup_message_rendering_enabled = true; // 0x01660DC7
static unsigned int Distance_target_handle = 0xFFFFFFFF; // 0x01660DF8
static float Hit_scale = 1.0f; // 0x01660DFC
static const char* Hud_bitmap[17] = {"hudk_hlthplus.tga", "hudk_hlthinf.tga", "hudk_invwcirc.tga", "invw-fist.tga", "hud-crosshair.tga", "hud-crosshairb.tga", "hudk_clipbarbak.tga", "hudk_clipbarcap.tga", "hudk_clipbarwht.tga", "sniper_hud.tga", "sniper_crshair.tga", "sniper_dotwhite.tga", "sniper_dotred.tga", "control_meter_base.tga", "control_meter_base2.tga", "control_meter_tic.tga", "hit_indicator.tga"}; // 0x01660E00
static float Hit_indicator_fade_speed = 120.0f; // 0x01660E44
// 0x01660E48: the file name and the digit's width in pixels (the id is set by hud_system_init).
static number_bitmap Health_numbers[13] = {{"hudk_hlth0.tga", -1, 14}, {"hudk_hlth1.tga", -1, 12}, {"hudk_hlth2.tga", -1, 14}, {"hudk_hlth3.tga", -1, 14}, {"hudk_hlth4.tga", -1, 14}, {"hudk_hlth5.tga", -1, 14}, {"hudk_hlth6.tga", -1, 14}, {"hudk_hlth7.tga", -1, 13}, {"hudk_hlth8.tga", -1, 14}, {"hudk_hlth9.tga", -1, 14}, {"hudk_hlthdolr.tga", -1, 14}, {"hudk_hlthcoma.tga", -1, 6}, {"hudk_hlthperd.tga", -1, 6}};
static float Hit_red_intensity = 0.4f; // 0x01660EE4
static bool Damage_indicators_enable = true; // 0x01660EE8
bool Debug_show_hud = true; // 0x01660EE9
static const char* Follower_health_bitmap_strings[6] = {"actcount000.tga", "actcount020.tga", "actcount040.tga", "actcount060.tga", "actcount080.tga", "actcount100.tga"}; // 0x01660EEC
static float Hit_damage_fade_speed = 0.8f; // 0x016626E0
static timestamp Show_cash_timestamp; // 0x01661778, initial data ffffffff
static timestamp Show_followers_timestamp; // 0x0166177C, initial data ffffffff
static int Follower_health_bitmap_ids[6]; // 0x02B9558C
static hud_display_slot Hud_slots[4]; // 0x02B955B8
static int Hud_ids[17]; // 0x02B95630
static hud_display_slot Recruit_slot; // 0x02B9DA18
static bool Distance_display_rendering_enabled; // 0x02B9DC07
static int Hud_x_position; // 0x02B9DC10
static int Hud_y_position; // 0x02B9DC14
static int Money_x_position; // 0x02B9DC18
static int Money_y_position; // 0x02B9DC1C
static int Recruit_x_position; // 0x02B9DC20
static int Recruit_y_position; // 0x02B9DC24
static int Alert_level_x_position; // 0x02B9DC28
static int Alert_level_y_position; // 0x02B9DC2C
static int Activity_hud_x_position; // 0x02B9DC30
static int Activity_hud_y_position; // 0x02B9DC34
static int District_name_x_position; // 0x02B9DC38
static int District_name_y_position; // 0x02B9DC3C
static int Control_morale_event_x_position; // 0x02B9DC40
static int Control_morale_event_y_position; // 0x02B9DC44
static int Ally_health_x_position; // 0x02B9DC48
static int Ally_health_y_position; // 0x02B9DC4C
static int Escort_health_x_position; // 0x02B9DC50
static int Escort_health_y_position; // 0x02B9DC54
static float Distance_minimum; // 0x02B9DC58
static float Distance_maximum; // 0x02B9DC5C
static float Distance_shaded_minimum; // 0x02B9DC60
static float Distance_shaded_maximum; // 0x02B9DC64
int Hide_hud_ref_count; // 0x02B9DC68
static float Hit_alpha_multiplier; // 0x02B9DC6C
static float Hit_damage_amount; // 0x02B9DC70
static hit_indicator_data Hit_indicators[20]; // 0x02BDAFE0
static follower_slot Follower_slots[6]; // 0x02BE4EC0

// min(X_res_multiplier, Y_res_multiplier), inlined at every use in rfg.exe: the uniform scale of a resolution
// independent element.
static float hud_uniform_res_scale()
{
    return X_res_multiplier - Y_res_multiplier >= 0.0f ? Y_res_multiplier : X_res_multiplier;
}

// 0x00854E80
void dcf_hud_off()
{
    Hide_hud_ref_count++;
}

// 0x00854E90
void dcf_hud_on()
{
    if (Hide_hud_ref_count > 0)
    {
        Hide_hud_ref_count--;
    }
}

// 0x00841530
xml_element* hud_parse_offset(xml_element* root, const char* ratio_string, const char* desired_cluster_name, int* pos_x, int* pos_y, bool resi)
{
    xml_element* cluster = xml_find(root, "Cluster");
    const char* name = nullptr;
    while (true)
    {
        if (cluster == nullptr)
        {
            return nullptr;
        }
        name = xml_table_get_opt_string_ref(cluster, "ClusterName");
        if (v_stricmp(name, desired_cluster_name) == 0)
        {
            break;
        }
        cluster = xml_find_next(root, cluster, "Cluster");
    }
    if (v_stricmp(name, desired_cluster_name) != 0)
    {
        return nullptr;
    }

    xml_element* offsets = xml_table_find_opt(cluster, "ClusterOffsetList");
    xml_element* offset = xml_find(offsets, "ClusterOffset");
    if (offset == nullptr)
    {
        return nullptr;
    }
    const char* ratio = nullptr;
    do
    {
        ratio = xml_table_get_opt_string_ref(offset, "ResolutionRatio");
        if (v_stricmp(ratio, ratio_string) == 0)
        {
            break;
        }
        offset = xml_find_next(offsets, offset, "ClusterOffset");
    } while (offset != nullptr);

    // Original bug kept: the last offset is read even when the loop ran off the list (offset null); only the name
    // check below keeps it from being used, and that compares the last name again.
    if (ratio != nullptr && v_stricmp(ratio, ratio_string) == 0)
    {
        if (pos_x != nullptr && xml_table_get_opt_int(*pos_x, offset, "XOffset") && resi)
        {
            *pos_x = fl_round(hud_uniform_res_scale() * (float)*pos_x) + Center_screen_x;
        }
        if (pos_y != nullptr && xml_table_get_opt_int(*pos_y, offset, "YOffset") && resi)
        {
            *pos_y = fl_round(hud_uniform_res_scale() * (float)*pos_y) + Center_screen_y;
        }
        return cluster;
    }
    return nullptr;
}

// 0x008416E0 (LTCG: root in ECX, ratio_string in EAX)
static void parse_hud_data(xml_element* root, const char* ratio_string)
{
    xml_element* cluster = hud_parse_offset(root, ratio_string, "HUD", &Hud_x_position, &Hud_y_position, false);
    xml_element* slots = xml_table_find_opt(cluster, "BitmapSlots");
    // Original bug kept: no check against the 4 entries of Hud_slots.
    hud_display_slot* slot = Hud_slots;
    for (xml_element* part = xml_table_find_opt(slots, "Part"); part != nullptr; part = xml_table_find_next(slots, part, "Part"))
    {
        xml_table_get_opt_int(slot->x, part, "XOffset");
        xml_table_get_opt_int(slot->y, part, "YOffset");
        xml_table_get_opt_float(slot->scale, part, "Scale");
        xml_table_get_opt_float(slot->alpha, part, "Alpha");
        slot++;
    }
}

// 0x00841780 (LTCG: root in ECX, ratio_string in EAX; inlined in parse_user_interface_table)
static void parse_hit_indicator_data(xml_element* root, const char* ratio_string)
{
    xml_element* cluster = hud_parse_offset(root, ratio_string, "Hit Indicators", nullptr, nullptr, true);
    xml_element* slots = xml_table_find_opt(cluster, "BitmapSlots");
    xml_element* part = xml_table_find_opt(slots, "Part");
    if (part != nullptr)
    {
        xml_table_get_opt_float(Hit_scale, part, "Scale");
        xml_table_get_opt_float(Hit_alpha_multiplier, part, "Alpha");
    }
}

// 0x008417E0 (LTCG: root in ECX, ratio_string in EAX)
static void parse_recruit_data(xml_element* root, const char* ratio_string)
{
    xml_element* cluster = hud_parse_offset(root, ratio_string, "Recruit", &Recruit_x_position, &Recruit_y_position, true);
    xml_element* slots = xml_table_find_opt(cluster, "BitmapSlots");
    xml_element* part = xml_table_find_opt(slots, "Part");
    // Original bug kept: the first part is read without a check.
    xml_table_get_opt_int(Recruit_slot.x, part, "XOffset");
    xml_table_get_opt_int(Recruit_slot.y, part, "YOffset");
    xml_table_get_opt_float(Recruit_slot.scale, part, "Scale");
    xml_table_get_opt_float(Recruit_slot.alpha, part, "Alpha");
    // Original bug kept: no check against the 6 entries of Follower_slots.
    follower_slot* follower = Follower_slots;
    for (part = xml_table_find_next(slots, part, "Part"); part != nullptr; part = xml_table_find_next(slots, part, "Part"))
    {
        xml_table_get_opt_int(follower->slot.x, part, "XOffset");
        xml_table_get_opt_int(follower->slot.y, part, "YOffset");
        xml_table_get_opt_float(follower->slot.scale, part, "Scale");
        xml_table_get_opt_float(follower->slot.alpha, part, "Alpha");
        follower++;
    }
}

// 0x00854EA0
static void parse_user_interface_table()
{
    xml_element* root = xml_table_parse("user_interface.xtbl");
    xml_element* ui = xml_table_find_opt(root, "UserInterface");
    while (ui != nullptr)
    {
        if (v_stricmp(xml_table_get_opt_string_ref(ui, "Name"), "PC_XBox2") == 0)
        {
            break;
        }
        ui = xml_table_find_next(root, ui, "UserInterface");
    }
    xml_element* clusters = xml_table_find_opt(ui, "UIClusters");

    // rfg.exe reads the screen size twice here and ignores it: the ratio is always 16:9.
    char ratio[8];
    sprintf(ratio, "16:9");

    parse_hud_data(clusters, ratio);
    parse_hit_indicator_data(clusters, ratio);
    parse_recruit_data(clusters, ratio);
    parse_inventory_data(clusters, ratio);
    // Original bug kept: "HUD" is read into the ally health position, overwritten by the next line.
    hud_parse_offset(clusters, ratio, "HUD", &Ally_health_x_position, &Ally_health_y_position, true);
    hud_parse_offset(clusters, ratio, "Ally Health", &Ally_health_x_position, &Ally_health_y_position, true);
    hud_parse_offset(clusters, ratio, "Money", &Money_x_position, &Money_y_position, true);
    hud_parse_offset(clusters, ratio, "Escort Health", &Escort_health_x_position, &Escort_health_y_position, true);
    hud_parse_offset(clusters, ratio, "District Name", &District_name_x_position, &District_name_y_position, true);
    hud_parse_offset(clusters, ratio, "Control Morale Events", &Control_morale_event_x_position, &Control_morale_event_y_position, true);
    hud_parse_offset(clusters, ratio, "Alert Level", &Alert_level_x_position, &Alert_level_y_position, true);
    hud_parse_offset(clusters, ratio, "Activity Hud", &Activity_hud_x_position, &Activity_hud_y_position, true);
    xml_table_close(root);
}

// 0x008418D0 (LTCG: pp in EAX). All that is left of the old health, weapon and notoriety display.
static void hud_render_health_weapon_notoriety(player* pp)
{
    if (pp != nullptr)
    {
        human_weapons_get_weapon(pp);
        gr_set_color(255.0f, 255.0f, 255.0f, Hud_slots[1].alpha * 255.0f);
    }
}

// 0x008550C0: the swap prompt of the object the local player can use.
static void hud_render_pickup_message()
{
    hud_ui_set_swap_menu_active(false);
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->action_object.action_icon != USE_OBJECT_ICON_SWAP || pp->action_object.handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM)
    {
        return;
    }
    if (objp->get_sub_type() == OT_SUB_ITEM_WEAPON || ((item*)objp)->info->safehouse_weapon != nullptr)
    {
        if (pp->get_equipped_weapon() != nullptr)
        {
            inventory_item* swap = player_get_swap_weapon(pp);
            if (swap != nullptr)
            {
                hud_ui_set_swap_menu_active(true);
                hud_ui_set_swap_data(pp->action_object.handle, swap->weapon_handle, true);
            }
        }
    }
    else if (objp->get_sub_type() == OT_SUB_ITEM_MULTI_BACKPACK && pp->backpack != nullptr)
    {
        hud_ui_set_swap_menu_active(true);
        hud_ui_set_swap_data(pp->action_object.handle, pp->backpack->get_handle(), false);
    }
}

// 0x008551A0
static void hud_render_damage_indicators()
{
    if (!hud_damage_indicators_enabled())
    {
        return;
    }
    for (hit_indicator_data& hit : Hit_indicators)
    {
        if (hit.alpha > 0.0f)
        {
            gr_set_color(255.0f, 255.0f, 255.0f, hit.alpha * Hit_alpha_multiplier);
            // Magic number: both directions are flattened onto the plane y = 0.001.
            vector direction(hit.direction.x, 0.001f, hit.direction.z);
            vector camera(Rfg_camera.real_orient.fvec.x, 0.001f, Rfg_camera.real_orient.fvec.z);
            int w, h;
            float scale_x, scale_y;
            bm_get_dimensions(Hud_ids[16], w, h, scale_x, scale_y);
            const float scale = hit.scale;
            const float angle = vec_delta_ang(direction, camera, UP_VECTOR);
            const float x = sinf(angle) * (float)Center_screen_y * scale + (float)Center_screen_x;
            const float y = (float)Center_screen_y - scale * cosf(angle) * (float)Center_screen_y;
            // Magic numbers: pi and pi / 4; the bitmap points up and to the right.
            gr_bitmap_rotated(Hud_ids[16], x, y, (3.1415927f - angle) + 0.7853982f, scale * 2.0f * scale_x, GM_FILTER);
        }
    }
}

// 0x00841920
static void hud_render_firearm_elements()
{
}

// 0x00841930
void hud_render_control_morale_bar(human_teams team, int x, int y, int width, int height, int border, float percent, float alpha)
{
    alpha = alpha * 255.0f;
    if (team == HUMAN_TEAM_CIVILIAN)
    {
        gr_set_color(100.0f, 100.0f, 255.0f, alpha);
    }
    else
    {
        gr_set_color(255.0f, 0.0f, 0.0f, alpha);
    }
    int w, h;
    float scale_x, scale_y;
    bm_get_dimensions(Hud_ids[14], w, h, scale_x, scale_y);
    gr_bitmap_ex(Hud_ids[14], x, (y - (int)((float)height * percent)) + height, width, (int)((float)height * percent), 0, 0, w, (int)((float)h * percent), false, true, GM_FILTER);
    gr_set_color(255.0f, 255.0f, 255.0f, alpha);
    gr_bitmap_ex(Hud_ids[13], x, y, width, height, 0, 0, w, h, false, true, GM_FILTER);
}

// 0x00855390
void hud_refresh_user_interface()
{
    parse_user_interface_table();
}

// 0x008553A0
void hud_system_critical_init()
{
    X_res_multiplier = (float)gr_safe_area_width() / 1280.0f;
    Y_res_multiplier = (float)gr_safe_area_height() / 720.0f;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    Center_screen_x = screen->get_screen_width() / 2;
    Center_screen_y = screen->get_screen_height() / 2;
}

// 0x00855420
void hud_system_init()
{
    for (int i = 0; i < 17; i++)
    {
        Hud_ids[i] = bm_load(Hud_bitmap[i], false);
    }
    for (number_bitmap& number : Health_numbers)
    {
        number.id = bm_load(number.bitmap, false);
    }
    for (int i = 0; i < 6; i++)
    {
        Follower_health_bitmap_ids[i] = bm_load(Follower_health_bitmap_strings[i], false);
    }
    hud_message_system_init();
    hud_floating_text_system_init();
    hud_icon_system_init();
    parse_user_interface_table();
    heartbeat_load_data();
}

// 0x00868500
void hud_level_init_post()
{
    hud_message_level_init_post();
    hud_floating_text_level_init_post();
    hud_reset_damage_indicators();
    hud_icon_level_init_post();
    // Magic number: the money shows 25 s into a level.
    Show_cash_timestamp.set(25000);
    Hide_hud_ref_count = 0;
    Show_followers_timestamp.invalidate();
}

// 0x0087F3F0
void hud_level_shutdown()
{
    hud_icon_level_shutdown();
}

// 0x00855520
void hud_reset_damage_indicators()
{
    for (hit_indicator_data& hit : Hit_indicators)
    {
        hit.alpha = 0.0f;
    }
    Hit_damage_amount = 0.0f;
    heartbeat_reset_timestamp();
}

// 0x00855560
void hud_process_damage_indicators()
{
    if (hud_damage_indicators_enabled())
    {
        const float fade = Frametime * Hit_indicator_fade_speed;
        for (hit_indicator_data& hit : Hit_indicators)
        {
            float alpha = hit.alpha - fade;
            if (alpha - 0.0f < 0.0f)
            {
                alpha = 0.0f;
            }
            hit.alpha = alpha;
        }
        const float amount = Hit_damage_amount - Hit_damage_fade_speed * Frametime;
        Hit_damage_amount = 0.0f;
        if (amount - 0.0f >= 0.0f)
        {
            Hit_damage_amount = amount;
        }
        heartbeat_process(Hit_damage_amount);
    }
    else
    {
        heartbeat_reset_effects();
    }
}

// 0x00855750
void hud_process()
{
    X_res_multiplier = (float)gr_safe_area_width() / 1280.0f;
    Y_res_multiplier = (float)gr_safe_area_height() / 720.0f;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    Center_screen_x = screen->get_screen_width() / 2;
    Center_screen_y = screen->get_screen_height() / 2;
    hud_floating_text_process();
    hud_process_damage_indicators();
    if (Multiplayer && multi_is_dedicated_server())
    {
        return;
    }
    hud_ui_process();
}

// 0x0087F400
void hud_render()
{
    if (Hide_hud_ref_count != 0 || !Debug_show_hud)
    {
        if (cutscene_is_playing())
        {
            hud_message_render();
        }
        return;
    }
    hud_render_damage_indicators();
    player* pp = player_get_local_player();
    if (Cartographer.get_map_mode() == MAP_MODE_FULL)
    {
        return;
    }
    hud_render_health_weapon_notoriety(pp);
    hud_message_render();
    hud_icon_render();
    if (player_is_scoped(pp))
    {
        hud_ui_set_swap_menu_active(false);
        return;
    }
    if (Pickup_message_rendering_enabled)
    {
        hud_render_pickup_message();
    }
    hud_message_render();
    hud_floating_text_render();
    game_hudtimer_render();
    if (Multiplayer)
    {
        const int screen_height = rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
        const int header_height = gr_get_font_height(Header_font);
        const int general_height = gr_get_font_height(General_font);
        gr_set_color(255, 255, 255, 255);
        const respawn_request_deny_reasons reason = multi_game_respawn_can_player_respawn(pp);
        if (pp != nullptr && pp->pflags.mp_display_respawn_msg != 0 && reason != RRDR_NO_REASON && reason != RRDR_TIME_UNTIL_RESPAWN && reason != RRDR_NOT_IN_GAMEPLAY)
        {
            // Magic numbers: 0x8000 centers the string; 0.65 is the fraction of the screen height. Original bug kept:
            // the fraction is -0.65 and subtracted. rfg.exe has this one text for every reason.
            gr_string(0x8000, (header_height - (int)((float)screen_height * -0.65f)) + general_height, "Unknown reason why you can't spawn", Header_font, GM_FILTER);
        }
    }
    multi_game_mode_spectator_render();
}

// 0x00841A90
void hud_hide(bool hide)
{
    if (hide)
    {
        Hide_hud_ref_count++;
        return;
    }
    if (Hide_hud_ref_count > 0)
    {
        Hide_hud_ref_count--;
    }
}

// 0x00841AC0
bool hud_is_hidden()
{
    return Hide_hud_ref_count != 0 || !Debug_show_hud;
}

// 0x00841AE0
void hud_enable_button_input(bool enabled)
{
    Button_input_enabled = enabled;
}

// 0x00841AF0
bool hud_is_button_input_enabled()
{
    return Button_input_enabled;
}

// 0x00841B00
void hud_display_progress_bar(float percent_complete, int x, int y, int width, int height, int border, const color& border_color, const color& bg_color, const color& complete_color)
{
    if (percent_complete - 1.0f >= 0.0f)
    {
        percent_complete = 1.0f;
    }
    gr_set_color(border_color);
    gr_rect(x, y, width, height, GM_FILTER);
    gr_set_color(bg_color);
    const int h = height - border * 2;
    const int w = width - border * 2;
    gr_rect(x + border, y + border, w, h, GM_FILTER);
    gr_set_color(complete_color);
    gr_rect(x + border, y + border, (int)((float)w * percent_complete), h, GM_FILTER);
}

// 0x00841BC0
void hud_display_cash(bool show, unsigned int display_time_ms)
{
    if (show)
    {
        Show_cash_timestamp.set(display_time_ms);
        return;
    }
    Show_cash_timestamp.invalidate();
}

// 0x00841BF0
void hud_display_followers(bool show, unsigned int display_time_ms)
{
    if (show)
    {
        Show_followers_timestamp.set(display_time_ms);
        return;
    }
    Show_followers_timestamp.invalidate();
}

// 0x008557F0
void hud_register_hit(const vector& direction, float percent_intensity)
{
    if (!hud_damage_indicators_enabled())
    {
        return;
    }
    for (hit_indicator_data& hit : Hit_indicators)
    {
        if (hit.alpha <= 0.0f)
        {
            float intensity = percent_intensity;
            if (intensity - 0.5f < 0.0f)
            {
                intensity = 0.5f;
            }
            if (intensity - 1.0f >= 0.0f)
            {
                intensity = 1.0f;
            }
            hit.alpha = intensity * 255.0f;
            // 0x02BE58A8: +-15 degrees (the static local's 30 degrees around the direction).
            static const float max_direction_offset = 0.5235988f;
            // fl_rand order as in rfg.exe: the direction's, then the scale's.
            matrix rotation(UP_VECTOR, (fl_rand() - 0.5f) * max_direction_offset);
            hit.direction = direction.copy_rotate(rotation);
            hit.scale = fl_rand() * 0.03f + Hit_scale;
            break;
        }
    }
    player* hp = player_get_local_player();
    if (hp != nullptr)
    {
        float red = Hit_red_intensity;
        if (human_in_vehicle(hp) != nullptr)
        {
            red = Hit_red_intensity * 0.1f;
        }
        Hit_damage_amount = red * percent_intensity + Hit_damage_amount;
        if (Hit_damage_amount - 1.0f >= 0.0f)
        {
            Hit_damage_amount = 1.0f;
        }
    }
}

// 0x00841C20
void hud_display_firearm_interface(bool display)
{
    Firearm_interface_rendering_enabled = display;
}

// 0x00841C30
void hud_display_pickup_messages(bool display)
{
    Pickup_message_rendering_enabled = display;
}

// 0x00841C40
void hud_display_default()
{
    Firearm_interface_rendering_enabled = true;
    Pickup_message_rendering_enabled = true;
}

// 0x00841C50
void hud_bm_res_independent(int bitmap_id, int cx, int cy, float scale, gr_state state)
{
    int bw, bh;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_id, bw, bh, scale_x, scale_y);
    const int w = fl_round((float)bw * scale * hud_uniform_res_scale());
    const int h = fl_round((float)bh * scale * hud_uniform_res_scale());
    gr_bitmap_ex(bitmap_id, cx - w / 2, cy - h / 2, w, h, 0, 0, bw, bh, false, false, state);
}

// 0x00841D80
void hud_bm_res_independent_ex_flip(int bitmap_id, int cx, int cy, bool flip_x, bool flip_y, gr_state state)
{
    int bw, bh;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_id, bw, bh, scale_x, scale_y);
    const int w = fl_round((float)bw * hud_uniform_res_scale());
    const int h = fl_round((float)bh * hud_uniform_res_scale());
    gr_bitmap_ex(bitmap_id, cx - w / 2, cy - h / 2, w, h, 0, 0, bw, bh, flip_x, flip_y, state);
}

// 0x00841EA0
void hud_bm_pos_res_independent(int bitmap_id, int cx, int cy, float scale, gr_state state)
{
    int bw, bh;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_id, bw, bh, scale_x, scale_y);
    const int w = fl_round((float)bw * scale * hud_uniform_res_scale());
    const int h = fl_round((float)bh * scale * hud_uniform_res_scale());
    const int x = fl_round((float)cx * X_res_multiplier) + (Center_screen_x - w / 2);
    const int y = fl_round((float)cy * Y_res_multiplier) + (Center_screen_y - h / 2);
    gr_bitmap_ex(bitmap_id, x, y, w, h, 0, 0, bw, bh, false, false, state);
}

// 0x00842030
void hud_bm_pos_res_independent_ex_cluster(int bitmap_id, int cx, int cy, int xo, int yo, float scale, gr_state state)
{
    int bw, bh;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_id, bw, bh, scale_x, scale_y);
    const int w = fl_round((float)bw * scale * hud_uniform_res_scale());
    const int h = fl_round((float)bh * scale * hud_uniform_res_scale());
    const int x = fl_round((float)xo * hud_uniform_res_scale()) + fl_round((float)cx * X_res_multiplier) + (Center_screen_x - w / 2);
    const int y = fl_round((float)yo * hud_uniform_res_scale()) + fl_round((float)cy * Y_res_multiplier) + (Center_screen_y - h / 2);
    gr_bitmap_ex(bitmap_id, x, y, w, h, 0, 0, bw, bh, false, false, state);
}

// 0x00842270
void hud_bm_pos_res_independent_rect(int x, int y, int xo, int yo, int w, int h, float scale, gr_state state)
{
    const int sw = fl_round((float)w * scale * hud_uniform_res_scale());
    const int sh = fl_round((float)h * scale * hud_uniform_res_scale());
    const int sx = fl_round((float)xo * hud_uniform_res_scale()) + fl_round((float)x * X_res_multiplier) + (Center_screen_x - sw);
    const int sy = fl_round((float)yo * hud_uniform_res_scale()) + fl_round((float)y * Y_res_multiplier) + (Center_screen_y - sh);
    gr_rect(sx, sy, sw, sh, state);
}

// 0x00842460
void hud_bm_pos_res_independent_ex_stretch(int bitmap_id, int x, int y, float scale, float sw, float sh, gr_state state)
{
    int bw, bh;
    float scale_x, scale_y;
    bm_get_dimensions(bitmap_id, bw, bh, scale_x, scale_y);
    const int w = fl_round((float)bw * scale * sw * hud_uniform_res_scale());
    const int h = fl_round((float)bh * scale * sh * hud_uniform_res_scale());
    const int px = fl_round((float)x * hud_uniform_res_scale()) + Center_screen_x;
    const int py = fl_round((float)y * hud_uniform_res_scale()) + Center_screen_y;
    gr_bitmap_ex(bitmap_id, px, py, w, h, 0, 0, bw, bh, false, false, state);
}

// 0x00842650
void hud_bm_pos_res_independent_ex_bitmap(int bitmap_id, int x, int y, int xo, int yo, int bx, int by, int bw, int bh, float scale, gr_state state)
{
    const int w = fl_round((float)bw * scale * hud_uniform_res_scale());
    const int h = fl_round((float)bh * scale * hud_uniform_res_scale());
    const int sx = fl_round((float)xo * hud_uniform_res_scale()) + fl_round((float)x * X_res_multiplier) + (Center_screen_x - w);
    const int sy = fl_round((float)yo * hud_uniform_res_scale()) + fl_round((float)y * Y_res_multiplier) + (Center_screen_y - h);
    gr_bitmap_ex(bitmap_id, sx, sy, w, h, bx, by, bw, bh, false, false, state);
}

// 0x00842860
int hud_string_wordwrap(int sx, int sy, int max_w, const char* s, int fontnum, const gr_state& state, bool skip_render)
{
    int total_h = 0;
    // rfg.exe writes a terminator into the caller's string for each line and restores it.
    char* text = const_cast<char*>(s);
    while (*text != '\0')
    {
        int len = 0;
        for (const char* p = text; *p != '\0' && *p != '\n'; p++, len++)
        {
            int w, h;
            gr_get_string_size(w, h, text, (int)(p + 1 - text), fontnum);
            if (max_w < w)
            {
                int space = len;
                while (space > 0 && text[space] != ' ')
                {
                    space--;
                }
                if (space == 0)
                {
                    space = len - 1;
                }
                len = space < 1 ? 1 : space;
                break;
            }
        }
        if (!skip_render)
        {
            const char saved = text[len];
            text[len] = '\0';
            gr_string(sx, total_h + sy, text, fontnum, state);
            text[len] = saved;
        }
        if (text[len] == ' ' || text[len] == '\n')
        {
            len++;
        }
        text += len;
        total_h += gr_get_font_height(fontnum);
    }
    return total_h;
}

// 0x00842960
void hud_screen_render(vector2& center, vector2& diameter, int bm_crn, int bm_left, int bm_top, int bm_mid, int bm_lend, int bm_lside, float complete, const float alpha)
{
    const int half_w = (int)(diameter.x * 0.5f * complete);
    const int cx = (int)center.x;
    const int cy = (int)center.y;
    const int half_h = (int)(diameter.y * 0.5f * complete);
    gr_state state(GM_FILTER_CLAMP);
    gr_set_color(255.0f, 255.0f, 255.0f, alpha * 255.0f);
    int w, h;
    float scale_x, scale_y;
    bm_get_dimensions(bm_crn, w, h, scale_x, scale_y);
    if (w * 2 >= half_w * 2 || h * 2 >= half_h * 2)
    {
        return;
    }
    const int top = cy - half_h;
    const int left = cx - half_w;
    gr_bitmap(bm_crn, left, top, state);
    gr_bitmap_ex(bm_crn, (cx - w) + half_w, top, w, h, 0, 0, w, h, true, false, state);
    gr_bitmap_ex(bm_crn, (cx - w) + half_w, (cy - h) + half_h, w, h, 0, 0, w, h, true, true, state);
    gr_bitmap_ex(bm_crn, left, (cy - h) + half_h, w, h, 0, 0, w, h, false, true, state);
    gr_bitmap_ex(bm_left, left, (h - half_h) + cy, w, (half_h - h) * 2, 0, 0, w, h, false, false, state);
    gr_bitmap_ex(bm_left, (cx - w) + half_w, (h - half_h) + cy, w, (half_h - h) * 2, 0, 0, w, h, true, false, state);
    gr_bitmap_ex(bm_top, w + (cx - half_w), top, (half_w - w) * 2, h, 0, 0, w, h, false, false, state);
    gr_bitmap_ex(bm_top, w + (cx - half_w), (cy - h) + half_h, (half_w - w) * 2, h, 0, 0, w, h, false, true, state);
    gr_bitmap_ex(bm_mid, w + (cx - half_w), (h - half_h) + cy, (half_w - w) * 2, (half_h - h) * 2, 0, 0, w, h, false, false, state);
    if (bm_lend > -1 && bm_lside > -1)
    {
        // The side pieces use the corner's dimensions.
        gr_bitmap(bm_lend, (cx - w / 2) - half_w, top, state);
        gr_bitmap_ex(bm_lend, (cx - w / 2) + half_w, top, w, h, 0, 0, w, h, true, false, state);
        gr_bitmap_ex(bm_lend, (cx - w / 2) + half_w, (cy - h) + half_h, w, h, 0, 0, w, h, true, true, state);
        gr_bitmap_ex(bm_lend, (cx - w / 2) - half_w, (cy - h) + half_h, w, h, 0, 0, w, h, false, true, state);
        gr_bitmap_ex(bm_lside, (cx - w / 2) - half_w, (h - half_h) + cy, w, (half_h - h) * 2, 0, 0, w, h, false, false, state);
        gr_bitmap_ex(bm_lside, (cx - w / 2) + half_w, (h - half_h) + cy, w, (half_h - h) * 2, 0, 0, w, h, true, false, state);
    }
}

// 0x00842D80
void hud_display_distance(bool display)
{
    Distance_display_rendering_enabled = display;
}

// 0x00842D90
void hud_distance_set_controls(unsigned int handle, float min, float max, float shaded_min, float shaded_max)
{
    Distance_minimum = min;
    Distance_maximum = max;
    Distance_shaded_minimum = shaded_min;
    Distance_target_handle = handle;
    Distance_shaded_maximum = shaded_max;
}

// 0x00842DE0
bool hud_damage_indicators_enabled()
{
    if (!Damage_indicators_enable)
    {
        return false;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    if (pp->hflags.invulnerable != 0)
    {
        return false;
    }
    if ((Multiplayer || wrecking_crew_is_active()) && multi_game_camera_is_running() && !multi_game_camera_following_killer())
    {
        return false;
    }
    if (wrecking_crew_get_active_mode() == WCM_WALKER)
    {
        return false;
    }
    return Rfg_camera.mode == CAMERA_TURRET_MODE || Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE || Rfg_camera.mode == CAMERA_FREE_MODE || Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE || Rfg_camera.mode == CAMERA_DEATH_MODE;
}
