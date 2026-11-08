// Original: volition/rfg/code/objects/humans/player.cpp
//
// Partial (player_minimal.md P4): the player object (class registration, construction, serialization, streaming,
// freeing), the per-frame player processing (player_do_frame, player::do_frame and its parts), the zoom, the jetpack,
// equipping, the saved inventory, armor props, movement speeds and the level init/shutdown; since multiplayer.md MP8
// the firearm triggers, invulnerability, combat targets, the usable items and weapon pickups and swaps. The other
// usable objects (vehicles, turrets, ladders, ammo boxes, mission starts), squads, cover and respawning stay under
// #if 0 at the end of the file (test-side stand-ins).
// The external interface is declared in player.h.

#include "volition/rfg/code/objects/humans/player.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/localgamesession.hpp"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/localization/localization.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/objects/humans/player_hold_object.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_bftp_node.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/objects/object_upgrade_node.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlogger_lib/include/logger.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

// ---- globals and file statics
// const float NORMAL_DEATH_SOUND_MAX_DIST (0x01310FF4) is used here too; placed in human_spinebend.cpp.
static const float SILENT_DEATH_TREAT_AS_NORMAL_DIST_SQ = 9.0f; // 0x01310FFC
static const char Comm_tower_enter_sound_name[18] = "HUD_COMM_RANGE_IN"; // 0x01311034
static const char Comm_tower_leave_sound_name[19] = "HUD_COMM_RANGE_OUT"; // 0x01311048
static const char Player_squad_definition_name[18] = "player_ally_squad"; // 0x0131105C
// 0x01311088, read from rfg.exe: the sledgehammer upgrades and the weapon each one gives (the golden hammer unlocks
// the golden hammer and levels 3-14). Defined here (player_meta_data.cpp in the original) because player.cpp is the
// first user in the unity TU.
static const hammer_upgrade_data Hammer_upgrade_data[16] = {
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL, "sledgehammer", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL1, "sledge_upgrade_1", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL2, "sledge_upgrade_2", false},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD, "golden_hammer", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL3, "sledge_upgrade_3", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL4, "sledge_upgrade_4", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL5, "sledge_upgrade_5", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL6, "sledge_upgrade_6", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL7, "sledge_upgrade_7", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL8, "sledge_upgrade_8", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL9, "sledge_upgrade_9", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL10, "sledge_upgrade_10", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL11, "sledge_upgrade_11", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL12, "sledge_upgrade_12", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL13, "sledge_upgrade_13", true},
    {UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL14, "sledge_upgrade_14", true},
};
// bool (* const Inv_use_scripts[1])() (0x01311148) is used here too; placed in player_meta_data.cpp.
// const float COVER_NODE_MAX_NAVMESH_ATTACH_DIST (0x01311864) is used here too; placed in human_spinebend.cpp.
static const float VEHICLE_ADOPTION_RANGE_SQRD = 16.0f; // 0x01311AF0
// const float NEAR_FOLLOW_DEST_DIST_SQ (0x01311B1C) is used here too; placed in human_spinebend.cpp.
static const float MIN_UPDATE_TAG_POS_DIST_SQ = 0.0001f; // 0x01311C74
// const float min_correction_diff (0x01315520) is used here too; placed in human_spinebend.cpp.
static const float FAR_FROM_PLAYER_DIST_SQRD = 10000.0f; // 0x01316B78
bool Player_instant_unequip = true; // 0x01694314
bool Allow_player_flying_equips = true; // 0x01694315
bool Player_recruit_enabled = true; // 0x01694422
bool Player_dismiss_enabled = true; // 0x01694423
unsigned int Player_mission_radio_last_node = 0xFFFFFFFF; // 0x01694450
voice_line_handle Player_mission_radio_message_rfc = INVALID_VOICE_LINE_HANDLE; // 0x01694454
voice_line_handle Player_mission_radio_message_tech = INVALID_VOICE_LINE_HANDLE; // 0x01694458
static unsigned int Jetpack_effect_info = 0xFFFFFFFF; // 0x0169445C
static int Jetpack_loop_sound_fade_time = 200; // 0x01694460
static float Player_blood_decal_fade_rate = 0.005f; // 0x01694464
static float Player_max_sprint_speed = 8.0f; // 0x01694468
static float Mp_player_acceleration = 6.25f; // 0x0169446C
static float Player_move_multiplier_zoom = 0.35f; // 0x01694470
static float Mp_player_move_multiplier_zoom = 0.4f; // 0x01694474
static float Player_move_multiplier_crouch = 0.5f; // 0x01694478
static float Mp_player_move_multiplier_crouch = 0.55f; // 0x0169447C
static float Jetpack_use_rate = 0.35f; // 0x01694480
static float Jetpack_acceleration = 18.0f; // 0x01694484
static int Player_usable_object_refresh_rate = 100; // 0x0169448C
static int s_lastBackpackIndex = -1; // 0x01694490
timestamp Player_mission_radio_timer; // 0x01695614
static vector PLAYER_OPEN_AMMO_BOX_OFFSET; // 0x01695618
static timestamp Liberated_radio_timestamp; // 0x01695628
static timestamp Blow_undercover_limiter; // 0x0169562C
static float Player_acceleration = 10.0f; // 0x01695630
// farray<object*,16384> Object_update_render_list (0x0169D028) is used here too; placed in object.cpp.
// The upgrade unlock state (s_backpacks_unlocked and the others) is placed in player_meta_data.cpp.
// inventory_item* Inv_item_free_list (0x0302366C) is used here too; placed in human.cpp.
// No initial value: tweak_table.xtbl sets it. Unregistered it stays 0 and player_get_max_movement_speeds scales a
// multiplayer player's speed by the camera's forward dot alone: no strafing, no walking backward.
static float Mp_player_move_multiplier_side; // 0x0302367C, from tweak_table.xtbl
static tweak_table_item Tweak_table_item_Mp_player_move_multiplier_side("Mp_player_move_multiplier_side", &Mp_player_move_multiplier_side); // 0x030664E4
static int Comm_tower_enter_sound; // 0x03023870
player* Player; // 0x03023874
// float Usable_object_facing_dot_prod_min (0x030266A8) is used here too; placed in object.cpp.
static int Comm_tower_leave_sound; // 0x03029B58
bool Wants_to_start_game_with_golden_hammer; // 0x0302F044
bool Look_at_player; // 0x0302F045
bool Player_instant_weapon_change; // 0x0302F048
bool Player_died; // 0x0302F0A3
static bool Custom_player_bone_transform; // 0x0302F0AC
static bool Player_never_die; // 0x0302F0AD
static bool player_is_near_rs_node; // 0x035B59CC
static bool player_is_near_ad_node; // 0x035B5A14
static bool Local_player_in_protected_vehicle_postion; // 0x0302F0AF
static district* Player_district; // 0x0302F0B0
float Player_max_movement_speed_override; // 0x0302F0B4 (tweak table item)
float Mp_player_max_movement_speed_override; // 0x0302F0B8 (tweak table item)
static bool Ammo_box_unlimited_use; // 0x0302F0BC
static bool Player_weapon_autoswap_enable; // 0x0302F0BD
bool using_high_resolution_weapons; // 0x0302F154
// int Num_inv_items (0x0302F0E8) is used here too; placed in human.cpp.
static object_pool<player, 16, 1> playerpool; // 0x0306F4A0
static checksum_stri UPGRADES_TOOL_TIP_NAME_CHECKSUM("upgrades", 0); // 0x033B1F7C
// read from rfg.exe: pool usage 0x2E590 bytes, 16 players
static obj_class_instantiator player_instantiator("player", alloc_player, types_player, bpo_index_player, classname_player, player::pool_remaining, 0x2E590, 16); // 0x0355083C

// ---- internal functions
static human_team_dispositions human_teams_get_disposition_internal(human_teams team1, human_teams team2); // 0x00A0D270, human_teams.cpp, later in the unity TU
static const human_info* player_local_get_human_info_for_creation(); // 0x00A33CC0 (:2611)
static void player_check_if_stuck(player* pp); // 0x00A5C5F0 (:2652)
static void play_district_liberated_radio_line(); // 0x00A10430 (:2704)
static void local_player_is_killed(player* pp); // 0x00A34620 (:7633)

// ---- class registration (:2734)
// 0x00A057A0 (.h:329)
const char* player::get_classname_static()
{
    return "player";
}

// 0x00943020 (.h:329)
int player::get_bpo_index_static()
{
    return 0;
}

// 0x00ADEE80 (.h:329)
const char* player::get_classname() const
{
    return "player";
}

// 0x00A104A0
void types_player(signed char& type, signed char& subtype)
{
    type = OT_HUMAN;
    subtype = OT_SUB_HUMAN_PLAYER;
}

// 0x00A104B0
int bpo_index_player()
{
    return 0;
}

// 0x00A104C0
const char* classname_player()
{
    return "player";
}

// 0x00A104D0
unsigned int player::get_classname_hash()
{
    return player_instantiator.this_hash;
}

// 0x00A33DB0
int player::pool_remaining()
{
    return playerpool.get_free();
}

// 0x00A5C780
object* alloc_player()
{
    return playerpool.alloc(0);
}

// 0x00A842E0
void player::pool_free()
{
    playerpool.free(this);
}

// ---- internal functions

// 0x00A5B5C0 (:1287)
// The ladder a player walking (1 m/s or faster, not sprinting, out of cover, no equip or ladder-blocking action) runs
// into: the first ladder box within 0.2 m (0.1 m in the air) of a point 6 cm ahead, enabled and not blocked by a
// standing npc at its foot, that the player steers towards (within 45 degrees) and along (within 30 degrees of its
// axis), not from behind it below its top, and that the camera faces. A local multiplayer player waits out its mount
// delay.
ladder* player_is_near_ladder(const player* pp)
{
    if (pp->hflags.is_on_ladder || pp->controls_on_foot_allowed == 0)
    {
        return nullptr;
    }
    if (Multiplayer && pp == Player && !pp->mp_ladder_state.local_ladder_mount_delay.elapsed())
    {
        return nullptr;
    }
    if (pp->hflags.playing_equip_anim || pp->hflags.playing_unequip_anim)
    {
        return nullptr;
    }
    if (pp->pflags.do_sprint)
    {
        return nullptr;
    }
    if (pp->direct_control_vel.mag() < 1.0f)
    {
        return nullptr;
    }
    if (pp->cur_state == HS_COVER)
    {
        return nullptr;
    }
    v_animate* vp = pp->char_inst->vp;
    if (vanim_is_playing_action(vp, ANIM_ALIAS_LADDER_ACTION) || vanim_is_playing_action(vp, ANIM_ALIAS_COMBAT_MOVE) || vanim_is_playing_action(vp, ANIM_ALIAS_FLINCH) ||
        vanim_is_playing_action(vp, ANIM_ALIAS_THROW) || vanim_is_playing_action(vp, ANIM_ALIAS_DIVE))
    {
        return nullptr;
    }
    // Magic numbers: the search radius on the ground and in the air, 6 cm ahead.
    const float radius = pp->ground_obj_handle == 0xFFFFFFFF ? 0.1f : 0.2f;
    const vector ahead(pp->pos.x + pp->orient.fvec.x * 0.06f, pp->orient.fvec.y * 0.06f + pp->pos.y, pp->orient.fvec.z * 0.06f + pp->pos.z);
    unsigned int ladder_handle;
    if (World.collect_objects<ladder>(ahead, radius, &ladder_handle, 1, nullptr, nullptr) == 0)
    {
        return nullptr;
    }
    ladder* lp = World.object_from_handle<ladder>(ladder_handle);
    // Magic number: an npc within 0.5 m of the ladder's foot that is not a ragdoll blocks it.
    unsigned int npc_handle;
    if (World.collect_objects<npc>(lp->pos, 0.5f, &npc_handle, 1, nullptr, nullptr) != 0 && World.object_from_handle<npc>(npc_handle)->ragdoll_idx == -1)
    {
        return nullptr;
    }
    if (lp == nullptr || !lp->is_enabled())
    {
        return nullptr;
    }
    const vector to_ladder = (lp->pos - pp->pos).copy_flatten_normalize();
    // Magic numbers: cos 45 and cos 30 degrees.
    if (pp->steering_vector * to_ladder < 0.70710677f)
    {
        return nullptr;
    }
    if (fabsf(pp->steering_vector * lp->orient.fvec) < 0.8660254f)
    {
        return nullptr;
    }
    if (to_ladder * lp->orient.fvec < 0.0f)
    {
        // From behind: only near its top (the top less than 1.9 m above the feet).
        vector bmin;
        vector bmax;
        lp->get_bbox(bmin, bmax);
        if (pp->pos.y + 1.9f < bmax.y)
        {
            return nullptr;
        }
    }
    const matrix* camera_orient = &Rfg_camera.real_orient;
    if (Multiplayer && !object_is_local_player(pp))
    {
        camera_orient = &pp->mp_camera_orient;
    }
    const vector flat_to_ladder(lp->pos.x - pp->pos.x, 0.0f, lp->pos.z - pp->pos.z);
    if (camera_orient->fvec.copy_flatten_normalize() * flat_to_ladder < 0.0f)
    {
        return nullptr;
    }
    return lp;
}

// 0x00A33CC0 (:2611)
// The local player's character: the DLC one when it is loaded (single player), or the multiplayer character of the
// player's team (civilians without teams) and selection.
static const human_info* player_local_get_human_info_for_creation()
{
    if (!Multiplayer)
    {
        const human_info* hi = human_info_find_internal("rfg_PC_DLC");
        if (hi == nullptr)
        {
            hi = human_info_find_internal("rfg_PC");
        }
        return hi;
    }
    const unsigned int handle = multi_game_data_get_my_handle(NUM_SESSION_TYPES);
    const multi_game_data* data = multi_game_data_get(handle, false, false);
    if (multi_game_mode_has_teams())
    {
        return multi_game_charcter_get_human_info(data->team, data->player_selection_preference);
    }
    return multi_game_charcter_get_human_info(HUMAN_TEAM_CIVILIAN, data->player_selection_preference);
}

// 0x00A5C5F0 (:2652). LTCG: pp in ESI.
// Every 10 s: a driver whose vehicle hardly moves (1 m/s) and who stayed within 1 m is stuck (checked again after
// 1 s).
static void player_check_if_stuck(player* pp)
{
    if (!pp->is_stuck_timer.valid())
    {
        pp->is_stuck_timer.set(10000);
        pp->pflags.is_stuck = 0;
        pp->last_stuck_pos = pp->pos;
        return;
    }
    if (!pp->is_stuck_timer.elapsed())
    {
        return;
    }
    pp->pflags.is_stuck = 0;
    const vector last_pos = pp->last_stuck_pos;
    pp->last_stuck_pos = pp->pos;
    // Magic numbers: checked every 10 s.
    pp->is_stuck_timer.set(10000);
    if (human_in_vehicle(pp, nullptr) && pp->cur_state == HS_VEHICLE_DRIVING && pp->vehicle_handle != 0xFFFFFFFF)
    {
        vehicle* vp = (vehicle*)World.object_from_handle(pp->vehicle_handle);
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE)
        {
            vector vel;
            vehicle_get_velocity(vp, vel);
            if ((double)vel.x * vel.x + (double)vel.y * vel.y + (double)vel.z * vel.z <= 1.0 && vec_dist_squared(pp->pos, last_pos) < 1.0f)
            {
                pp->pflags.is_stuck = 1;
                pp->is_stuck_timer.set(1000);
            }
        }
    }
}

// 0x00A10430 (:2704)
// The district liberated radio line, 2 s after it becomes ready.
static void play_district_liberated_radio_line()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    if (!Liberated_radio_timestamp.valid())
    {
        if (object_district_radio_line_ready())
        {
            Liberated_radio_timestamp.set(2000);
        }
    }
    else if (Liberated_radio_timestamp.elapsed() && object_district_play_liberated_line())
    {
        Liberated_radio_timestamp.invalidate();
    }
}

// ---- player

// 0x00ADECA0 (:2735)
// The members' constructors set the timestamps invalid (-1), the blood decals empty and the meta data reset.
player::player()
{
}

// 0x00ADEE90
player::~player()
{
}

// 0x00ADEEF0 (:2739)
// Load: the multiplayer handle, then the human; the player state is reset (multiplayer bookkeeping, timers, cover,
// tracking records, custom vehicles, material effects, meta data upgrades), the respawn point is made unique, the
// player stands directly controlled on the ground and equips its default weapons.
bool player::serialize(obj_prop_block& pblock, bool load)
{
    if (load)
    {
        mp_data = 0xFFFFFFFF;
        pblock.get<unsigned int>(MULTI_PLAYER_HANDLE, mp_data);
    }
    if (!human::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    backpack = nullptr;
    mp_respawn_backpack_type = -1;
    if (!Multiplayer)
    {
        mp_flags.is_multiplayer = 0;
    }
    else
    {
        mp_flags.is_multiplayer = 1;
        mp_sim_state.reset();
    }
    mp_flags.jump_id = 0;
    mp_current_command.action = false;
    mp_current_command.command_id = 0;
    mp_simulation_time_accumulated = 0.0f;
    mp_simulation_time_used = 0.0f;
    mp_simulation_time_extrapolated = 0.0f;
    mp_desired_weapon_slot = 0;
    mp_disarm_bomb_handle = 0xFFFFFFFF;
    mp_last_respawn_time = 0;
    mp_last_spawn_point_handle = 0xFFFFFFFF;
    mp_ragdoll_force_timeout.invalidate();
    mp_ragdoll_force.zero();
    mp_ragdoll_pos.zero();
    mp_edge_correction_info.increasing_velocity_count = 0;
    mp_edge_correction_info.vertical_velocity = 0.0f;
    mp_blocked_info.raycast_blocked_count = 0;
    mp_ragdoll_linear_velocity_aggregate.zero();
    mp_ragdoll_angular_velocity_aggregate.zero();
    memset(mp_player_damage_history, 0, sizeof(mp_player_damage_history));
    mp_ladder_state.remote_ladder_mount_delay.set(0);
    mp_ladder_state.local_ladder_mount_delay.set(0);
    pflags.quick_turn = 0;
    jetpack_use_log_handle = 0;
    backpack_equip_log_handle = 0;
    district_log_handle = 0;
    quick_turn_speed = -1.0f;
    set_equipped_inv_item(nullptr);
    radio_id = 0;
    tag_trigger = -1;
    tag_seq = TAGGING_SEQUENCE_NONE;
    hud_message = INVALID_HUD_MESSAGE_HANDLE;
    dead_timestamp.invalidate();
    frametime_multiplier = 1.0f;
    aim_target = 0xFFFFFFFF;
    aim_target_duration.set(0);
    heat_seeking_target = 0xFFFFFFFF;
    combat_target_update_time.set(0);
    comm_tower_check_period.set(0);
    ammo_box_handle = 0xFFFFFFFF;
    pathfind_time_out.invalidate();
    footstep_propagate_time.set(0);
    facial_idle.set(0);
    bump_into_propagate_time.set(0);
    exiting_cover.set(0);
    cover_dive_capsule.set(0);
    zoom_cancel_timestamp.invalidate();
    combat_move_queue = -1;
    cover_approaching_edge_ts.set(0);
    entering_cover.set(0);
    obj_flags.special_lifetime = 1;
    mp_action_object_handle = 0xFFFFFFFF;
    script_mode = PSM_NONE;
    for (int i = 0; i < 8; i++)
    {
        positional_records[i].pos = pos;
        // Magic number: 250 ms per tracking record.
        positional_records[i].period_ms = 250;
    }
    next_record = 0;
    tracking_period.set(0);
    direct_control_dir.x = 0.0f;
    direct_control_dir.y = 1.0f;
    set_team(h_info->default_team);
    hold_object_available = false;
    hold_object_handle = 0xFFFFFFFF;
    abandoned_vehicles[0] = 0;
    abandoned_vehicles[1] = 0;
    abandoned_vehicles[2] = 0;
    num_abandoned_vehicles = 0;
    memset(custom_vehicles, 0, sizeof(custom_vehicles));
    custom_vehicle_list = nullptr;
    custom_vehicle_free_list = nullptr;
    num_custom_vehicles = 0;
    for (int i = 0; i < 50; i++)
    {
        custom_vehicle* cvp = &custom_vehicles[i];
        if (custom_vehicle_free_list == nullptr)
        {
            cvp->next = cvp;
            cvp->prev = cvp;
            custom_vehicle_free_list = cvp;
        }
        else
        {
            cvp->prev = custom_vehicle_free_list->prev;
            cvp->next = custom_vehicle_free_list;
            custom_vehicle_free_list->prev->next = cvp;
            custom_vehicle_free_list->prev = cvp;
        }
    }
    for (int i = 0; i < 16; i++)
    {
        mat_fx_handle[i] = 0xFFFFFFFF;
    }
    current_mat_fx = 0xFFFFFFFF;
    blood_decals_fade_index = 0;
    non_inventory_item_handle = 0xFFFFFFFF;
    last_vehicle_driven = 0xFFFFFFFF;
    // the meta data statistics and upgrades (player_meta_data::reset, inlined)
    meta_data.reset();
    radiation_timestamp.invalidate();
    radiation_damage = 0.0f;
    radiation_foley = -1;
    ragdoll_override_get_up_time = -1;
    fade_backpack_time.invalidate();
    vector respawn_pos = pos;
    human_validate_respawn_point(respawn_pos);
    sprint_delay_timestamp.set(0);
    hflags.casts_shadows = 1;
    hflags.casts_drop_shadows = 1;
    hflags.xray_visible = 0;
    turnto_target = respawn_pos;
    memset(&pflags, 0, sizeof(pflags));
    pflags.never_die = Player_never_die;
    pflags.is_loaded = 1;
    mp_camera_pos = respawn_pos;
    mp_camera_orient = orient;
    jetpack_fuel_percent = 1.0f;
    jetpack_foley = -1;
    jetpack_effect = 0xFFFFFFFF;
    mp_camera_angular_velocity = ZERO_VECTOR2;
    cover_root_offset = 0.0f;
    spinebend_root_offset = 0.0f;
    jump_refresh_timestamp.set(0);
    if (Multiplayer)
    {
        int team = 0;
        pblock.get<int>(MULTI_TEAM, team);
        set_team((human_teams)team);
        if (Multiplayer)
        {
            const multi_game_data* data = multi_game_data_get(mp_data, false, false);
            const voice_persona_list* personas = nullptr;
            if (data != nullptr)
            {
                personas = multi_game_charcter_get_voice_persona_list(multi_game_mode_has_teams() ? data->team : HUMAN_TEAM_CIVILIAN, data->player_selection_preference);
            }
            if (personas == nullptr)
            {
                personas = &human_info_find_internal("rfg_PC")->voice_personas;
            }
            if (personas != nullptr && personas->voice_list.size() > 0)
            {
                farray<human_voice_persona*, 64> candidates;
                for (int i = 0; i < personas->voice_list.size(); i++)
                {
                    candidates.add(personas->voice_list[i]);
                }
                human_voice_persona* persona = ((voice_persona_list*)personas)->pick_voice_generic(candidates);
                if (persona != nullptr)
                {
                    voice = persona;
                }
            }
        }
        if (voice == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    if (Multiplayer == true && multi_is_server())
    {
        remote_obj_flags.remote_object = 1;
        multi_core_object_create_remote_object(2, get_handle());
    }
    // human_set_state(this, HS_STANDARD, HMS_IDLE), inlined
    move_speed = -1.0f;
    cur_state = HS_STANDARD;
    cur_movement_state = HMS_IDLE;
    if ((get_sub_type() != OT_SUB_HUMAN_NPC || is_killed() || cur_state != HS_COVER) && ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
        ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        human_set_movement_mode(this, HMM_DIRECT, HMSM_NONE); // inlined
    }
    human_stick_to_ground(this);
    const inv_item_info* default_equip;
    if (Multiplayer == true)
    {
        default_equip = multi_game_player_get_default_weapon(mp_data);
    }
    else
    {
        default_equip = h_info->get_default_equip_item(); // the field (single player)
    }
    inventory_item* equip = human_has_item(this, default_equip);
    if (equip == nullptr && default_equip != nullptr && default_equip->w_info != nullptr)
    {
        equip = human_has_weapon_of_type(this, default_equip->w_info->weapon_class);
    }
    else if (equip == nullptr)
    {
        // Original: without a default equip item the first inventory item is equipped.
        equip = inventory;
    }
    if (equip != nullptr)
    {
        player_equip_item(this, equip, nullptr, false);
        hflags.do_instant_equip = 1;
    }
    const inv_item_info* default_last = Multiplayer ? multi_game_classes_get_default_class()->default_last_equip_item : h_info->get_default_last_equip_item();
    inventory_item* last = human_has_item(this, default_last);
    if (last == nullptr && default_last != nullptr && default_last->w_info != nullptr)
    {
        last = human_has_weapon_of_type(this, default_last->w_info->weapon_class);
    }
    if (last != nullptr)
    {
        last_equipped_weapon = last;
    }
    ally_squad_handle = 0xFFFFFFFF;
    escort_squad_handle = 0xFFFFFFFF;
    check_squads_timer.set(0);
    memset(activity_inventory_buffer, 0, sizeof(activity_inventory_buffer));
    hflags.invulnerable = 0;
    activity_inventory_save_file = nullptr;
    importance_level = OBJECT_IMPORTANCE_LEVEL_MEDIUM;
    pathfind_info.last_visited_veh_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    pathfind_info.last_visited_veh_nav_cell_update_timer.invalidate();
    action_node_data.action_node_set.clear();
    action_node_data.action_state = 0;
    action_node_data.action_sub_state = 0;
    action_node_data.exit_quick = true;
    action_node_data.current_action_node = 0xFFFFFFFF;
    action_node_data.state_duration.invalidate();
    human_stick_to_ground(this);
    return true;
}

// 0x00A10520 (:3050)
obj_serialize_mode player::serialize_mode()
{
    return OSM_NONE;
}

// 0x00AC2070 (:3076)
// Streaming in: the backpack the upgrades give (outside DLC), the armor props and the campaign start log; streaming
// out stops the material effects and the jetpack.
bool player::stream(bool load)
{
    if (!load)
    {
        player_stop_material_effect(this);
    }
    if (!human::stream(load))
    {
        return false;
    }
    if (!load)
    {
        player_update_jetpack(this, false);
        return true;
    }
    amb_prop_info = nullptr;
    if (Multiplayer || wrecking_crew_is_active())
    {
        return true;
    }
    const upgrade_definition_names type = upgrade_get_backpack_type();
    if (type != NUM_OF_UPGRADE_DEFINITIONS)
    {
        int index = -1;
        if (type == UPGRADE_DEFINITION_JETPACK)
        {
            index = 0;
        }
        else if ((int)type >= UPGRADE_DEFINITION_BACKPACK_THRUST)
        {
            // Original: the backpack index counts from the last sledgehammer upgrade (thrust = 1).
            index = type - UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL14;
        }
        if (index >= 0 && World.dlc_bundle_id == -1)
        {
            multi_object_backpack* bp = multi_game_backpack_create(index, pos, orient);
            bp->event_player_picked_up(this, true, false);
            s_lastBackpackIndex = index;
        }
    }
    const int armor = upgrade_get_armor_level();
    if (armor > 0)
    {
        player_create_armor_prop(armor, this);
    }
    if (gamelog_has_campaign_start_been_logged())
    {
        return true;
    }
    // Magic number: the gold tier flag is always set.
    const char gold_tier = 1;
    const vector log_pos = gamelog_get_human_pos(this);
    const char difficulty = gamelog_get_difficulty();
    const int title_id = gamelog_get_titleid();
    const int language = gamelog_get_language();
    const int locale_id = gamelog_get_locale();
    const char pct_game_complete = gamelog_get_pct_complete();
    const int time_played = gamelog_get_time_played();
    const int time = gamelog_get_logging_time();
    const int player_checksum = gamelog_get_player_checksum();
    log_event_campaign(player_checksum, time, time_played, pct_game_complete, locale_id, language, title_id, difficulty, log_pos, gold_tier);
    gamelog_campaign_start_logged(true);
    return true;
}

// 0x00ACD440 (:3137)
// The player's squads go with it (their minimap icons first), and the backpack equip log ends.
void player::free_this()
{
    if (Multiplayer)
    {
        multi_core_object_process_object_free(this);
    }
    else
    {
        const unsigned int squads[2] = {ally_squad_handle, escort_squad_handle};
        for (int s = 0; s < 2; s++)
        {
            object_squad* squad = (object_squad*)World.object_from_handle(squads[s]);
            if (squads[s] != 0xFFFFFFFF && squad != nullptr && squad->get_obj_type() == OT_SQUAD)
            {
                for (int i = 0; i < squad->member_array.size(); i++)
                {
                    Cartographer.remove_icon(squad->member_array[i]);
                }
                World.destroy_object(squads[s], false);
            }
        }
    }
    const unsigned int event_handle = backpack_equip_log_handle;
    pflags.custom_mesh = 0;
    if (event_handle != 0)
    {
        // Magic number: 0x1D, the backpack equipped log event.
        if (log_record_event(0x1D))
        {
            log_stop_continuous_event(event_handle);
        }
        backpack_equip_log_handle = 0;
    }
    human::free_this();
}

// 0x00A10550 (:3240): how long the target has been aimed at. Magic numbers: 0.5 s and 2 s.
weapon_lock_on_status player::heat_seeking_target_locking_status()
{
    if (aim_target_duration.time_since() > 2000)
    {
        return WLOS_LOCKED;
    }
    return (weapon_lock_on_status)(aim_target_duration.time_since() > 500);
}

// 0x00ACD560 (:3252)
void player::ammo_box_reset()
{
    ammo_box_handle = 0xFFFFFFFF;
    move_speed = -1.0f;
    cur_state = HS_STANDARD;
    unhide_weapon();
}

// 0x00A84420 (:3260)
// Hides the equipped weapon (and the held offhand projectile).
void player::hide_weapon()
{
    if (pflags.weapon_hidden)
    {
        return;
    }
    weapon* wp = get_equipped_weapon();
    if (wp != nullptr)
    {
        wp->hold_item(this, false);
        if (offhand_projectile_handle != 0xFFFFFFFF)
        {
            item* offhand = (item*)World.object_from_handle(offhand_projectile_handle);
            if (offhand != nullptr && offhand->get_obj_type() == OT_ITEM)
            {
                offhand->hold_item(this, false);
            }
        }
    }
    pflags.weapon_hidden = 1;
}

// 0x00AC21B0 (:3286)
void player::unhide_weapon()
{
    if (!pflags.weapon_hidden)
    {
        return;
    }
    weapon* wp = get_equipped_weapon();
    if (wp == nullptr)
    {
        human_equip_inventory_item(this, inventory);
    }
    else
    {
        wp->hold_item(this, true);
        if (offhand_projectile_handle != 0xFFFFFFFF)
        {
            item* offhand = (item*)World.object_from_handle(offhand_projectile_handle);
            if (offhand != nullptr && offhand->get_obj_type() == OT_ITEM)
            {
                offhand->hold_item(this, true);
            }
        }
    }
    pflags.weapon_hidden = 0;
}

// 0x00AC2230 (:3313)
void player::action_node_reset(object_action_node* anp)
{
    amb_prop_detach_props(this, true);
    // Original: the action node data is cleared twice.
    for (int pass = 0; pass < 2; pass++)
    {
        action_node_data.action_node_set.clear();
        action_node_data.action_state = 0;
        action_node_data.action_sub_state = 0;
        action_node_data.exit_quick = true;
        action_node_data.current_action_node = 0xFFFFFFFF;
        action_node_data.state_duration.invalidate();
        if (pass == 0)
        {
            pflags.action_node_anim_started = 0;
            hflags.override_default_anim_state = 0;
        }
    }
    if ((anp == nullptr || !anp->base_node_info->node_flags.weapon_cabinet) && cur_state == HS_ACTION_NODE)
    {
        human_equip_inventory_item(this, last_equipped_weapon);
        move_speed = -1.0f;
        cur_state = HS_STANDARD;
    }
    pflags.weapon_hidden = 0;
}

// 0x00A10590 (:3336)
// Pathfinding the player (scripted) times out once the movement stick is used: back to direct control.
void player::do_frame_path_timeout()
{
    if (movement_mode != HMM_PATHFINDING || cur_state == HS_VEHICLE_ENTERING || !pathfind_time_out.valid() || !pathfind_time_out.elapsed())
    {
        return;
    }
    const float forward = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
    const float turn = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
    // Magic number: a stick deflection above 0.01.
    if (0.01 < fabsf(forward) || 0.01 < fabsf(turn))
    {
        move_speed = -1.0f;
        cur_state = HS_STANDARD;
        cur_movement_state = HMS_IDLE;
        if ((get_sub_type() != OT_SUB_HUMAN_NPC || is_killed() || cur_state != HS_COVER) && ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
            ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            human_set_movement_mode(this, HMM_DIRECT, HMSM_NONE); // inlined
        }
    }
}

// 0x00AD59E0 (:3362)
// Radiation damage once a second without the radiation shield.
void player::do_frame_radiation()
{
    if (upgrade_has_radiation_shield() || !pflags.in_radiation || radiation_damage <= 0.0f)
    {
        return;
    }
    if (is_killed())
    {
        player_radiation_stop(this);
    }
    if (radiation_timestamp.valid() && radiation_timestamp.elapsed())
    {
        human_damage(this, nullptr, radiation_damage, DBT_RADIATION, nullptr, nullptr, true, false);
        radiation_timestamp.set(1000);
    }
}

// 0x00A5C7D0 (:3381)
// The blood decals on the player fade out (all at once when invulnerable, or for a guerrilla tech, RFC or old coot
// character outside missions).
void player::do_frame_blood_decals()
{
    bool remove_all = false;
    if (hflags.invulnerable ||
        (!Multiplayer && !wrecking_crew_is_active() && (h_info->flags.guerrilla_tech || h_info->flags.guerrilla_rfc || h_info->flags.old_coot) && !mission_is_active()))
    {
        blood_decals_fade_index = 0;
        remove_all = true;
    }
    // Deviation (frame rate independence): rfg.exe subtracts Player_blood_decal_fade_rate every frame, so the fade lasts
    // 200 frames (3.3 s at 60 fps, under 1 s at 240 Hz, before the bloodsplat shader's reveal completes). Here the rate
    // is per 1/60 s of frame time. Magic number: 60, the frame rate the original rate gives 3.3 s at.
    const float fade_step = Player_blood_decal_fade_rate * Frametime * 60.0f;
    for (int i = blood_decals.size() - 1; blood_decals_fade_index <= i; i--)
    {
        blood_decals[i].opacity = blood_decals[i].opacity - fade_step;
        const float opacity = blood_decals[i].opacity;
        if (opacity <= 0.0f || remove_all)
        {
            blood_decals.remove(); // inlined
        }
        else
        {
            rl_color_float color;
            color.red = 1.0f;
            color.green = 1.0f;
            color.blue = 1.0f;
            color.alpha = opacity;
            rl_decal_update_color(blood_decals[i].handle, color, 0xFFFFFFFF);
        }
    }
}

// 0x00A5C920 (:3405)
void player::remove_blood_decals()
{
    for (int i = blood_decals.size() - 1; i >= 0; i--)
    {
        blood_decals.remove(); // inlined
    }
    blood_decals_fade_index = 0;
}

// 0x00A5C980 (:3414)
// A guerrilla driving an EDF vehicle blows its cover (for 1 s) near EDF npcs (10 m), checked every 3 s.
void player::do_frame_blow_undercover()
{
    timestamp& not_undercover = combat.not_undercover_timestamp;
    if ((not_undercover.valid() && !not_undercover.elapsed()) || !Blow_undercover_limiter.elapsed())
    {
        return;
    }
    Blow_undercover_limiter.set(3000);
    if (vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    vehicle* vp = (vehicle*)World.object_from_handle(vehicle_handle);
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || vp->info == nullptr || vp->info->default_team != HUMAN_TEAM_EDF)
    {
        return;
    }
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        npc* np = World.get_object<npc>(i);
        // Magic number: 100, 10 m squared.
        if (np->get_team() == HUMAN_TEAM_EDF && vec_dist_squared(np->pos, pos) <= 100.0f)
        {
            not_undercover.set(1000);
        }
    }
}

// 0x00A84490 (:3452)
// The backpack fades with the camera alpha override while a fade runs; at the end it is fully hidden (out) or shown
// (in, ending both fades).
void player::update_backpack_fade()
{
    if (Multiplayer)
    {
        return;
    }
    if (backpack == nullptr)
    {
        pflags.backpack_fade_out = 0;
        pflags.backpack_fade_in = 0;
        return;
    }
    if (pflags.backpack_fade_in)
    {
        if (fade_backpack_time.valid() && !fade_backpack_time.elapsed())
        {
            backpack->set_opacity_plus_child_items(camera_alpha_override * (1.0f - fade_backpack_time.percent()));
            return;
        }
        if (camera_alpha_override == 1.0f)
        {
            fade_backpack_time.invalidate();
            backpack->set_opacity_plus_child_items(1.0f);
            pflags.backpack_fade_out = 0;
            pflags.backpack_fade_in = 0;
        }
    }
    else if (pflags.backpack_fade_out)
    {
        if (fade_backpack_time.valid() && !fade_backpack_time.elapsed())
        {
            backpack->set_opacity_plus_child_items(camera_alpha_override * fade_backpack_time.percent());
            return;
        }
        if (camera_alpha_override == 1.0f)
        {
            fade_backpack_time.invalidate();
            backpack->set_opacity_plus_child_items(0.0f);
        }
    }
}

// 0x00ACD590 (:3487)
// Opening an ammo box: when the impact ends the weapon cabinet opens (Demolitions Master: weapons are restocked by a
// share of their box restock, scaled by the district morale and difficulty, and the inventory is shown).
void player::do_frame_open_ammo_box()
{
    if (cur_state != HS_AMMO_BOX)
    {
        return;
    }
    if (ammo_box_handle == 0xFFFFFFFF)
    {
        ammo_box_reset(); // inlined
        return;
    }
    object* box = World.object_from_handle(ammo_box_handle);
    if (box == nullptr || box->get_obj_type() != OT_ITEM)
    {
        ammo_box_reset(); // inlined
        return;
    }
    // Magic number: 0x344, the open ammo box action alias.
    if (!char_inst->is_playing_action((vanim_aliases)0x344))
    {
        ammo_box_reset();
        return;
    }
    if (!vanim_has_trigger_just_elapsed(char_inst->vp, Trigger_impact_end, (vanim_aliases)0x344, -1))
    {
        return;
    }
    if (!activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER))
    {
        hud_ui_weapon_cabinet_show(true);
        return;
    }
    // Magic numbers: the morale share of the restock, 40% to 100% between 10% and 85% morale.
    float morale_share = (float)(((double)gfm_get_district_morale_percent(nullptr) - 0.1) / 0.75);
    float clamped = 0.0f;
    if (0.0f <= morale_share - 0.0f)
    {
        clamped = morale_share;
    }
    if (0.0f <= clamped - 1.0f)
    {
        clamped = 1.0f;
    }
    const float restock_scale = (float)((double)clamped * 0.6 + 0.4);
    item_pickup_info pickups[3];
    int num_pickups = 0;
    inventory_item* iip = inventory;
    while (iip != nullptr)
    {
        weapon* wp = iip->weapon_handle != 0xFFFFFFFF ? (weapon*)World.object_from_handle(iip->weapon_handle) : nullptr;
        if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && wp->w_info->ammo_box_restock != 0)
        {
            int charges = 0;
            if (wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
            {
                int unused;
                charges = count_charges(get_handle(), unused);
            }
            const float share = (float)wp->w_info->ammo_box_restock / (float)wp->w_info->max_rounds;
            const float rounds = (float)wp->get_max_rounds() * share;
            int restock = (int)(rounds * restock_scale * difficulty_get_value(DIFFICULTY_PLAYER_AMMO_CRATES, NUM_DIFFICULTY_LEVELS));
            if (restock < 1)
            {
                restock = 1;
            }
            const int room = ((wp->get_max_rounds() - wp->get_rounds_in_magazine()) - wp->get_rounds_in_reserve()) - charges;
            if (room <= restock)
            {
                restock = room;
            }
            if (restock > 0)
            {
                wp->add_ammo(restock);
                hud_ui_weapons_add_weapon_ammo_slot_update(iip->selection_slot);
                if (num_pickups < 3)
                {
                    pickups[num_pickups].item_name = wp->w_info->icon_name;
                    pickups[num_pickups].num_picked_up = restock;
                    num_pickups++;
                }
            }
            ammo_acquired_event ev;
            ev.w_info = wp->w_info;
            gem_post_event(ev);
        }
        iip = iip->next;
        if (iip == inventory)
        {
            break;
        }
    }
    if (inventory != nullptr)
    {
        if (num_pickups > 0)
        {
            hud_ui_add_item_pickup_message(pickups, num_pickups);
        }
        ovl_inventory_open(CBA_BUTTON_INVALID, 1.0f);
    }
}

// 0x00AC2300 (:3596)
// The player at an action node: leaving the weapon locker re-equips; at the reserved node the entry animation runs
// (props created and removed at their triggers), then the action until the node is completed and left.
void player::do_frame_action_node()
{
    if (char_inst != nullptr)
    {
        // Magic number: 0x389, the weapon locker exit action alias.
        const bool exiting = char_inst->is_playing_action((vanim_aliases)0x389);
        if (exiting)
        {
            pflags.waiting_for_locker_exit = 1;
        }
        if (pflags.waiting_for_locker_exit && (!exiting || vanim_is_action_ending_this_frame(char_inst->vp, (vanim_aliases)0x389)))
        {
            human_equip_inventory_item(this, last_equipped_weapon);
            pflags.waiting_for_locker_exit = 0;
            move_speed = -1.0f;
            cur_state = HS_STANDARD;
        }
    }
    if (action_node_data.action_node_set.size() < 1)
    {
        return;
    }
    const unsigned int node_handle = action_node_data.action_node_set[0];
    if (node_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_action_node* anp = (object_action_node*)World.object_from_handle(node_handle);
    bool usable = false;
    if (anp != nullptr && anp->get_obj_type() == OT_ACTION_NODE)
    {
        // A node of a vehicle spawn node only counts while its vehicle stands upright (0.95).
        const unsigned int parent_handle = anp->get_parent();
        object* parent_obj = parent_handle != 0xFFFFFFFF ? World.object_from_handle(parent_handle) : nullptr;
        if (parent_obj == nullptr || parent_obj->get_obj_type() != OT_VEHICLE_SPAWN_NODE)
        {
            usable = true;
        }
        else
        {
            object* vehp = World.object_from_handle(((object_vehicle_spawn_node*)parent_obj)->get_child_vehicle_handle());
            usable = vehp == nullptr || 0.95f <= vehp->orient.uvec.y;
        }
    }
    if (!usable)
    {
        action_node_reset(nullptr);
        return;
    }
    if (!has_node_reserved(anp->get_handle()) || anp->get_handle() == 0xFFFFFFFF || action_node_data.current_action_node != anp->get_handle())
    {
        return;
    }
    direct_control_vel.zero();
    steering_vector.zero();
    if (anp->base_node_info->prop_creation_time == PROP_CREATION_TIME_ENTRY_ANIM && amb_prop_info == nullptr && anp->pickup_action_trigger_elapsed(this))
    {
        amb_prop_create(anp->base_node_info->amb_prop_info, this);
    }
    if (anp->base_node_info->prop_removal_time == PROP_REMOVAL_TIME_ENTRY_ANIM && amb_prop_info != nullptr && anp->drop_action_trigger_elapsed(this))
    {
        amb_prop_detach_props(this, true);
    }
    if (!pflags.action_node_anim_started)
    {
        if (anp->play_action(this))
        {
            pflags.action_node_anim_started = 1;
            return;
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        action_node_reset(nullptr);
        return;
    }
    if (!has_entered_action_node(anp))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        action_node_reset(nullptr);
    }
    if (completed_action_node(anp))
    {
        if (!anp->doing_action(this))
        {
            if (anp->base_node_info->node_flags.weapon_cabinet)
            {
                gamelog_store_current_loadout(this);
                hud_ui_weapon_cabinet_show(false);
                const vector log_pos = pos;
                const int duration = gamelog_get_game_duration();
                const char difficulty = gamelog_get_difficulty();
                const int locale_id = gamelog_get_locale();
                const char pct_game_complete = gamelog_get_pct_complete();
                const int time_played = gamelog_get_time_played();
                const int time = gamelog_get_logging_time();
                const int player_checksum = gamelog_get_player_checksum();
                log_event_player_use_weapons_cache(player_checksum, time, time_played, pct_game_complete, locale_id, difficulty, duration, log_pos, 0);
            }
            anp->exit_node(this);
            action_node_reset(anp);
        }
    }
    else if (!anp->doing_action(this) && !anp->play_action(this))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        action_node_reset(nullptr);
    }
}

// 0x00A106E0 (:3734)
// A positional record every 250 ms (8 kept).
void player::do_frame_track_pos()
{
    const int since = tracking_period.time_since();
    // Magic number: a record every 250 ms.
    if (since > 249)
    {
        tracking_period.set(0);
        positional_records[next_record].pos = pos;
        positional_records[next_record].period_ms = since;
        next_record++;
        if (next_record > 7)
        {
            next_record = 0;
        }
    }
}

// 0x00AD5A80 (:3865)
// The backpack the upgrades give (when it changed), tracking, ammo boxes, action nodes, the path timeout, radiation,
// blood decals, cover blowing and the liberation radio; in single player the play time, the backpack fade in
// protected vehicle positions and the district change message.
void player::do_frame()
{
    if (!Multiplayer && !wrecking_crew_is_active() && dlc_get_current_id() == -1)
    {
        const upgrade_definition_names type = upgrade_get_backpack_type();
        if (type != NUM_OF_UPGRADE_DEFINITIONS)
        {
            int index = -1;
            if (type == UPGRADE_DEFINITION_JETPACK)
            {
                index = 0;
            }
            else if ((int)type >= UPGRADE_DEFINITION_BACKPACK_THRUST)
            {
                index = type - UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL14;
            }
            if (index >= 0 && index != s_lastBackpackIndex)
            {
                multi_object_backpack* bp = multi_game_backpack_create(index, pos, orient);
                bp->event_player_picked_up(this, true, false);
                s_lastBackpackIndex = index;
            }
        }
    }
    do_frame_track_pos();
    do_frame_open_ammo_box();
    do_frame_action_node();
    do_frame_path_timeout();
    do_frame_radiation();
    do_frame_blood_decals();
    do_frame_blow_undercover();
    play_district_liberated_radio_line();
    if (Multiplayer == true)
    {
        if (!hflags.is_on_ladder && human_combat_get_manned_turret(this) == nullptr && human_in_vehicle(this, nullptr) == false)
        {
            // Magic number: combat ready for a minute.
            human_set_combat_ready(this, true, 60000);
        }
        return;
    }
    meta_data.update_play_time(); // inlined
    const bool protected_pos = human_in_protected_vehicle_pos(this) || activity_vrail_is_riding_shotgun_running();
    if (!Local_player_in_protected_vehicle_postion)
    {
        if (protected_pos)
        {
            local_player_fade_out_backpack(200);
        }
    }
    else if (!protected_pos)
    {
        local_player_fade_in_backpack(200);
    }
    Local_player_in_protected_vehicle_postion = protected_pos;
    district* current_district = district_local_player();
    if (Player_district != current_district)
    {
        wchar_t district_name[128];
        wchar_t message[128];
        _snprintf_safe(district_name, 128, localization_get_display_string_req(current_district->get_localized_name_hash()));
        // Original: the district name is formatted and then not used; the message is the format string itself.
        _snprintf_safe(message, 128, localization_get_display_string_req("HM_NEW_SECTOR", nullptr));
        hud_message_add_minor(message, -1.0f, false);
        Player_district = current_district;
        meta_data.set_current_district(current_district);
    }
}

// 0x00A10850 (:4707)
bool player::is_undercover() const
{
    if (current_team == HUMAN_TEAM_GUERILLA && combat.not_undercover_timestamp.elapsed())
    {
        return !activity_is_player_cover_blown();
    }
    return false;
}

// ---- system

// 0x00A10880 (:4729)
void player_system_init()
{
    audiolib_cue_get_id(Comm_tower_enter_sound_name, &Comm_tower_enter_sound);
    audiolib_cue_get_id(Comm_tower_leave_sound_name, &Comm_tower_leave_sound);
    Player_mission_radio_message_rfc = voice_line_handle_from_name("mis_alert_green_rfc");
    Player_mission_radio_message_tech = voice_line_handle_from_name("mis_alert_green_tech");
}

// 0x00A108C0 (:4753)
void player_level_init_pre()
{
    Player_died = false;
}

// 0x00AD5CD0 (:4758)
// Creates the local player at the territory's player start (multiplayer: its spawn point, with its game data handle
// and team) and resets its upgrades; a save game restores the inventory and meta data. Single player gets its
// squads, district and the unlocks of a completed game (backpacks, golden hammers); the zoom is reset.
bool player_level_init_post()
{
    input_reset_gyro_orientation_origin();
    Player_recruit_enabled = true;
    Player_dismiss_enabled = true;
    Jetpack_effect_info = effects_get_handle("jetpack");
    Player_mission_radio_timer.invalidate();
    Player_mission_radio_last_node = 0xFFFFFFFF;
    Liberated_radio_timestamp.invalidate();
    Blow_undercover_limiter.set(0);
    if (Multiplayer == true && multi_is_dedicated_server())
    {
        return true;
    }
    if (multi_game_spectating())
    {
        return true;
    }
    int spawn_index = -1;
    if (wrecking_crew_is_active())
    {
        wrecking_crew_spawn_players();
        Player = wrecking_crew_get_current_player();
    }
    else
    {
        vector start_pos;
        matrix start_orient;
        if (!Multiplayer)
        {
            start_pos = World.sl_flags.player_start_pos;
            start_orient = World.sl_flags.player_start_orient;
        }
        else
        {
            multi_game_get_player_starting_position(spawn_index, start_pos, start_orient);
        }
        const human_info* hi = player_local_get_human_info_for_creation();
        obj_prop_block props(start_pos, start_orient, nullptr);
        props.set<const human_info*>(HUMAN_PROP_CINFO, hi);
        if (Multiplayer == true)
        {
            const unsigned int handle = multi_game_data_get_my_handle(ST_GAME);
            props.set<unsigned int>(MULTI_PLAYER_HANDLE, handle);
            const multi_game_data* data = multi_game_data_get(handle, false, false);
            const human_teams team = data == nullptr ? HUMAN_TEAM_CIVILIAN : data->team;
            props.set<int>(MULTI_TEAM, (int)team);
        }
        Player = World.create_object<player>(props, 0xFFFFFFFF);
        // Original: the meta data of a missing player (offset from null) is reset anyway.
        player_meta_data* meta = Player == nullptr ? nullptr : &Player->meta_data;
        meta->reset_upgrades();
    }
    player* pp = Player;
    if (pp == nullptr)
    {
        return false;
    }
    if (World.save_file != nullptr)
    {
        inventory_item* iip = pp->inventory;
        while (iip != nullptr)
        {
            inventory_item* next = iip->next;
            inventory_item* first = pp->inventory;
            human_delete_item_from_inventory(pp, iip, false, false);
            iip = next != first ? next : nullptr;
        }
        // The offset is taken before the read: the operands of != have no evaluation order.
        unsigned int section_pos = cf_tell(World.save_file);
        if (section_pos != cf_read_uint32(World.save_file))
        {
            return false;
        }
        unsigned int end = cf_read_uint32(World.save_file);
        Player->load_inventory(World.save_file, false);
        if (cf_tell(World.save_file) != end)
        {
            if (World.save_file != nullptr)
            {
                cf_close(World.save_file);
                World.save_file = nullptr;
            }
            return false;
        }
        section_pos = cf_tell(World.save_file);
        if (section_pos != cf_read_uint32(World.save_file))
        {
            return false;
        }
        end = cf_read_uint32(World.save_file);
        Player->meta_data.load_meta_data(World.save_file);
        if (cf_tell(World.save_file) != end)
        {
            if (World.save_file != nullptr)
            {
                cf_close(World.save_file);
                World.save_file = nullptr;
            }
            return false;
        }
    }
    if (!Multiplayer)
    {
        Player->create_player_squads();
    }
    else
    {
        pp = Player;
        if (Multiplayer == true && multi_is_server())
        {
            const unsigned short net_handle = multi_core_network_handle_from_game_handle(Player->get_handle(), false);
            multi_game_data_set_player_instance_network_handle(Player->mp_data, net_handle);
            const char xp_level = gamelog_get_player_xp_level(Player);
            const int backpack_crc = gamelog_get_backpack_crc(Player->backpack);
            const char player_index = gamelog_get_player_index(Player);
            const int time = gamelog_get_logging_time();
            const int checksum = gamelog_get_player_checksum();
            pp = Player;
            Player->backpack_equip_log_handle = log_start_event_backpack_equipped(checksum, time, player_index, backpack_crc, xp_level);
        }
        if (spawn_index > 0)
        {
            player_start* start = World.get_object<player_start>(spawn_index);
            if (start != nullptr)
            {
                pp->mp_last_spawn_point_handle = start->get_handle();
            }
        }
        multi_gameplay_mode_add_player(pp);
    }
    {
        const vector log_pos = Player->pos;
        const int duration = gamelog_get_game_duration();
        const int locale_id = gamelog_get_locale();
        const char player_index = gamelog_get_player_index(Player);
        const int time = gamelog_get_logging_time();
        const int checksum = gamelog_get_player_checksum();
        log_event_player_create(checksum, time, player_index, locale_id, duration, log_pos);
    }
    pp = Player;
    if (!Multiplayer)
    {
        if (!wrecking_crew_is_active())
        {
            Player_district = district_local_player();
            Player->meta_data.set_current_district(Player_district);
        }
        pp = Player;
        if (!Multiplayer && !wrecking_crew_is_active())
        {
            const int dlc_id = dlc_get_current_id();
            player_profile_data* profile = player_profile_get("player", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
            pp = Player;
            if (profile != nullptr)
            {
                // A completed game (or completed DLC) unlocks the golden hammer upgrade (39) and, for the base game,
                // all backpacks, keeping the selected one.
                const bool completed = (dlc_id < 0 && profile->game_completed) || (dlc_id == 1 && profile->new_data.game_completed_dlc);
                if (completed)
                {
                    // Magic number: upgrade 39, the golden sledgehammer reward.
                    Player->meta_data.unlock_upgrade(0x27, 1, false);
                    if (dlc_get_current_id() != -1)
                    {
                        Player->meta_data.purchase_upgrade(0x27, false, true);
                    }
                }
                pp = Player;
                if (dlc_id < 0 && profile->game_completed)
                {
                    s_lastBackpackIndex = -1;
                    const upgrade_definition_names selected = upgrade_get_backpack_type();
                    for (unsigned int id = UPGRADE_DEFINITION_BACKPACK_THRUST; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
                    {
                        Player->meta_data.unlock_upgrade(id, 1, false);
                    }
                    pp = Player;
                    if (selected != NUM_OF_UPGRADE_DEFINITIONS)
                    {
                        if ((unsigned int)(selected - UPGRADE_DEFINITION_BACKPACK_THRUST) < 9)
                        {
                            Player->meta_data.unlock_upgrade(selected, 1, false);
                        }
                        else if (selected == UPGRADE_DEFINITION_JETPACK)
                        {
                            Player->meta_data.select_jetpack();
                        }
                        pp = Player;
                    }
                }
                if (profile->golden_hammer_unlocked)
                {
                    for (const hammer_upgrade_data& hd : Hammer_upgrade_data)
                    {
                        if (hd.unlocked_with_golden_hammer)
                        {
                            pp->meta_data.unlock_upgrade(hd.upgrade_id, 1, false);
                            pp = Player;
                        }
                    }
                }
            }
        }
    }
    pp->move_speed = -1.0f;
    pp->cur_state = HS_STANDARD;
    pp->cur_movement_state = HMS_IDLE;
    player* target = pp;
    if (pp->zoom_state != PZS_OFF)
    {
        player_set_zoom_state(pp, PZS_OFF, false); // inlined
        target = Player;
    }
    Cartographer.set_target(target->get_handle());
    Player->hflags.pushes_other_humans = 1;
    return true;
}

// 0x00A9E080 (:4963)
void player_level_shutdown(player* pp)
{
    if (pp->ragdoll_idx != -1)
    {
        havok_ragdoll_unbind_from_human(pp->ragdoll_idx);
        pp->ragdoll_idx = -1;
        pp->ragdoll_source_human_handle = 0xFFFFFFFF;
        pp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
    }
    pp->disband_ally_and_escort_squads();
    World.destroy_object(pp, false);
}

// 0x00AE66B0 (:4982)
// One player frame: its own processing, zoom cancelling (death, falling, jumping), the facial idle, the
// non-inventory item, queued melee moves, camera shakes, then the look-at target from the camera, weapons, the root
// bone offsets of cover and aiming, and the human frame.
void player_do_frame(player* pp)
{
    if (wrecking_crew_is_active() && (pp == nullptr || pp != Player))
    {
        return;
    }
    pp->hflags.processed_this_frame = 0;
    if (pp->obj_flags.destroyed)
    {
        return;
    }
    pp->hflags.processed_this_frame = 1;
    pp->do_frame();
    if (Multiplayer == true)
    {
        multi_game_player_do_frame(pp);
    }
    if (controls_is_button_just_pressed(CBA_RESET_GYRO, Player_controller_index))
    {
        input_reset_gyro_orientation_origin();
    }
    if (pp->check_squads_timer.elapsed())
    {
        // Magic numbers: the squads are checked every 0.9 to 1.1 s.
        pp->check_squads_timer.set_rand(900, 1100);
        pp->check_squads();
    }
    if (pp == Player && pp->is_killed() && pp->zoom_state != PZS_OFF)
    {
        player_set_zoom_state(pp, PZS_OFF, true); // inlined
    }
    if (pp->zoom_state == PZS_OFF || pp->zoom_state == PZS_SPRINT)
    {
        pp->zoom_cancel_timestamp.invalidate();
    }
    else
    {
        vector vel;
        if (pp->rdd.is_loaded())
        {
            pp->rdd.get_data()->controller->get_velocity(vel);
        }
        else
        {
            vel.zero();
        }
        // Falling (not supported, outside a vehicle, faster than 2 m/s vertically) or jumping cancels the zoom after
        // 200 ms; otherwise the player stays combat ready.
        const bool falling = !(pp->rdd.is_loaded() && pp->rdd.get_data()->controller->flags.is_supported) && !human_in_vehicle(pp, nullptr) && 2.0f < fabsf(vel.y);
        const bool jumping = pp->movement_mode == HMM_JUMP;
        if ((falling || jumping) && pp->zoom_cancel_timestamp.valid())
        {
            if ((pp->zoom_cancel_timestamp.elapsed() || jumping) && pp->zoom_state != PZS_OFF)
            {
                player_set_zoom_state(pp, PZS_OFF, false); // inlined
            }
        }
        else
        {
            // Magic numbers: combat ready for 1.5 s, the zoom cancel after 200 ms.
            human_set_combat_ready(pp, true, 1500);
            pp->zoom_cancel_timestamp.set(200);
        }
    }
    if (pp->facial_idle.elapsed())
    {
        // Magic number: animation 0x142, the facial idle morph.
        if (!cutscene_is_playing() && !vanim_is_playing_action(pp->char_inst->vp, ANIM_ALIAS_FACIAL_MORPH))
        {
            vanim_play_action(pp->char_inst->vp, index_alias(0x142, ANIM_ALIAS_FACIAL_MORPH), 0, 1.0f, 1.0f);
        }
        pp->facial_idle.set(1500);
    }
    if (pp->non_inventory_item_handle != 0xFFFFFFFF)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && (!wp->w_info->flags.non_inventory || wp->get_handle() != pp->non_inventory_item_handle) &&
            (pp->desired_equipped_inv_item == nullptr || pp->desired_equipped_inv_item->weapon_handle != pp->non_inventory_item_handle))
        {
            object* objp = World.object_from_handle(pp->non_inventory_item_handle);
            bool keep = false;
            if (objp != nullptr && objp->get_obj_type() == OT_ITEM)
            {
                // Magic number: within half a meter the item is still being put down.
                if (vec_dist(objp->pos, pp->pos) <= 0.5f)
                {
                    keep = true;
                }
                else
                {
                    hkpRigidBody* rb = havok_body_get_pointer(objp->havok_handle);
                    if (havok_has_ignore_property(rb))
                    {
                        havok_remove_ignore_property(rb);
                    }
                }
            }
            if (!keep)
            {
                pp->non_inventory_item_handle = 0xFFFFFFFF;
            }
        }
    }
    if (pp->combat_move_queue != -1)
    {
        bool start = true;
        if (vanim_is_playing_action(pp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE))
        {
            if (pp->block_movement_anim != -1)
            {
                start = false;
            }
            else
            {
                vanim_get_action(pp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE)->alias = ANIM_ALIAS_NONE;
            }
        }
        if (start && pp->block_movement_anim == -1)
        {
            const human_melee_attack_types attack = (human_melee_attack_types)pp->combat_move_queue;
            pp->combat.cur_melee_combat_move = nullptr;
            human_combat_attack_melee_armed(pp, attack, false, false);
            pp->combat_move_queue = -1;
        }
    }
    v_animate* vp = pp->char_inst->vp;
    // Magic numbers: 0x3DB, the remote charge throw alias; past frame 10 the jump or fall ends the combat ready state.
    if (vp->default_group_index == ANIMATION_GROUP_REMOTE_CHARGE && (pp->cur_state == HS_JUMP || pp->cur_state == HS_FALL) &&
        (!vanim_is_playing_action(vp, (vanim_aliases)0x3DB) || 10 < vanim_get_anim_frame(pp->char_inst->vp, (vanim_aliases)0x3DB)))
    {
        human_in_vehicle_interaction(pp, nullptr);
        pp->combat.combat_ready_timestamp.invalidate();
    }
    if (!human_in_vehicle(pp, nullptr) && pp->get_handle() == Rfg_camera.target_handle)
    {
        if (pp->rdd.get_data()->controller->flags.sliding)
        {
            // Magic number: the slope slide shake reaches full strength at 7 m/s downward.
            float strength = -pp->actual_velocity.y / 7.0f;
            if (strength - 0.0f < 0.0f)
            {
                strength = 0.0f;
            }
            if (0.0f <= strength - 1.0f)
            {
                strength = 1.0f;
            }
            camera_shake_play("slope_slide", strength, false);
        }
        const char* shake;
        float strength;
        // Magic numbers: sprinting above 2 m/s (4.5 m/s actual), running above 0.5 m/s (full at 4.5 m/s).
        if (pp->pflags.do_sprint && 2.0f < pp->move_speed && 4.5f < pp->actual_velocity_mag && pp->movement_mode != HMM_JUMP && pp->movement_mode != HMM_FALL)
        {
            strength = pp->move_speed / Player_max_sprint_speed;
            shake = "player_sprinting";
        }
        else if (pp->zoom_state == PZS_SCOPE)
        {
            strength = pp->cur_stance == HST_CROUCH ? 0.5f : 1.0f;
            shake = "player_zooming";
        }
        else if (pp->move_speed <= 0.5f)
        {
            strength = 1.0f;
            shake = "player_standing";
        }
        else
        {
            strength = pp->move_speed / 4.5f;
            shake = "player_running";
        }
        camera_shake_play(shake, strength, false);
    }
    else if (human_in_vehicle(pp, nullptr) && pp->vehicle_handle != 0xFFFFFFFF)
    {
        vehicle* vehp = (vehicle*)World.object_from_handle(pp->vehicle_handle);
        if (vehp != nullptr && vehp->get_obj_type() == OT_VEHICLE && vehp->m_flags.m_on_fire)
        {
            camera_shake_play("vehicle_onfire", 1.0f, false);
        }
    }
    if (Player == nullptr || pp->get_handle() != Player->get_handle())
    {
        // a remote player: looks where its camera looks
        if (Multiplayer == true && multi_is_server())
        {
            multi_game_player_process_state(pp);
        }
        if (pp->mp_flags.get_detonated_charges())
        {
            pp->mp_flags.set_detonated_charges(0);
            gameplay_register_player_attack(pp, nullptr, DBT_EXPLOSION, true);
        }
        matrix look = pp->orient;
        bool aim = pp->combat.combat_ready_timestamp.valid() && !pp->combat.combat_ready_timestamp.elapsed();
        if (Rfg_camera.mode == CAMERA_FREE_MODE || Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE)
        {
            if (!pp->combat.combat_ready_timestamp.valid() || pp->combat.combat_ready_timestamp.elapsed())
            {
                look.fvec = pp->orient.fvec;
            }
            else
            {
                look.fvec = pp->mp_camera_orient.fvec;
                aim = true;
            }
        }
        else
        {
            look.fvec = pp->mp_camera_orient.fvec;
            aim = true;
        }
        if (pp->cur_state == HS_LADDER)
        {
            look.fvec = pp->orient.fvec;
        }
        // Magic number: the look-at point 10 km ahead.
        // x87: each coordinate is computed in double and rounded once
    const vector target((float)((double)look.fvec.x * 10000.0 + (double)pp->pos.x), (float)((double)pp->pos.y + (double)look.fvec.y * 10000.0),
                        (float)((double)pp->pos.z + (double)look.fvec.z * 10000.0));
        human_set_look_at_target(pp, target, aim, -1.0f);
        player_process_object_interaction(pp);
        human_weapons_process_human(pp);
        const float frametime = Frametime;
        const weapon_info* w_info = pp->get_equipped_weapon() == nullptr ? nullptr : pp->get_equipped_weapon()->w_info;
        multi_game_stats_record_weapon_time(pp, w_info, frametime);
        multi_game_stats_record_backpack_time(pp, pp->backpack == nullptr ? nullptr : pp->backpack->backpack_info, Frametime);
        player_cover_do_frame(pp);
        human_do_frame(pp, true);
        return;
    }
    // the local player: looks where the camera looks (the free and third person cameras only while combat ready;
    // melee weapons look level)
    matrix look = pp->orient;
    bool aim = pp->combat.combat_ready_timestamp.valid() && !pp->combat.combat_ready_timestamp.elapsed();
    if (Rfg_camera.mode == CAMERA_FREE_MODE || Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE || Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        if (!aim || Rfg_camera.mode == CAMERA_SLEW_MODE)
        {
            look.fvec = pp->orient.fvec;
        }
        else
        {
            weapon* wp = pp->get_equipped_weapon();
            look.fvec = Rfg_camera.ideal_orient.fvec;
            if (wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
            {
                look.fvec.y = 0.0f;
            }
            aim = true;
        }
    }
    else
    {
        look.rotate_around_local_x(Rfg_camera.lookaround_params.pitch);
        aim = true;
    }
    // x87: each coordinate is computed in double and rounded once
    const vector target((float)((double)look.fvec.x * 10000.0 + (double)pp->pos.x), (float)((double)pp->pos.y + (double)look.fvec.y * 10000.0),
                        (float)((double)pp->pos.z + (double)look.fvec.z * 10000.0));
    human_set_look_at_target(pp, target, aim, -1.0f);
    if (!using_high_resolution_weapons)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && wp->rdd.is_loaded() && !wp->special_flags.using_high_res_texture && wp->info->high_res_srid != 0)
        {
            wp->stream_in_high_res();
        }
    }
    if (pp->pflags.script_controlled)
    {
        if (pp->script_mode != PSM_VEHICLE_ENTER)
        {
            return;
        }
        // the scripted vehicle entry: request it within 2 m, pathfind to the vehicle otherwise
        const unsigned int vh = pp->script_data.vehicle_enter_data.vehicle_handle;
        vehicle* vehp = vh != 0xFFFFFFFF ? (vehicle*)World.object_from_handle(vh) : nullptr;
        if (vehp == nullptr || vehp->get_obj_type() != OT_VEHICLE)
        {
            pp->pflags.script_controlled = 0;
            pp->script_mode = PSM_NONE;
            return;
        }
        if (vec_dist_squared(pp->pos, vehp->pos) < 4.0f)
        {
            vi_enter_data enter;
            enter.specified_seat = VSI_INVALID;
            enter.seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
            enter.destination_seat = VSI_INVALID;
            memset(&enter.flags, 0, sizeof(enter.flags));
            if (vehicle_interaction_request_enter(pp, vehp, &enter))
            {
                return;
            }
            pp->pflags.script_controlled = 0;
            pp->script_mode = PSM_NONE;
        }
        if (pp->movement_mode != HMM_NONE)
        {
            return;
        }
        human_pathfind_to(pp, vehp->pos, HS_STANDARD, (human_movement_states)((pp->script_data.vehicle_enter_data.run ? 1 : 0) + HMS_WALK), 0xFFFFFFFFFFFFFFFFull, false);
        return;
    }
    player_process_object_interaction(pp);
    human_weapons_process_human(pp);
    if (Multiplayer)
    {
        const float frametime = Frametime;
        const weapon_info* w_info = pp->get_equipped_weapon() == nullptr ? nullptr : pp->get_equipped_weapon()->w_info;
        multi_game_stats_record_weapon_time(pp, w_info, frametime);
        multi_game_stats_record_backpack_time(pp, pp->backpack == nullptr ? nullptr : pp->backpack->backpack_info, Frametime);
    }
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        automobile* ap = (automobile*)World.object_from_handle(pp->vehicle_handle);
        if (ap != nullptr && ap->get_obj_type() == OT_VEHICLE && ap->get_sub_type() == OT_SUB_VEHICLE_AUTO)
        {
            automobile_handle_human_run_over(ap);
        }
    }
    // the camera bone follows the animation unmodified
    animlib_clear_bone_info(pp->char_inst->aim_handle);
    if (pp->h_info->camera_bone_index > -1 && animlib_allocate_bones(pp->char_inst->aim_handle))
    {
        matrix identity;
        identity.make_identity();
        // Magic number: 0xD, the bone modifier flags (rotation, translation and scale).
        animlib_register_bone_modifier(pp->char_inst->aim_handle, BONE_MODIFIER_COPY, pp->h_info->camera_bone_index, 0xD, identity, nullptr, 0.0f);
    }
    // the cover root offset ramps to -0.1 m in cover and back to 0
    float cover_offset = pp->cover_root_offset;
    if (pp->cur_state == HS_COVER)
    {
        if (-0.1f < cover_offset)
        {
            cover_offset = cover_offset - Frametime;
            if (cover_offset - -0.1f < 0.0f)
            {
                cover_offset = -0.1f;
            }
            pp->cover_root_offset = cover_offset;
        }
    }
    else if (cover_offset < 0.0f)
    {
        cover_offset = cover_offset + Frametime;
        if (0.0f <= cover_offset - 0.0f)
        {
            cover_offset = 0.0f;
        }
        pp->cover_root_offset = cover_offset;
    }
    // the spinebend root offset ramps (4 per second) to the camera pitch below the minimum, doubled when moving
    static float min_pitch = 0.7f; // 0x016AFE98, function static
    static float test_val = 0.9f; // 0x016AFE94, function static
#pragma warning(suppress : 4459) // the static's name is from rfg.pdb
    static float scaler = -0.155f; // 0x016AFE90, function static
    float pitch_offset;
    if (!Multiplayer)
    {
        pitch_offset = Rfg_camera.lookaround_params.pitch - min_pitch;
    }
    else
    {
        pitch_offset = pp->mp_camera_orient.extract_pitch() - min_pitch;
    }
    if (0.0f <= 0.0f - pitch_offset)
    {
        pitch_offset = 0.0f;
    }
    if (!pp->hflags.aim_at_pos)
    {
        pitch_offset = 0.0f;
    }
    if (0.01f < pp->move_speed)
    {
        pitch_offset = pitch_offset * 2.5f;
    }
    float spine_offset = pp->spinebend_root_offset;
    if (pitch_offset <= spine_offset)
    {
        spine_offset = spine_offset - Frametime * 4.0f;
        if (pitch_offset - spine_offset < 0.0f)
        {
            pitch_offset = spine_offset;
        }
    }
    else
    {
        spine_offset = Frametime * 4.0f + spine_offset;
        if (0.0f <= pitch_offset - spine_offset)
        {
            pitch_offset = spine_offset;
        }
    }
    pp->spinebend_root_offset = pitch_offset;
    float height_offset = 0.0f;
    bool offset_root = false;
    if (Custom_player_bone_transform && (pp->cur_state == HS_STANDARD || pp->cur_state == HS_COVER))
    {
        matrix43 root;
        animlib_get_bone_transform(pp->char_inst->aim_handle, 0, root);
        height_offset = (test_val - root.m_translation.y) * scaler;
        offset_root = height_offset != 0.0f;
    }
    if (!offset_root)
    {
        offset_root = pp->cover_root_offset != 0.0f || 0.0f < pp->spinebend_root_offset;
    }
    if (offset_root && animlib_allocate_bones(pp->char_inst->aim_handle))
    {
        matrix43 offset;
        offset.m_rotation.make_identity();
        float back = pp->cover_root_offset;
        float heading;
        if (back == 0.0f)
        {
            // Magic number: the root moves back 0.22 m per unit of spinebend offset, along the camera heading.
            back = pp->spinebend_root_offset * -0.22f;
            offset.m_rotation.fvec.set(0.0f, 0.0f, back);
            offset.m_rotation.fvec.rotate(pp->orient);
            heading = Rfg_camera.real_orient.extract_heading();
        }
        else
        {
            offset.m_rotation.fvec.set(0.0f, 0.0f, back);
            offset.m_rotation.fvec.rotate(pp->orient);
            heading = fl_atan2_safe(pp->cover_vector.x, pp->cover_vector.z);
        }
        offset.m_rotation.fvec.rotate_around_y_axis(heading);
        offset.m_rotation.fvec.y = height_offset;
        // Magic number: 0xE, the bone modifier flags (translation and scale).
        animlib_register_bone_modifier(pp->char_inst->aim_handle, BONE_MODIFIER_POSITION_OFFSET, 0, 0xE, offset.m_rotation, nullptr, 0.0f);
    }
    player_cover_do_frame(pp);
    player_record_single_player_stats(pp);
    human_do_frame(pp, true);
    player_check_if_stuck(pp);
}

// 0x00A108D0 (:5507)
player* player_get_local_player()
{
    return Player;
}

// 0x00A108E0 (:5512)
player_meta_data* player_get_meta_data()
{
    return Player == nullptr ? nullptr : &Player->meta_data;
}

// 0x00AE7750 (:5527)
void player_process_all()
{
    const int num = World.get_num_objects<player>();
    for (int i = 0; i < num; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->char_inst->rdd.is_loaded())
        {
            player_do_frame(pp);
        }
    }
}

// 0x00AB3260 (:5538)
void player_process_all_no_animation()
{
    const int num = World.get_num_objects<player>();
    for (int i = 0; i < num; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->char_inst->rdd.is_loaded())
        {
            human_do_frame_no_animation(pp);
        }
    }
}

// 0x00A9E0D0 (:5553)
// Original: the players are destroyed while walking the player list by index (the list is not shortened until the
// world processes its destroyed objects).
void player_level_shutdown_all()
{
    const int num = World.get_num_objects<player>();
    for (int i = 0; i < num; i++)
    {
        player_level_shutdown(World.get_object<player>(i)); // inlined
    }
    Player = nullptr;
}

// 0x00A9E160 (:5750)
// In a vehicle: asks to get out, and drops any zoom; with do_action false it only tells that the action is there. Near
// one: asks to get in, at the driver's seat (the turret's on the rail patrol vehicle), throwing out an NPC driver that
// is not defending the player. A multiplayer client only asks the server (mp_flags.vehicle).
bool player_use_vehicle(player* pp, bool do_action)
{
    if (human_in_vehicle_interaction(pp, nullptr))
    {
        if (pp->pathfind_time_out.valid() && !pp->pathfind_time_out.elapsed())
        {
            return false;
        }
        if (!do_action || pp->hflags.disallow_vehicle_exit)
        {
            return true;
        }
        if (Multiplayer && !multi_is_server())
        {
            pp->mp_flags.vehicle = true;
            pp->mp_action_object_handle = pp->action_object.handle;
            return false;
        }
        if (wrecking_crew_is_active())
        {
            return false;
        }
        vi_exit_data exit_data;
        vehicle_interaction_request_exit(pp, &exit_data);
        if (pp->zoom_state == PZS_OFF)
        {
            return true;
        }
        const turret* tp = human_combat_get_manned_turret(pp);
        if (tp != nullptr && tp->t_info->flags.force_fine_aim)
        {
            return true;
        }
        const player_zoom_state zoom = pp->zoom_state;
        pp->previous_zoom_state = zoom;
        pp->zoom_state = PZS_OFF;
        if (zoom != PZS_OFF)
        {
            if (pp != Player)
            {
                return true;
            }
            pp->get_equipped_weapon(); // Original: the result is not used.
            if (pp->previous_zoom_state == PZS_SCOPE)
            {
                pp->set_opacity_plus_child_items(1.0f);
                pp->set_casts_transparent_shadows_plus_child_items(false);
                hud_ui_rail_driver_set_visible(false);
                hud_ui_sniper_set_visible(false);
            }
        }
        if (pp == Player)
        {
            camera_set_view_player(PZS_OFF, false);
        }
        return true;
    }
    if (pp != nullptr)
    {
        const inventory_item* iip = pp->equipped_inv_item;
        if (iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory)
        {
            return false;
        }
    }
    if (pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return false;
    }
    vehicle* vp = (vehicle*)objp;
    vi_enter_data enter_data;
    if (!vehicle_is_usable(pp, &enter_data, vp, true, false))
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (Multiplayer && !multi_is_server())
    {
        pp->mp_flags.vehicle = true;
        pp->mp_action_object_handle = vp->get_handle();
        return true;
    }
    unsigned int patrol_vehicle = 0xFFFFFFFF;
    vrail_get_patrol_vehicle(patrol_vehicle);
    enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    enter_data.specified_seat = patrol_vehicle == vp->get_handle() ? vp->m_turret_mounts[0].info->seat : VSI_DRIVER;
    npc* driver = World.object_cast<npc>(vehicle_passenger_in_or_entering_seat(vp, VSI_DRIVER));
    if (driver != nullptr && !driver->npc_flags.activity_driver_vr && !npc_is_defending_player(driver))
    {
        driver->hflags.disallow_vehicle_exit = 0;
        driver->hflags.limited_vehicle_exit = 0;
        ai_system* ai = driver->get_ai();
        if (!driver->npc_flags.converted_guerrilla)
        {
            driver->npc_flags.despawn_npc = 1;
            ai->request_reaction((const object*)pp, 0x200, -1); // Magic number: the reaction reason bit for being hijacked (my reading)
        }
        vp->m_flags.m_hijacked = 1;
        ai->planner.request_replan();
    }
    pp->pathfind_time_out.set(400); // Magic number: ms before the action can be used again
    return vehicle_interaction_request_enter(pp, vp, &enter_data);
}

// 0x00A34200 (:5910)
// Between the driver's seat and the first turret's, unless this is the rail patrol vehicle.
void player_switch_seat(player* pp)
{
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = (vehicle*)objp;
    // Original: the handle shares the stack slot of pp and starts as its bits, so without a patrol vehicle it holds a
    // pointer. Kept: a pointer that equals a handle is not expected.
    unsigned int patrol_vehicle = (unsigned int)pp;
    vrail_get_patrol_vehicle(patrol_vehicle);
    if (patrol_vehicle == pp->vehicle_handle || pp->cur_state == HS_VEHICLE_EXITING)
    {
        return;
    }
    const vehicle_turret_mount* mount = vehicle_get_turret_mount(vp, 0);
    if (mount == nullptr)
    {
        return;
    }
    const vehicle_seat_index turret_seat = mount->info->seat;
    if (turret_seat == VSI_INVALID)
    {
        return;
    }
    const int index = vp->vii.find_human(pp);
    if (index == -1)
    {
        return;
    }
    const vehicle_seat_index seat = vp->vii.humans[index].enter_data.destination_seat == VSI_DRIVER ? turret_seat : VSI_DRIVER;
    if (seat == pp->vehicle_seat_idx)
    {
        return;
    }
    vi_enter_data enter_data;
    enter_data.specified_seat = seat;
    enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
    vehicle_interaction_request_enter(pp, vp, &enter_data);
}

// 0x00AC2620 (:5994): leaves the manned turret, or mans the one in the action object. A multiplayer client only asks the
// server (mp_flags.turret).
bool player_use_turret(player* pp, bool do_action)
{
    if (human_combat_get_manned_turret(pp) != nullptr)
    {
        if (!do_action)
        {
            return true;
        }
        if (Multiplayer && !multi_is_server())
        {
            pp->mp_flags.turret = true;
            return false;
        }
        if (pp != nullptr && pp == Player)
        {
            weapon* wp = human_combat_get_manned_turret(pp)->get_weapon_inst();
            if (wp != nullptr)
            {
                wp->stream_out_high_res();
            }
        }
        human_combat_abandon_turret(pp);
        return true;
    }
    if (pp->action_object.handle == 0xFFFFFFFF)
    {
        return true;
    }
    turret* tp = World.object_from_handle<turret>(pp->action_object.handle);
    if (tp == nullptr || !human_combat_can_man_turret(pp, tp) || !do_action)
    {
        return true;
    }
    if (Multiplayer && !multi_is_server())
    {
        pp->mp_flags.turret = true;
        pp->mp_action_object_handle = tp->get_handle();
        return true;
    }
    if (object_is_local_player(pp))
    {
        // Original: reads through null without an equipped weapon.
        pp->get_equipped_weapon()->stream_out_high_res();
    }
    human_combat_man_turret(pp, tp);
    return true;
}

// 0x00ABE560 (:6057)
// A player off a ladder without a non-inventory item mounts the ladder it is near (in single player one nobody uses);
// with do_action false it only tells whether it could.
bool player_use_ladder(player* pp, bool do_action)
{
    if (pp->hflags.is_on_ladder)
    {
        return false;
    }
    const inventory_item* iip = pp->equipped_inv_item;
    if (iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory)
    {
        return false;
    }
    ladder* lp = player_is_near_ladder(pp);
    if (lp == nullptr || !lp->is_enabled() || (!Multiplayer && lp->get_use_count() >= 1))
    {
        return false;
    }
    if (do_action)
    {
        human_mount_ladder(pp, lp);
    }
    return true;
}

// 0x00A9E430 (:6847)
// The jetpack (the upgrade without a backpack object, or an inactive jetpack backpack): boosting while ready burns
// fuel and accelerates upward (less near the 5 m/s ceiling) with its start sounds and effect; otherwise the fuel
// refills and a running boost stops. The effect follows the fuel, and the local player logs its use.
void player_update_jetpack(player* pp, bool boost_active)
{
    static float max_upward_speed = 5.0f; // 0x016AFE24, function static (m/s)
    static float jetpack_boost_fail_percent = 0.3f; // 0x016AFE28, function static
    const float old_fuel = pp->jetpack_fuel_percent;
    bool on_ground = false;
    if (pp->rdd.is_loaded())
    {
        const character_controller* cc = pp->rdd.get_data()->controller;
        on_ground = cc->flags.is_supported && !cc->flags.sliding;
    }
    if (on_ground || pp->hflags.is_on_ladder || pp->is_killed())
    {
        pp->pflags.jetpack_ready = 0;
    }
    if (Multiplayer && pp != Player)
    {
        pp->jetpack_fuel_percent = 1.0f;
    }
    bool has_jetpack = false;
    if (pp->backpack == nullptr)
    {
        has_jetpack = upgrade_get_backpack_type() == UPGRADE_DEFINITION_JETPACK;
    }
    else if (!pp->backpack->activated && pp->backpack->backpack_info->jetpack == true)
    {
        has_jetpack = true;
    }
    const float frametime = Multiplayer == true ? Frametime_real : Frametime;
    bool boosting = false;
    if (pp->jetpack_fuel_percent > 0.0f && has_jetpack && boost_active && pp->pflags.jetpack_ready)
    {
        boosting = true;
        if (Multiplayer == true && pp->hflags.invulnerable)
        {
            pp->hflags.invulnerable = 0;
            player_stop_material_effect(pp);
        }
        if (!pp->pflags.jetpack_started)
        {
            pp->pflags.do_sprint = 0;
            pp->pflags.jetpack_started = 1;
            if (pp->zoom_state == PZS_SPRINT)
            {
                player_set_zoom_state(pp, PZS_OFF, false); // inlined
            }
            audiolib_params params;
            params.flags = 8;
            params.fade_ms = 200;
            if (object_is_local_player(pp))
            {
                audiolib_play_stop(pp->jetpack_foley);
                if (jetpack_boost_fail_percent <= pp->jetpack_fuel_percent)
                {
                    audiolib_cue_play(Ga_cue_ids.jetpack_start_pc, nullptr, nullptr);
                    pp->jetpack_foley = audiolib_cue_play(Ga_cue_ids.jetpack_loop_pc, nullptr, nullptr);
                }
                else
                {
                    audiolib_cue_play(Ga_cue_ids.jetpack_boost_fail_pc, nullptr, nullptr);
                    pp->jetpack_foley = -1;
                }
            }
            else
            {
                audiolib_play_stop(pp->jetpack_foley);
                if (jetpack_boost_fail_percent <= pp->jetpack_fuel_percent)
                {
                    audiolib_cue_play_3d(Ga_cue_ids.jetpack_start_npc, &pp->pos, nullptr, nullptr, nullptr);
                    params.pos = pp->pos;
                    params.flags |= 1;
                    pp->jetpack_foley = audiolib_cue_play(Ga_cue_ids.jetpack_loop_npc, &params, nullptr);
                }
                else
                {
                    audiolib_cue_play_3d(Ga_cue_ids.jetpack_boost_fail_npc, &pp->pos, nullptr, nullptr, nullptr);
                    pp->jetpack_foley = -1;
                }
            }
            item* jetpack = human_get_prop(pp, PROP_TYPE_JETPACK, "multi_object_backpack_jetpack");
            if (jetpack != nullptr)
            {
                const int tag = jetpack->get_tag_index_from_name("Attach_To_Rack");
                pp->jetpack_effect = effects_play(Jetpack_effect_info, jetpack->get_handle(), tag, true, jetpack->get_handle(), 1.0f, false);
            }
        }
        else if (!object_is_local_player(pp))
        {
            audiolib_play_set_3d_position(pp->jetpack_foley, &pp->pos, nullptr, nullptr);
        }
        float use_rate = Jetpack_use_rate;
        if ((Multiplayer || wrecking_crew_is_active()) && pp->backpack != nullptr && pp->backpack->backpack_info->jetpack == true && pp->backpack->backpack_info->uses_fuel == true)
        {
            use_rate = pp->backpack->backpack_info->fuel_use_per_second;
        }
        float fuel = (float)((double)pp->jetpack_fuel_percent - (double)use_rate * (double)frametime);
        if (0.0f <= 0.0f - fuel)
        {
            fuel = 0.0f;
        }
        pp->jetpack_fuel_percent = fuel;
        vector vel;
        if (pp->rdd.is_loaded())
        {
            pp->rdd.get_data()->controller->get_velocity(vel);
        }
        else
        {
            vel.zero();
        }
        float lift;
        if (0.0f <= vel.y)
        {
            float up = vel.y;
            if (0.0f <= vel.y - max_upward_speed)
            {
                up = max_upward_speed;
            }
            lift = (float)(1.0 - (double)up / (double)max_upward_speed);
        }
        else
        {
            lift = 1.0f;
        }
        vel.y = (float)((double)Jetpack_acceleration * (double)lift * (double)frametime + (double)vel.y);
        human_controller_set_vel(pp, vel);
        if (pp->get_handle() == Rfg_camera.target_handle)
        {
            camera_shake_play("jump_pack", 1.0f, false);
        }
    }
    if (!boosting)
    {
        if (!(pp->jetpack_fuel_percent > 0.0f && has_jetpack) || !boost_active)
        {
            multi_object_backpack* bp = pp->backpack;
            if (bp == nullptr || bp->backpack_info->jetpack != true || bp->backpack_info->uses_fuel != true)
            {
                pp->jetpack_fuel_percent = 1.0f;
            }
            else
            {
                pp->jetpack_fuel_percent = bp->fuel;
            }
        }
        if (pp->pflags.jetpack_started)
        {
            pp->pflags.jetpack_started = 0;
            object_effect* effect = pp->jetpack_effect != 0xFFFFFFFF ? (object_effect*)World.object_from_handle(pp->jetpack_effect) : nullptr;
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            pp->jetpack_effect = 0xFFFFFFFF;
            if (pp->jetpack_foley != -1)
            {
                audiolib_play_fade_out(pp->jetpack_foley, Jetpack_loop_sound_fade_time);
                if (pp == Player)
                {
                    pp->jetpack_foley = audiolib_cue_play(Ga_cue_ids.jetpack_release_pc, nullptr, nullptr);
                }
                else
                {
                    pp->jetpack_foley = audiolib_cue_play_3d(Ga_cue_ids.jetpack_release_npc, &pp->pos, nullptr, nullptr, nullptr);
                }
            }
        }
    }
    if (old_fuel != pp->jetpack_fuel_percent && pp->jetpack_effect != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(pp->jetpack_effect);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->set_expression_variable(EFFECTS_VAR_JETPACK_FUEL_PERCENT, pp->jetpack_fuel_percent);
        }
    }
    if (pp == Player)
    {
        if (pp->pflags.jetpack_started && pp->jetpack_use_log_handle == 0)
        {
            const char pct_game_complete = gamelog_get_pct_complete();
            const int time_played = gamelog_get_time_played();
            const vector log_pos = gamelog_get_human_pos(pp);
            const char xp_level = gamelog_get_player_xp_level(pp);
            const char player_index = gamelog_get_player_index(pp);
            const int time = gamelog_get_logging_time();
            const int checksum = gamelog_get_player_checksum();
            pp->jetpack_use_log_handle = log_start_event_jetpack_used(checksum, time, player_index, xp_level, log_pos, time_played, pct_game_complete);
            return;
        }
        if (!pp->pflags.jetpack_started && pp->jetpack_use_log_handle != 0)
        {
            const unsigned int handle = pp->jetpack_use_log_handle;
            // Magic number: 0x1F, the jetpack used log event.
            if (log_record_event(0x1F))
            {
                log_stop_continuous_event(handle);
            }
            pp->jetpack_use_log_handle = 0;
        }
    }
}

// 0x00A10980 (:7018): Magic number: the first damage comes after 1000 ms.
void player_radiation_start(player* pp, float damage_per_second)
{
    if (pp == nullptr)
    {
        return;
    }
    pp->radiation_damage = damage_per_second;
    pp->radiation_timestamp.set(1000);
    pp->pflags.in_radiation = 1;
    if (pp->radiation_damage > 0.0f)
    {
        audiolib_cue_play(Ga_cue_ids.radiation_start, nullptr, nullptr);
        pp->radiation_foley = audiolib_cue_play(Ga_cue_ids.radiation_loop, nullptr, nullptr);
    }
}

// 0x00A109F0 (:7032)
void player_radiation_stop(player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    const int play_id = pp->radiation_foley;
    pp->pflags.in_radiation = 0;
    pp->radiation_damage = 0.0f;
    pp->radiation_timestamp.invalidate();
    audiolib_play_fade_out(play_id, -1);
    pp->radiation_foley = -1;
    audiolib_cue_play(Ga_cue_ids.radiation_kill, nullptr, nullptr);
}

// 0x00A85570 (:7077)
// Changes the zoom (off always; on with a weapon that is not reloading, blind firing, meleeing, flinching, in the
// inventory overlay, with a no-fine-aim backpack active, or scoping without ammo; a forced fine aim turret allows only
// fine aim). Scoping hides the local player (rail driver: every human is drawn through walls) and shows the scope HUD.
void player_set_zoom_state(player* pp, player_zoom_state zoom, bool instant_change)
{
    if (zoom == pp->zoom_state)
    {
        return;
    }
    if (zoom != PZS_OFF)
    {
        if (human_weapons_get_weapon_info(pp) == nullptr || human_weapons_is_reloading(pp, false) || human_combat_is_blind_firing(pp) ||
            pp->combat.cur_melee_combat_move != nullptr || pp->char_inst->is_playing_action((vanim_aliases)0x3E3))
        {
            return;
        }
        if (ovl_inventory_is_open() && !ovl_inventory_is_closing_or_closed())
        {
            return;
        }
        if (pp->backpack != nullptr && pp->backpack->activated == true && pp->backpack->backpack_info->no_fine_aim == true)
        {
            return;
        }
        if (zoom == PZS_SCOPE)
        {
            weapon* wp = pp->get_equipped_weapon();
            if (wp != nullptr && wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine() == 0)
            {
                return;
            }
        }
    }
    turret* tp = human_combat_get_manned_turret(pp);
    if (tp != nullptr && tp->t_info->flags.force_fine_aim && zoom != PZS_FINE_AIM)
    {
        return;
    }
    const player_zoom_state previous = pp->zoom_state;
    pp->previous_zoom_state = previous;
    pp->zoom_state = zoom;
    if (previous != zoom)
    {
        if (pp != Player)
        {
            return;
        }
        weapon* wp = pp->get_equipped_weapon();
        if (pp->previous_zoom_state == PZS_SCOPE)
        {
            if (zoom != PZS_SCOPE)
            {
                pp->set_opacity_plus_child_items(1.0f);
                pp->set_casts_transparent_shadows_plus_child_items(false);
                hud_ui_rail_driver_set_visible(false);
                hud_ui_sniper_set_visible(false);
            }
        }
        else if (zoom == PZS_SCOPE)
        {
            pp->set_opacity_plus_child_items(0.0f);
            pp->set_casts_transparent_shadows_plus_child_items(true);
            if (wp != nullptr)
            {
                if (wp->w_info->flags.penetrating_bullets)
                {
                    const int num = World.get_num_objects<human>();
                    for (int i = 0; i < num; i++)
                    {
                        // Magic number: render flag 8, the x-ray outline pass.
                        World.get_object<human>(i)->set_render_flags(8);
                    }
                    hud_ui_rail_driver_set_visible(true);
                }
                else
                {
                    hud_ui_sniper_set_visible(true);
                }
            }
        }
    }
    if (pp == Player)
    {
        camera_set_view_player(zoom, instant_change);
    }
}

// 0x00A10A50 (:7187)
bool player_is_zoomed(const player* pp)
{
    return pp->zoom_state != PZS_OFF && pp->zoom_state != PZS_SPRINT;
}

// 0x00A10A70 (:7193)
bool player_is_fine_aimed(const player* pp)
{
    return pp->zoom_state == PZS_FINE_AIM;
}

// 0x00A10A90 (:7199)
bool player_is_scoped(const player* pp)
{
    return pp != nullptr && pp->zoom_state == PZS_SCOPE;
}

// 0x00A10AB0 (:7205)
// A last equipped weapon that is gone becomes the second last; with neither, the inventory's sledgehammer (the last
// weapon walked over when there is none).
void player_make_last_equipped_weapon_valid(player* pp)
{
    if (pp->last_equipped_weapon != nullptr && pp->last_equipped_weapon->info != nullptr)
    {
        return;
    }
    if (pp->second_last_equipped_weapon != nullptr && pp->second_last_equipped_weapon->info != nullptr)
    {
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
        return;
    }
    inventory_item* first = pp->inventory;
    for (inventory_item* iip = first; iip != nullptr;)
    {
        if (iip->info != nullptr && iip->info->w_info != nullptr)
        {
            pp->last_equipped_weapon = iip;
            if (iip->info->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
            {
                return;
            }
        }
        iip = iip->next;
        if (iip == first)
        {
            return;
        }
    }
}

// 0x00A10B20 (:7240)
// A free selection slot among the first four.
bool player_has_room_in_inventory(const player* pp)
{
    bool used[4] = {false, false, false, false};
    const inventory_item* iip = pp->inventory;
    while (iip != nullptr)
    {
        if (iip->selection_slot < 4 && iip->selection_slot > -1)
        {
            used[iip->selection_slot] = true;
        }
        iip = iip->next;
        if (iip == pp->inventory)
        {
            break;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (!used[i])
        {
            return true;
        }
    }
    return false;
}

// Removes an inventory item that ran out of ammo (multiplayer servers only) from the inventory and the equip chain;
// a dropped non-inventory weapon falls (an added gravity force) and becomes opaque. Inlined in player_equip_item,
// player_equip_sledgehammer and player_meta_data::purchase_upgrade.
static void player_free_inventory_item(player* pp, inventory_item* iip)
{
    if (iip == pp->get_equipped_inv_item())
    {
        inventory_item* desired = pp->desired_equipped_inv_item;
        if (iip == desired)
        {
            pp->set_equipped_inv_item(pp->last_equipped_weapon);
            pp->desired_equipped_inv_item = pp->last_equipped_weapon;
            pp->last_equipped_weapon = pp->second_last_equipped_weapon;
            pp->second_last_equipped_weapon = nullptr;
        }
        else
        {
            pp->set_equipped_inv_item(desired);
        }
    }
    if (iip == pp->desired_equipped_inv_item)
    {
        pp->desired_equipped_inv_item = pp->get_equipped_inv_item();
    }
    if (iip == pp->last_equipped_weapon)
    {
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
    }
    if (iip == pp->second_last_equipped_weapon)
    {
        pp->second_last_equipped_weapon = nullptr;
    }
    if (pp->inventory == iip && iip->next == iip)
    {
        iip->next = nullptr;
        iip->prev = nullptr;
        pp->inventory = nullptr;
    }
    else
    {
        if (pp->inventory == iip)
        {
            pp->inventory = iip->next;
        }
        if (iip->prev != nullptr)
        {
            iip->prev->next = iip->next;
        }
        if (iip->next != nullptr)
        {
            iip->next->prev = iip->prev;
        }
        iip->next = nullptr;
        iip->prev = nullptr;
    }
    weapon* wp = nullptr;
    if (iip->weapon_handle != 0xFFFFFFFF)
    {
        wp = (weapon*)World.object_from_handle(iip->weapon_handle);
        if (wp == nullptr || wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON)
        {
            wp = nullptr;
        }
        else if (wp->w_info->flags.non_inventory)
        {
            // Magic number: gravity, 9.8 m/s^2.
            const vector gravity(0.0f, -9.8f, 0.0f);
            havok_body_add_custom_force(wp->havok_handle, gravity);
            wp->set_opacity_plus_child_items(1.0f);
        }
    }
    if (Multiplayer == true && multi_is_server() && pp->get_obj_type() == OT_HUMAN && pp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        multi_game_player_remove_weapon(pp, iip);
    }
    // Original: wp is read without a null check.
    if (pp == Player && wp->info->special_flags.high_res_texture_requested)
    {
        wp->info->special_flags.high_res_texture_request_unload = true;
    }
    iip->weapon_handle = 0xFFFFFFFF;
    inv_item_free(iip); // inlined
}

// 0x00AC2B60 (:7274)
// Equips an inventory item (unequips with none); a multiplayer server first removes an empty equipped weapon, and
// clients only request the slot. On a ladder nothing changes; in a vehicle the item becomes the last equipped one;
// equipping a weapon that cannot be used in cover leaves cover.
bool player_equip_item(player* pp, inventory_item* ip, item** /*unequipped_item*/, bool /*server_setting*/)
{
    if (ip == pp->get_equipped_inv_item())
    {
        return true;
    }
    gamelog_maybe_log_weapon_equipped(pp);
    inventory_item* current = pp->get_equipped_inv_item();
    if (Multiplayer && current != nullptr && current->weapon_handle != 0xFFFFFFFF)
    {
        weapon* wp = (weapon*)World.object_from_handle(current->weapon_handle);
        if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && !wp->has_ammo())
        {
            if (Multiplayer && !multi_is_server())
            {
                multi_game_player_send_remove_weapon(current);
            }
            else
            {
                const int count = current->count;
                if (!(Multiplayer == true && !multi_is_server()))
                {
                    current->count = current->count - count;
                    if (current->count < 1)
                    {
                        player_free_inventory_item(pp, current); // inlined
                    }
                }
            }
        }
    }
    if (Multiplayer == true && !multi_is_server())
    {
        // Magic number: slot 7, nothing equipped.
        pp->mp_desired_weapon_slot = ip == nullptr ? 7 : ip->selection_slot;
    }
    if (pp->cur_state == HS_LADDER)
    {
        return false;
    }
    if (ip == nullptr)
    {
        human_unequip_inventory_item(pp);
        return true;
    }
    if (!human_in_vehicle(pp, nullptr))
    {
        human_equip_inventory_item(pp, ip);
    }
    else
    {
        pp->second_last_equipped_weapon = pp->last_equipped_weapon;
        pp->last_equipped_weapon = ip;
    }
    if (pp->cur_state == HS_COVER && ip->weapon_handle != 0xFFFFFFFF)
    {
        weapon* wp = (weapon*)World.object_from_handle(ip->weapon_handle);
        if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && wp->w_info->flags.disable_player_cover)
        {
            player_exit_cover(pp, false);
        }
    }
    return true;
}

// 0x00ADF870 (:7348)
// The inventory starts over: the weapon is holstered at once, every item is deleted, the local player's high
// resolution weapon textures unload, the default inventory comes back and the default weapon (multiplayer: the
// player's default weapon) is equipped, else the first item; the default last weapon is remembered.
void player_reequip(player* pp)
{
    human_weapons_immediately_holster_weapon(pp);
    inventory_item* iip = pp->inventory;
    while (iip != nullptr)
    {
        inventory_item* next = iip->next;
        inventory_item* first = pp->inventory;
        human_delete_item_from_inventory(pp, iip, true, false);
        iip = next != first ? next : nullptr;
    }
    if (pp == Player)
    {
        for (int i = 0; i < Num_high_res_weapons_loaded; i++)
        {
            High_res_weapons_loaded[i]->special_flags.high_res_texture_request_unload = true;
        }
    }
    pp->last_equipped_weapon = nullptr;
    pp->second_last_equipped_weapon = nullptr;
    pp->desired_equipped_inv_item = nullptr;
    pp->grenade_weapon = nullptr;
    pp->offhand_projectile_handle = 0xFFFFFFFF;
    human_set_default_inventory(pp);
    const inv_item_info* equip_info;
    if (Multiplayer)
    {
        equip_info = multi_game_player_get_default_weapon(pp->mp_data);
    }
    else
    {
        equip_info = pp->h_info->get_default_equip_item(); // inlined (single player: the field)
    }
    inventory_item* equip = nullptr;
    inventory_item* first = pp->inventory;
    for (inventory_item* item = first; item != nullptr;)
    {
        if (item->info == equip_info)
        {
            equip = item;
            break;
        }
        item = item->next;
        if (item == first)
        {
            break;
        }
    }
    if (equip == nullptr)
    {
        inventory_item* of_class = nullptr;
        if (equip_info != nullptr && equip_info->w_info != nullptr)
        {
            of_class = human_has_weapon_of_type(pp, equip_info->w_info->weapon_class);
        }
        equip = of_class != nullptr ? of_class : first;
    }
    player_equip_item(pp, equip, nullptr, false);
    pp->desired_equipped_inv_item = equip;
    const inv_item_info* last_info = pp->h_info->get_default_last_equip_item(); // inlined
    first = pp->inventory;
    for (inventory_item* item = first; item != nullptr;)
    {
        if (item->info == last_info)
        {
            pp->second_last_equipped_weapon = nullptr;
            pp->last_equipped_weapon = item;
            return;
        }
        item = item->next;
        if (item == first)
        {
            break;
        }
    }
    inventory_item* last = nullptr;
    if (last_info != nullptr && last_info->w_info != nullptr)
    {
        last = human_has_weapon_of_type(pp, last_info->w_info->weapon_class);
    }
    pp->second_last_equipped_weapon = nullptr;
    pp->last_equipped_weapon = last;
}

// 0x00ADFA10 (:7437)
// A player comes back to life at a position (a null position: where it is): visible, its nano effects and blood
// decals gone, off the jetpack and the ladder, its hit records cleared, out of a vehicle, teleported (single player:
// to a place to stand within 6 m; multiplayer: directly), at full health with its knockdown hits, out of a ragdoll and
// a crouch, a full jetpack, its one-shot animations cleared, standing idle with a standing controller and the default
// collision filter. Its props are destroyed (single player keeps the misc accessories and the jetpack), the backpack
// knows, the inventory starts over, the spine bend, jump start and death time reset, the animation states clear and the
// multiplayer action ids start at 0.
void player_respawn(player* pp, vector& respawn_pos, matrix& respawn_orient, bool do_teleport)
{
    if (respawn_pos.is_null())
    {
        respawn_pos = pp->pos;
        respawn_orient = pp->orient;
    }
    object_set_visibility_plus_children(pp, true);
    pp->render_alpha = 1.0f;
    for (int i = 0; i < 16; i++)
    {
        material_effects_remove(pp->nano_mat_fx[i]);
        pp->nano_mat_fx[i] = 0xFFFFFFFF;
        pp->nano_cb_info[i].human_nano_index = -1;
        pp->nano_cb_info[i].killed_by_nano = false;
        pp->nano_cb_info[i].target_handle = 0xFFFFFFFF;
    }
    pp->hflags.fading_in = 0;
    pp->hflags.fading_out_from_nano = 0;
    pp->hflags.fading_out = 0;
    pp->remove_blood_decals();
    if (pp == Player)
    {
        control_gameplay_clear_crouch();
    }
    else
    {
        human_set_xray_visible(pp, true);
    }
    player_update_jetpack(pp, false);
    if (pp->hflags.is_on_ladder)
    {
        human_dismount_ladder(pp);
    }
    pp->hflags.cancelling_melee_attack = 0;
    pp->hflags.killed_by_killzone = 0;
    pp->last_fire_time.invalidate();
    pp->combat.melee_delay_timer.invalidate();
    pp->combat.last_hit_by_handle = 0xFFFFFFFF;
    pp->combat.last_hit_by_other_handle = 0xFFFFFFFF;
    pp->combat.last_hit_by_weapon_handle = 0xFFFFFFFF;
    pp->combat.last_hit_by_other_weapon_handle = 0xFFFFFFFF;
    pp->combat.last_hit_by_damage_type = DBT_NONE;
    pp->combat.last_hit_by_other_damage_type = DBT_NONE;
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !pp->obj_flags.destroyed && !objp->obj_flags.destroyed)
        {
            // Not ported (no multiplayer map places vehicles, multiplayer.md 2.2): the original exits only once the
            // human's interaction (vi_instance::find_human) is past entering and not in state 7.
            vi_exit_data exit_data;
            exit_data.flags.teleport = 1;
            exit_data.flags.maintain_position = do_teleport ? 1 : 0;
            vehicle_interaction_request_exit(pp, &exit_data);
        }
    }
    if (do_teleport)
    {
        if (!Multiplayer)
        {
            // Magic number: a place to stand within 6 m.
            human_teleport(pp, respawn_pos, respawn_orient, 6.0f, false);
        }
        else
        {
            human_teleport_unsafe(pp, respawn_pos, respawn_orient);
        }
    }
    const float max_hit_points = (float)pp->max_hit_points;
    pp->health_restore_hit_points = pp->health_restore_hit_points + max_hit_points;
    if (max_hit_points < pp->health_restore_hit_points)
    {
        pp->health_restore_hit_points = max_hit_points;
    }
    pp->health_restore_min_hit_points = 0.0f;
    pp->hit_points = pp->health_restore_hit_points;
    pp->knockdown_hits = (float)pp->max_knockdown_hits;
    if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        pp->hflags.dont_deform_bones = 0;
    }
    else
    {
        human_ragdoll_stop(pp, false);
    }
    if (pp->cur_stance == HST_CROUCH)
    {
        human_move_stop_crouching(pp);
    }
    pp->jetpack_fuel_percent = 1.0f;
    if (pp->char_inst->aim_handle != -1)
    {
        animlib_clear_non_looping_animations(pp->char_inst->aim_handle);
    }
    if ((pp->get_sub_type() != OT_SUB_HUMAN_NPC || pp->is_killed() || pp->cur_state != HS_COVER) && (!pp->hflags.is_on_ladder || pp->get_sub_type() != OT_SUB_HUMAN_NPC) &&
        pp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE && pp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        human_set_movement_mode(pp, HMM_NONE, HMSM_NONE); // inlined
    }
    pp->move_speed = -1.0f;
    pp->cur_state = HS_STANDARD;
    pp->cur_movement_state = HMS_IDLE;
    if (pp->rdd.is_loaded())
    {
        pp->rdd.get_data()->controller->set_shape(CSM_STAND, true);
        pp->rdd.get_data()->controller->set_velocity(ZERO_VECTOR);
        const unsigned int filter = get_default_collision_filter(pp);
        hkpShapePhantom* phantom = pp->rdd.get_data()->controller->m_phantom;
        if (phantom != nullptr)
        {
            phantom->getCollidableRw()->getBroadPhaseHandle()->m_collisionFilterInfo = filter;
            havok_phantom_update_collision_filter(phantom, true);
        }
    }
    pp->hflags.death_func_done = 0;
    if (pp == Player)
    {
        camera_level_init_post();
        hud_reset_damage_indicators();
    }
    object* child = pp->child_ptr;
    while (child != nullptr)
    {
        object* next = child->child_next;
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            if (ip->m_prop_type != PROP_TYPE_NONE && !ip->item_flags.character_prop &&
                ((ip->m_prop_type != PROP_TYPE_JETPACK && ip->m_prop_type != PROP_TYPE_ACCESSORY_MISC) || Multiplayer))
            {
                // Original: the next child is read after the destroy (a destroyed child stays linked until later).
                World.destroy_object(child, false);
                next = child->child_next;
            }
        }
        child = next;
        if (child == pp->child_ptr)
        {
            break;
        }
    }
    if (Multiplayer && pp->backpack != nullptr)
    {
        pp->backpack->event_player_respawned();
    }
    player_reequip(pp);
    pp->spinebend_data.flags.disabled = 0;
    pp->code_driven_start_jump = false;
    pp->dead_timestamp.invalidate();
    vanim_clear_states(pp->char_inst->vp);
    pp->mp_flags.melee_primary_id = 0;
    pp->mp_flags.throw_attack_id = 0;
    pp->mp_flags.firearm_trigger_up_id = 0;
    pp->mp_flags.firearm_trigger_down_id = 0;
    pp->mp_flags.jump_id = 0;
    pp->mp_flags.melee_firearm_id = 0;
    pp->mp_flags.melee_tertiary_id = 0;
    pp->mp_flags.melee_secondary_id = 0;
}

// 0x00ADFED0 (:7603): at a player start (multiplayer and wrecking crew only); the mode hears of it
void player_respawn(player* pp, int player_start_index, bool do_teleport)
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return;
    }
    // Original: the index is not checked.
    player_start* start = World.get_object<player_start>(player_start_index);
    if (start == nullptr)
    {
        return;
    }
    player_respawn(pp, start->pos, start->orient, do_teleport);
    if (Multiplayer)
    {
        multi_gameplay_mode_player_respawn(pp, player_start_index);
    }
}

// 0x00A34620 (:7633)
// The local player died: the death camera, the district morale penalty (outside missions and Demolitions Master), the
// death sound and time, the death shake, and the mission is told.
static void local_player_is_killed(player* pp)
{
    GAME_TRACE("player", "killed at (%.1f %.1f %.1f)", pp->pos.x, pp->pos.y, pp->pos.z);
    camera_view_set_default();
    if (!wrecking_crew_is_active())
    {
        camera_push_mode(CAMERA_DEATH_MODE);
    }
    if (!Multiplayer)
    {
        if (!mission_is_active() && !activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER))
        {
            gfm_change_district_morale(difficulty_get_value(DIFFICULTY_MORALE_PLAYER_DEATH, NUM_DIFFICULTY_LEVELS), nullptr, nullptr, nullptr);
        }
        if (!Multiplayer)
        {
            audiolib_cue_play(Ga_cue_ids.general_player_died, nullptr, nullptr);
            if (!Multiplayer && !wrecking_crew_is_active() && Player != nullptr)
            {
                Player->meta_data.set_last_death_time(); // inlined
            }
        }
    }
    camera_shake_play("player_died", 1.0f, false);
    player_stop_material_effect(pp);
    pp->hflags.in_air_melee = 0;
    mission_player_died();
}

// 0x00A346F0 (:7682)
// The death timestamp (4 s) starts once, unless a load is pending.
void player_is_killed(player* pp)
{
    if (pp->dead_timestamp.valid())
    {
        return;
    }
    if (gameseq_get_pending_state() == GS_LOAD)
    {
        return;
    }
    pp->dead_timestamp.set(4000);
    if (pp != nullptr && pp == Player)
    {
        local_player_is_killed(pp);
    }
}

// 0x00A857A0 (:8165)
// The top speed: the character's (or the multiplayer class's), blended to the sprint speed over the 300 ms before the
// sprint delay ends (doubled by the super sprint cheat), overridden by the tweaks; scaled by the multiplayer settings,
// the backpack, the weapon, the zoom or crouch (multiplayer: moving sideways to the camera) and a charge backpack's
// top speed. Falling or jumping in single player keeps the horizontal speed (20 m/s at most).
void player_get_max_movement_speeds(player* pp, float& max_movement_speed, float& max_movement_speed_base)
{
    float speed = Multiplayer ? multi_game_classes_get_default_class()->max_speed : pp->h_info->get_max_speed(); // the field (single player)
    max_movement_speed = speed;
    max_movement_speed_base = speed;
    float scale = 1.0f;
    const int until = pp->sprint_delay_timestamp.time_until();
    if (until > 0)
    {
        float sprint = Player_max_sprint_speed;
        if (stats_cheat_enabled(CHEATS_SUPER_SPRINTING))
        {
            sprint = Player_max_sprint_speed * 2.0f;
        }
        const float base = Multiplayer ? multi_game_classes_get_default_class()->max_speed : pp->h_info->get_max_speed();
        // Magic number: the sprint blends in over 300 ms.
        max_movement_speed = (float)((1.0 - (double)(float)until / 300.0) * (double)base + (double)sprint * ((double)(float)until / 300.0));
    }
    if (!Multiplayer)
    {
        if (0.0f < Player_max_movement_speed_override)
        {
            max_movement_speed = Player_max_movement_speed_override;
        }
    }
    else
    {
        if (0.0f < Mp_player_max_movement_speed_override)
        {
            max_movement_speed = Mp_player_max_movement_speed_override;
        }
        float stat = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_MOVE_SPEED);
        if (multi_gameplay_mode_get() == MGPM_CAPTURE_THE_FLAG && player_has_flag(pp))
        {
            stat = (float)(((double)(float)Mp_server_options.mode_options.capture_the_flag.flag_carrier_speed / 100.0) * (double)stat);
        }
        max_movement_speed_base = max_movement_speed;
        scale = (float)(((double)(float)Mp_server_options.generic_options.movement_speed / 100.0) * (double)stat);
    }
    if (pp->backpack != nullptr)
    {
        const multi_backpack_info::abilities* ab = pp->backpack->activated == true ? &pp->backpack->backpack_info->active_abilities : &pp->backpack->backpack_info->passive_abilities;
        scale = ab->movement_speed_factor * scale;
    }
    turret* tp = human_combat_get_manned_turret(pp);
    unsigned int weapon_handle = 0xFFFFFFFF;
    bool has_weapon = true;
    if (tp == nullptr)
    {
        if (pp->get_equipped_inv_item() == nullptr)
        {
            has_weapon = false;
        }
        else
        {
            weapon_handle = pp->get_equipped_inv_item()->weapon_handle;
        }
    }
    else
    {
        weapon_handle = tp->get_weapon_inst() != nullptr ? tp->get_weapon_inst()->get_handle() : 0xFFFFFFFF;
    }
    if (has_weapon)
    {
        weapon* wp = weapon_handle != 0xFFFFFFFF ? (weapon*)World.object_from_handle(weapon_handle) : nullptr;
        if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            scale = wp->w_info->player_move_speed_multiplier * scale;
        }
    }
    if (pp->zoom_state == PZS_OFF || pp->zoom_state == PZS_SPRINT)
    {
        if (pp->cur_stance == HST_CROUCH)
        {
            scale = (Multiplayer == true ? Mp_player_move_multiplier_crouch : Player_move_multiplier_crouch) * scale;
        }
        else if (Multiplayer == true)
        {
            vector vel;
            if (pp->rdd.is_loaded())
            {
                pp->rdd.get_data()->controller->get_velocity(vel);
            }
            else
            {
                vel.zero();
            }
            if (!vel.is_null())
            {
                vel.flatten_normalize_safe();
                vector cam;
                cam = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
                float dot = cam.y * vel.y + vel.x * cam.x + cam.z * vel.z;
                if (dot - 0.0f < 0.0f)
                {
                    dot = 0.0f;
                }
                scale = (float)(((1.0 - (double)dot) * ((double)Mp_player_move_multiplier_side - 1.0) + 1.0) * (double)scale);
            }
        }
    }
    else
    {
        scale = (Multiplayer == true ? Mp_player_move_multiplier_zoom : Player_move_multiplier_zoom) * scale;
    }
    if (pp->backpack != nullptr)
    {
        const multi_backpack_info::abilities* ab = pp->backpack->activated == true ? &pp->backpack->backpack_info->active_abilities : &pp->backpack->backpack_info->passive_abilities;
        if (ab->charge == true)
        {
            scale = 1.0f;
            max_movement_speed = ab->charge_top_speed;
        }
    }
    max_movement_speed = max_movement_speed * scale;
    max_movement_speed_base = max_movement_speed_base * scale;
    if (!Multiplayer && (pp->cur_state == HS_FALL || pp->cur_state == HS_JUMP))
    {
        float horizontal = (float)sqrt((double)pp->actual_velocity.x * pp->actual_velocity.x + (double)pp->actual_velocity.z * pp->actual_velocity.z);
        if (0.0f <= max_movement_speed - horizontal)
        {
            horizontal = max_movement_speed;
        }
        max_movement_speed = horizontal;
        // Magic number: 20 m/s at most in the air.
        if (0.0f <= horizontal - 20.0f)
        {
            horizontal = 20.0f;
        }
        max_movement_speed = horizontal;
    }
}

// 0x00A85D10 (:8267)
// Direct control toward new_dest_point at speed (0 to 1 of the top speed): the movement velocity accelerates toward
// it in x and z (Player_acceleration, faster for a bigger change, half off the ground) without overshooting, the
// player travels directly by it, and the move speed sets the run blend (above the base speed, up to 1.5 while
// sprinting). The steering follows the movement (a stopping combat-ready player keeps facing), or a blocking
// animation's own direction.
void player_process_normal_movement(player* pp, vector new_dest_point, float speed)
{
    vector wanted(new_dest_point.x - pp->pos.x, new_dest_point.y - pp->pos.y, new_dest_point.z - pp->pos.z);
    if (pp->block_movement_anim != -1)
    {
        const anim* ap = anim_get_anim_from_handle(vanim_get_anim_handle(pp->char_inst->vp, pp->block_movement_anim));
        // Editor flag 0x10: the animation stops direct control.
        if (ap != nullptr && (ap->editor_flags & 0x10) != 0)
        {
            wanted.zero();
            speed = 0.0f;
        }
    }
    float max_speed;
    float max_speed_base;
    player_get_max_movement_speeds(pp, max_speed, max_speed_base);
    if (0.0001f < speed && !wanted.is_null())
    {
        wanted.normalize();
        if (speed - 0.0f < 0.0f)
        {
            speed = 0.0f;
        }
        if (0.0f <= speed - 1.0f)
        {
            speed = 1.0f;
        }
        const float scale = speed * max_speed;
        wanted.y = scale * wanted.y;
        wanted.x = scale * wanted.x;
        wanted.z = scale * wanted.z;
    }
    pp->direct_control_vel = wanted;
    vector last = pp->movement_velocity;
    const vector vel_dir = pp->velocity.copy_normalize_safe();
    const vector move_dir = pp->movement_velocity.copy_normalize_safe();
    // A reversal starts from rest.
    if (vel_dir.y * move_dir.y + move_dir.x * vel_dir.x + vel_dir.z * move_dir.z < 0.0f)
    {
        last.zero();
    }
    const vector target = wanted;
    pp->pflags.moving_to_a_stop = 0;
    // Original: moving_to_a_stop was just cleared, so only a remaining movement velocity counts.
    if (pp->direct_control_vel.is_null() && ((pp->pflags.moving_to_a_stop && pp->hflags.moving_and_transitioning_states) || !last.is_null()))
    {
        pp->pflags.moving_to_a_stop = 1;
    }
    const float dx = target.x - last.x;
    const float dy = target.y - last.y;
    const float dz = target.z - last.z;
    float diff = sqrtf(dx * dx + dy * dy + dz * dz);
    float accel;
    if (Multiplayer == true)
    {
        accel = Mp_player_acceleration * multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_MOVE_ACCELERATION);
    }
    else
    {
        accel = Player_acceleration;
    }
    if (pp->block_movement_anim != -1 && pp->block_movement_type == BLOCK_MOVEMENT_ORIENT)
    {
        // Moving against a blocking animation's direction accelerates at a quarter.
        vector block_dir = pp->block_steering_vector;
        block_dir.rotate_around_y_axis(pp->block_steering_heading_offset);
        const vector control_dir = pp->direct_control_vel.copy_normalize_safe();
        float along = control_dir.x * block_dir.x + control_dir.y * block_dir.y + control_dir.z * block_dir.z;
        if (0.0f <= 0.0f - along)
        {
            along = 0.0f;
        }
        accel = (1.0f - along) * 0.25f * accel + along * accel;
    }
    float step = (accel * 0.5f * diff + accel) * Frametime;
    if (pp->rdd.is_loaded() && !pp->rdd.get_data()->controller->flags.is_supported)
    {
        step = step * 0.5f;
    }
    float step_x = 0.0f;
    float step_z;
    if (diff <= 0.0f)
    {
        step_z = 0.0f;
    }
    else
    {
        step_x = (fabsf(target.x - last.x) * step) / diff;
        step_z = (fabsf(target.z - last.z) * step) / diff;
    }
    bool reached;
    float new_x;
    if (target.x < last.x)
    {
        new_x = last.x - step_x;
        reached = target.x < new_x;
    }
    else
    {
        new_x = last.x + step_x;
        reached = new_x < target.x;
    }
    // Original: "reached" is true while the target is not reached yet (it takes the step, else the target).
    float move_x = target.x;
    if (reached || new_x == target.x)
    {
        move_x = new_x;
    }
    float new_z;
    if (target.z < last.z)
    {
        new_z = last.z - step_z;
        reached = target.z < new_z;
    }
    else
    {
        new_z = last.z + step_z;
        reached = new_z < target.z;
    }
    float move_z = target.z;
    if (reached || new_z == target.z)
    {
        move_z = new_z;
    }
    pp->movement_velocity = vector(move_x, target.y, move_z);
    vector dest(move_x + pp->pos.x, pp->pos.y + target.y, pp->pos.z + move_z);
    human_move_direct_travel_to(pp, dest);
    const vector& mv = pp->movement_velocity;
    float move_speed = sqrtf(mv.x * mv.x + mv.y * mv.y + mv.z * mv.z);
    if (move_speed - 0.0f < 0.0f)
    {
        move_speed = 0.0f;
    }
    if (0.0f <= move_speed - max_speed)
    {
        move_speed = max_speed;
    }
    pp->move_speed = move_speed;
    float blend_speed = pp->actual_velocity_mag;
    const object* ground = World.object_from_handle(pp->ground_obj_handle);
    if (ground != nullptr && ground->get_obj_type() == OT_VEHICLE)
    {
        blend_speed = pp->move_speed;
    }
    float blend;
    if (blend_speed <= max_speed_base)
    {
        blend = blend_speed / max_speed_base;
    }
    else
    {
        blend = (blend_speed - max_speed_base) / (max_speed - max_speed_base);
        if (0.0f <= blend - 1.0f)
        {
            blend = 1.0f;
        }
        blend = blend * 0.5f + 1.0f;
        if (pp->sprint_delay_timestamp.elapsed() && 0.0f <= blend - 1.0f)
        {
            blend = 1.0f;
        }
    }
    // Magic number: 0x409, the blend tree control input of the movement speed.
    vanim_set_blend_tree_control_input(pp->char_inst->vp, 0x409, blend);
    if (!pp->movement_velocity.is_null())
    {
        pp->movement_velocity.normalize();
        const float s = pp->move_speed;
        pp->movement_velocity.x = pp->movement_velocity.x * s;
        pp->movement_velocity.y = s * pp->movement_velocity.y;
        pp->movement_velocity.z = pp->movement_velocity.z * s;
    }
    if (human_combat_get_manned_turret(pp) == nullptr && pp->ladder_handle == 0xFFFFFFFF)
    {
        if (!pp->pflags.disable_controls && pp->movement_mode != HMM_PATHFINDING_TO_VEHICLE)
        {
            vector old_steering = pp->steering_vector;
            old_steering.rotate_around_y_axis(pp->steering_heading_offset);
            pp->steering_vector = vector(pp->turnto_target.x - pp->pos.x, pp->turnto_target.y - pp->pos.y, pp->turnto_target.z - pp->pos.z);
            const vector& s = pp->steering_vector;
            if (0.0f < s.x * s.x + s.y * s.y + s.z * s.z)
            {
                pp->steering_vector.normalize();
            }
            if (pp->pflags.moving_to_a_stop && pp->steering_vector.is_null() && human_is_combat_ready(pp))
            {
                pp->steering_vector = old_steering;
            }
        }
        if (pp->block_movement_anim != -1 && pp->block_movement_type == BLOCK_MOVEMENT_ORIENT && !human_is_in_air(pp))
        {
            pp->steering_vector = pp->block_steering_vector;
            pp->steering_vector.rotate_around_y_axis(pp->block_steering_heading_offset);
            if (pp->hflags.block_forced_movement)
            {
                // Magic number: forced movement goes at 2.5 m/s at least.
                float forced = pp->move_speed;
                if (forced - 2.5f < 0.0f)
                {
                    forced = 2.5f;
                }
                pp->move_speed = forced;
                if (pp->movement_velocity.is_null())
                {
                    const float s = pp->move_speed;
                    pp->movement_velocity = vector(pp->steering_vector.x * s, pp->steering_vector.y * s, pp->steering_vector.z * s);
                }
            }
        }
    }
}

// 0x00A347B0 (:8529). LTCG: the dispositions are read with the player's team in EDI and the human's in ESI.
bool local_player_is_friendly(const human* hp)
{
    if (Player == nullptr || hp == nullptr)
    {
        return false;
    }
    if (Player == (const player*)hp)
    {
        return true;
    }
    return (int)human_teams_get_disposition_internal(Player->get_team(), hp->get_team()) > TEAM_DISPOSITION_NONE;
}

// 0x00A34800 (:8551)
// Refills the ammo of the local player's weapons that use it (except those never in the weapon cabinet).
void local_player_fill_ammo()
{
    if (Multiplayer || Player == nullptr)
    {
        return;
    }
    inventory_item* iip = Player->inventory;
    while (iip != nullptr)
    {
        weapon* wp = iip->weapon_handle != 0xFFFFFFFF ? (weapon*)World.object_from_handle(iip->weapon_handle) : nullptr;
        if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && wp->uses_ammo() && !wp->w_info->flags.never_in_cabinet)
        {
            wp->fill_ammo();
        }
        iip = iip->next;
        if (iip == Player->inventory)
        {
            break;
        }
    }
}

// A backpack fade (out or in, the flag given) that continues from the opacity a running opposite fade reached.
// Inlined in local_player_fade_out_backpack and local_player_fade_in_backpack.
static void local_player_fade_backpack(player* pp, int fade_time_ms, bool resume)
{
    timestamp_percent& fade = pp->fade_backpack_time;
    if (resume && fade.valid() && !fade.elapsed())
    {
        float percent = fade.percent();
        if (percent - 0.0f < 0.0f)
        {
            percent = 0.0f;
        }
        if (0.0f <= percent - 1.0f)
        {
            percent = 1.0f;
        }
        fade.set(fade_time_ms);
        const float total = (float)fade_time_ms;
        float done = (float)((double)total - (double)percent * (double)total);
        if (done - 0.0f < 0.0f)
        {
            done = 0.0f;
        }
        if (0.0f <= done - total)
        {
            done = total;
        }
        fade.deduct_time((int)done);
        return;
    }
    fade.set(fade_time_ms);
}

// 0x00A10F20 (:8579)
void local_player_fade_out_backpack(int fade_time_ms)
{
    if (fade_time_ms < 0 || Player == nullptr || Player->pflags.backpack_fade_out)
    {
        return;
    }
    Player->pflags.backpack_fade_out = 1;
    local_player_fade_backpack(Player, fade_time_ms, Player->pflags.backpack_fade_in); // inlined
}

// 0x00A11060 (:8605)
// Original: a fade in starts only while a fade out runs; the fade out flag stays set until the fade in completes.
void local_player_fade_in_backpack(int fade_time_ms)
{
    if (fade_time_ms < 0 || Player == nullptr || Player->pflags.backpack_fade_in || !Player->pflags.backpack_fade_out)
    {
        return;
    }
    Player->pflags.backpack_fade_in = 1;
    local_player_fade_backpack(Player, fade_time_ms, true); // inlined
}

// 0x00A111F0 (:8687)
// The weapon a pickup would replace: none with a non-inventory item in the hands, else the equipped weapon, else the
// last one, unless it is the class's unique weapon (single player: the slot 3 weapon).
inventory_item* player_get_swap_weapon(const player* pp)
{
    if (pp != nullptr && pp->equipped_inv_item != nullptr && pp->equipped_inv_item->info != nullptr && pp->equipped_inv_item->info->w_info != nullptr &&
        pp->equipped_inv_item->info->w_info->flags.non_inventory)
    {
        return nullptr;
    }
    // Original: a null player is read through here.
    inventory_item* equipped = pp->equipped_inv_item;
    if (equipped != nullptr)
    {
        const bool unique = Multiplayer ? multi_game_classes_get_default_class()->is_unique_weapon_info(equipped->info) : equipped->selection_slot == 3;
        if (!unique)
        {
            return equipped;
        }
    }
    inventory_item* last = pp->last_equipped_weapon;
    if (last != nullptr)
    {
        const bool unique = Multiplayer ? multi_game_classes_get_default_class()->is_unique_weapon_info(last->info) : last->selection_slot == 3;
        if (!unique)
        {
            return last;
        }
    }
    return nullptr;
}

// 0x00AC3000 (:8958)
// Equips the sledgehammer (player_equip_item inlined) and refreshes the inventory overlay for the local player.
void player_equip_sledgehammer(player* pp)
{
    inventory_item* iip = pp->inventory;
    while (iip != nullptr)
    {
        // Original: every inventory item is assumed to be a weapon.
        if (iip->info->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
        {
            if (iip != pp->get_equipped_inv_item())
            {
                player_equip_item(pp, iip, nullptr, false); // inlined
            }
            break;
        }
        iip = iip->next;
        if (iip == pp->inventory)
        {
            break;
        }
    }
    if (pp == Player)
    {
        ovl_inventory_update_avail_weapons();
    }
}

// 0x00A11300
// Demolitions Master swaps the inventory out: the save copies the backup taken when it started.
void player::save_inventory_from_activity_backup(cfile* save_file)
{
    activity_inventory_save_file = cf_open(activity_inventory_buffer, sizeof(activity_inventory_buffer), "rb", PLATFORM_PC);
    if (activity_inventory_save_file == nullptr)
    {
        return;
    }
    int num_items = cf_read_int32(activity_inventory_save_file);
    cf_write_int32(num_items, save_file);
    for (; num_items > 0; num_items--)
    {
        const int slot = cf_read_int32(activity_inventory_save_file);
        char name[256];
        cf_read_string(name, sizeof(name), activity_inventory_save_file);
        const int count = cf_read_int32(activity_inventory_save_file);
        const int ammo = cf_read_int32(activity_inventory_save_file);
        cf_write_int32(slot, save_file);
        cf_write_string(name, save_file);
        cf_write_int32(count, save_file);
        cf_write_int32(ammo, save_file);
    }
    cf_close(activity_inventory_save_file);
}

// 0x00A34AD0 (:9114)
// Per item its slot, name, count and ammo (a weapon's magazine plus reserve), read back by load_inventory.
void player::save_inventory(cfile* save_file)
{
    if (activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER) && !is_killed())
    {
        save_inventory_from_activity_backup(save_file);
        return;
    }
    int num_items = 0;
    for (inventory_item* iip = inventory; iip != nullptr;)
    {
        num_items++;
        iip = iip->next;
        if (iip == inventory)
        {
            break;
        }
    }
    cf_write_int32(num_items, save_file);
    for (inventory_item* iip = inventory; iip != nullptr;)
    {
        cf_write_int32(iip->selection_slot, save_file);
        cf_write_string(iip->info->name, save_file);
        cf_write_int32(iip->count, save_file);
        int ammo = 0;
        if (iip->weapon_handle != 0xFFFFFFFF)
        {
            weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
            if (wp != nullptr)
            {
                ammo = wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine();
            }
        }
        cf_write_int32(ammo, save_file);
        iip = iip->next;
        if (iip == inventory)
        {
            break;
        }
    }
}

// 0x00AC3370 (:9149)
// The saved inventory: per item its slot, name, count and ammo (at least the starting magazines of cabinet weapons
// unless Demolitions Master overrides); then the default weapon (or one of its class, or the last equipped one) is
// equipped and the default last weapon becomes the last equipped one.
bool player::load_inventory(cfile* save_file, bool demolitions_master_override)
{
    int num_items = cf_read_int32(save_file);
    while (num_items > 0)
    {
        const int slot = cf_read_int32(save_file);
        char name[256];
        cf_read_string(name, 256, save_file);
        const int count = cf_read_int32(save_file);
        int ammo = cf_read_int32(save_file);
        if (!demolitions_master_override)
        {
            for (int i = 0; i < Num_inv_item_info; i++)
            {
                if (v_stricmp(Inv_item_info[i].name, name) == 0)
                {
                    const weapon_info* wi = Inv_item_info[i].w_info;
                    if (wi != nullptr && !wi->flags.never_in_cabinet)
                    {
                        const int start = (int)wi->magazine_start_num * (int)wi->magazine_size;
                        if (ammo <= start)
                        {
                            ammo = start;
                        }
                    }
                    break;
                }
            }
        }
        const inv_item_info* info = nullptr;
        for (int i = 0; i < Num_inv_item_info; i++)
        {
            if (v_stricmp(Inv_item_info[i].name, name) == 0)
            {
                info = &Inv_item_info[i];
                break;
            }
        }
        human_add_item_to_inventory(this, info, count, ammo, slot, true, demolitions_master_override, true);
        num_items--;
    }
    const inv_item_info* default_equip;
    if (Multiplayer == true)
    {
        default_equip = multi_game_player_get_default_weapon(mp_data);
    }
    else
    {
        default_equip = h_info->get_default_equip_item(); // the field (single player)
    }
    inventory_item* equip = human_has_item(this, default_equip);
    if (equip == nullptr && default_equip != nullptr && default_equip->w_info != nullptr)
    {
        equip = human_has_weapon_of_type(this, default_equip->w_info->weapon_class);
    }
    if (equip == nullptr)
    {
        equip = last_equipped_weapon;
    }
    if (equip != nullptr)
    {
        // the equipped one leaves the last equipped slots
        inventory_item* iip = inventory;
        while (iip != nullptr)
        {
            if (iip != equip)
            {
                if (last_equipped_weapon == equip)
                {
                    last_equipped_weapon = iip;
                }
                else
                {
                    if (second_last_equipped_weapon != equip)
                    {
                        break;
                    }
                    if (iip != last_equipped_weapon)
                    {
                        second_last_equipped_weapon = iip;
                        break;
                    }
                }
            }
            iip = iip->next;
            if (iip == inventory || iip == nullptr)
            {
                break;
            }
        }
        player_equip_item(this, equip, nullptr, false);
    }
    const inv_item_info* default_last = Multiplayer ? multi_game_classes_get_default_class()->default_last_equip_item : h_info->get_default_last_equip_item();
    inventory_item* last = human_has_item(this, default_last);
    if (last != nullptr && last != desired_equipped_inv_item)
    {
        last_equipped_weapon = last;
    }
    return true;
}

// 0x00A112B0 (:9040)
// The camera a player aims with: the game camera for the local player, else the camera its client reports (multiplayer).
// Nothing for no player.
void player_get_camera_pos_and_orient(const player* pp, const vector** pos, const matrix** orient)
{
    if (pp == nullptr)
    {
        return;
    }
    if (pp == Player)
    {
        if (pos != nullptr)
        {
            *pos = &Rfg_camera.real_pos;
        }
        if (orient != nullptr)
        {
            *orient = &Rfg_camera.real_orient;
        }
        return;
    }
    if (pos != nullptr)
    {
        *pos = &pp->mp_camera_pos;
    }
    if (orient != nullptr)
    {
        *orient = &pp->mp_camera_orient;
    }
}

// 0x00A11400 (:9355)
// Removes the material effects (and the head's unless it fades from nano).
bool player_stop_material_effect(player* pp)
{
    bool removed = false;
    if (pp == nullptr)
    {
        // Original: writes current_mat_fx through a null player (address 0x1160).
        return removed;
    }
    for (int i = 0; i < 16; i++)
    {
        if (pp->mat_fx_handle[i] != 0xFFFFFFFF)
        {
            material_effects_remove(pp->mat_fx_handle[i]);
            pp->mat_fx_handle[i] = 0xFFFFFFFF;
            removed = true;
        }
    }
    if (pp->mat_fx_handle_for_head_skin != 0xFFFFFFFF && !pp->hflags.fading_out_from_nano)
    {
        material_effects_remove(pp->mat_fx_handle_for_head_skin);
        pp->mat_fx_handle_for_head_skin = 0xFFFFFFFF;
        removed = true;
    }
    pp->current_mat_fx = 0xFFFFFFFF;
    return removed;
}

// 0x00A34D10 (:9378)
// Re-applies the current material effect to the head (without the time-in-w term while stealthed).
void player_refresh_head_material_effect(player* pp)
{
    if (pp == nullptr || pp->current_mat_fx == 0xFFFFFFFF || pp->char_inst == nullptr)
    {
        return;
    }
    human::resource_dependent_data* data = pp->rdd.is_loaded() ? pp->rdd.get_data() : nullptr;
    if (pp->mat_fx_handle_for_head_skin != 0xFFFFFFFF)
    {
        material_effects_remove(pp->mat_fx_handle_for_head_skin);
        pp->mat_fx_handle_for_head_skin = 0xFFFFFFFF;
    }
    if (data != nullptr && data->head_renderable_instance_p != nullptr)
    {
        material_effect_parameters params = material_effects_get_parameters(0, -1);
        if (fl_greater_than_robust(pp->m_stealth_percent, 0.0f, 0.0001f))
        {
            params.use_time_in_w = false;
        }
        pp->mat_fx_handle_for_head_skin = material_effects_apply(data->head_renderable_instance_p, (int)pp->current_mat_fx, &params);
        material_effects_force_finalize();
    }
}

// 0x00A11480 (:9409)
// The local player's team (EDF without one) and where it is: the player while alive, else the game camera.
void player_get_local_player_info(local_player_info* info)
{
    player* pp = Player;
    info->team = pp == nullptr ? HUMAN_TEAM_EDF : pp->current_team;
    if (pp != nullptr && !pp->is_killed())
    {
        info->orient = &pp->orient;
        info->pos = &pp->pos;
        return;
    }
    info->pos = &Rfg_camera.real_pos;
    info->orient = &Rfg_camera.real_orient;
}

// 0x00A5D360 (:9424)
// The single player statistics of the local player: distance (km) and time on foot or in a vehicle, play time, time
// at each alert level and in missions or activities.
void player_record_single_player_stats(player* pp)
{
    if (Multiplayer || wrecking_crew_is_active() || pp == nullptr || pp != Player)
    {
        return;
    }
    const float dx = pp->pos.x - pp->last_pos.x;
    const float dy = pp->pos.y - pp->last_pos.y;
    const float dz = pp->pos.z - pp->last_pos.z;
    const float dist_sq = (float)((double)dz * dz + (double)dy * dy + (double)dx * dx);
    float distance = 0.0f;
    // Magic number: moves under ~3 cm are ignored.
    if (0.001f < dist_sq)
    {
        distance = sqrtf(dist_sq);
    }
    // Magic number: meters to kilometers.
    distance = distance * 0.001f;
    float frametime = Frametime;
    if (!human_in_vehicle(pp, nullptr))
    {
        stats_increment(STATS_DISTANCE_ON_FOOT, &distance);
        stats_increment(STATS_TIME_ON_FOOT, &frametime);
    }
    else
    {
        stats_increment(STATS_DISTANCE_IN_VEHICLE, &distance);
        stats_increment(STATS_TIME_IN_VEHICLE, &frametime);
    }
    unit4::LocalGameSession* session = rfg_get_local_game_session();
    if (session != nullptr)
    {
        // Magic number: hero stat 3, the distance travelled.
        session->updateHeroStatAdd(nullptr, 3, (double)distance);
    }
    stats_increment(STATS_TIME_PLAYED_SP, &frametime);
    const gfm_alert_levels alert = gsm_get_alert_level();
    if (alert == ALERT_LEVEL_YELLOW)
    {
        stats_increment(STATS_TIME_AT_YELLOW_ALERT, &frametime);
    }
    else if (alert == ALERT_LEVEL_ORANGE)
    {
        stats_increment(STATS_TIME_AT_ORANGE_ALERT, &frametime);
    }
    else if (alert == ALERT_LEVEL_RED)
    {
        stats_increment(STATS_TIME_AT_RED_ALERT, &frametime);
    }
    if (mission_is_active())
    {
        stats_increment(STATS_TIME_IN_MISSIONS, &frametime);
    }
    else if (!activity_none())
    {
        stats_increment(STATS_TIME_IN_ACTIVITIES, &frametime);
    }
}

// 0x00A9EBA0 (:9491)
// Hides the jetpack straps prop (object::set_is_visible(false) inlined).
void player_hide_jetpack_straps(human* hp)
{
    item* ip = (item*)hp->child_ptr;
    while (ip != nullptr)
    {
        if (ip->get_obj_type() == OT_ITEM && ip->m_prop_type == PROP_TYPE_JETPACK && v_stricmp(ip->info->name, "Jetpack_straps") == 0)
        {
            ip->set_is_visible(false); // inlined
            return;
        }
        ip = (item*)ip->child_next;
        if (ip == (item*)hp->child_ptr)
        {
            return;
        }
    }
}

// Creates and wears one armor prop item. Inlined twice in player_create_armor_prop.
static void player_wear_armor_prop(player* pp, const item_info* info)
{
    // Original: the flags are the armor_level argument reused, with only no_physics, simulate, cast_shadows and
    // cast_drop_shadows (0x6006) set and the other used bits cleared.
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    flags.no_physics = 1;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_item_props props(info, pp->h_info->srid, pp->pos, pp->orient, flags, COLLIDE_ITEM, -1.0f);
    item* ip = World.create_object<item>(props, pp->get_handle());
    if (ip != nullptr)
    {
        ip->m_prop_type = PROP_TYPE_ACCESSORY_MISC;
        human_wear_prop(pp, ip);
    }
}

// 0x00AB3350 (:9505)
// The armor upgrade props: level 1 the L1 armor, level 2 also the L2 armor (the L1 one only if no accessory is worn
// yet). The jetpack straps are hidden.
void player_create_armor_prop(int armor_level, player* pp)
{
    player_hide_jetpack_straps(pp);
    if (armor_level != 1)
    {
        if (armor_level != 2)
        {
            return;
        }
        bool has_accessory = false;
        object* child = pp->child_ptr;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_ITEM && ((item*)child)->m_prop_type == PROP_TYPE_ACCESSORY_MISC)
            {
                has_accessory = true;
                break;
            }
            child = child->child_next;
            if (child == pp->child_ptr)
            {
                break;
            }
        }
        const item_info* l2 = object_item_get_info("Mason_Armor_L2");
        if (l2 == nullptr)
        {
            return;
        }
        player_wear_armor_prop(pp, l2); // inlined
        if (has_accessory)
        {
            return;
        }
    }
    const item_info* l1 = object_item_get_info("Mason_Armor_L1");
    if (l1 != nullptr)
    {
        player_wear_armor_prop(pp, l1); // inlined
    }
}

// ---- combat and weapon pickups (multiplayer.md MP8)
// Placed here: its tweak item is in this file's unity translation unit (object.cpp has no copy).
float Usable_object_facing_dot_prod_min; // 0x030266A8, from tweak_table.xtbl
static tweak_table_item Tweak_table_item_Usable_object_facing_dot_prod_min("Usable_object_facing_dot_prod_min", &Usable_object_facing_dot_prod_min); // 0x03197810
// Registered in rfg.exe, read by nothing there.
static float Player_death_salvage_percent_kept; // 0x03029B5C
static tweak_table_item Tweak_table_item_Player_death_salvage_percent_kept("Player_death_salvage_percent_kept", &Player_death_salvage_percent_kept); // 0x034264F0

// 0x00ACD990 (:6778): the local player's trigger, for the ammo types that fire. Not while reloading, holding an
// object, without a reticule, right after a throw or (in cover) away from an edge.
bool player_firearm_attack_start(bool secondary_weapon)
{
    player* pp = Player;
    if (!pp->char_inst->rdd.is_loaded())
    {
        return false;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(pp);
    if (w_info == nullptr)
    {
        return false;
    }
    const weapon_ammo_type ammo = w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY && ammo != WEAPON_AMMO_TYPE_REPAIR &&
        ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return false;
    }
    // Magic number: action alias 0x3E9 (the equip and hold action).
    if (human_weapons_is_reloading(pp, secondary_weapon) || vanim_is_playing_action(pp->char_inst->vp, 0x3E9))
    {
        return false;
    }
    if (pp->cur_state == HS_COVER && !player_cover_search_for_edge(pp))
    {
        return false;
    }
    if (pp->pflags.no_reticule || pp->hold_object_handle != 0xFFFFFFFF)
    {
        return false;
    }
    if (Player_hold_throw_timestamp.valid() && !Player_hold_throw_timestamp.elapsed())
    {
        return false;
    }
    if (secondary_weapon)
    {
        human_weapons_firearm_pull_secondary_weapon_trigger(pp);
    }
    else
    {
        human_weapons_firearm_pull_trigger(pp);
    }
    return true;
}

// 0x00A5CDC0 (:6832)
bool player_firearm_attack_stop(bool secondary_weapon)
{
    player* pp = Player;
    const weapon_info* w_info = human_weapons_get_weapon_info(pp);
    if (w_info == nullptr)
    {
        return false;
    }
    const weapon_ammo_type ammo = w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY && ammo != WEAPON_AMMO_TYPE_REPAIR &&
        ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return false;
    }
    if (secondary_weapon)
    {
        human_weapons_firearm_release_secondary_weapon_trigger(pp);
    }
    else
    {
        human_weapons_firearm_release_trigger(pp);
    }
    return true;
}

// 0x00A10BC0 (:7403)
// orig_main.md OM7: the in-game options lock the local player's controls while they are open.
void player_lock_local_player_controls()
{
    if (Player != nullptr)
    {
        Player->hflags.locked_controller = 1;
    }
}

// 0x00A10BE0 (:7411)
void player_unlock_local_player_controls()
{
    if (Player != nullptr)
    {
        Player->hflags.locked_controller = 0;
    }
    input_reset_gyro_orientation_origin();
}

// 0x00A10C00 (:7421)
bool player_is_local_player_controls_locked()
{
    if (Player != nullptr)
    {
        return Player->hflags.locked_controller;
    }
    return false;
}

// 0x00A10C20 (:7621): in Wrecking Crew the local player is the one whose turn it is (wrecking.md WC1).
void player_set_next_active_wrecking_crew_player()
{
    if (wrecking_crew_is_active())
    {
        player* pp = wrecking_crew_get_current_player();
        if (pp != nullptr)
        {
            Player = pp;
        }
    }
}

// 0x00A34BB0 (:9285): a material effect on the player's body, head and held items (up to 16 handles), unless one is
// already running.
void player_start_material_effect(player* pp, unsigned int effect, const material_effect_parameters& mat_fx_params)
{
    if (pp == nullptr || player_stop_material_effect(pp) || effect == 0xFFFFFFFF)
    {
        return;
    }
    pp->current_mat_fx = effect;
    character_instance* ci = pp->char_inst;
    if (ci == nullptr)
    {
        return;
    }
    int num_handles = 0;
    if (ci->rdd.is_loaded() && ci->rdd.get_data()->renderable_instance_p != nullptr)
    {
        pp->mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)ci->rdd.get_data()->renderable_instance_p, (int)effect, &mat_fx_params);
        num_handles = 1;
    }
    if (pp->rdd.is_loaded() && pp->rdd.get_data()->head_renderable_instance_p != nullptr)
    {
        pp->mat_fx_handle_for_head_skin = material_effects_apply((rl_renderable_instance*)pp->rdd.get_data()->head_renderable_instance_p, (int)effect, &mat_fx_params);
    }
    object* child = pp->child_ptr;
    if (child == nullptr)
    {
        return;
    }
    while (true)
    {
        // Every item but weapons, projectiles and backpacks (sub types 7, 8 and 10).
        const int sub_type = child->get_sub_type();
        if (child->get_obj_type() == OT_ITEM && (sub_type < OT_SUB_ITEM_WEAPON || (sub_type > OT_SUB_ITEM_PROJECTILE && sub_type != 10)))
        {
            item* ip = (item*)child;
            if (ip->rdd.is_loaded() && ip->rdd.get_data()->smesh_instance_p != nullptr)
            {
                if (num_handles > 15)
                {
                    return;
                }
                pp->mat_fx_handle[num_handles++] = material_effects_apply((rl_renderable_instance*)ip->rdd.get_data()->smesh_instance_p, (int)effect, &mat_fx_params);
            }
            if (ip->char_inst != nullptr && ip->char_inst->rdd.is_loaded() && ip->char_inst->rdd.get_data()->renderable_instance_p != nullptr)
            {
                if (num_handles > 15)
                {
                    return;
                }
                pp->mat_fx_handle[num_handles++] = material_effects_apply((rl_renderable_instance*)ip->char_inst->rdd.get_data()->renderable_instance_p, (int)effect, &mat_fx_params);
            }
        }
        child = child->child_next;
        if (child == pp->child_ptr || child == nullptr)
        {
            return;
        }
    }
}

// 0x00A5D2D0 (:9258): an invulnerable player glows with the sledgehammer's material effect.
void player_set_player_invulnerable(player* pp, bool is_invulnerable)
{
    if (pp == nullptr || is_invulnerable == (bool)pp->hflags.invulnerable)
    {
        return;
    }
    pp->hflags.invulnerable = is_invulnerable;
    if (!is_invulnerable)
    {
        player_stop_material_effect(pp);
        return;
    }
    const item_info* hammer = object_item_get_info(upgrade_get_sledgehamer_weapon_name_from_upgrade_level());
    if (hammer != nullptr)
    {
        player_start_material_effect(pp, hammer->material_effect_id, material_effects_get_parameters(0, -1));
    }
}

// 0x00A34980 (:8981): the living enemy behind a hit object (a human, a vehicle's driver or first enemy passenger, a
// turret's operator).
const human* player_get_enemy_target_from_object(const player* pp, const object* target)
{
    if (target == nullptr)
    {
        return nullptr;
    }
    const human* hp = nullptr;
    if (target->get_obj_type() == OT_HUMAN)
    {
        hp = (const human*)target;
    }
    else if (target->get_obj_type() == OT_VEHICLE)
    {
        hp = vehicle_passenger_in_seat((const vehicle*)target, VSI_DRIVER);
        if (hp == nullptr)
        {
            // Magic number: the 8 seats of the vehicle interaction.
            const vehicle* vp = (const vehicle*)target;
            for (int i = 0; i < 8; i++)
            {
                const unsigned int handle = vp->vii.humans[i].handle;
                const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
                hp = objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (const human*)objp : nullptr;
                if (hp != nullptr && human_teams_are_enemies(hp, pp))
                {
                    break;
                }
            }
        }
    }
    else if (target->get_obj_type() == OT_TURRET)
    {
        hp = human_combat_turret_get_operator((const turret*)target);
    }
    else
    {
        return nullptr;
    }
    if (hp == nullptr || hp->is_killed() || !human_teams_are_enemies(hp, pp))
    {
        return nullptr;
    }
    return hp;
}

// 0x00A34A50 (:9024)
bool player_update_combat_target(player* pp, const object* hit_objp)
{
    const human* hp = player_get_enemy_target_from_object(pp, hit_objp);
    if (hp == nullptr)
    {
        return false;
    }
    pp->combat.target_handle = hp->get_handle();
    // Magic number: the target is kept for 3 s.
    pp->combat_target_update_time.set(3000);
    return true;
}

// 0x00A10920 (:6735)
bool player_has_non_inventory(const player* pp)
{
    if (pp == nullptr)
    {
        return false;
    }
    const inventory_item* iip = pp->equipped_inv_item;
    return iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory;
}

// 0x00A103A0 (:2312)
bool player_is_usable_object_allowed(const player* pp, const t_usable_object* usable_object)
{
    if (usable_object->action_callback != nullptr && usable_object->action_type != GENERAL_ACTION_TYPE_INVALID)
    {
        return (pp->general_actions_allowed & (1u << ((unsigned int)usable_object->action_type & 0x1F))) != 0;
    }
    return true;
}

// 0x00A103D0 (:2327): a better facing wins, then (equal facing, or no current object) a shorter distance.
bool player_is_usable_object_better(const player* pp, const t_usable_object* usable_object)
{
    const float dot = pp->action_object.dot_prod;
    if (usable_object->dot_prod <= dot && pp->action_object.dist_squared != -1.0f && (usable_object->dot_prod != dot || pp->action_object.dist_squared <= usable_object->dist_squared))
    {
        return false;
    }
    return true;
}

// 0x00A10280 (:1950): an upright, enabled ammo box with the player in front of it (within 1 m of its front and 0.1 m
// of its height).
bool ammo_box_is_usable_by_player(const item* const abp, const player* const pp)
{
    bool in_front = true;
    if (pp != nullptr)
    {
        const vector front = abp->orient.fvec + abp->pos;
        const float height = pp->pos.y - abp->pos.y;
        if (vec_xz_dist_squared(pp->pos, front) >= 1.0f || height >= 0.1f || -height >= 0.1f)
        {
            in_front = false;
        }
    }
    // Magic number: 0.607, the box's up vector at most about 52 degrees from vertical.
    return in_front && abp->orient.uvec.y > 0.607f && (abp->special_flags.use_enabled || Ammo_box_unlimited_use);
}

// 0x00ADDF30 (:2002): the item the player may use: the one faced best (then nearest) among the touched items:
// weapons (faced by the camera; a full inventory needs a weapon dead ahead), ammo boxes and safehouse weapons.
bool player_process_usable_items(player* pp, t_usable_object& usable_object)
{
    float best_dist = 20.0f; // Magic number: the initial distance bound (squared).
    float best_dot = -1.0f;
    item* best = nullptr;
    const vector* view_pos = nullptr;
    const matrix* view_orient = nullptr;
    if (pp != nullptr)
    {
        if (pp == Player)
        {
            view_pos = &Rfg_camera.real_pos;
            view_orient = &Rfg_camera.real_orient;
        }
        else
        {
            view_pos = &pp->mp_camera_pos;
            view_orient = &pp->mp_camera_orient;
        }
    }
    const int num_items = World.get_num_objects<item>();
    float dist = 0.0f;
    for (int i = 0; i < num_items; i++)
    {
        item* itemp = World.get_object<item>(i);
        if (itemp->obj_flags.destroyed || !itemp->obj_flags._dont_use_me || itemp->is_owned())
        {
            continue;
        }
        if (!object_item_player_close_enough_to_touch(pp, itemp, false))
        {
            itemp->item_flags.touched_last_frame = 0;
            continue;
        }
        if (!object_item_player_touch(pp, itemp))
        {
            continue;
        }
        const vector& item_pos = itemp->pos;
        float dot;
        if (itemp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            dot = vec_dot_to_point(view_orient->fvec, *view_pos, item_pos);
            if (!player_can_pickup_weapon(pp, itemp))
            {
                continue;
            }
            const weapon_info* w_info = ((weapon*)itemp)->w_info;
            if (!w_info->flags.non_inventory)
            {
                if (human_has_item(pp, w_info->weapon_inv_item_info) == nullptr && !player_has_room_in_inventory(pp))
                {
                    dist = (item_pos - pp->pos).mag_squared();
                    // Magic number: a swap needs the weapon almost dead ahead.
                    if (dot < 0.99f)
                    {
                        dot = 0.0f;
                    }
                }
                // Original: otherwise the distance of the previous item is kept.
            }
            else if (dot < Usable_object_facing_dot_prod_min)
            {
                dist = 100.0f;
                dot = -1.0f;
            }
            else
            {
                dist = vec_dist_squared(item_pos, pp->pos);
            }
        }
        else
        {
            dot = vec_dot_to_point(pp->orient.fvec, pp->pos, item_pos);
            if (itemp->info->safehouse_weapon == nullptr)
            {
                // Magic number: action alias 0x3DD (the ammo box use).
                if ((!itemp->special_flags.is_ammo_box || (ammo_box_is_usable_by_player(itemp, pp) && Usable_object_facing_dot_prod_min <= dot)) &&
                    !pp->char_inst->is_playing_action(0x3DD))
                {
                    dist = vec_dist_squared(item_pos, pp->pos);
                }
                else
                {
                    dist = 100.0f;
                    dot = -1.0f;
                }
            }
            else
            {
                // A safehouse weapon is used where the camera ray meets it.
                line view_line;
                view_line.point = *view_pos;
                view_line.dir = view_orient->fvec;
                ix_collision hit;
                if (!ix_ray_sphere(hit, view_line, item_pos, itemp->get_bounding_radius() + 0.1f))
                {
                    continue;
                }
                dist = (hit.hit_point - item_pos).mag_squared();
            }
        }
        if (best_dot < dot || (dot == best_dot && dist < best_dist))
        {
            best_dist = dist;
            best_dot = dot;
            best = itemp;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    const int sub_type = best->get_sub_type();
    if (sub_type == OT_SUB_ITEM_WEAPON || best->info->safehouse_weapon != nullptr)
    {
        const bool non_inventory = sub_type == OT_SUB_ITEM_WEAPON && ((weapon*)best)->w_info->flags.non_inventory;
        usable_object.action_icon = player_has_room_in_inventory(pp) || non_inventory ? USE_OBJECT_ICON_DEFAULT : USE_OBJECT_ICON_SWAP;
        usable_object.action_callback = player_pickup_weapon;
        usable_object.action_type = GENERAL_ACTION_USE_NEAREST_OBJECT;
    }
    else if (sub_type == 10)
    {
        // Sub type 10: a multiplayer backpack.
        usable_object.action_icon = USE_OBJECT_ICON_SWAP;
        usable_object.action_callback = player_swap_backpack;
        usable_object.action_type = GENERAL_ACTION_USE_NEAREST_OBJECT;
    }
    else if (best->special_flags.is_ammo_box)
    {
        usable_object.action_callback = player_use_ammo_box;
        usable_object.action_type = GENERAL_ACTION_USE_AMMO_BOX;
        usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        usable_object.action_callback = nullptr;
        usable_object.handle = 0xFFFFFFFF;
        usable_object.action_type = GENERAL_ACTION_TYPE_INVALID;
        usable_object.action_icon = USE_OBJECT_ICON_INVALID;
        usable_object.dot_prod = -1.0f;
        usable_object.dist_squared = -1.0f;
        return false;
    }
    usable_object.dist_squared = best_dist;
    usable_object.handle = best->get_handle();
    usable_object.dot_prod = best_dot;
    return true;
}

// 0x00AB2F70 (:1103)
// The closest vehicle (2 m around the body's middle): to enter it, or to flip it when it is on its roof.
bool player_is_near_vehicle(const player* pp, t_usable_object& usable_object)
{
    vector middle;
    human_get_head_pos(pp, middle);
    middle.x = (middle.x + pp->pos.x) * 0.5f;
    middle.y = (pp->pos.y + middle.y) * 0.5f;
    middle.z = (pp->pos.z + middle.z) * 0.5f;
    if (human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    const unsigned int handle = vehicle_get_closest_vehicle(middle, 2.0f, nullptr, true); // Magic number: 2 m
    vehicle* vp = World.object_from_handle<vehicle>(handle);
    if (vp == nullptr || vp->is_killed())
    {
        return false;
    }
    vi_enter_data parameters;
    parameters.specified_seat = VSI_INVALID;
    parameters.seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
    parameters.destination_seat = VSI_INVALID;
    memset(&parameters.flags, 0, sizeof(parameters.flags));
    if (vehicle_is_usable(pp, &parameters, vp, false, false))
    {
        usable_object.handle = vp->get_handle();
        usable_object.dist_squared = vec_dist_squared(middle, vp->pos);
        usable_object.dot_prod = 0.01f; // Magic number: a low facing, so that anything faced better wins.
        usable_object.action_callback = player_use_vehicle;
        usable_object.action_type = GENERAL_ACTION_USE_VEHICLE;
        usable_object.action_icon = USE_OBJECT_ICON_VEHICLE;
        return true;
    }
    const unsigned int vehicle_handle = vp->get_handle();
    if (vehicle_is_flipped(vehicle_handle) && pp->ground_obj_handle != vehicle_handle && vehicle_is_usable(pp, &parameters, vp, false, true))
    {
        usable_object.handle = vehicle_handle;
        vector to_vehicle;
        to_vehicle.x = middle.x - vp->pos.x;
        to_vehicle.y = middle.y - vp->pos.y;
        to_vehicle.z = middle.z - vp->pos.z;
        usable_object.dist_squared = to_vehicle.mag_squared();
        usable_object.dot_prod = 0.01f;
        usable_object.action_callback = player_flip_vehicle;
        usable_object.action_type = GENERAL_ACTION_USE_VEHICLE;
        usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
        return true;
    }
    return false;
}

// 0x00ACCE20 (:1167)
// The nearest turret within 1 m that the player faces and whose handles face the player, with nobody else alive on the
// way to the handles.
bool player_is_near_turret(player* pp, t_usable_object& usable_object)
{
    const inventory_item* iip = pp != nullptr ? pp->equipped_inv_item : nullptr;
    if (iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory)
    {
        return false;
    }
    // Magic numbers: 45 and 60 degrees, in float radians as rfg.exe's constants have them.
    static const float USABLE_TURRET_COLLECT_VIEW_DOT = (float)cos((double)0.7853982f); // 0x035B5A40 (first use)
    static const float USABLE_TURRET_HANDLES_COLLECT_VIEW_DOT = (float)cos((double)1.0471976f); // 0x035B5A44 (first use)
    unsigned int turrets[8];
    const int num_turrets = World.collect_objects<turret>(pp->pos, 1.0f, turrets, 8, nullptr, nullptr);
    float best_dist = FLT_MAX;
    turret* best = nullptr;
    for (int i = 0; i < num_turrets; i++)
    {
        turret* tp = World.object_from_handle<turret>(turrets[i]);
        if (tp == nullptr || !human_combat_can_man_turret(pp, tp))
        {
            continue;
        }
        vector to_turret;
        to_turret.x = tp->pos.x - pp->pos.x;
        to_turret.y = tp->pos.y - pp->pos.y;
        to_turret.z = tp->pos.z - pp->pos.z;
        const float dist = to_turret.flatten_mag_and_normalize_safe();
        // Magic number: 0.3 m of height difference.
        if (dist > best_dist || fabsf(tp->pos.y - pp->pos.y) > 0.3f)
        {
            continue;
        }
        const vector view = (object_is_local_player(pp) ? Rfg_camera.real_orient.fvec : pp->mp_camera_orient.fvec).copy_flatten_normalize_safe();
        if (!(USABLE_TURRET_COLLECT_VIEW_DOT <= view * to_turret))
        {
            if (!object_is_local_player(pp))
            {
                continue;
            }
            const vector body = pp->orient.fvec.copy_flatten_normalize_safe();
            if (!(USABLE_TURRET_COLLECT_VIEW_DOT <= body * to_turret))
            {
                continue;
            }
        }
        const vector handles = tp->get_weapon_inst()->orient.fvec.copy_flatten_normalize_safe();
        if (!(USABLE_TURRET_HANDLES_COLLECT_VIEW_DOT <= handles * to_turret))
        {
            continue;
        }
        const vector weapon_pos = tp->get_weapon_inst()->pos;
        vector half;
        half.x = (pp->pos.x - weapon_pos.x) * 0.5f;
        half.y = (pp->pos.y - weapon_pos.y) * 0.5f;
        half.z = (pp->pos.z - weapon_pos.z) * 0.5f;
        const float radius = half.mag_approx() * 0.5f;
        vector centre;
        centre.x = weapon_pos.x + half.x;
        centre.y = weapon_pos.y + half.y;
        centre.z = weapon_pos.z + half.z;
        unsigned int humans[8];
        const int num_humans = World.collect_objects<human>(centre, radius, humans, 8, nullptr, nullptr);
        bool blocked = false;
        for (int j = 0; j < num_humans; j++)
        {
            const human* hp = World.object_from_handle<human>(humans[j]);
            // Original: compares with the local player, not pp.
            if (hp != nullptr && hp != Player && !hp->is_killed())
            {
                blocked = true;
            }
        }
        if (!blocked)
        {
            best_dist = dist;
            best = tp;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    usable_object.handle = best->get_handle();
    usable_object.dist_squared = vec_dist_squared(best->pos, pp->pos);
    usable_object.dot_prod = vec_dot_to_point(pp->orient.fvec, pp->pos, best->pos);
    usable_object.action_callback = player_use_turret;
    usable_object.action_type = GENERAL_ACTION_USE_TURRET;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    return true;
}

// 0x00A5C2E0 (:1702). Original: the start line plays once on coming within 20 m, and again after leaving 30 m.
bool player_is_near_riding_shotgun_node(const player* pp, t_usable_object& usable_object)
{
    if (!activity_riding_shotgun_can_be_started())
    {
        player_is_near_rs_node = false;
        return false;
    }
    object_riding_shotgun_node* best = nullptr;
    float best_dist = 0.0f;
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        if (!node->can_spawn()) // rfg.exe inlines it
        {
            continue;
        }
        const float dist = vec_dist_squared(node->pos, pp->pos);
        if (best == nullptr || dist < best_dist)
        {
            best = node;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    if (best_dist >= 400.0f || audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_LOADING) // Magic number: 20 m
    {
        if (best_dist > 900.0f) // Magic number: 30 m
        {
            player_is_near_rs_node = false;
        }
        return false;
    }
    if (!player_is_near_rs_node)
    {
        if (activity_get_node_start_on_load() != best->get_handle())
        {
            activity_riding_shotgun_play_start_voiceline(best);
        }
        player_is_near_rs_node = true;
    }
    if (best_dist >= 25.0f || human_in_vehicle_interaction(pp, nullptr)) // Magic number: 5 m
    {
        return false;
    }
    usable_object.action_callback = player_start_riding_shotgun;
    usable_object.action_type = GENERAL_ACTION_START_RIDING_SHOTGUN;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.handle = best->get_handle();
    usable_object.dist_squared = best_dist;
    return true;
}

// 0x00A5BDD0 (:1593). Magic number: 1.5 is the squared reach of the start point.
bool player_is_near_offensive_raid_start(const player* pp, t_usable_object& usable_object)
{
    vector start_pos;
    unsigned int start_node_handle;
    if (!activity_raid_attack_start_info(start_pos, start_node_handle) || human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    const float dist = vec_dist_squared(pp->pos, start_pos);
    if (dist > 1.5f)
    {
        return false;
    }
    usable_object.dist_squared = dist;
    usable_object.action_callback = player_start_offensive_raid;
    usable_object.action_type = GENERAL_ACTION_START_OFFENSIVE_RAID;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.handle = start_node_handle;
    vector to_start = start_pos - pp->pos;
    to_start.normalize_safe();
    usable_object.dot_prod = pp->orient.fvec.x * to_start.x + pp->orient.fvec.y * to_start.y + pp->orient.fvec.z * to_start.z;
    return true;
}

// 0x00A5BF40 (:1637). The first streamed node the player stands in front of. Original: while it cannot start, rfg.exe still
// walks the nodes and works out two distances it drops, left out here. In a vehicle the walk ends at the first streamed
// node. Magic numbers: half a metre in front of the node, within one metre.
bool player_is_near_demolitions_master_node(const player* pp, t_usable_object& usable_object)
{
    if (Multiplayer || (!activity_none() && activity_get_mode() != ACTIVITY_MODE_DEMOLITIONS_MASTER) || !Demolitions_master_activity.activity_base_modal::can_initialize() || !activity_demolitions_master_alert_allows_start() || Demolitions_master_activity.current_state != DM_STATE_WAIT || hud_ui_activity_interface_active())
    {
        return false;
    }
    const bool in_vehicle = human_in_vehicle_interaction(pp, nullptr);
    for (int i = 0; i < World.get_num_objects<object_demolitions_master_node>(); i++)
    {
        const object_demolitions_master_node* node = World.get_object<object_demolitions_master_node>(i);
        if (!node->obj_flags.streamed)
        {
            continue;
        }
        if (in_vehicle)
        {
            return false;
        }
        const vector in_front = node->pos + node->orient.fvec * 0.5f;
        const float dist = vec_dist_squared(in_front, pp->pos);
        const float dot = vec_dot_to_point(pp->orient.fvec, pp->pos, node->pos);
        if (dist < 1.0f && dot > Usable_object_facing_dot_prod_min)
        {
            usable_object.action_callback = player_start_demolitions_master;
            usable_object.action_type = GENERAL_ACTION_START_DEMOLITIONS_MASTER;
            usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
            usable_object.handle = node->get_handle();
            usable_object.dist_squared = dist;
            usable_object.dot_prod = dot;
            return true;
        }
    }
    return false;
}

// 0x00A83F20 (:1787). Original: the start line plays once on coming within 20 m, and again after leaving 30 m; the distances
// are to the node's start point.
bool player_is_near_area_defense_node(const player* pp, t_usable_object& usable_object)
{
    if (pp == nullptr || pp->cur_movement_state == HMS_SPRINT || (pp->sprint_delay_timestamp.valid() && !pp->sprint_delay_timestamp.elapsed()))
    {
        return false;
    }
    if (!activity_area_defense_can_be_started())
    {
        player_is_near_ad_node = false;
        return false;
    }
    object_area_defense_node* best = nullptr;
    float best_dist = 0.0f;
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        object_area_defense_node* node = World.get_object<object_area_defense_node>(i);
        if (!node->can_spawn()) // rfg.exe inlines it
        {
            continue;
        }
        const object_activity_spawn* start = node->get_start_node();
        if (start == nullptr || !start->obj_flags.streamed)
        {
            continue;
        }
        const float dist = vec_dist_squared(start->pos, pp->pos);
        if (best == nullptr || dist < best_dist)
        {
            best = node;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    if (best_dist >= 400.0f || audiolib_system_get_game_state() == AUDIOLIB_GAME_STATE_LOADING) // Magic number: 20 m
    {
        if (best_dist > 900.0f) // Magic number: 30 m
        {
            player_is_near_ad_node = false;
        }
        return false;
    }
    if (!player_is_near_ad_node)
    {
        activity_area_defense_play_start_voiceline(best);
        player_is_near_ad_node = true;
    }
    if (best_dist >= 25.0f || human_in_vehicle_interaction(pp, nullptr)) // Magic number: 5 m
    {
        return false;
    }
    usable_object.action_callback = player_start_area_defense;
    usable_object.action_type = GENERAL_ACTION_START_AREA_DEFENSE;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.handle = best->get_handle();
    usable_object.dist_squared = best_dist;
    return true;
}

// 0x00A5C490 (:1877). The prompt is on the informant, within 5 m. Magic number: 25 is that distance squared.
bool player_is_near_house_arrest_node(const player* pp, t_usable_object& usable_object)
{
    if (!house_arrest_can_be_started() || human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    const unsigned int informant_handle = house_arrest_get_informant_npc();
    if (informant_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const human* informant = World.object_from_handle<human>(informant_handle);
    if (informant == nullptr)
    {
        return false;
    }
    const float dist = vec_dist_squared(pp->pos, informant->pos);
    if (dist >= 25.0f)
    {
        return false;
    }
    unsigned int as_node_handle = 0xFFFFFFFF;
    unsigned int ha_node_handle = 0xFFFFFFFF;
    if (house_arrest_get_nearest_activity(as_node_handle, ha_node_handle) < 0.0f || ha_node_handle == 0xFFFFFFFF || World.object_from_handle<object_activity_spawn>(as_node_handle) == nullptr)
    {
        return false;
    }
    usable_object.action_callback = player_start_house_arrest;
    usable_object.action_type = GENERAL_ACTION_START_HOUSE_ARREST;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.handle = ha_node_handle;
    usable_object.dist_squared = dist;
    return true;
}

// 0x00A5BC70 (:1511)
// The nearest enabled upgrade node within 1 m. No node exists until gameflow.md ports the class (only its small inlines
// are here).
bool player_is_near_upgrade_node(const player* pp, t_usable_object& usable_object)
{
    if (Multiplayer || mission_is_active() || human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    float best_dist = 1.0f; // Magic number: 1 m squared.
    object_upgrade_node* best = nullptr;
    const int num_nodes = World.get_num_objects<object_upgrade_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        object_upgrade_node* node = World.get_object<object_upgrade_node>(i);
        if (node->is_enabled())
        {
            const float dist = vec_dist_squared(pp->pos, node->pos);
            if (dist <= best_dist)
            {
                best = node;
                best_dist = dist;
            }
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    usable_object.action_callback = player_start_upgrades;
    usable_object.action_type = GENERAL_ACTION_UPGRADE;
    usable_object.action_icon = USE_OBJECT_ICON_UPGRADE;
    usable_object.handle = best->get_handle();
    usable_object.dist_squared = best_dist;
    usable_object.dot_prod = vec_dot_to_point(pp->orient.fvec, pp->pos, best->pos);
    return true;
}

// 0x00A84100 (:2171)
// In the walker mission: the walker close enough behind the flatbed, and lined up with it, may mount.
bool is_player_near_wmr_flatbed(player* pp, t_usable_object& usable_object)
{
    if (!mission_is_active() || mission_get_mode() != MISSION_MODE_WMR || pp != Player)
    {
        return false;
    }
    const mission_data* data = mission_get_data();
    if (data == nullptr || !data->valid)
    {
        return false;
    }
    walker* wp = World.object_from_handle<walker>(data->handle1);
    vehicle* flatbed = World.object_from_handle<vehicle>(data->handle2);
    unsigned int flatbed_handle = data->handle2;
    if (flatbed == nullptr)
    {
        const object_squad* squad = World.object_from_handle<object_squad>(flatbed_handle);
        if (squad != nullptr)
        {
            flatbed = World.object_from_handle<vehicle>(squad->squad_vehicle);
            flatbed_handle = squad->squad_vehicle;
        }
    }
    if (wp == nullptr || flatbed == nullptr || !human_in_vehicle(pp, wp))
    {
        return false;
    }
    // Magic numbers: a point 9 m behind the flatbed, within 4 m of the walker (16 squared), 40 degrees of heading.
    const float dx = (flatbed->pos.x - flatbed->orient.fvec.x * 9.0f) - wp->pos.x;
    const float dz = (flatbed->pos.z - flatbed->orient.fvec.z * 9.0f) - wp->pos.z;
    if (dx * dx + dz * dz > 16.0f)
    {
        return false;
    }
    if (!(fabsf(flatbed->orient.extract_heading() - wp->orient.extract_heading()) < 0.69813174f))
    {
        return false;
    }
    usable_object.dot_prod = 1.0f;
    usable_object.handle = flatbed_handle;
    usable_object.action_callback = player_mount_walker_in_vehicle;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.action_type = GENERAL_ACTION_TYPE_INVALID;
    usable_object.dist_squared = 0.0f;
    return true;
}

// 0x00AC1DF0 (:2243)
// The action node whose entry point is within 1 m and is faced best.
bool player_is_near_action_node(player* pp, t_usable_object& usable_object)
{
    if (pp->cur_movement_state == HMS_SPRINT || (pp->sprint_delay_timestamp.valid() && !pp->sprint_delay_timestamp.elapsed()))
    {
        return false;
    }
    farray<object_action_node const*,608> nodes;
    if (!action_node_collect_nodes(pp, 15.0f, nodes)) // Magic number: 15 m
    {
        return false;
    }
    const object_action_node* best = nullptr;
    float best_dist = FLT_MAX;
    float best_dot = Usable_object_facing_dot_prod_min;
    // Original bug kept: a disabled node leaves the previous node's entry point here and is measured with it. Deviation:
    // the first time it is zero, where rfg.exe reads an uninitialized stack slot.
    vector entry = ZERO_VECTOR;
    for (int i = 0; i < nodes.size(); i++)
    {
        const object_action_node* anp = nodes[i];
        // Original: for a human that is not the player the test is inverted, the node must not allow players.
        const bool allowed = pp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? anp->allow_player() : (!anp->allow_player() && anp->is_allowed_to_use(pp->current_team));
        if (!allowed)
        {
            continue;
        }
        if (anp->disabled_by == 0)
        {
            const int index = pp->has_node_reserved(anp->get_handle()) ? pp->get_action_node_index(anp) : 0;
            anp->get_entry_point_int(anp->base_node_info->action_anim_list[index], entry);
        }
        const float dist = vec_dist_squared(pp->pos, entry);
        if (!(1.0f > dist)) // Magic number: 1 m squared.
        {
            continue;
        }
        matrix rotation;
        const float dot = anp->get_entry_rotation(pp, rotation) ? pp->orient.fvec.y * rotation.fvec.y + rotation.fvec.x * pp->orient.fvec.x + pp->orient.fvec.z * rotation.fvec.z
                                                                : vec_dot_to_point(pp->orient.fvec, pp->pos, entry);
        if (best_dot < dot)
        {
            best = anp;
            best_dist = dist;
            best_dot = dot;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    usable_object.dot_prod = best_dot;
    usable_object.handle = best->get_handle();
    usable_object.action_callback = player_use_action_node;
    usable_object.action_icon = USE_OBJECT_ICON_DEFAULT;
    usable_object.action_type = GENERAL_ACTION_USE_NEAREST_OBJECT;
    usable_object.dist_squared = best_dist;
    return true;
}

// 0x00ADE470 (:2347)
// What the use button acts on: dropped and searched again every 100 ms (Player_usable_object_refresh_rate); in between
// the current one goes once it is no longer allowed or its callback refuses; none with a turret, a non-inventory item
// or on a ladder. The blast from the past tags and car bombs are processed here too.
void player_process_object_interaction(player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    bool refresh = true;
    if (!pp->action_object_timestamp.valid() || pp->action_object_timestamp.elapsed())
    {
        pp->action_object.reset();
        pp->action_object_timestamp.set(Player_usable_object_refresh_rate);
    }
    else
    {
        if (pp->action_object.action_callback != nullptr && (!player_is_usable_object_allowed(pp, &pp->action_object) || !pp->action_object.action_callback(pp, false)))
        {
            pp->action_object.reset();
        }
        refresh = false;
    }
    const inventory_item* iip = pp->equipped_inv_item;
    const bool non_inventory = iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory;
    if (human_combat_get_manned_turret(pp) != nullptr || non_inventory || pp->hflags.is_on_ladder)
    {
        pp->action_object.reset();
        refresh = false;
    }
    if (Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        refresh = false;
    }
    if (pp->is_killed() || pp->zoom_state == PZS_SCOPE)
    {
        pp->action_object.reset();
        return;
    }
    blast_from_the_past_process_car_bombs();
    t_usable_object usable_object;
    if (human_in_vehicle(pp) != nullptr)
    {
        if (pp->action_object.action_callback != player_mount_walker_in_vehicle && is_player_near_wmr_flatbed(pp, usable_object))
        {
            pp->action_object = usable_object;
        }
        if (upgrade_has_salvage_collector())
        {
            vector ground = pp->pos;
            rfg_snap_to_ground(ground);
            unsigned int items[30];
            const int num_items = World.collect_objects<item>(ground, 5.0f, items, 30, nullptr, nullptr); // Magic number: 5 m
            for (int i = 0; i < num_items; i++)
            {
                item* itemp = World.object_from_handle<item>(items[i]);
                if (itemp != nullptr && itemp->salvage_material != INVALID_SALVAGE_MATERIAL)
                {
                    object_item_player_touch(pp, itemp);
                }
            }
        }
        return;
    }
    if (!player_process_usable_items(pp, usable_object))
    {
        usable_object.reset();
    }
    if (!refresh)
    {
        return;
    }
    if (Player != pp || pp->combat.cur_melee_combat_move != nullptr ||
        ((controls_is_button_down(CBA_GAC_WEAPON_MENU, Player_controller_index) || controls_button_is_masked(CBA_GAC_WEAPON_MENU, Player_controller_index)) &&
         control_gameplay_general_controls_allowed()))
    {
        pp->action_object.reset();
        return;
    }
    // While reloading in single player, no vehicle, turret or action node becomes the action object.
    const bool reloading = !Multiplayer && pp->char_inst->is_playing_action(ANIM_ALIAS_RELOAD);
    if (player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (Multiplayer && multi_gameplay_mode_player_set_action_object(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    player_use_ladder(pp, true);
    if (player_is_near_mission_start(pp, &usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_upgrade_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_demolitions_master_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_offensive_raid_start(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_riding_shotgun_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_area_defense_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_house_arrest_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (reloading)
    {
        return;
    }
    if (player_is_near_vehicle(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_turret(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
    if (player_is_near_action_node(pp, usable_object) && player_is_usable_object_allowed(pp, &usable_object) && player_is_usable_object_better(pp, &usable_object))
    {
        pp->action_object = usable_object;
    }
}

// 0x00A10200 (:1411)
void get_ammo_box_anim_position(const item* abp, vector& open_pos)
{
    open_pos = PLAYER_OPEN_AMMO_BOX_OFFSET;
    vec_change_to_global_coords(open_pos, abp->pos, abp->orient);
}

// 0x00A846E0 (:4000)
void player::open_ammo_box(item* abp)
{
    vector facing;
    facing.x = -abp->orient.fvec.x;
    facing.y = -abp->orient.fvec.y;
    facing.z = -abp->orient.fvec.z;
    matrix dest_orient;
    dest_orient.make(facing);
    if (cur_state == HS_AMMO_BOX)
    {
        return;
    }
    vector dest_pos = PLAYER_OPEN_AMMO_BOX_OFFSET;
    vec_change_to_global_coords(dest_pos, abp->pos, abp->orient);
    dest_pos.y = pos.y;
    char_inst->play_action(ANIMATION_ACTION_OPEN_AMMO_BOX, ANIM_ALIAS_NONE, 1.0f, 0);
    // Magic number: 0.3 s to reach the box's front.
    char_inst->set_action_dest_position(pos, orient, dest_pos, ANIMATION_ACTION_OPEN_AMMO_BOX, 0.3f, false);
    char_inst->set_action_dest_orient(orient, dest_orient, ANIMATION_ACTION_OPEN_AMMO_BOX, 0.3f);
    // Magic number: state 3, the box's open animation state.
    abp->char_inst->set_state(index_alias(3, ANIM_ALIAS_NONE), index_alias(ANIMATION_ACTION_CUSTOM, ANIM_ALIAS_NONE), 0.0f, false);
    block_movement_anim = ANIMATION_ACTION_OPEN_AMMO_BOX;
    block_steering_vector = facing;
    block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    combat.combat_ready_timestamp.invalidate();
    pflags.weapon_hidden = 0;
    combat.combat_aim_timestamp.invalidate();
    move_speed = -1.0f;
    cur_state = HS_AMMO_BOX;
    cur_movement_state = HMS_IDLE;
    hide_weapon();
    abp->special_flags.use_enabled = 0;
    if (abp->salvage_effect != 0xFFFFFFFF)
    {
        material_effects_remove(abp->salvage_effect);
    }
    Cartographer.remove_icon(abp->get_handle());
    abp->damage_info.flags.can_explode = 0;
}

// 0x00A853D0 (:6089)
// Walks to the box's front first when too far from it; do_frame_open_ammo_box opens it on arrival.
bool player_use_ammo_box(player* pp, bool do_action)
{
    item* abp = World.object_from_handle<item>(pp->action_object.handle);
    if (pp->hflags.is_on_ladder || pp->cur_state == HS_VEHICLE_DRIVING || abp == nullptr || pp->cur_state == HS_JUMP || pp->cur_state == HS_FALL)
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (ovl_inventory_is_open())
    {
        ovl_inventory_close();
    }
    pp->ammo_box_handle = abp->get_handle();
    vector open_pos;
    get_ammo_box_anim_position(abp, open_pos);
    // Magic numbers: 1 m squared in the plane, 0.1 m of height.
    const float height_diff = pp->pos.y - open_pos.y;
    if (vec_xz_dist_squared(pp->pos, open_pos) >= 1.0f || height_diff >= 0.1f || -height_diff >= 0.1f)
    {
        pp->pathfind_time_out.set(2000); // Magic number: 2 s
        if (human_pathfind_to(pp, open_pos, HS_PLAYER_PATHING, HMS_WALK, 0xFFFFFFFFFFFFFFFFull, false))
        {
            return true;
        }
    }
    gamelog_store_current_loadout(pp);
    pp->open_ammo_box(abp);
    log_event_player_use_weapons_cache(gamelog_get_player_checksum(), gamelog_get_logging_time(), gamelog_get_time_played(), gamelog_get_pct_complete(), gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_game_duration(), pp->pos, 1);
    return true;
}

// 0x00A344D0 (:6294)
bool player_start_riding_shotgun(player* pp, bool do_action)
{
    if (Multiplayer)
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (human_in_vehicle_interaction(pp, nullptr) || !activity_riding_shotgun_can_be_started() || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_riding_shotgun_node* node = World.object_from_handle<object_riding_shotgun_node>(pp->action_object.handle);
    return node != nullptr && activity_riding_shotgun_start(node);
}

// 0x00A34430 (:6252)
bool player_start_offensive_raid(player* pp, bool do_action)
{
    if (Multiplayer)
    {
        return false;
    }
    if (do_action)
    {
        if (human_in_vehicle_interaction(pp, nullptr))
        {
            return false;
        }
        activity_raid_attack_start();
    }
    return true;
}

// 0x00A34460 (:6270)
bool player_start_demolitions_master(player* pp, bool do_action)
{
    if (Multiplayer)
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (human_in_vehicle_interaction(pp, nullptr) || !Demolitions_master_activity.activity_base_modal::can_initialize() || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_demolitions_master_node* node = World.object_from_handle<object_demolitions_master_node>(pp->action_object.handle);
    return node != nullptr && activity_demolitions_master_start(node);
}

// 0x00A34540 (:6319)
bool player_start_area_defense(player* pp, bool do_action)
{
    if (Multiplayer)
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (human_in_vehicle_interaction(pp, nullptr) || !activity_area_defense_can_be_started() || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_area_defense_node* node = World.object_from_handle<object_area_defense_node>(pp->action_object.handle);
    return node != nullptr && activity_area_defense_start(node);
}

// 0x00A345B0 (:6343)
bool player_start_house_arrest(player* pp, bool do_action)
{
    if (Multiplayer)
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (human_in_vehicle_interaction(pp, nullptr) || !house_arrest_can_be_started() || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_house_arrest_node* node = World.object_from_handle<object_house_arrest_node>(pp->action_object.handle);
    return node != nullptr && activity_house_arrest_start(node);
}

// 0x00A34370 (:6210)
bool player_start_upgrades(player* pp, bool do_action)
{
    if (Multiplayer || mission_is_active() || human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    // The upgrades tool tip has to play first, when tips are on.
    if ((ui_tool_tips_tip_is_queued(UPGRADES_TOOL_TIP_NAME_CHECKSUM) || !ui_tool_tips_tip_has_been_played(UPGRADES_TOOL_TIP_NAME_CHECKSUM, false)) && ui_tool_tips_tips_are_enabled())
    {
        return false;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_UPGRADE_NODE || !static_cast<object_upgrade_node*>(objp)->is_enabled())
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    if (game_is_paused() || gameseq_get_pending_state() != GS_NONE)
    {
        return false;
    }
    gameseq_push_state(GS_INGAME_UPGRADES_HUD, true, false);
    return true;
}

// 0x00A5CD40 (:6559)
bool player_mount_walker_in_vehicle(player* pp, bool do_action)
{
    if (!mission_is_active() || mission_get_mode() != MISSION_MODE_WMR)
    {
        return false;
    }
    vehicle* in_vehicle = human_in_vehicle(pp);
    if (in_vehicle == nullptr || in_vehicle->get_sub_type() != OT_SUB_VEHICLE_WALKER)
    {
        return false;
    }
    walker* wp = static_cast<walker*>(in_vehicle);
    if (wp->w_flags.is_mounting)
    {
        return false;
    }
    vehicle* vp = World.object_from_handle<vehicle>(pp->action_object.handle);
    if (vp == nullptr)
    {
        return false;
    }
    if (do_action)
    {
        walker_mount_vehicle(wp, vp);
        mission_clear_data();
    }
    return true;
}

// 0x00ABE5D0 (:6590)
bool player_use_action_node(player* pp, bool do_action)
{
    if (pp->cur_state == HS_JUMP || pp->cur_state == HS_FALL)
    {
        return false;
    }
    object_action_node* anp = World.object_from_handle<object_action_node>(pp->action_object.handle);
    if (anp == nullptr)
    {
        if (pp->cur_state == HS_ACTION_NODE)
        {
            pp->hflags.override_default_anim_state = 0;
            pp->hflags.override_default_fire_anim = 0;
            pp->custom_fire_animation = ANIMATION_ACTION_NONE;
            pp->move_speed = -1.0f;
            pp->cur_state = HS_STANDARD;
        }
        return false;
    }
    if (anp->disabled_by != 0 || !anp->is_allowed_to_use(pp) || anp->get_occupant() != nullptr || !anp->at_entry_point(pp))
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    player_exit_cover(pp, false);
    // Original: reserves twice; a failure only prints (Int3 in the debug build).
    if (!action_node_reserve_node(pp, anp) || !action_node_reserve_node(pp, anp) || !anp->enter_node(pp))
    {
        return false;
    }
    if (anp->is_weapon_cabinet_action_node())
    {
        audiolib_cue_play(Ga_cue_ids.weapon_cabinet_open, nullptr, nullptr);
    }
    if (pp->block_movement_anim == -1)
    {
        pp->block_movement_anim = anp->get_player_animation(pp);
        pp->block_steering_vector = pp->orient.fvec;
        pp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    }
    pp->combat.combat_ready_timestamp.invalidate();
    pp->combat.combat_aim_timestamp.invalidate();
    human_weapons_cancel_reload(pp);
    pp->hide_weapon();
    pp->move_speed = -1.0f;
    pp->cur_state = HS_ACTION_NODE;
    return true;
}

// 0x00A10910 (:5584): empty in the release build.
bool player_start_dialogue(player* pp)
{
    return false;
}

// 0x00A34080 (:5873)
bool player_flip_vehicle(player* pp, bool do_action)
{
    if (human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    const inventory_item* iip = pp != nullptr ? pp->equipped_inv_item : nullptr;
    if (iip != nullptr && iip->info != nullptr && iip->info->w_info != nullptr && iip->info->w_info->flags.non_inventory)
    {
        return false;
    }
    vehicle* vp = World.object_from_handle<vehicle>(pp->action_object.handle);
    if (vp == nullptr)
    {
        return false;
    }
    vi_enter_data parameters;
    parameters.specified_seat = VSI_INVALID;
    parameters.seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
    parameters.destination_seat = VSI_INVALID;
    memset(&parameters.flags, 0, sizeof(parameters.flags));
    if (!vehicle_is_usable(pp, &parameters, vp, true, true))
    {
        return false;
    }
    if (do_action)
    {
        // Magic number: an upward impulse of twice the chassis mass, at the player.
        const float mass = vp->info->chassis_mass;
        vector impulse;
        impulse.x = UP_VECTOR.x * mass * 2.0f;
        impulse.y = UP_VECTOR.y * mass * 2.0f;
        impulse.z = UP_VECTOR.z * mass * 2.0f;
        havok_body_apply_point_impulse(vp->havok_handle, impulse, pp->pos);
    }
    return true;
}

// 0x00A10960 (:6751)
bool player_use_equipped_item(player* pp)
{
    const inventory_item* iip = pp->equipped_inv_item;
    if (iip != nullptr && iip->info->use != nullptr)
    {
        return iip->info->use();
    }
    return false;
}

// 0x00AB32D0 (:7045)
bool player_is_sabotaging_flyer(player* pp, flyer* fp)
{
    if (pp == nullptr || pp->is_killed())
    {
        return false;
    }
    object_action_node* anp = World.object_from_handle<object_action_node>(pp->action_node_data.current_action_node);
    if (anp == nullptr || pp->cur_state != HS_ACTION_NODE)
    {
        return false;
    }
    if (fp != nullptr)
    {
        return anp->get_target_object() == static_cast<const object*>(fp);
    }
    return anp->is_flyer_sabotage_node();
}

// 0x00A5CE20 (:7699)
bool player_occupied_vehicle(const vehicle* vp)
{
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        if (vp->get_handle() == World.get_object<player>(i)->vehicle_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00A34A90 (:9065): an enemy shows only with Minimap_render_humans.
bool player::mp_should_render_on_minimap() const
{
    if (Player == nullptr)
    {
        return false;
    }
    if (this != nullptr && Player != this && human_teams_get_disposition(current_team, Player->current_team) == TEAM_DISPOSITION_HOSTILE && !Minimap_render_humans)
    {
        return false;
    }
    return true;
}

// 0x00A5D220 (:9232)
// The npc's target is the stuck player, or a vehicle's occupant while the stuck player drives that vehicle, within 30 m.
bool is_player_my_target_or_is_my_target_with_player_in_stuck_vehicle(const npc* np)
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return false;
    }
    const player* pp;
    if (target->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        pp = static_cast<const player*>(target);
    }
    else
    {
        if (human_in_vehicle(target) == nullptr)
        {
            return false;
        }
        const vehicle* vp = World.object_from_handle<vehicle>(target->vehicle_handle);
        pp = Player;
        if (vp == nullptr || pp == nullptr || !vehicle_human_in_vehicle(vp, pp) || pp->cur_state != HS_VEHICLE_DRIVING)
        {
            return false;
        }
    }
    return pp->pflags.is_stuck != 0 && vec_dist_squared(np->pos, pp->pos) < 900.0f; // Magic number: 30 m squared.
}

// 0x00A348E0 (:8719): not with a non-inventory item in hand; a non-inventory weapon out of cover; another weapon not
// already carried, or a safehouse weapon; and room or a weapon to swap.
bool player_can_pickup_weapon(player* pp, item* itemp)
{
    if (player_has_non_inventory(pp))
    {
        return false;
    }
    if (itemp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        if (((weapon*)itemp)->w_info->flags.non_inventory)
        {
            return pp->cur_state != HS_COVER;
        }
        if (human_has_item(pp, itemp->info->name) != nullptr)
        {
            return false;
        }
    }
    else if (itemp->info->safehouse_weapon == nullptr)
    {
        return false;
    }
    return player_has_room_in_inventory(pp) || player_get_swap_weapon(pp) != nullptr;
}

// 0x00AB8860 (:6710): the equipped weapon leaves the player's hands and inventory, dropped where it is (a
// non-inventory weapon falls again and turns opaque).
void player_remove_item_from_hands(player* pp)
{
    inventory_item* inv_item = pp->equipped_inv_item;
    item* itemp = inv_item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(inv_item->weapon_handle) : nullptr;
    pp->set_equipped_inv_item(nullptr);
    // Original: itemp is used unchecked.
    itemp->owner_handle = 0xFFFFFFFF;
    if (!itemp->obj_flags.streamed)
    {
        itemp->item_flags.no_physics = 0;
    }
    else if (itemp->havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(itemp);
    }
    object_set_visibility_plus_children(itemp, true);
    // Inlined human_remove_item_from_inventory (all of the item).
    human_remove_item_from_inventory(pp, inv_item, inv_item->count, false);
    if (pp->desired_equipped_inv_item == nullptr)
    {
        pp->desired_equipped_inv_item = pp->last_equipped_weapon;
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
    }
}

// 0x00ABE780 (:6673): puts the non-inventory item down in front of the player (or unequips it).
bool player_set_down_non_inventory(player* pp)
{
    inventory_item* iip = pp->equipped_inv_item;
    weapon* wp = iip->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(iip->weapon_handle) : nullptr;
    if (wp != nullptr)
    {
        // Magic numbers: action aliases 0x3DE (pick up), 0x3E9 (equip and hold), 0x3DB (unequip).
        if (pp->char_inst->is_playing_action(0x3DE))
        {
            return false;
        }
        if (pp->char_inst->is_playing_action(0x3E9) && pp->desired_equipped_inv_item == pp->equipped_inv_item)
        {
            pp->char_inst->stop_action(0x3E9, 3, false);
            player_remove_item_from_hands(pp);
            pp->block_movement_anim = -1;
            // Magic number: 0.3 m ahead.
            const vector drop_pos = wp->pos + pp->orient.fvec * 0.3f;
            wp->update_pos_and_orient(drop_pos, wp->orient, true);
            return true;
        }
        if (pp->char_inst->is_playing_action(0x3DB))
        {
            return false;
        }
        human_weapons_unequip_weapon(pp, wp);
    }
    if (pp->desired_equipped_inv_item == iip)
    {
        pp->desired_equipped_inv_item = pp->last_equipped_weapon;
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
    }
    human_in_vehicle_interaction(pp, nullptr);
    pp->combat.combat_ready_timestamp.invalidate();
    return true;
}

// 0x00AC2720 (:6458): the local player standing (not in cover) lifts the non-inventory weapon in reach (0.7 m from a
// point 0.5 m ahead at 0.3 m height, with a clear line), holding it against gravity.
bool player_pickup_non_inventory(player* pp, bool do_action)
{
    item* obj_itemp = pp->action_object.handle != 0xFFFFFFFF ? World.object_from_handle<item>(pp->action_object.handle) : nullptr;
    if (Player == nullptr || pp->get_handle() != Player->get_handle() || pp->cur_stance != HST_STAND || pp->cur_state == HS_COVER)
    {
        return false;
    }
    raycast_input ray_in;
    ray_in.radius = 0.0f;
    // Magic number: the ray starts 0.1 m above the feet.
    ray_in.start = pp->pos + UP_VECTOR * 0.1f;
    ray_in.m_filter_info = COLLIDE_RAY_DEFAULT;
    ray_in.filter_params = nullptr;
    ray_in.filter_callback = nullptr;
    ray_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray_in.m_flags = 0;
    // Original: obj_itemp is used unchecked.
    ray_in.end = obj_itemp->pos;
    raycast_output ray_out;
    if (havok_raycast(ray_in, ray_out) && ray_out.m_hits[0].hit_object_handle != obj_itemp->get_handle())
    {
        return false;
    }
    const vector reach_base(pp->pos.x, pp->pos.y + 0.3f, pp->pos.z);
    const vector ahead = pp->orient.fvec.copy_flatten_normalize();
    const vector diff = obj_itemp->pos - (ahead * 0.5f + reach_base);
    if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z >= 0.7f || World.object_from_handle<weapon>(obj_itemp->get_handle()) == nullptr)
    {
        return false;
    }
    pp->non_inventory_item_handle = obj_itemp->get_handle();
    if (!do_action)
    {
        return true;
    }
    if (obj_itemp->get_parent() != 0xFFFFFFFF)
    {
        obj_itemp->remove_from_parent();
    }
    hkpRigidBody* rb = havok_body_get_pointer(obj_itemp->havok_handle);
    if (rb != nullptr)
    {
        if (havok_has_ignore_property(rb))
        {
            havok_remove_ignore_property(rb);
        }
        havok_set_ignore_property(rb, pp->get_handle());
        havok_body_set_linear_velocity(obj_itemp->havok_handle, ZERO_VECTOR);
        havok_body_set_angular_velocity(obj_itemp->havok_handle, ZERO_VECTOR);
    }
    // Magic number: 9.8 m/s^2 up cancels gravity.
    havok_body_add_custom_force(obj_itemp->havok_handle, vector(0.0f, 9.8f, 0.0f));
    // Projectiles stuck to it are reattached.
    for (object* child = obj_itemp->child_ptr; child != nullptr;)
    {
        if (child->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
        {
            obj_itemp->set_child(child, -1, -1, true, false);
        }
        child = child->child_next;
        if (child == obj_itemp->child_ptr)
        {
            break;
        }
    }
    // A persistent item with no persistent children stops being persistent.
    if (obj_itemp->obj_flags.persistent)
    {
        bool persistent_child = false;
        for (object* child = obj_itemp->child_ptr; child != nullptr;)
        {
            if (child->obj_flags.persistent)
            {
                persistent_child = true;
                break;
            }
            child = child->child_next;
            if (child == obj_itemp->child_ptr)
            {
                break;
            }
        }
        if (!persistent_child)
        {
            obj_itemp->obj_flags.persistent = 0;
        }
    }
    inventory_item* iip = human_add_item_to_inventory(pp, obj_itemp, true, false, true);
    obj_itemp->item_flags.item_held = 1;
    pp->block_steering_vector = ZERO_VECTOR;
    pp->block_movement_anim = 0x3E9;
    pp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    human_equip_inventory_item(pp, iip);
    human_in_vehicle_interaction(pp, nullptr);
    pp->combat.combat_ready_timestamp.invalidate();
    return true;
}

// 0x00ACDAB0 (:8764): the swap of a carried weapon (the swap slot, or the one player_get_swap_weapon picks) for the
// one on the ground. A safehouse weapon gives a full new weapon instead.
void player_do_weapon_swap(player* pp, unsigned int weapon_to_pickup, int swap_slot)
{
    inventory_item* swap_item;
    if (swap_slot == -1)
    {
        swap_item = player_get_swap_weapon(pp);
    }
    else
    {
        inventory_item* first = pp->inventory;
        swap_item = first;
        if (first == nullptr)
        {
            return;
        }
        while (swap_item->selection_slot != swap_slot)
        {
            swap_item = swap_item->next;
            if (swap_item == first || swap_item == nullptr)
            {
                return;
            }
        }
    }
    if (swap_item == nullptr)
    {
        return;
    }
    if (pp != nullptr && pp == Player && swap_item->weapon_handle != 0xFFFFFFFF)
    {
        weapon* old_wp = World.object_from_handle<weapon>(swap_item->weapon_handle);
        if (old_wp != nullptr && old_wp->info->special_flags.high_res_texture_requested)
        {
            old_wp->info->special_flags.high_res_texture_request_unload = 1;
        }
    }
    const bool swapping_other = swap_item != pp->equipped_inv_item;
    item* itemp = weapon_to_pickup != 0xFFFFFFFF ? World.object_from_handle<item>(weapon_to_pickup) : nullptr;
    if (itemp == nullptr || itemp->is_owned() || !itemp->obj_flags._dont_use_me)
    {
        return;
    }
    if (itemp->info->safehouse_weapon == nullptr)
    {
        if (player_has_non_inventory(pp))
        {
            player_set_down_non_inventory(pp);
        }
        else if (swapping_other)
        {
            human_weapons_drop_weapon(pp, swap_item);
            pp->second_last_equipped_weapon = pp->equipped_inv_item;
        }
        else
        {
            human_weapons_drop_equipped_weapon(pp);
            pp->second_last_equipped_weapon = pp->last_equipped_weapon;
        }
        if (!object_item_player_touch(pp, itemp))
        {
            inventory_item* new_item = human_add_item_to_inventory(pp, itemp, true, false, true);
            player_equip_item(pp, new_item, nullptr, false);
            // Magic number: 1 s between swaps.
            pp->weapon_swap_timestamp.set(1000);
            ovl_inventory_update_avail_weapons();
        }
        return;
    }
    if (!player_has_room_in_inventory(pp))
    {
        if (player_has_non_inventory(pp))
        {
            player_set_down_non_inventory(pp);
        }
        else
        {
            human_delete_item_from_inventory(pp, swap_item, false, false);
            pp->second_last_equipped_weapon = swapping_other ? pp->equipped_inv_item : pp->last_equipped_weapon;
        }
    }
    const weapon_info* w_info = weapons_get_weapon_info(itemp->info->safehouse_weapon);
    inventory_item* new_item = human_add_item_to_inventory(pp, inv_item_get_info(w_info->name), 1, (int)w_info->max_rounds + (int)w_info->magazine_size, -1, true, false, true);
    player_equip_item(pp, new_item, nullptr, false);
    pp->weapon_swap_timestamp.set(1000);
}

// 0x00AD6420 (:6373): the use callback of a weapon on the ground (do_action false only checks). A multiplayer client
// asks the server for the swap (swap_weapon in its control flags).
bool player_pickup_weapon(player* pp, bool do_action)
{
    item* itemp = pp->action_object.handle != 0xFFFFFFFF ? (item*)World.object_from_handle(pp->action_object.handle) : nullptr;
    if (itemp == nullptr || itemp->get_obj_type() != OT_ITEM)
    {
        return false;
    }
    if (!object_item_player_close_enough_to_touch(pp, itemp, false) || !player_can_pickup_weapon(pp, itemp))
    {
        return false;
    }
    if (!do_action)
    {
        return true;
    }
    gamelog_store_current_loadout(pp);
    if (itemp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        if (((weapon*)itemp)->w_info->flags.non_inventory && (Multiplayer != true || multi_is_server()))
        {
            return player_pickup_non_inventory(pp, do_action);
        }
        // Original: a weapon with room in the inventory is left to the touch pickup.
        if (player_has_room_in_inventory(pp))
        {
            return false;
        }
        if (Multiplayer && !multi_is_server())
        {
            if (player_get_swap_weapon(pp) != nullptr)
            {
                pp->mp_flags.swap_weapon = 1;
                pp->mp_action_object_handle = pp->action_object.handle;
            }
            gamelog_maybe_record_loadout_change(pp, GLLCT_WEAPON_PICKUP);
            return true;
        }
    }
    else
    {
        if (Multiplayer)
        {
            return false;
        }
        const inv_item_info* info = itemp->info->safehouse_weapon;
        if (info == nullptr)
        {
            return false;
        }
        if (human_add_item_to_inventory(pp, info, 1, -1, -1, true, false, true) != nullptr)
        {
            gamelog_maybe_record_loadout_change(pp, GLLCT_WEAPON_PICKUP);
            return true;
        }
    }
    player_do_weapon_swap(pp, pp->action_object.handle, -1);
    gamelog_maybe_record_loadout_change(pp, GLLCT_WEAPON_PICKUP);
    return true;
}

// 0x00AC2EE0 (:8908): back to the last weapon, or the lowest weapon class with ammo (melee weapons always count).
// Player_weapon_autoswap_enable is never set in retail.
bool player_weapon_autoswap(player* pp)
{
    if (!Player_weapon_autoswap_enable)
    {
        return false;
    }
    const inventory_item* last = pp->last_equipped_weapon;
    const weapon* wp = last != nullptr && last->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(last->weapon_handle) : nullptr;
    if (wp != nullptr && (wp->get_rounds_total() > 0 || wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE))
    {
        player_equip_item(pp, pp->last_equipped_weapon, nullptr, false);
        return true;
    }
    inventory_item* best = nullptr;
    int best_class = 0x11; // Magic number: past the last weapon class.
    inventory_item* iip = pp->inventory;
    if (iip == nullptr)
    {
        return false;
    }
    do
    {
        const weapon* other = iip->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(iip->weapon_handle) : nullptr;
        if (other != nullptr && (int)other->w_info->weapon_class < best_class && (other->get_rounds_total() > 0 || other->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE))
        {
            best_class = (int)other->w_info->weapon_class;
            best = iip;
        }
        iip = iip->next;
    } while (iip != pp->inventory && iip != nullptr);
    if (best != nullptr)
    {
        player_equip_item(pp, best, nullptr, false);
        return true;
    }
    return false;
}

// 0x00A5D160 (:8537)
bool local_player_is_in_vehicle(unsigned int vehicle_handle)
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        if (World.get_object<player>(i)->vehicle_handle == vehicle_handle)
        {
            return true;
        }
    }
    return false;
}

#if 0
// ---- not ported yet (the rest of the recovered stub list; stand-ins in the headless tests)
static char* Squad_name_string_pool_data; // 0x0302F218
static unsigned int $S12; // 0x035B5A48
static const voice_persona_list* player_local_get_voice_persona_list_for_creation(unsigned int mp_data); // 0x00A33D40 (:2634)
#endif

// ---- ally and escort squads (NPC.md NP1)

extern gfm_orders_manager Orders_manager;

// 0x00A5CCE0 (:4560)
// The player's ally and escort squads plan again (the player entered or left a vehicle).
void player::request_all_followers_replan() const
{
    const object_squad* ally = World.object_from_handle<object_squad>(ally_squad_handle);
    if (ally != nullptr)
    {
        ally->request_ai_replan(0xFFFFFFFF);
    }
    const object_squad* escort = World.object_from_handle<object_squad>(escort_squad_handle);
    if (escort != nullptr)
    {
        escort->request_ai_replan(0xFFFFFFFF);
    }
}

// global_event_data.h: written next to its first user, as the other event constructors are.
recruitment_event::recruitment_event()
    : event_data(EVENT_RECRUITMENT)
{
}

static const int PLAYER_SQUAD_MAX_MEMBERS = 4;
static const float SQUAD_LEAVE_WARN_DIST_SQ = 10000.0f; // 100 m
static const float SQUAD_LEAVE_WARNED_DIST_SQ = 8100.0f; // 90 m once warned, so the warning does not flicker
static const float SQUAD_REACHED_DIST_SQ = 5625.0f; // 75 m
static const float SQUAD_LEAVE_BEHIND_DIST_SQ = 15625.0f; // 125 m
static const float ASSIGN_ORDERS_RAY_LENGTH = 100.0f;
static const float SQUAD_ORDERS_MESSAGE_SECONDS = 4.0f;
// 0x02231B22 and 0x02270D39, both set by a dynamic initializer.
static const gfm_orders_flags PERMANENT_ORDERS_FLAGS(false, false);
static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false);

// 0x00A10180 (:1088)
// LTCG: np in EAX, default_description in ECX, fmt_string on the stack.
// Original quirk kept: rfg.exe formats into a thread-local scratch buffer and nothing shows it, so these messages never
// reach the screen. A VIP's name replaces the default; a VIP with an empty name was an Int3.
static void ally_squad_hud_message(const char* fmt_string, const npc* np, const char* default_description)
{
    const char* description = default_description;
    if (np != nullptr && np->h_info->flags.vip && np->h_info->name != nullptr && np->h_info->name[0] != '\0')
    {
        description = np->h_info->name;
    }
    char message[256];
    _snprintf_s(message, sizeof(message), _TRUNCATE, fmt_string, description);
}

// 0x00A842F0 (:3194): both squads use one definition. rfg.exe also builds and resets two unused parameter blocks here.
void player::create_player_squads()
{
    const squad_definition* squad_def = squad_definition_from_name(Player_squad_definition_name);
    ally_squad_handle = create_a_player_squad(squad_def);
    escort_squad_handle = create_a_player_squad(squad_def);
}

// 0x00A33DD0 (:3209)
unsigned int player::create_a_player_squad(const squad_definition* squad_def)
{
    if (squad_def == nullptr)
    {
        return 0xFFFFFFFF;
    }
    squad_spawn_parameters params;
    params.squad.squad_pos = pos;
    params.squad.squad_orient = IDENTITY_MATRIX;
    params.squad.squad_def = squad_def;
    params.squad.squad_flags.player_squad = 1;
    params.orders.team = current_team;
    params.orders.object_target = get_handle();
    params.orders.location_target = pos;
    params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.orders.ofp = PERMANENT_ORDERS_FLAGS;
    params.spawn_squad();
    if (params.spawned_squad == nullptr)
    {
        return 0xFFFFFFFF;
    }
    params.spawned_squad->set_persistent(true);
    return params.spawned_squad->get_handle();
}

static bool player_squad_has_room(unsigned int squad_handle)
{
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    return squad != nullptr && squad->member_array.size() < squad->member_array.capacity() && squad->member_array.size() < PLAYER_SQUAD_MAX_MEMBERS;
}

// 0x00A33F40 (:4055)
bool player::ally_squad_can_add() const
{
    return player_squad_has_room(ally_squad_handle);
}

// 0x00A34000 (:4336)
bool player::escort_squad_can_add() const
{
    return player_squad_has_room(escort_squad_handle);
}

// The shared end of ally_squad_add and escort_squad_add: takes np out of its old squad unless that is already one of the
// player's, in which case nothing happens. Answers whether np joined.
static bool player_squad_take_npc(player* pp, object_squad* squad, npc* np)
{
    ai_system* ai = np->get_ai();
    const unsigned int old_handle = ai->get_squad_handle();
    if (old_handle != 0xFFFFFFFF)
    {
        if (old_handle == pp->escort_squad_handle || old_handle == pp->ally_squad_handle)
        {
            return false;
        }
        ai->clear_squad();
        object_squad* old_squad = World.object_from_handle<object_squad>(old_handle);
        if (old_squad != nullptr)
        {
            old_squad->remove_member(np->get_handle());
        }
    }
    np->change_manager(NPC_MANAGER_SQUAD);
    ai->assign_squad(squad->get_handle());
    squad->add_existing_npc(np->get_handle());
    return true;
}

static void player_squad_npc_joined(player* pp, npc* np)
{
    np->set_team(pp->current_team);
    np->npc_flags.played_added_message = 0;
    np->npc_flags.too_far_from_player = 0;
    np->npc_flags.has_reached_player = 0;
    recruitment_event recruited;
    gem_post_event(recruited);
}

// The shared part of ally_squad_remove and escort_squad_remove, after the membership test.
static void player_squad_release_npc(object_squad* squad, npc* np)
{
    Cartographer.remove_icon(np->get_handle());
    squad->remove_member(np->get_handle());
    ai_system* ai = np->get_ai();
    ai->clear_squad();
    ai->perception.clear();
    np->npc_flags.played_added_message = 0;
    np->npc_flags.too_far_from_player = 0;
    np->npc_flags.has_reached_player = 0;
}

// An NPC goes back to the ambient manager when it was ambient, otherwise to NPC_MANAGER_DESPAWN (rfg.exe: AMBIENT + 5).
static npc_manager_systems player_squad_released_manager(const npc* np)
{
    return np->get_creator() != NPC_MANAGER_AMBIENT ? NPC_MANAGER_DESPAWN : NPC_MANAGER_AMBIENT;
}

// 0x00A84910 (:4072)
void player::ally_squad_add(npc* np)
{
    if (np == nullptr || !human_teams_are_allies(this, np) || !ally_squad_can_add())
    {
        return;
    }
    object_squad* squad = World.object_from_handle<object_squad>(ally_squad_handle);
    if (player_squad_take_npc(this, squad, np))
    {
        ally_squad_reassign_icons(squad);
        player_squad_npc_joined(this, np);
    }
}

// 0x00A84A50 (:4123)
void player::ally_squad_remove(npc* np)
{
    object_squad* squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (squad == nullptr || squad->member_array.find(np->get_handle(), false) < 0)
    {
        return;
    }
    player_squad_release_npc(squad, np);
    ally_squad_hud_message("%s has left your squad.", np, "An ally");
    np->change_manager(player_squad_released_manager(np));
    if (squad->member_array.size() > 0)
    {
        ally_squad_reassign_icons(squad);
    }
}

// 0x00A5CB80 (:4151)
void player::ally_squad_fall_back()
{
    object_squad* squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (squad == nullptr || squad->member_array.size() <= 0)
    {
        return;
    }
    unsigned int old_orders;
    if (squad->clear_assigned_orders(&old_orders) && old_orders != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(old_orders);
    }
    hud_message_add("Allies assigned to defend player.", SQUAD_ORDERS_MESSAGE_SECONDS, nullptr, nullptr, nullptr);
}

// size 0x8: what the assign orders ray must not hit.
struct ally_squad_assign_orders_data
{
    const player* pp;
    const object_squad* squad;
};

// 0x00A33F80 (:4178): the ray ignores the player and the squad's members.
static bool ally_squad_assign_orders_callback(object* hit_objp, void* cb_data)
{
    const ally_squad_assign_orders_data* data = (const ally_squad_assign_orders_data*)cb_data;
    if (hit_objp == data->pp)
    {
        return false;
    }
    return data->squad->member_array.find(hit_objp->get_handle(), false) < 0;
}

// 0x00A84B30 (:4194): orders the allies at what the camera aims at: defend a friendly building, otherwise attack a point
// on the nav mesh.
void player::ally_squad_assign_orders()
{
    object_squad* squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (squad == nullptr || squad->member_array.size() < 1)
    {
        return;
    }
    ally_squad_assign_orders_data filter_data;
    filter_data.pp = this;
    filter_data.squad = squad;
    raycast_input input;
    input.radius = 0.0f;
    input.start = Rfg_camera.real_pos;
    input.end = Rfg_camera.real_pos + Rfg_camera.real_orient.fvec * ASSIGN_ORDERS_RAY_LENGTH;
    input.m_filter_info = COLLIDE_PHYSICAL_OBJECT;
    input.filter_params = &filter_data;
    input.filter_callback = ally_squad_assign_orders_callback;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    raycast_output output;
    if (!havok_raycast(input, output))
    {
        hud_message_add("Not aiming at anything.", SQUAD_ORDERS_MESSAGE_SECONDS, nullptr, nullptr, nullptr);
        return;
    }
    const object* hit = World.object_from_handle(output.m_hits[0].hit_object_handle);
    vector hit_pos = output.m_hits[0].hit_pos;
    gfm_orders_creation_params params;
    params.team = HUMAN_TEAM_GUERILLA;
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const rfg_mover* building = hit != nullptr && hit->get_obj_type() == OT_MOVER && hit->get_sub_type() == OT_SUB_MOVER_RFG ? (const rfg_mover*)hit : nullptr;
    bool attack = false;
    if (building != nullptr && human_teams_are_friendly(building->team, HUMAN_TEAM_GUERILLA))
    {
        params.orders_type = GFM_ORDERS_TYPE_DEFEND;
        params.object_target = building->get_handle();
    }
    else
    {
        nav_cell* cell = bb_pf_get_nav_cell(hit_pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        navpoint* closest_navpoint = nullptr;
        float closest_dist_sq;
        bb_pf_find_closest_navpoint(hit_pos, closest_navpoint, closest_dist_sq);
        if (cell == nullptr && closest_navpoint == nullptr)
        {
            vector adjusted_pos;
            if (!bb_pf_find_closest_point_on_nav_mesh(hit_pos, adjusted_pos, cell, false) && !bb_pf_find_closest_point_on_nav_mesh_slow(hit_pos, adjusted_pos, cell))
            {
                hud_message_add("Location is inaccessible.", SQUAD_ORDERS_MESSAGE_SECONDS, nullptr, nullptr, nullptr);
                return;
            }
            hit_pos = adjusted_pos;
        }
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        params.object_target = 0xFFFFFFFF;
        attack = true;
    }
    params.location_target = hit_pos;
    const unsigned int new_orders = Orders_manager.create_new_orders(params);
    if (new_orders == 0xFFFFFFFF || Orders_manager.get_orders_from_handle(new_orders) == nullptr)
    {
        return;
    }
    const unsigned int old_orders = squad->assigned_orders_handle;
    squad->assigned_orders_handle = new_orders;
    squad->assign_orders_to_members();
    if (old_orders != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(old_orders);
    }
    hud_message_add(attack ? "Allies ordered to attack target." : "Allies ordered to defend target.", SQUAD_ORDERS_MESSAGE_SECONDS, nullptr, nullptr, nullptr);
}

// 0x00A10800 (:4296)
void player::ally_squad_call_specialist()
{
    hud_message_add("Feature not supported.", SQUAD_ORDERS_MESSAGE_SECONDS, nullptr, nullptr, nullptr);
}

// 0x00A5CC00 (:4301)
void player::ally_squad_report_member_death(const npc* np)
{
    Cartographer.remove_icon(np->get_handle());
    ally_squad_hud_message("%s has just died!", np, "An ally");
    object_squad* squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (squad == nullptr)
    {
        return;
    }
    if (squad->member_array.size() >= 1)
    {
        ally_squad_reassign_icons(squad);
        return;
    }
    unsigned int old_orders;
    if (squad->clear_assigned_orders(&old_orders) && old_orders != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(old_orders);
    }
}

// 0x00A33FC0 (:4327)
void player::ally_squad_reassign_icons(const object_squad* squad) const
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        Cartographer.add_icon(squad->member_array[i], MAP_ELEMENT_STAR_BLUE_ICON, true, false, false);
    }
}

// 0x00A9DDC0 (:4355): empties both squads, then drops every other order aimed at the player.
// Original bug kept: a member handle that no longer names an NPC is never removed, and these loops then never end.
void player::disband_ally_and_escort_squads()
{
    object_squad* ally_squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (ally_squad != nullptr)
    {
        while (ally_squad->member_array.size() > 0)
        {
            const unsigned int member = ally_squad->member_array[0];
            npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
            if (np != nullptr)
            {
                ally_squad_remove(np);
                if (!np->npc_flags.converted_guerrilla)
                {
                    np->npc_flags.despawn_npc = 1;
                }
            }
        }
    }
    object_squad* escort_squad = escort_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(escort_squad_handle) : nullptr;
    if (escort_squad != nullptr)
    {
        while (escort_squad->member_array.size() > 0)
        {
            const unsigned int member = escort_squad->member_array[0];
            npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
            if (np != nullptr)
            {
                escort_squad_remove(np, false);
                if (!np->npc_flags.converted_guerrilla)
                {
                    np->npc_flags.despawn_npc = 1;
                }
            }
        }
    }
    gfm_orders* orders = Orders_manager.master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = Orders_manager.master_orders_pool.get_next(orders);
        const unsigned int assigned = orders->get_assigned_squad_handle();
        if (assigned != ally_squad_handle && assigned != escort_squad_handle && orders->get_target_object_handle() == get_handle())
        {
            Orders_manager.destroy_orders(orders);
        }
        orders = next;
    }
}

// 0x00A84EE0 (:4405)
void player::escort_squad_add(npc* np)
{
    if (np == nullptr || !escort_squad_can_add() || !human_teams_are_friendly(this, np))
    {
        return;
    }
    object_squad* squad = World.object_from_handle<object_squad>(escort_squad_handle);
    if (player_squad_take_npc(this, squad, np))
    {
        escort_squad_reassign_icons(squad);
        player_squad_npc_joined(this, np);
    }
}

// 0x00A85010 (:4469): activity_success is unused in rfg.exe.
void player::escort_squad_remove(npc* np, bool activity_success)
{
    object_squad* squad = escort_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(escort_squad_handle) : nullptr;
    if (squad == nullptr || squad->member_array.find(np->get_handle(), false) < 0)
    {
        return;
    }
    player_squad_release_npc(squad, np);
    np->change_manager(player_squad_released_manager(np));
    escort_squad_reassign_icons(squad);
}

// 0x00A5CC90 (:4495)
void player::escort_squad_report_member_death(const npc* np)
{
    Cartographer.remove_icon(np->get_handle());
    const object_squad* squad = escort_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(escort_squad_handle) : nullptr;
    if (squad != nullptr && squad->member_array.size() > 0)
    {
        escort_squad_reassign_icons(squad);
    }
}

// 0x00A34040 (:4506)
void player::escort_squad_reassign_icons(const object_squad* squad) const
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        Cartographer.add_icon(squad->member_array[i], MAP_ELEMENT_STAR_BLUE_ICON, true, false, false);
    }
}

// 0x00A9DFA0 (:4518): runs about once a second. An empty squad waits at the player.
void player::check_squads()
{
    object_squad* ally_squad = ally_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ally_squad_handle) : nullptr;
    if (ally_squad != nullptr)
    {
        if (ally_squad->member_array.size() < 1)
        {
            gfm_orders* orders = ally_squad->get_squad_orders();
            if (orders != nullptr)
            {
                orders->set_target_position(pos);
            }
            ally_squad->update_pos_and_orient(pos, IDENTITY_MATRIX, true);
        }
        if (check_squad(ally_squad, true))
        {
            ally_squad_reassign_icons(ally_squad);
        }
    }
    object_squad* escort_squad = escort_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(escort_squad_handle) : nullptr;
    if (escort_squad != nullptr)
    {
        if (escort_squad->member_array.size() < 1)
        {
            gfm_orders* orders = escort_squad->get_squad_orders();
            if (orders != nullptr)
            {
                orders->set_target_position(pos);
            }
            escort_squad->update_pos_and_orient(pos, IDENTITY_MATRIX, true);
        }
        if (check_squad(escort_squad, false))
        {
            escort_squad_reassign_icons(escort_squad);
        }
    }
}

// 0x00A850D0 (:4581): tracks who has reached the player, warns before leaving someone behind and drops members left 125 m
// behind. Answers whether a member was dropped. VIPs are never dropped. The members' messages never show, see
// ally_squad_hud_message.
bool player::check_squad(object_squad* squad, bool is_squadmate)
{
    const char* description = is_squadmate ? "an ally" : "someone";
    const float warn_dist_sq = pflags.leaving_squad_member_behind ? SQUAD_LEAVE_WARNED_DIST_SQ : SQUAD_LEAVE_WARN_DIST_SQ;
    bool removed = false;
    bool leaving_behind = false;
    const npc* leaving_vip = nullptr; // Original quirk kept: only set for a VIP on a path VIPs never take, so always null
    int num_new = 0;
    bool new_member_on_the_way = false;
    npc* new_vip = nullptr;
    for (int i = squad->member_array.size() - 1; i >= 0; i--)
    {
        const unsigned int member = squad->member_array[i];
        npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
        if (np == nullptr)
        {
            continue;
        }
        const vehicle* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(np->vehicle_handle) : nullptr;
        const bool riding = vp != nullptr && human_in_vehicle(np, vp);
        if (!riding || np->get_ai()->get_defend_object() != this)
        {
            const float dist_sq = vec_dist_squared(np->pos, pos);
            if (!np->npc_flags.has_reached_player)
            {
                if (dist_sq <= SQUAD_REACHED_DIST_SQ)
                {
                    np->npc_flags.has_reached_player = 1;
                }
            }
            else if (dist_sq < SQUAD_LEAVE_BEHIND_DIST_SQ || np->h_info->flags.vip)
            {
                if (dist_sq < warn_dist_sq || np->h_info->flags.vip)
                {
                    np->npc_flags.too_far_from_player = 0;
                }
                else if (!np->npc_flags.too_far_from_player)
                {
                    np->npc_flags.too_far_from_player = 1;
                    leaving_behind = true;
                }
            }
            else
            {
                // Unlike ally_squad_remove this keeps the NPC's squad flags and its manager (asm 0x00A85241).
                Cartographer.remove_icon(np->get_handle());
                squad->remove_member(np->get_handle());
                ai_system* ai = np->get_ai();
                ai->clear_squad();
                ai->perception.clear();
                ally_squad_hud_message("You have left %s behind.", np, description);
                removed = true;
            }
        }
        if (is_squadmate && !np->npc_flags.played_added_message)
        {
            num_new++;
            if (!np->npc_flags.has_reached_player)
            {
                new_member_on_the_way = true;
            }
            if (np->h_info->flags.vip)
            {
                new_vip = np;
            }
            np->npc_flags.played_added_message = 1;
        }
    }
    if (leaving_behind)
    {
        if (!pflags.leaving_squad_member_behind)
        {
            pflags.leaving_squad_member_behind = 1;
            ally_squad_hud_message("You are about to leave %s behind.", leaving_vip, description);
        }
    }
    else
    {
        pflags.leaving_squad_member_behind = 0;
    }
    if (num_new == 1)
    {
        if (new_member_on_the_way)
        {
            ally_squad_hud_message("Hold on - %s is on the way!", new_vip, description);
        }
        else
        {
            ally_squad_hud_message("%s has joined your squad.", new_vip, "An ally");
        }
    }
    return removed;
}

// 0x00A5CFE0 (:8486)
float player_find_shortest_player_dist_sqrd(const vector& point)
{
    if (!Multiplayer)
    {
        return vec_dist_squared(Player->pos, point);
    }
    float best = FLT_MAX;
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        const float dist_sq = vec_dist_squared(World.get_object<player>(i)->pos, point);
        if (dist_sq < best)
        {
            best = dist_sq;
        }
    }
    return best;
}

// 0x00A5D0A0 (:8507)
float player_find_shortest_player_xz_dist_sqrd(const vector& point)
{
    if (!Multiplayer)
    {
        return vec_xz_dist_squared(Player->pos, point);
    }
    float best = FLT_MAX;
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        const float dist_sq = vec_xz_dist_squared(World.get_object<player>(i)->pos, point);
        if (dist_sq < best)
        {
            best = dist_sq;
        }
    }
    return best;
}

// 0x00A5D1C0 (:8632)
bool player_is_player_squad(unsigned int squad_handle)
{
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        const player* pp = World.get_object<player>(i);
        if (pp->ally_squad_handle == squad_handle || pp->escort_squad_handle == squad_handle)
        {
            return true;
        }
    }
    return false;
}
