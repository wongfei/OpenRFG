// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.cpp
//
// wrecking.md WC1: the Fall Guy mode ("barrel dash" in the mode table's display name). Each round marks up to 32
// exploding barrels of the map; five random ones are important (red on the HUD, "HM_RED_BARRELS") and the rest normal
// (blue). The turn ends when every important barrel is destroyed or the time runs out; the score counts destroyed
// barrels, the time left, and a bonus for each colour destroyed completely.

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <keen/base/trace.hpp>

#include <string.h>
#include <wchar.h>

// size 0x8
struct fall_guy_barrel
{
    unsigned int handle; // 0x00
    bool destroyed; // 0x04
};

// size 0x10
struct fall_guy_barrel_stats
{
    int num_important_remaining; // 0x00
    int num_important_total; // 0x04
    int num_normal_remaining; // 0x08
    int num_normal_total; // 0x0C
};

static const float Fall_guy_icon_important_barrel_scale = 0.75f; // 0x012DBF6C
static const float Fall_guy_icon_normal_barrel_scale = 0.333f; // 0x012DBF70
static float Fall_guy_normal_barrel_minimum_dot_visible = 0.83f; // 0x01651904
static int Fall_guy_turn_length_ms = 60000; // 0x01651908
static int Fall_guy_score_per_time_second = 250; // 0x0165190C
static int Fall_guy_score_per_important_barrel = 2500; // 0x01651910
static int Fall_guy_score_per_normal_barrel = 1000; // 0x01651914
static int Fall_guy_score_bonus_for_all_important_barrels_destroyed = 5000; // 0x01651918
static int Fall_guy_score_bonus_for_all_normal_barrels_destroyed = 8000; // 0x0165191C
static color Fall_guy_important_barrel_color(0xC9, 0x4B, 0x4B, 0xFF); // 0x0165A010
static color Fall_guy_normal_barrel_color(0x78, 0xAA, 0xBE, 0xFF); // 0x0165A014
static farray<fall_guy_barrel, 32> Fall_guy_barrels; // 0x0165A018
static fall_guy_barrel_stats Fall_guy_barrel_stats; // 0x02226770
// Magic number 5: the important barrels of a round. Original: fewer than five barrels on the map leaves entries null
// (or stale from an earlier round) and every walk below dereferences them.
static fall_guy_barrel* Fall_guy_important_barrels[5]; // 0x02226BE0

// 0x007612E0 (:70)
static bool fall_guy_barrel_is_important(unsigned int handle)
{
    for (fall_guy_barrel* barrel : Fall_guy_important_barrels)
    {
        if (barrel->handle == handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00761300 (:81): the time bonus doubles while at least half the turn is left.
static int fall_guy_points_per_second(int time_ms)
{
    if (Fall_guy_turn_length_ms / 2 <= time_ms)
    {
        return Fall_guy_score_per_time_second * 2;
    }
    return Fall_guy_score_per_time_second;
}

// 0x00761320 (:91)
static int fall_guy_time_to_seconds(int time_ms)
{
    return (time_ms + 999) / 1000;
}

// 0x00777A80 (:96): in displayed points, converted back to the internal score.
static float fall_guy_calculate_score(int time_left)
{
    float score = 0.0f;
    score = (float)(fall_guy_time_to_seconds(time_left) * fall_guy_points_per_second(time_left)) + score;
    bool all_important_destroyed = true;
    bool all_normal_destroyed = true;
    for (int i = 0; i < Fall_guy_barrels.size(); i++)
    {
        const fall_guy_barrel& barrel = Fall_guy_barrels[i];
        const bool important = fall_guy_barrel_is_important(barrel.handle);
        if (barrel.destroyed)
        {
            score = (float)(important ? Fall_guy_score_per_important_barrel : Fall_guy_score_per_normal_barrel) + score;
        }
        else if (important)
        {
            all_important_destroyed = false;
        }
        else
        {
            all_normal_destroyed = false;
        }
    }
    if (all_important_destroyed)
    {
        score = (float)Fall_guy_score_bonus_for_all_important_barrels_destroyed + score;
    }
    if (all_normal_destroyed)
    {
        score = (float)Fall_guy_score_bonus_for_all_normal_barrels_destroyed + score;
    }
    return wrecking_crew_unconvert_score((int)score);
}

// 0x00761340 (:134): important barrels stay large and grow as their icon fades out after the barrel is gone; normal
// barrels show only near the view direction, fading in towards its centre.
static bool fall_guy_barrel_icon_callback(vector& world_pos, hud_icon* icon)
{
    const bool important = fall_guy_barrel_is_important(icon->obj);
    if (World.object_from_handle(icon->obj) == nullptr)
    {
        if (important)
        {
            if (icon->internal_state.fading_out)
            {
                icon->icon_scale = (float)(hud_icon_get_fade_percent(icon) * 5.0 + 0.75);
            }
            return true;
        }
    }
    else if (important)
    {
        icon->icon_scale = Fall_guy_icon_important_barrel_scale;
        return true;
    }
    icon->icon_scale = Fall_guy_icon_normal_barrel_scale;
    if (important)
    {
        return true;
    }
    vector dir = world_pos - Rfg_camera.real_pos;
    dir.normalize_safe();
    const float dot = Rfg_camera.real_orient.fvec.x * dir.x + Rfg_camera.real_orient.fvec.y * dir.y + Rfg_camera.real_orient.fvec.z * dir.z;
    if (dot <= Fall_guy_normal_barrel_minimum_dot_visible)
    {
        return false;
    }
    icon->icon_color.alpha = (unsigned char)(int)(fl_parameter(dot, Fall_guy_normal_barrel_minimum_dot_visible, 1.0f, false) * 255.0);
    return true;
}

// 0x00777BC0 (:173): recount the barrels and keep the remaining ones highlighted.
void wrecking_crew_mode_fall_guy_gameplay_process()
{
    Fall_guy_barrel_stats.num_important_remaining = 0;
    Fall_guy_barrel_stats.num_important_total = 0;
    Fall_guy_barrel_stats.num_normal_remaining = 0;
    Fall_guy_barrel_stats.num_normal_total = 0;
    for (int i = 0; i < Fall_guy_barrels.size(); i++)
    {
        const fall_guy_barrel& barrel = Fall_guy_barrels[i];
        if (fall_guy_barrel_is_important(barrel.handle))
        {
            Fall_guy_barrel_stats.num_important_total++;
            if (!barrel.destroyed)
            {
                Fall_guy_barrel_stats.num_important_remaining++;
                objective_highlight_add(barrel.handle, HIGHLIGHT_PURPOSE_DESTROY, HIGHLIGHT_STYLE_GLOW);
            }
        }
        else
        {
            Fall_guy_barrel_stats.num_normal_total++;
            if (!barrel.destroyed)
            {
                Fall_guy_barrel_stats.num_normal_remaining++;
                objective_highlight_add(barrel.handle, HIGHLIGHT_PURPOSE_EDF, HIGHLIGHT_STYLE_GLOW);
            }
        }
    }
}

// 0x00761520 (:198)
void wrecking_crew_mode_fall_guy_render(wrecking_crew_states state)
{
}

// 0x00761530 (:204)
void wrecking_crew_mode_fall_guy_match_begin()
{
}

// 0x00790F80 (:208)
void wrecking_crew_mode_fall_guy_round_begin()
{
    Fall_guy_barrels.remove_all();
    const int num_movers = World.get_num_objects<general_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        general_mover* gm = World.get_object<general_mover>(i);
        // Original: strstr("Default", name) looks for the property name inside "Default", not the other way round; a
        // barrel counts unless its gameplay properties are named "Default" (or a part of it).
        if (gm->explodes_on_death() && (gm->gameplay_props == nullptr || strstr("Default", gm->gameplay_props->name) == nullptr))
        {
            fall_guy_barrel barrel;
            barrel.handle = gm->get_handle();
            barrel.destroyed = false;
            Fall_guy_barrels.add(barrel);
            if (Fall_guy_barrels.size() >= 32)
            {
                break;
            }
        }
    }
    if (Fall_guy_barrels.size() <= 4)
    {
        keen::debug::nativePrintArgs("Achtung! Not enough barrels!");
        return;
    }
    int num_important = 0;
    do
    {
        const int pick = int_rand_range(0, Fall_guy_barrels.size() - 1);
        bool taken = false;
        for (int j = 0; j < num_important; j++)
        {
            if (Fall_guy_important_barrels[j]->handle == Fall_guy_barrels[pick].handle)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            Fall_guy_important_barrels[num_important] = &Fall_guy_barrels[pick];
            num_important++;
        }
    } while (num_important < 5);
    for (int i = 0; i < Fall_guy_barrels.size(); i++)
    {
        hud_icon* icon = hud_icon_add(Fall_guy_barrels[i].handle);
        if (icon == nullptr)
        {
            continue;
        }
        if (fall_guy_barrel_is_important(Fall_guy_barrels[i].handle))
        {
            icon->bitmap_id = hud_icon_get_bitmap_id(HI_RING);
            icon->display_distance = true;
            icon->fade_out_time_ms = 500;
            icon->icon_color = Fall_guy_important_barrel_color;
            icon->text_color.set(0xD3, 0x89, 0x00, 0xFF);
        }
        else
        {
            icon->bitmap_id = minimap_get_element_bitmap_id(MAP_ELEMENT_BLIP);
            icon->icon_color = Fall_guy_normal_barrel_color;
        }
        icon->callback = fall_guy_barrel_icon_callback;
        icon->remove_icon_when_obj_is_destroyed = false;
    }
}

// 0x00761540 (:278)
int wrecking_crew_mode_fall_guy_round_end()
{
    const int winner = wrecking_crew_get_player_with_most_round_wins();
    // Original: a -1 winner (no players) reads through a null status.
    const wrecking_crew_player_status* status = (unsigned int)winner < 4 ? &Wrecking_crew_player_status[winner] : nullptr;
    if (status->round_wins < Wrecking_crew_options.rounds_to_win)
    {
        return -1;
    }
    return winner;
}

// 0x00761570 (:289)
int wrecking_crew_mode_fall_guy_players_turn_begin()
{
    return Fall_guy_turn_length_ms;
}

// 0x00761580 (:294)
int wrecking_crew_mode_fall_guy_players_turn_end(int time_left)
{
    return wrecking_crew_get_player_with_highest_round_score();
}

// 0x00761590 (:299)
bool wrecking_crew_mode_fall_guy_is_players_turn_over()
{
    for (const fall_guy_barrel* barrel : Fall_guy_important_barrels)
    {
        if (!barrel->destroyed)
        {
            return false;
        }
    }
    return true;
}

// 0x00777C60 (:312)
float wrecking_crew_mode_fall_guy_get_current_players_score(int time_left)
{
    return fall_guy_calculate_score(time_left);
}

// 0x00777C80 (:317)
void wrecking_crew_mode_fall_guy_map_is_being_restored()
{
    for (int i = 0; i < Fall_guy_barrels.size(); i++)
    {
        Fall_guy_barrels[i].destroyed = false;
    }
}

// 0x00777CB0 (:324)
void wrecking_crew_mode_fall_guy_mover_destroyed(const object_mover* mover)
{
    if (Wrecking_crew_state != WCS_PLAYERS_TURN && Wrecking_crew_state != WCS_PRE_BREAK)
    {
        return;
    }
    for (int i = 0; i < Fall_guy_barrels.size(); i++)
    {
        if (Fall_guy_barrels[i].handle == mover->get_handle())
        {
            Fall_guy_barrels[i].destroyed = true;
            gameplay_play_sfx(fall_guy_barrel_is_important(Fall_guy_barrels[i].handle) ? GSFX_WRECKING_CREW_IMPORTANT_BARREL_DESTROYED : GSFX_WRECKING_CREW_NORMAL_BARREL_DESTROYED);
            objective_highlight_remove(Fall_guy_barrels[i].handle);
            return;
        }
    }
}

// 0x007615B0 (:354): destroyed / total of each colour.
void wrecking_crew_mode_fall_guy_get_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score)
{
    score_set->should_display = true;
    wcsncpy(score_set->label, localization_get_display_string_req("HM_RED_BARRELS", nullptr), 32);
    score_set->label[31] = L'\0';
    _snprintf_safe(score_set->text, 32, "%d/%d", Fall_guy_barrel_stats.num_important_total - Fall_guy_barrel_stats.num_important_remaining, Fall_guy_barrel_stats.num_important_total);
    high_score_set->should_display = true;
    wcsncpy(high_score_set->label, localization_get_display_string_req("HM_BLUE_BARRELS", nullptr), 32);
    high_score_set->label[31] = L'\0';
    _snprintf_safe(high_score_set->text, 32, "%d/%d", Fall_guy_barrel_stats.num_normal_total - Fall_guy_barrel_stats.num_normal_remaining, Fall_guy_barrel_stats.num_normal_total);
    *green_score = false;
}

// 0x00761650 (:368)
void wrecking_crew_mode_fall_guy_get_turn_end_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score)
{
    score_set->should_display = false;
    high_score_set->should_display = false;
    *green_score = false;
}

// 0x00761670 (:376): the score breakdown of the turn, shown on the break.
void wrecking_crew_mode_fall_guy_get_barrel_text(hud_wrecking_crew_barrel_text_set* barrel_set)
{
    barrel_set->should_display = false;
    if (Wrecking_crew_state != WCS_BREAK)
    {
        return;
    }
    barrel_set->should_display = true;
    const int time_left = Wrecking_crew_turn_time_left;
    const int player_index = Wrecking_crew_current_player;
    if ((unsigned int)player_index >= 4)
    {
        return;
    }
    wcsncpy(barrel_set->row_one.label, localization_get_display_string_req("HM_RED_BARRELS", nullptr), 32);
    barrel_set->row_one.label[31] = L'\0';
    barrel_set->row_one.value_one = Fall_guy_barrel_stats.num_important_total - Fall_guy_barrel_stats.num_important_remaining;
    barrel_set->row_one.value_two = Fall_guy_score_per_important_barrel;
    wcsncpy(barrel_set->mini_row_one.label, localization_get_display_string_req("HM_ALL_RED_BONUS", nullptr), 32);
    barrel_set->mini_row_one.label[31] = L'\0';
    barrel_set->mini_row_one.value_two = Fall_guy_score_bonus_for_all_important_barrels_destroyed;
    barrel_set->mini_row_one.value_one = Fall_guy_barrel_stats.num_important_remaining < 1 ? 1 : 0;
    wcsncpy(barrel_set->row_two.label, localization_get_display_string_req("HM_BLUE_BARRELS", nullptr), 32);
    barrel_set->row_two.label[31] = L'\0';
    barrel_set->row_two.value_one = Fall_guy_barrel_stats.num_normal_total - Fall_guy_barrel_stats.num_normal_remaining;
    barrel_set->row_two.value_two = Fall_guy_score_per_normal_barrel;
    wcsncpy(barrel_set->mini_row_two.label, localization_get_display_string_req("HM_ALL_BLUE_BONUS", nullptr), 32);
    barrel_set->mini_row_two.label[31] = L'\0';
    barrel_set->mini_row_two.value_two = Fall_guy_score_bonus_for_all_normal_barrels_destroyed;
    barrel_set->mini_row_two.value_one = Fall_guy_barrel_stats.num_normal_remaining < 1 ? 1 : 0;
    wcsncpy(barrel_set->row_three.label, localization_get_display_string_req("HM_TIME", nullptr), 32);
    barrel_set->row_three.label[31] = L'\0';
    barrel_set->row_three.value_one = fall_guy_time_to_seconds(time_left);
    barrel_set->row_three.value_two = fall_guy_points_per_second(time_left);
    wcsncpy(barrel_set->final_row_label, localization_get_display_string_req("MENU_MISSION_COMPLETE_WORD_TOTAL", nullptr), 32);
    barrel_set->final_row_label[31] = L'\0';
    barrel_set->final_row_value = wrecking_crew_convert_score(Wrecking_crew_player_status[player_index].turn_score);
}

// 0x00761840 (:420)
hud_wrecking_crew_icon_state wrecking_crew_mode_fall_guy_get_time_icon_state()
{
    return HWCIS_NORMAL;
}
