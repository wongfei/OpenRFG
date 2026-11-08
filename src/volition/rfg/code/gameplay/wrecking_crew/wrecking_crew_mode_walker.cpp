// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.cpp
//
// wrecking.md WC1: the walker mode. Each player's turn starts in one walker (civilian, mining or flamer: the options'
// walker_choice, or a random one vetoable on the splash) placed at a random player start; wrecking_crew.xtbl's
// locked_walker_starts disables the starts a walker cannot use.

#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_internal.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <keen/base/trace.hpp>

static int Walker_time_civ_per_turn_ms = 50000; // 0x01651930
static int Walker_time_min_per_turn_ms = 60000; // 0x01651934
static int Walker_time_combat_per_turn_ms = 55000; // 0x01651938
static int Wrecking_crew_current_walker = -1; // 0x0165193C: vehicle type of the current walker
static int Walker_start = -1; // 0x01651940: player start index of this round
static unsigned int Current_walker_handle = 0xFFFFFFFF; // 0x01651944
static farray<disabled_start_pos, 30> Disabled_start_positions; // 0x0165A128
static bool Enter_walker_asap; // 0x022296BE
static wrecking_crew_walker_choices Current_walker = WCWC_CIVILIAN; // 0x022296C4: the random choice (walker_choice none)

// 0x00777D50 (:61)
static bool start_position_is_disabled(const disabled_start_pos& start_pos)
{
    for (int i = 0; i < Disabled_start_positions.size(); i++)
    {
        if (start_pos.map_index == Disabled_start_positions[i].map_index && start_pos.start_pos_index == Disabled_start_positions[i].start_pos_index)
        {
            return true;
        }
    }
    return false;
}

// 0x00791520 (:74): a random enabled start other than the last one.
static void pick_random_start_position()
{
    const int num_starts = World.get_num_objects<player_start>();
    if (num_starts < 1)
    {
        keen::debug::nativePrintArgs("No start points on map. Game is likely to explodicate very shortly.");
    }
    const int first = int_rand_range(0, num_starts - 1);
    for (int i = 0; i < num_starts; i++)
    {
        const int index = (i + first) % num_starts;
        if (index == Walker_start)
        {
            continue;
        }
        disabled_start_pos start_pos;
        start_pos.map_index = Wrecking_crew_options.map_index;
        start_pos.start_pos_index = index;
        if (!start_position_is_disabled(start_pos))
        {
            Walker_start = index;
            break;
        }
    }
    if (Walker_start < 0 || num_starts <= Walker_start)
    {
        Walker_start = 0;
    }
}

// The current walker, or null.
static vehicle* wc_current_walker()
{
    if (Current_walker_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(Current_walker_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_WALKER)
    {
        return nullptr;
    }
    return (vehicle*)objp;
}

// 0x007915C0 (:120)
static void destroy_walker()
{
    if (World.get_num_objects<walker>() == 0)
    {
        return;
    }
    vehicle* vp = wc_current_walker();
    if (vp == nullptr)
    {
        return;
    }
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver != nullptr)
    {
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        vehicle_interaction_request_exit(driver, &exit_data);
        animlib_clear_looping_animations(driver->char_inst->aim_handle, true);
        animlib_clear_non_looping_animations(driver->char_inst->aim_handle);
        vanim_change_state(driver->char_inst->vp, index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
        vanim_process_states(driver->char_inst->vp);
        object_set_visibility_plus_children(driver, false);
    }
    World.destroy_object(vp, false);
    Current_walker_handle = 0xFFFFFFFF;
}

// 0x0079F3C0 (:144): put the walker (created again if it is gone or a corpse) on this round's start and the local player
// in it; until the walker's resources are in, retry every frame (Enter_walker_asap).
static void reset_walker()
{
    vehicle* vp = wc_current_walker();
    if (vp == nullptr || vehicle_is_corpse(vp))
    {
        wrecking_crew_create_walker();
        vp = wc_current_walker();
    }
    // Original: a walker that could not be created is read through null here.
    walker* wp = (walker*)vp;
    if (!wp->w_rdd.is_loaded())
    {
        Enter_walker_asap = true;
        return;
    }
    Enter_walker_asap = false;
    object* start = World.get_object<player_start>(Walker_start);
    if (start == nullptr)
    {
        return;
    }
    vector pos = start->pos;
    wp->w_flags.teleport_components = 1;
    vp->update_pos_and_orient(pos, start->orient, true);
    vp->last_velocity.zero();
    vp->last_angular_velocity.zero();
    vp->forward_velocity = 0.0f;
    vp->last_forward_velocity = 0.0f;
    // Magic numbers: the camera starts 1.9 m above the start and 0.3 m behind it.
    vector camera_pos = UP_VECTOR * 1.9f + pos - start->orient.fvec * 0.3f;
    camera_set_transform(camera_pos, start->orient);
    human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
    player* local_player = player_get_local_player();
    if (driver != nullptr)
    {
        if (driver == local_player)
        {
            camera_trans_abort();
            vehicle_set_invulnerable(vp->get_handle(), true);
            return;
        }
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        exit_data.flags.maintain_position = 1;
        vehicle_interaction_request_exit(driver, &exit_data);
        human_teleport(driver, pos, driver->orient, 6.0f, false);
    }
    vanim_change_state(vp->vp, index_alias(0x6A, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
    vanim_process_states(vp->vp);
    vi_enter_data enter_data;
    enter_data.specified_seat = VSI_INVALID;
    enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_BEST;
    enter_data.destination_seat = VSI_INVALID;
    *(unsigned char*)&enter_data.flags = 0;
    enter_data.flags.teleport = 1;
    vehicle_interaction_request_enter(local_player, vp, &enter_data);
    camera_trans_abort();
    vehicle_set_invulnerable(vp->get_handle(), true);
}

// 0x00761BE0 (:245)
static wrecking_crew_walker_choices wrecking_crew_current_walker_choice()
{
    if (Wrecking_crew_options.walker_choice == WCWC_NONE)
    {
        return Current_walker;
    }
    return Wrecking_crew_options.walker_choice;
}

// 0x00761BF0 (:253)
static int walker_round_time()
{
    switch (wrecking_crew_current_walker_choice())
    {
    case WCWC_CIVILIAN:
        return Walker_time_civ_per_turn_ms;
    case WCWC_MINING:
        return Walker_time_min_per_turn_ms;
    case WCWC_FLAMER:
        return Walker_time_combat_per_turn_ms;
    default:
        return 60000; // magic number: one minute
    }
}

// The HUD icon of the current walker.
static const char* walker_icon_name()
{
    switch (wrecking_crew_current_walker_choice())
    {
    case WCWC_CIVILIAN:
        return "ui_hud_icon_walker";
    case WCWC_FLAMER:
        return "ui_hud_icon_combatwalker";
    default:
        return "ui_hud_icon_walker_min";
    }
}

// 0x00761C30 (:284)
int wrecking_crew_get_current_walker_vehicle_type()
{
    return Wrecking_crew_current_walker;
}

// 0x00761C40 (:289)
const char* wrecking_crew_get_walker_display_name(wrecking_crew_walker_choices choice)
{
    if (choice == WCWC_NONE)
    {
        choice = Current_walker;
    }
    if (choice < NUM_WRECKING_CREW_WALKER_CHOICES)
    {
        return Wrecking_crew_walkers[choice].display_name;
    }
    return "MENU_NONE";
}

// 0x00761C70 (:298): the veto on the splash cycles the random walker.
void wrecking_crew_next_walker()
{
    if (Wrecking_crew_options.walker_choice != WCWC_NONE)
    {
        return;
    }
    Current_walker = (wrecking_crew_walker_choices)(Current_walker + 1);
    if (Current_walker < 0 || Current_walker > WCWC_FLAMER)
    {
        Current_walker = WCWC_CIVILIAN;
    }
    Wrecking_crew_current_walker = Current_walker >= 0 ? Wrecking_crew_walkers[Current_walker].vehicle_type : -1;
}

// 0x007916C0 (:309)
void wrecking_crew_assign_walker()
{
    if (Wrecking_crew_options.mode != WCM_WALKER)
    {
        destroy_walker();
        return;
    }
    if (Wrecking_crew_options.walker_choice == WCWC_NONE)
    {
        Current_walker = (wrecking_crew_walker_choices)int_rand_range(0, 2);
        if ((unsigned int)Current_walker < 3)
        {
            Wrecking_crew_current_walker = Wrecking_crew_walkers[Current_walker].vehicle_type;
            return;
        }
    }
    else if (Wrecking_crew_options.walker_choice < NUM_WRECKING_CREW_WALKER_CHOICES)
    {
        Wrecking_crew_current_walker = Wrecking_crew_walkers[Wrecking_crew_options.walker_choice].vehicle_type;
        return;
    }
    Wrecking_crew_current_walker = -1;
}

// 0x00791720 (:324): a new walker at this round's start; every player's animations are cleared first.
void wrecking_crew_create_walker()
{
    destroy_walker();
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp != nullptr)
        {
            animlib_clear_looping_animations(pp->char_inst->aim_handle, true);
            animlib_clear_non_looping_animations(pp->char_inst->aim_handle);
            vanim_change_state(pp->char_inst->vp, index_alias(1, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
            vanim_process_states(pp->char_inst->vp);
        }
    }
    vector pos = ZERO_VECTOR;
    matrix orient(IDENTITY_MATRIX);
    const object* start = World.get_object<player_start>(Walker_start);
    pos = start->pos;
    orient = start->orient;
    if (Wrecking_crew_options.walker_choice != WCWC_NONE)
    {
        if (Wrecking_crew_options.mode != WCM_WALKER)
        {
            destroy_walker();
        }
        else if (Wrecking_crew_options.walker_choice < NUM_WRECKING_CREW_WALKER_CHOICES)
        {
            Wrecking_crew_current_walker = Wrecking_crew_walkers[Wrecking_crew_options.walker_choice].vehicle_type;
        }
        else
        {
            Wrecking_crew_current_walker = -1;
        }
    }
    obj_prop_block pblock(pos, orient, nullptr);
    pblock.set<int>(VEHICLE_PROP_TYPE, Wrecking_crew_current_walker);
    pblock.set<bool>(VEHICLE_PROP_AT_GROUND, false);
    pblock.set<bool>(VEHICLE_PROP_AT_GROUND, true); // original: set twice, the second value wins
    pblock.set<float>(VEHICLE_INITIAL_SPEED, 0.0f);
    // Original: a failed creation is read through null here.
    object* objp = World.create_object<walker>(pblock, 0xFFFFFFFF);
    Current_walker_handle = objp->get_handle();
}

// 0x00791960 (:363)
bool wrecking_crew_walker_loaded()
{
    vehicle* vp = wc_current_walker();
    return vp != nullptr && ((walker*)vp)->w_rdd.is_loaded();
}

// 0x0079F6C0 (:378)
void wrecking_crew_mode_walker_gameplay_process()
{
    if (Enter_walker_asap)
    {
        reset_walker();
    }
}

// 0x00761CD0 (:385)
void wrecking_crew_mode_walker_render(wrecking_crew_states state)
{
}

// 0x00761CE0 (:391)
void wrecking_crew_mode_walker_match_begin()
{
    Current_walker_handle = 0xFFFFFFFF;
}

// 0x00761CF0 (:396)
void wrecking_crew_mode_walker_match_end()
{
}

// 0x0079F6D0 (:400)
void wrecking_crew_mode_walker_round_begin()
{
    pick_random_start_position();
    reset_walker();
}

// 0x00761D00 (:406)
int wrecking_crew_mode_walker_round_end()
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

// 0x0079F6E0 (:417)
int wrecking_crew_mode_walker_players_turn_begin()
{
    wrecking_crew_respawn_player();
    reset_walker();
    wrecking_crew_walker_visible(true);
    return walker_round_time();
}

// 0x00761D30 (:429)
int wrecking_crew_mode_walker_players_turn_end(int time_left)
{
    return wrecking_crew_get_player_with_highest_round_score();
}

// 0x0079F730 (:434)
void wrecking_crew_mode_walker_player_respawned()
{
    reset_walker();
}

// 0x00761D40 (:439): the walker's icon instead of a weapon.
void wrecking_crew_mode_walker_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    buffer[0] = L'\0';
    *override_icon = true;
    strncpy_safe(icon, walker_icon_name(), icon_buffer_len);
}

// 0x00761DD0 (:461): no backpack.
void wrecking_crew_mode_walker_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len)
{
    buffer[0] = L'\0';
    *override_icon = true;
    icon[0] = '\0';
}

// 0x00761DF0 (:468): the walker can be vetoed when it is random; the label holds the icon name.
bool wrecking_crew_mode_walker_get_veto_text(hud_wrecking_crew_text_set* weapon_set, bool* can_veto_weapon, hud_wrecking_crew_text_set* backpack_set, bool* can_veto_backpack)
{
    *can_veto_weapon = Wrecking_crew_options.walker_choice == WCWC_NONE;
    weapon_set->should_display = true;
    copy_to_wide(weapon_set->label, walker_icon_name(), 32);
    const wrecking_crew_walker_choices choice = wrecking_crew_current_walker_choice();
    const char* name = "MENU_NONE";
    if (choice < NUM_WRECKING_CREW_WALKER_CHOICES)
    {
        name = Wrecking_crew_walkers[choice].display_name;
    }
    if (name != nullptr)
    {
        copy_to_wide(weapon_set->text, name, 32);
    }
    *can_veto_backpack = false;
    backpack_set->should_display = false;
    backpack_set->label[0] = L'\0';
    backpack_set->text[0] = L'\0';
    return true;
}

// 0x00761EB0 (:507)
hud_wrecking_crew_icon_state wrecking_crew_mode_walker_get_time_icon_state()
{
    return HWCIS_NORMAL;
}

// 0x00777D90 (:512)
void wrecking_crew_walker_clear_start_positions()
{
    Disabled_start_positions.remove_all();
}

// 0x007919A0 (:517)
void wrecking_crew_walker_visible(bool visible)
{
    if (World.get_num_objects<walker>() == 0)
    {
        return;
    }
    vehicle* vp = wc_current_walker();
    if (vp == nullptr)
    {
        return;
    }
    object_set_visibility_plus_children(vp, visible);
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver != nullptr)
    {
        object_set_visibility_plus_children(driver, visible);
    }
}

// 0x00791A00 (:530)
void wrecking_crew_mode_walker_disable_start_pos(int map_index, int start_pos_index)
{
    disabled_start_pos start_pos;
    start_pos.map_index = map_index;
    start_pos.start_pos_index = start_pos_index;
    if (start_position_is_disabled(start_pos))
    {
        return;
    }
    if (Disabled_start_positions.size() < Disabled_start_positions.capacity())
    {
        Disabled_start_positions.add(start_pos);
    }
}
