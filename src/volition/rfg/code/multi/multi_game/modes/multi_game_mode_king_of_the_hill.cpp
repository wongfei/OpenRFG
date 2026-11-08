// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.cpp
//
// The King of the Hill row of the mode table (shown as Damage Control): the level's King of the Hill markers (5 at
// most) sit on buildings. A building damaged to 76 % is destroyed (neutral); repaired to 26 % it belongs to the last
// repairer's team. Every 2 s each held target scores a point for its team (2 with every target held, by the options'
// multiplier). The server decides (PT_MODE_SPECIFIC: 0 destroyed, 1 repaired, 2 every target's state after a host
// migration). The targets have HUD icons with status rings and their own HUD document data group.
// Experience events without a weapon pass 0xFF as the weapon id, as rfg.exe does.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_king_of_the_hill.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_icon.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "keen/base/trace.hpp"

#include <float.h>
#include <math.h>
#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// size 0x78
struct king_of_the_hill_target
{
    king_of_the_hill_target(); // 0x00977380

    wchar_t name[47]; // 0x00
    multi_object_marker* marker; // 0x60
    multi_timestamp scoring_timer; // 0x64
    bool destroyer_was_friendly; // 0x68
    bool full_control_awarded; // 0x69
    float percent_destroyed_last_frame; // 0x6C
    float percent_destroyed_accumulator; // 0x70
    float percent_repaired_accumulator; // 0x74
};

// 0x00977380: the scoring timer starts invalid (its constructor)
king_of_the_hill_target::king_of_the_hill_target()
{
}

// ---- file-static globals
// How often the server logs the destruction volume.
static int Logging_snapshot_time_interval_ms = 15000; // 0x0167DEBC
static int Target_update_play_id_handle = -1; // 0x0167DEC0
// The status ring colors of a target being damaged and being repaired.
static color Status_damage_color(0xFF, 0x00, 0x00, 0xFF); // 0x01691DCC
static color Status_repairing_color(0xFF, 0xFF, 0x99, 0xFF); // 0x01691DD0
static timestamp_realtime Logging_snapshot_timer; // 0x01691DD4, initial data ffffffff
// multi_announcer_line_info Multi_announcer_lines[52] (0x01691F00) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// multi_team_stats Mp_team_stats[2] (0x02FE82F0) is used here too; placed in multi_game_mode_bagman.cpp.
// point_of_interest Camera_points_of_interest[2] (0x02FE8300) is used here too; placed in multi_game_camera.cpp.
static vint_data_item* Target_data_items[5]; // 0x02FE8DF0
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Dedicated (0x02FEB591) is used here too; placed in multi.cpp.
static int Num_king_of_the_hill_targets; // 0x02FEB644
static vint_data_group* Targets_data_group; // 0x02FEB648
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// multi_player_stats* Mp_player_stats (0x02FEB720) is used here too; placed in multi_game_mode_bagman.cpp.
static king_of_the_hill_target King_of_the_hill_targets[5]; // 0x02FEDA38
// The backpack objects (multi_game_object_backpack.cpp allocates them; the stem rule placed the pool in its first
// user). Moved here from rfg_multi_unity_pending_early.inl.
static object_pool<multi_object_backpack,76,1> multi_object_backpackpool; // 0x02FEDC90 (0x01071080)
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.

static const int Max_king_of_the_hill_targets = 5;
// The PT_MODE_SPECIFIC subtypes (the byte after the header).
static const unsigned char Koth_destroyed = 0;
static const unsigned char Koth_repaired = 1;
static const unsigned char Koth_migration = 2;
// The building's destruction fraction that destroys a target, and the one a destroyed target must be repaired to.
static const float Koth_destroyed_percent = 0.76f;
static const float Koth_repaired_percent = 0.26f;
// The fraction under which a held target counts as untouched (full control).
static const float Koth_full_control_percent = 0.01f;
// A held target scores every 2 s.
static const int Koth_scoring_interval_ms = 2000;
// The "near the point limit" announcements start 50 points before a limit over 50.
static const int Koth_win_near_points = 50;

// ---- internal functions
// Port helper: the rfg mover a target's marker sits on (null if none), as rfg.exe inlines it.
static rfg_mover* target_building(const king_of_the_hill_target& target)
{
    const unsigned int handle = target.marker->get_parent();
    if (handle == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return (rfg_mover*)objp;
}

// Port helper: the player stats of a game data handle (index 16 passes, as in rfg.exe).
static multi_player_stats* koth_stats_of(unsigned int handle)
{
    if (handle == 0xFFFFFFFFu || (handle & 0xFFFF) > 16)
    {
        return nullptr;
    }
    return &Mp_player_stats[handle & 0xFFFF];
}

// 0x00994250 (:123): the target's icon: its team's color (white when destroyed), the team's map element, and on a
// target of ours status rings while its building is being damaged or repaired. Original: the building is not checked
// for null.
static bool king_of_the_hill_hud_icon_callback(vector& obj_pos, hud_icon* icon)
{
    local_player_info info;
    player_get_local_player_info(&info);
    const king_of_the_hill_target* target = (const king_of_the_hill_target*)icon->parameter;
    const multi_object_marker* marker = target->marker;
    rfg_mover* building = target_building(*target);
    // Magic numbers: the icon and its name's scales.
    icon->icon_scale = 1.5f;
    icon->text_font = Body_font;
    icon->text_scale = 0.75f;
    if (!marker->destroyed)
    {
        multi_game_render_get_team_color(icon->icon_color, MTCT_ICON, marker);
    }
    else
    {
        icon->icon_color = color::WHITE;
    }
    icon->text_color = icon->icon_color;
    minimap_elements element = MAP_ELEMENT_KOTH_NEUTRAL;
    if (!marker->destroyed)
    {
        if (marker->team == HUMAN_TEAM_GUERILLA)
        {
            element = MAP_ELEMENT_KOTH_GUERRILLA;
        }
        else if (marker->team == HUMAN_TEAM_EDF)
        {
            element = MAP_ELEMENT_KOTH_EDF;
        }
    }
    icon->bitmap_id = minimap_get_element_bitmap_id(element);
    const human_team_dispositions disposition = human_teams_get_disposition(marker->team, info.team);
    if (marker->destroyed)
    {
        hud_icon_enable_status_rings(icon->obj, false);
        return true;
    }
    if (disposition != TEAM_DISPOSITION_ALLIED)
    {
        return true;
    }
    color ring_color;
    if (building->percent_destroyed() > target->percent_destroyed_last_frame)
    {
        ring_color = Status_damage_color;
    }
    else if (building->percent_destroyed() < target->percent_destroyed_last_frame)
    {
        ring_color = Status_repairing_color;
    }
    else
    {
        return true;
    }
    icon->status_ring_color.red = ring_color.red;
    icon->status_ring_color.green = ring_color.green;
    icon->status_ring_color.blue = ring_color.blue;
    icon->status_ring_color.alpha = 0xFF;
    hud_icon_enable_status_rings(icon->obj, true);
    return true;
}

// 0x009943F0 (:174): the target's icon with its name and the status ring setup. LTCG: target_index in EAX.
static void add_target_icon(int target_index)
{
    if (target_index < 0 || target_index >= Num_king_of_the_hill_targets)
    {
        return;
    }
    // Original: the local player's info is fetched and not used.
    local_player_info info;
    player_get_local_player_info(&info);
    king_of_the_hill_target& target = King_of_the_hill_targets[target_index];
    // Original: the icon is not checked for null.
    hud_icon* icon = hud_icon_add(target.marker->get_handle());
    icon->callback = king_of_the_hill_hud_icon_callback;
    icon->parameter = &target;
    icon->snap_to_edge = true;
    wcsncpy(icon->text[0], target.name, 32);
    icon->text[0][31] = L'\0';
    icon->status_ring_bitmap_id = hud_icon_get_bitmap_id(HI_STATUS_RING);
    // Magic numbers: a ring every 0.4 s lasting 1.5 s, for 4 s, growing to 5 times.
    icon->status_ring_interval_ms = 400;
    icon->status_ring_lifetime_ms = 1500;
    icon->status_ring_total_lifetime_ms = 4000;
    icon->status_ring_max_scale = 5.0f;
}

// 0x0094DB70 (:199): LTCG: target_index in EAX.
static void remove_target_icon(int target_index)
{
    if (target_index >= 0 && target_index < Num_king_of_the_hill_targets)
    {
        hud_icon_remove(King_of_the_hill_targets[target_index].marker->get_handle());
    }
}

// Port helper: the target event cue (the playing one stops).
static void koth_play_target_update(int cue_id)
{
    if (audiolib_is_playing(Target_update_play_id_handle, true))
    {
        audiolib_play_stop(Target_update_play_id_handle);
    }
    Target_update_play_id_handle = audiolib_cue_play(cue_id, nullptr, nullptr);
}

// Port helper: the server's log of a change of a target's owner.
static void koth_log_ownership(const rfg_mover* building, int target_index, human_teams new_owner)
{
    const vector pos = building->pos;
    const int duration = gamelog_get_game_duration();
    const short seconds_left = (short)(Timer_time.time_until() / 1000);
    const int time = gamelog_get_logging_time();
    log_event_damage_control_target_changed_ownership(gamelog_get_player_checksum(), time, seconds_left, (char)target_index, (char)new_owner, duration, pos);
}

// 0x009ED150 (:209): a target is destroyed. The server: its enemy destroyer gets the destroy experience and a
// destruction stat, the enemies of the target near the building the assist; whether the last damage came from a
// teammate of the target; the server tells everyone (with the team scores) and logs it. The announcer (not on a
// dedicated server), the HUD and the cue; the target becomes neutral.
static void king_of_the_hill_target_destroyed(int target_index, bool destroyer_was_friendly)
{
    if (target_index < 0 || target_index >= Num_king_of_the_hill_targets)
    {
        return;
    }
    king_of_the_hill_target& target = King_of_the_hill_targets[target_index];
    if (Multiplayer && Server)
    {
        rfg_mover* building = target_building(target);
        player* destroyer = multi_game_last_player_to_damage_rfg_mover(building);
        destroyer_was_friendly = false;
        if (destroyer != nullptr)
        {
            if (human_teams_get_disposition(destroyer->current_team, target.marker->team) == TEAM_DISPOSITION_HOSTILE)
            {
                multi_game_xp_trigger_event(destroyer->mp_data, MXE_DESTROY_KING_OF_THE_HILL_TARGET, 1, 0xFF, -1, false);
                vector bmin;
                vector bmax;
                building->get_bbox(bmin, bmax);
                const float assist_dist = multi_game_xp_get_event_parameter(MXE_DESTROY_ASSIST_KING_OF_THE_HILL_TARGET); // inlined
                const float assist_dist_squared = assist_dist * assist_dist;
                const int count = World.get_num_objects<player>();
                for (int i = 0; i < count; i++)
                {
                    player* pp = World.get_object<player>(i);
                    if (pp != destroyer && human_teams_are_enemies(target.marker->team, pp->current_team) &&
                        bbox_find_distance_squared(bmin, bmax, pp->pos) <= assist_dist_squared)
                    {
                        multi_game_xp_trigger_event(pp->mp_data, MXE_DESTROY_ASSIST_KING_OF_THE_HILL_TARGET, 1, 0xFF, -1, false);
                    }
                }
                multi_player_stats* stats = multi_game_stats_get_player_stats(destroyer->mp_data);
                if (stats != nullptr)
                {
                    stats->dc_target_destruction++;
                }
            }
            destroyer_was_friendly = human_teams_are_allies(destroyer->current_team, target.marker->team);
        }
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Koth_destroyed;
        packet[packet_size++] = (unsigned char)target_index;
        packet[packet_size++] = destroyer_was_friendly;
        multi_game_stats_write_all_team_scores(packet, packet_size);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
        // Original: the building is not checked for null.
        koth_log_ownership(building, target_index, HUMAN_TEAM_CIVILIAN);
    }
    if (!(Multiplayer && Server && Dedicated) && !Spectating)
    {
        local_player_info info;
        player_get_local_player_info(&info);
        if (human_teams_are_allies(target.marker->team, info.team))
        {
            multi_game_announcer_play_line(MAL_KOTH_FRIENDLY_TARGET_DESTROYED, target.marker->get_handle());
        }
        else if (human_teams_are_enemies(target.marker->team, info.team))
        {
            multi_game_announcer_play_line(MAL_KOTH_ENEMY_TARGET_DESTROYED, target.marker->get_handle());
        }
    }
    if (target.marker->team != HUMAN_TEAM_CIVILIAN && Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        hud_ui_multiplayer_trigger_objective(target.name, localization_get_display_string_req("HM_DESTROYED", nullptr), multi_game_mode_get_opposite_team(target.marker->team));
        koth_play_target_update(Ga_cue_ids.multiplayer_target_destroyed);
    }
    target.marker->destroyed = true;
    target.destroyer_was_friendly = destroyer_was_friendly;
    remove_target_icon(target_index); // inlined
    add_target_icon(target_index);
}

// 0x009ED5A0 (:294): a destroyed target is repaired and belongs to the repairing team. The server: the repairer gets
// the repair experience and a repair stat (not for a re-repair of a target its own team destroyed), the repairer's
// teammates near the building the assist; the server tells everyone and logs it. The target scores again in 2 s; the
// announcer (not on a dedicated server), the icon, the HUD and the cue.
static void king_of_the_hill_target_repaired(int target_index, human_teams repairing_team)
{
    if (target_index < 0 || target_index >= Num_king_of_the_hill_targets)
    {
        return;
    }
    king_of_the_hill_target& target = King_of_the_hill_targets[target_index];
    if (Multiplayer && Server)
    {
        rfg_mover* building = target_building(target);
        player* repairer = multi_game_last_player_to_repair_rfg_mover(building);
        if (repairer != nullptr &&
            !(human_teams_get_disposition(repairer->current_team, target.marker->team) == TEAM_DISPOSITION_ALLIED && target.destroyer_was_friendly))
        {
            multi_game_xp_trigger_event(repairer->mp_data, MXE_REPAIR_KING_OF_THE_HILL_TARGET, 1, 0xFF, -1, false); // inlined
            vector bmin;
            vector bmax;
            building->get_bbox(bmin, bmax);
            const float assist_dist = multi_game_xp_get_event_parameter(MXE_REPAIR_ASSIST_KING_OF_THE_HILL_TARGET); // inlined
            const float assist_dist_squared = assist_dist * assist_dist;
            const int count = World.get_num_objects<player>();
            for (int i = 0; i < count; i++)
            {
                player* pp = World.get_object<player>(i);
                if (pp != repairer && human_teams_are_allies(repairer->current_team, pp->current_team) &&
                    bbox_find_distance_squared(bmin, bmax, pp->pos) <= assist_dist_squared)
                {
                    multi_game_xp_trigger_event(pp->mp_data, MXE_REPAIR_ASSIST_KING_OF_THE_HILL_TARGET, 1, 0xFF, -1, false);
                }
            }
            multi_player_stats* stats = multi_game_stats_get_player_stats(repairer->mp_data);
            if (stats != nullptr)
            {
                stats->dc_target_repair++;
            }
        }
        unsigned char packet[1200];
        packet[0] = PT_MODE_SPECIFIC;
        unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
        packet[packet_size++] = Koth_repaired;
        packet[packet_size++] = (unsigned char)target_index;
        packet[packet_size++] = (unsigned char)repairing_team;
        multi_game_stats_write_all_team_scores(packet, packet_size);
        packet_set_size(packet, packet_size);
        multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
        // Original: the building is not checked for null.
        koth_log_ownership(building, target_index, repairing_team);
    }
    target.scoring_timer.set(Koth_scoring_interval_ms);
    target.marker->destroyed = false;
    target.marker->team = repairing_team;
    if (!(Multiplayer && Server && Dedicated) && !Spectating)
    {
        local_player_info info;
        player_get_local_player_info(&info);
        if (human_teams_are_allies(target.marker->team, info.team))
        {
            multi_game_announcer_play_line(MAL_KOTH_FRIENDLY_TARGET_REPAIRED, target.marker->get_handle());
        }
        else if (human_teams_are_enemies(target.marker->team, info.team))
        {
            multi_game_announcer_play_line(MAL_KOTH_ENEMY_TARGET_REPAIRED, target.marker->get_handle());
        }
    }
    remove_target_icon(target_index); // inlined
    add_target_icon(target_index);
    hud_ui_multiplayer_trigger_objective(target.name, localization_get_display_string_req("HM_REPAIRED", nullptr), target.marker->team);
    koth_play_target_update(Ga_cue_ids.multiplayer_target_repair);
}

// 0x0094DBA0 (:377): the target's item of the HUD data group (priority, name, percent destroyed, distance, angle,
// team, a flag, the number, 0). LTCG: target_index in ESI.
static void add_king_of_the_hill_target_data_item(int target_index)
{
    if ((unsigned int)target_index >= (unsigned int)Max_king_of_the_hill_targets || Targets_data_group == nullptr || Target_data_items[target_index] != nullptr)
    {
        return;
    }
    static const vint_variant_type target_template[9] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT,
                                                         VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
    vint_data_item* item = Targets_data_group->add_data_item(nullptr);
    Target_data_items[target_index] = item;
    if (item != nullptr)
    {
        item->set_template(target_template, 9);
    }
}

// 0x009944A0 (:429): the King of the Hill markers by priority (from the last) become the targets, named after their
// building (a localized tag, the name, or "!!<target> <n>" when unnamed). Original: the first marker without a
// building ends the list.
void king_of_the_hill_mode_level_init_post()
{
    Num_king_of_the_hill_targets = 0;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        multi_object_marker* marker = Multi_markers_prioritized[i];
        if (marker->get_marker_type() != MMT_KING_OF_THE_HILL_TARGET)
        {
            continue;
        }
        king_of_the_hill_target& target = King_of_the_hill_targets[Num_king_of_the_hill_targets];
        target.marker = marker;
        target.scoring_timer.invalidate();
        target.destroyer_was_friendly = false;
        target.full_control_awarded = true;
        rfg_mover* building = target_building(target);
        if (building == nullptr)
        {
            break;
        }
        // An unnamed object's name is the empty string (rfg.exe compares it with that literal, No_name).
        const char* name = building->get_name();
        if (name == nullptr || name[0] == '\0')
        {
            _snprintf_safe(target.name, 47, "!!%ls %d", localization_get_display_string_req("HM_TARGET_TITLE", nullptr), Num_king_of_the_hill_targets + 1);
        }
        else if (localization_does_tag_exist(building->get_name()))
        {
            wcsncpy(target.name, localization_get_display_string_req(building->get_name(), nullptr), 47);
            target.name[46] = L'\0';
        }
        else
        {
            copy_to_wide(target.name, building->get_name(), 47);
        }
        target.percent_destroyed_accumulator = 0.0f;
        target.percent_repaired_accumulator = 0.0f;
        target.percent_destroyed_last_frame = building->percent_destroyed();
        Num_king_of_the_hill_targets++;
    }
    for (int i = Num_king_of_the_hill_targets; i < Max_king_of_the_hill_targets; i++)
    {
        King_of_the_hill_targets[i].marker = nullptr;
    }
}

// 0x00994630 (:477): the target stats start over
void king_of_the_hill_mode_enter()
{
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFFu;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_player_stats* stats = koth_stats_of(handle);
        if (stats != nullptr)
        {
            stats->dc_target_destruction = 0;
            stats->dc_target_repair = 0;
        }
    }
}

// 0x009EDB50 (:492): per target the server destroys or repairs it by its building's state and gives the full control
// experience to the repairer of an untouched held target; the destruction and repair since the last frame accumulate
// (the server logs them every Logging_snapshot_time_interval_ms). Held targets score; each holding team gets the hold
// experience (doubled with every target); "near the point limit" announcements.
void king_of_the_hill_mode_process()
{
    local_player_info info;
    player_get_local_player_info(&info);
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        king_of_the_hill_target& target = King_of_the_hill_targets[i];
        rfg_mover* building = target_building(target);
        if (Multiplayer && Server && building != nullptr)
        {
            if (!target.marker->destroyed)
            {
                if (building->percent_destroyed() >= Koth_destroyed_percent)
                {
                    king_of_the_hill_target_destroyed(i, false);
                }
                else if (!target.full_control_awarded && building->percent_destroyed() < Koth_full_control_percent)
                {
                    player* repairer = multi_game_last_player_to_repair_rfg_mover(building);
                    if (repairer != nullptr && human_teams_are_allies(repairer->current_team, target.marker->team))
                    {
                        multi_game_xp_trigger_event(repairer->mp_data, MXE_FULL_CONTROL, 1, 0xFF, -1, false);
                        target.full_control_awarded = true;
                    }
                }
            }
            else if (building->percent_destroyed() <= Koth_repaired_percent)
            {
                player* repairer = multi_game_last_player_to_repair_rfg_mover(building);
                if (repairer == nullptr || repairer->current_team == HUMAN_TEAM_NONE)
                {
                    // Original: no accumulation this frame.
                    continue;
                }
                king_of_the_hill_target_repaired(i, repairer->current_team);
                target.full_control_awarded = false;
            }
        }
        // Original: the building is not checked for null here.
        const float delta = building->percent_destroyed() - target.percent_destroyed_last_frame;
        if (delta < 0.0f)
        {
            target.percent_repaired_accumulator -= delta;
        }
        else
        {
            target.percent_destroyed_accumulator += delta;
        }
        target.percent_destroyed_last_frame = building->percent_destroyed();
    }
    if (Multiplayer && Server && Logging_snapshot_timer.elapsed())
    {
        Logging_snapshot_timer.set(Logging_snapshot_time_interval_ms);
        short destruction[Max_king_of_the_hill_targets] = {};
        short repair[Max_king_of_the_hill_targets] = {};
        for (int i = 0; i < Num_king_of_the_hill_targets; i++)
        {
            king_of_the_hill_target& target = King_of_the_hill_targets[i];
            destruction[i] = (short)(int)(target.percent_destroyed_accumulator * 100.0f);
            repair[i] = (short)(int)(target.percent_repaired_accumulator * 100.0f);
            target.percent_destroyed_accumulator = 0.0f;
            target.percent_repaired_accumulator = 0.0f;
        }
        const short seconds_left = (short)(Timer_time.time_until() / 1000);
        log_event_damage_control_destruction_volume(gamelog_get_player_checksum(), seconds_left, destruction[0], repair[0], destruction[1], repair[1], destruction[2], repair[2],
                                                    destruction[3], repair[3], destruction[4], repair[4]);
    }
    // The held targets per team.
    int held[2] = {0, 0};
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        const multi_object_marker* marker = King_of_the_hill_targets[i].marker;
        if (!marker->destroyed && marker->team != HUMAN_TEAM_CIVILIAN)
        {
            held[marker->team]++;
        }
    }
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        king_of_the_hill_target& target = King_of_the_hill_targets[i];
        const multi_object_marker* marker = target.marker;
        if (marker->destroyed || marker->team == HUMAN_TEAM_CIVILIAN || !target.scoring_timer.valid() || !target.scoring_timer.elapsed())
        {
            continue;
        }
        const bool all_held = held[marker->team] == Num_king_of_the_hill_targets;
        const king_of_the_hill_multipliers multiplier = Mp_server_options.mode_options.king_of_the_hill.all_target_multiplier;
        if (all_held || multiplier != KOTHM_ONLY)
        {
            multi_game_stats_alter_team_score(marker->team, all_held && multiplier == KOTHM_NORMAL ? 2 : 1);
            target.scoring_timer.set(Koth_scoring_interval_ms);
        }
    }
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        if (held[team] > 0)
        {
            const int repetitions = held[team] * (held[team] == Num_king_of_the_hill_targets ? 2 : 1);
            multi_game_xp_trigger_event((human_teams)team, MXE_HOLD_KING_OF_THE_HILL_TARGET, repetitions, false); // inlined
        }
    }
    const unsigned short point_limit = Mp_server_options.generic_options.point_limit;
    if (point_limit == 0)
    {
        return;
    }
    for (int team = HUMAN_TEAM_GUERILLA; team <= HUMAN_TEAM_EDF; team++)
    {
        if (point_limit > Mp_team_stats[team].round_score + Koth_win_near_points || point_limit <= Koth_win_near_points)
        {
            continue;
        }
        // The friendly line is 3 lines before the enemy line.
        const bool enemy = human_teams_get_disposition(info.team, (human_teams)team) < TEAM_DISPOSITION_FRIENDLY;
        const multi_announcer_lines line = enemy ? MAL_KOTH_ENEMY_NEAR_WIN : MAL_KOTH_FRIENDLY_NEAR_WIN;
        if (line < NUM_MULTI_ANNOUNCER_LINES && Multi_announcer_lines[line].last_played_time.valid())
        {
            continue;
        }
        multi_game_announcer_play_line(line, 0xFFFFFFFF);
    }
}

// 0x00994710 (:637): the held target nearest to the local player within 22 m gets a pulsing outline in its team's
// color, fading from 12 m to 22 m
void king_of_the_hill_mode_handshake()
{
    local_player_info info;
    player_get_local_player_info(&info);
    king_of_the_hill_target* nearest = nullptr;
    float nearest_dist_squared = FLT_MAX;
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        king_of_the_hill_target& target = King_of_the_hill_targets[i];
        rfg_mover* building = target_building(target);
        if (building == nullptr)
        {
            continue;
        }
        building->disable_object_outline();
        if (target.marker->destroyed || target.marker->team == HUMAN_TEAM_CIVILIAN)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        building->get_bbox(bmin, bmax);
        const float dist_squared = bbox_find_distance_squared(bmin, bmax, *info.pos);
        if (dist_squared < nearest_dist_squared)
        {
            nearest = &target;
            nearest_dist_squared = dist_squared;
        }
    }
    // Magic number: 22 m (squared).
    if (nearest == nullptr || nearest_dist_squared >= 484.0f)
    {
        return;
    }
    const float dist = (float)sqrt(nearest_dist_squared);
    rl_scene* scene = game_render_get_main_scene();
    if (scene == nullptr)
    {
        return;
    }
    color team_color;
    multi_game_render_get_team_color(team_color, MTCT_OUTLINE, nearest->marker);
    float red = (float)(team_color.red / 255.0);
    float green = (float)(team_color.green / 255.0);
    float blue = (float)(team_color.blue / 255.0);
    float alpha = 1.0f;
    // Magic numbers: full strength to 12 m, gone at 22 m.
    if (dist > 12.0f)
    {
        const float fade = ((dist - 12.0f) / 10.0f) * -1.0f + 1.0f;
        red *= fade;
        green *= fade;
        blue *= fade;
        alpha = fade * 1.0f;
    }
    // A 0.5..1.5 pulse with a period of 1000 * pi ms (Frametime_total_milliseconds as unsigned).
    const float pulse = (float)sin((float)((double)(unsigned int)Frametime_total_milliseconds / 500.0)) * 0.5f + 1.0f;
    rl_outline_state state;
    state.colors[0].set(pulse * red, green * pulse, blue * pulse, pulse * alpha);
    state.colors[1].set(0.0f, 0.0f, 0.0f, 1.0f);
    state.layer[0] = RLOL_USE_COLOR0;
    state.layer[1] = RLOL_DISABLED;
    state.layer[2] = RLOL_DISABLED;
    state.layer[3] = RLOL_DISABLED;
    scene->set_outline_state(state);
    rfg_mover* building = target_building(*nearest);
    if (building != nullptr)
    {
        building->enable_object_outline(0);
    }
}

// 0x009EE1F0 (:705): an enemy killed near one of the killer's held targets (the protect distance; the killer or the
// victim): the protect experience
void king_of_the_hill_mode_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info)
{
    if (victim == killer || human_teams_get_disposition(victim, killer) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        const multi_object_marker* marker = King_of_the_hill_targets[i].marker;
        if (marker->destroyed || human_teams_get_disposition(killer->current_team, marker->team) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        const float killer_dist_squared = vec_dist_squared(killer->pos, marker->pos);
        const float victim_dist_squared = vec_dist_squared(victim->pos, marker->pos);
        const float protect_dist = multi_game_xp_get_event_parameter(MXE_PROTECT_KING_OF_THE_HILL_TARGET);
        if (killer_dist_squared < protect_dist * protect_dist || victim_dist_squared < protect_dist * protect_dist)
        {
            multi_game_xp_trigger_event(killer->mp_data, MXE_PROTECT_KING_OF_THE_HILL_TARGET, 1, 0xFF, -1, false); // inlined
        }
    }
}

// 0x00977390 (:726): respawning near an enemy target costs up to 150 (50 m, scaled by the level's distance
// multiplier), near a held one of ours gains up to 15 (40 m)
float king_of_the_hill_mode_adjust_respawn_weight(const player* pp, const player_start* pstart)
{
    float weight = 0.0f;
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        const multi_object_marker* marker = King_of_the_hill_targets[i].marker;
        if (marker->destroyed)
        {
            continue;
        }
        float dist = vec_dist(pstart->pos, marker->pos) / Mp_server_options.get_level()->distance_multiplier;
        if (human_teams_get_disposition(marker->team, pp->current_team) == TEAM_DISPOSITION_HOSTILE)
        {
            dist /= 50.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight = (dist * 150.0f - 150.0f) + weight;
        }
        else if (human_teams_get_disposition(marker->team, pp->current_team) == TEAM_DISPOSITION_ALLIED)
        {
            dist /= 40.0f;
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight = dist * -15.0f + 15.0f + weight;
        }
        else
        {
            // A neutral target adds nothing (rfg.exe still clamps the unscaled distance).
            if (dist - 1.0f >= 0.0f)
            {
                dist = 1.0f;
            }
            weight = dist * 0.0f + 0.0f + weight;
        }
    }
    return weight;
}

// 0x009EE4D0 (:759): PT_MODE_SPECIFIC (packet_data: the subtype byte)
void king_of_the_hill_mode_process_mode_specific(unsigned char* packet_data, const sender& from)
{
    switch (packet_data[0])
    {
    case Koth_destroyed:
        if ((int)packet_data[1] < Num_king_of_the_hill_targets)
        {
            int offset = 3;
            king_of_the_hill_target_destroyed(packet_data[1], packet_data[2] != 0);
            multi_game_stats_read_all_team_scores(packet_data, offset);
        }
        return;
    case Koth_repaired:
        if ((int)packet_data[1] < Num_king_of_the_hill_targets)
        {
            int offset = 3;
            king_of_the_hill_target_repaired(packet_data[1], (human_teams)packet_data[2]);
            multi_game_stats_read_all_team_scores(packet_data, offset);
        }
        return;
    case Koth_migration:
    {
        const unsigned char* p = packet_data + 1;
        for (int i = 0; i < Num_king_of_the_hill_targets; i++, p += 3)
        {
            king_of_the_hill_target& target = King_of_the_hill_targets[i];
            target.marker->destroyed = p[0] != 0;
            target.marker->team = (human_teams)p[1];
            target.destroyer_was_friendly = p[2] != 0;
            if (!target.marker->destroyed && target.marker->team != HUMAN_TEAM_CIVILIAN)
            {
                target.scoring_timer.set(Koth_scoring_interval_ms);
            }
            else
            {
                target.scoring_timer.invalidate();
            }
        }
        return;
    }
    default:
        keen::debug::nativePrintArgs("Unknown King of the Hill event");
        return;
    }
}

// 0x00994AB0 (:822): the targets' HUD data items and icons; the destruction log starts
void king_of_the_hill_mode_round_begin()
{
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        add_king_of_the_hill_target_data_item(i); // inlined
        add_target_icon(i);
    }
    Logging_snapshot_timer.set(Logging_snapshot_time_interval_ms);
}

// 0x0094DC30 (:833)
void king_of_the_hill_mode_round_over(multi_round_winner winner)
{
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        remove_target_icon(i); // inlined
    }
}

// 0x00994B70 (:841): the last man standing's team, else the team with the better round score (a tie has no winner)
multi_round_winner king_of_the_hill_mode_get_round_winner()
{
    multi_round_winner winner;
    player* last = multi_game_mode_get_last_man_standing();
    if (Mp_last_man_standing && last != nullptr)
    {
        winner.team = last->current_team;
        return winner;
    }
    if (Mp_team_stats[HUMAN_TEAM_EDF].round_score > Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.team = HUMAN_TEAM_EDF;
    }
    else if (Mp_team_stats[HUMAN_TEAM_EDF].round_score == Mp_team_stats[HUMAN_TEAM_GUERILLA].round_score)
    {
        winner.player_game_data_handle = 0xFFFFFFFFu;
    }
    else
    {
        winner.team = HUMAN_TEAM_GUERILLA;
    }
    return winner;
}

// 0x009EE600 (:857): server: a player damaging a target of its own team or repairing an enemy one gets a tooltip
void king_of_the_hill_mode_building_damage_changed(player* pp, rfg_mover* building, float structural_mass_delta)
{
    if (pp == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        const multi_object_marker* marker = King_of_the_hill_targets[i].marker;
        if (marker->get_parent() != building->get_handle())
        {
            continue;
        }
        if (marker->destroyed)
        {
            return;
        }
        if (human_teams_get_disposition(pp->current_team, marker->team) == TEAM_DISPOSITION_ALLIED && structural_mass_delta < 0.0f)
        {
            multi_game_trigger_tooltip(pp, checksum_stri("damaged_friendly_target", 0));
            return;
        }
        if (human_teams_get_disposition(pp->current_team, marker->team) == TEAM_DISPOSITION_HOSTILE && structural_mass_delta > 0.0f)
        {
            multi_game_trigger_tooltip(pp, checksum_stri("repaired_enemy_target", 0));
        }
        return;
    }
}

// 0x009BAF40 (:882): the new server sends every target's state and restarts the scoring; full control is awarded again
void king_of_the_hill_mode_server_migration_update_clients()
{
    unsigned char packet[1200];
    packet[0] = PT_MODE_SPECIFIC;
    unsigned short packet_size = packet_header_size(PT_MODE_SPECIFIC);
    packet[packet_size++] = Koth_migration;
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        king_of_the_hill_target& target = King_of_the_hill_targets[i];
        packet[packet_size++] = target.marker->destroyed;
        packet[packet_size++] = (unsigned char)target.marker->team;
        packet[packet_size++] = target.destroyer_was_friendly;
        if (!target.marker->destroyed && target.marker->team != HUMAN_TEAM_CIVILIAN)
        {
            target.scoring_timer.set(Koth_scoring_interval_ms);
        }
        else
        {
            target.scoring_timer.invalidate();
        }
        target.full_control_awarded = false;
    }
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x0094DC70 (:911)
void king_of_the_hill_mode_get_objective_description(char* buffer)
{
    memcpy(buffer, "HM_KING_OF_THE_HILL_OBJECTIVE", sizeof("HM_KING_OF_THE_HILL_OBJECTIVE"));
}

// 0x0094DCB0 (:921): the targets' data group of the HUD document
void king_of_the_hill_mode_hud_doc_init(int doc_handle)
{
    Targets_data_group = vint_data_group::create_new("damage_control_targets");
    if (Targets_data_group != nullptr)
    {
        ui_add_data_group(doc_handle, (int)Targets_data_group->get_handle()); // m_handle, inlined
    }
}

// 0x00977580 (:930): each target's data item: priority, name, percent destroyed, distance (to the building's box, else
// flat to the marker), angle, team (civilian when destroyed), false, the number, 0
void king_of_the_hill_mode_hud_doc_process(int doc_handle)
{
    local_player_info info;
    player_get_local_player_info(&info);
    if (Targets_data_group == nullptr)
    {
        return;
    }
    for (int i = 0; i < Num_king_of_the_hill_targets; i++)
    {
        vint_data_item* item = Target_data_items[i];
        if (item == nullptr)
        {
            continue;
        }
        king_of_the_hill_target& target = King_of_the_hill_targets[i];
        vector to_target = target.marker->pos - *info.pos;
        float dist = to_target.flatten_mag_and_normalize_safe();
        const float angle = vec_delta_ang_flattened(to_target, Rfg_camera.real_orient.fvec);
        float percent = 0.0f;
        rfg_mover* building = target_building(target);
        if (building != nullptr)
        {
            percent = building->percent_destroyed() * 100.0f;
            vector bmin;
            vector bmax;
            building->get_bbox(bmin, bmax);
            dist = (float)sqrt(bbox_find_distance_squared(bmin, bmax, *info.pos));
        }
        const int team = target.marker->destroyed ? (int)HUMAN_TEAM_CIVILIAN : (int)target.marker->team;
        char encoded[141];
        vint_encode_wide_text(encoded, sizeof(encoded), target.name);
        item->set_element(0, vint_variant(target.marker->priority));
        item->set_element(1, vint_variant((const char*)encoded));
        item->set_element(2, vint_variant(percent));
        item->set_element(3, vint_variant(dist));
        item->set_element(4, vint_variant(-angle));
        item->set_element(5, vint_variant(team));
        item->set_element(6, vint_variant(false));
        item->set_element(7, vint_variant(i + 1));
        item->set_element(8, vint_variant(0));
    }
}

// 0x0094DCE0 (:981)
bool king_of_the_hill_mode_hud_doc_should_render(int doc_handle)
{
    return !hud_ui_multiplayer_splash_screen_is_active();
}

// 0x0094DCF0 (:986)
void king_of_the_hill_mode_hud_doc_shutdown(int doc_handle)
{
    Targets_data_group = nullptr;
    for (int i = 0; i < Max_king_of_the_hill_targets; i++)
    {
        Target_data_items[i] = nullptr;
    }
}

// 0x009778E0 (:995): server: targets destroyed and repaired (BE)
void king_of_the_hill_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = koth_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || !Server)
    {
        return;
    }
    write_u32_be(packet_data, packet_size, (unsigned int)stats->dc_target_destruction);
    write_u32_be(packet_data, packet_size, (unsigned int)stats->dc_target_repair);
}

// 0x009779F0 (:1007): client
void king_of_the_hill_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset, unsigned int player_game_data_handle)
{
    multi_player_stats* stats = koth_stats_of(player_game_data_handle);
    if (stats == nullptr || !Multiplayer || Server)
    {
        return;
    }
    stats->dc_target_destruction = (int)read_u32_be(packet_data, packet_offset);
    stats->dc_target_repair = (int)read_u32_be(packet_data, packet_offset);
}

// 0x0094DD20 (:1019)
int king_of_the_hill_mode_get_mode_specific_stat_count()
{
    return 2;
}

// 0x0094DD30 (:1024): the results screen's column names (string ids)
void king_of_the_hill_mode_get_mode_specific_stat_name(char* name, const int max_name_len, int stat_index)
{
    const char* id = stat_index == 0 ? "MENU_TARGETS_DESTROYED" : stat_index == 1 ? "MENU_TARGETS_REPAIRED" : "";
    _snprintf_safe(name, max_name_len, "%s", id);
}

// 0x00977B20 (:1039): the column values. Original: no null check of the stats.
void king_of_the_hill_mode_get_mode_specific_stat_value(char* value, const int max_value_len, int stat_index, unsigned int player_game_data_handle)
{
    const multi_player_stats* stats = koth_stats_of(player_game_data_handle);
    if (stat_index == 0)
    {
        _snprintf_safe(value, max_value_len, "%d", stats->dc_target_destruction);
        return;
    }
    if (stat_index == 1)
    {
        _snprintf_safe(value, max_value_len, "%d", stats->dc_target_repair);
        return;
    }
    _snprintf_safe(value, max_value_len, "%s", "");
}
