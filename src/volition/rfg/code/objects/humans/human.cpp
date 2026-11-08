// Original: volition/rfg/code/objects/humans/human.cpp
//
// Partial (player_minimal.md P1, P3, P4, P7): the human object of the player subset: frame processing (animation,
// footsteps, IK, character controllers), the animation state lookup, inventory and equipping, damage and death,
// serialization and streaming, props, look-at and head positions, and the render update (the body and head skin
// renderables); hit locations, shields, stealth and inventory removal (multiplayer.md MP8); pushing and ragdolling
// other humans (the human_collide.cpp port). The rest of the recovered
// stub list stays under #if 0 at the end of the file.
// The external interface is declared in human.h.

#include "volition/rfg/code/objects/humans/human.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/havok/havok_mt_query.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/hk_custom_character_proxy.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/humans/human_fire.h"
#include "volition/rfg/code/objects/humans/human_heads_and_props.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/humans/player_hold_object.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object.inl"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/spawn/ambient_spawn_behavior.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_enums.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/weapons/weapons_enums.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ik_2link.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include <string.h>

#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/geom/character_mesh_shared.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/vlib/code/math/ix.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
// const float PI (0x01310C80) is used here too; placed in human_spinebend.cpp.
static const float FIREARM_READY_MOVE_SIDEWAYS_ANGLE = 0.820291f; // 0x01310E1C
static const float FIREARM_READY_MOVE_BACKWARDS_ANGLE = 2.391061f; // 0x01310E20
static const float FIREARM_CROUCH_MOVE_BACK_LEFT_ANGLE = -1.91983f; // 0x01310E24
// const float NORMAL_DEATH_SOUND_MAX_DIST (0x01310FF4) is used here too; placed in human_spinebend.cpp.
// const float COVER_NODE_MAX_NAVMESH_ATTACH_DIST (0x01311864) is used here too; placed in human_spinebend.cpp.
// const float NEAR_FOLLOW_DEST_DIST_SQ (0x01311B1C) is used here too; placed in human_spinebend.cpp.
static const float MAX_SIMULATE_RAMP_SPEED_FACTOR = 1.5f; // 0x01311B90, also the upper bound of aim_error_pct (human_info.cpp)
static const float min_correction_diff = 0.2f; // 0x01315520 (also read by human_combat.cpp and human_spinebend.cpp, later in the unity TU)
// 0x0342A398, a dynamic initializer: (float)cos((double)0.34906f), cos 20 degrees through the double CRT.
static const float MIN_HUMAN_IS_SHIELDED_DOT = (float)cos((double)0.34906f);
// 0x016AFE34: a player in cover attacks from cover with a melee weapon (placement inferred; not named in rfg.pdb).
static bool Melee_cover_attack_enabled = true;
static unsigned char Human_flesh_material = 0xFF; // 0x016940C1
static unsigned char Human_flesh_effect_material = 0xFF; // 0x016940C2
static bool Breath_effect_enable = true; // 0x016940C3
// s_groupIdGameplay and s_highlightId* (0x01694270-0x01694284) are header statics of nvidia_highlights.h.
// 0x01694288, read from rfg.exe: the facial pose animations (ANIM_ALIAS_FACIAL_POSE states) and the morale range
// [min, max) each is picked in.
facial_anim_level Facial_anims[10] = {
    {253, 0.0f, 0.3f}, {261, 0.0f, 0.3f}, {254, 0.0f, 0.5f}, {256, 0.0f, 0.5f}, {257, 0.0f, 0.5f},
    {255, 0.0f, 0.6f}, {262, 0.5f, 0.9f}, {258, 0.5f, 1.1f}, {259, 0.7f, 1.1f}, {260, 0.7f, 1.1f},
};
int Num_facial_anims = 10; // 0x01694300
bool No_npc_npc_damage; // 0x0302F065
anim_set* NPC_male_guerrilla_anim_set; // 0x0302F0A4
const vector Default_target_object_offset(0.0f, 1.8f, 0.0f); // 0x016947A0, read from rfg.exe: about head height
static const char* Human_vehicle_impact_foley[2] = {"MOV_BODY_CAR_HIT", "MOV_BODY_CAR_CRUNCH"}; // 0x0169430C
// bool Player_instant_unequip (0x01694314) is used here too; placed in player.cpp.
// bool Allow_player_flying_equips (0x01694315) is used here too; placed in player.cpp.
static bool Transform_combat_ready_humans = true; // 0x01694316
static bool Human_use_bone_lods = true; // 0x01694317
// The jump height (m) and the super sprint cheats' heights.
static float Default_jump_height = 1.2f; // 0x01694318
static float Super_sprint_run_jump = 6.0f; // 0x0169431C
static float Super_sprint_normal_jump = 4.0f; // 0x01694320
static int Human_corpse_cleanup_germany = 3000; // 0x01694328
// 0x01695018: one entry per voice_lines value, in enum order (names read from rfg.exe); the descriptors are found
// when the voice line table loads. External: the human_voice files use it (human_voice_lines.h).
voice_line_lookup_entry Voice_line_lookup_table[129] = {
    voice_line_lookup_entry("act_ha_00A_help_res"),
    voice_line_lookup_entry("act_ha_00A_spot_pc"),
    voice_line_lookup_entry("act_ha_00A_help"),
    voice_line_lookup_entry("act_ha_00A_help_to_pc"),
    voice_line_lookup_entry("act_ha_00A_safehouse"),
    voice_line_lookup_entry("act_ha_00A_follow_pc"),
    voice_line_lookup_entry("act_ha_00A_veh_chat"),
    voice_line_lookup_entry("act_ha_00A_veh_complain"),
    voice_line_lookup_entry("act_ha_00A_thanks"),
    voice_line_lookup_entry("act_ra_00A_fallback"),
    voice_line_lookup_entry("act_ra_00A_fallback_res_pos"),
    voice_line_lookup_entry("act_ra_00A_push"),
    voice_line_lookup_entry("act_ra_00A_push_res_pos"),
    voice_line_lookup_entry("act_ro_00A_pc_arrive"),
    voice_line_lookup_entry("act_rd_00A_pc_arrive"),
    voice_line_lookup_entry("act_rd_00A_min_def_start"),
    voice_line_lookup_entry("act_ra_00A_pc_desert"),
    voice_line_lookup_entry("act_ra_00A_reinforce_edf"),
    voice_line_lookup_entry("act_ra_00A_reinforce_gur"),
    voice_line_lookup_entry("act_ro_00A_win"),
    voice_line_lookup_entry("act_rd_00A_win"),
    voice_line_lookup_entry("act_ra_00A_win_cheer"),
    voice_line_lookup_entry("amb_veh_attacked"),
    voice_line_lookup_entry("amb_veh_imp_veh"),
    voice_line_lookup_entry("amb_ally_veh_dodge"),
    voice_line_lookup_entry("amb_enemy_veh_dodge"),
    voice_line_lookup_entry("amb_ally_veh_sacrifice"),
    voice_line_lookup_entry("amb_ally_veh_sacrifice_reluctant"),
    voice_line_lookup_entry("amb_veh_stuck"),
    voice_line_lookup_entry("amb_enemy_cower_edf"),
    voice_line_lookup_entry("amb_enemy_flee_edf"),
    voice_line_lookup_entry("amb_enemy_spot_and_turn_edf"),
    voice_line_lookup_entry("amb_pat_checkin"),
    voice_line_lookup_entry("amb_pat_civ_beat"),
    voice_line_lookup_entry("amb_pat_civ_beat_res_civ_sub"),
    voice_line_lookup_entry("amb_pat_civ_beat_res_edf_pos"),
    voice_line_lookup_entry("amb_pat_civ_req_move"),
    voice_line_lookup_entry("amb_pat_civ_req_move_res_civ_def"),
    voice_line_lookup_entry("amb_pat_civ_req_move_res_civ_sub"),
    voice_line_lookup_entry("amb_pat_civ_stomp"),
    voice_line_lookup_entry("amb_pat_civ_taunt"),
    voice_line_lookup_entry("amb_pat_civ_taunt_res_civ_def"),
    voice_line_lookup_entry("amb_pc_follow_warn"),
    voice_line_lookup_entry("amb_talkself_morale_low"),
    voice_line_lookup_entry("amb_talkself_morale_med"),
    voice_line_lookup_entry("amb_talkself_morale_high"),
    voice_line_lookup_entry("amb_pc_greet_morale_low"),
    voice_line_lookup_entry("amb_pc_greet_morale_med"),
    voice_line_lookup_entry("amb_pc_greet_morale_high"),
    voice_line_lookup_entry("amb_pc_greet_morale_low_famous"),
    voice_line_lookup_entry("amb_pc_greet_morale_med_famous"),
    voice_line_lookup_entry("amb_pc_greet_morale_high_famous"),
    voice_line_lookup_entry("amb_pat_civ_req_move_res_civ_def"),
    voice_line_lookup_entry("amb_pat_civ_req_move_res_civ_sub"),
    voice_line_lookup_entry("amb_gur_threaten"),
    voice_line_lookup_entry("amb_rep_ally_body"),
    voice_line_lookup_entry("amb_rep_charge"),
    voice_line_lookup_entry("amb_rep_disturbance"),
    voice_line_lookup_entry("com_ally_combat_flee"),
    voice_line_lookup_entry("com_ally_combat_join"),
    voice_line_lookup_entry("com_ally_congrat_kill"),
    voice_line_lookup_entry("com_ally_death_spot"),
    voice_line_lookup_entry("com_ally_fire_make_hit"),
    voice_line_lookup_entry("com_ally_fire_spot"),
    voice_line_lookup_entry("com_ally_fire_take_hit"),
    voice_line_lookup_entry("ext_ally_fire_take_hit"),
    voice_line_lookup_entry("com_ally_team_morale"),
    voice_line_lookup_entry("com_building_flee_to_ally"),
    voice_line_lookup_entry("com_veh_blow"),
    voice_line_lookup_entry("com_enemy_taunt"),
    voice_line_lookup_entry("com_enemy_veh_taunt"),
    voice_line_lookup_entry("com_enemy_walker_taunt"),
    voice_line_lookup_entry("com_enemy_tank_taunt"),
    voice_line_lookup_entry("com_enemy_edf_taunt"),
    voice_line_lookup_entry("com_enemy_gur_taunt"),
    voice_line_lookup_entry("com_enemy_mar_taunt"),
    voice_line_lookup_entry("com_enemy_veh_spot_to_ally"),
    voice_line_lookup_entry("com_enemy_mar_spot_to_ally"),
    voice_line_lookup_entry("com_enemy_edf_spot_to_ally"),
    voice_line_lookup_entry("com_enemy_tank_flee"),
    voice_line_lookup_entry("com_enemy_walker_flee"),
    voice_line_lookup_entry("com_req_cover_fire"),
    voice_line_lookup_entry("com_req_cover_fire_res_neg"),
    voice_line_lookup_entry("com_req_cover_fire_res_pos"),
    voice_line_lookup_entry("com_wep_grenade_throw_to_ally"),
    voice_line_lookup_entry("com_wep_grenade_react_to_ally"),
    voice_line_lookup_entry("com_wep_projectile_spot"),
    voice_line_lookup_entry("com_src_ord_search"),
    voice_line_lookup_entry("com_src_ord_search_res_pos"),
    voice_line_lookup_entry("com_src_threaten_area"),
    voice_line_lookup_entry("com_src_disgruntled"),
    voice_line_lookup_entry("com_src_spot_enemy"),
    voice_line_lookup_entry("ext_check_wave"),
    voice_line_lookup_entry("ext_res_spot_intruder1"),
    voice_line_lookup_entry("ext_res_warn1"),
    voice_line_lookup_entry("ext_res_warn2"),
    voice_line_lookup_entry("ext_res_bash"),
    voice_line_lookup_entry("ext_res_intruder_hostile"),
    voice_line_lookup_entry("ext_res_intruder_retreat"),
    voice_line_lookup_entry("ext_check_id"),
    voice_line_lookup_entry("ext_scan_clean"),
    voice_line_lookup_entry("ext_scan_unclean"),
    voice_line_lookup_entry("ext_execute_setup"),
    voice_line_lookup_entry("ext_execute"),
    voice_line_lookup_entry("ext_ply_wep_use"),
    voice_line_lookup_entry("ext_ply_jetpack_use"),
    voice_line_lookup_entry("saf_pur_upgrade_gen"),
    voice_line_lookup_entry("saf_pur_detector"),
    voice_line_lookup_entry("saf_pur_grinder"),
    voice_line_lookup_entry("saf_pur_jetpack"),
    voice_line_lookup_entry("saf_pur_proximity"),
    voice_line_lookup_entry("saf_pur_rpg"),
    voice_line_lookup_entry("saf_pur_shielding"),
    voice_line_lookup_entry("saf_pur_thermobaric"),
    voice_line_lookup_entry("saf_pur_welder"),
    voice_line_lookup_entry("voc_battle_cry"),
    voice_line_lookup_entry("voc_blunt"),
    voice_line_lookup_entry("voc_bomb"),
    voice_line_lookup_entry("voc_cheer"),
    voice_line_lookup_entry("voc_cough"),
    voice_line_lookup_entry("voc_death"),
    voice_line_lookup_entry("voc_death_scream"),
    voice_line_lookup_entry("voc_electric"),
    voice_line_lookup_entry("voc_flinch"),
    voice_line_lookup_entry("voc_pain"),
    voice_line_lookup_entry("voc_panic"),
    voice_line_lookup_entry("voc_thrown"),
    voice_line_lookup_entry("veh_spk_edf_chase"),
    voice_line_lookup_entry("voc_ambush"),
};
static timestamp Marauder_retreat_check_timer; // 0x016955C8, initial data ffffffff
// farray<object*,16384> Object_update_render_list (0x0169D028) is used here too; placed in object.cpp.
// game_clock Game_clock (0x025054AC) is used here too; placed in gamestate_gameplay.cpp.
// Tweak table items (0 unless tweak_table.xtbl sets them).
static float Human_ragdoll_on_vehicle_rotation; // 0x03017C54
static float Max_ambient_walk_speed_pct; // 0x0301A218
static float Human_ragdoll_on_wheels_speed; // 0x0301A230
static float Npc_use_incline_blend_tree_dist; // 0x0302386C
static float Human_ragdoll_on_vehicle_speed; // 0x03029B54
static float Min_ambient_walk_speed_pct; // 0x0302AD6C
static tweak_table_item Tweak_table_item_Npc_use_incline_blend_tree_dist("Npc_use_incline_blend_tree_dist", &Npc_use_incline_blend_tree_dist);
static tweak_table_item Tweak_table_item_Human_ragdoll_on_wheels_speed("Human_ragdoll_on_wheels_speed", &Human_ragdoll_on_wheels_speed);
static tweak_table_item Tweak_table_item_Human_ragdoll_on_vehicle_speed("Human_ragdoll_on_vehicle_speed", &Human_ragdoll_on_vehicle_speed);
static tweak_table_item Tweak_table_item_Human_ragdoll_on_vehicle_rotation("Human_ragdoll_on_vehicle_rotation", &Human_ragdoll_on_vehicle_rotation);
static tweak_table_item Tweak_table_item_Min_ambient_walk_speed_pct("Min_ambient_walk_speed_pct", &Min_ambient_walk_speed_pct); // 0x03067AB0
static tweak_table_item Tweak_table_item_Max_ambient_walk_speed_pct("Max_ambient_walk_speed_pct", &Max_ambient_walk_speed_pct); // 0x03067AD4
// player* Player (0x03023874) is used here too; placed in player.cpp.
// head_manager Head_managers[18] (0x030299D8) is used here too; placed in human_heads_and_props.cpp.
static int Human_vehicle_impact[2]; // 0x0302F028
static int Ai_reuse_turret_after_death_time; // 0x03029B50, seconds (tweak table item)
int Ai_reuse_turret_after_nearby_explosive = 10; // 0x01694324
static tweak_table_item Tweak_table_item_Ai_reuse_turret_after_death_time("Ai_reuse_turret_after_death_time", &Ai_reuse_turret_after_death_time); // 0x033AFED0
static tweak_table_item Tweak_table_item_Ai_reuse_turret_after_nearby_explosive("Ai_reuse_turret_after_nearby_explosive", &Ai_reuse_turret_after_nearby_explosive); // 0x033C143C
// 0x01315704, read from rfg.exe: the test voice persona of each team (civilians use the guerrilla one).
static const char* Test_persona_names[NUM_HUMAN_TEAMS] = {"TEST_GUR", "TEST_EDF", "TEST_GUR", "TEST_MAR"};
// 0x030299D8: one per loaded head container. Defined here (not in human_heads_and_props.cpp) because human.cpp is the
// first user in the unity TU.
static head_manager Head_managers[18];
static bool Human_voice_use_test_persona; // 0x0302F049
human_voice_line_info* Voice_line_descriptors; // 0x0302F08C, external: the human_voice files use it (human_voice_lines.h)
static checksum_stri MULTI_ROCKETS_TIP_NAME_CHECKSUM("Multi_Rockets", 0); // 0x0303CAC4
static object_pool<attach_info_data,512,1> Attach_info_pool; // 0x0304BAA0
static cs_wrapper animation_threading_cs(0); // 0x03187274
static checksum_stri THERMO_ROCKET_CHECKSUM("thermobaric_rockets", 0); // 0x033B590C
static checksum_stri HYDROGEN_TANKS_TIP_NAME_CHECKSUM("Hydrogen_Tanks", 0); // 0x033D4D7C
static checksum_stri THERMO_ROCKETS_TIP_NAME_CHECKSUM("Thermobaric_Rocket", 0); // 0x03429820
static checksum_stri HYDROGEN_TANK_CHECKSUM("hydrogen_tank", 0); // 0x03500C90
static checksum_stri PROPANE_TANK_CHECKSUM("propane_tank", 0); // 0x03538998
static checksum_stri RPG_LAUNCHER_CHECKSUM("rpg_launcher", 0); // 0x0353899C
// unsigned int $S5, $S6, $S7 (0x035B5A10, 0x035B5A64, 0x035B5A2C): the guards of function statics.

// ---- file-static globals compiled since P3
// 0x01310F38, [team1][team2], read from rfg.exe: guerrillas and EDF hostile, civilians friendly to guerrillas and
// unfriendly to the EDF, marauders hostile to everyone.
static const human_team_dispositions Default_human_team_dispositions[NUM_HUMAN_TEAMS][NUM_HUMAN_TEAMS] = {
    {TEAM_DISPOSITION_ALLIED, TEAM_DISPOSITION_HOSTILE, TEAM_DISPOSITION_FRIENDLY, TEAM_DISPOSITION_HOSTILE},
    {TEAM_DISPOSITION_HOSTILE, TEAM_DISPOSITION_ALLIED, TEAM_DISPOSITION_UNFRIENDLY, TEAM_DISPOSITION_HOSTILE},
    {TEAM_DISPOSITION_FRIENDLY, TEAM_DISPOSITION_UNFRIENDLY, TEAM_DISPOSITION_ALLIED, TEAM_DISPOSITION_HOSTILE},
    {TEAM_DISPOSITION_HOSTILE, TEAM_DISPOSITION_HOSTILE, TEAM_DISPOSITION_HOSTILE, TEAM_DISPOSITION_ALLIED},
};
static unsigned int breath_effect = 0xFFFFFFFF; // 0x01694308
static anim_set* Civilian_female_anim_set; // 0x0301A22C
static int Human_melee_blood_decal_id; // 0x0301AE74
static inventory_item* Inv_item_free_list; // 0x0302366C
static int Ladder_slide_loop_cue; // 0x03024E70
static int Ladder_slide_land_cue; // 0x030269C0
static int Num_inv_items; // 0x0302F0E8

int Bone_lods[100]; // 0x03023690, the coarsest LOD at which each rig bone is still animated

// 0x005DBFF0 (.h:630, inline in the original header)
human_teams human::get_team() const
{
    return current_team;
}

// 0x00AB6080 (:16437): first of object_system_init's systems. The headless start-up calls it directly until
// object_system_init does (P4).
void human_system_init()
{
    human_teams_reset_all_dispositions();
    ai_system_init();
    // human_combat_system_init, inlined
    parse_melee_table(-1, false);
    Human_melee_blood_decal_id = decal_info_find("Test_decal_blood");
    Explosion_gibs_effect = effects_get_handle("char_blood_gibs_a");
    Riot_shield_melee_group = rfg_get_melee_group_index("Riot_shield");
    Marauder_block_effect_idx = rfg_get_effect_material_index("Steel");
    audiolib_cue_get_id("MOV_FOLEY_LADDER_LOOP_01", &Ladder_slide_loop_cue);
    audiolib_cue_get_id("MOV_FOLEY_LADDER_RELEASE_01", &Ladder_slide_land_cue);
    Civilian_female_anim_set = anim_set_find("Civilian_Female");
    breath_effect = effects_get_handle("char_breath");
    // Magic numbers: the bone LOD table of the original (stored there one bone at a time), indexed by bone of the
    // common human rig; bones 52-99 stay 0.
    memset(Bone_lods, 0, sizeof(Bone_lods));
    static const int bone_lods[52] = {
        3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 3, 0, 0, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1,
        1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    };
    memcpy(Bone_lods, bone_lods, sizeof(bone_lods));
}

// The controller of a human. Original: the rdd pointer is masked to null when not loaded (and read anyway).
static inline character_controller* human_controller(human* hp)
{
    return hp->rdd.get_data()->controller;
}

// 0x00A9A3A0 (:6549): the character controllers of all integrated humans step together. Each human's state is set
// up (and its phantom's broadphase box grown), the initial casts of all humans are queued, then up to 10 iterations
// solve and cast for every human still moving, waiting for the queued casts (havok_mt_query) between the passes.
// The debris pushes and the final moves follow for everyone.
// Static in the original; external here so that p1_controller_test can drive it until P4 ports
// human_process_controllers with its pre- and post-integration.
void human_process_controllers_integrate_mt(human_integration_parameters* human_integration_params, unsigned int num_humans, float time_step)
{
    for (unsigned int i = 0; i < num_humans; i++)
    {
        human_integration_parameters& params = human_integration_params[i];
        if (!params.m_integrate)
        {
            continue;
        }
        character_controller* cc = human_controller(params.m_human);
        if (params.m_refresh_manifold)
        {
            hk_custom_character_proxy* proxy = cc->m_characterProxy;
            proxy->refreshManifold_custom(proxy->get_start_point_collector(), proxy->get_cast_collector());
        }
        cc->update_state_for_integration(params.m_human, params.m_desired_velocity, params.m_start_jump_height, time_step, params.m_check_support, params.m_use_velocity, params.m_ignore_y);
        cc->flags.phantom_added_to_world = 1;
        cc->update_broadphase_for_integration(time_step);
    }

    hkArray<human*, hkContainerHeapAllocator> listA;
    hkArray<human*, hkContainerHeapAllocator> listB;
    if (num_humans != 0)
    {
        listA.reserveExactly((int)num_humans);
        listB.reserveExactly((int)num_humans);
    }
    for (unsigned int i = 0; i < num_humans; i++)
    {
        if (human_integration_params[i].m_integrate)
        {
            human* hp = human_integration_params[i].m_human;
            listA.pushBack(hp);
            human_controller(hp)->mt_integrate_begin(time_step);
        }
    }

    hkArray<human*, hkContainerHeapAllocator>* humans = &listA;
    hkArray<human*, hkContainerHeapAllocator>* next = &listB;
    int iteration = 0;
    while (humans->getSize() > 0)
    {
        const int count = humans->getSize();
        const bool last_iteration = iteration == 9; // 10 iterations at most
        havok_mt_query_wait();
        for (int i = 0; i < count; i++)
        {
            human_controller((*humans)[i])->mt_integrate_process_casts(time_step);
        }
        havok_mt_query_wait();
        for (int i = 0; i < count; i++)
        {
            human* hp = (*humans)[i];
            human_controller(hp)->mt_integrate_further_casts(*next, hp, last_iteration);
        }
        iteration++;
        humans->setSizeUnchecked(0);
        hkArray<human*, hkContainerHeapAllocator>* t = humans;
        humans = next;
        next = t;
    }

    for (unsigned int i = 0; i < num_humans; i++)
    {
        if (human_integration_params[i].m_integrate)
        {
            human_controller(human_integration_params[i].m_human)->mt_push_objects();
        }
    }
    for (unsigned int i = 0; i < num_humans; i++)
    {
        if (human_integration_params[i].m_integrate)
        {
            human* hp = human_integration_params[i].m_human;
            character_controller* cc = human_controller(hp);
            cc->mt_integrate_finish();
            cc->post_integrate(hp);
        }
    }
}


// ---- internal functions
static human_team_dispositions human_teams_get_disposition_internal(human_teams team1, human_teams team2); // 0x00A0D270, defined in human_teams.cpp later in the unity TU
static void queue_render_update(object* objp, object_flags& flags, unsigned char render_flags); // object.cpp, later in the unity TU
static void human_move_path_set(human* hp, const vector& pos, const vector& dest, uint64_t end_nav_cell_hint, bool out_of_chunk); // 0x00A30BA0, human_move.cpp, later in the unity TU
static void create_physics_object(item* ip); // 0x00A64340, object_item.cpp, later in the unity TU
static void human_animation_processing(human* hp);
static unsigned int get_default_collision_filter(human* hp);
static void human_animation_process_postsim(human* hp);
static void human_play_footstep_sound(human* hp, footstep_version foot, footstep_variation variation);
static void human_do_footsteps(human* hp);
static void human_ik_process(human* hp);
static void human_animation_pre_process(human* hp);
static weapon_class_type get_firearm_weapon_class(human* hp);
static weapon_class_type get_melee_weapon_class_type(human* hp);
static weapon_class_type get_thrown_weapon_class_type(human* hp);
static bool human_should_breath_effect_play(const human* hp);
static void get_weapon_animation_state(weapon_class_type weapon_class, human_states, human_movement_states, human* hp, animation_state& anim_state, vanim_aliases& anim_alias, float& blend_time, const weapon_anim_flags& old_flags);
static void get_unarmed_animation_state(human_movement_states m_state, human* hp, animation_state& anim_state, vanim_aliases&);
static void human_lookup_animation_state(human* hp, animation_state& new_state, vanim_aliases& anim_alias, float& blend_time);
static void human_set_steering_heading_offset(human* hp);
static int human_find_open_inventory_selection_slot(human* hp);
static void insert_item_into_inventory(human* hp, inventory_item* inv_item);
static void human_health_damage_register(human* hp, float);
static void human_restore_health(human* hp);
static void human_get_head_pos_from_stance(const human* hp, vector& head_pos);
static unsigned int human_process_controllers_pre_integrate(human_integration_parameters* human_integration_params, unsigned int max_humans, float time_step);
static void human_process_controllers_post_integrate(human_integration_parameters* human_integration_params, unsigned int num_humans);
static bool should_kill_when_damaged(const human* const hp, damaged_by_types dbt);
static void kill_light_effects(human* hp);
static void human_modify_animation_set(character_instance* char_inst);
static void human_create_prop_item(human* hp, const human_info::human_prop_info& prop, prop_type type, const obj_item_flags& flags);

// ---- inline methods of human.h (compiled here)

// 0x005FB420 (.h:299)
// Every named flag cleared (all four words).
human_flags::human_flags()
{
    memset(this, 0, sizeof(*this));
}

// 0x005DBFB0 (.h:465)
void human_flags::reset()
{
    memset(this, 0, sizeof(*this));
}

// 0x00A09380 (:16928)
void human_flags_bitwise_or(const human_flags& s1, const human_flags& s2, human_flags& return_value)
{
    for (int i = 0; i < sizeof(human_flags); i++)
    {
        ((unsigned char*)&return_value)[i] = ((const unsigned char*)&s1)[i] | ((const unsigned char*)&s2)[i];
    }
}

// 0x00942F60 (.h:470)
void human_mp_flags::set_charge_weapon(unsigned int _charge_weapon)
{
    charge_weapon = _charge_weapon;
}

// 0x00942F80 (.h:471)
unsigned int human_mp_flags::get_charge_weapon() const
{
    return charge_weapon;
}

// 0x00942F90 (.h:474)
void human_mp_flags::set_running_attack(unsigned int _running_attack)
{
    running_attack = _running_attack;
}

// 0x00942FB0 (.h:475)
unsigned int human_mp_flags::get_running_attack() const
{
    return running_attack;
}

// 0x00A2C5F0 (.h:529)
weapon_anim_flags::weapon_anim_flags()
{
    clear();
}

// 0x00A056A0 (.h:530)
// The whole word is cleared.
void weapon_anim_flags::clear()
{
    memset(this, 0, sizeof(*this));
}

// 0x00A83AA0
// The members' constructors set every timestamp invalid (-1) and the render distance 0; human_combat's likewise.
human::human()
{
}

// 0x00A83C40 (.h:560)
const char* human::get_classname() const
{
    return "human";
}

// 0x00A83C50 (.h:560): humans are pooled by their classes (player, npc).
void human::pool_free()
{
}

// 0x00A074E0 (:5172)
inventory_item* human::get_last_equipped_weapon() const
{
    return last_equipped_weapon;
}

// 0x00A52E70 (:4813)
object_action_node* human::get_desired_action_node() const
{
    if (action_node_data.action_node_set.size() < 1 || action_node_data.action_node_set[0] == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(action_node_data.action_node_set[0]);
    return objp != nullptr && objp->get_obj_type() == OT_ACTION_NODE ? static_cast<object_action_node*>(objp) : nullptr;
}

// 0x00A07910 (:7885)
void human_set_stance(human* hp, human_stances stance)
{
    if (stance != HST_CURRENT)
    {
        hp->cur_stance = stance;
    }
}

// 0x00A2E900 (:12777)
bool human_in_vehicle(const human* hp, unsigned int vehicle_handle)
{
    const object* objp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(vehicle_handle) : nullptr;
    return human_in_vehicle(hp, objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? static_cast<const vehicle*>(objp) : nullptr);
}

// 0x00A53C90 (:12783)
bool human_in_flyer(const human* p_human)
{
    if (p_human == nullptr)
    {
        return false;
    }
    const vehicle* vp = World.object_from_handle<vehicle>(p_human->vehicle_handle);
    return human_in_vehicle(p_human, vp) && vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER;
}

// 0x00A09270 (:14497)
bool human_is_driving(const human* hp)
{
    return hp->cur_state == HS_VEHICLE_DRIVING;
}

// 0x00A08710 (:13194)
void human_stop_playing_turn(human* hp, int ramp_out_frames, bool pause)
{
    hp->char_inst->stop_action(ANIM_ALIAS_TURN_LEFT, ramp_out_frames, pause);
    hp->char_inst->stop_action(ANIM_ALIAS_TURN_RIGHT, ramp_out_frames, pause);
}

// 0x00A08540 (:12641). While the pause lasts the human stands; afterwards it moves as it did before.
bool human::do_avoidance_pause_frame()
{
    if (!avoidance_pause_timer.valid())
    {
        return false;
    }
    if (avoidance_pause_timer.elapsed())
    {
        avoidance_pause_timer.invalidate();
        if (avoidance_original_movement_state != HMS_CURRENT)
        {
            cur_movement_state = avoidance_original_movement_state;
        }
        move_speed = -1.0f;
        return false;
    }
    steering_vector = ZERO_VECTOR;
    move_speed = -1.0f;
    return true;
}

// 0x00A2EB90 (:14038)
void human_enable_gravity(human* p_human, bool enable)
{
    if (p_human->rdd.is_loaded())
    {
        p_human->rdd.get_data()->controller->flags.disable_gravity = !enable;
    }
}

// 0x00AB1450 (:4555)
bool human::at_entry_point(unsigned int node_handle) const
{
    object* objp = node_handle != 0xFFFFFFFF ? World.object_from_handle(node_handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_ACTION_NODE)
    {
        return false;
    }
    return static_cast<object_action_node*>(objp)->at_entry_point(this);
}

// 0x00A7B120 (:4540)
bool human::has_entered_head_action_node() const
{
    if (action_node_data.action_node_set.size() <= 0 || action_node_data.action_node_set[0] == 0xFFFFFFFF)
    {
        return false;
    }
    const object* head = World.object_from_handle(action_node_data.action_node_set[0]);
    return head != nullptr && head->get_obj_type() == OT_ACTION_NODE && head->get_handle() != 0xFFFFFFFF && action_node_data.current_action_node == head->get_handle();
}

// 0x00A2DAB0 (:4700)
void human::release_all_action_nodes()
{
    action_node_data.action_node_set.remove_all();
    action_node_data.action_state = 0;
    action_node_data.action_sub_state = 0;
    action_node_data.exit_quick = true;
    action_node_data.current_action_node = 0xFFFFFFFF;
    action_node_data.state_duration.invalidate();
}

// 0x00A073D0 (:4805)
int human::get_lifetime() const
{
    return lifetime.valid() ? lifetime.time_since() : 0;
}

// 0x00A073F0
unsigned int human::get_current_action_node_handle() const
{
    return action_node_data.current_action_node;
}

// 0x00A2DC20 (:4834)
object_action_node* human::get_current_action_node(bool get_even_if_dead) const
{
    if ((!get_even_if_dead && is_killed()) || action_node_data.current_action_node == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(action_node_data.current_action_node);
    return (objp != nullptr && objp->get_obj_type() == OT_ACTION_NODE) ? reinterpret_cast<object_action_node*>(objp) : nullptr;
}

// 0x00A7B370 (:4891)
void human::get_reserved_nodes(farray<unsigned int, 4>& action_node_set) const
{
    action_node_set.copy(action_node_data.action_node_set);
}

// 0x00A074F0 (:5177)
inventory_item* human::get_equipped_inv_item() const
{
    return equipped_inv_item;
}

// 0x00A07500 (:5182)
inventory_item* human::get_desired_inv_item() const
{
    return desired_equipped_inv_item;
}

// 0x00A52D70 (:4519): a property block with the position, orientation and name, and the human info
obj_human_props::obj_human_props(const human_info* h_info, const vector& pos, const matrix& orient, const char* name)
    : obj_prop_block(pos, orient, name)
{
    set<const human_info*>(HUMAN_PROP_CINFO, h_info);
}

// 0x00760B90: obj_prop_block's destructor (inlined)
obj_human_props::~obj_human_props()
{
}

// 0x01310E2C: min, average and max speed (m/s) of idle, walk, run, sprint (human_movement_states).
extern const human_move_speed_info Default_move_speed_block[4] =
{
    { 0.0f, 0.0f, 0.8f },
    { 0.7f, 1.3f, 2.25f },
    { 2.0f, 4.5f, 5.75f },
    { 5.5f, 6.2f, 12.75f },
};

// 0x00A07300 (:4501)
const human_move_speed_info* human_get_move_speed_info(const human* hp, human_movement_states m_state)
{
    if (m_state < NUM_HUMAN_MOVEMENT_STATES)
    {
        return &Default_move_speed_block[m_state];
    }
    return nullptr;
}

// 0x00A07320 (:4507)
const human_move_speed_info (&human_get_move_speed_block(const human* hp))[4]
{
    return Default_move_speed_block;
}

// 0x00A07330 (:4530): a guerrilla while the not-undercover timer has run out.
bool human::is_undercover() const
{
    return current_team == HUMAN_TEAM_GUERILLA && combat.not_undercover_timestamp.elapsed();
}

// 0x00A83C60
human::~human()
{
}

// ---- header inlines (global_event_data.h)
// 0x00943040 (global_event_data.h:56)
weapon_acquired_event::weapon_acquired_event()
    : event_data(EVENT_WEAPON_ACQUIRED)
{
}

// 0x00A05870 (global_event_data.h:60)
ammo_acquired_event::ammo_acquired_event()
    : event_data(EVENT_AMMO_ACQUIRED)
{
}

// 0x00A05890 (global_event_data.h:99)
human_dead_event::human_dead_event()
    : event_data(EVENT_HUMAN_DEAD)
{
}

// 0x00A05910 (global_event_data.h:188)
player_kills_human_event::player_kills_human_event()
    : event_data(EVENT_PLAYER_KILLS_HUMAN)
{
}

// 0x00A05980 (global_event_data.h:291)
npc_despawned::npc_despawned()
    : event_data(EVENT_NPC_DESPANED)
{
}

// 0x00A05990 (global_event_data.h:295)
player_fires_weapon_or_detonates_event::player_fires_weapon_or_detonates_event()
    : event_data(EVENT_PLAYER_FIRES_WEAPON)
{
}

// 0x00A059A0 (global_event_data.h:298)
human_stopped_being_rendered_event::human_stopped_being_rendered_event()
    : event_data(EVENT_HUMAN_STOPPED_BEING_RENDERED)
{
}

// 0x00AD9970 (:1146). LTCG: hp in ESI.
// The animation pass of a human (human_do_frame): a released blocking movement ends once the human steers away
// from it, a flinch stops blocking movement at its release trigger, the state machine runs while alive (or while
// the death animation plays), then the spine bending and the bone transform request of this frame.
static void human_animation_processing(human* hp)
{
    animation_threading_cs.enter();
    if (hp->released_movement_anim != -1)
    {
        if (vanim_is_playing_action(hp->char_inst->vp, hp->released_movement_anim))
        {
            const block_movement_types type = hp->released_movement_type;
            // Magic number: the binary compares the dot product with (double)0.9f.
            if (!hp->steering_vector.is_null() &&
                (type == BLOCK_MOVEMENT_POS || type == BLOCK_MOVEMENT_POS_AND_ORIENT ||
                 (type == BLOCK_MOVEMENT_ORIENT && (double)(hp->steering_vector * hp->orient.fvec) < (double)0.9f)))
            {
                vanim_ramp_out_action(hp->char_inst->vp, hp->released_movement_anim, 4);
                hp->released_movement_anim = -1;
                if (hp->block_movement_anim == -1)
                {
                    hp->block_movement_anim = -1;
                    hp->block_movement_type = BLOCK_MOVEMENT_ORIENT;
                }
            }
        }
        else
        {
            hp->released_movement_anim = -1;
        }
    }
    if (vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_FLINCH) &&
        vanim_has_trigger_elapsed(hp->char_inst->vp, Trigger_release_movement, ANIM_ALIAS_FLINCH, -1))
    {
        v_anim* action = vanim_get_action(hp->char_inst->vp, ANIM_ALIAS_FLINCH);
        action->alias = ANIM_ALIAS_NONE;
        action->index = -1;
    }
    if (!hp->is_killed() || vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_DEATH))
    {
        human_set_and_process_animation_states(hp);
        hp->root_bone_offset = 0.0f;
        v_animate* vp = hp->char_inst->vp;
        // Magic numbers: editor flag 0x20 of the civilian female set lowers the root bone by 6.5 cm.
        if (vp->animation_set == Civilian_female_anim_set && vanim_check_editor_flag(vp, 0x20))
        {
            hp->root_bone_offset = -0.065f;
        }
    }
    const human_ragdoll_state rs = hp->ragdoll_state;
    if (rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || rs == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        human_spinebend_pre_process(hp);
    }
    human_animation_pre_process(hp);
    animation_threading_cs.leave();
}

// 0x00A52AF0 (:1289). LTCG: hp in ESI.
// The collision filter of a human's character proxy for its state: hidden humans and humans at an ammo box do not
// collide, humans in or entering vehicles use the vehicle's filter where the vehicle wants that, dead and ragdolled
// humans use the dead filter. Players carry their system group in the upper half.
static unsigned int get_default_collision_filter(human* hp)
{
    if (hp->hflags.hidden)
    {
        return COLLIDE_NO_COLLISION;
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    npc* np = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? (npc*)hp : nullptr;
    switch (hp->cur_state)
    {
    case HS_DEAD:
        return COLLIDE_DEAD_HUMAN;
    case HS_LADDER:
        return COLLIDE_HUMAN_ON_LADDER;
    case HS_VEHICLE_ENTERING:
    {
        vehicle* vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
        if (vp != nullptr && vp->havok_handle != 0xFFFFFFFF && vp->vii.humans[vp->vii.find_human(hp)].use_vehicle_collision)
        {
            return vehicle_get_collision_filter(vp);
        }
        return COLLIDE_HUMAN_MINIMUM;
    }
    case HS_VEHICLE_SITTING:
    case HS_VEHICLE_DRIVING:
    {
        if (human_in_players_vehicle(hp))
        {
            return COLLIDE_HUMAN_MINIMUM;
        }
        vehicle* vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
        if (vp == nullptr)
        {
            return COLLIDE_HUMAN_MINIMUM;
        }
        if (vp->info->default_team == HUMAN_TEAM_EDF && human_in_protected_vehicle_pos(hp))
        {
            return COLLIDE_HUMAN_MINIMUM;
        }
        if (vp->havok_handle != 0xFFFFFFFF)
        {
            return vehicle_get_collision_filter(vp);
        }
        break;
    }
    case HS_VEHICLE_EXITING:
    {
        vehicle* vp = World.object_from_handle<vehicle>(hp->vehicle_handle);
        if (vp == nullptr)
        {
            return COLLIDE_HUMAN_MINIMUM;
        }
        if (vp->havok_handle != 0xFFFFFFFF && vp->vii.humans[vp->vii.find_human(hp)].use_vehicle_collision)
        {
            return vehicle_get_collision_filter(vp);
        }
        break;
    }
    case HS_AMMO_BOX:
        return COLLIDE_NO_COLLISION;
    default:
        break;
    }
    if (!hp->is_killed() && (!human_is_ragdolled(hp) || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT))
    {
        if (pp != nullptr)
        {
            return (pp->havok_filter_group << 16) | COLLIDE_PLAYER;
        }
        if (np != nullptr && np->npc_flags.dont_collide_with_humans)
        {
            return COLLIDE_HUMAN_NOT_OTHER_HUMANS;
        }
        return COLLIDE_NPC;
    }
    return COLLIDE_DEAD_HUMAN;
}

// 0x00AD9B20 (:1411)
// After the simulation, a ragdolled human's pose follows its ragdoll: an active ragdoll writes the animation
// matrices back (with the eyes rolled up) and is destroyed or gets up when Havok says it should; a flinch drives
// the ragdoll from the animation; an impact ragdoll becomes a full one when it hits a wall or is shot.
static void human_animation_process_postsim(human* hp)
{
    if (hp->hflags.dont_deform_bones || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        return;
    }
    bool stop_ragdoll_now = false;
    switch (hp->ragdoll_state)
    {
    case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
        hp->ragdoll_on_impact_time -= Frametime;
        if (!havok_ragdoll_did_hit_wall(hp->ragdoll_idx) && !hp->hflags.ragdoll_shot)
        {
            if (hp->ragdoll_on_impact_time >= 0.0f)
            {
                havok_sync_ragdoll_to_anim(hp->ragdoll_idx, hp->pos, hp->orient, hp->actual_velocity, 0.0f, 1.0f, false, hp->hflags.ragdoll_on_impact_use_ragdoll_pos, true);
                if (hp->hit_points <= 0.0f)
                {
                    hp->set_render_flags(4);
                }
                return;
            }
            human_ragdoll_stop(hp, false);
            return;
        }
        havok_ragdoll_add_constraints_to_world(hp->ragdoll_idx);
        human_ragdoll_start(hp, false, 0xFFFFFFFF);
        hp->ragdoll_on_impact_min_vel = 0.0f;
        // fall through: the ragdoll is active now
    case HUMAN_RAGDOLL_STATE_ACTIVE:
    {
        havok_ragdoll_set_gravity(hp->ragdoll_idx, true);
        if (!hp->hflags.bones_transformed_this_frame)
        {
            // Magic numbers: the approximate half extents of a standing human.
            hp->approx_local_bmax.x = 0.5f;
            hp->approx_local_bmax.y = 1.8f;
            hp->approx_local_bmax.z = 0.5f;
            return;
        }
        havok_sync_anim_to_ragdoll(hp->ragdoll_idx, hp->pos, hp->orient);
        hp->set_render_flags(4);
        havok_ragdoll_get_local_bbox(hp->approx_local_bmin, hp->approx_local_bmax, hp->ragdoll_idx, hp->pos, IDENTITY_MATRIX);
        matrix43* model_matrices;
        animlib_get_instanced_model_matrices(hp->char_inst->aim_handle, &model_matrices);
        if (model_matrices != nullptr)
        {
            matrix43* v2m = animlib_get_vertex_to_model_matrices(hp->char_inst->aim_handle);
            // Magic number: -8 degrees (0.139624 rad) around x rolls a dead human's eyes up.
            const int eye_bones[2] = {hp->h_info->eye_l_bone_index, hp->h_info->eye_r_bone_index};
            for (int i = 0; i < 2; i++)
            {
                const int bone = eye_bones[i];
                if (bone > 0)
                {
                    matrix43 inv = animlib_get_bone_inv_transformation(hp->char_inst->aim_handle, bone);
                    model_matrices[bone].m_rotation.rotate_around_local_x(-0.139624f);
                    v2m[bone] = inv * model_matrices[bone];
                }
            }
            animlib_validate_instanced_model_matrices(hp->char_inst->aim_handle);
        }
        // Original: the masked rdd pointer is read without a check (a ragdolled human is loaded).
        human::resource_dependent_data* rdd = hp->rdd.get_data();
        if (rdd->head_manager_idx >= 0 && animlib_allocate_bones(rdd->head_aim_handle))
        {
            matrix43 tmp_matrices[64];
            animlib_transform_all_bones_pre(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
            animlib_transform_all_bones_post(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
        }
        if (!havok_ragdoll_should_destroy(hp->ragdoll_idx))
        {
            return;
        }
        if (Multiplayer && !object_is_local_player(hp))
        {
            return;
        }
        if (wrecking_crew_is_active())
        {
            return;
        }
        if (human_is_dead(hp) && !object_is_local_player(hp))
        {
            World.destroy_object(hp, false);
            return;
        }
        if (!human_is_dead(hp))
        {
            human_ragdoll_get_up(hp, true);
            return;
        }
        stop_ragdoll_now = true;
        human_ragdoll_stop(hp, stop_ragdoll_now);
        break;
    }
    case HUMAN_RAGDOLL_STATE_BLEND_OUT:
    case HUMAN_RAGDOLL_STATE_POSE_SAVED:
        break;
    case HUMAN_RAGDOLL_STATE_FLINCH:
    {
        havok_sync_ragdoll_to_anim(hp->ragdoll_idx, hp->pos, hp->orient, hp->actual_velocity, 0.0f, 1.0f, false, false, false);
        havok_blend_anim_to_ragdoll(hp->ragdoll_idx, hp->pos, hp->orient, 1.0f);
        vector delta_v = hp->actual_velocity - hp->ragdoll_last_frame_velocity;
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, delta_v, true);
        hp->ragdoll_last_frame_velocity = hp->actual_velocity;
        break;
    }
    case HUMAN_RAGDOLL_STATE_DEACTIVATED:
        if (hp->cloth_sim != nullptr)
        {
            hp->cloth_sim->initialize_nodes = 1;
        }
        break;
    default:
        // Original: an Int3 debug message.
        break;
    }
}

// 0x00A99620 (:1557). LTCG: hp in ESI.
// A footstep near the local player (10 m): the walk or run foley of the human's footstep effects, then the ground
// material's footstep effect.
static void human_play_footstep_sound(human* hp, footstep_version foot, footstep_variation variation)
{
    if (Player == nullptr)
    {
        return;
    }
    const vector d = Player->pos - hp->pos;
    // Magic number: 100 = 10 m squared.
    if (d.x * d.x + d.y * d.y + d.z * d.z > 100.0f)
    {
        return;
    }
    int foley = -1;
    bool play_foley = true;
    if (variation == FOOTSTEP_WALKING)
    {
        foley = hp->h_info->foot_effects->walk_foley;
    }
    else if (variation == FOOTSTEP_RUNNING)
    {
        foley = hp->h_info->foot_effects->run_foley;
    }
    else
    {
        play_foley = false;
    }
    if (play_foley && foley != -1)
    {
        gameaudio_play_cross_linked_sound(foley, hp->get_handle(), nullptr, nullptr);
    }
    unsigned char material;
    material_shape_type shape_type;
    if (!human_get_standing_on_material(hp, material, shape_type) || material != 0xFF)
    {
        const unsigned int effect = hp->h_info->foot_effects->get_footstep_effect_handle(material, shape_type, foot, variation);
        effects_play(effect, hp->pos, hp->orient, false, 0xFFFFFFFF, -1, hp->get_handle(), 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00A99760 (:1590)
// Footsteps from the foot triggers: an action that has foot triggers plays its steps directly; otherwise the
// state animations' foot plant and release times (trigger times in frames of 30 per second) give the planted
// feet, and a foot that lands plays its step. A player's running steps are heard by npcs (twice per second).
static void human_do_footsteps(human* hp)
{
    bool running_step = false;
    bool no_foot_trigger = false;
    int plant_ms = 180; // Magic number: 180 ms between steps, 130 ms when running.
    float frame_time = -1.0f;
    float right_release = -1.0f;
    float left_plant = -1.0f;
    float right_plant = -1.0f;
    float left_release = -1.0f;
    if (hp == nullptr || hp->is_killed() || hp->movement_mode == HMM_JUMP || hp->movement_mode == HMM_FALL)
    {
        return;
    }
    v_animate* vp = hp->char_inst->vp;
    if (vp->desired_state == nullptr)
    {
        return;
    }
    v_anim* states[2];
    states[1] = vp->secondary_state;
    if (states[1] == nullptr || states[1]->blend < vp->desired_state->blend)
    {
        states[0] = vp->desired_state;
    }
    else
    {
        states[0] = states[1];
        states[1] = vp->desired_state;
    }
    const bool is_running = hp->cur_movement_state == HMS_RUN || hp->cur_movement_state == HMS_SPRINT;
    const footstep_variation variation = is_running ? FOOTSTEP_RUNNING : FOOTSTEP_WALKING;
    for (int i = 0; i < vp->max_simultaneous_anims; i++)
    {
        v_anim& a = vp->anims[i];
        if (!a.used)
        {
            break;
        }
        if (!a.is_state && a.alias == ANIM_ALIAS_VEHICLE_TRANSITION &&
            (animlib_anim_has_trigger(vp->aim_handle, Trigger_left_foot, a.instance) || animlib_anim_has_trigger(vp->aim_handle, Trigger_right_foot, a.instance)))
        {
            if (vanim_has_trigger_just_elapsed(vp, Trigger_right_foot, a.instance, -1))
            {
                human_play_footstep_sound(hp, FOOTSTEP_RIGHT, variation);
                running_step = running_step || is_running;
                hp->foot_planted = FP_RIGHT_FOOT;
            }
            if (vanim_has_trigger_just_elapsed(vp, Trigger_left_foot, a.instance, -1))
            {
                human_play_footstep_sound(hp, FOOTSTEP_LEFT, variation);
                running_step = running_step || is_running;
                hp->foot_planted = FP_LEFT_FOOT;
            }
            if (!running_step)
            {
                return;
            }
            goto heard_by_npcs;
        }
    }
    for (int i = 0; i < 2; i++)
    {
        v_anim* state = states[i];
        if (state == nullptr)
        {
            continue;
        }
        const int frame = vanim_get_anim_frame(vp, state->instance);
        if (left_plant == -1.0f && frame >= 0)
        {
            frame_time = (float)frame;
            left_plant = vanim_get_trigger_time(vp, Trigger_left_foot, state->instance, 0);
            right_plant = vanim_get_trigger_time(vp, Trigger_right_foot, state->instance, 0);
            left_release = vanim_get_trigger_time(vp, Trigger_left_foot_release, state->instance, 0);
            right_release = vanim_get_trigger_time(vp, Trigger_right_foot_release, state->instance, 0);
            // the next plant and release after the current frame (up to the third pair)
            for (int t = 1; t < 3; t++)
            {
                if (left_release < frame_time)
                {
                    const float plant = vanim_get_trigger_time(vp, Trigger_left_foot, state->instance, t);
                    if (plant <= 0.0f)
                    {
                        break;
                    }
                    left_release = vanim_get_trigger_time(vp, Trigger_left_foot_release, state->instance, t);
                    left_plant = plant;
                }
                if (right_release < frame_time)
                {
                    const float plant = vanim_get_trigger_time(vp, Trigger_right_foot, state->instance, t);
                    if (plant <= 0.0f)
                    {
                        break;
                    }
                    right_release = vanim_get_trigger_time(vp, Trigger_right_foot_release, state->instance, t);
                    right_plant = plant;
                }
            }
            // Magic number: 30 animation frames per second (the binary divides by the double 30.0).
            frame_time = (float)((double)frame_time / 30.0);
            if (left_plant == -1.0f && state->blend > 0.5f)
            {
                no_foot_trigger = true;
            }
        }
        else if (vanim_get_trigger_time(vp, Trigger_left_foot, state->instance, 0) < 0.0f && state->blend > 0.5f)
        {
            no_foot_trigger = true;
        }
    }
    if (left_plant == -1.0f)
    {
        hp->foot_planted = FP_BOTH_FEET;
        return;
    }
    if (hp->cur_movement_state == HMS_RUN)
    {
        plant_ms = 130;
    }
    if (hp->cur_movement_state == HMS_IDLE)
    {
        hp->foot_planted = FP_BOTH_FEET;
        return;
    }
    {
        const foot_plant was = hp->foot_planted;
        const bool left_was_planted = was == FP_LEFT_FOOT || was == FP_BOTH_FEET;
        const bool right_was_planted = was == FP_RIGHT_FOOT || was == FP_BOTH_FEET;
        const float t = frame_time;
        // A foot is down from its plant to its release time, the window wrapping around the loop's end. When a
        // blended state has no foot triggers, the two release times stand in for the plant times.
        const float left_start = no_foot_trigger ? right_release : left_plant;
        const float right_start = no_foot_trigger ? left_release : right_plant;
        const bool left_down = (left_start <= t && t < left_release) || ((left_start <= t || t < left_release) && left_release < left_start);
        const bool right_down = (right_start <= t && t < right_release) || ((right_start <= t || t < right_release) && right_release < right_start);
        hp->foot_planted = FP_NEITHER_FOOT;
        if (right_down)
        {
            if (!right_was_planted)
            {
                hp->foot_plant_time.set(plant_ms);
                human_play_footstep_sound(hp, FOOTSTEP_RIGHT, variation);
                running_step = running_step || is_running;
            }
            hp->foot_planted = FP_RIGHT_FOOT;
        }
        if (left_down)
        {
            if (!left_was_planted)
            {
                hp->foot_plant_time.set(plant_ms);
                human_play_footstep_sound(hp, FOOTSTEP_LEFT, variation);
                running_step = running_step || is_running;
            }
            hp->foot_planted = hp->foot_planted == FP_RIGHT_FOOT ? FP_BOTH_FEET : FP_LEFT_FOOT;
        }
    }
    if (!running_step)
    {
        return;
    }
heard_by_npcs:
    if (hp->cur_state != HS_COVER && hp->cur_stance == HST_STAND && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        player* pp = (player*)hp;
        if (pp->footstep_propagate_time.elapsed())
        {
            pp->footstep_propagate_time.set(500);
            npc_sound_info_footsteps info(pp);
            npc_sound_info::queue<npc_sound_info_footsteps>(info);
        }
    }
}

// 0x00A99E90 (:1783). LTCG: hp in ESI.
// Hand and foot IK at full bone LOD: each joint that has a target this frame bends its chain; the bone matrices are
// taken back from the instanced ones once and instanced again after the last joint.
static void human_ik_process(human* hp)
{
    if (!hp->char_inst->rdd.is_loaded() || hp->bone_lod_level >= 1)
    {
        return;
    }
    matrix43* instanced;
    if (!animlib_get_instanced_model_matrices(hp->char_inst->aim_handle, &instanced))
    {
        return;
    }
    bool have_bone_matrices = false;
    vector target_pos(0.0f, 0.0f, 0.0f);
    matrix43 bone_matrices[64];
    for (int i = 0; i < NUM_ANIMATION_IK_TYPES; i++)
    {
        ik_joint* joint = &hp->ik_joints[i];
        if (joint->process(hp, target_pos))
        {
            if (!have_bone_matrices)
            {
                animlib_get_bone_matrices_from_instanced(hp->char_inst->aim_handle, bone_matrices);
                have_bone_matrices = true;
            }
            joint->do_ik(hp, target_pos);
        }
    }
    if (have_bone_matrices)
    {
        animlib_instance_bone_matrices(hp->char_inst->aim_handle, bone_matrices, nullptr, 0);
        animlib_update_vertex_to_model_matrices(hp->char_inst->aim_handle);
    }
}

// 0x00ACE300 (:1818)
// The bone transform request of a human for this frame. The bone LOD follows the apparent distance from the camera
// (20/45/70 m); invisible npcs are not transformed, and distant ones skip frames depending on their speed. A
// ragdolled human transforms only what its ragdoll state needs. The lean of a moving human (or the root offset of
// some animation sets) is a bone modifier on the root.
static void human_animation_pre_process(human* hp)
{
    // Magic numbers: function statics of the original (.data 0x016AFE50-0x016AFE58).
    static float acceleration_rate = 1.4f;
    static float s_normalizeMinVectorLengthSquared = 1e-05f;
    static float scaler = 0.4f;

    if (!hp->rdd.is_loaded())
    {
        return;
    }
    hp->hflags.bones_transformed_this_frame = 0;
    if (hp->hflags.dont_deform_bones)
    {
        return;
    }
    bool dont_transform = false;
    const float distance = hp->render_distance.get_apparent_distance(hp->pos); // inlined
    bool force_lod_1 = false;
    bool on_screen = false;
    if ((!hp->get_is_visible() || hp->hflags.hidden) && hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        dont_transform = true;
    }
    else
    {
        force_lod_1 = hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE;
        bool invisible_npc = false;
        if (!hp->is_offscreen())
        {
            force_lod_1 = true;
            on_screen = true;
        }
        else if (hp->cur_state == HS_COVER)
        {
            force_lod_1 = true;
        }
        else if (hp->combat.thrown_weapon == 0xFFFFFFFF || hp->combat.thrown_weapon_released)
        {
            if (!Transform_combat_ready_humans || !human_is_combat_ready(hp))
            {
                if (cutscene_is_playing() || cutscene_rendered_this_frame())
                {
                    force_lod_1 = true;
                }
                else if (!force_lod_1)
                {
                    invisible_npc = true;
                }
            }
            else
            {
                force_lod_1 = true;
            }
        }
        else
        {
            force_lod_1 = true;
        }
        if (invisible_npc)
        {
            dont_transform = true;
        }
        // Magic number: past 90 m, a human in a vehicle (not on a turret) is not transformed, except on a server.
        else if (distance > 90.0f && hp->vehicle_handle != 0xFFFFFFFF && hp->combat.reserved_turret == 0xFFFFFFFF && (!Multiplayer || !multi_is_server()))
        {
            dont_transform = true;
        }
    }
    // Magic numbers: the bone LOD distances (20, 45 and 70 m).
    hp->bone_lod_level = 0;
    if (distance > 70.0f)
    {
        hp->bone_lod_level = 3;
    }
    else if (distance > 45.0f)
    {
        hp->bone_lod_level = 2;
    }
    else if (distance > 20.0f)
    {
        hp->bone_lod_level = 1;
    }
    if (!Human_use_bone_lods)
    {
        hp->bone_lod_level = 0;
    }
    if (hp->hflags.dont_lod_bones)
    {
        hp->bone_lod_level = 0;
        dont_transform = false;
    }
    if (force_lod_1 && !on_screen)
    {
        hp->bone_lod_level = 1;
    }
    if (hp->bone_lod_level > 1 && hp->hflags.aim_at_pos)
    {
        hp->bone_lod_level = 1;
    }
    // Distant coarse humans transform only every few frames: the number of frames skipped in a row grows with the
    // distance (90/120/180 m) and when they stand still (under 1 m/s).
    bool skip_this_frame = false;
    if (hp->bone_lod_level > 1 && !dont_transform)
    {
        const bool slow = hp->actual_velocity_mag < 1.0f;
        int frames = 0;
        if (distance < 90.0f)
        {
            frames = slow ? 1 : 0;
        }
        else if (distance < 120.0f)
        {
            frames = slow ? 2 : 1;
        }
        else if (distance < 180.0f)
        {
            frames = slow ? 3 : 2;
        }
        else
        {
            frames = slow ? 4 : 3;
        }
        skip_this_frame = hp->transform_frames_skipped < frames;
    }
    if (hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        animlib_clear_bone_info(hp->char_inst->aim_handle);
    }
    vector scale(hp->char_inst->character_scale, hp->char_inst->character_scale, hp->char_inst->character_scale);
    animlib_enable_scaling(hp->char_inst->aim_handle, scale);

    if (hp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE)
    {
        int aim_handle;
        switch (hp->ragdoll_state)
        {
        case HUMAN_RAGDOLL_STATE_ACTIVE:
            if (havok_ragdoll_is_added_to_world(hp->ragdoll_idx))
            {
                havok_ragdoll_add_constraints_to_world(hp->ragdoll_idx);
                havok_ragdoll_cast_saved_ray(hp->ragdoll_idx);
            }
            if (!havok_ragdoll_is_synced(hp->ragdoll_idx))
            {
                aim_handle = hp->char_inst->aim_handle;
                break;
            }
            dont_transform = false;
            // fall through: a synced ragdoll is transformed like one on impact
        case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
            if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_ragdoll, -1, -1))
            {
                havok_ragdoll_add_constraints_to_world(hp->ragdoll_idx);
                human_ragdoll_start(hp, false, 0xFFFFFFFF);
            }
            // fall through
        case HUMAN_RAGDOLL_STATE_BLEND_OUT:
        case HUMAN_RAGDOLL_STATE_FLINCH:
        {
            animlib_transform_command* cmd = animlib_request_batch_start();
            cmd->process_instance = 1;
            cmd->human_ptr = hp;
            cmd->aim_handle = hp->char_inst->aim_handle;
            cmd->frametime = hp->frametime_tally;
            if (!dont_transform && animlib_allocate_bones(hp->char_inst->aim_handle))
            {
                hp->hflags.bones_transformed_this_frame = 1;
                cmd->transform_all_bones_pre = 1;
                cmd->bone_lod_level = hp->bone_lod_level;
                cmd->bone_lods = Bone_lods;
                cmd->num_bone_lods = 100;
                if (!cutscene_is_playing())
                {
                    cmd->process_spinebending = 1;
                    cmd->spinebend_data = &hp->spinebend_data;
                }
                cmd->transform_all_bones_post = 1;
            }
            animlib_request_batch_end();
            return;
        }
        case HUMAN_RAGDOLL_STATE_DEACTIVATED:
            if (havok_ragdoll_is_active(hp->ragdoll_idx))
            {
                human_ragdoll_start(hp, false, 0xFFFFFFFF);
            }
            if (!animlib_allocate_bones(hp->char_inst->aim_handle))
            {
                return;
            }
            havok_ragdoll_recall_matrices(hp->ragdoll_idx, hp->char_inst);
            hp->hflags.bones_transformed_this_frame = 1;
            return;
        case HUMAN_RAGDOLL_STATE_POSE_SAVED:
            aim_handle = hp->char_inst->aim_handle;
            break;
        default:
            // Original: an Int3 debug message.
            return;
        }
        if (animlib_allocate_bones(aim_handle))
        {
            hp->hflags.bones_transformed_this_frame = 1;
        }
        return;
    }

    animlib_transform_command* cmd = animlib_request_batch_start();
    cmd->process_instance = 1;
    cmd->aim_handle = hp->char_inst->aim_handle;
    cmd->human_ptr = hp;
    cmd->frametime = hp->frametime_tally;
    if (dont_transform)
    {
        // Magic number: a hidden human counts as having skipped 50 frames, so it transforms as soon as it shows.
        hp->transform_frames_skipped = 50;
        animlib_request_batch_end();
        return;
    }
    if (!animlib_allocate_bones(hp->char_inst->aim_handle))
    {
        animlib_request_batch_end();
        return;
    }
    if (hp->root_bone_offset == 0.0f)
    {
        if (hp->run_stand_blend_pose_weight > 0.0f || hp->lean_weight > 0.0f)
        {
            vector lean = hp->steering_vector;
            lean.rotate_around_y_axis(hp->steering_heading_offset);
            lean.rotate(hp->orient);
            const float s = hp->lean_weight * scaler;
            lean.x = s * lean.x;
            lean.z = s * lean.z;
            lean.y = 1.0f;
            if (s_normalizeMinVectorLengthSquared < lean.mag_squared())
            {
                lean.normalize();
            }
            const float step = Frametime * acceleration_rate;
            // Original: lean_dir is reset to up before the ramp, so it ramps from zero every frame (at most one step
            // of acceleration_rate from upright).
            hp->lean_dir.y = 1.0f;
            hp->lean_dir.x = 0.0f;
            hp->lean_dir.z = 0.0f;
            float x;
            if (lean.x <= 0.0f)
            {
                x = 0.0f - step;
                if (lean.x - x >= 0.0f)
                {
                    x = lean.x;
                }
            }
            else
            {
                x = step + 0.0f;
                if (lean.x - (step + 0.0f) < 0.0f)
                {
                    x = lean.x;
                }
            }
            hp->lean_dir.x = x;
            const float cur_z = hp->lean_dir.z;
            float z;
            if (lean.z <= cur_z)
            {
                z = cur_z - step;
                if (lean.z - z >= 0.0f)
                {
                    z = lean.z;
                }
            }
            else
            {
                z = cur_z + step;
                if (lean.z - (cur_z + step) < 0.0f)
                {
                    z = lean.z;
                }
            }
            hp->lean_dir.z = z;
            if (s_normalizeMinVectorLengthSquared < hp->lean_dir.mag_squared())
            {
                hp->lean_dir.normalize();
            }
            // the root's rotation: up is the lean, forward stays forward as far as it can
            matrix m;
            m.uvec = hp->lean_dir;
            m.fvec.x = 0.0f;
            m.fvec.y = 0.0f;
            m.fvec.z = 1.0f;
            m.rvec.x = m.uvec.y - m.uvec.z * 0.0f;
            m.rvec.y = m.uvec.z * 0.0f - m.uvec.x;
            m.rvec.z = m.uvec.x * 0.0f - m.uvec.y * 0.0f;
            if (s_normalizeMinVectorLengthSquared < m.rvec.mag_squared())
            {
                m.rvec.normalize();
            }
            const float fx = m.rvec.y * m.uvec.z - m.rvec.z * m.uvec.y;
            const float fy = m.rvec.z * m.uvec.x - m.uvec.z * m.rvec.x;
            const float fz = m.uvec.y * m.rvec.x - m.rvec.y * m.uvec.x;
            m.fvec.x = fx;
            m.fvec.y = fy;
            m.fvec.z = fz;
            // Magic number: storage slot 14 of the instance's bone modifiers.
            animlib_register_bone_modifier(hp->char_inst->aim_handle, BONE_MODIFIER_ROTATE_RC, 0, 0xE, m, nullptr, 0.0f);
        }
    }
    else
    {
        matrix m;
        m.make_identity();
        // the offset rides in the matrix's fvec
        m.fvec.x = 0.0f;
        m.fvec.y = hp->root_bone_offset;
        m.fvec.z = 0.0f;
        animlib_register_bone_modifier(hp->char_inst->aim_handle, BONE_MODIFIER_POSITION_OFFSET, 0, 0xE, m, nullptr, 0.0f);
    }
    hp->hflags.bones_transformed_this_frame = 1;
    if (skip_this_frame)
    {
        hp->transform_frames_skipped++;
        animlib_request_batch_end();
        return;
    }
    hp->transform_frames_skipped = 0;
    cmd->transform_all_bones_pre = 1;
    cmd->bone_lod_level = hp->bone_lod_level;
    cmd->bone_lods = Bone_lods;
    cmd->num_bone_lods = 100;
    if (!cutscene_is_playing())
    {
        cmd->process_spinebending = 1;
        cmd->spinebend_data = &hp->spinebend_data;
    }
    cmd->transform_all_bones_post = 1;
    animlib_request_batch_end();
}

// 0x00ABAB70 (:2104)
// The firearm class the human holds, or is equipping (the weapon object of the desired item while the equip or
// unequip animation plays).
static weapon_class_type get_firearm_weapon_class(human* hp)
{
    human_validate_inventory(hp);
    if ((!hp->hflags.playing_equip_anim && !hp->hflags.playing_unequip_anim) || hp->desired_equipped_inv_item == nullptr ||
        hp->desired_equipped_inv_item->weapon_handle == 0xFFFFFFFF)
    {
        const weapon_info* wi = human_weapons_get_weapon_info(hp);
        if (wi != nullptr)
        {
            const weapon_ammo_type ammo = wi->ammo_type;
            if (ammo == WEAPON_AMMO_TYPE_BULLET || ammo == WEAPON_AMMO_TYPE_PROJECTILE || ammo == WEAPON_AMMO_TYPE_ELECTRICITY || ammo == WEAPON_AMMO_TYPE_REPAIR ||
                ammo == WEAPON_AMMO_TYPE_BLANKS || ammo == WEAPON_AMMO_TYPE_FLAME)
            {
                return human_weapons_get_weapon_info(hp)->weapon_class;
            }
        }
    }
    else
    {
        object* objp = World.object_from_handle(hp->desired_equipped_inv_item->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            const weapon_info* wi = ((weapon*)objp)->w_info;
            if (wi->is_firearm())
            {
                return wi->weapon_class;
            }
        }
    }
    return WEAPON_CLASS_NONE;
}

// 0x00A7B020 (:2157). LTCG: hp in ESI.
static weapon_class_type get_melee_weapon_class_type(human* hp)
{
    if ((!hp->hflags.playing_equip_anim && !hp->hflags.playing_unequip_anim) || hp->desired_equipped_inv_item == nullptr ||
        hp->desired_equipped_inv_item->weapon_handle == 0xFFFFFFFF)
    {
        const weapon_info* wi = human_weapons_get_weapon_info(hp);
        if (wi != nullptr && wi->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            return human_weapons_get_weapon_info(hp)->weapon_class;
        }
    }
    else
    {
        object* objp = World.object_from_handle(hp->desired_equipped_inv_item->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && ((weapon*)objp)->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            return ((weapon*)objp)->w_info->weapon_class;
        }
    }
    return WEAPON_CLASS_NONE;
}

// 0x00A7B0A0 (:2190). LTCG: hp in ESI.
static weapon_class_type get_thrown_weapon_class_type(human* hp)
{
    if ((!hp->hflags.playing_equip_anim && !hp->hflags.playing_unequip_anim) || hp->desired_equipped_inv_item == nullptr ||
        hp->desired_equipped_inv_item->weapon_handle == 0xFFFFFFFF)
    {
        const weapon_info* wi = human_weapons_get_weapon_info(hp);
        if (wi != nullptr && wi->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            return human_weapons_get_weapon_info(hp)->weapon_class;
        }
    }
    else
    {
        object* objp = World.object_from_handle(hp->desired_equipped_inv_item->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && ((weapon*)objp)->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            return ((weapon*)objp)->w_info->weapon_class;
        }
    }
    return WEAPON_CLASS_NONE;
}

// 0x00A52CD0 (:2223). LTCG: hp in ESI.
// Visible breath at night (single player), for living humans that are not EDF allies, in view of the camera within
// 40 m (the player always) and not getting in or out of a vehicle.
static bool human_should_breath_effect_play(const human* hp)
{
    if (!Breath_effect_enable || Multiplayer || game_clock_get_time_of_day() != GC_NIGHT || hp == nullptr || hp->is_killed() || hp->render_alpha <= 0.0f ||
        !hp->breath_timer.elapsed() || human_teams_are_allies(hp, HUMAN_TEAM_EDF))
    {
        return false;
    }
    // Magic number: 1600 = 40 m squared.
    if (!object_is_player(hp) && vec_dist_squared(Rfg_camera.real_pos, hp->pos) > 1600.0f)
    {
        return false;
    }
    return !human_in_vehicle_interaction(hp, nullptr);
}

// 0x00ABAC10 (:2273)
// The weapon animation flags of a human for a state: stance, ready/down-ready/aim/fine-aim, the remote charge's
// "no ammo" pose, walk/run/drive/ride, and for a ready human moving (or sitting in a vehicle) the side it moves
// towards relative to where it looks (left/right/back/backpedal). The look direction is the camera's for the local
// player, the replicated camera's for a remote one and the eyes' for an npc. The previous frame's flags give the
// npc thresholds some hysteresis.
void human_get_animation_flags(const human* hp, human_states state, human_movement_states m_state, weapon_anim_flags& animation_flags)
{
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    const human* npc_hp = nullptr;
    vector look;
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        if (pp == nullptr)
        {
            // Original: an Int3 debug message.
            return;
        }
        if (pp == Player)
        {
            look = Rfg_camera.free_params.flags.debug_mode ? pp->orient.fvec : Rfg_camera.ideal_orient.fvec;
        }
        else
        {
            look = pp->mp_camera_orient.fvec;
        }
    }
    else
    {
        npc_hp = hp;
        vector eye_pos;
        human_get_eye_pos_orient(hp, eye_pos, nullptr);
        human_get_look_at_vector_from_eye_pos(hp, eye_pos, look);
    }
    look.y = 0.0f;
    look.normalize_safe();
    const weapon_anim_flags old_flags = hp->animation_flags;
    if (hp->cur_stance == HST_CROUCH)
    {
        animation_flags.crouch = 1;
    }
    else if (hp->cur_stance == HST_PRONE)
    {
        animation_flags.prone = 1;
    }
    bool combat_ready = hp->combat.combat_ready_timestamp.valid() && !hp->combat.combat_ready_timestamp.elapsed();
    const animation_group group = hp->char_inst->vp->default_group_index;
    if (vanim_check_editor_flag_ramped(hp->char_inst->vp, 0x400) &&
        (group == ANIMATION_GROUP_RPG || group == ANIMATION_GROUP_GRINDER || group == ANIMATION_GROUP_ARC || group == ANIMATION_GROUP_THERMOBARIC_ROCKET_LAUNCHER))
    {
        combat_ready = false;
    }
    else if (combat_ready)
    {
        bool down_ready = false;
        if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            const ai_system* ai = ((const npc*)hp)->get_ai();
            bool knocked_down = false;
            if (hp->char_inst != nullptr && hp->char_inst->vp != nullptr && hp->char_inst->vp->anims != nullptr)
            {
                for (const v_anim* a = hp->char_inst->vp->anims; a->anim_handle != -1; a++)
                {
                    if (a->alias == ANIM_ALIAS_EXPLOSION_KNOCKDOWN || a->alias == ANIM_ALIAS_GET_UP)
                    {
                        knocked_down = true;
                        break;
                    }
                }
            }
            down_ready = human_weapons_is_firearm_equipped(hp) && !hp->hflags.aim_at_pos && ai->perception.get_attention_mode() != ATTENTION_AIM_STRAIGHT_AHEAD && !knocked_down;
        }
        if (down_ready)
        {
            animation_flags.down_ready = 1;
        }
        else
        {
            animation_flags.ready = 1;
        }
    }
    if (hp->combat.combat_aim_timestamp.valid() && !hp->combat.combat_aim_timestamp.elapsed())
    {
        const animation_group g = hp->char_inst->vp->default_group_index;
        if (g == ANIMATION_GROUP_AK || g == ANIMATION_GROUP_REVOLVER || g == ANIMATION_GROUP_SHOTGUN || g == ANIMATION_GROUP_SNIPER || g == ANIMATION_GROUP_NANO || g == ANIMATION_GROUP_ENFORCER ||
            g == ANIMATION_GROUP_RAILGUN)
        {
            animation_flags.aim = 1;
        }
    }
    animation_flags.fine_aim = 0;
    if (pp != nullptr)
    {
        if (pp->zoom_state == PZS_FINE_AIM || pp->zoom_state == PZS_SCOPE)
        {
            animation_flags.fine_aim = 1;
        }
        if (pp->char_inst->vp->default_group_index == ANIMATION_GROUP_REMOTE_CHARGE)
        {
            object* charge = World.object_from_handle(pp->offhand_projectile_handle);
            weapon* wp = pp->get_equipped_weapon();
            if (wp != nullptr && wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && !Multiplayer &&
                (charge == nullptr || (!charge->get_is_visible() && !vanim_is_playing_action(pp->char_inst->vp, ANIM_ALIAS_THROW))))
            {
                animation_flags.no_ammo = 1;
                human_in_vehicle_interaction(pp, nullptr); // Original: the result is not used.
                pp->combat.combat_ready_timestamp.invalidate();
            }
        }
    }
    // Magic number: 3 m/s, the speed above which a human counts as running.
    const bool aiming_while_moving = (m_state == HMS_RUN || m_state == HMS_WALK || hp->move_speed > 3.0f || (m_state == HMS_IDLE && (state == HS_FALL || state == HS_JUMP))) &&
                                     combat_ready && hp->hflags.aim_at_pos;
    bool in_vehicle_seat = false;
    bool check_idle;
    if (vanim_check_editor_flag_ramped(hp->char_inst->vp, 0x10))
    {
        m_state = HMS_IDLE;
        check_idle = true;
    }
    else if (m_state == HMS_WALK)
    {
        animation_flags.walk = 1;
        check_idle = false;
    }
    else if (m_state == HMS_RUN || m_state == HMS_SPRINT)
    {
        animation_flags.run = 1;
        check_idle = false;
    }
    else
    {
        check_idle = true;
    }
    if (check_idle)
    {
        if (hp->move_speed > 3.0f)
        {
            animation_flags.run = 1;
        }
        else if (m_state == HMS_IDLE)
        {
            if (state == HS_VEHICLE_DRIVING)
            {
                animation_flags.drive = 1;
                in_vehicle_seat = true;
            }
            else if (state == HS_VEHICLE_SITTING)
            {
                if (hp->vehicle_seat_idx == VSI_PASSENGER_2)
                {
                    animation_flags.ride_left = 1;
                }
                else
                {
                    animation_flags.ride = 1;
                }
                in_vehicle_seat = true;
            }
        }
    }
    if ((!aiming_while_moving && !in_vehicle_seat) || look.is_null())
    {
        return;
    }
    vector move_dir;
    if (in_vehicle_seat)
    {
        object* vobj = hp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(hp->vehicle_handle) : nullptr;
        move_dir = vobj == nullptr || vobj->get_obj_type() != OT_VEHICLE ? FORWARD_VECTOR : vobj->orient.fvec;
    }
    else if (pp == nullptr)
    {
        if (npc_hp == nullptr)
        {
            // Original: an Int3 debug message.
            move_dir = FORWARD_VECTOR;
        }
        else
        {
            move_dir = npc_hp->steering_vector;
        }
    }
    else
    {
        const human::resource_dependent_data* rdd = hp->rdd.get_data();
        // Magic number: in the air for more than 200 ms.
        if ((!old_flags.aim && animation_flags.aim) || (rdd != nullptr && !rdd->controller->flags.big_step && hp->air_time.time_since() > 200))
        {
            move_dir = pp->orient.fvec;
        }
        else if (pp->block_movement_anim == -1 || (pp->block_movement_type != BLOCK_MOVEMENT_ORIENT && pp->block_movement_type != BLOCK_MOVEMENT_POS_AND_ORIENT))
        {
            move_dir = pp->direct_control_vel;
        }
        else
        {
            move_dir = pp->block_steering_vector;
            const float d = hp->block_steering_heading_offset - 0.0f;
            if (d >= 0.0001f || -d >= 0.0001f)
            {
                move_dir.rotate_around_y_axis(hp->block_steering_heading_offset);
            }
        }
        if (move_dir.is_null())
        {
            move_dir = pp->orient.fvec;
        }
    }
    move_dir.y = 0.0f;
    move_dir.normalize_safe();
    if (move_dir.is_null())
    {
        return;
    }
    const float angle = vec_delta_ang(look, move_dir, UP_VECTOR);
    // The angle sectors (radians): sideways past 47 degrees, backwards past 137 (110 crouched). Npcs keep a side
    // or back flag of the previous frame 4 degrees longer.
    float back_left = animation_flags.crouch ? FIREARM_CROUCH_MOVE_BACK_LEFT_ANGLE : -FIREARM_READY_MOVE_BACKWARDS_ANGLE;
    float left = -FIREARM_READY_MOVE_SIDEWAYS_ANGLE;
    if (pp == nullptr)
    {
        if (old_flags.left)
        {
            left = -0.750479f;
        }
        else if (old_flags.back || old_flags.backpedal)
        {
            back_left = (float)((double)back_left + (double)0.069812f);
        }
    }
    float right = FIREARM_READY_MOVE_SIDEWAYS_ANGLE;
    float back_right = FIREARM_READY_MOVE_BACKWARDS_ANGLE;
    if (pp == nullptr)
    {
        if (old_flags.right)
        {
            right = 0.750479f;
            back_right = FIREARM_READY_MOVE_BACKWARDS_ANGLE;
        }
        else if (old_flags.back || old_flags.backpedal)
        {
            back_right = 2.321249f;
        }
        if (angle < back_left || 3.1415927f < angle)
        {
            if (!in_vehicle_seat && !animation_flags.crouch && animation_flags.run && animation_flags.ready)
            {
                animation_flags.backpedal = 1;
            }
            else
            {
                animation_flags.back = 1;
            }
        }
        else if (angle < left)
        {
            animation_flags.left = 1;
        }
        else if (angle > back_right)
        {
            animation_flags.back = 1;
        }
        else if (angle > right)
        {
            animation_flags.right = 1;
        }
        return;
    }
    if (angle < back_left || 3.1415927f < angle)
    {
        animation_flags.backpedal = 1;
    }
    else if (angle < left)
    {
        animation_flags.left = 1;
    }
    else if (FIREARM_READY_MOVE_BACKWARDS_ANGLE < angle)
    {
        animation_flags.back = 1;
    }
    else if (FIREARM_READY_MOVE_SIDEWAYS_ANGLE < angle)
    {
        animation_flags.right = 1;
    }
}

// 0x00A2D430 (:2632). LTCG: weapon_class in EDX, anim_alias in EAX, blend_time in EDI, hp/anim_state/old_flags on
// the stack; state and m_state are not passed (unused).
// The weapon's state animation for the current flags. Starting to aim snaps the blend and teleports the spine bend;
// EDF npcs stand nervously where the EDF lost control of the district (under 40 % and 20 %).
static void get_weapon_animation_state(weapon_class_type weapon_class, human_states, human_movement_states, human* hp, animation_state& anim_state, vanim_aliases& anim_alias,
                                       float& blend_time, const weapon_anim_flags& old_flags)
{
    human_weapons_get_state_animation(weapon_class, hp->animation_flags, anim_state, anim_alias, blend_time);
    if (!old_flags.aim && hp->animation_flags.aim)
    {
        blend_time = 0.0f;
        hp->spinebend_data.flags.spinebend_teleport = 1;
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && hp->get_team() == HUMAN_TEAM_EDF && anim_state == ANIMATION_STATE_STAND)
    {
        const float control = gfm_get_edf_control_percent(district_get_from_pos(hp->pos));
        // Magic numbers: the binary compares with the doubles 0.2 and (double)0.4f.
        if ((double)control < 0.2)
        {
            anim_state = ANIMATION_STATE_STAND_LOW_CONTROL;
            return;
        }
        if ((double)control < (double)0.4f)
        {
            anim_state = ANIMATION_STATE_STAND_LOW_CONTROL2;
        }
    }
}

// 0x00A06D30 (:2654). LTCG: anim_state in EDI; anim_alias is not passed (unused).
// The unarmed state animation for the stance and movement (the ready variants while combat ready).
static void get_unarmed_animation_state(human_movement_states m_state, human* hp, animation_state& anim_state, vanim_aliases&)
{
    anim_state = ANIMATION_STATE_NONE;
    const bool ready = hp->combat.combat_ready_timestamp.valid() && !hp->combat.combat_ready_timestamp.elapsed();
    switch (hp->cur_stance)
    {
    case HST_STAND:
        switch (m_state)
        {
        case HMS_IDLE:
            anim_state = ready ? ANIMATION_STATE_STAND_READY : ANIMATION_STATE_STAND;
            return;
        case HMS_WALK:
            anim_state = ready ? ANIMATION_STATE_WALK_READY : ANIMATION_STATE_WALK;
            return;
        case HMS_RUN:
            anim_state = ANIMATION_STATE_RUN;
            return;
        case HMS_SPRINT:
            anim_state = ANIMATION_STATE_SPRINT;
            return;
        default:
            break;
        }
        break;
    case HST_CROUCH:
        switch (m_state)
        {
        case HMS_IDLE:
            anim_state = ANIMATION_STATE_CROUCH;
            return;
        case HMS_WALK:
            anim_state = ANIMATION_STATE_CROUCH_WALK;
            return;
        case HMS_RUN:
        case HMS_SPRINT:
            anim_state = ANIMATION_STATE_CROUCH_RUN;
            return;
        default:
            break;
        }
        break;
    case HST_PRONE:
        anim_state = m_state != HMS_IDLE ? ANIMATION_STATE_PRONE_CRAWL : ANIMATION_STATE_PRONE;
        return;
    default:
        break;
    }
    // Original: an Int3 debug message.
}

// One turret state's blend pose (the original writes this out per call; alias -1).
static void human_turret_blend(human* hp, int state, float blend, unsigned int flags)
{
    vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(state, (vanim_aliases)-1), blend, flags);
}

// The original's inlined clamp to 0..1.
static float human_turret_clamp01(float x)
{
    if (x - 0.0f < 0.0f)
    {
        x = 0.0f;
    }
    if (0.0f <= x - 1.0f)
    {
        x = 1.0f;
    }
    return x;
}

// 0x00AC6510 (:2734): the operator's blend of the aim states while manning a turret. Past 25 degrees (0.436325 rad) of
// off-center aim a standing operator walks sideways; vertical input comes from the weapon's pitch.
void human_get_turret_state_anim(human* hp)
{
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp == nullptr || tp->t_info->flags.no_operator_animation || tp->should_autoaim())
    {
        return;
    }
    if (vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_ENTER))
    {
        const unsigned int anim = vanim_get_anim_instance_handle(hp->char_inst->vp, ANIM_ALIAS_ENTER);
        if (animlib_get_current_time(hp->char_inst->aim_handle, anim) < 0.2f)
        {
            return;
        }
    }
    // Left the turret's level: abandons it.
    if (0.15f < fabsf(tp->pos.y - hp->pos.y) && !human_in_vehicle_interaction(hp, nullptr))
    {
        human_combat_abandon_turret(hp);
        return;
    }
    const float h_fov_pos = tp->get_h_fov_pos();
    const float h_fov_neg = tp->get_h_fov_neg();
    weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    const float aim_x = wp->orient.fvec.x;
    const float aim_z = wp->orient.fvec.z;
    const float aim_pitch = (float)asin((double)wp->orient.fvec.y);
    const float aim_heading = fl_atan2_safe(aim_x, aim_z);
    const float body_heading = tp->turret_flags.vehicle_turret ? hp->orient.extract_heading() : fl_atan2_safe(tp->pos.x - hp->pos.x, tp->pos.z - hp->pos.z);
    float pitch_input = -aim_pitch - hp->orient.extract_pitch();
    const float off_body = -fl_angle_cap_radians(aim_heading - body_heading, -3.1415927f);
    const float off_turret = fl_angle_cap_radians(aim_heading - tp->orient.extract_heading(), -3.1415927f);
    if (pitch_input <= 0.0f)
    {
        pitch_input = pitch_input / 0.767932f; // 44 degrees down
    }
    else
    {
        pitch_input = pitch_input / 0.87265f; // 50 degrees up
    }
    if (pitch_input - -1.0f < 0.0f)
    {
        pitch_input = -1.0f;
    }
    if (0.0f <= pitch_input - 1.0f)
    {
        pitch_input = 1.0f;
    }
    const float walk_heading = off_body + off_turret;
    float walk_limit_left = h_fov_pos - 0.87265f;
    float walk_limit_right = h_fov_neg + 0.87265f;
    // Original: the second test is against 3.1425927 where -3.1405928 was surely meant, so it is nearly always true.
    if (3.1405928f < h_fov_pos && h_fov_neg < 3.1425927f)
    {
        walk_limit_left = 40.0f; // no limit
        walk_limit_right = -40.0f;
    }
    hp->char_inst->set_animation_group(ANIMATION_GROUP_TURRET_MACHINEGUN, index_alias(-1, (vanim_aliases)-1), 0.3f, false);
    v_anim* desired = hp->char_inst->vp->desired_state;
    if (desired != nullptr)
    {
        desired->blend = 0.0f;
    }

    const turret_info* info = tp->t_info;
    if (tp->turret_flags.walking_left)
    {
        bool keep_walking = false;
        if (off_body < 0.0f)
        {
            const float wanted = human_turret_clamp01(off_body / -0.436325f);
            float blend = Frametime * 3.0f + tp->walk_blend_left;
            if (wanted - blend < 0.0f)
            {
                blend = wanted;
            }
            tp->walk_blend_left = blend;
            hp->steering_vector.x = -hp->orient.rvec.x;
            hp->steering_vector.y = -hp->orient.rvec.y;
            hp->steering_vector.z = -hp->orient.rvec.z;
            hp->move_speed = info->h_rot_max_mul * info->h_rot_speed * 0.75f;
            float offset = tp->vehicle_heading_offset - (Frametime * 1.3f) / 0.75f;
            if (info->movement_limit_left - offset >= 0.0f)
            {
                offset = info->movement_limit_left;
            }
            tp->vehicle_heading_offset = offset;
            keep_walking = walk_heading < walk_limit_left && (!info->flags.manned_vehicle_turret || info->movement_limit_left < offset);
        }
        if (!keep_walking)
        {
            tp->turret_flags.walking_left = false;
            hp->steering_vector = ZERO_VECTOR;
            tp->walk_blend_left = 0.0f;
            human_turret_blend(hp, ANIMATION_STATE_WALK_LEFT, 0.0f, 0x10);
        }
    }
    if (0.0f < tp->walk_blend_left)
    {
        if (!tp->turret_flags.walking_left)
        {
            const float blend = tp->walk_blend_left - Frametime * 5.0f;
            tp->walk_blend_left = 0.0f - blend < 0.0f ? blend : 0.0f;
        }
        human_turret_blend(hp, ANIMATION_STATE_WALK_LEFT, tp->walk_blend_left, 0x10);
        vanim_set_blend_tree_init_control_input(hp->char_inst->vp, ANIMATION_STATE_WALK_LEFT, pitch_input);
    }
    if (tp->turret_flags.walking_right)
    {
        bool keep_walking = false;
        if (0.0f < off_body)
        {
            const float wanted = human_turret_clamp01(off_body / 0.436325f);
            float blend = Frametime * 3.0f + tp->walk_blend_right;
            if (wanted - blend < 0.0f)
            {
                blend = wanted;
            }
            tp->walk_blend_right = blend;
            hp->steering_vector = hp->orient.rvec;
            hp->move_speed = info->h_rot_max_mul * info->h_rot_speed * 0.75f;
            float offset = (Frametime * 1.3f) / 0.75f + tp->vehicle_heading_offset;
            if (info->movement_limit_right - offset < 0.0f)
            {
                offset = info->movement_limit_right;
            }
            tp->vehicle_heading_offset = offset;
            keep_walking = walk_limit_right < walk_heading && (!info->flags.manned_vehicle_turret || offset < info->movement_limit_right);
        }
        if (!keep_walking)
        {
            tp->turret_flags.walking_right = false;
            hp->steering_vector = ZERO_VECTOR;
            tp->walk_blend_right = 0.0f;
            // Original: flags 0 here, 0x10 on the left side.
            human_turret_blend(hp, ANIMATION_STATE_WALK_RIGHT, 0.0f, 0);
        }
    }
    if (0.0f < tp->walk_blend_right)
    {
        if (!tp->turret_flags.walking_right)
        {
            const float blend = tp->walk_blend_right - Frametime * 5.0f;
            tp->walk_blend_right = 0.0f - blend < 0.0f ? blend : 0.0f;
        }
        human_turret_blend(hp, ANIMATION_STATE_WALK_RIGHT, tp->walk_blend_right, 0x10);
        vanim_set_blend_tree_init_control_input(hp->char_inst->vp, ANIMATION_STATE_WALK_RIGHT, pitch_input);
    }

    v_animate* vp = hp->char_inst->vp;
    if (0.0f <= off_body)
    {
        tp->turret_flags.walking_left = false;
        if (0.436325f < off_body && !tp->turret_flags.walking_right)
        {
            if (!info->flags.stationary_operator && walk_limit_right < walk_heading && (!info->flags.manned_vehicle_turret || tp->vehicle_heading_offset < info->movement_limit_right))
            {
                tp->turret_flags.walking_right = true;
            }
            const float t = (off_body - 0.436325f) / 0.436325f;
            human_turret_blend(hp, ANIMATION_STATE_TURRET_LEFT, 0.0f, 0);
            human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, 0.0f, 0);
            human_turret_blend(hp, ANIMATION_STATE_TURRET_MIDDLE, 0.0f, 0);
            human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, human_turret_clamp01(1.0f - t), 0x10);
            human_turret_blend(hp, ANIMATION_STATE_TURRET_RIGHT, human_turret_clamp01(t), 0x10);
            vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, pitch_input);
            vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_RIGHT, pitch_input);
            return;
        }
        human_turret_blend(hp, ANIMATION_STATE_TURRET_LEFT, 0.0f, 0);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, 0.0f, 0);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_MIDDLE, human_turret_clamp01(1.0f - off_body / 0.436325f), 0x10);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, human_turret_clamp01(off_body / 0.436325f - tp->walk_blend_right), 0x10);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_RIGHT, 0.0f, 0);
        vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_MIDDLE, pitch_input);
        vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, pitch_input);
        return;
    }
    tp->turret_flags.walking_right = false;
    if (-0.436325f <= off_body || tp->turret_flags.walking_left)
    {
        human_turret_blend(hp, ANIMATION_STATE_TURRET_LEFT, 0.0f, 0);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, human_turret_clamp01(off_body / -0.436325f - tp->walk_blend_left), 0x10);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_MIDDLE, human_turret_clamp01(1.0f - off_body / -0.436325f), 0x10);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, 0.0f, 0);
        human_turret_blend(hp, ANIMATION_STATE_TURRET_RIGHT, 0.0f, 0);
        vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, pitch_input);
        vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_MIDDLE, pitch_input);
        return;
    }
    if (!info->flags.stationary_operator && walk_heading < walk_limit_left && (!info->flags.manned_vehicle_turret || info->movement_limit_left < tp->vehicle_heading_offset))
    {
        tp->turret_flags.walking_left = true;
    }
    const float t = (off_body - -0.436325f) / -0.436325f;
    human_turret_blend(hp, ANIMATION_STATE_TURRET_LEFT, human_turret_clamp01(t), 0x10);
    human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, human_turret_clamp01(1.0f - t), 0x10);
    human_turret_blend(hp, ANIMATION_STATE_TURRET_MIDDLE, 0.0f, 0);
    human_turret_blend(hp, ANIMATION_STATE_TURRET_SLIGHT_RIGHT, 0.0f, 0);
    human_turret_blend(hp, ANIMATION_STATE_TURRET_RIGHT, 0.0f, 0);
    vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_LEFT, pitch_input);
    vanim_set_blend_tree_init_control_input(vp, ANIMATION_STATE_TURRET_SLIGHT_LEFT, pitch_input);
}

// 0x00ABB3E0 (:3070). LTCG: hp in ECX, blend_time in EAX, new_state and anim_alias on the stack.
// The state animation a human should be in: turret operators and jumps/falls first, then the default (weapon or
// unarmed) states of standing, walking and running humans, the dead pose and the cover states.
static void human_lookup_animation_state(human* hp, animation_state& new_state, vanim_aliases& anim_alias, float& blend_time)
{
    const human_movement_states m_state = hp->cur_movement_state;
    const human_states state = hp->cur_state;
    animation_state state_index = ANIMATION_STATE_NONE;
    // Magic numbers: the default blend times (0.4 s, 0.2 s between standing and crouching).
    blend_time = 0.4f;
    animation_state weapon_state = ANIMATION_STATE_NONE;
    anim_alias = ANIM_ALIAS_NONE;
    if (human_combat_get_manned_turret(hp) != nullptr)
    {
        if (hp->vehicle_handle == 0xFFFFFFFF)
        {
            new_state = ANIMATION_STATE_TURRET_MIDDLE;
        }
        return;
    }
    weapon_class_type weapon_class = get_firearm_weapon_class(hp);
    if (weapon_class == WEAPON_CLASS_NONE)
    {
        weapon_class = get_melee_weapon_class_type(hp);
        if (weapon_class == WEAPON_CLASS_NONE)
        {
            weapon_class = get_thrown_weapon_class_type(hp);
        }
    }
    if ((hp->last_stance == HST_STAND && hp->cur_stance == HST_CROUCH) || (hp->last_stance == HST_CROUCH && hp->cur_stance == HST_STAND))
    {
        blend_time = 0.2f;
    }
    const weapon_anim_flags old_flags = hp->animation_flags;
    weapon_anim_flags flags;
    *(unsigned int*)&flags = 0;
    switch (state)
    {
    case HS_JUMP:
        if (hp->hflags.jumping_from_building)
        {
            anim_alias = ANIM_ALIAS_STATE_FALL;
            new_state = ANIMATION_STATE_FALL_FROM_BUILDING;
            return;
        }
        break; // the jump start below
    case HS_FALL:
        human_get_animation_flags(hp, hp->cur_state, hp->cur_movement_state, flags);
        hp->animation_flags = flags;
        switch (hp->movement_submode)
        {
        case HMSM_NONE:
        case HMSM_KNOCKDOWN_RAGDOLL:
            state_index = ANIMATION_STATE_CALM_FALL;
            break;
        case HMSM_FALL_START:
        case HMSM_FALL_NORMAL:
        {
            if (hp->hflags.jumping_from_building)
            {
                state_index = ANIMATION_STATE_FALL_FROM_BUILDING;
            }
            else
            {
                const animation_group group = hp->char_inst->vp->default_group_index;
                if (hp->hflags.in_air_melee && (group == ANIMATION_GROUP_HAMMER || group == ANIMATION_GROUP_GUTTER))
                {
                    state_index = ANIMATION_STATE_MELEE_FALL;
                }
                else
                {
                    state_index = m_state == HMS_RUN || m_state == HMS_SPRINT ? ANIMATION_STATE_CALM_FALL_RUN : ANIMATION_STATE_CALM_FALL;
                }
            }
            if (hp->char_inst->is_in_state(state_index))
            {
                human_get_animation_flags(hp, hp->cur_state, hp->cur_movement_state, flags);
                hp->animation_flags = flags;
                if (flags.ready)
                {
                    vanim_variants variant;
                    if (flags.left)
                    {
                        variant = ANIM_VARIANT_LEFT;
                    }
                    else if (flags.right)
                    {
                        variant = ANIM_VARIANT_RIGHT;
                    }
                    else if (!flags.back && !flags.backpedal)
                    {
                        variant = ANIM_VARIANT_FORWARD;
                    }
                    else
                    {
                        variant = ANIM_VARIANT_BACK;
                    }
                    vanim_set_variant(hp->char_inst->vp, state_index, variant, true, true);
                    anim_alias = ANIM_ALIAS_STATE_FALL;
                    new_state = state_index;
                    return;
                }
            }
            break;
        }
        case HMSM_FALL_WORRY:
            anim_alias = ANIM_ALIAS_STATE_FALL;
            if (hp->hflags.jumping_from_building)
            {
                new_state = ANIMATION_STATE_FALL_FROM_BUILDING;
            }
            else
            {
                new_state = m_state != HMS_RUN && m_state != HMS_SPRINT ? ANIMATION_STATE_WORRIED_FALL : ANIMATION_STATE_WORRIED_FALL_RUN;
            }
            return;
        case HMSM_FALL_PANIC:
            anim_alias = ANIM_ALIAS_STATE_FALL;
            new_state = ANIMATION_STATE_PANIC_FALL;
            return;
        case HMSM_FALL_FREE:
            anim_alias = ANIM_ALIAS_STATE_FALL;
            new_state = ANIMATION_STATE_VEHICLE_DIVE_EXIT;
            return;
        case HMSM_LAND:
        {
            human_get_animation_flags(hp, HS_FALL, m_state, flags);
            hp->animation_flags = flags;
            vanim_aliases unused_alias;
            if (weapon_class == WEAPON_CLASS_NONE)
            {
                get_unarmed_animation_state(m_state, hp, weapon_state, unused_alias);
            }
            else
            {
                get_weapon_animation_state(weapon_class, HS_CURRENT, HMS_IDLE, hp, weapon_state, anim_alias, blend_time, old_flags);
            }
            anim_alias = ANIM_ALIAS_STATE_FALL;
            new_state = weapon_state;
            return;
        }
        default:
            break;
        }
        anim_alias = ANIM_ALIAS_STATE_FALL;
        new_state = state_index;
        return;
    case HS_DEAD:
        new_state = ANIMATION_STATE_BK_LIEDOWN;
        return;
    case HS_PLAYER_PATHING:
    case HS_VEHICLE_ENTERING:
        if (object_is_player(hp))
        {
            float max_speed;
            float max_speed_base;
            player_get_max_movement_speeds((player*)hp, max_speed, max_speed_base);
            // Magic number: 0x409, the run blend tree's control index.
            vanim_set_blend_tree_control_input(hp->char_inst->vp, 0x409, hp->move_speed / max_speed_base);
        }
        // fall through
    case HS_STANDARD:
    case HS_ACTION_NODE:
        if (!hp->hflags.override_default_anim_state)
        {
            human_get_animation_flags(hp, state, m_state, flags);
            hp->animation_flags = flags;
            if (m_state == HMS_SPRINT)
            {
                new_state = flags.no_ammo ? ANIMATION_STATE_RUN_NO_AMMO : ANIMATION_STATE_RUN;
                return;
            }
            vanim_aliases unused_alias;
            if (weapon_class == WEAPON_CLASS_NONE)
            {
                get_unarmed_animation_state(m_state, hp, weapon_state, unused_alias);
            }
            else
            {
                get_weapon_animation_state(weapon_class, HS_CURRENT, HMS_IDLE, hp, weapon_state, anim_alias, blend_time, old_flags);
            }
            new_state = weapon_state;
            return;
        }
        // fall through
    case HS_LADDER:
    case HS_VEHICLE_SITTING:
    case HS_VEHICLE_DRIVING:
    case HS_VEHICLE_EXITING:
    case HS_AMMO_BOX:
    case HS_MP_DEFUSE:
        new_state = ANIMATION_STATE_NONE;
        return;
    case HS_COVER:
    {
        // The cover states: idle or walking along the cover, standing or crouched, side firing, leaning, leaning up
        // over the cover and blind firing, each with its fire action. Npcs lean in 0.3 s, players in 0.15 s.
        const human_move_submodes sub = hp->movement_submode;
        const bool charge = weapon_class == WEAPON_CLASS_THROWN_CHARGE;
        const float lean_blend = hp->get_sub_type() != OT_SUB_HUMAN_NPC ? 0.15f : 0.3f;
        if (m_state == HMS_IDLE)
        {
            if (!hp->hflags.crouch_cover && hp->cur_stance == HST_STAND)
            {
                switch (sub)
                {
                case HMSM_COVER_RIGHT:
                case HMSM_COVER_RIGHT_EDGE:
                    if (hp->hflags.side_firing)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_AIM_FIRE;
                        new_state = ANIMATION_STATE_COVER_STAND_RIGHT_AIM;
                        return;
                    }
                    state_index = ANIMATION_STATE_COVER_STAND_RIGHT;
                    if (charge && sub == HMSM_COVER_RIGHT_EDGE)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_BLIND_FIRE;
                        new_state = ANIMATION_STATE_COVER_STAND_RIGHT;
                        return;
                    }
                    break;
                case HMSM_COVER_LEFT:
                case HMSM_COVER_LEFT_EDGE:
                    if (hp->hflags.side_firing)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_AIM_FIRE;
                        new_state = ANIMATION_STATE_COVER_STAND_LEFT_AIM;
                        return;
                    }
                    state_index = ANIMATION_STATE_COVER_STAND_LEFT;
                    if (charge && sub == HMSM_COVER_LEFT_EDGE)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_BLIND_FIRE;
                        new_state = ANIMATION_STATE_COVER_STAND_LEFT;
                        return;
                    }
                    break;
                case HMSM_COVER_RIGHT_LEAN:
                    blend_time = lean_blend;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_LEAN_FIRE;
                    new_state = ANIMATION_STATE_COVER_STAND_RIGHT_LEAN;
                    return;
                case HMSM_COVER_LEFT_LEAN:
                    blend_time = lean_blend;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_LEAN_FIRE;
                    new_state = ANIMATION_STATE_COVER_STAND_LEFT_LEAN;
                    return;
                case HMSM_COVER_RIGHT_BLINDFIRE:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_BLIND_FIRE;
                    new_state = ANIMATION_STATE_COVER_STAND_RIGHT_BLIND_IDLE;
                    return;
                case HMSM_COVER_LEFT_BLINDFIRE:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_BLIND_FIRE;
                    new_state = ANIMATION_STATE_COVER_STAND_LEFT_BLIND_IDLE;
                    return;
                default:
                    break;
                }
            }
            else
            {
                // crouched behind a cover (or crouching behind a standing one)
                const bool crouch_cover = hp->hflags.crouch_cover;
                if (crouch_cover && !hp->combat.combat_aim_timestamp.elapsed() && hp->combat.combat_aim_timestamp.valid())
                {
                    switch (sub)
                    {
                    case HMSM_COVER_RIGHT:
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_RIGHT_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_RIGHT;
                        return;
                    case HMSM_COVER_RIGHT_EDGE:
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_BLIND_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_RIGHT;
                        return;
                    case HMSM_COVER_LEFT:
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_LEFT_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_LEFT;
                        return;
                    case HMSM_COVER_LEFT_EDGE:
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_BLIND_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_LEFT;
                        return;
                    default:
                        break;
                    }
                }
                switch (sub)
                {
                case HMSM_COVER_RIGHT_EDGE:
                    if (!crouch_cover || charge)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEAN_FIRE;
                        if (crouch_cover)
                        {
                            new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT;
                            return;
                        }
                    }
                    // fall through
                case HMSM_COVER_RIGHT:
                    if (!crouch_cover)
                    {
                        if (!hp->hflags.side_firing)
                        {
                            new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT;
                            return;
                        }
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_AIM_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_AIM;
                        return;
                    }
                    if (!hp->hflags.side_firing)
                    {
                        state_index = ANIMATION_STATE_COVER_CROUCH_RIGHT;
                    }
                    else
                    {
                        state_index = ANIMATION_STATE_COVER_CROUCH_RIGHT_AIM;
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_AIM_FIRE;
                    }
                    if (charge)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_BLIND_FIRE;
                        new_state = state_index;
                        return;
                    }
                    break;
                case HMSM_COVER_LEFT_EDGE:
                    if (!crouch_cover || charge)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_LEAN_FIRE;
                        if (crouch_cover)
                        {
                            new_state = ANIMATION_STATE_COVER_CROUCH_LEFT;
                            return;
                        }
                    }
                    // fall through
                case HMSM_COVER_LEFT:
                    if (!crouch_cover)
                    {
                        if (!hp->hflags.side_firing)
                        {
                            new_state = ANIMATION_STATE_COVER_CROUCH_LEFT;
                            return;
                        }
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_AIM_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_AIM;
                        return;
                    }
                    if (!hp->hflags.side_firing)
                    {
                        state_index = ANIMATION_STATE_COVER_CROUCH_LEFT;
                    }
                    else
                    {
                        state_index = ANIMATION_STATE_COVER_CROUCH_LEFT_AIM;
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_AIM_FIRE;
                    }
                    if (charge)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_BLIND_FIRE;
                        new_state = state_index;
                        return;
                    }
                    break;
                case HMSM_COVER_RIGHT_LEAN:
                    blend_time = lean_blend;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEAN_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_LEAN;
                    return;
                case HMSM_COVER_LEFT_LEAN:
                    blend_time = lean_blend;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_LEAN_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_LEAN;
                    return;
                case HMSM_COVER_RIGHT_LEANUP:
                    // Original: leaning up while crouching at a standing cover uses the ready fire action.
                    hp->custom_fire_animation = crouch_cover ? ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEANUP_FIRE : ANIMATION_ACTION_COVER_CROUCH_READY_RIGHT_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_LEANUP_IDLE;
                    return;
                case HMSM_COVER_LEFT_LEANUP:
                    hp->custom_fire_animation = crouch_cover ? ANIMATION_ACTION_COVER_CROUCH_LEFT_LEANUP_FIRE : ANIMATION_ACTION_COVER_CROUCH_READY_LEFT_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_LEANUP_IDLE;
                    return;
                case HMSM_COVER_RIGHT_BLINDFIRE:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_RIGHT_BLIND_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_BLIND_IDLE;
                    return;
                case HMSM_COVER_LEFT_BLINDFIRE:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_LEFT_BLIND_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_BLIND_IDLE;
                    return;
                case HMSM_COVER_RIGHT_BLINDFIRE2:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_BLIND_RIGHT_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_BLIND_RIGHT;
                    return;
                case HMSM_COVER_LEFT_BLINDFIRE2:
                    blend_time = 0.2f;
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_BLIND_LEFT_FIRE;
                    state_index = ANIMATION_STATE_COVER_CROUCH_BLIND_LEFT;
                    break;
                default:
                    break;
                }
            }
        }
        else if ((int)m_state > 0 && (int)m_state < 3)
        {
            // walking along the cover (HMS_WALK, HMS_RUN)
            if (!hp->hflags.crouch_cover)
            {
                if (hp->cur_stance == HST_STAND)
                {
                    if (sub == HMSM_COVER_RIGHT)
                    {
                        if (!hp->hflags.side_firing)
                        {
                            new_state = ANIMATION_STATE_COVER_STAND_RIGHT_WALK;
                            return;
                        }
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_RIGHT_AIM_FIRE;
                        new_state = hp->hflags.side_firing_walk_back ? ANIMATION_STATE_COVER_STAND_RIGHT_AIM_WALKBACK : ANIMATION_STATE_COVER_STAND_RIGHT_AIM_WALK;
                        return;
                    }
                    if (hp->hflags.side_firing)
                    {
                        if (!hp->hflags.side_firing_walk_back)
                        {
                            hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_AIM_WALK_FIRE;
                            new_state = ANIMATION_STATE_COVER_STAND_LEFT_AIM_WALK;
                            return;
                        }
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_STAND_LEFT_AIM_WALK_BACK_FIRE;
                        new_state = ANIMATION_STATE_COVER_STAND_LEFT_AIM_WALKBACK;
                        return;
                    }
                    new_state = ANIMATION_STATE_COVER_STAND_LEFT_WALK;
                    return;
                }
            }
            else
            {
                if (!hp->combat.combat_aim_timestamp.elapsed() && hp->combat.combat_aim_timestamp.valid())
                {
                    if (sub == HMSM_COVER_RIGHT)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_RIGHT_WALK_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_RIGHT_WALK;
                        return;
                    }
                    if (sub == HMSM_COVER_RIGHT_LEANUP)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_RIGHT_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_LEANUP_IDLE;
                        return;
                    }
                    if (sub != HMSM_COVER_LEFT_LEANUP)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_LEFT_WALK_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_READY_LEFT_WALK;
                        return;
                    }
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_LEFT_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_LEANUP_IDLE;
                    return;
                }
                if (!hp->combat.combat_blindfire_timestamp.elapsed() && hp->combat.combat_blindfire_timestamp.valid())
                {
                    if (sub != HMSM_COVER_RIGHT && sub != HMSM_COVER_RIGHT_BLINDFIRE2)
                    {
                        hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_BLIND_LEFT_WALK_FIRE;
                        new_state = ANIMATION_STATE_COVER_CROUCH_BLIND_LEFT_WALK;
                        return;
                    }
                    hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_BLIND_RIGHT_WALK_FIRE;
                    new_state = ANIMATION_STATE_COVER_CROUCH_BLIND_RIGHT_WALK;
                    return;
                }
            }
            if (sub != HMSM_COVER_RIGHT)
            {
                hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_LEFT_FIRE;
                if (!hp->hflags.side_firing)
                {
                    new_state = ANIMATION_STATE_COVER_CROUCH_LEFT_WALK;
                }
                else
                {
                    new_state = hp->hflags.side_firing_walk_back ? ANIMATION_STATE_COVER_CROUCH_LEFT_AIM_WALKBACK : ANIMATION_STATE_COVER_CROUCH_LEFT_AIM_WALK;
                }
                return;
            }
            hp->custom_fire_animation = ANIMATION_ACTION_COVER_CROUCH_READY_RIGHT_FIRE;
            if (!hp->hflags.side_firing)
            {
                new_state = ANIMATION_STATE_COVER_CROUCH_RIGHT_WALK;
            }
            else
            {
                new_state = hp->hflags.side_firing_walk_back ? ANIMATION_STATE_COVER_CROUCH_RIGHT_AIM_WALKBACK : ANIMATION_STATE_COVER_CROUCH_RIGHT_AIM_WALK;
            }
            return;
        }
        new_state = state_index;
        return;
    }
    default:
        new_state = state_index;
        return;
    }

    // HS_JUMP: the jump start state, with the side a ready player jumps towards relative to the camera
    switch (m_state)
    {
    case HMS_IDLE:
    case HMS_WALK:
        state_index = ANIMATION_STATE_STAND_JUMP_START;
        blend_time = 0.035f;
        break;
    case HMS_RUN:
    case HMS_SPRINT:
        state_index = ANIMATION_STATE_RUN_JUMP_START;
        blend_time = 0.035f;
        break;
    default:
        break;
    }
    if (hp->char_inst->is_in_state(state_index))
    {
        hp->char_inst->vp->desired_state->flags |= 0x10000;
        human_get_animation_flags(hp, hp->cur_state, hp->cur_movement_state, flags);
        hp->animation_flags = flags;
        player* pp;
        if (flags.ready && (pp = World.object_cast<player>(hp)) != nullptr)
        {
            vector look;
            if (object_is_local_player(pp))
            {
                look = Rfg_camera.free_params.flags.debug_mode ? pp->orient.fvec : Rfg_camera.ideal_orient.fvec;
            }
            else
            {
                look = pp->mp_camera_orient.fvec;
            }
            look.flatten_normalize_safe();
            const float angle = vec_delta_ang(hp->orient.fvec, look, UP_VECTOR);
            // Magic numbers: quarter sectors (pi/4 and 3 pi/4) around the human's forward direction.
            vanim_variants variant;
            if (angle > -0.7853982f && angle <= 0.7853982f)
            {
                variant = ANIM_VARIANT_FORWARD;
            }
            else if (angle > 0.7853982f && angle < 2.3561945f)
            {
                variant = ANIM_VARIANT_LEFT;
            }
            else if (angle > -0.7853982f || angle <= -2.3561945f)
            {
                variant = ANIM_VARIANT_BACK;
            }
            else
            {
                variant = ANIM_VARIANT_RIGHT;
            }
            vanim_set_variant(hp->char_inst->vp, state_index, variant, true, true);
        }
    }
    const float length = vanim_get_anim_length(hp->char_inst->vp, state_index);
    // Magic number: the take-off ends 0.6 s before the jump start animation does (computed in double).
    if ((length > 0.0f && (double)(float)hp->jump_state_timer.time_since() / 1000.0 > (double)length - (double)0.6f) ||
        (!hp->code_driven_start_jump && human_controller_is_on_ground(hp)))
    {
        hp->hflags.jump_take_off = 0;
    }
    anim_alias = ANIM_ALIAS_STATE_FALL;
    new_state = state_index;
}

// 0x00A2D4E0 (:3740)
// The heading offset between the human's facing and its movement: a combat ready player stopping (or flying the
// jetpack) faces the camera; walking backwards or sideways with a heavy weapon or zoomed turns the body around (pi);
// the player's direct control direction rotates the offset further; in cover a walking player turns along it.
static void human_set_steering_heading_offset(human* hp)
{
    if (hp->cur_state == HS_TURRET || hp->vehicle_handle != 0xFFFFFFFF)
    {
        return;
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    // Original: pp may be null here; the remote camera of a null player is never read in single player.
    const matrix* camera_orient = Multiplayer ? &pp->mp_camera_orient : &Rfg_camera.real_orient;
    switch (hp->cur_state)
    {
    case HS_STANDARD:
    {
        if (hp->hflags.override_steering_heading_offset)
        {
            return;
        }
        hp->steering_heading_offset = 0.0f;
        if (pp != nullptr && human_is_combat_ready(hp) && pp->pflags.moving_to_a_stop)
        {
            if (!hp->steering_vector.is_null())
            {
                const float steering = fl_atan2_safe(hp->steering_vector.x, hp->steering_vector.z);
                hp->steering_heading_offset = camera_orient->extract_heading() - steering;
                return;
            }
            hp->steering_vector = camera_orient->fvec;
            hp->steering_vector.flatten_normalize_safe();
            return;
        }
        const weapon_anim_flags flags = hp->animation_flags;
        bool turn_around;
        animation_group group = ANIMATION_GROUP_DEFAULT;
        bool check_group = false;
        if (!flags.run && !flags.crouch)
        {
            // walking upright
            if (pp == nullptr)
            {
                if (!flags.left && !flags.right && flags.back)
                {
                    hp->steering_heading_offset = 3.1415927f;
                    return;
                }
                hp->steering_heading_offset = 0.0f;
                return;
            }
            if (flags.left)
            {
                turn_around = player_is_zoomed(pp);
                if (!turn_around)
                {
                    group = pp->char_inst->vp->default_group_index;
                    check_group = true;
                }
            }
            else if (flags.right)
            {
                turn_around = false;
            }
            else if (flags.back)
            {
                turn_around = player_is_zoomed(pp);
            }
            else if (!flags.backpedal)
            {
                turn_around = false;
            }
            else
            {
                turn_around = player_is_zoomed(pp);
                if (!turn_around)
                {
                    group = pp->char_inst->vp->default_group_index;
                    check_group = true;
                }
            }
        }
        else if (!flags.crouch)
        {
            // running upright
            if (pp == nullptr)
            {
                const animation_group g = hp->char_inst->vp->default_group_index;
                if (!flags.left && !flags.right && !flags.backpedal && flags.back &&
                    (g == ANIMATION_GROUP_RPG || g == ANIMATION_GROUP_ARC || g == ANIMATION_GROUP_THERMOBARIC_ROCKET_LAUNCHER || g == ANIMATION_GROUP_GRINDER))
                {
                    hp->steering_heading_offset = 3.1415927f;
                    return;
                }
                hp->steering_heading_offset = 0.0f;
                return;
            }
            if (flags.left)
            {
                turn_around = player_is_zoomed(pp);
                if (!turn_around)
                {
                    group = pp->char_inst->vp->default_group_index;
                    check_group = true;
                }
            }
            else if (flags.right || (!flags.backpedal && !flags.back))
            {
                turn_around = false;
            }
            else
            {
                turn_around = player_is_zoomed(pp);
            }
        }
        else
        {
            // crouched
            if (pp == nullptr)
            {
                if (!flags.left && !flags.right && flags.back)
                {
                    hp->steering_heading_offset = 3.1415927f;
                    return;
                }
                hp->steering_heading_offset = 0.0f;
                return;
            }
            if (flags.left)
            {
                turn_around = player_is_zoomed(pp);
                if (!turn_around)
                {
                    group = hp->char_inst->vp->default_group_index;
                    check_group = true;
                }
            }
            else if (flags.right)
            {
                turn_around = false;
            }
            else if (!flags.backpedal)
            {
                turn_around = flags.back;
            }
            else
            {
                turn_around = true;
            }
        }
        if (check_group)
        {
            turn_around = group == ANIMATION_GROUP_RPG || group == ANIMATION_GROUP_ARC || group == ANIMATION_GROUP_THERMOBARIC_ROCKET_LAUNCHER || group == ANIMATION_GROUP_GRINDER;
        }
        hp->steering_heading_offset = turn_around ? 3.1415927f : 0.0f;
        if (pp->direct_control_vel.is_null())
        {
            return;
        }
        if (hp->block_movement_anim != -1 && hp->block_movement_type != BLOCK_MOVEMENT_POS && hp->block_movement_type != BLOCK_MOVEMENT_POS_AND_ORIENT)
        {
            return;
        }
        const float control = fl_atan2_safe(pp->direct_control_vel.x, pp->direct_control_vel.z);
        const float steering = fl_atan2_safe(pp->steering_vector.x, pp->steering_vector.z);
        pp->steering_heading_offset = (control - steering) + pp->steering_heading_offset;
        return;
    }
    case HS_JUMP:
    case HS_FALL:
        if (pp != nullptr && human_is_combat_ready(pp) && pp->pflags.jetpack_started)
        {
            if (!hp->steering_vector.is_null())
            {
                const float steering = fl_atan2_safe(hp->steering_vector.x, hp->steering_vector.z);
                hp->steering_heading_offset = camera_orient->extract_heading() - steering;
                return;
            }
            hp->steering_vector = camera_orient->fvec;
            hp->steering_vector.flatten_normalize_safe();
            return;
        }
        break;
    case HS_COVER:
        if (hp->get_sub_type() != OT_SUB_HUMAN_NPC)
        {
            if (hp->cur_movement_state != HMS_WALK)
            {
                if (hp->movement_submode == HMSM_COVER_LEFT_LEAN)
                {
                    hp->steering_heading_offset = 1.5707964f;
                }
                return;
            }
            const float dot = pp->cover_vector * hp->steering_vector;
            if (dot <= -0.9f || dot >= 0.9f)
            {
                return;
            }
            const float angle = fl_acos(dot);
            const human_move_submodes sub = hp->movement_submode;
            if (sub == HMSM_COVER_RIGHT || sub == HMSM_COVER_RIGHT_BLINDFIRE2)
            {
                hp->steering_heading_offset = hp->hflags.side_firing && hp->hflags.side_firing_walk_back ? -fabsf(angle) : fabsf(angle);
            }
            else if (sub == HMSM_COVER_LEFT || sub == HMSM_COVER_LEFT_BLINDFIRE2)
            {
                hp->steering_heading_offset = !hp->hflags.side_firing || !hp->hflags.side_firing_walk_back ? -fabsf(angle) : fabsf(angle);
            }
            return;
        }
        break;
    default:
        break;
    }
    hp->steering_heading_offset = 0.0f;
}

// 0x00ABEDF0 (:3943)
// Sets the state animation the human should be in and runs its state machine: the looked-up state (with the
// player's run instead of walk while armed, sprint delays and landing blends), npc ambient prop states and the
// civilian jog, the state change with its fire action swap, rotation limits of a ready player, the run blend tree
// inputs, the transition's foot phase, the jump variant, and the incline input of nearby npcs.
void human_set_and_process_animation_states(human* hp)
{
    animation_state state = ANIMATION_STATE_NONE;
    vanim_aliases alias = ANIM_ALIAS_NONE;
    float blend_time = 0.0f;
    human_lookup_animation_state(hp, state, alias, blend_time);
    player* pp;
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        pp = nullptr;
        vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIM_ALIAS_RUN_STAND_BLEND, ANIM_ALIAS_NONE), hp->run_stand_blend_pose_weight, 0x20000);
    }
    else
    {
        pp = (player*)hp;
        if (cutscene_is_playing())
        {
            state = ANIMATION_STATE_STAND;
        }
        else
        {
            if (hp->get_equipped_weapon() != nullptr && state == ANIMATION_STATE_WALK)
            {
                // an armed player runs where it would walk
                state = ANIMATION_STATE_RUN;
                blend_time = 0.1f;
            }
            else if (state == ANIMATION_STATE_STAND || state == ANIMATION_STATE_STAND_READY)
            {
                v_animate* vp = hp->char_inst->vp;
                if ((!hp->combat.combat_ready_timestamp.valid() || hp->combat.combat_ready_timestamp.elapsed()) && vp->desired_state != nullptr &&
                    vanim_is_blend_tree(vp, vp->desired_state->instance) && vanim_get_blend_tree_ramped_input(vp, vp->desired_state->instance) > 0.0f)
                {
                    // still blending the run tree down to a stop: keep it
                    state = ANIMATION_STATE_NONE;
                }
            }
            // Magic number: the binary divides by the double 1000.0 and rounds to float.
            const float sprint_delay = (float)((double)(float)pp->sprint_delay_timestamp.time_until() / 1000.0);
            if (state != ANIMATION_STATE_SPRINT && (double)sprint_delay > (double)blend_time && alias != ANIM_ALIAS_STATE_FALL)
            {
                blend_time = sprint_delay;
            }
        }
        // Magic number: landing blends in at most 0.15 s (computed in double).
        if (hp->previous_movement_mode == HMM_FALL && (float)((double)0.15f - (double)blend_time) < 0.0f)
        {
            blend_time = 0.15f;
        }
    }
    bool set_blend_tree_inputs = false;
    bool skip_steering = false;
    if (state != ANIMATION_STATE_NONE)
    {
        int handle = -1;
        bool prop_state = false;
        bool lookup_handle = true;
        if (pp == nullptr)
        {
            const ambient_prop_info* api = hp->amb_prop_info;
            if (api != nullptr && hp->char_inst->vp->base_anim_set_variant == ANIM_SET_VARIANT_NONE)
            {
                if (state == ANIMATION_STATE_WALK)
                {
                    handle = api->prop_walk_state;
                }
                else if (state == ANIMATION_STATE_STAND)
                {
                    handle = api->prop_stand_state;
                }
                prop_state = handle != -1;
            }
            // Magic number: running civilians and guerrillas (not fleeing) jog at 5.75 m/s at most.
            if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && (hp->get_team() == HUMAN_TEAM_CIVILIAN || hp->get_team() == HUMAN_TEAM_GUERILLA) &&
                hp->char_inst->vp->default_group_index == ANIMATION_GROUP_DEFAULT && (int)hp->cur_movement_state > HMS_WALK && !((npc*)hp)->get_ai()->is_fleeing())
            {
                state = ANIMATION_STATE_WALK_YELLOW_ALERT;
                if (hp->move_speed > 5.75f)
                {
                    hp->move_speed = 5.75f;
                }
            }
            if (prop_state)
            {
                set_blend_tree_inputs = true;
                lookup_handle = false;
            }
        }
        if (lookup_handle)
        {
            v_animate* vp = hp->char_inst->vp;
            handle = vanim_get_state_handle(vp, vp->default_group_index, state);
            set_blend_tree_inputs = (hp->cur_state == HS_STANDARD || hp->cur_state == HS_PLAYER_PATHING) && (int)hp->cur_movement_state > HMS_IDLE;
        }
        if (handle != -1)
        {
            v_anim* desired = hp->char_inst->vp->desired_state;
            if (desired == nullptr || desired->anim_handle != handle)
            {
                if (prop_state)
                {
                    vanim_change_custom_state(hp->char_inst->vp, index_alias(handle, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
                    hp->char_inst->vp->desired_state->index = state;
                }
                else
                {
                    hp->char_inst->set_state(index_alias(state, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), blend_time, false);
                    hp->last_stance = hp->cur_stance;
                    weapon* wp;
                    if (hp->char_inst->is_playing_action(ANIM_ALIAS_FIRE) && (wp = hp->get_equipped_weapon()) != nullptr)
                    {
                        const animation_action attack = human_weapons_firearm_get_attack_animation(wp->w_info->weapon_class, hp);
                        vanim_swap_animation(hp->char_inst->vp, ANIM_ALIAS_FIRE, index_alias(attack, ANIM_ALIAS_FIRE));
                    }
                }
                if (pp == nullptr)
                {
                    if (!set_blend_tree_inputs)
                    {
                        vanim_set_blend_tree_init_control_input(hp->char_inst->vp, 0x409, 0.0f);
                    }
                }
                else
                {
                    if (hp->char_inst->vp->desired_state != nullptr && (pp->exiting_cover.elapsed() || !pp->exiting_cover.valid()))
                    {
                        if (!human_is_combat_ready(hp) || (hp->cur_state == HS_COVER && pp->entering_cover.elapsed()) || vanim_check_editor_flag_ramped(hp->char_inst->vp, 0x400))
                        {
                            hp->rotate_dir = HUMAN_ROTATE_SHORTEST;
                        }
                        else if (!vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_DIVE))
                        {
                            if (hp->rotate_dir == HUMAN_ROTATE_SHORTEST)
                            {
                                vector look;
                                human_get_look_at_vector(hp, look);
                                const float look_heading = fl_atan2_safe(look.x, look.z);
                                const float delta = hp->orient.extract_heading() - look_heading;
                                hp->rotate_internal_heading = fl_angle_cap_radians(delta, hp->rotate_heading_left);
                            }
                            hp->rotate_dir = HUMAN_ROTATE_USE_LIMITS;
                        }
                    }
                    // Magic number: 5 m/s, the speed of the run blend tree's full input.
                    v_animate* vp = hp->char_inst->vp;
                    const float init_input = vp->secondary_state == nullptr || vp->secondary_state->index != ANIMATION_STATE_STAND ? hp->move_speed / 5.0f : 0.0f;
                    vanim_set_blend_tree_init_control_input(vp, 0x409, init_input);
                    const vector& dcv = pp->direct_control_vel;
                    vanim_set_blend_tree_control_input(hp->char_inst->vp, state, sqrtf(dcv.x * dcv.x + dcv.y * dcv.y + dcv.z * dcv.z) / 5.0f);
                }
                v_animate* vp = hp->char_inst->vp;
                if (vp->secondary_state != nullptr)
                {
                    if (vanim_is_transitioning_to_still_state(vp) || alias == ANIM_ALIAS_STATE_FALL)
                    {
                        vp->flags.next_state_start_at_zero = 1;
                    }
                    else
                    {
                        // the new state starts at the same foot phase as the old one
                        const float old_foot = vanim_get_trigger_time(vp, Trigger_left_foot, vp->secondary_state->instance, 0);
                        const float new_foot = vanim_get_trigger_time(vp, Trigger_left_foot, vp->desired_state->instance, 0);
                        if (old_foot >= 0.0f && new_foot >= 0.0f)
                        {
                            // The lower clamps are NaN-safe as in rfg.exe (FCOMIP then JC, taken when unordered): a state
                            // of length 0 (the turret poses) gives 0/0 here. Written as "< 0" the NaN went on into the
                            // state's init cyclic percent, made the instance's cyclic_percent NaN for good and every
                            // looping animation after it a NaN pose (the invisible player after the turret Demolitions
                            // Master, 2026-11-06).
                            float new_pct = new_foot / vanim_get_anim_length(vp, vp->desired_state->instance);
                            if (!(new_pct - 0.0f >= 0.0f))
                            {
                                new_pct = 0.0f;
                            }
                            if (new_pct - 1.0f >= 0.0f)
                            {
                                new_pct = 1.0f;
                            }
                            float old_pct = old_foot / vanim_get_anim_length(vp, vp->secondary_state->instance);
                            if (!(old_pct - 0.0f >= 0.0f))
                            {
                                old_pct = 0.0f;
                            }
                            if (old_pct - 1.0f >= 0.0f)
                            {
                                old_pct = 1.0f;
                            }
                            float pct = new_pct - old_pct;
                            if (pct < 0.0f)
                            {
                                pct += 1.0f;
                            }
                            vanim_set_transition_ending_percent(vp, pct);
                        }
                    }
                }
                if (hp->cur_state == HS_JUMP)
                {
                    hp->char_inst->vp->desired_state->flags |= 0x10000;
                    if (hp->animation_flags.ready && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                    {
                        vector look;
                        if (object_is_local_player(hp))
                        {
                            look = Rfg_camera.free_params.flags.debug_mode ? hp->orient.fvec : Rfg_camera.ideal_orient.fvec;
                        }
                        else
                        {
                            look = ((player*)hp)->mp_camera_orient.fvec;
                        }
                        look.flatten_normalize_safe();
                        const float angle = vec_delta_ang(hp->orient.fvec, look, UP_VECTOR);
                        vanim_variants variant;
                        if (angle > -0.7853982f && angle <= 0.7853982f)
                        {
                            variant = ANIM_VARIANT_FORWARD;
                        }
                        else if (angle > 0.7853982f && angle < 2.3561945f)
                        {
                            variant = ANIM_VARIANT_LEFT;
                        }
                        else if (angle > -0.7853982f || angle <= -2.3561945f)
                        {
                            variant = ANIM_VARIANT_BACK;
                        }
                        else
                        {
                            variant = ANIM_VARIANT_RIGHT;
                        }
                        vanim_set_variant(hp->char_inst->vp, state, variant, true, true);
                    }
                }
            }
            if (pp == nullptr && set_blend_tree_inputs)
            {
                // the incline input: the pitch of the velocity of a supported npc near the camera, in degrees
                float incline = 0.0f;
                const double max_dist = (double)Npc_use_incline_blend_tree_dist;
                if ((double)vec_dist_squared(hp->pos, Rfg_camera.real_pos) < max_dist * max_dist && hp->rdd.is_loaded() && hp->rdd.get_data()->controller->flags.is_supported)
                {
                    const float speed_xz = hp->actual_velocity.mag_xz();
                    if (speed_xz > 0.1f)
                    {
                        const float pitch = (float)atan2((double)hp->actual_velocity.y, (double)speed_xz);
                        // Magic number: 57.29583 degrees per radian (as the double of the float constant).
                        incline = (float)((double)pitch * (double)57.29583f);
                    }
                }
                vanim_set_all_blend_tree_control_inputs(hp->char_inst->vp, state, incline);
            }
        }
    }
    if (pp != nullptr && pp == Player && Rfg_camera.mode != CAMERA_FREE_MODE && Rfg_camera.mode != CAMERA_THIRD_PERSON_MODE)
    {
        // the camera modes that steer the player themselves (first person, turret...)
        skip_steering = true;
    }
    if (!skip_steering)
    {
        human_set_steering_heading_offset(hp);
    }
    if (!hp->hflags.hidden)
    {
        vanim_process_states(hp->char_inst->vp);
    }
}

// 0x00A06E40 (:4211)
// The first of the four selection slots no inventory item uses, or -1.
static int human_find_open_inventory_selection_slot(human* hp)
{
    inventory_item* first = hp->inventory;
    char used[4] = {0, 0, 0, 0};
    for (inventory_item* iip = first; iip != nullptr;)
    {
        if (iip->selection_slot != -1)
        {
            used[iip->selection_slot] = 1;
        }
        iip = iip->next;
        if (iip == first)
        {
            break;
        }
    }
    for (int slot = 0; slot < 4; slot++)
    {
        if (!used[slot])
        {
            return slot;
        }
    }
    return -1;
}

// 0x00AB1260 (:4304)
// The pain or death voice line of a damaged human (not for the local player's pain in single player), and the
// sound that tells nearby npcs (silent deaths, deaths, and pain at most every 2 s).
void human_play_pain_sound(human* target, const human* attacker, const weapon_info* w_info, damaged_by_types damage_type, bool silent)
{
    if (target == nullptr)
    {
        return;
    }
    if (silent)
    {
        if (attacker != nullptr)
        {
            npc_sound_info_silent_death info(target, attacker, w_info);
            npc_sound_info::queue<npc_sound_info_silent_death>(info);
        }
        return;
    }
    if (target->is_killed())
    {
        if (localization_allow_pain_sounds())
        {
            const voice_lines line = damage_type == DBT_MELEE || w_info == nullptr || (w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE && w_info->weapon_class != WEAPON_CLASS_GRINDER)
                                         ? VOICE_LINE_DEATH
                                         : VOICE_LINE_DEATH_SCREAM;
            if (target->play_voice_line(line, nullptr, nullptr, target->voice_cue_priority, false, true) == PLAY_VOICE_SUCCESS)
            {
                target->set_voice_play_inst(-1);
            }
        }
        if (attacker != nullptr)
        {
            npc_sound_info_death info(target, attacker, w_info);
            npc_sound_info::queue<npc_sound_info_death>(info);
        }
    }
    else if (localization_allow_pain_sounds() && ((player*)target != Player || Multiplayer))
    {
        voice_lines line;
        bool play = true;
        if (w_info == nullptr || damage_type == DBT_MELEE || w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            line = VOICE_LINE_PAIN_BLUNT;
        }
        else if (w_info->weapon_class == WEAPON_CLASS_ARC_WELDER)
        {
            line = VOICE_LINE_PAIN_ELECTRIC;
        }
        else if (damage_type == DBT_BULLET && human_combat_is_manning_turret(target))
        {
            play = false; // turret operators make no pain sounds from bullets
            line = VOICE_LINE_PAIN_BULLET;
        }
        else if (object_is_player(target) || attacker == nullptr || attacker == target || !human_teams_are_allies(attacker, target) || !w_info->is_firearm())
        {
            line = VOICE_LINE_PAIN_BULLET;
        }
        else
        {
            line = VOICE_LINE_COMBAT_FRIENDLY_FIRE_VICTIM;
        }
        if (play)
        {
            target->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    // Magic number: npcs hear a human's pain at most every 2000 ms.
    if (!target->is_killed() && attacker != nullptr && target->combat.pain_sound_timestamp.elapsed())
    {
        npc_sound_info_pain info(target, attacker, w_info);
        npc_sound_info::queue<npc_sound_info_pain>(info);
        target->combat.pain_sound_timestamp.set(2000);
    }
}

// 0x00A7B260 (:4659)
// Only the first reserved node is released; the next one (if any) picks its action state, else the set empties.
void human::release_action_node(unsigned int action_node_handle)
{
    if (action_node_data.action_node_set.size() <= 0 || action_node_data.action_node_set[0] != action_node_handle)
    {
        return;
    }
    action_node_data.action_node_set.remove(0);
    object_action_node* anp = action_node_data.action_node_set.size() > 0 ? get_desired_action_node() : nullptr;
    if (anp != nullptr)
    {
        action_node_data.action_state = anp->pick_action_state();
        return;
    }
    action_node_data.action_node_set.remove_all();
}

// 0x00A7B2E0 (:4691)
void human::release_action_node(object_action_node* anp)
{
    if (anp != nullptr)
    {
        release_action_node(anp->get_handle());
    }
}

// 0x00A07380 (:4577)
bool human::has_entered_action_node(const object_action_node* anp) const
{
    const unsigned int node_handle = anp->get_handle();
    if (node_handle == 0xFFFFFFFF)
    {
        return false;
    }
    return action_node_data.current_action_node == node_handle;
}

// 0x00A2DAE0 (:4709)
bool human::has_node_reserved(unsigned int action_node_handle) const
{
    if (action_node_handle == 0xFFFFFFFF || is_killed())
    {
        return false;
    }
    for (int i = 0; i < action_node_data.action_node_set.size(); i++)
    {
        if (action_node_data.action_node_set[i] == action_node_handle)
        {
            return true;
        }
    }
    return action_node_data.current_action_node == action_node_handle;
}

// 0x00A2DB50 (:4743)
bool human::has_node_reserved(const farray<unsigned int,4>& action_node_set) const
{
    if (is_killed() || action_node_set.size() != action_node_data.action_node_set.size())
    {
        return false;
    }
    for (int i = 0; i < action_node_set.size(); i++)
    {
        if (action_node_set[i] != action_node_data.action_node_set[i])
        {
            return false;
        }
    }
    return true;
}

// 0x00A52DC0 (:4584) (LTCG: destination_list in EDI)
static bool convert_action_node_list(farray<object_action_node*,608>& destination_list, const farray<unsigned int,4>& action_node_set)
{
    destination_list.clear();
    for (int i = 0; i < action_node_set.size(); i++)
    {
        object_action_node* anp = action_node_set[i] != 0xFFFFFFFF ? World.object_from_handle<object_action_node>(action_node_set[i]) : nullptr;
        if (anp == nullptr)
        {
            return false;
        }
        destination_list.add(anp);
    }
    return true;
}

// 0x00A7B170 (:4605)
bool human::reserve_action_node(const farray<unsigned int,4>& action_node_set)
{
    farray<object_action_node*,608> nodes;
    if (has_node_reserved(action_node_set))
    {
        return true;
    }
    if (!convert_action_node_list(nodes, action_node_set) || nodes.size() < 1)
    {
        return false;
    }
    for (int i = 0; i < nodes.size(); i++)
    {
        if (nodes[i]->disabled_by != 0)
        {
            return false;
        }
        const human* occupant = nodes[i]->get_occupant();
        if (occupant != nullptr && occupant != this)
        {
            return false;
        }
    }
    action_node_data.action_node_set.clear();
    for (int i = 0; i < nodes.size(); i++)
    {
        const unsigned int node_handle = nodes[i]->get_handle();
        action_node_data.action_node_set.add(node_handle);
    }
    action_node_data.action_state = nodes[0]->pick_action_state();
    return true;
}

// 0x00A2DBB0 (:4778)
bool human::enter_action_node(const object_action_node* anp, int duration_ms)
{
    if (!has_node_reserved(anp->get_handle()))
    {
        return false;
    }
    action_node_data.current_action_node = anp->get_handle();
    action_node_data.state_duration.set(duration_ms > 0 ? duration_ms : 0);
    return true;
}

// 0x00A52E30 (:4767)
// The action state of the first reserved node (0 for any other node, negative states as 0).
int human::get_action_node_index(const object_action_node* anp) const
{
    const unsigned int first = action_node_data.action_node_set.size() < 1 ? 0xFFFFFFFF : action_node_data.action_node_set[0];
    if (anp->get_handle() == first)
    {
        const int state = (int)action_node_data.action_state;
        return state < 0 ? 0 : state;
    }
    return 0;
}

// 0x00A07400 (:4851)
bool human::completed_action_node(const object_action_node* anp) const
{
    if (anp->get_handle() == action_node_data.current_action_node && !anp->node_flags.infinite_duration)
    {
        return action_node_data.state_duration.valid() && action_node_data.state_duration.elapsed();
    }
    return false;
}

// 0x00A07450 (:4917)
bool human::is_killed() const
{
    return hit_points <= 0.0f;
}

// 0x00A07470 (:4923): invulnerable by flag, and the story characters (guerrilla techs, the RFC and the old coot) in the
// main game outside missions.
bool human::is_invulnerable() const
{
    if (hflags.invulnerable)
    {
        return true;
    }
    return !Multiplayer && !wrecking_crew_is_active() && (h_info->flags.guerrilla_tech || h_info->flags.guerrilla_rfc || h_info->flags.old_coot) && !mission_is_active();
}

// 0x00A99F90 (:4940): halfway between the feet and the head.
void human::get_center_pos(vector& output) const
{
    vector head_pos;
    human_get_head_pos(this, head_pos);
    output.x = (pos.x + head_pos.x) * 0.5f;
    output.y = (pos.y + head_pos.y) * 0.5f;
    output.z = (pos.z + head_pos.z) * 0.5f;
}

// 0x00A074B0 (:4948)
bool human::render_on_minimap() const
{
    return hflags.always_show_on_minimap != 0 || is_in_combat() || minimap_flags != 0;
}

// 0x00AB4D20 (:5031)
// Equips an inventory item: the weapon of the old item goes back to its attachment point (players carry it) or is
// released and detached, its off-hand projectile and shield are destroyed; the new weapon is attached to the hand
// tag, with its off-hand projectile item and the riot shield; the animation group follows the weapon.
void human::set_equipped_inv_item(inventory_item* iip)
{
    weapon* wp = nullptr;
    if (equipped_inv_item != nullptr)
    {
        const unsigned int weapon_handle = equipped_inv_item->weapon_handle;
        if (weapon_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(weapon_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
            {
                wp = (weapon*)objp;
            }
        }
    }
    if (equipped_inv_item != nullptr && wp != nullptr)
    {
        if (get_sub_type() == OT_SUB_HUMAN_PLAYER && last_equipped_weapon != nullptr && last_equipped_weapon->attachment_prop >= 0)
        {
            set_child(wp, last_equipped_weapon->attachment_prop, wp->get_grip_tag_index(), false, true);
        }
        else
        {
            human_weapons_firearm_release_trigger(this);
            wp->hold_item(this, false);
            remove_child(wp);
        }
        World.destroy_object(offhand_projectile_handle, false);
        offhand_projectile_handle = 0xFFFFFFFF;
        World.destroy_object(shield_handle, false);
        shield_handle = 0xFFFFFFFF;
    }
    equipped_inv_item = iip;
    wp = nullptr;
    if (iip != nullptr && iip->weapon_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(iip->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            wp = (weapon*)objp;
        }
    }
    if (equipped_inv_item != nullptr && wp != nullptr)
    {
        const int parent_prop_point = wp->w_info->flags.held_in_left_hand ? equip_offhand_tag_index : equip_tag_index;
        set_child(wp, parent_prop_point, wp->get_grip_tag_index(), false, true);
        wp->hold_item(this, true);
        if (wp->w_info->flags.draw_proj_in_opposite_hand && (wp->w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE || can_fire_charge(get_handle(), true)))
        {
            obj_item_flags flags;
            *(unsigned int*)&flags = 0;
            flags.cast_drop_shadows = 1;
            flags.cast_shadows = 1;
            flags.simulate = 1;
            flags.no_physics = 1;
            obj_item_props props(wp->w_info->projectile_info.obj_item_info, wp->pos, wp->orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
            item* offhand = World.create_object<item>(props, 0xFFFFFFFF);
            if (offhand == nullptr)
            {
                // Original: "Unable to create offhand item for weapon %s - World.create_object() failed." into the
                // thread's debug message buffer.
            }
            else
            {
                offhand_projectile_handle = offhand->get_handle();
                const int hand = wp->w_info->flags.held_in_left_hand ? equip_tag_index : equip_offhand_tag_index;
                set_child(offhand, hand, -1, false, true);
                offhand->set_owner(get_handle(), true);
                offhand->hold_item(this, wp->is_loaded());
            }
        }
        if (hflags.riot_shield)
        {
            const item_info* shield_info = object_item_get_info("riot_shield");
            if (shield_info != nullptr && !create_shield_item(shield_info, true))
            {
                World.destroy_object(shield_handle, false);
            }
        }
        if ((player*)this == Player)
        {
            if (get_obj_type() == OT_HUMAN && get_sub_type() == OT_SUB_HUMAN_PLAYER && wp->w_info != nullptr)
            {
                if (v_stricmp(wp->w_info->name, "rail_driver") == 0)
                {
                    hud_ui_set_loaded_module(HUD_MODULE_RAIL_DRIVER);
                }
                else if (v_stricmp(wp->w_info->name, "sniper_rifle") == 0)
                {
                    hud_ui_set_loaded_module(HUD_MODULE_SNIPER);
                }
            }
            if (ovl_inventory_is_open())
            {
                ovl_inventory_update_avail_weapons();
            }
        }
        if (get_sub_type() == OT_SUB_HUMAN_PLAYER && get_obj_type() == OT_HUMAN && ((player*)this)->backpack != nullptr)
        {
            ((player*)this)->backpack->process_equip_item();
        }
    }
    if (char_inst != nullptr)
    {
        const animation_group group = equipped_inv_item == nullptr || wp == nullptr ? ANIMATION_GROUP_DEFAULT : wp->w_info->weapon_anim_group;
        char_inst->set_animation_group(group, index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        if (get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            const vector& dcv = ((player*)this)->direct_control_vel;
            vanim_set_blend_tree_init_control_input(char_inst->vp, 0x409, sqrtf(dcv.x * dcv.x + dcv.y * dcv.y + dcv.z * dcv.z) / 5.0f);
            vanim_set_blend_tree_control_input(char_inst->vp, 0x409, sqrtf(dcv.x * dcv.x + dcv.y * dcv.y + dcv.z * dcv.z) / 5.0f);
        }
    }
    if (Multiplayer && !multi_is_server() && get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        // Magic number: 7, no slot.
        ((player*)this)->mp_desired_weapon_slot = iip != nullptr ? iip->selection_slot : 7;
    }
}

// 0x00A7B390 (:5187)
// The weapon in the human's hands: the turret's primary weapon while manning one, else the equipped item's.
weapon* human::get_equipped_weapon() const
{
    unsigned int obj_handle;
    turret* tp = human_combat_get_manned_turret(this);
    if (tp == nullptr)
    {
        if (equipped_inv_item == nullptr)
        {
            return nullptr;
        }
        obj_handle = equipped_inv_item->weapon_handle;
    }
    else
    {
        obj_handle = tp->primary_weapon_handle;
    }
    if (obj_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(obj_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            return (weapon*)objp;
        }
    }
    return nullptr;
}

// 0x00A2DC70 (:5202)
bool human::fade_with_vehicle() const
{
    if (vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            const vehicle* vp = (const vehicle*)objp;
            return vp->m_flags.m_fading_in || vp->m_flags.m_fading_out;
        }
    }
    return false;
}

// 0x00A07540 (:5229)
void human::fade_out(int new_fade_time, bool fading_from_nano)
{
    hflags.fading_in = 0;
    hflags.fading_out = 1;
    hflags.fading_out_from_nano = fading_from_nano;
    fade_timer.set(0);
    fade_time = new_fade_time;
}

// 0x00A2DCB0 (:5241)
// A human in a fading vehicle fades with it, for the rest of the vehicle's fade time.
void human::sync_to_vehicle_opacity()
{
    if (vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = (vehicle*)objp;
    const int remaining = vp->m_fade_time - vp->m_fade_timer.time_since();
    if (remaining < 0)
    {
        return;
    }
    if (vp->m_flags.m_fading_in)
    {
        hflags.fading_in = 1;
        hflags.fading_out_from_nano = 0;
        hflags.fading_out = 0;
        fade_timer.set(0);
        fade_time = remaining;
        return;
    }
    if (vp->m_flags.m_fading_out && !vp->m_flags.m_fading_from_nano)
    {
        fade_out(remaining, vp->m_flags.m_fading_from_nano);
    }
}

// 0x00A07590 (:5294)
// A character tag, else a bone, of the human's rig.
int human::get_tag_index_from_name(const char* tag_name) const
{
    int index = -1;
    if (tag_name != nullptr && char_inst != nullptr)
    {
        index = animlib_find_character_tag(char_inst->aim_handle, tag_name);
        if (index == -1)
        {
            index = animlib_find_character_bone(char_inst->aim_handle, tag_name);
        }
    }
    return index;
}

// 0x00A2DDC0 (:5309)
// A tag in world space for the given (or the human's own) placement; the placement itself until the character is
// loaded.
bool human::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    const vector* p = obj_pos != nullptr ? obj_pos : &pos;
    const matrix* o = obj_orient != nullptr ? obj_orient : &orient;
    if (!char_inst->rdd.is_loaded())
    {
        tag_pos = *p;
        if (tag_orient != nullptr)
        {
            *tag_orient = *o;
        }
        return false;
    }
    animlib_get_tag_world_coords(char_inst->aim_handle, tag_index, *p, *o, tag_pos, tag_orient);
    return true;
}

// 0x00A2DE50 (:5332)
bool human::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    if (!char_inst->rdd.is_loaded())
    {
        return false;
    }
    animlib_get_tag_local_coords(char_inst->aim_handle, tag_index, tag_pos, tag_orient);
    return true;
}

// 0x00ABC490 (:5346)
// Moves the equipped item towards the desired one. When they match, a finished equip drops the player's zoom and
// shows the weapon's tool tip. Otherwise, unless the human is busy (in the air, at an ammo box or action node,
// reloading, throwing, in a melee move), the change is instant in vehicles, cutscenes and for instant equips, or
// plays the unequip and equip actions: the items swap at the equip action's impact trigger, and a non-inventory
// item (carried in the hands) moves towards the hands until then.
void human_do_equip_unequip(human* hp, bool)
{
    const bool was_equipping = hp->hflags.playing_equip_anim;
    const bool was_unequipping = hp->hflags.playing_unequip_anim;
    if (hp->get_equipped_inv_item() == hp->desired_equipped_inv_item)
    {
        if (was_equipping)
        {
            hp->hflags.playing_equip_anim = 0;
            if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                player* pp = hp->get_obj_type() == OT_HUMAN ? (player*)hp : nullptr;
                turret* tp;
                if (pp->zoom_state != PZS_SPRINT && pp->zoom_state != PZS_OFF && ((tp = human_combat_get_manned_turret(pp)) == nullptr || !tp->t_info->flags.force_fine_aim))
                {
                    const player_zoom_state zoom = pp->zoom_state;
                    pp->previous_zoom_state = zoom;
                    pp->zoom_state = PZS_OFF;
                    bool set_view = true;
                    if (zoom != PZS_OFF)
                    {
                        if (pp != Player)
                        {
                            set_view = false;
                        }
                        else
                        {
                            pp->get_equipped_weapon(); // Original: the result is not used.
                            if (pp->previous_zoom_state == PZS_SCOPE)
                            {
                                pp->set_opacity_plus_child_items(1.0f);
                                pp->set_casts_transparent_shadows_plus_child_items(false);
                                hud_ui_rail_driver_set_visible(false);
                                hud_ui_sniper_set_visible(false);
                            }
                        }
                    }
                    if (set_view && pp == Player)
                    {
                        camera_set_view_player(PZS_OFF, false);
                    }
                }
                // the tool tip of a newly equipped tank, thermobaric or multi-rocket launcher
                const inventory_item* iip = pp->get_equipped_inv_item();
                if (!Multiplayer && iip != nullptr && iip->info != nullptr)
                {
                    const unsigned int crc = iip->info->name_checksum.get_crc();
                    checksum_stri* tip = nullptr;
                    if (crc == PROPANE_TANK_CHECKSUM.get_crc() || crc == HYDROGEN_TANK_CHECKSUM.get_crc())
                    {
                        tip = &HYDROGEN_TANKS_TIP_NAME_CHECKSUM;
                    }
                    else if (crc == THERMO_ROCKET_CHECKSUM.get_crc())
                    {
                        tip = &THERMO_ROCKETS_TIP_NAME_CHECKSUM;
                    }
                    else if (crc == RPG_LAUNCHER_CHECKSUM.get_crc() && upgrade_get_multi_rockets() >= 2)
                    {
                        tip = &MULTI_ROCKETS_TIP_NAME_CHECKSUM;
                    }
                    if (tip != nullptr)
                    {
                        ui_tool_tips_add_tip(*tip, 0, nullptr);
                    }
                }
            }
        }
        if (was_unequipping)
        {
            hp->hflags.playing_unequip_anim = 0;
        }
        return;
    }
    if ((hp->movement_mode == HMM_JUMP || hp->movement_mode == HMM_FALL) && !hp->hflags.allow_flying_equips && (!Allow_player_flying_equips || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER))
    {
        return;
    }
    if (hp->cur_state == HS_AMMO_BOX || hp->cur_state == HS_ACTION_NODE)
    {
        return;
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_RELOAD))
    {
        if (hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
        {
            return;
        }
        if (hp->char_inst->is_playing_action(ANIM_ALIAS_RELOAD))
        {
            human_weapons_cancel_reload(hp);
        }
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_THROW) || hp->combat.cur_melee_combat_move != nullptr)
    {
        return;
    }
    const inventory_item* equipped = hp->get_equipped_inv_item();
    bool instant = human_in_vehicle(hp) != nullptr;
    if (!instant && hp->vehicle_handle != 0xFFFFFFFF)
    {
        // getting thrown out of a vehicle (not ejected) changes instantly too
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = (vehicle*)objp;
            const int idx = vp->vii.find_human(hp);
            if (idx >= 0)
            {
                const vi_interaction interaction = vp->vii.humans[idx].interaction;
                if (interaction > VI_INTERACTION_RESERVE && interaction < 4 && vp->vii.humans[idx].state != VI_STATE_EXIT_EJECT && interaction == VI_INTERACTION_EXIT)
                {
                    instant = true;
                }
            }
        }
    }
    if (instant || cutscene_is_playing() || (object_is_player(hp) && Player_instant_weapon_change) || hp->hflags.do_instant_equip)
    {
        if (was_equipping || was_unequipping || equipped != hp->desired_equipped_inv_item)
        {
            hp->hflags.playing_equip_anim = 0;
            hp->hflags.playing_unequip_anim = 0;
        }
        if (hp->hflags.do_instant_equip)
        {
            hp->hflags.do_instant_equip = 0;
            hp->set_equipped_inv_item(hp->desired_equipped_inv_item);
            if (hp->char_inst->vp->desired_state != nullptr)
            {
                hp->char_inst->vp->desired_state->blend = 1.0f;
            }
            if (hp->char_inst->vp->secondary_state != nullptr)
            {
                hp->char_inst->vp->secondary_state->blend = 0.0f;
            }
        }
        return;
    }
    const bool impact = vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_start, ANIM_ALIAS_EQUIP, -1);
    bool instant_unequip = false;
    if (object_is_player(hp) && Player_instant_unequip)
    {
        instant_unequip = !player_has_non_inventory(World.object_from_handle<player>(hp->get_handle()));
    }
    if (impact)
    {
        if (was_equipping)
        {
            hp->set_equipped_inv_item(hp->desired_equipped_inv_item);
        }
        else if (!instant_unequip && was_unequipping)
        {
            player* pp = World.object_from_handle<player>(hp->get_handle());
            if (player_has_non_inventory(pp))
            {
                player_remove_item_from_hands(pp);
                return;
            }
            hp->set_equipped_inv_item(nullptr);
        }
        hp->hflags.allow_flying_equips = 0;
    }
    else if (was_equipping && hp->desired_equipped_inv_item != nullptr)
    {
        weapon* wp = World.object_from_handle<weapon>(hp->desired_equipped_inv_item->weapon_handle);
        if (wp != nullptr && wp->w_info->flags.non_inventory)
        {
            // Original: the remaining frames are the trigger id minus the action's frame (a trigger time was meant).
            const int frames = Trigger_impact_start - vanim_get_anim_frame(hp->char_inst->vp, ANIM_ALIAS_EQUIP);
            if (frames > 0)
            {
                // Magic number: the carried object goes to 0.35 m in front of and above the human.
                const vector f = hp->orient.fvec.copy_flatten_normalize();
                const float n = (float)frames;
                wp->pos.x = (((f.x + UP_VECTOR.x) * 0.35f + hp->pos.x) - wp->pos.x) / n + wp->pos.x;
                wp->pos.y = ((hp->pos.y + (f.y + UP_VECTOR.y) * 0.35f) - wp->pos.y) / n + wp->pos.y;
                wp->pos.z = ((hp->pos.z + (f.z + UP_VECTOR.z) * 0.35f) - wp->pos.z) / n + wp->pos.z;
                vector fvec;
                fvec.x = (hp->orient.fvec.x - wp->orient.fvec.x) / n + wp->orient.fvec.x;
                fvec.y = (hp->orient.fvec.y - wp->orient.fvec.y) / n + wp->orient.fvec.y;
                fvec.z = (hp->orient.fvec.z - wp->orient.fvec.z) / n + wp->orient.fvec.z;
                wp->orient.make(fvec);
                havok_body_set_pos_orient(wp->havok_handle, wp->pos, wp->orient);
            }
        }
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_EQUIP) && !vanim_is_action_ending_this_frame(hp->char_inst->vp, ANIM_ALIAS_EQUIP))
    {
        return;
    }
    weapon* wp = nullptr;
    animation_action action = ANIMATION_ACTION_EQUIP;
    animation_group group = ANIMATION_GROUP_DEFAULT;
    bool play = false;
    if (hp->get_equipped_inv_item() == nullptr)
    {
        if (hp->desired_equipped_inv_item != nullptr && (wp = World.object_from_handle<weapon>(hp->desired_equipped_inv_item->weapon_handle)) != nullptr)
        {
            if (object_is_player(hp))
            {
                wp->precache_audio_cues();
            }
            action = wp->w_info->flags.non_inventory ? ANIMATION_ACTION_EQUIP : human_weapons_get_equip_animation(wp->w_info->weapon_class, hp);
            hp->hflags.playing_equip_anim = 1;
            hp->hflags.playing_unequip_anim = 0;
            group = wp->w_info->weapon_anim_group;
            play = true;
        }
    }
    else
    {
        wp = World.object_from_handle<weapon>(hp->get_equipped_inv_item()->weapon_handle);
        if (wp == nullptr)
        {
            return;
        }
        if (wp->can_unequip())
        {
            hp->hflags.playing_equip_anim = 0;
            hp->hflags.playing_unequip_anim = 1;
            if (instant_unequip)
            {
                hp->set_equipped_inv_item(nullptr);
            }
            else
            {
                // the moving unequip is two actions further
                action = wp->w_info->flags.non_inventory ? ANIMATION_ACTION_UNEQUIP : (animation_action)(ANIMATION_ACTION_UNEQUIP + (hp->cur_movement_state != HMS_IDLE ? 2 : 0));
                group = wp->w_info->weapon_anim_group;
                play = true;
            }
        }
    }
    if (play)
    {
        hp->char_inst->set_animation_group(group, index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        hp->char_inst->play_action(action, ANIM_ALIAS_EQUIP, 1.0f, 0x20);
        if (wp->rdd.is_loaded() && wp->char_inst != nullptr)
        {
            wp->char_inst->play_action(action, ANIM_ALIAS_EQUIP, 1.0f, 0x20);
        }
        if (wp->w_info->flags.non_inventory)
        {
            hp->block_movement_anim = ANIM_ALIAS_EQUIP;
            hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
            hp->block_steering_vector = ZERO_VECTOR;
        }
    }
    if (!object_is_player(hp))
    {
        return;
    }
    const float speed = ((player*)hp)->direct_control_vel.mag();
    vanim_set_blend_tree_init_control_input(hp->char_inst->vp, 0x409, speed / 5.0f);
    vanim_set_blend_tree_control_input(hp->char_inst->vp, 0x409, ((player*)hp)->direct_control_vel.mag() / 5.0f);
}

// 0x00A9A050 (:5882). LTCG: hp in EDI.
// Links an item into the human's inventory ring; a player remembers its first two weapons as the last equipped
// ones. The item's weapon object becomes the human's: its body is freed and it is hidden (a non-inventory item is
// taken in the hands instead); a remote weapon's network object goes away.
static void insert_item_into_inventory(human* hp, inventory_item* inv_item)
{
    if (hp->inventory == nullptr)
    {
        inv_item->next = inv_item;
        inv_item->prev = inv_item;
        hp->inventory = inv_item;
    }
    else
    {
        inv_item->prev = hp->inventory->prev;
        inv_item->next = hp->inventory;
        hp->inventory->prev->next = inv_item;
        hp->inventory->prev = inv_item;
    }
    int count = 0;
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        iip = iip->next;
        count++;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        if (count >= 2 && hp->last_equipped_weapon == nullptr)
        {
            hp->last_equipped_weapon = inv_item;
        }
        else if (count > 2 && hp->second_last_equipped_weapon == nullptr)
        {
            hp->second_last_equipped_weapon = inv_item;
        }
    }
    if (inv_item->weapon_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(inv_item->weapon_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return;
    }
    weapon* wp = (weapon*)objp;
    const weapon_info* wi = inv_item->info->w_info;
    if (wi == nullptr || !wi->flags.non_inventory)
    {
        wp->owner_handle = hp->get_handle();
        if (wp->havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(wp->havok_handle, false, true, false);
            wp->havok_handle = 0xFFFFFFFF;
        }
        object_set_visibility_plus_children(wp, false);
        if (wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            weapon_stop_pickup_effect(wp);
        }
    }
    else
    {
        wp->equip_non_inventory_item(hp);
    }
    if (Multiplayer && wp->remote_obj_flags.remote_object)
    {
        multi_core_object_buffer_destroy_remote_object(multi_core_network_handle_from_game_handle(wp->get_handle(), false));
    }
}

// 0x00A075E0 (:5922)
void human::refill_health(float percent)
{
    const float max = (float)max_hit_points;
    health_restore_hit_points = max * percent + health_restore_hit_points;
    if (max < health_restore_hit_points)
    {
        health_restore_hit_points = max;
    }
    hit_points = health_restore_hit_points;
    health_restore_min_hit_points = 0.0f;
}

// 0x00A07650 (:5935)
bool human::is_in_combat() const
{
    return combat.combat_ready_timestamp.valid() && !combat.combat_ready_timestamp.elapsed();
}

// 0x00A9A1A0 (:5950)
// Whether the human's shield item faces the source: the direction from the human's center to the source within
// MIN_HUMAN_IS_SHIELDED_DOT of the shield's facing (its up vector for a carried riot shield).
bool human::is_shielded(const vector& source_pos, bool physical_shield_only) const
{
    if (physical_shield_only && !hflags.riot_shield)
    {
        return false;
    }
    if (shield_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* shield = World.object_from_handle(shield_handle);
    if (shield == nullptr || shield->get_obj_type() != OT_ITEM)
    {
        return false;
    }
    const human_ragdoll_state rs = ragdoll_state;
    if (rs != HUMAN_RAGDOLL_STATE_BLEND_OUT && rs != HUMAN_RAGDOLL_STATE_NONE && rs != HUMAN_RAGDOLL_STATE_FLINCH &&
        (rs != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hflags.ragdoll_on_impact_use_ragdoll_pos))
    {
        return false;
    }
    vector head_pos;
    human_get_head_pos(this, head_pos);
    vector to_source;
    to_source.x = source_pos.x - (pos.x + head_pos.x) * 0.5f;
    to_source.y = source_pos.y - (pos.y + head_pos.y) * 0.5f;
    to_source.z = source_pos.z - (pos.z + head_pos.z) * 0.5f;
    const vector to_source_dir = to_source.copy_normalize_safe();
    // An item whose info carries the 0x2000 flag (the riot shield) faces along its up vector.
    const vector& facing = (((item*)shield)->info->flags & 0x2000) != 0 ? shield->orient.uvec : shield->orient.fvec;
    const vector facing_dir = facing.copy_normalize_safe();
    return MIN_HUMAN_IS_SHIELDED_DOT <= to_source_dir.y * facing_dir.y + facing_dir.x * to_source_dir.x + to_source_dir.z * facing_dir.z;
}

// 0x00AB14C0 (:5987)
// The riot shield (or another shield item) of a loaded human: carried on the back (neck) or in the free hand.
bool human::create_shield_item(const item_info* i_info, bool visible)
{
    // Magic number: 0x3000, the two shield flags of an item info.
    if (i_info == nullptr || (i_info->flags & 0x3000) == 0 || !rdd.is_loaded() || shield_handle != 0xFFFFFFFF)
    {
        return false;
    }
    // Original: the item flags are only partly initialized; the other bits are those of the i_info pointer, whose
    // stack slot the compiler reused (cast_shadows, cast_drop_shadows, simulate and no_physics are set).
    unsigned int raw_flags = ((unsigned int)(uintptr_t)i_info & 0xFFFC6006u) | 0x6006u;
    obj_item_props props(i_info, pos, orient, *(obj_item_flags*)&raw_flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* shield = World.create_object<item>(props, 0xFFFFFFFF);
    if (shield == nullptr)
    {
        // Original: "Unable to create riot shield item - World.create_object() failed." into the thread's debug
        // message buffer.
        return false;
    }
    shield_handle = shield->get_handle();
    const int child_prop_point = shield->get_tag_index_from_name("weapon_handle");
    if (child_prop_point < 0)
    {
        return false;
    }
    int parent_prop_point;
    // Magic number: flag bit 13, a shield worn on the back.
    if (((unsigned int)shield->info->flags >> 13 & 1) == 0)
    {
        const weapon_info* wi = human_weapons_get_weapon_info(this);
        if (wi == nullptr || wi->flags.draw_proj_in_opposite_hand)
        {
            return false;
        }
        parent_prop_point = wi->flags.held_in_left_hand ? equip_tag_index : equip_offhand_tag_index;
    }
    else
    {
        parent_prop_point = get_tag_index_from_name("neck");
    }
    if (parent_prop_point < 0)
    {
        return false;
    }
    set_child(shield, parent_prop_point, child_prop_point, false, true);
    shield->set_owner(get_handle(), true);
    if (!visible)
    {
        shield->set_is_visible(false);
    }
    if (shield->obj_flags.streamed)
    {
        init_shield(shield);
    }
    return true;
}

// 0x00A7B3E0 (:6048)
// A streamed shield: held, with a steel body that collides as a personal shield, floats (an upward force against
// gravity) and carries the steel effect material.
bool human::init_shield(item* const shield_inst)
{
    // function statics of the original, initialized on first use ($S5 bits 0 and 1)
    static unsigned char DEFAULT_SHIELD_PHYSICAL_MATERIAL_INDEX = rfg_get_physical_material_index("Steel-Normal");
    static unsigned char DEFAULT_SHIELD_EFFECT_MATERIAL_INDEX = rfg_get_effect_material_index(DEFAULT_SHIELD_PHYSICAL_MATERIAL_INDEX);
    if (!shield_inst->obj_flags.streamed)
    {
        return false;
    }
    shield_inst->item_flags.shield_initialized = 1;
    shield_inst->hold_item(this, true);
    if (shield_inst->havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(shield_inst);
    }
    if (shield_inst->havok_handle == 0xFFFFFFFF)
    {
        // Original: an Int3 debug message.
        return false;
    }
    hkpWorldObject* obj = (hkpWorldObject*)havok_body_get_pointer(shield_inst->havok_handle);
    havok_body_set_collision_type((hkpRigidBody*)obj, COLLIDE_PERSONAL_SHIELD, true, false);
    // Magic number: 9.8 m/s^2 up cancels the gravity.
    vector antigravity(0.0f, 9.8f, 0.0f);
    havok_body_add_custom_force((hkpRigidBody*)obj, antigravity);
    // Magic number: Havok property 4, the effect material.
    if (havok_has_property(obj, 4))
    {
        havok_remove_property(obj, 4);
    }
    havok_set_property(obj, 4, (void*)(uintptr_t)DEFAULT_SHIELD_EFFECT_MATERIAL_INDEX);
    return true;
}

// 0x00A2DEE0 (:6100). LTCG: hp in ESI; damage is not passed (unused).
// Damage delays the health regeneration: 2.5 s for npcs, 5.5 s scaled by the difficulty for players.
static void human_health_damage_register(human* hp, float)
{
    int delay_ms;
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        delay_ms = 2500;
    }
    else
    {
        const float scale = Multiplayer ? multi_game_rubberbanding_get_stat_value(((player*)hp)->mp_data, MRS_HEALTH_REGEN) : difficulty_get_value(DIFFICULTY_PLAYER_REGEN_DELAY, NUM_DIFFICULTY_LEVELS);
        delay_ms = (int)(scale * 5500.0f);
    }
    hp->health_restore_timestamp.set(delay_ms);
    hp->health_restore_min_hit_points = hp->hit_points;
}

// Deviation (human.h, not in rfg.exe): on a multiplayer client human_damage returns before human_health_damage_register,
// so a hit player's regeneration delay never starts there: human_restore_health then regenerates every frame and resets
// the blood decal fade index, and the player's blood decals fade at once (the server keeps them for the delay). The
// delay is started as on the server. The lowest hit points since the delay (the survivor experience check) are left
// as they were, so a client's regeneration end awards nothing new.
void human_health_damage_register_from_network(human* hp)
{
    const float min_hit_points = hp->health_restore_min_hit_points;
    human_health_damage_register(hp, 0.0f);
    hp->health_restore_min_hit_points = min_hit_points;
}

// 0x00A2DF50 (:6127). LTCG: hp in ESI.
// Health regeneration of players and squad members once the damage delay elapsed: 80 (players) or 5 hit points per
// second, scaled by the difficulty (slower while sprinting) or the multiplayer options, up to the hit points the
// regeneration started from. Multiplayer survivors of a near-death get the survivor experience.
static void human_restore_health(human* hp)
{
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    ai_system* ai = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? ((npc*)hp)->get_ai() : nullptr;
    if (pp == nullptr && (ai == nullptr || ai->is_in_player_squad(false) == nullptr))
    {
        return;
    }
    const bool restore = (hp->health_restore_timestamp.valid() && hp->health_restore_timestamp.elapsed()) ||
                         (!hp->health_restore_timestamp.valid() && hp->hit_points < (float)hp->max_hit_points && hp->hit_points > 0.0f);
    if (!restore)
    {
        return;
    }
    hp->hflags.used_death_buffer = 0;
    // Magic numbers: the regeneration rates (hit points per second).
    float amount = (pp == nullptr ? 5.0f : 80.0f) * Frametime;
    if (Multiplayer)
    {
        amount = (float)((double)(float)Mp_server_options.generic_options.health_regen_speed / 100.0 * (double)amount);
        if (multi_gameplay_mode_get() == MGPM_CAPTURE_THE_FLAG && player_has_flag(pp))
        {
            // Original: the second byte of the mode options, the flag carrier's regeneration percent.
            amount = (float)((double)(float)((const unsigned char*)&Mp_server_options.mode_options)[1] / 100.0 * (double)amount);
        }
        amount = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_HEALTH) * amount;
    }
    else if ((player*)hp == Player)
    {
        amount = difficulty_get_value(DIFFICULTY_PLAYER_REGEN_RATE, NUM_DIFFICULTY_LEVELS) * amount;
        if (pp->pflags.do_sprint)
        {
            amount = amount * 0.75f;
        }
        if (stats_cheat_enabled(CHEATS_SUPER_TOUGHNESS))
        {
            amount = Frametime * 80.0f * 5.0f;
        }
    }
    const float hit_points = hp->hit_points + amount;
    hp->hit_points = hit_points;
    if (hp->health_restore_hit_points <= hit_points)
    {
        hp->hit_points = hp->health_restore_hit_points;
        hp->health_restore_timestamp.invalidate();
        if (Multiplayer)
        {
            if (pp == nullptr)
            {
                return;
            }
            const float min_hit_points = pp->health_restore_min_hit_points;
            if (min_hit_points != 0.0f)
            {
                if (pp->combat.last_hit_by_handle != pp->get_handle() && min_hit_points / (float)pp->max_hit_points < multi_game_xp_get_event_parameter(MXE_SURVIVOR))
                {
                    multi_game_xp_trigger_event(pp->mp_data, MXE_SURVIVOR, 1, 0xFF, -1, false);
                }
                memset(pp->mp_player_damage_history, 0, sizeof(pp->mp_player_damage_history));
                pp->health_restore_min_hit_points = 0.0f;
            }
        }
    }
    if (pp != nullptr)
    {
        pp->blood_decals_fade_index = 0;
    }
}

// 0x00A07680 (:6215). LTCG: hp in ECX, head_pos in EAX.
// An estimate of the head position from the stance (1.5 m standing, 1.05 m crouched, near the ground and ahead
// when prone).
static void human_get_head_pos_from_stance(const human* hp, vector& head_pos)
{
    head_pos = hp->pos;
    switch (hp->cur_stance)
    {
    case HST_STAND:
        head_pos.y = head_pos.y + 1.5f;
        return;
    case HST_CROUCH:
        head_pos.y = head_pos.y + 1.05f;
        return;
    case HST_PRONE:
        head_pos.y = head_pos.y + 0.15f;
        head_pos.x = head_pos.x + hp->orient.fvec.x * 0.65f;
        head_pos.y = head_pos.y + hp->orient.fvec.y * 0.65f;
        head_pos.z = head_pos.z + hp->orient.fvec.z * 0.65f;
        return;
    default:
        // Original: an Int3 debug message.
        return;
    }
}

// 0x00A52ED0 (:6245)
// Fills the integration parameters of every loaded, living human (dead, knocked down, satellite and destroyed ones
// only update their shape mode): the desired velocity, the code driven jump, whether the proxy moves (distant still
// ambient npcs and npcs over 200 m do not; humans in vehicles follow the vehicle), the snap to the ground of a
// bashed controller, and whether support is checked and the velocity used as is. Returns the number filled.
static unsigned int human_process_controllers_pre_integrate(human_integration_parameters* human_integration_params, unsigned int max_humans, float time_step)
{
    unsigned int count = 0;
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        if (!hp->rdd.is_loaded())
        {
            continue;
        }
        if (hp == nullptr || hp->is_killed() || (hp->movement_mode == HMM_KNOCKDOWN && hp->movement_submode != HMSM_KNOCKDOWN_GETUP) || hp->movement_mode == HMM_SATELLITE ||
            hp->obj_flags.destroyed)
        {
            hp->rdd.get_data()->controller->update_collision_shape_mode(hp);
            continue;
        }
        if (count >= max_humans)
        {
            return count;
        }
        human_integration_parameters* params = &human_integration_params[count++];
        params->m_human = hp;
        params->m_desired_velocity.zero();
        params->m_integrate = false;
        params->m_refresh_manifold = false;
        params->m_check_fall = true;
        params->m_move_character_controller = true;
        params->m_ignore_y = false;
        params->m_use_velocity = false;
        params->m_check_support = false;
        params->m_start_jump_height = 0.0f;
        params->m_refresh_manifold = hp->hflags.just_got_up_from_ragdoll == 1;
        hp->hflags.just_got_up_from_ragdoll = 0;
        bool check_support = cutscene_is_playing();
        object* self = World.object_from_handle(hp->get_handle());
        const bool is_player = self != nullptr && self->get_sub_type() == OT_SUB_HUMAN_PLAYER;
        const bool is_local_player = Player != nullptr && hp->get_handle() == Player->get_handle();
        float start_jump_height;
        if (hp->code_driven_start_jump == true && hp->code_driven_jump_timer == 0.0f)
        {
            start_jump_height = hp->code_driven_jump_height;
            hp->code_driven_start_jump = false;
        }
        else
        {
            start_jump_height = 0.0f;
        }
        hp->code_driven_jump_timer = hp->code_driven_jump_timer - time_step;
        if (hp->code_driven_jump_timer < 0.0f)
        {
            hp->code_driven_jump_timer = 0.0f;
        }
        // Magic number: npc velocities under 1e-5 squared count as zero.
        if (!is_player && hp->velocity.x * hp->velocity.x + hp->velocity.y * hp->velocity.y + hp->velocity.z * hp->velocity.z < 1e-05f)
        {
            hp->velocity.zero();
        }
        bool bash_controller = false;
        if (hp->vehicle_handle == 0xFFFFFFFF)
        {
            if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && hp->cur_state != HS_LADDER)
            {
                npc* np = hp->get_obj_type() == OT_HUMAN ? (npc*)hp : nullptr;
                const float distance = hp->render_distance.get_apparent_distance(np->pos); // inlined
                // Magic numbers: ambient npcs on foot past 50 m, and still npcs past 100 m (or off screen), do not
                // move their proxies; past 200 m the proxy is placed directly.
                if (np->get_manager() == NPC_MANAGER_AMBIENT && np->vehicle_handle == 0xFFFFFFFF && !np->hflags.bones_transformed_this_frame && distance > 50.0f)
                {
                    params->m_move_character_controller = false;
                }
                if (np->velocity.x * np->velocity.x + np->velocity.y * np->velocity.y + np->velocity.z * np->velocity.z < 0.01f && np->cur_state != HS_FALL)
                {
                    if (!np->not_moving_update.elapsed())
                    {
                        np->not_moving_update.set(int_rand_range(1000, 2000));
                        if (distance > 100.0f || hp->is_offscreen())
                        {
                            params->m_move_character_controller = false;
                        }
                    }
                }
                if (distance > 200.0f)
                {
                    params->m_move_character_controller = false;
                    bash_controller = true;
                }
            }
        }
        else
        {
            object* objp = World.object_from_handle(hp->vehicle_handle);
            if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
            {
                params->m_move_character_controller = false;
                bash_controller = true;
            }
            else
            {
                vehicle* vp = (vehicle*)objp;
                const int idx = vp->vii.find_human(hp);
                if (vp->vii.humans[idx].pos_correction == VI_POS_CORRECTION_VEHICLE_BASH_POS)
                {
                    params->m_move_character_controller = false;
                }
                bash_controller = false;
                params->m_check_fall = false;
            }
        }
        if (params->m_move_character_controller || is_player)
        {
            if (hp->hflags.bashed_character_controller)
            {
                // the proxy was placed directly: put the human back on its path or the ground first
                hp->hflags.bashed_character_controller = 0;
                path_node* path = hp->pf.path;
                bool set_pos = true;
                if (path == nullptr)
                {
                    float ground_y;
                    havok_terrain_height(ground_y, hp->pos.x, hp->pos.z, false, nullptr);
                    if (ground_y <= hp->pos.y)
                    {
                        set_pos = false;
                    }
                    else
                    {
                        hp->pos.y = ground_y;
                    }
                }
                else if (path->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull)
                {
                    hp->pos = path->pos;
                }
                else
                {
                    havok_terrain_height(hp->pos.y, hp->pos.x, hp->pos.z, false, nullptr);
                }
                if (set_pos)
                {
                    hp->rdd.get_data()->controller->set_pos(hp->pos);
                }
            }
            if (hp->char_inst->vp->flags.use_velocity || hp->hflags.use_as_final_velocity || check_support || human_controller_is_on_ground(hp))
            {
                check_support = !cutscene_is_playing();
                // standing still on the terrain: no support check
                object* ground = World.object_from_handle(hp->ground_obj_handle);
                if (ground != nullptr && ground->get_obj_type() == OT_ZONE && start_jump_height == 0.0f && hp->velocity.y < 1.0f && hp->velocity.x == 0.0f && hp->velocity.z == 0.0f)
                {
                    check_support = false;
                }
                // entering a vehicle by pathfinding
                if (hp->vehicle_handle != 0xFFFFFFFF)
                {
                    object* objp = World.object_from_handle(hp->vehicle_handle);
                    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                    {
                        vehicle* vp = (vehicle*)objp;
                        const int idx = vp->vii.find_human(hp);
                        if (idx >= 0)
                        {
                            const vi_interaction interaction = vp->vii.humans[idx].interaction;
                            if (interaction > VI_INTERACTION_RESERVE && interaction < 4 && vp->vii.humans[idx].state != VI_STATE_EXIT_EJECT && interaction == VI_INTERACTION_ENTER &&
                                hp->movement_mode == HMM_PATHFINDING)
                            {
                                check_support = true;
                            }
                        }
                    }
                }
                params->m_integrate = true;
                params->m_desired_velocity = hp->velocity;
                params->m_start_jump_height = start_jump_height;
                params->m_check_support = check_support;
                params->m_use_velocity = hp->char_inst->vp->flags.use_velocity;
                params->m_ignore_y = hp->char_inst->vp->flags.ignore_y;
                hp->hflags.use_as_final_velocity = 0;
            }
            else
            {
                vector desired = hp->velocity;
                if (is_player)
                {
                    // in the air, a player's velocity scales with the stick deflection (the remote player's command
                    // on a server and on clients)
                    float v = Stick_v;
                    float h = Stick_h;
                    if (Multiplayer && hp->mp_flags.is_multiplayer && Multiplayer == true && ((multi_is_server() && !is_local_player) || (Multiplayer == true && !multi_is_server())))
                    {
                        v = ((player*)hp)->mp_current_command.forward;
                        h = ((player*)hp)->mp_current_command.sideways;
                    }
                    float s = fabsf(h) + fabsf(v);
                    if (s - 0.0f < 0.0f)
                    {
                        s = 0.0f;
                    }
                    if (s - 1.0f >= 0.0f)
                    {
                        s = 1.0f;
                    }
                    desired.x = hp->velocity.x * s;
                    desired.y = hp->velocity.y * s;
                    desired.z = hp->velocity.z * s;
                }
                params->m_desired_velocity = desired;
                params->m_start_jump_height = start_jump_height;
                params->m_integrate = true;
                params->m_ignore_y = false;
                params->m_use_velocity = false;
                params->m_check_support = true;
            }
        }
        else if (bash_controller)
        {
            // the proxy is placed directly where the velocity takes it (following a path's height)
            hp->hflags.bashed_character_controller = 1;
            path_node* path = hp->pf.path;
            vector new_pos;
            new_pos.x = hp->velocity.x * time_step + hp->pos.x;
            new_pos.y = hp->pos.y + hp->velocity.y * time_step;
            new_pos.z = hp->pos.z + hp->velocity.z * time_step;
            if (path != nullptr && path->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull)
            {
                vector2 d(path->pos.x - hp->pos.x, path->pos.z - hp->pos.z);
                const float dist = d.mag();
                if (dist > 0.1f)
                {
                    new_pos.y = (hp->velocity.mag() * time_step * (path->pos.y - hp->pos.y)) / dist + new_pos.y;
                }
            }
            hp->rdd.get_data()->controller->set_pos(new_pos);
        }
    }
    return count;
}

// 0x00AE4720 (:6810). LTCG: human_integration_params in ECX, num_humans on the stack.
// After the integration: landing (a falling human that is supported again plays its land), the air timer, the
// actual velocity, the support timer, ragdolls of humans sliding on a ragdoll check surface or falling too fast,
// the hit phantom, and the start of a fall when the support is gone for 200 ms (a short ray finds no ground).
static void human_process_controllers_post_integrate(human_integration_parameters* human_integration_params, unsigned int num_humans)
{
    for (unsigned int i = 0; i < num_humans; i++)
    {
        human_integration_parameters& params = human_integration_params[i];
        human* hp = params.m_human;
        hp->char_inst->vp->flags.use_velocity = 0;
        // Original: the rdd pointer is masked to null when not loaded and read anyway below; only loaded humans
        // are integrated.
        human::resource_dependent_data* rdd = hp->rdd.get_data();
        const bool supported = hp->rdd.is_loaded() ? hp->rdd.get_data()->controller->flags.is_supported : false;
        if (hp->hflags.is_falling || hp->hflags.in_air_melee)
        {
            if (supported)
            {
                hp->hflags.is_falling = 0;
                human_move_play_land(hp);
            }
            hp->falling_vel_mag = -hp->actual_velocity.y;
        }
        if (supported)
        {
            hp->air_time.set(0);
        }
        vector vel;
        if (hp->rdd.is_loaded())
        {
            hp->rdd.get_data()->controller->get_velocity(vel);
        }
        else
        {
            vel.zero();
        }
        hp->actual_velocity = vel;
        hp->actual_velocity_mag = sqrtf(vel.y * vel.y + vel.x * vel.x + vel.z * vel.z);
        // Magic numbers: the ground slopes (normal y 0.715 while moving, 0.7) that count as support.
        if ((!rdd->controller->flags.sliding || (vel.y - 0.0f < 0.01f && -(vel.y - 0.0f) < 0.01f && supported)) &&
            (((hp->ground_obj_normal >= 0.715f || hp->move_speed <= 0.0f) && hp->ground_obj_normal >= 0.7f) || hp->rdd.get_data()->controller->flags.check_ragdoll))
        {
            hp->last_supported.set(300);
        }
        // Magic numbers: a human that keeps sliding (under 0.1 m/s vertically) on a ragdoll check surface for 400 ms
        // while falling ragdolls.
        if (!rdd->controller->flags.check_ragdoll || !params.m_move_character_controller || vel.y - 0.0f >= 0.1f || -(vel.y - 0.0f) >= 0.1f || !hp->hflags.is_falling)
        {
            hp->do_ragdoll_ts.invalidate();
        }
        else if (!hp->do_ragdoll_ts.valid())
        {
            hp->do_ragdoll_ts.set(400);
        }
        else if (hp->do_ragdoll_ts.elapsed())
        {
            human_ragdoll_start(hp, false, 0xFFFFFFFF);
            hp->do_ragdoll_ts.invalidate();
        }
        if (rdd->hit_detect_phantom != nullptr)
        {
            hkVector4f p;
            game_to_havok_vector(p, hp->pos);
            ((hkpShapePhantom*)rdd->hit_detect_phantom)->setPosition(p, 0.0f);
        }
        // Magic number: sliding down faster than 12.5 m/s ragdolls.
        if (rdd->controller->flags.sliding && vel.y < -12.5f)
        {
            human_ragdoll_start(hp, false, 0xFFFFFFFF);
        }
        if (params.m_check_fall && params.m_move_character_controller && !supported)
        {
            if ((hp->air_time.time_since() > 200 || hp->cur_state == HS_JUMP || rdd->controller->flags.no_surface_ahead) && !hp->hflags.is_on_ladder && !hp->hflags.is_falling)
            {
                // Magic numbers: a ray from 0.5 m above the feet to 0.6 m below.
                raycast_input in;
                in.start.x = hp->pos.x;
                in.start.y = hp->pos.y + 0.5f;
                in.start.z = hp->pos.z;
                in.end.x = hp->pos.x;
                in.end.y = hp->pos.y - 0.6f;
                in.end.z = in.start.z;
                in.radius = 0.0f;
                in.filter_params = nullptr;
                in.filter_callback = nullptr;
                in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
                in.m_flags = 0;
                in.m_filter_info = COLLIDE_PLAYER;
                raycast_output out;
                if (!havok_raycast(in, out) && (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) && hp->vehicle_handle == 0xFFFFFFFF &&
                    !hp->char_inst->is_playing_action(399) && !hp->char_inst->is_playing_action(396) && !hp->char_inst->is_playing_action(ANIM_ALIAS_DIVE))
                {
                    // Magic numbers: actions 399 and 396 (the cover dives).
                    hp->hflags.is_falling = 1;
                    hp->falling_vel_mag = 0.0f;
                    if (hp->movement_mode != HMM_JUMP)
                    {
                        human_combat_cover_abandon_node(hp);
                        human_set_movement_mode(hp, HMM_FALL, HMSM_FALL_START);
                    }
                }
            }
        }
    }
}

// 0x00A2E310 (:6934). LTCG: hp in EAX, dbt in ECX.
// A tied-up npc dies from any bullet, explosion, fire or melee damage.
static bool should_kill_when_damaged(const human* const hp, damaged_by_types dbt)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        const npc* np = (const npc*)hp;
        if (np->npc_flags.tied_up && !np->npc_flags.freed_from_bonds && (int)dbt > 0 && ((int)dbt < 4 || dbt == DBT_MELEE))
        {
            return true;
        }
    }
    return false;
}

// 0x00A2E350 (:6960)
static void kill_light_effects(human* hp)
{
    for (int i = 0; i < 2; i++)
    {
        if (hp->light_effects[i] != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(hp->light_effects[i]);
            if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
            {
                ((object_effect*)objp)->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        hp->light_effects[i] = 0xFFFFFFFF;
    }
}

// 0x00A07790 (:6980)
void human::human_path_data_init()
{
    pf.path = nullptr;
    pf.last_visited_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    // Magic numbers: the nav cell is refreshed every 0.5 to 1 s.
    pf.last_visited_nav_cell_update_timer.set_rand(500, 1000);
    pf.dest_check_timer.invalidate();
    pf.path_repulsors_collection_time.invalidate();
    pf.repulsion_field_timer.invalidate();
    pf.hole_repulsion_field_timer.invalidate();
    pf.transition_node_to_avoid = 0xFFFFFFFF;
    pf.affected_by_repulsors = 0;
    pf.num_path_repulsors = 0;
    pf.num_problem_detours = 0;
}

// 0x00A537E0 (:7006)
// A living human takes the position of its proxy; its turn-to target moves with it.
void human_controller_synch(human* hp)
{
    if (hp->hit_points <= 0.0f)
    {
        return;
    }
    const vector old_target = hp->turnto_target;
    const vector old_pos = hp->pos;
    vector pos;
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->get_pos(pos);
    }
    else
    {
        pos.zero();
    }
    hp->update_pos_and_orient(pos, hp->orient, false);
    hp->turnto_target.x = (old_target.x - old_pos.x) + hp->pos.x;
    hp->turnto_target.y = hp->pos.y + (old_target.y - old_pos.y);
    hp->turnto_target.z = hp->pos.z + (old_target.z - old_pos.z);
    human_update_valid_position_before_ragdoll(hp);
}

// 0x00A2E3A0 (:7048)
bool human_controller_is_on_ground(human* hp)
{
    if (hp->rdd.is_loaded())
    {
        return hp->rdd.get_data()->controller->flags.is_supported;
    }
    return false;
}

// 0x00A2E400 (:7065): the character proxy's position (zero while the human is not loaded)
void human_controller_get_pos(const human* hp, vector& pos)
{
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->get_pos(pos);
        return;
    }
    pos.zero();
}

// 0x00A2E430 (:7076)
void human_controller_set_pos(human* hp, const vector& pos)
{
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->set_pos(pos);
    }
}

// 0x00A2E450 (:7087): the character proxy's velocity (zero while the human is not loaded).
void human_controller_get_vel(const human* hp, vector& vel)
{
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->get_velocity(vel);
        return;
    }
    vel.zero();
}

// 0x00A7B510 (:7099)
// A human in a vehicle keeps the vehicle's velocity.
void human_controller_set_vel(human* hp, const vector& vel)
{
    if (hp != nullptr)
    {
        vehicle* vp = nullptr;
        if (hp->vehicle_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(hp->vehicle_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                vp = (vehicle*)objp;
            }
        }
        if (human_in_vehicle(hp, vp) && vp != nullptr)
        {
            return;
        }
    }
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->set_velocity(vel);
    }
}

// 0x00A2E480 (:7122)
void human_controller_reset(human* hp)
{
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->set_shape(CSM_STAND, true);
        hp->rdd.get_data()->controller->set_velocity(ZERO_VECTOR);
    }
}

// 0x00A7B580 (:7133)
// The velocity a human asks its proxy for this frame: the animation's root motion (its position change, in world
// space, per second) plus the steering motion. The steering motion is the move speed along the steering vector (or
// the animation's own speed scaled by the speed throttle and the weapon's move multiplier), smoothed by the
// inertia (faster with a foot planted, by the turn speed when not combat ready). Running also drives the run/stand
// blend pose and the lean towards turns; the looping animations play at the speed throttle that matches the
// animation's speed to the motion. Several expressions are computed in double and rounded once, as the binary
// does.
void human_controller_process_pre(human* hp, bool)
{
    // Magic numbers: function statics of the original (.data 0x016AFCEC-0x016AFCF8).
    static float acceleration_out = 2.0f;
    static float acceleration_in = 10.0f;
    static float acceleration2 = 40.0f;
    static float acceleration1 = 20.0f;

    hp->actual_velocity_mag = 0.0f;
    if (vehicle_interaction_process_human_pre(hp))
    {
        // Magic numbers: the run/stand blend fades at 15 per second; the lean drops by 2 at once and then with time.
        float w = (float)((double)hp->run_stand_blend_pose_weight - (double)Frametime * 15.0);
        if ((float)(0.0 - (double)w) >= 0.0f)
        {
            w = 0.0f;
        }
        hp->run_stand_blend_pose_weight = w;
        float l = (float)((double)hp->lean_weight - 2.0 - (double)Frametime);
        if ((float)(0.0 - (double)l) >= 0.0f)
        {
            l = 0.0f;
        }
        hp->lean_weight = l;
        return;
    }
    const bool cutscene = cutscene_is_playing();
    if (!hp->hflags.use_current_velocity && !hp->hflags.use_as_final_velocity)
    {
        hp->velocity.zero();
    }
    if (cutscene)
    {
        return;
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE &&
        (hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos))
    {
        return;
    }
    bool is_local_player = false;
    if (Player != nullptr)
    {
        is_local_player = hp->get_handle() == Player->get_handle();
        if (is_local_player && hp->cur_state == HS_SATELLITE)
        {
            return;
        }
    }
    if (!hp->rdd.is_loaded() || hp->is_killed() || hp->movement_submode == HMSM_FALL_FREE)
    {
        return;
    }
    vector pos_change;
    vector motion_change;
    vector local_movement;
    animlib_get_position_change(pos_change, motion_change, local_movement, hp->char_inst->aim_handle);
    hp->char_inst->vp->local_anim_movement = local_movement;
    // Magic number: a vertical root motion of 0.1 mm or more makes the proxy use the velocity as is.
    const float dy = (float)((double)pos_change.y - 0.0);
    if ((double)0.0001f <= (double)dy || (double)0.0001f <= (double)-dy)
    {
        hp->char_inst->vp->flags.use_velocity = 1;
    }
    vector steer;
    if (hp->block_movement_anim == -1 || (hp->block_movement_type != BLOCK_MOVEMENT_POS_AND_ORIENT && hp->block_movement_type != BLOCK_MOVEMENT_POS))
    {
        const float speed = hp->move_speed;
        if (speed >= 0.0f)
        {
            steer = hp->steering_vector * speed;
        }
        else
        {
            // a negative move speed follows the animation's speed
            const float mag = motion_change.mag();
            steer = hp->steering_vector * mag / Frametime / hp->state_speed_throttle;
            weapon* wp = hp->get_equipped_weapon();
            if (wp != nullptr)
            {
                const float multiplier = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? wp->w_info->player_move_speed_multiplier : wp->w_info->npc_move_speed_multiplier;
                steer = steer * multiplier;
            }
        }
    }
    else
    {
        steer.zero();
    }
    v_animate* vp = hp->char_inst->vp;
    float throttle = 1.0f;
    if (vp->default_group_index == ANIMATION_GROUP_DEFAULT && vp->desired_state != nullptr &&
        (vp->desired_state->index == ANIMATION_STATE_WALK || vp->desired_state->index == ANIMATION_STATE_WALK_YELLOW_ALERT))
    {
        throttle = hp->walk_animation_speed_pct;
    }
    bool throttle_by_speed = true;
    if (!is_local_player)
    {
        // npcs and remote players: the steering motion has inertia
        float accel = hp->foot_planted == FP_NEITHER_FOOT ? acceleration1 : acceleration2;
        if (!human_is_combat_ready(hp))
        {
            accel = hp->spinebend_data.turn_speed * accel;
        }
        float x = steer.x;
        float cur = hp->inertial_motion.x;
        if (steer.x > cur)
        {
            const float v = (float)((double)Frametime * (double)accel + (double)cur);
            if ((float)((double)steer.x - (double)v) >= 0.0f)
            {
                x = v;
            }
        }
        else
        {
            const float v = (float)((double)cur - (double)Frametime * (double)accel);
            if ((float)((double)steer.x - (double)v) < 0.0f)
            {
                x = v;
            }
        }
        hp->inertial_motion.x = x;
        float z = steer.z;
        cur = hp->inertial_motion.z;
        if (steer.z > cur)
        {
            const float v = (float)((double)Frametime * (double)accel + (double)cur);
            if ((float)((double)steer.z - (double)v) >= 0.0f)
            {
                z = v;
            }
        }
        else
        {
            const float v = (float)((double)cur - (double)Frametime * (double)accel);
            if ((float)((double)steer.z - (double)v) < 0.0f)
            {
                z = v;
            }
        }
        hp->inertial_motion.z = z;
        hp->inertial_motion.y = steer.y;
        if (hp->steering_vector.is_null() || (hp->cur_movement_state != HMS_RUN && hp->cur_movement_state != HMS_SPRINT))
        {
            float w = (float)((double)hp->run_stand_blend_pose_weight - (double)Frametime * 15.0);
            if ((float)(0.0 - (double)w) >= 0.0f)
            {
                w = 0.0f;
            }
            hp->run_stand_blend_pose_weight = w;
            float l = (float)((double)hp->lean_weight - 2.0 - (double)Frametime);
            if ((float)(0.0 - (double)l) >= 0.0f)
            {
                l = 0.0f;
            }
            hp->lean_weight = l;
        }
        else
        {
            // running: the run/stand pose blends in as the motion falls behind the animation's speed
            const float motion = (float)((long double)hp->inertial_motion.mag() * throttle);
            float anim_speed = 0.0f;
            v_animate* va = hp->char_inst->vp;
            if (va->desired_state != nullptr)
            {
                const v_anim* state = va->desired_state->variant != nullptr && va->desired_state->variant->is_variant_fullbody ? va->desired_state->variant : va->desired_state;
                anim_speed = vanim_get_anim_velocity(va, state->instance);
            }
            va = hp->char_inst->vp;
            if (va->secondary_state != nullptr)
            {
                const float secondary_speed = vanim_get_anim_velocity(va, va->secondary_state->instance);
                va = hp->char_inst->vp;
                anim_speed = (float)((long double)va->secondary_state->blend * secondary_speed + (long double)va->desired_state->blend * anim_speed);
            }
            // Magic numbers: 4 % of slack, doubled into the 0..1 blend weight.
            const float ratio = (float)((double)motion / (double)anim_speed);
            const float slack = (float)(1.0 - (double)ratio - (double)0.04f);
            float target = (float)((double)slack * 2.0);
            if ((float)((double)target - 0.0) < 0.0f)
            {
                target = 0.0f;
            }
            if ((float)((double)target - 1.0) >= 0.0f)
            {
                target = 1.0f;
            }
            const float cur_w = hp->run_stand_blend_pose_weight;
            if ((double)target > (double)cur_w)
            {
                const float v = (float)((double)Frametime * 15.0 + (double)cur_w);
                hp->run_stand_blend_pose_weight = (float)((double)v - (double)target) < 0.0f ? v : target;
            }
            else if ((double)cur_w > (double)target)
            {
                const float v = (float)((double)cur_w - (double)Frametime * 15.0);
                hp->run_stand_blend_pose_weight = (float)((double)target - (double)v) < 0.0f ? v : target;
            }
            // the lean grows with the motion's lag behind the steering motion; turning sharply disables it for 1.5 s
            const float steer_mag = steer.mag();
            const float lag = steer_mag > 0.0f ? (float)((double)motion / (double)steer_mag) : 0.0f;
            float lean_target = (float)((1.0 - (double)lag - (double)0.04f) * 2.0);
            const vector turn = hp->lean_last_steering_vector - hp->steering_vector;
            const float turn_sq = turn.mag_squared();
            bool disable_lean = false;
            if (turn_sq > 0.01f)
            {
                if (hp->lean_disabled.elapsed())
                {
                    hp->lean_disabled.set(1500);
                    hp->hflags.leaning_disabled = 0;
                    hp->spinebend_data.turn_speed = 1.0f;
                }
                else
                {
                    disable_lean = true;
                }
            }
            else if (hp->hflags.leaning_disabled)
            {
                disable_lean = true;
            }
            else
            {
                float t = (float)((double)hp->spinebend_data.turn_speed * (double)1.15f);
                if ((float)(1.0 - (double)t) < 0.0f)
                {
                    t = 1.0f;
                }
                hp->spinebend_data.turn_speed = t;
            }
            if (disable_lean)
            {
                // Magic numbers: a turn of more than 0.4 (squared) slows the turn speed by a quarter, to 0.35 at
                // least; smaller turns speed it up again.
                hp->hflags.leaning_disabled = 1;
                lean_target = 0.0f;
                hp->lean_disabled.set(1500);
                if ((double)turn_sq > (double)0.4f)
                {
                    float t = (float)((double)hp->spinebend_data.turn_speed * 0.75);
                    if ((float)((double)0.35f - (double)t) >= 0.0f)
                    {
                        t = 0.35f;
                    }
                    hp->spinebend_data.turn_speed = t;
                }
                else
                {
                    float t = (float)((double)hp->spinebend_data.turn_speed * (double)1.15f);
                    if ((float)(1.0 - (double)t) < 0.0f)
                    {
                        t = 1.0f;
                    }
                    hp->spinebend_data.turn_speed = t;
                }
            }
            hp->lean_last_steering_vector = hp->steering_vector;
            const float cur_l = hp->lean_weight;
            if ((double)lean_target > (double)cur_l)
            {
                const float v = (float)((double)acceleration_in * (double)Frametime + (double)cur_l);
                hp->lean_weight = (float)((double)lean_target - (double)v) < 0.0f ? lean_target : v;
            }
            else if ((double)cur_l > (double)lean_target)
            {
                const float v = (float)((double)cur_l - (double)acceleration_out * (double)Frametime);
                hp->lean_weight = (float)((double)lean_target - (double)v) < 0.0f ? v : lean_target;
            }
        }
        steer = hp->inertial_motion;
    }
    if (vanim_check_editor_flag(hp->char_inst->vp, 0x100))
    {
        // the animation's own motion replaces the steering, in the human's frame
        steer = motion_change / Frametime;
        steer.unrotate(hp->orient);
        throttle_by_speed = false;
    }
    character_instance* ci = hp->char_inst;
    const v_anim* desired = ci->vp->desired_state;
    if (desired != nullptr && desired->is_blend_tree)
    {
        for (int i = 0; i < desired->blend_tree_info->num_actions; i++)
        {
            if (desired->blend_tree_instance.actions[i].current_blend > 0.0f && !desired->blend_tree_info->actions[i].speed_throttling)
            {
                throttle_by_speed = false;
                break;
            }
        }
    }
    if (hp->cur_state != HS_JUMP && hp->cur_state != HS_FALL && (hp->move_speed >= 0.0f || (double)hp->run_stand_blend_pose_weight >= (double)0.2f) && throttle_by_speed)
    {
        // the looping animations play at the speed that matches the motion, ramped (up to 1.5 times)
        const float motion = (float)((long double)steer.mag() * throttle);
        if ((double)motion > (double)0.0001f)
        {
            const float anim_speed = (float)((long double)motion_change.mag() / Frametime + (long double)(double)0.0001f);
            float ratio = (float)((double)motion / (double)anim_speed);
            if (ratio > 3.0f)
            {
                ratio = 1.0f;
            }
            if ((double)ratio > (double)hp->state_speed_throttle)
            {
                const float t = (float)((double)hp->state_speed_throttle + (double)Frametime);
                hp->state_speed_throttle = t;
                if (1.0f > t)
                {
                    hp->state_speed_throttle = (float)((double)t + (double)Frametime);
                }
            }
            if ((double)hp->state_speed_throttle > (double)ratio)
            {
                float t = (float)((double)hp->state_speed_throttle - (double)Frametime * (double)1.4f);
                hp->state_speed_throttle = t;
                if (1.0f > t)
                {
                    t = (float)((double)t - (double)Frametime * (double)1.4f);
                    hp->state_speed_throttle = t;
                    if ((double)t * 2.0 > (double)ratio)
                    {
                        hp->state_speed_throttle = (float)((double)t - (double)Frametime);
                    }
                }
            }
            throttle = hp->state_speed_throttle;
            if ((float)((double)throttle - (double)0.1f) < 0.0f)
            {
                throttle = 0.1f;
            }
            if ((float)((double)throttle - 1.5) >= 0.0f)
            {
                throttle = MAX_SIMULATE_RAMP_SPEED_FACTOR;
            }
        }
    }
    hp->state_speed_throttle = throttle;
    animlib_set_looping_speed_multiplier(ci->aim_handle, throttle);
    if (hp->hflags.use_current_velocity)
    {
        steer = hp->velocity + steer;
    }
    pos_change.unrotate(hp->orient);
    hp->hflags.use_current_velocity = 0;
    hp->velocity = pos_change / Frametime + steer;
}

// 0x00AE4BD0 (:7452)
// The character proxies of all humans step together (up to 256).
void human_process_controllers(float time_step)
{
    human_integration_parameters params[256];
    if (time_step != 0.0f)
    {
        const unsigned int num_humans = human_process_controllers_pre_integrate(params, 256, time_step);
        human_process_controllers_integrate_mt(params, num_humans, time_step);
        human_process_controllers_post_integrate(params, num_humans);
    }
}

// 0x00A2E4B0 (:7556)
void human::destroy_notify()
{
    remove_constraints();
    npc_despawned ev;
    ev.npc_handle = get_handle();
    gem_post_event(ev);
    if (vehicle_handle != 0xFFFFFFFF)
    {
        vehicle_interaction_remove(this);
    }
    if (voice_instance != -1)
    {
        audiolib_play_stop(voice_instance);
        if (audiolib_play_get_state(-1, nullptr) != AUDIOLIB_ERR_NONE)
        {
            voice_time_since_finish.set(0);
        }
        voice_instance = -1;
    }
}

// 0x00A07840 (:7582)
// A streamed human is off screen when a 1.25 m sphere around its chest (0.85 m up) is culled.
bool human::is_offscreen() const
{
    if (!obj_flags.streamed)
    {
        return false;
    }
    const vector center(pos.x, pos.y + 0.85f, pos.z);
    return game_render_cull_sphere(center, 1.25f);
}

// 0x00A078A0 (:7601)
bool human::is_transformed() const
{
    return hflags.bones_transformed_this_frame != 0;
}

// 0x00A7C540 (:7706)
// Puts the human on the static level under it (a ray from 1 m above to 4 m below).
bool human_stick_to_ground(human* hp)
{
    timer_get(TIMER_MICROSECONDS); // Original: the result is not used (a profiling leftover).
    raycast_input in;
    in.start.x = hp->pos.x;
    in.start.y = hp->pos.y + 1.0f;
    in.start.z = hp->pos.z;
    in.end.x = hp->pos.x;
    in.end.y = hp->pos.y - 4.0f;
    in.end.z = in.start.z;
    in.radius = 0.0f;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    const bool hit = havok_raycast_against_static_level_only(in, out);
    if (hit)
    {
        hp->update_pos_and_orient(out.m_hits[0].hit_pos, hp->orient, true);
    }
    return hit;
}

// 0x00A7C660 (:7737)
// Moves the human (or its vehicle) without a placement check: it lands from a jump start (unless it is an NPC alive in
// cover or a ragdoll), its path is dropped outside path following, and its attached objects follow. An invalid orient
// keeps the current one; an invalid position does nothing. A living human leaves its action node.
void human_teleport_unsafe(human* hp, vector pos, matrix orient)
{
    if (!pos.is_valid())
    {
        return;
    }
    if (!orient.is_valid())
    {
        orient = hp->orient;
    }
    if (hp->vehicle_handle == 0xFFFFFFFF)
    {
        hp->update_pos_and_orient(pos, orient, true);
        human_controller_set_vel(hp, ZERO_VECTOR);
        hp->move_speed = -1.0f;
        hp->cur_state = HS_JUMP;
        hp->cur_movement_state = HMS_IDLE;
        // Original: cur_state was just set to HS_JUMP, so the cover test is always false.
        if ((hp->get_sub_type() != OT_SUB_HUMAN_NPC || hp->is_killed() || hp->cur_state != HS_COVER) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
            hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            if (hp->movement_mode != HMM_JUMP)
            {
                hp->previous_movement_mode = hp->movement_mode;
                hp->movement_mode = HMM_JUMP;
            }
            const human_move_modes mode = hp->movement_mode;
            hp->movement_submode = HMSM_JUMP_START;
            if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
            {
                hp->pf.dest_check_timer.invalidate();
                if (hp->pf.path != nullptr)
                {
                    bb_pf_free_path(&hp->pf.path);
                }
            }
        }
        for (object* child = hp->child_ptr; child != nullptr;)
        {
            child->update_pos_and_orient(hp->pos, hp->orient, false);
            if (child->child_ptr != nullptr)
            {
                child->adjust_child_transforms(false, true);
            }
            child = child->child_next;
            if (child == hp->child_ptr)
            {
                break;
            }
        }
    }
    else
    {
        object* vp = World.object_from_handle(hp->vehicle_handle);
        if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
        {
            vp = nullptr;
        }
        // Original: no null check.
        vp->update_pos_and_orient(pos, orient, true);
    }
    if (!hp->is_killed() && hp->action_node_data.current_action_node != 0xFFFFFFFF)
    {
        object* node = World.object_from_handle(hp->action_node_data.current_action_node);
        if (node != nullptr && node->get_obj_type() == OT_ACTION_NODE)
        {
            hp->release_action_node(node->get_handle());
        }
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        // Not recovered: ai_planner::lock_action(false) on the NPC's planner (the headless tiers have no NPCs).
    }
}

// 0x00A078B0
// HS_CURRENT and HMS_CURRENT keep the current state.
void human_set_state(human* hp, human_states state, human_movement_states movement_state, float move_speed)
{
    if (state == HS_CURRENT)
    {
        state = hp->cur_state;
    }
    if (movement_state == HMS_CURRENT)
    {
        movement_state = hp->cur_movement_state;
    }
    hp->move_speed = move_speed;
    hp->cur_state = state;
    hp->cur_movement_state = movement_state;
}

// 0x00A9A7A0 (:7773)
// A checked teleport: out of a vehicle, then to a place to stand within placement_range of the position (single
// player and the multiplayer server); without one the position itself unless allow_fail. A multiplayer client moves
// there directly.
void human_teleport(human* hp, vector& pos, matrix& orient, float placement_range, bool allow_fail)
{
    if (!pos.is_valid())
    {
        return;
    }
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        exit_data.flags.maintain_position = 1;
        vehicle_interaction_request_exit(hp, &exit_data);
    }
    if (!Multiplayer || multi_is_server())
    {
        vector destination;
        if (human_find_place_to_stand(destination, hp, placement_range, &pos))
        {
            human_teleport_unsafe(hp, destination, orient);
            return;
        }
        if (allow_fail)
        {
            return;
        }
    }
    human_teleport_unsafe(hp, pos, orient);
}

// 0x00A078F0 (:7881)
void human_clear_bored_idle(human* hp)
{
    hp->char_inst->stop_action(ANIM_ALIAS_IDLE, 3, false);
}

// 0x00A52EA0
bool human::is_armed(bool ranged_only) const
{
    if (ranged_only)
    {
        return inv_item_get_best_firearm(this) != nullptr;
    }
    return inv_item_get_best_weapon(this, false) != nullptr;
}

// 0x00A07930 (:7914)
// Changes the movement mode (not for npcs in cover outside the cover submodes, nor stopping one on a ladder). A
// fall that starts on a path with a jump, drop or leap is a fall from a building; a free fall keeps the velocity.
// Leaving the pathfinding modes frees the path.
void human_set_movement_mode(human* hp, human_move_modes move_mode, human_move_submodes move_submode)
{
    const bool cover_npc = hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER;
    const bool cover_submode = (int)move_submode > HMSM_PTV_FINAL_ADJUSTMENT && (int)move_submode < NUM_HUMAN_MOVE_SUBMODES;
    if ((cover_npc && !cover_submode) || (hp->hflags.is_on_ladder && hp->get_sub_type() == OT_SUB_HUMAN_NPC && move_mode == HMM_NONE))
    {
        return;
    }
    if (move_mode == HMM_FALL)
    {
        if (move_submode == HMSM_FALL_START)
        {
            path_node* first = hp->pf.path;
            if (first != nullptr && !hp->hflags.jumping_from_building)
            {
                for (path_node* node = first;;)
                {
                    if (node->flags.jumping || node->flags.dropping_down || node->flags.leap_over)
                    {
                        hp->hflags.jumping_from_building = 1;
                        break;
                    }
                    node = node->next;
                    if (node == first || node == nullptr)
                    {
                        break;
                    }
                }
            }
        }
        else if (move_submode == HMSM_FALL_FREE)
        {
            hp->steering_vector = hp->actual_velocity.copy_flatten_normalize_safe();
            hp->hflags.use_current_velocity = 1;
            hp->hflags.use_as_final_velocity = 1;
        }
    }
    if ((hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT) && move_mode != HMM_KNOCKDOWN)
    {
        return;
    }
    if (hp->movement_mode != move_mode)
    {
        if (move_mode == HMM_PATHFINDING || move_mode == HMM_PATHFINDING_TO_VEHICLE)
        {
            hp->pf.dest_check_timer.set(0);
            hp->pf.last_dest_check_pos = hp->pos;
        }
        if (move_mode == HMM_NONE)
        {
            hp->steering_vector.zero();
        }
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = move_mode;
    }
    const human_move_modes mode = hp->movement_mode;
    hp->movement_submode = move_submode;
    if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
    {
        hp->pf.dest_check_timer.invalidate();
        if (hp->pf.path != nullptr)
        {
            bb_pf_free_path(&hp->pf.path);
        }
    }
}

// 0x00A07AD0 (:8026)
bool human_is_in_movement_mode(human* hp, human_move_modes mode, human_move_submodes submode)
{
    return mode == hp->movement_mode && (submode == HMSM_NONE || submode == hp->movement_submode);
}

// 0x00A53920 (:8077)
// Plans a path to a position and starts following it in the given (or current) state.
bool human_pathfind_to(human* hp, const vector& pos, human_states state, human_movement_states m_state, uint64_t end_nav_cell_hint, bool out_of_chunk)
{
    hp->hflags.last_dest_in_repulsor = 0;
    human_move_path_set(hp, hp->pos, pos, end_nav_cell_hint, out_of_chunk);
    hp->pf.eventual_goal_pos.x = pos.x;
    hp->pf.eventual_goal_pos.y = pos.y;
    if (hp->pf.path == nullptr)
    {
        hp->hflags.last_pf_failed = 1;
        hp->pf.eventual_goal_pos.z = pos.z;
        return false;
    }
    hp->pf.eventual_goal_pos.z = pos.z;
    if (state == HS_CURRENT)
    {
        state = hp->cur_state;
    }
    if (m_state == HMS_CURRENT)
    {
        m_state = hp->cur_movement_state;
    }
    hp->move_speed = -1.0f;
    hp->cur_state = state;
    hp->cur_movement_state = m_state;
    if (!(hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
        hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        // human_set_movement_mode(hp, HMM_PATHFINDING, HMSM_NONE), inlined
        if (hp->movement_mode != HMM_PATHFINDING)
        {
            hp->pf.dest_check_timer.set(0);
            hp->pf.last_dest_check_pos = hp->pos;
            hp->previous_movement_mode = hp->movement_mode;
            hp->movement_mode = HMM_PATHFINDING;
        }
        const human_move_modes mode = hp->movement_mode;
        hp->movement_submode = HMSM_NONE;
        if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
        {
            hp->pf.dest_check_timer.invalidate();
            if (hp->pf.path != nullptr)
            {
                bb_pf_free_path(&hp->pf.path);
            }
        }
    }
    hp->hflags.last_pf_failed = 0;
    return true;
}

// 0x00AE5D40 (:8129)
// One frame of a human: the frame time tally, death (the dead state, the player's death, gibbing, dropping the
// weapon of a ragdolled npc), the idle facial poses (picked by the district morale every 20-30 s, blended in and
// out over 0.5 s), movement (humans riding in vehicles do not move themselves), turret operator animations,
// equipping, and the animation pass.
void human_do_frame(human* hp, bool do_animation)
{
    if (hp->obj_flags.destroyed)
    {
        return;
    }
    hp->hflags.health_change_was_negative = 0;
    hp->last_pos = hp->pos;
    hp->equipped_inv_item_last_frame = hp->get_equipped_inv_item();
    const float frametime = cutscene_is_playing() ? Cutscene_timer.get_frame_time() : Frametime;
    hp->frametime_tally = frametime + hp->frametime_tally;
    if (hp->hit_points <= 0.0f)
    {
        if (hp->cur_state == HS_DEAD || hp->cur_state == HS_SPECTATOR)
        {
            // Magic number: the dead stay in the render update for 5 s.
            if (timer_get(TIMER_MILLISECONDS) - hp->combat.last_killed_time_ms < 5001)
            {
                hp->set_render_flags(8);
            }
        }
        else
        {
            human_switch_to_dead_state(hp);
            if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                player_is_killed((player*)hp);
            }
            else if (human_is_ragdolled(hp) && !havok_ragdoll_is_active(hp->ragdoll_idx))
            {
                human_ragdoll_stop(hp, true);
            }
        }
        // Magic number: an npc ragdolled for 350 ms drops its weapon.
        if (hp->desired_equipped_inv_item != nullptr && hp->ragdoll_idx != -1 && hp->get_sub_type() != OT_SUB_HUMAN_PLAYER && havok_ragdoll_get_time(hp->ragdoll_idx) > 350)
        {
            human_weapons_drop_shield(hp);
            human_weapons_drop_equipped_weapon(hp);
        }
        if (hp->hflags.was_gibbed)
        {
            World.destroy_object(hp, false);
        }
    }
    if (hp->facial_pose_ts.valid() && !hp->hflags.lod_head_loaded)
    {
        v_animate* vp = nullptr;
        float blend = 0.0f;
        bool set_pose = true;
        if (hp->facial_pose_ts.elapsed())
        {
            const int since = hp->facial_pose_ts.time_since();
            if (since > 500)
            {
                // pick a new facial pose for the morale
                vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIM_ALIAS_FACIAL_POSE, ANIM_ALIAS_NONE), 0.0f, 0);
                const float effective_morale = gfm_get_effective_morale_percent();
                float morale = effective_morale;
                if (!human_is_combat_ready(hp) && hp->look_at_handle == Player->get_handle())
                {
                    const float control = gfm_get_edf_control_percent(nullptr);
                    morale = (1.0f - control) * 0.5f + effective_morale;
                    if (1.0f - morale < 0.0f)
                    {
                        morale = 1.0f;
                    }
                }
                int candidates[30];
                int num_candidates = 0;
                for (int i = 0; i < Num_facial_anims; i++)
                {
                    if (Facial_anims[i].min_morale <= morale && morale < Facial_anims[i].max_morale)
                    {
                        candidates[num_candidates++] = i;
                    }
                }
                if (num_candidates - 1 >= 0)
                {
                    const int pick = int_rand_range(0, num_candidates - 1);
                    vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(Facial_anims[candidates[pick]].anim, ANIM_ALIAS_FACIAL_POSE), 0.05f, 0);
                }
                // Magic numbers: a new pose every 20 to 30 s.
                hp->facial_pose_ts.set(int_rand_range(20000, 30000));
                set_pose = false;
            }
            else
            {
                vp = hp->char_inst->vp;
                blend = 1.0f - (float)since / 500.0f;
            }
        }
        else
        {
            v_anim* pose = vanim_get_action(hp->char_inst->vp, ANIM_ALIAS_FACIAL_POSE);
            if (pose == nullptr)
            {
                set_pose = false;
            }
            else
            {
                blend = Frametime * 2.0f + pose->blend;
                if (1.0f - blend < 0.0f)
                {
                    blend = 1.0f;
                }
                vp = hp->char_inst->vp;
            }
        }
        if (set_pose)
        {
            vanim_set_state_blend_pose(vp, index_alias(ANIM_ALIAS_FACIAL_POSE, ANIM_ALIAS_NONE), blend, 0);
        }
    }
    vehicle* vp = nullptr;
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vp = (vehicle*)objp;
        }
    }
    if (!human_in_vehicle(hp, vp) || vp == nullptr)
    {
        if (hp->vehicle_handle != 0xFFFFFFFF)
        {
            World.object_from_handle(hp->vehicle_handle); // Original: the result is not used.
        }
        if (!hp->is_killed() || hp->movement_mode == HMM_KNOCKDOWN)
        {
            if (hp->ragdoll_idx >= 0 && hp->movement_mode != HMM_KNOCKDOWN && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
            {
                human_ragdoll_stop(hp, false);
            }
            human_move_process(hp);
        }
    }
    else
    {
        object* self = World.object_from_handle(hp->get_handle());
        if (((self != nullptr && self->get_sub_type() == OT_SUB_HUMAN_PLAYER) || (hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed())) &&
            hp->combat.combat_ready_timestamp.valid() && hp->combat.combat_ready_timestamp.elapsed())
        {
            human_in_vehicle_interaction(hp, nullptr); // Original: the result is not used.
            hp->combat.combat_ready_timestamp.invalidate();
        }
    }
    if (hp->action_node_data.action_node_set.size() > 0)
    {
        const unsigned int node = hp->action_node_data.action_node_set[0];
        const unsigned int current = hp->action_node_data.current_action_node;
        if (node != 0xFFFFFFFF && (current == 0xFFFFFFFF || current == node))
        {
            object* objp = World.object_from_handle(node);
            if (objp != nullptr && objp->get_obj_type() == OT_ACTION_NODE)
            {
                ((object_action_node*)objp)->disable_node_collision(hp);
            }
        }
    }
    if (human_combat_get_manned_turret(hp) == nullptr)
    {
        if (hp->turret_hold_animations.elapsed())
        {
            // clear the turret operator poses
            v_animate* va = hp->char_inst->vp;
            const animation_group group = va->default_group_index;
            va->default_group_index = ANIMATION_GROUP_TURRET_MACHINEGUN;
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_WALK_LEFT, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_LEFT, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_SLIGHT_LEFT, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_MIDDLE, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_SLIGHT_RIGHT, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_RIGHT, ANIM_ALIAS_NONE), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_WALK_RIGHT, ANIM_ALIAS_NONE), 0.0f, 0);
            hp->char_inst->vp->default_group_index = group;
            hp->turret_hold_animations.invalidate();
        }
    }
    else if (hp->vehicle_handle != 0xFFFFFFFF || hp->cur_state == HS_TURRET)
    {
        human_get_turret_state_anim(hp);
    }
    if (!hp->is_killed() && (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT))
    {
        human_do_equip_unequip(hp, human_in_vehicle(hp) == nullptr);
    }
    if (do_animation)
    {
        human_animation_processing(hp);
    }
}

// 0x00AB1660 (:8432)
// The per-frame work that does not depend on the animation: morphs, fades (with the vehicle, or timed fade in/out;
// a nano fade out destroys an npc), the breath effect at night (every 3.5-4.5 s), health regeneration, the light
// effects of the human info (on at night), and the broadphase boxes of the carried weapons and of the human.
void human_do_frame_no_animation(human* hp)
{
    if (hp->obj_flags.destroyed)
    {
        return;
    }
    human_hap_process_morphs(hp);
    if ((hp->hflags.fading_in || hp->hflags.fading_out || hp->hflags.fading_out_from_nano || hp->fade_with_vehicle() || hp->camera_alpha_override != 1.0f) &&
        hp->get_obj_type() != OT_NAVPOINT && hp->get_obj_type() != OT_COVER_NODE)
    {
        // queue_render_update(hp, flags, 4), inlined
        queue_render_update(hp, hp->obj_flags, 4); // queue_render_update, inlined
    }
    {
        object* vobj = hp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(hp->vehicle_handle) : nullptr;
        if (vobj != nullptr && vobj->get_obj_type() == OT_VEHICLE && (((vehicle*)vobj)->m_flags.m_fading_in || ((vehicle*)vobj)->m_flags.m_fading_out))
        {
            object* objp = hp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(hp->vehicle_handle) : nullptr;
            if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
            {
                hp->render_alpha = 1.0f;
            }
            else
            {
                hp->render_alpha = ((vehicle*)objp)->get_vehicle_opacity();
            }
            hp->sync_to_vehicle_opacity();
        }
        else if (hp->hflags.fading_in)
        {
            const int since = hp->fade_timer.time_since();
            if (since < hp->fade_time)
            {
                hp->render_alpha = (float)since / (float)hp->fade_time;
            }
            else
            {
                hp->render_alpha = 1.0f;
                hp->hflags.fading_in = 0;
            }
        }
        else if (hp->hflags.fading_out)
        {
            const int since = hp->fade_timer.time_since();
            if (hp->fade_time <= since)
            {
                hp->hflags.fading_out = 0;
                hp->render_alpha = 0.0f;
                if (hp->hflags.fading_out_from_nano && !hp->obj_flags.destroyed && !object_is_player(hp))
                {
                    World.destroy_object(hp, false);
                }
            }
            else if (!hp->hflags.fading_out_from_nano)
            {
                hp->render_alpha = 1.0f - (float)since / (float)hp->fade_time;
            }
        }
    }
    if (human_should_breath_effect_play(hp))
    {
        // Magic numbers: a breath every 3.5 to 4.5 s.
        hp->breath_timer.set_rand(3500, 4500);
        if (hp->hflags.was_rendered)
        {
            vector pos;
            matrix orient;
            const int head = hp->h_info->head_bone_index;
            if (head < 0)
            {
                pos = hp->pos;
                orient = hp->orient;
            }
            else
            {
                hp->get_tag_world_coords(head, pos, &orient, nullptr, nullptr);
            }
            effects_play(breath_effect, pos, orient, false, hp->get_handle(), hp->h_info->head_bone_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
    }
    if (!hp->is_killed())
    {
        human_restore_health(hp);
        if (Multiplayer && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            human_maybe_play_falling_voice(hp);
        }
        if (hp->h_info->light_effect_handle != 0xFFFFFFFF)
        {
            const game_clock_time_of_day tod = game_clock_get_time_of_day();
            bool lit = false;
            for (int i = 0; i < 2; i++)
            {
                if (hp->light_effects[i] != 0xFFFFFFFF)
                {
                    lit = true;
                    break;
                }
            }
            if (lit != (tod == GC_NIGHT))
            {
                if (tod == GC_NIGHT)
                {
                    for (int i = 0; i < 2; i++)
                    {
                        if (hp->light_tags[i] != -1)
                        {
                            hp->light_effects[i] = effects_play(hp->h_info->light_effect_handle, hp->get_handle(), hp->light_tags[i], true, 0xFFFFFFFF, 1.0f, false);
                        }
                    }
                }
                else
                {
                    kill_light_effects(hp);
                }
            }
        }
    }
    // the carried weapons go with the human (a held weapon is placed by its attachment), with their children
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->weapon_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(iip->weapon_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
            {
                item* ip = (item*)objp;
                if (ip->is_owned() && !ip->item_flags.item_held)
                {
                    ip->pos = hp->pos;
                }
                if (ip->get_bpo_handle() != nullptr)
                {
                    ip->get_bbox(ip->last_known_bmin, ip->last_known_bmax);
                    havok_bpo_update(ip->get_bpo_handle(), ip->last_known_bmin, ip->last_known_bmax);
                }
                for (object* child = ip->child_ptr; child != nullptr;)
                {
                    if (child->get_bpo_handle() != nullptr)
                    {
                        child->get_bbox(child->last_known_bmin, child->last_known_bmax);
                        havok_bpo_update(child->get_bpo_handle(), child->last_known_bmin, child->last_known_bmax);
                    }
                    child = child->child_next;
                    if (child == ip->child_ptr)
                    {
                        break;
                    }
                }
            }
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    if (hp->get_bpo_handle() != nullptr)
    {
        hp->get_bbox(hp->last_known_bmin, hp->last_known_bmax);
        havok_bpo_update(hp->get_bpo_handle(), hp->last_known_bmin, hp->last_known_bmax);
    }
}

// The approximate local box of a transformed human: the bone (or exact) box grown by a third around its center.
static inline void human_set_approx_local_bbox(human* hp, const vector& bmin, const vector& bmax)
{
    const float hz = (bmax.z - bmin.z) * 0.5f;
    const float hx = (bmax.x - bmin.x) * 0.5f;
    const float hy = (bmax.y - bmin.y) * 0.5f;
    vector center(hx + bmin.x, hy + bmin.y, hz + bmin.z);
    // Magic number: 1.33, the growth of the half extents.
    const float ex = hx * 1.33f;
    const float ey = hy * 1.33f;
    const float ez = hz * 1.33f;
    hp->approx_local_bmax.x = center.x + ex;
    hp->approx_local_bmax.y = center.y + ey;
    hp->approx_local_bmax.z = center.z + ez;
    hp->approx_local_bmin.x = center.x - ex;
    hp->approx_local_bmin.y = center.y - ey;
    hp->approx_local_bmin.z = center.z - ez;
}

// 0x00ADA0E0 (:8559)
// After the bone transforms: morphs and the approximate box, the ragdoll blends and flinches, spine bending after
// the transform, the head's bones, footsteps and prop triggers, melee hits and the next frame's controller input.
void human_process_post_transform(human* hp)
{
    if (!hp->rdd.is_loaded())
    {
        return;
    }
    vector bmin;
    vector bmax;
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        if (hp->hflags.bones_transformed_this_frame)
        {
            animlib_blend_all_morphs(hp->char_inst->aim_handle);
            animlib_get_bone_bbox(hp->char_inst->aim_handle, bmin, bmax);
            human_set_approx_local_bbox(hp, bmin, bmax);
        }
    }
    else
    {
        switch (hp->ragdoll_state)
        {
        case HUMAN_RAGDOLL_STATE_ACTIVE:
        case HUMAN_RAGDOLL_STATE_DEACTIVATED:
        case HUMAN_RAGDOLL_STATE_POSE_SAVED:
            break;
        case HUMAN_RAGDOLL_STATE_BLEND_OUT:
        case HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT:
            if (hp->hflags.bones_transformed_this_frame)
            {
                animlib_blend_all_morphs(hp->char_inst->aim_handle);
                hp->char_inst->get_exact_local_bbox(bmin, bmax);
                human_set_approx_local_bbox(hp, bmin, bmax);
            }
            if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
            {
                havok_blend_anim_to_ragdoll(hp->ragdoll_idx, hp->pos, hp->orient, 1.0f - hp->ragdoll_blend_pct);
            }
            break;
        case HUMAN_RAGDOLL_STATE_FLINCH:
            if (hp->hflags.bones_transformed_this_frame)
            {
                animlib_blend_all_morphs(hp->char_inst->aim_handle);
                hp->char_inst->get_exact_local_bbox(bmin, bmax);
                human_set_approx_local_bbox(hp, bmin, bmax);
            }
            if (!havok_ragdoll_is_added_to_world(hp->ragdoll_idx))
            {
                havok_ragdoll_add_to_world(hp->ragdoll_idx);
                havok_sync_ragdoll_to_anim(hp->ragdoll_idx, hp->pos, hp->orient, hp->actual_velocity, 0.0f, 1.0f, false, false, false);
                hp->ragdoll_last_frame_velocity = hp->actual_velocity;
            }
            if (!havok_ragdoll_increment_flinches(hp->ragdoll_idx))
            {
                havok_ragdoll_unbind_from_human(hp->ragdoll_idx);
                hp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
                hp->ragdoll_idx = -1;
                hp->ragdoll_source_human_handle = 0xFFFFFFFF;
            }
            break;
        default:
            // Original: an Int3 debug message.
            break;
        }
    }
    human_spinebend_post_transform(hp);
    if (hp->hflags.bones_transformed_this_frame && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        // the separate head's bones
        human::resource_dependent_data* rdd = hp->rdd.get_data();
        if (rdd->head_manager_idx >= 0 && animlib_allocate_bones(rdd->head_aim_handle))
        {
            matrix43 tmp_matrices[64];
            animlib_transform_all_bones_pre(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
            animlib_transform_all_bones_post(rdd->head_aim_handle, tmp_matrices, nullptr, 0);
        }
    }
    if ((!Multiplayer || Multi_match_info.match_state == MGMS_GAMEPLAY) &&
        (hp->ragdoll_idx == -1 || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT))
    {
        human_do_footsteps(hp);
        vanim_check_prop_handles(hp->char_inst->vp, hp);
    }
    multi_game_weapons_reset_melee_hit(hp);
    if (hp->rdd.is_loaded() && hp->rdd.get_data()->controller != nullptr)
    {
        if (!hp->is_killed() && (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT))
        {
            human_combat_process_melee(hp);
            human_controller_process_pre(hp, false);
        }
        if (hp->hflags.in_air_melee && human_controller_is_on_ground(hp))
        {
            hp->hflags.in_air_melee = 0;
        }
    }
    multi_game_weapons_process_melee_hit(hp);
}

// 0x00AE7860 (:8697)
void human_process_all()
{
    npc_process_all();
    human_fire_do_frame();
}

// 0x00AB5200 (:8717)
// The animation-independent work of the npcs processed this frame.
void human_process_all_no_animation()
{
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<npc>(i);
        if (hp->hflags.processed_this_frame)
        {
            human_do_frame_no_animation(hp);
        }
    }
}

// 0x00A07B60 (:8722)
// The queued bone transforms of all humans run (animlib_batch_process_all_start, inlined in the binary).
void human_process_all_animations_start()
{
    animlib_batch_process_all_start();
}

// 0x00A07B70 (:8727)
void human_process_all_animations_end()
{
    animlib_batch_process_all_end();
}

// 0x00ADA8E0 (:8732)
void human_process_all_post_transform()
{
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        npc* np = (npc*)World.get_object<human>(i);
        if (!np->obj_flags.destroyed && np->hflags.processed_this_frame)
        {
            human_process_post_transform(np);
            if (np->get_sub_type() == OT_SUB_HUMAN_NPC)
            {
                npc_process_post_transform(np);
            }
        }
    }
}

// 0x00ADA950 (:8745)
// After the simulation, for every human: the x-ray material and alpha the local player sees (by disposition; the
// dead fade over 5 s), ragdolls of humans standing on fast or spinning vehicles (or on a walker), the animation's
// post-simulation pass and the position taken from the proxy, the cloth, and the last visited nav cell.
void human_process_all_postsim()
{
    // Magic number: function static of the original (.data 0x016AFE6C).
    static int Human_xray_on_death_fade_time_ms = 5000;
    player* local = Player;
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        player* hp = (player*)World.get_object<human>(i);
        if (hp->obj_flags.destroyed)
        {
            continue;
        }
        if (local != nullptr && local != hp)
        {
            const unsigned char old_material = hp->xray_material;
            hp->xray_material = xray_effect_get_xray_material((xray_teams)((int)human_teams_get_disposition_internal(local->get_team(), hp->get_team()) < TEAM_DISPOSITION_UNFRIENDLY));
            const unsigned char old_alpha = hp->xray_render_alpha;
            if (hp->is_killed())
            {
                const int since = timer_get(TIMER_MILLISECONDS) - hp->combat.last_killed_time_ms;
                // Magic numbers: the alpha fades from 255 to 30 after death.
                if (since < 0 || Human_xray_on_death_fade_time_ms < since)
                {
                    hp->xray_render_alpha = 30;
                }
                else
                {
                    hp->xray_render_alpha = (unsigned char)(int)((double)(float)(Human_xray_on_death_fade_time_ms - since) * 225.0 / (double)Human_xray_on_death_fade_time_ms + 30.0);
                }
            }
            else
            {
                hp->xray_render_alpha = 0xFF;
            }
            bool changed;
            if (hp->xray_render_alpha == old_alpha && hp->xray_material == old_material)
            {
                changed = false;
            }
            else
            {
                changed = true;
                if (hp->get_obj_type() != OT_NAVPOINT && hp->get_obj_type() != OT_COVER_NODE)
                {
                    // queue_render_update(hp, flags, 0x10), inlined
                    queue_render_update(hp, hp->obj_flags, 0x10 >> 1); // queue_render_update, inlined Original: the 4-bit field at bit 1 gets bit 4 of the byte
                }
            }
            xray_my_children(hp, hp->hflags.xray_visible, hp->xray_material, hp->xray_render_alpha, changed);
        }
        if (hp->hflags.processed_this_frame)
        {
            const unsigned int ground = hp->ground_obj_handle;
            const human_ragdoll_state rs = hp->ragdoll_state;
            if (ground != 0xFFFFFFFF &&
                (rs == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_FLINCH ||
                 (rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos)))
            {
                object* objp = World.object_from_handle(ground);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                {
                    vehicle* vp = (vehicle*)objp;
                    unsigned int attacker;
                    if (activity_register_damage_is_player_controlled(vp))
                    {
                        attacker = local->get_handle();
                    }
                    else
                    {
                        attacker = vp->most_recent_driver != local->get_handle() ? vp->most_recent_driver : 0xFFFFFFFF;
                    }
                    bool ragdoll = false;
                    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER && local != hp)
                    {
                        ragdoll = true;
                    }
                    else
                    {
                        const unsigned int body = vp->get_alt_hk_body_handle(hp->ground_obj_alt_body_idx);
                        if (body != 0xFFFFFFFF && !human_in_vehicle_interaction(hp, nullptr) && hp->ragdoll_idx < 0)
                        {
                            vector lin;
                            vector ang;
                            havok_body_get_linear_velocity(body, lin);
                            havok_body_get_angular_velocity(body, ang);
                            // the smaller of this frame's and the last frame's speeds
                            const float lin_sq = lin.mag_squared();
                            const float last_lin_sq = vp->last_velocity.mag_squared();
                            const float speed_sq = lin_sq - last_lin_sq >= 0.0f ? last_lin_sq : lin_sq;
                            const float ang_sq = ang.mag_squared();
                            const float last_ang_sq = vp->last_angular_velocity.mag_squared();
                            const float spin_sq = ang_sq - last_ang_sq >= 0.0f ? last_ang_sq : ang_sq;
                            if (Human_ragdoll_on_vehicle_speed * Human_ragdoll_on_vehicle_speed < speed_sq || Human_ragdoll_on_vehicle_rotation * Human_ragdoll_on_vehicle_rotation < spin_sq)
                            {
                                ragdoll = true;
                            }
                            else if (vp->havok_handle == body && Human_ragdoll_on_wheels_speed < fabsf(vp->forward_velocity) && Human_ragdoll_on_wheels_speed < fabsf(vp->last_forward_velocity))
                            {
                                // Original: the filter layer of the first vehicle body stood on is cached once ($S6 bit 0).
                                static int filter_layer = (int)(vehicle_get_collision_filter(vp, hp->ground_obj_alt_body_idx, hp->ground_obj_shape_key) & 0xFFFF);
                                // Magic number: layer 47, the wheels.
                                if (filter_layer == 0x2F)
                                {
                                    ragdoll = true;
                                }
                            }
                        }
                    }
                    if (ragdoll)
                    {
                        human_ragdoll_start(hp, false, attacker);
                    }
                }
            }
            if (hp->char_inst->rdd.is_loaded())
            {
                human_animation_process_postsim(hp);
                vehicle* vp = nullptr;
                if (hp->vehicle_handle != 0xFFFFFFFF)
                {
                    object* objp = World.object_from_handle(hp->vehicle_handle);
                    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                    {
                        vp = (vehicle*)objp;
                    }
                }
                if ((!human_in_vehicle(hp, vp) || vp == nullptr) && !hp->is_killed())
                {
                    human_controller_synch(hp);
                }
            }
            hp->frametime_tally = 0.0f;
        }
        if (hp->cloth_sim != nullptr)
        {
            matrix43* model_matrices;
            if (animlib_get_instanced_model_matrices(hp->char_inst->aim_handle, &model_matrices))
            {
                hp->cloth_sim->pre_simulate(hp, model_matrices, model_matrices, &hp->pos, &hp->orient);
                if (hp->get_obj_type() != OT_NAVPOINT && hp->get_obj_type() != OT_COVER_NODE)
                {
                    queue_render_update(hp, hp->obj_flags, 8 >> 1); // queue_render_update, inlined
                }
            }
            else
            {
                hp->cloth_sim->initialize_nodes = 1;
            }
        }
        if (!Multiplayer && !hp->obj_flags.destroyed && !hp->is_killed())
        {
            if (human_in_vehicle(hp) == nullptr)
            {
                bb_pf_update_last_visited_nav_cell(hp);
            }
            else
            {
                hp->pf.last_visited_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                hp->pf.last_visited_nav_cell_update_timer.invalidate();
            }
        }
    }
}

// 0x00A9A890 (:8913)
void human_do_ik_process()
{
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->hflags.bones_transformed_this_frame)
        {
            human_ik_process(hp);
        }
    }
}

// 0x00AC7690 (:8935)
// Starts a jump: not while ragdolled (an impact ragdoll only without its ragdoll position) or in a vehicle interaction
// or a blocking animation; a falling player only readies its jetpack; on a ladder the ladder jump. A player jumps at
// most every 1.1 s and leaves combat readiness (aiming too with a launcher, charge, mine, hammer or singularity bomb).
// The code driven jump height (the backpack's speed factor, the super sprint cheats); from a building, over an obstacle
// or dropping down an action plays, else the jump takes off from standing or a ladder. The jump id counts up (the
// multiplayer action id), the jump foley and the take-off effect of the ground material play.
void human_jump(human* hp, bool from_building, bool leap_over, bool dropping_down)
{
    if (hp == nullptr)
    {
        return;
    }
    const human_ragdoll_state ragdoll_state = hp->ragdoll_state;
    if (ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (ragdoll_state != HUMAN_RAGDOLL_STATE_NONE && ragdoll_state != HUMAN_RAGDOLL_STATE_FLINCH &&
        (ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos))
    {
        return;
    }
    if ((hp->hflags.is_falling && !hp->hflags.is_on_ladder) || hp->movement_mode == HMM_JUMP)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            ((player*)hp)->pflags.jetpack_ready = 1;
        }
        return;
    }
    if (human_in_vehicle_interaction(hp, nullptr) || hp->block_movement_anim != -1)
    {
        return;
    }
    if (hp->hflags.is_on_ladder && !human_ladder_jump(hp))
    {
        return;
    }
    if (object_is_player(hp))
    {
        player* pp = (player*)hp;
        if (!pp->jump_refresh_timestamp.elapsed())
        {
            return;
        }
        // Magic number: 1.1 s between jumps.
        pp->jump_refresh_timestamp.set(1100);
        weapon* wp = hp->get_equipped_weapon();
        if (wp != nullptr)
        {
            switch (wp->w_info->weapon_class)
            {
            case WEAPON_CLASS_LAUNCHER:
            case WEAPON_CLASS_THROWN_CHARGE:
            case WEAPON_CLASS_THROWN_MINE:
            case WEAPON_CLASS_SLEDGEHAMMER:
            case WEAPON_CLASS_GUTTER:
            case WEAPON_CLASS_SINGULARITY_BOMB:
                human_in_vehicle_interaction(hp, nullptr); // Original: the result is not used.
                hp->combat.combat_ready_timestamp.invalidate();
                hp->combat.combat_aim_timestamp.invalidate();
                hp->combat.combat_ready_timestamp.invalidate();
                break;
            default:
                if (!human_is_combat_aiming(hp))
                {
                    hp->combat.combat_ready_timestamp.invalidate();
                }
                break;
            }
        }
    }
    if (!hp->code_driven_start_jump)
    {
        // Magic number: the jump starts within 0.1 s.
        hp->code_driven_jump_timer = 0.1f;
        hp->code_driven_start_jump = true;
        hp->code_driven_jump_height = Default_jump_height;
        if (from_building || leap_over)
        {
            hp->code_driven_start_jump = false;
        }
        player* pp = World.object_cast<player>(hp);
        if (pp != nullptr)
        {
            if (pp->backpack != nullptr)
            {
                pp->code_driven_jump_height = pp->backpack->get_backpack_abilities()->movement_speed_factor * pp->code_driven_jump_height;
            }
            if (stats_cheat_enabled(CHEATS_SUPER_JETPACK) && stats_cheat_enabled(CHEATS_SUPER_SPRINTING))
            {
                hp->code_driven_jump_height = hp->cur_movement_state == HMS_SPRINT ? Super_sprint_run_jump : Super_sprint_normal_jump;
            }
        }
    }
    if (!from_building && !leap_over && !dropping_down)
    {
        hp->hflags.jumping_from_building = 0;
        if (hp->cur_state != HS_STANDARD && hp->cur_state != HS_LADDER)
        {
            return;
        }
        hp->jump_state_timer.set(0);
        hp->hflags.jump_take_off = 1;
    }
    else
    {
        animation_action action = ANIMATION_ACTION_JUMP_FROM_BUILDING;
        if (hp->cur_movement_state == HMS_WALK || dropping_down)
        {
            action = ANIMATION_ACTION_JUMP_DOWN;
        }
        if (leap_over)
        {
            action = ANIMATION_ACTION_LEAP_OVER_2;
        }
        // Magic number: the action's play flags (0x4020).
        hp->char_inst->play_action(action, ANIM_ALIAS_SPECIAL_JUMP, 1.0f, 0x4020);
        hp->hflags.jumping_from_building = 1;
    }
    if ((hp->get_sub_type() != OT_SUB_HUMAN_NPC || hp->is_killed() || hp->cur_state != HS_COVER) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
        hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        human_set_movement_mode(hp, HMM_JUMP, HMSM_JUMP_PRE); // inlined
    }
    hp->mp_flags.jump_id = (hp->mp_flags.jump_id + 1) & 3;
    const int foley = hp->h_info->foot_effects->jump_foley;
    if (foley != -1)
    {
        gameaudio_play_cross_linked_sound(foley, hp->get_handle(), nullptr, nullptr);
    }
    unsigned char material;
    material_shape_type shape_type;
    if (human_get_standing_on_material(hp, material, shape_type) && material != 0xFF)
    {
        const unsigned int effect = hp->h_info->foot_effects->get_jump_effect_handle(material, shape_type, JUMP_TAKEOFF);
        effects_play(effect, hp->pos, hp->orient, false, 0xFFFFFFFF, -1, hp->get_handle(), 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00A2E5C0 (:9104): freezes the swing (speed 0) and plays the impact reaction; human_combat_process_melee ends it.
void human_melee_cancel_attack(human* hp)
{
    if (hp->combat.cur_melee_combat_move == nullptr)
    {
        return;
    }
    const unsigned int handle = vanim_get_anim_instance_handle(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE);
    animlib_set_speed_multiplier(hp->char_inst->aim_handle, handle, 0.0f);
    hp->hflags.cancelling_melee_attack = 1;
    // Magic number: 0x2000, the action flag of the reaction.
    hp->char_inst->play_action(ANIMATION_ACTION_IMPACT_FROM_FRONT, ANIM_ALIAS_NONE, 1.0f, 0x2000);
}

// 0x00AB1B50 (:9123)
// A melee attack of the equipped weapon (or the manned turret's): not while ragdolled, throwing, equipping or holding
// an object; it cancels a reload and the fire action and leaves cover (unless the cover attack applies) and the zoom.
bool human_melee_attack(human* hp, human_melee_attack_types attack_type, bool from_server, bool force_running)
{
    // human::get_equipped_weapon, inlined.
    weapon* wp = hp->get_equipped_weapon();
    if (wp == nullptr || turret_from_item(wp) != nullptr)
    {
        return false;
    }
    bool result = true;
    const bool cover_attack = Melee_cover_attack_enabled && hp != nullptr && hp == Player && hp->cur_state == HS_COVER && human_weapons_is_melee_weapon_equipped(hp) &&
                              !wp->w_info->flags.disable_player_cover;
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if ((hp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) || !gameplay_can_player_attack(wp->w_info, DBT_MELEE))
    {
        return false;
    }
    if (human_weapons_is_reloading(hp, false))
    {
        human_weapons_cancel_reload(hp);
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_FIRE))
    {
        hp->char_inst->stop_action(ANIM_ALIAS_FIRE, 0, true);
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_EQUIP) || hp->char_inst->is_playing_action(ANIM_ALIAS_THROW))
    {
        return false;
    }
    bool left_cover = false;
    if (pp != nullptr)
    {
        if (pp->hold_object_handle != 0xFFFFFFFF)
        {
            return false;
        }
        if (Player_hold_throw_timestamp.valid() && !Player_hold_throw_timestamp.elapsed())
        {
            return false;
        }
        if (pp->cur_state == HS_COVER && !cover_attack)
        {
            left_cover = true;
            player_exit_cover(pp, false);
        }
    }
    human_clear_bored_idle(hp);
    if (human_weapons_is_melee_weapon_equipped(hp) || attack_type == MELEE_ATTACK_FIREARM)
    {
        human_melee_attack_types attack = attack_type;
        // A thrown charge weapon without its charge in the other hand swings the secondary attack.
        if (wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            const item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
            if (charge == nullptr || !charge->obj_flags._dont_use_me)
            {
                attack = MELEE_ATTACK_SECONDARY;
            }
        }
        result = human_combat_attack_melee_armed(hp, attack, from_server, force_running);
        if (!result)
        {
            goto zoom;
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/rfg/code/objects/humans/human.cpp", 9201);
    }
    if (Multiplayer)
    {
        // The swing counters that tell remote machines a new attack started (two bits each, counting up).
        human_mp_flags& mp = hp->mp_flags;
        switch (attack_type)
        {
        case MELEE_ATTACK_PRIMARY:
            mp.melee_primary_id++;
            break;
        case MELEE_ATTACK_SECONDARY:
            mp.melee_secondary_id++;
            break;
        case MELEE_ATTACK_TERTIARY:
            mp.melee_tertiary_id++;
            break;
        case MELEE_ATTACK_FIREARM:
            mp.melee_firearm_id++;
            break;
        default:
            break;
        }
    }
    if (pp != nullptr)
    {
        gameplay_register_player_attack(pp, wp->w_info, DBT_MELEE, false);
    }
    if (!hp->hflags.in_air_melee)
    {
        hp->block_movement_type = object_is_player(hp) ? BLOCK_MOVEMENT_POS_AND_ORIENT : BLOCK_MOVEMENT_POS;
    }
    if (left_cover)
    {
        if (!Multiplayer || object_is_local_player(hp))
        {
            hp->block_steering_vector = Rfg_camera.ideal_orient.fvec.copy_flatten_normalize();
        }
        else
        {
            // Original: pp is non-null here (only a player leaves cover).
            hp->block_steering_vector = pp->mp_camera_orient.fvec.copy_flatten_normalize();
        }
    }
zoom:
    if (pp != nullptr && pp->zoom_state != PZS_OFF)
    {
        const turret* manned = human_combat_get_manned_turret(pp);
        if (manned == nullptr || !manned->t_info->flags.force_fine_aim)
        {
            const player_zoom_state previous = pp->zoom_state;
            pp->previous_zoom_state = previous;
            pp->zoom_state = PZS_OFF;
            if (previous != PZS_OFF)
            {
                if (pp != Player)
                {
                    return result;
                }
                pp->get_equipped_weapon();
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
        }
    }
    return result;
}

// 0x00ADAED0 (:9252)
// Replaces the inventory by the human info's default one (the golden hammer instead of the sledgehammer for a
// player whose profile unlocked it and wants it); Wrecking Crew and multiplayer have their own.
void human_set_default_inventory(human* hp)
{
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        inventory_item* next = iip->next;
        inventory_item* first = hp->inventory;
        human_delete_item_from_inventory(hp, iip, false, false);
        iip = next != first ? next : nullptr;
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if (wrecking_crew_is_active())
    {
        if (pp != nullptr)
        {
            wrecking_crew_set_default_inventory(pp);
        }
        return;
    }
    if (!Multiplayer)
    {
        const human_info* h_info = hp->h_info;
        if (h_info->default_inventory.size() < 1)
        {
            return;
        }
        for (int i = 0; i < h_info->default_inventory.size(); i++)
        {
            const inv_item_info* info = h_info->default_inventory[i].i_info;
            const player_profile_data* profile = player_profile_get("human", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
            if (profile != nullptr && profile->golden_hammer_unlocked && Wants_to_start_game_with_golden_hammer && object_is_player(hp) && info != nullptr &&
                v_stricmp(info->name, "sledgehammer") == 0)
            {
                info = inv_item_get_info("golden_hammer");
            }
            human_add_item_to_inventory(hp, info, h_info->default_inventory[i].count, -1, h_info->default_inventory[i].slot, true, false, true);
        }
    }
    if (Multiplayer == true && pp != nullptr)
    {
        multi_game_set_player_default_inventory(pp);
    }
}

// 0x00A07B80 (:9288)
bool human_inventory_slot_is_available(human* hp, int slot)
{
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->selection_slot == slot)
        {
            return false;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return true;
}

// 0x00A07BB0 (:9300)
inventory_item* human_get_inventory_item(const human* hp, int selection_slot)
{
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->selection_slot == selection_slot)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// The ammo multiplier of a player's backpack (1 without one).
static inline float human_backpack_ammo_factor(player* pp)
{
    if (pp != nullptr && pp->backpack != nullptr)
    {
        return pp->backpack->get_backpack_abilities()->ammo_factor;
    }
    return 1.0f;
}

// 0x00AB5210 (:9319)
// Adds an inventory item: a weapon the human has already gives its ammo (count times a full load, scaled by the
// backpack); a new one gets a selection slot, a weapon object attached to its carry point and its ammo, and goes
// into the inventory. The local player's weapon cabinet hears of it.
inventory_item* human_add_item_to_inventory(human* hp, const inv_item_info* info, int count, int ammo_count, int slot, bool add_to_weapon_cabinet, bool demolitions_master_override,
                                            bool show_inventory)
{
    if (hp == nullptr || info == nullptr)
    {
        return nullptr;
    }
    inventory_item* existing = nullptr;
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->info == info)
        {
            existing = iip;
            break;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    const weapon_info* w_info = weapons_get_weapon_info(info);
    if (w_info == nullptr)
    {
        return nullptr;
    }
    if (count < 1)
    {
        count = demolitions_master_override ? 0 : info->default_count;
    }
    if (existing != nullptr)
    {
        if (w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
        {
            weapon* wp = World.object_from_handle<weapon>(existing->weapon_handle);
            const int ammo = ammo_count < 1 ? (wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine()) * count : ammo_count;
            player* pp = World.object_cast<player>(hp);
            float factor = human_backpack_ammo_factor(pp);
            if (Multiplayer == true)
            {
                factor = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_AMMO_PICKUP) * factor;
            }
            ammo_acquired_event ev;
            ev.w_info = wp->w_info;
            const float added = (float)ammo * factor;
            gem_post_event(ev);
            wp->add_ammo((int)added);
            if (object_is_player(hp))
            {
                wp->obj_flags.special_lifetime = 1;
                if (Multiplayer == true && multi_is_server() && existing->weapon_handle != 0xFFFFFFFF)
                {
                    multi_game_player_picked_up_ammo((player*)hp, existing, (int)added, 0);
                }
            }
        }
        return existing;
    }
    const int selection_slot = slot < 0 ? human_find_open_inventory_selection_slot(hp) : slot;
    if (!info->w_info->flags.non_inventory && !(Multiplayer && !multi_is_server()) && object_is_player(hp) &&
        (selection_slot < 0 || !human_inventory_slot_is_available(hp, selection_slot)))
    {
        return nullptr;
    }
    inventory_item* iip = inv_item_allocate();
    if (iip == nullptr)
    {
        return nullptr;
    }
    iip->info = info;
    iip->count = count;
    iip->weapon_handle = 0xFFFFFFFF;
    iip->attachment_prop = -1;
    iip->selection_slot = selection_slot;
    // Original: the flags reuse a stack slot; only the bits set here are used (the struct has 18 bits).
    obj_item_flags flags;
    *(unsigned int*)&flags = 0;
    flags.no_physics = 1;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_weapon_props props(w_info, w_info->weapon_obj_item_info, hp->pos, hp->orient, flags, COLLIDE_ITEM, -1.0f);
    weapon* wp = World.create_object<weapon>(props, 0xFFFFFFFF);
    if (wp == nullptr)
    {
        inv_item_free(iip);
        return nullptr;
    }
    iip->weapon_handle = wp->get_handle();
    if (hp->char_inst->rdd.is_loaded())
    {
        iip->attachment_prop = animlib_find_character_tag(hp->char_inst->aim_handle, w_info->attachment_point);
        if (iip->attachment_prop == -1)
        {
            iip->attachment_prop = animlib_find_character_bone(hp->char_inst->aim_handle, w_info->attachment_point);
        }
    }
    if (iip->attachment_prop >= 0)
    {
        hp->set_child(wp, iip->attachment_prop, wp->get_grip_tag_index(), false, true);
        wp->hold_item(hp, true);
    }
    player* pp = World.object_cast<player>(hp);
    int ammo = ammo_count;
    if (pp != nullptr && pp->backpack != nullptr)
    {
        ammo = (int)(pp->backpack->get_backpack_abilities()->ammo_factor * (float)ammo_count);
    }
    if (ammo >= 0)
    {
        if (wp->get_magazine_size() < ammo)
        {
            wp->fill_magazine();
            ammo = ammo - wp->get_magazine_size();
        }
        else
        {
            wp->set_rounds_in_magazine(ammo);
            ammo = 0;
        }
        wp->set_rounds_in_reserve(ammo);
    }
    if (object_is_player(hp))
    {
        wp->obj_flags.special_lifetime = 1;
        if (Multiplayer == true && multi_is_server() && iip->weapon_handle != 0xFFFFFFFF)
        {
            multi_game_player_picked_up_weapon((player*)hp, iip);
        }
    }
    insert_item_into_inventory(hp, iip);
    if (!object_is_local_player(hp))
    {
        return iip;
    }
    if (add_to_weapon_cabinet)
    {
        weapon_acquired_event ev;
        ev.w_handle = weapons_get_weapon_info_handle(w_info);
        gem_post_event(ev);
    }
    if (hp->get_equipped_inv_item() != nullptr && hp->get_equipped_inv_item()->info == nullptr)
    {
        hp->desired_equipped_inv_item = hp->inventory;
        hp->set_equipped_inv_item(hp->inventory);
    }
    if (show_inventory == true)
    {
        ovl_inventory_open(CBA_BUTTON_INVALID, 1.0f);
    }
    if (hp->get_equipped_weapon() != nullptr && human_count_inventory_items(hp) == 2)
    {
        hp->last_equipped_weapon = iip;
    }
    return iip;
}

// 0x00AB5690 (:9501)
inventory_item* human_add_item_to_inventory(human* hp, const char* name, int count, int ammo_count, int slot, bool add_to_weapon_cabinet, bool demolitions_master_override, bool show_inventory)
{
    return human_add_item_to_inventory(hp, inv_item_get_info(name), count, ammo_count, slot, add_to_weapon_cabinet, demolitions_master_override, show_inventory);
}

// 0x00AB56B0 (:9518)
// Adds a picked up item object: a non-weapon by its inventory info; a weapon the human has gives its ammo (the
// picked up weapon loses what was taken); a new weapon joins the inventory with its own object.
inventory_item* human_add_item_to_inventory(human* hp, item* obj_itemp, bool add_to_weapon_cabinet, bool demolitions_master_override, bool show_inventory)
{
    if (hp == nullptr || obj_itemp == nullptr)
    {
        return nullptr;
    }
    if (obj_itemp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return human_add_item_to_inventory(hp, inv_item_get_info(obj_itemp->info->name), 0, -1, -1, add_to_weapon_cabinet, demolitions_master_override, show_inventory);
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    weapon* picked = nullptr;
    const inv_item_info* info;
    if (obj_itemp->get_obj_type() == OT_ITEM)
    {
        picked = (weapon*)obj_itemp;
        info = picked->w_info->weapon_inv_item_info;
    }
    else
    {
        info = inv_item_get_info(obj_itemp->info->name);
    }
    if (info == nullptr)
    {
        return nullptr;
    }
    if (pp != nullptr)
    {
        obj_itemp->obj_flags.special_lifetime = 1;
    }
    inventory_item* existing = human_has_item(hp, info);
    if (existing != nullptr)
    {
        weapon* owned = World.object_from_handle<weapon>(existing->weapon_handle);
        if (owned == picked)
        {
            return existing;
        }
        float factor = 1.0f;
        if (pp != nullptr)
        {
            if (pp->backpack != nullptr)
            {
                factor = pp->backpack->get_backpack_abilities()->ammo_factor;
            }
            if (Multiplayer == true)
            {
                factor = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_AMMO_PICKUP) * factor;
            }
            ammo_acquired_event ev;
            ev.w_info = owned->w_info;
            gem_post_event(ev);
        }
        const int added = owned->add_ammo((int)((float)(picked->get_rounds_in_magazine() + picked->get_rounds_in_reserve()) * factor));
        if ((!Multiplayer || multi_is_server()) && !picked->item_flags.respawn && added > 0)
        {
            // the picked up weapon loses the rounds taken (from its reserve first)
            const int reserve = picked->get_rounds_in_reserve();
            int taken = (int)((float)added / factor);
            if (reserve < taken)
            {
                picked->set_rounds_in_reserve(0);
                taken = taken - reserve;
                if (taken > 0)
                {
                    if (picked->get_rounds_in_magazine() < taken)
                    {
                        picked->set_rounds_in_magazine(0);
                    }
                    else
                    {
                        picked->set_rounds_in_magazine(picked->get_rounds_in_magazine() - taken);
                    }
                }
            }
            else
            {
                picked->add_rounds_to_reserve(-taken);
            }
        }
        if (human_has_equipped(hp, info) && owned->w_info->flags.draw_proj_in_opposite_hand)
        {
            item* offhand = World.object_from_handle<item>(hp->offhand_projectile_handle);
            if (offhand != nullptr)
            {
                offhand->hold_item(hp, true);
            }
        }
        if (Multiplayer == true && multi_is_server() && object_is_player(hp) && existing->weapon_handle != 0xFFFFFFFF)
        {
            multi_game_player_picked_up_ammo((player*)hp, existing, added, 0);
        }
        return existing;
    }
    if (obj_itemp->item_flags.respawn)
    {
        // a respawning pickup gives a new weapon of its kind
        return human_add_item_to_inventory(hp, inv_item_get_info(obj_itemp->info->name), 1, -1, -1, add_to_weapon_cabinet, demolitions_master_override, show_inventory);
    }
    if (!info->w_info->flags.non_inventory && (!Multiplayer || multi_is_server()) && object_is_player(hp) && human_find_open_inventory_selection_slot(hp) < 0)
    {
        return nullptr;
    }
    inventory_item* iip = inv_item_allocate();
    if (iip == nullptr)
    {
        return nullptr;
    }
    iip->info = info;
    iip->count = 1;
    iip->weapon_handle = obj_itemp->get_handle();
    iip->selection_slot = human_find_open_inventory_selection_slot(hp);
    iip->attachment_prop = -1;
    insert_item_into_inventory(hp, iip);
    if (hp->char_inst->rdd.is_loaded())
    {
        iip->attachment_prop = animlib_find_character_tag(hp->char_inst->aim_handle, picked->w_info->attachment_point);
        if (iip->attachment_prop == -1)
        {
            iip->attachment_prop = animlib_find_character_bone(hp->char_inst->aim_handle, picked->w_info->attachment_point);
        }
    }
    if (iip->attachment_prop >= 0)
    {
        hp->set_child(picked, iip->attachment_prop, picked->get_grip_tag_index(), false, true);
        picked->hold_item(hp, true);
    }
    if (pp != nullptr && pp->backpack != nullptr)
    {
        picked->set_rounds_in_magazine((int)(pp->backpack->get_backpack_abilities()->ammo_factor * (float)picked->get_rounds_in_magazine()));
        picked->set_rounds_in_reserve((int)(pp->backpack->get_backpack_abilities()->ammo_factor * (float)picked->get_rounds_in_reserve()));
    }
    if (Multiplayer == true && multi_is_server() && object_is_player(hp) && iip->weapon_handle != 0xFFFFFFFF)
    {
        multi_game_player_picked_up_weapon(pp, iip);
    }
    if (!object_is_local_player(hp))
    {
        return iip;
    }
    if (add_to_weapon_cabinet)
    {
        weapon_acquired_event ev;
        ev.w_handle = weapons_get_weapon_info_handle(picked->w_info);
        gem_post_event(ev);
    }
    if (hp->get_equipped_weapon() != nullptr && human_count_inventory_items(hp) == 2)
    {
        hp->last_equipped_weapon = iip;
    }
    if (picked->w_info == nullptr || picked->w_info->flags.non_inventory || !show_inventory)
    {
        return iip;
    }
    ovl_inventory_open(CBA_BUTTON_INVALID, 1.0f);
    return iip;
}

// 0x00ACEC90 (:9764)
void human_delete_all_items_from_inventory(human* hp, bool from_server, bool skip_grenades)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        // The list is a ring; the next item is taken before this one can be deleted.
        inventory_item* next = iip->next != hp->inventory ? iip->next : nullptr;
        bool keep = false;
        if (skip_grenades && iip->weapon_handle != 0xFFFFFFFF)
        {
            const object* objp = World.object_from_handle(iip->weapon_handle);
            keep = objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON &&
                   static_cast<const weapon*>(objp)->w_info->weapon_class == WEAPON_CLASS_THROWN_GRENADE;
        }
        if (!keep)
        {
            human_delete_item_from_inventory(hp, iip, from_server, false);
        }
        iip = next;
    }
}

// 0x00ACED10 (:9781): Every weapon goes, except a player's in selection slot 3.
void human_disarm(human* hp, bool from_server)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        // The list is a ring; the next item is taken before this one can be deleted.
        inventory_item* next = iip->next != hp->inventory ? iip->next : nullptr;
        if (iip->weapon_handle != 0xFFFFFFFF && (!object_is_player(hp) || iip->selection_slot != 3))
        {
            human_delete_item_from_inventory(hp, iip, from_server, false);
        }
        iip = next;
    }
}

// 0x00A07BE0 (:9794)
int human_count_inventory_items(human* hp)
{
    int count = 0;
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->info != nullptr)
        {
            count++;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return count;
}

// 0x00AC7A90 (:9894)
// Removes an item from the inventory: the equipped item falls back to the last equipped one (none when the owner is
// being deleted), the item leaves the ring, its weapon object is destroyed (the local player's high-res textures
// are released with it), and the item goes back to the free list.
void human_delete_item_from_inventory(human* hp, inventory_item* iip, bool from_server, bool owner_being_deleted)
{
    if (Multiplayer)
    {
        if (!multi_is_server() && !from_server)
        {
            return;
        }
        if (Multiplayer == true && multi_is_server() && hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            multi_game_player_remove_weapon((player*)hp, iip);
        }
    }
    if (iip == hp->second_last_equipped_weapon)
    {
        hp->second_last_equipped_weapon = nullptr;
    }
    if (iip == hp->get_equipped_inv_item())
    {
        if (owner_being_deleted || hp->last_equipped_weapon == iip)
        {
            hp->set_equipped_inv_item(nullptr);
            hp->desired_equipped_inv_item = nullptr;
            hp->last_equipped_weapon = nullptr;
        }
        else
        {
            hp->set_equipped_inv_item(hp->last_equipped_weapon);
            hp->desired_equipped_inv_item = hp->last_equipped_weapon;
            hp->last_equipped_weapon = hp->second_last_equipped_weapon;
        }
        hp->second_last_equipped_weapon = nullptr;
    }
    if (iip == hp->desired_equipped_inv_item)
    {
        hp->desired_equipped_inv_item = hp->get_equipped_inv_item();
    }
    if (iip == hp->last_equipped_weapon)
    {
        hp->last_equipped_weapon = hp->second_last_equipped_weapon;
        hp->second_last_equipped_weapon = nullptr;
    }
    bool unlinked = false;
    if (hp->inventory == iip)
    {
        if (iip->next == iip)
        {
            iip->next = nullptr;
            iip->prev = nullptr;
            hp->inventory = nullptr;
            unlinked = true;
        }
        else
        {
            hp->inventory = iip->next;
        }
    }
    if (!unlinked)
    {
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
    if (iip->weapon_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(iip->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            item_info* i_info = ((item*)objp)->info;
            if ((player*)hp == Player && i_info->special_flags.high_res_texture_requested)
            {
                i_info->special_flags.high_res_texture_request_unload = 1;
            }
            World.destroy_object(objp, false);
        }
        iip->weapon_handle = 0xFFFFFFFF;
    }
    // inv_item_free, inlined
    if (Inv_item_free_list == nullptr)
    {
        iip->next = iip;
        iip->prev = iip;
        Inv_item_free_list = iip;
    }
    else
    {
        iip->prev = Inv_item_free_list->prev;
        iip->next = Inv_item_free_list;
        Inv_item_free_list->prev->next = iip;
        Inv_item_free_list->prev = iip;
    }
    Num_inv_items--;
    iip->info = nullptr;
    if (Multiplayer == true && hp->get_equipped_inv_item() == nullptr && hp->desired_equipped_inv_item == nullptr)
    {
        player_equip_sledgehammer(hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr);
    }
    if (!owner_being_deleted)
    {
        human_validate_inventory(hp);
    }
}

// Whether an inventory item's weapon object is gone.
static inline bool human_inv_item_weapon_missing(const inventory_item* iip)
{
    if (iip->weapon_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    return objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON;
}

// 0x00AB5EE0 (:9974)
// Forgets references to weapon objects that are gone.
void human_validate_inventory(human* hp)
{
    if (hp->second_last_equipped_weapon != nullptr && human_inv_item_weapon_missing(hp->second_last_equipped_weapon))
    {
        hp->second_last_equipped_weapon = nullptr;
    }
    if (hp->last_equipped_weapon != nullptr && human_inv_item_weapon_missing(hp->last_equipped_weapon))
    {
        hp->last_equipped_weapon = hp->second_last_equipped_weapon;
        hp->second_last_equipped_weapon = nullptr;
    }
    if (hp->desired_equipped_inv_item != nullptr && human_inv_item_weapon_missing(hp->desired_equipped_inv_item))
    {
        hp->desired_equipped_inv_item = nullptr;
    }
    if (hp->get_equipped_inv_item() != nullptr && human_inv_item_weapon_missing(hp->get_equipped_inv_item()))
    {
        hp->set_equipped_inv_item(hp->last_equipped_weapon);
    }
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (human_inv_item_weapon_missing(iip))
        {
            iip->weapon_handle = 0xFFFFFFFF;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
}

// 0x00A07C10 (:10049)
inventory_item* human_has_item(const human* hp, const inv_item_info* item_info)
{
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        if (iip->info == item_info)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A07C70 (:10082)
inventory_item* human_has_weapon_of_type(const human* hp, weapon_class_type weapon_class)
{
    if (hp == nullptr)
    {
        return nullptr;
    }
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        const weapon_info* wi = iip->info->w_info;
        if (wi != nullptr && wi->weapon_class == weapon_class)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A7C860 (:10105). A melee weapon counts as ammo, so the sledgehammer of a challenge never runs out. Original: the
// charges are asked for before the null check, which reads through a null hp when include_thrown_charges is set.
bool human_has_ammo(const human* hp, bool include_thrown_charges, bool include_non_inventory)
{
    if (include_thrown_charges)
    {
        int num_attached;
        if (count_charges(hp->get_handle(), num_attached) != 0)
        {
            return true;
        }
    }
    if (hp == nullptr)
    {
        return false;
    }
    for (inventory_item* iip = hp->inventory; iip != nullptr;)
    {
        const weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
        if (wp != nullptr && (wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine() > 0 || wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE) && (include_non_inventory || !wp->w_info->flags.non_inventory))
        {
            return true;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            return false;
        }
    }
    return false;
}

// 0x00A2E630 (:10134)
inventory_item* human_has_item(const human* hp, const char* name)
{
    return human_has_item(hp, inv_item_get_info(name));
}

// 0x00A07D00 (:10155)
bool human_has_equipped(const human* hp, const inv_item_info* item_info)
{
    return hp->get_equipped_inv_item() != nullptr && hp->get_equipped_inv_item()->info == item_info;
}

// 0x00ABF800 (:10165)
// Asks for an inventory item to be equipped: a player leaves its zoom; the old weapon is unequipped (a carried
// non-inventory item is set down first) and remembered as the last equipped one; the new weapon starts equipping
// (human_do_equip_unequip plays the change).
void human_equip_inventory_item(human* hp, inventory_item* iip)
{
    if (hp == nullptr || iip == nullptr)
    {
        return;
    }
    inventory_item* equipped = hp->get_equipped_inv_item();
    bool carrying_non_inventory = false;
    if (equipped != nullptr)
    {
        if (equipped->info == nullptr)
        {
            hp->desired_equipped_inv_item = hp->inventory;
            hp->set_equipped_inv_item(hp->inventory);
            equipped = hp->get_equipped_inv_item();
        }
        carrying_non_inventory = equipped->info->w_info->flags.non_inventory;
    }
    if (hp->desired_equipped_inv_item == iip)
    {
        return;
    }
    player* pp = nullptr;
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        pp = (player*)hp;
        turret* tp;
        if (pp->zoom_state != PZS_OFF && pp->zoom_state != PZS_SPRINT && ((tp = human_combat_get_manned_turret(pp)) == nullptr || !tp->t_info->flags.force_fine_aim))
        {
            const player_zoom_state zoom = pp->zoom_state;
            pp->previous_zoom_state = zoom;
            pp->zoom_state = PZS_OFF;
            bool set_view = true;
            if (zoom != PZS_OFF)
            {
                if (pp != Player)
                {
                    set_view = false;
                }
                else
                {
                    pp->get_equipped_weapon(); // Original: the result is not used.
                    if (pp->previous_zoom_state == PZS_SCOPE)
                    {
                        pp->set_opacity_plus_child_items(1.0f);
                        pp->set_casts_transparent_shadows_plus_child_items(false);
                        hud_ui_rail_driver_set_visible(false);
                        hud_ui_sniper_set_visible(false);
                    }
                }
            }
            if (set_view && pp == Player)
            {
                camera_set_view_player(PZS_OFF, false);
            }
        }
    }
    if (hp->hflags.playing_equip_anim && hp->char_inst->is_playing_action(ANIM_ALIAS_EQUIP) && equipped == nullptr)
    {
        hp->char_inst->stop_action(ANIM_ALIAS_EQUIP, 3, true);
        vanim_get_action(hp->char_inst->vp, ANIM_ALIAS_EQUIP)->alias = ANIM_ALIAS_NONE;
    }
    human_validate_inventory(hp);
    inventory_item* last;
    if (equipped == nullptr)
    {
        if (hp->desired_equipped_inv_item != nullptr)
        {
            hp->second_last_equipped_weapon = hp->last_equipped_weapon;
            hp->last_equipped_weapon = hp->desired_equipped_inv_item;
            goto equip;
        }
        last = hp->second_last_equipped_weapon;
        hp->second_last_equipped_weapon = nullptr;
    }
    else
    {
        bool set_down = false;
        if (equipped->weapon_handle == 0xFFFFFFFF)
        {
            set_down = carrying_non_inventory;
        }
        else
        {
            weapon* wp = World.object_from_handle<weapon>(equipped->weapon_handle);
            if (carrying_non_inventory)
            {
                set_down = true;
            }
            else
            {
                human_weapons_unequip_weapon(hp, wp);
            }
        }
        if (set_down)
        {
            if (!player_set_down_non_inventory(pp))
            {
                hp->desired_equipped_inv_item = iip;
                human_validate_inventory(hp);
                return;
            }
            last = hp->second_last_equipped_weapon;
        }
        else
        {
            last = hp->get_equipped_inv_item();
            hp->second_last_equipped_weapon = hp->last_equipped_weapon;
        }
    }
    hp->last_equipped_weapon = last;
equip:
    if (hp->desired_equipped_inv_item != iip && iip->weapon_handle != 0xFFFFFFFF)
    {
        World.object_from_handle<weapon>(iip->weapon_handle)->equip();
    }
    hp->desired_equipped_inv_item = iip;
    human_validate_inventory(hp);
}

// 0x00A53BC0 (:10279)
// Puts the equipped weapon away (and leaves combat readiness).
void human_unequip_inventory_item(human* hp)
{
    if (hp == nullptr)
    {
        return;
    }
    inventory_item* equipped = hp->get_equipped_inv_item();
    if (equipped == nullptr)
    {
        return;
    }
    if (equipped->weapon_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(equipped->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            human_weapons_unequip_weapon(hp, (weapon*)objp);
        }
    }
    hp->second_last_equipped_weapon = hp->last_equipped_weapon;
    hp->last_equipped_weapon = equipped;
    hp->desired_equipped_inv_item = nullptr;
    human_in_vehicle_interaction(hp, nullptr); // Original: the result is not used.
    hp->combat.combat_ready_timestamp.invalidate();
}

// 0x00AC7C90 (:10313)
// A human died: kill and death bookkeeping (who, when, where), highlights, the multiplayer mode and the backpack,
// morale and stats, activity reports, turret abandon, the game log, ladders, a flyer losing its driver, the
// player's weapon kill stats and kill event, and its voice and lights stop.
void human_died(human* hp, human* attacker, damaged_by_types damage_type, const weapon_info* w_info, const object* owner)
{
    if (Multiplayer && attacker == nullptr)
    {
        attacker = hp;
    }
    weapon* victim_weapon = nullptr;
    if (hp->get_equipped_inv_item() != nullptr)
    {
        const unsigned int handle = hp->get_equipped_inv_item()->weapon_handle;
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            victim_weapon = (weapon*)objp;
            if (victim_weapon->w_info->weapon_class == WEAPON_CLASS_GRINDER)
            {
                victim_weapon->stop_all_grinder_effects();
            }
        }
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_player_died(hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr);
        return;
    }
    const bool victim_is_player = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
    const bool attacker_is_player = attacker != nullptr && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER;
    if (Player == nullptr || Player != (player*)hp)
    {
        if (attacker_is_player && Player == (player*)attacker)
        {
            // Magic numbers: the highlight video spans 15 s before to 3 s after the kill.
            nvidia_highlights_save_video(s_highlightIdKill, s_groupIdGameplay, -15000, 3000);
            nvidia_highlights_save_screenshot(s_highlightIdKill, s_groupIdGameplay);
        }
    }
    else
    {
        nvidia_highlights_save_video(s_highlightIdDeath, s_groupIdGameplay, -10000, 2000);
        nvidia_highlights_save_screenshot(s_highlightIdDeath, s_groupIdGameplay);
    }
    if (Multiplayer == true)
    {
        if (victim_is_player && attacker_is_player == victim_is_player)
        {
            player* victim = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
            if (attacker == nullptr || attacker->get_obj_type() != OT_HUMAN || attacker->get_sub_type() != OT_SUB_HUMAN_PLAYER)
            {
                multi_gameplay_mode_player_died(victim, nullptr, damage_type, w_info);
            }
            else
            {
                multi_gameplay_mode_player_died(victim, (player*)attacker, damage_type, w_info);
            }
        }
        else
        {
            // Original: a debug message ("Wha? We have non-player humans in multiplayer? No wai!").
        }
    }
    if ((!Multiplayer || multi_is_server()) && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)hp)->backpack != nullptr)
    {
        ((player*)hp)->backpack->event_player_died();
    }
    hp->combat.last_killed_by_handle = attacker == nullptr ? 0xFFFFFFFF : attacker->get_handle();
    hp->combat.last_killed_time_ms = timer_get(TIMER_MILLISECONDS);
    hp->combat.last_killed_pos = hp->pos;
    if (attacker != nullptr)
    {
        attacker->combat.last_kill_handle = hp->get_handle();
        attacker->combat.last_kill_time_ms = timer_get(TIMER_MILLISECONDS);
        if (attacker != hp)
        {
            hp->combat.last_killed_by_other_handle = hp->combat.last_killed_by_handle;
            hp->combat.last_killed_by_other_time_ms = hp->combat.last_killed_time_ms;
            attacker->combat.last_kill_other_handle = hp->get_handle();
            attacker->combat.last_kill_other_time_ms = timer_get(TIMER_MILLISECONDS);
        }
    }
    if (victim_is_player && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        ((player*)hp)->pflags.no_fineaim = 0;
        player_update_jetpack((player*)hp, false);
    }
    gfm_control_morale_human_killed(hp, attacker);
    stats_register_human_death(hp, attacker, damage_type);
    if (!Multiplayer && !wrecking_crew_is_active() && attacker != nullptr && attacker->get_team() == HUMAN_TEAM_EDF && hp->get_team() == HUMAN_TEAM_CIVILIAN)
    {
        // Magic numbers: civilians within 30 m may turn on the EDF (half the chance).
        morale_maybe_convert_civilians_spontaneously_around_event(attacker->pos, 30.0f, attacker, 0.5f);
    }
    if (!victim_is_player)
    {
        alm_report_human_death(hp);
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && !((npc*)hp)->npc_flags.spawned_as_corpse)
    {
        activity_register_death(hp, attacker != nullptr ? (object*)attacker : (object*)owner);
    }
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp != nullptr)
    {
        tp->disable_for_npcs_time.set(Ai_reuse_turret_after_death_time * 1000);
    }
    human_combat_abandon_turret(hp);
    if ((!Multiplayer || multi_is_server()) && (attacker_is_player || victim_is_player))
    {
        const char attacker_in_cover = attacker != nullptr ? attacker->cur_state == HS_COVER : 0;
        const char pct_complete = gamelog_get_pct_complete();
        const int time_played = gamelog_get_time_played();
        const int playing_crc = gamelog_get_player_playing_crc();
        const int duration = gamelog_get_game_duration();
        const char difficulty = gamelog_get_difficulty();
        const int locale = gamelog_get_locale();
        // falling debris (level objects) logs the debris weapon
        const int attacker_weapon_crc = !Multiplayer && damage_type == DBT_LEVEL_OBJECT ? gamelog_get_weapon_crc(multi_gw_get_custom_weapon_info(MCW_DEBRIS)) : gamelog_get_weapon_crc(w_info);
        const vector attacker_pos = gamelog_get_human_pos(attacker);
        const char attacker_index = gamelog_get_player_index(attacker);
        const int victim_weapon_crc = gamelog_get_weapon_crc(victim_weapon);
        const vector victim_pos = gamelog_get_human_pos(hp);
        const char victim_index = gamelog_get_player_index(hp);
        const int time = gamelog_get_logging_time();
        const int checksum = gamelog_get_player_checksum();
        log_event_human_killed(checksum, time, victim_index, victim_pos, victim_weapon_crc, attacker_index, attacker_pos, attacker_weapon_crc, locale, difficulty, duration, attacker_in_cover,
                               playing_crc, time_played, pct_complete);
    }
    if (hp->hflags.is_on_ladder)
    {
        human_dismount_ladder(hp);
    }
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->flags.disable_gravity = 0;
    }
    if (hp->cur_state == HS_VEHICLE_DRIVING)
    {
        Cartographer.remove_icon(hp->vehicle_handle);
    }
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER && hp == vehicle_passenger_in_seat((vehicle*)objp, VSI_DRIVER))
        {
            // a flyer without its driver goes down
            vehicle_queue_damage((vehicle*)objp, attacker, w_info, DBT_EXPLOSION, &hp->pos, &UP_VECTOR, 2.0f, 0x7FFFFFFF, &ZERO_VECTOR, false);
        }
    }
    if (!Multiplayer && !wrecking_crew_is_active() && !victim_is_player && Player != nullptr && (player*)attacker == Player && w_info != nullptr)
    {
        bool count_stat = true;
        tracked_stats_indices stat = STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER;
        const unsigned int handle = weapons_get_weapon_info_handle(w_info);
        switch (w_info->weapon_class)
        {
        case WEAPON_CLASS_BULLET:
            if (handle == weapons_get_weapon_info_handle("edf_pistol"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_PISTOL;
            }
            else if (handle == weapons_get_weapon_info_handle("m16"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_M16;
            }
            else if (handle == weapons_get_weapon_info_handle("enforcer"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_ENFORCER;
            }
            else if (handle == weapons_get_weapon_info_handle("autoblaster"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_AUTOBLASTER;
            }
            else if (handle == weapons_get_weapon_info_handle("gauss_rifle"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_GAUSS_RIFLE;
            }
            else if (handle == weapons_get_weapon_info_handle("sniper_rifle"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_SNIPER_RIFLE;
            }
            else if (handle == weapons_get_weapon_info_handle("rail_driver"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_RAIL_DRIVER;
            }
            else if (handle == weapons_get_weapon_info_handle("shotgun"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_SHOTGUN;
            }
            else
            {
                count_stat = false;
            }
            break;
        case WEAPON_CLASS_LAUNCHER:
            if (handle == weapons_get_weapon_info_handle("thermobaric_rockets"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_THERMOBARIC_ROCKETS;
            }
            else if (handle == weapons_get_weapon_info_handle("rpg_launcher"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_ROCKET_LAUNCHER;
            }
            else
            {
                count_stat = false;
            }
            break;
        case WEAPON_CLASS_THROWN_CHARGE:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_REMOTE_CHARGES;
            break;
        case WEAPON_CLASS_THROWN_MINE:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_PROXIMITY_MINES;
            break;
        case WEAPON_CLASS_SLEDGEHAMMER:
            if (hp->get_team() == HUMAN_TEAM_EDF)
            {
                stats_increment_by_one(STATS_EDF_KILLED_SLEDGE);
            }
            stat = STATS_SP_TOTAL_WEAPON_KILLS_SLEDGEHAMMER;
            break;
        case WEAPON_CLASS_GUTTER:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_GUTTER;
            break;
        case WEAPON_CLASS_GRINDER:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_GRINDER;
            break;
        case WEAPON_CLASS_ARC_WELDER:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_ARC_WELDER;
            break;
        case WEAPON_CLASS_NANO_RIFLE:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_NANO_RIFLE;
            break;
        case WEAPON_CLASS_HARPOON:
            if (handle == weapons_get_weapon_info_handle("spiker"))
            {
                stat = STATS_SP_TOTAL_WEAPON_KILLS_SPIKER;
            }
            else
            {
                count_stat = false;
            }
            break;
        case WEAPON_CLASS_SINGULARITY_BOMB:
            stat = STATS_SP_TOTAL_WEAPON_KILLS_SINGULARITY_BOMB;
            break;
        default:
            count_stat = false;
            break;
        }
        if (count_stat)
        {
            stats_increment_by_one(stat);
        }
    }
    if (attacker_is_player)
    {
        player_kills_human_event ev;
        ev.handle = hp->get_handle();
        gem_post_event(ev);
    }
    if (hp->voice_instance != -1)
    {
        audiolib_play_stop(hp->voice_instance);
        if (audiolib_play_get_state(-1, nullptr) != AUDIOLIB_ERR_NONE)
        {
            hp->voice_time_since_finish.set(0);
        }
        hp->voice_instance = -1;
    }
    kill_light_effects(hp);
}

// 0x00ACED70 (:10582)
// Damages a human: the attacker of a ragdoll (its source), the difficulty and backpack scaling, the VIP complaint,
// invulnerability (also of guerrilla specialists outside missions), multiplayer rules, the hit points (a player
// has a death buffer: a hit that would kill by little leaves 1-20 hit points), the hit bookkeeping, and death or
// the npc's perception of the hit. Returns the damage applied.
float human_damage(human* hp, human* attacker, float damage_amount, damaged_by_types damage_type, const weapon_info* w_info, const object* owner, bool play_pain_sound, bool silent_attack)
{
    if (hp == nullptr)
    {
        return 0.0f;
    }
    const human_ragdoll_state rs = hp->ragdoll_state;
    if (attacker == nullptr && rs != HUMAN_RAGDOLL_STATE_BLEND_OUT && rs != HUMAN_RAGDOLL_STATE_NONE && rs != HUMAN_RAGDOLL_STATE_FLINCH &&
        (rs != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos))
    {
        // a ragdolled human is hurt by whoever ragdolled it
        object* source = hp->ragdoll_source_human_handle != 0xFFFFFFFF ? World.object_from_handle(hp->ragdoll_source_human_handle) : nullptr;
        attacker = source != nullptr && source->get_obj_type() == OT_HUMAN ? (human*)source : nullptr;
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    player* attacker_pp = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)attacker : nullptr;
    if (Multiplayer == true && multi_is_server() && attacker_pp != nullptr)
    {
        multi_connection* connection = Game_session->get_player_connection(attacker_pp->mp_data);
        if (connection != nullptr && (multi_core_is_server_using_standby_hack() || connection->possibly_using_standby_hack()))
        {
            return 0.0f;
        }
    }
    float backpack_factor = 1.0f;
    if (pp != nullptr && pp->backpack != nullptr)
    {
        backpack_factor = pp->backpack->get_backpack_abilities()->damage_taken_factor;
    }
    if (Multiplayer || wrecking_crew_is_active())
    {
        if (wrecking_crew_is_active())
        {
            damage_amount = upgrade_get_player_armor() * damage_amount;
        }
    }
    else if (damage_amount != hp->hit_points && damage_amount >= 0.0f && damage_type != DBT_RADIATION)
    {
        if (object_is_local_player(hp))
        {
            damage_amount = difficulty_get_value(DIFFICULTY_PLAYER_DAMAGE_TAKEN, NUM_DIFFICULTY_LEVELS) * damage_amount;
            damage_amount = upgrade_get_player_armor() * damage_amount;
        }
        else if (Player != nullptr && !local_player_is_friendly(hp))
        {
            damage_amount = difficulty_get_value(DIFFICULTY_ENEMY_DAMAGE_TAKEN, NUM_DIFFICULTY_LEVELS) * damage_amount;
        }
    }
    if (attacker_pp != nullptr && damage_amount > 0.0f)
    {
        hp->hflags.damaged_by_player = 1;
        if (!Multiplayer && !wrecking_crew_is_active() &&
            (hp->h_info->flags.guerrilla_tech || hp->h_info->flags.guerrilla_rfc || hp->h_info->flags.old_coot || hp->hflags.safehouse_vip || hp->hflags.complain_when_shot))
        {
            hp->play_voice_line(VOICE_LINE_COMBAT_FRIENDLY_FIRE_VIP_COMPLAIN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    const bool specialist = hp->h_info->flags.guerrilla_tech || hp->h_info->flags.guerrilla_rfc || hp->h_info->flags.old_coot;
    if ((hp->hflags.invulnerable || (!Multiplayer && !wrecking_crew_is_active() && specialist && !mission_is_active()) || backpack_factor == 0.0f) && damage_amount > 0.0f)
    {
        // Magic number: a Rhino backpack shrugging off a killing hit of 100 or more gives the good timing experience.
        if (Multiplayer == true && pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->rhino == true && attacker_pp != nullptr &&
            human_teams_are_enemies(attacker_pp, pp) && damage_amount >= 100.0f && pp->hit_points <= damage_amount)
        {
            multi_game_xp_trigger_event(pp->mp_data, MXE_GOOD_TIMING, 1, 0xFF, -1, false);
        }
        return 0.0f;
    }
    if (hp->is_killed())
    {
        return 0.0f;
    }
    if (Multiplayer == true)
    {
        if (!multi_is_server())
        {
            if (damage_amount > 0.0f && play_pain_sound)
            {
                human_play_pain_sound(hp, attacker, w_info, damage_type, silent_attack);
            }
            return 0.0f;
        }
        if (Multiplayer == true)
        {
            if (Multi_match_info.match_state != MGMS_GAMEPLAY)
            {
                return 0.0f;
            }
            if (attacker != nullptr && hp != attacker && human_teams_are_allies(hp, attacker) && !Mp_server_options.generic_options.friendly_fire)
            {
                return 0.0f;
            }
        }
    }
    if (mission_get_mode() == MISSION_MODE_CUTSCENE)
    {
        return 0.0f;
    }
    const float raw_damage = damage_amount;
    if (damage_amount > 0.0f && damage_type != DBT_RADIATION)
    {
        if (human_in_players_vehicle(hp))
        {
            return 0.0f;
        }
        if (pp != nullptr)
        {
            if (hp->cur_state == HS_AMMO_BOX)
            {
                return 0.0f;
            }
            if (!Multiplayer && !wrecking_crew_is_active() && player_is_sabotaging_flyer(pp, nullptr))
            {
                return 0.0f;
            }
        }
    }
    if (raw_damage < 0.0f || should_kill_when_damaged(hp, damage_type))
    {
        // Original: negative damage (and tied-up npcs) take all the hit points.
        damage_amount = hp->hit_points;
    }
    else
    {
        damage_amount = backpack_factor * raw_damage;
        if (attacker_pp != nullptr && attacker_pp->backpack != nullptr && w_info != nullptr && damage_type != DBT_EXPLOSION)
        {
            damage_amount = attacker_pp->backpack->get_backpack_abilities()->direct_damage_dealt_factor * damage_amount;
        }
    }
    if (damage_amount > 0.0f)
    {
        if (attacker_pp != nullptr && attacker != hp)
        {
            if (!Multiplayer && attacker_pp == Player)
            {
                attacker_pp->pflags.local_player_hit_someone_this_frame = 1;
            }
            else
            {
                attacker_pp->pflags.mp_hit_someone_this_network_frame = 1;
            }
        }
        if (pp != nullptr)
        {
            pp->pflags.mp_lost_hp = 1;
        }
    }
    const float old_hit_points = hp->hit_points;
    hp->hit_points = old_hit_points - damage_amount;
    if (!Multiplayer)
    {
        if (!wrecking_crew_is_active() && pp != nullptr && !hp->hflags.used_death_buffer)
        {
            // Magic numbers: a death by bullets, explosions, fire, melee or a vehicle that leaves the player at
            // -1 to -600 times the difficulty's buffer leaves 1 to 20 hit points instead (once); under -150 the
            // player ragdolls.
            const float buffer = difficulty_get_value(DIFFICULTY_PLAYER_DEATH_BUFFER, NUM_DIFFICULTY_LEVELS);
            if (hp->hit_points <= -1.0f && buffer * -600.0f <= hp->hit_points && (int)damage_type > 0 && (int)damage_type < 6)
            {
                hp->hflags.used_death_buffer = 1;
                const int left = int_rand_range(1, 20);
                if (hp->hit_points < -150.0f && !vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_DEATH) && hp->ragdoll_idx < 0)
                {
                    human_ragdoll_start(hp, false, 0xFFFFFFFF);
                }
                hp->hit_points = (float)left;
            }
        }
    }
    float hit_points = hp->hit_points;
    if (hp->hit_points - 0.0f < 0.0f)
    {
        hit_points = 0.0f;
    }
    hp->hflags.health_change_was_negative = 1;
    hp->hit_points = hit_points;
    if (pp != nullptr)
    {
        if (pp->pflags.never_die && hit_points <= 0.0f)
        {
            hp->hit_points = 1.0f;
        }
        if (Multiplayer == true && attacker_pp != nullptr)
        {
            const int index = multi_game_data_get_player_index(attacker_pp->mp_data);
            if (index != -1)
            {
                pp->mp_player_damage_history[index] = pp->mp_player_damage_history[index] + damage_amount;
            }
        }
    }
    if (attacker != nullptr)
    {
        hp->combat.last_hit_by_handle = attacker->get_handle();
        const int now = timer_get(TIMER_MILLISECONDS);
        hp->combat.last_hit_time_ms = now;
        hp->combat.last_hit_by_damage_type = damage_type;
        if (hp != attacker)
        {
            hp->combat.last_hit_by_other_handle = hp->combat.last_hit_by_handle;
            hp->combat.last_hit_by_other_time_ms = now;
            hp->combat.last_hit_by_other_damage_type = damage_type;
        }
    }
    if (w_info != nullptr)
    {
        const unsigned int handle = weapons_get_weapon_info_handle(w_info);
        hp->combat.last_hit_by_weapon_handle = handle;
        if (attacker != nullptr && hp != attacker)
        {
            hp->combat.last_hit_by_other_weapon_handle = handle;
        }
    }
    human_health_damage_register(hp, damage_amount);
    if (hp->hit_points > 0.0f)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)hp)->get_ai()->perception.register_hit(attacker, damage_amount, w_info);
            activity_register_damage(hp, attacker != nullptr ? (object*)attacker : (object*)owner, damage_amount, (int)raw_damage);
        }
    }
    else
    {
        human_died(hp, attacker, damage_type, w_info, owner);
    }
    if (damage_amount > 0.0f && play_pain_sound)
    {
        human_play_pain_sound(hp, attacker, w_info, damage_type, silent_attack);
    }
    return damage_amount;
}

// 0x00A2E6C0 (:10879)
// Stops the voice line (not for a lower priority than the playing one; the dan of a living human never stops).
bool human::stop_voice(voice_line_priorities priority)
{
    if ((unsigned int)(priority - VOICE_LINE_PRIORITY_VERY_LOW) > 9 || (priority != VOICE_LINE_PRIORITY_FORCE_ALWAYS && (int)priority < (int)voice_priority))
    {
        return false;
    }
    if (h_info->flags.dan && !is_killed())
    {
        return false;
    }
    if (radio_instance != 0)
    {
        hud_ui_clear_current_radio_message(radio_instance);
    }
    if (voice_instance != -1)
    {
        audiolib_play_stop(voice_instance);
        if (audiolib_play_get_state(-1, nullptr) != AUDIOLIB_ERR_NONE)
        {
            voice_time_since_finish.set(0);
        }
        voice_instance = -1;
    }
    hflags.voice_line_play_2d = 0;
    queued_voice_line = INVALID_VOICE_LINE_HANDLE;
    situational_voice_line = VOICE_LINE_NONE;
    lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    voice_priority = VOICE_LINE_PRIORITY_NONE;
    voice_cue_priority = AUDIOLIB_CUE_PRIORITY_INVALID;
    voice_time_since_finish.set(0);
    voice_delay_time.invalidate();
    return true;
}

// 0x00A9A8F0 (:10932)
// Plays a voice line of the human's persona: the group's play chance, the persona's line and cooldown, the
// priority against the playing line, a random delay, then the line (2D for the local player).
play_voice_status human::play_voice_line(voice_line_handle line_handle, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay,
                                         bool play_intro, bool play_outro)
{
    human_voice_persona* persona = (human_voice_persona*)voice;
    const human_voice_line_info* line_info;
    if (persona == nullptr || (line_info = human_voice_line_get_info(line_handle)) == nullptr || !line_info->group_descriptors[persona->group].flags.valid)
    {
        return PLAY_VOICE_FAILURE_ERROR;
    }
    const human_voice_line_group_info* group = &line_info->group_descriptors[persona->group];
    if (!force_play && group->play_chance_pct < 100 && !int_rand_chance((unsigned int)group->play_chance_pct))
    {
        return PLAY_VOICE_FAILURE_CHANCE;
    }
    persona = (human_voice_persona*)voice;
    if (persona->find_voice_line(line_handle) == nullptr)
    {
        return PLAY_VOICE_FAILURE_MISSING;
    }
    if (force_play)
    {
        persona->reset_line_cooldown(line_handle);
    }
    else if (!voice_time_since_finish.valid() && (radio_instance != 0 || voice_instance != -1))
    {
        const unsigned char priority = group->default_priority;
        if ((int)priority < (int)voice_priority || ((unsigned int)priority == (unsigned int)voice_priority && priority < 9))
        {
            return PLAY_VOICE_FAILURE_PRIORITY;
        }
    }
    int delay_ms = 0;
    if (!no_delay && group->max_delay != 0)
    {
        delay_ms = int_rand_range((unsigned int)group->min_delay, (unsigned int)group->max_delay);
    }
    persona = (human_voice_persona*)voice;
    human_voice_persona_line* line = persona->find_voice_line(line_handle);
    if (line == nullptr)
    {
        persona->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        return voice->last_operation_status;
    }
    if (!persona->can_play_line_internal(line, delay_ms))
    {
        return voice->last_operation_status;
    }
    stop_voice(VOICE_LINE_PRIORITY_FORCE_ALWAYS);
    player* local = Player;
    const voice_line_priorities old_priority = voice_priority;
    queued_voice_line = line_handle;
    situational_voice_line = VOICE_LINE_NONE;
    voice_priority = (voice_line_priorities)group->default_priority;
    voice_cue_priority = cue_priority;
    voice_time_since_finish.invalidate();
    voice_delay_time.set(delay_ms);
    voice_delay_cb_data = delay_cb_data;
    voice_delay_cb = delay_cb;
    hflags.voice_line_play_2d = group->play_2d();
    vector head_pos;
    human_get_head_pos(this, head_pos);
    human* source = this;
    if ((player*)this == local)
    {
        hflags.voice_line_play_2d = 1;
        head_pos.zero();
        source = nullptr;
    }
    if (((human_voice_persona*)voice)
            ->persona_play_voice_line(queued_voice_line, head_pos, voice_cue_priority, voice_instance, radio_instance, &lipsync_handle, delay_ms > 0, false, play_intro, play_outro, source))
    {
        return PLAY_VOICE_SUCCESS;
    }
    voice_priority = old_priority;
    voice_time_since_finish.set(0);
    return PLAY_VOICE_FAILURE_ERROR;
}

// 0x00A9AC00 (:11089)
// Plays a situational voice line through its descriptor (none until the voice lines are loaded).
play_voice_status human::play_voice_line(voice_lines line, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay)
{
    if (voice != nullptr && line < NUM_VOICE_LINES && Voice_line_lookup_table[line].descriptor != nullptr)
    {
        // Magic number: 0x38, the size of a voice line descriptor.
        const voice_line_handle voice_handle = (voice_line_handle)(((const char*)Voice_line_lookup_table[line].descriptor - (const char*)Voice_line_descriptors) / 0x38);
        if (voice_handle != INVALID_VOICE_LINE_HANDLE)
        {
            return play_voice_line(voice_handle, delay_cb, delay_cb_data, cue_priority, force_play, no_delay, true, true);
        }
    }
    return PLAY_VOICE_FAILURE_ERROR;
}

// 0x00A07E50 (:11423)
int human::get_voice_line_play_id()
{
    if (voice_instance != -1)
    {
        return voice_instance;
    }
    if (radio_instance != 0)
    {
        return hud_ui_radio_get_play_id_from_msg(radio_instance);
    }
    return -1;
}

// 0x00A07E80 (:11435)
void human::set_voice_play_inst(int play_id)
{
    if (audiolib_play_get_state(play_id, nullptr) != AUDIOLIB_ERR_NONE)
    {
        voice_time_since_finish.set(0);
    }
    voice_instance = play_id;
}

// 0x00A9ABB0 (:11067)
play_voice_status human::play_voice_line(const char* line_name, bool (*delay_cb)(const human*, void*), void* delay_cb_data, audiolib_cue_priority cue_priority, bool force_play, bool no_delay)
{
    const voice_line_handle line_handle = voice_line_handle_from_name(line_name);
    if (line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return PLAY_VOICE_FAILURE_ERROR;
    }
    return play_voice_line(line_handle, delay_cb, delay_cb_data, cue_priority, force_play, no_delay, true, true);
}

// 0x00A9AC70 (:11194), SFX.md SX6. Per frame (human_voice_do_frame): a delayed line waits (following the head) until
// its delay passes and its callback agrees, then unpauses and plays; a playing line follows the human; the line ends
// when its play or radio message does.
play_voice_status human::voice_do_frame()
{
    play_voice_status status = PLAY_VOICE_SUCCESS;
    if (voice_time_since_finish.valid())
    {
        return status;
    }
    if (voice_delay_time.valid())
    {
        if (!voice_delay_time.elapsed())
        {
            if (audiolib_play_get_state(voice_instance, nullptr) == AUDIOLIB_ERR_NONE)
            {
                vector head_pos;
                human_get_head_pos(this, head_pos);
                audiolib_play_set_3d_position(voice_instance, &head_pos, nullptr, nullptr);
            }
            return status;
        }
        if (voice_delay_cb == nullptr || voice_delay_cb(this, voice_delay_cb_data))
        {
            vector head_pos = ZERO_VECTOR;
            if ((player*)this != Player)
            {
                human_get_head_pos(this, head_pos);
            }
            if (radio_instance == 0 && voice_instance != -1)
            {
                bool paused = false;
                if (audiolib_play_get_pause(voice_instance, &paused) == AUDIOLIB_ERR_NONE && paused)
                {
                    audiolib_play_set_pause(voice_instance, false);
                }
                if (audiolib_play_get_state(voice_instance, nullptr) == AUDIOLIB_ERR_NONE)
                {
                    // Original: the head position is read again (head_pos above is unused).
                    vector reread_head_pos;
                    human_get_head_pos(this, reread_head_pos);
                    audiolib_play_set_3d_position(voice_instance, &reread_head_pos, nullptr, nullptr);
                }
                else
                {
                    set_voice_play_inst(-1);
                }
            }
            head_morphs.current_lipsync_handle = lipsync_handle;
            head_morphs.has_new_data = 1;
        }
        else
        {
            // The callback refused the line: it is dropped.
            audiolib_play_stop(voice_instance);
            if (audiolib_play_get_state(-1, nullptr) != AUDIOLIB_ERR_NONE)
            {
                voice_time_since_finish.set(0);
            }
            voice_instance = -1;
        }
        if (voice_instance == -1 && radio_instance == 0)
        {
            situational_voice_line = VOICE_LINE_NONE;
            voice_time_since_finish.set(0);
            voice_delay_time.invalidate();
            return status;
        }
    }
    else
    {
        if (radio_instance != 0)
        {
            if (hud_ui_is_radio_message_valid(radio_instance, nullptr, nullptr))
            {
                return status;
            }
            voice_time_since_finish.set(0);
            radio_instance = 0;
            return status;
        }
        if (voice_instance != -1)
        {
            if (audiolib_play_get_state(voice_instance, nullptr) != AUDIOLIB_ERR_NONE)
            {
                voice_time_since_finish.set(0);
                voice_instance = -1;
                return status;
            }
            if (!audiolib_is_playing(voice_instance, false))
            {
                audiolib_play_set_pause(voice_instance, false);
            }
            if (!hflags.voice_line_play_2d)
            {
                audiolib_play_set_3d_position(voice_instance, &pos, nullptr, nullptr);
            }
            return status;
        }
        status = PLAY_VOICE_FAILURE_MISSING;
    }
    voice_delay_time.invalidate();
    return status;
}

// 0x00A2E670 (:10854): a dead human's voice is done when its line finished or none is queued.
bool human::voice_persona_finished() const
{
    if (get_sub_type() == OT_SUB_HUMAN_PLAYER || !is_killed())
    {
        return false;
    }
    if (voice_time_since_finish.valid())
    {
        return true;
    }
    return queued_voice_line == INVALID_VOICE_LINE_HANDLE;
}

// 0x00A07D50 (:11163)
// Run over: the crunch, with the timer set back 200 ms. A hit: the thud, at most once per 400 ms of that timer.
void human::play_vehicle_impact_sound(const vector& posp, float volume, bool run_over)
{
    if (hflags.prevent_ragdoll_sfx)
    {
        return;
    }
    int play_id;
    int delta_milliseconds;
    if (run_over)
    {
        play_id = audiolib_cue_play_3d(Human_vehicle_impact[1], &posp, nullptr, nullptr, nullptr);
        delta_milliseconds = -200;
    }
    else
    {
        if (collision_damage_timer.valid() && collision_damage_timer.time_since() < 400)
        {
            return;
        }
        play_id = audiolib_cue_play_3d(Human_vehicle_impact[0], &posp, nullptr, nullptr, nullptr);
        delta_milliseconds = 0;
    }
    collision_damage_timer.set(delta_milliseconds);
    audiolib_play_set_volume(play_id, volume, 100); // Magic number: a 100 ms fade
}

// 0x00A07DF0 (:11187)
bool human::has_special_voice_flags() const
{
    return hflags.activity_raid_required || hflags.activity_raid_optional || hflags.activity_ha_hostage || hflags.raid_introduction_lines || hflags.miner_persona_lines;
}

// 0x00A07E10 (:11346)
voice_line_handle human::get_current_voice_line_handle() const
{
    if (voice_time_since_finish.valid())
    {
        return INVALID_VOICE_LINE_HANDLE;
    }
    return queued_voice_line;
}

// 0x00A07E30 (:11381)
voice_lines human::get_current_voice_line() const
{
    if (voice_time_since_finish.valid())
    {
        return VOICE_LINE_NONE;
    }
    return situational_voice_line;
}

// 0x00A07EB0 (:11449)
int human::time_since_last_voice() const
{
    if (!voice_time_since_finish.valid())
    {
        return -1;
    }
    return voice_time_since_finish.time_since();
}

// 0x00A07ED0 (:11459)
const timestamp* human::get_voice_delay_timer() const
{
    return &voice_delay_time;
}

// 0x005DC100 (.h:1074)
const human_voice_persona* human::get_voice() const
{
    return voice;
}

// 0x00A07EE0 (:11467)
unsigned char human::get_physical_material(unsigned int, const vector*, const hkpWorldObject*) const
{
    return Human_flesh_material;
}

// 0x00A07EF0 (:11475)
unsigned char human::get_effect_material(unsigned int, const vector*, const hkpWorldObject*) const
{
    return Human_flesh_effect_material;
}

// 0x00A7C900 (:11480)
// The human info's values: hit points and knockdown hits (the multiplayer class's), the specialists' pushing and
// action node rules, a voice persona, the team (guerrillas start as civilians), the human container request and
// the character instance with a random height (fl_rand, plus or minus the height variation).
bool human::init_from_h_info()
{
    max_hit_points = Multiplayer ? multi_game_classes_get_default_class()->max_hit_points : h_info->get_max_hit_points();
    max_knockdown_hits = !Multiplayer ? h_info->get_max_knockdown_hits() : multi_game_classes_get_default_class()->max_knockdown_hits;
    knockdown_hits = (float)(!Multiplayer ? h_info->get_max_knockdown_hits() : multi_game_classes_get_default_class()->max_knockdown_hits);
    if (Multiplayer == true)
    {
        max_hit_points = (int)((float)((double)(float)Mp_server_options.generic_options.max_health / 100.0) * (float)max_hit_points);
    }
    initial_max_hit_points = max_hit_points;
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        const human_info* info = h_info;
        if (info->flags.guerrilla_tech || info->flags.guerrilla_rfc || info->flags.old_coot || info->flags.vip)
        {
            hflags.pushes_other_humans = 1;
        }
        if (info->flags.old_coot)
        {
            hflags.disallow_flinches_and_ragdolls = 1;
        }
        if (info->flags.guerrilla_rfc || info->flags.old_coot)
        {
            hflags.only_use_action_nodes = 1;
        }
    }
    voice = nullptr;
    if (!Multiplayer)
    {
        if (Human_voice_use_test_persona)
        {
            voice = human_voice_persona_from_name(Test_persona_names[h_info->default_team]);
        }
        if (voice == nullptr)
        {
            voice = h_info->voice_personas.pick_voice(hflags.activity_ha_hostage, hflags.activity_raid_required || hflags.activity_raid_optional, hflags.raid_introduction_lines,
                                                      hflags.miner_persona_lines);
            if (voice == nullptr)
            {
                // Original: an Int3 debug message.
            }
        }
    }
    human_teams team = h_info->default_team;
    if (team == HUMAN_TEAM_GUERILLA)
    {
        team = HUMAN_TEAM_CIVILIAN;
    }
    undercover_team = team;
    current_team = team;
    const unsigned int info_srid = h_info->srid;
    if (get_srid() == 0 && info_srid != 0)
    {
        request_streaming_resource(info_srid, STREAM_PRIORITY_HUMAN);
    }
    const human_info* info = h_info;
    const float r = fl_rand();
    // Magic number: the scale varies by up to the variation either way (x87 at double precision, rounded once).
    const float scale = (float)(((double)r - 0.5) * 2.0 * (double)info->height_scale_variation + (double)info->default_height_scale);
    char_inst = character_instance_create(scale, info->anim_set_name);
    if (char_inst == nullptr)
    {
        // Original: "Unable to create character instance for character '%s'!\n" into the thread's debug message
        // buffer, printed as a debug message.
        return false;
    }
    return true;
}

// 0x00A07F00 (:11565). LTCG: char_inst in ESI.
// In multiplayer and Wrecking Crew every human animates with the Player set.
static void human_modify_animation_set(character_instance* char_inst)
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return;
    }
    char_inst->vp->base_animation_set = anim_set_find("Player");
    char_inst->vp->animation_set = char_inst->vp->base_animation_set;
}

// 0x00ADB030 (:11582)
// Saves the human info's name and the importance, or creates the human from its properties: the human info (by
// pointer from the creator or by name), the flags (from the save), the human info's values and character instance,
// the riot shield variant, an ambient behavior (the first whose time of day, morale and chance fit), the nano
// effects, full health, the look-at, avoidance, action node, voice, inventory and movement state, the stand state,
// the importance, the team's hit point scaling (tech level for guerrillas and civilians, progression for
// marauders), and the squad loadout or the default inventory.
bool human::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(NPC_PROP_CINFO_NAME, h_info->name);
        pblock.set<object_importance_level>(HUMAN_PROP_IMPORTANCE, importance_level);
        return true;
    }
    h_info = nullptr;
    pblock.get<const human_info*>(HUMAN_PROP_CINFO, h_info);
    if (h_info == nullptr)
    {
        char name[64];
        if (pblock.get_optional(NPC_PROP_CINFO_NAME, name))
        {
            h_info = human_info_find_internal(name);
        }
        if (h_info == nullptr)
        {
            return false;
        }
    }
    hflags.reset();
    pf.extended_stuck_cumulative = 0;
    pblock.get_buf_copy_optional(HUMAN_PROP_HUMAN_FLAGS, (unsigned char*)&hflags, sizeof(hflags));
    inertial_motion.zero();
    state_speed_throttle = 1.0f;
    lifetime.set(0);
    if (!init_from_h_info())
    {
        return false;
    }
    human_modify_animation_set(char_inst);
    hflags.riot_shield = h_info->flags.riot_shield;
    if (hflags.riot_shield)
    {
        vanim_set_base_animation_set_variant(char_inst->vp, ANIM_SET_VARIANT_PERSONAL_SHIELD);
    }
    amb_prop_info = nullptr;
    int behavior_id = -1;
    ambient_behavior_group* group;
    if (pblock.get_optional<int>(HUMAN_PROP_AMBIENT_BEHAVIOR, behavior_id) && (group = ambient_behavior_from_id(behavior_id)) != nullptr)
    {
        // Magic number: the time of day in hours (the binary divides the minutes by the double 60.0).
        const float hour = (float)((double)(float)Gamewide_game_clock->m_minutes / 60.0 + (double)(float)Gamewide_game_clock->m_hour);
        const double morale = (double)gfm_get_district_morale(nullptr);
        const float morale_pct = (float)(morale / (double)gfm_get_district_morale_max(nullptr));
        // Magic number: the chances add up to 100.
        float chance = fl_rand_range(0.0f, 100.0f);
        for (int i = 0; i < group->num_behaviors; i++)
        {
            const ambient_behavior& b = group->behaviors[i];
            // Original: the morale has to be at least the maximum too (not at most).
            if ((double)hour >= (double)(float)b.day_start && (double)(float)b.day_end >= (double)hour && (double)morale_pct >= (double)b.morale_min && (double)morale_pct >= (double)b.morale_max)
            {
                const float ratio = h_info->flags.female ? b.male_female_ratio : (float)(2.0 - (double)b.male_female_ratio);
                if ((double)(float)b.occurance_chance * (double)ratio > (double)chance)
                {
                    if (b.variant_anim_set != ANIM_SET_VARIANT_NONE)
                    {
                        vanim_set_base_animation_set_variant(char_inst->vp, b.variant_anim_set);
                        hflags.maintain_ambient_props = 1;
                    }
                    if (b.amb_prop_info != nullptr)
                    {
                        amb_prop_info = b.amb_prop_info;
                    }
                    break;
                }
                chance = (float)((double)chance - (double)(float)b.occurance_chance);
            }
        }
    }
    for (nano_index = 0; nano_index < 16; nano_index++)
    {
        nano_mat_fx[nano_index] = 0xFFFFFFFF;
    }
    nano_index = 0;
    mat_fx_handle_for_head_skin = 0xFFFFFFFF;
    refill_health(1.0f);
    last_pos = pos;
    look_at_pos = pos + orient.fvec;
    aim_override_dir = ZERO_VECTOR;
    look_at_speed = 1.0f;
    is_turning = false;
    minimap_flags = 0;
    lean_dir.x = 0.0f;
    lean_dir.y = 1.0f;
    lean_dir.z = 0.0f;
    lean_weight = 0.0f;
    lean_disabled.set(0);
    facial_pose_ts.invalidate();
    if ((current_team == HUMAN_TEAM_CIVILIAN || current_team == HUMAN_TEAM_GUERILLA) && get_sub_type() == OT_SUB_HUMAN_NPC && !h_info->flags.guerrilla_tech)
    {
        facial_pose_ts.set(0);
    }
    avoidance_move_dir = ZERO_VECTOR;
    avoidance_hint_dir = ZERO_VECTOR;
    avoidance_request_human = 0xFFFFFFFF;
    avoidance_request_timer.invalidate();
    avoidance_pause_timer.invalidate();
    avoidance_check_timer.invalidate();
    do_ragdoll_ts.invalidate();
    action_node_data.action_node_set.clear();
    action_node_data.action_state = 0;
    action_node_data.action_sub_state = 0;
    action_node_data.exit_quick = true;
    action_node_data.current_action_node = 0xFFFFFFFF;
    action_node_data.state_duration.invalidate();
    cloth_sim = nullptr;
    queued_voice_line = INVALID_VOICE_LINE_HANDLE;
    situational_voice_line = VOICE_LINE_NONE;
    voice_time_since_finish.set(0);
    voice_delay_time.invalidate();
    set_voice_play_inst(-1);
    voice_delay_cb = nullptr;
    voice_delay_cb_data = nullptr;
    lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    offhand_projectile_handle = 0xFFFFFFFF;
    shield_handle = 0xFFFFFFFF;
    acknowledged_time.set(0);
    ragdoll_damaged.set(200);
    hflags.casts_shadows = 1;
    hflags.casts_drop_shadows = 1;
    hflags.xray_visible = 0;
    inventory = nullptr;
    reload_timer.invalidate();
    equipped_inv_item = nullptr;
    desired_equipped_inv_item = nullptr;
    last_equipped_weapon = nullptr;
    second_last_equipped_weapon = nullptr;
    grenade_weapon = nullptr;
    look_at_handle = 0xFFFFFFFF;
    custom_fire_animation = ANIMATION_ACTION_NONE;
    reported_timer.set(0);
    cur_state = HS_STANDARD;
    cur_movement_state = HMS_IDLE;
    movement_mode = HMM_NONE;
    movement_submode = HMSM_NONE;
    cur_stance = HST_STAND;
    walk_animation_speed_pct = fl_rand_range((float)((double)Min_ambient_walk_speed_pct / 100.0), (float)((double)Max_ambient_walk_speed_pct / 100.0));
    frametime_tally = 0.0f;
    actual_velocity_mag = 0.0f;
    max_speed = 0.0f;
    turnto_target = pos;
    ragdoll_num_collisions_with_walker = 0;
    cash = 0.0f;
    steering_vector.zero();
    // the turn limits of a ready human (-137 to 223 degrees)
    rotate_heading_left = -2.391061f;
    rotate_dir = HUMAN_ROTATE_SHORTEST;
    rotate_heading_right = 3.892019f;
    block_movement_anim = -1;
    block_steering_vector.zero();
    block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    block_steering_heading_offset = 0.0f;
    released_movement_anim = -1;
    ragdoll_source_human_handle = 0xFFFFFFFF;
    ragdoll_idx = -1;
    ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
    ragdoll_blend_pct = 0.0f;
    ragdoll_last_frame_velocity.zero();
    ragdoll_on_impact_min_vel = 0.0f;
    ragdoll_on_impact_time = 0.0f;
    last_valid_pos_before_ragdoll_index = 0;
    object_filter_handle = 0xFFFFFFFF;
    collision_damage_timer.set(0);
    current_collision_damage = 0.0f;
    // Magic numbers: the first bored idle after 50 to 90 s.
    bored_idle_timestamp.set_rand(50000, 90000);
    human_combat_init(&combat);
    human_path_data_init();
    render_alpha = 1.0f;
    camera_alpha_override = 1.0f;
    m_stealth_percent = 0.0f;
    vehicle_handle = 0xFFFFFFFF;
    reserved_vehicle_handle = 0xFFFFFFFF;
    ladder_handle = 0xFFFFFFFF;
    scripted_action_node_handle = 0xFFFFFFFF;
    turret_hold_animations.invalidate();
    ladder_slide_play_id = -1;
    vehicle_seat_idx = VSI_INVALID;
    pblock.get_optional<uint64_t>(HUMAN_PROP_NAV_CELL_HANDLE, pf.last_visited_nav_cell_handle);
    update_pos_and_orient(pos, orient, true);
    char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    // Magic numbers: the approximate box of a standing human until its bones are transformed.
    approx_local_bmin.x = -0.5f;
    approx_local_bmin.y = 0.0f;
    approx_local_bmin.z = -0.5f;
    approx_local_bmax.x = 0.5f;
    approx_local_bmax.y = 0.9f;
    approx_local_bmax.z = 0.5f;
    if (Look_at_player)
    {
        human_set_look_at_target(this, Player->get_handle(), Default_target_object_offset, false, -1.0f);
    }
    const human_teams team = current_team;
    importance_level = OBJECT_IMPORTANCE_LEVEL_NONE;
    if (!human_team_is_unaligned(team))
    {
        importance_level = OBJECT_IMPORTANCE_LEVEL_LOW;
    }
    pblock.get_optional<object_importance_level>(HUMAN_PROP_IMPORTANCE, importance_level);
    if (!Multiplayer && !wrecking_crew_is_active() && World.object_cast<player>(this) == nullptr)
    {
        bool scaled = true;
        if (team == HUMAN_TEAM_CIVILIAN || team == HUMAN_TEAM_GUERILLA)
        {
            // Magic number: each tech level adds an eighth of the hit points.
            const float tech = gfm_get_tech_level();
            max_hit_points = (int)(((double)tech * 0.125 + 1.0) * (double)(float)max_hit_points);
        }
        else if (team == HUMAN_TEAM_MARAUDER)
        {
            // Magic number: each progression level adds 5 % (the double of 0.05f).
            max_hit_points = (int)(((double)marauder_progression_level() * (double)0.05f + 1.0) * (double)max_hit_points);
        }
        else
        {
            scaled = false;
        }
        if (scaled)
        {
            refill_health(1.0f);
        }
    }
    int loadout;
    if (pblock.get_optional<int>(HUMAN_PROP_LOAD_OUT, loadout) && loadout != -1) // -1: no squad load out
    {
        squad_loadout_create(this, loadout);
    }
    else
    {
        human_set_default_inventory(this);
    }
    damage_percent = 0.0f;
    mp_flags.melee_primary_id = 0;
    mp_flags.set_running_attack(0);
    mp_flags.set_charge_weapon(0);
    mp_flags.throw_attack_id = 0;
    mp_flags.firearm_trigger_up_id = 0;
    mp_flags.firearm_trigger_down_id = 0;
    mp_flags.melee_firearm_id = 0;
    mp_flags.melee_tertiary_id = 0;
    mp_flags.melee_secondary_id = 0;
    *(unsigned int*)&animation_flags = 0;
    health_restore_min_hit_points = 0.0f;
    light_tags[0] = -1;
    light_effects[0] = 0xFFFFFFFF;
    light_tags[1] = -1;
    light_effects[1] = 0xFFFFFFFF;
    return true;
}

// 0x00A07F30 (:11963)
bool human::set_team(human_teams new_team)
{
    if ((int)new_team > -2 && (int)new_team < NUM_HUMAN_TEAMS)
    {
        current_team = new_team;
        return true;
    }
    return false;
}

// One prop item of a human (a character prop): created from its item info (in the human's container when it has
// none of its own) and attached to its character prop point.
static void human_create_prop_item(human* hp, const human_info::human_prop_info& prop, prop_type type, const obj_item_flags& flags)
{
    item* ip;
    if (prop.item->srid == 0)
    {
        obj_item_props props(prop.item, hp->h_info->srid, hp->pos, hp->orient, flags, COLLIDE_ITEM, -1.0f);
        ip = World.create_object<item>(props, hp->get_handle());
    }
    else
    {
        obj_item_props props(prop.item, hp->pos, hp->orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
        ip = World.create_object<item>(props, hp->get_handle());
    }
    if (ip != nullptr)
    {
        ip->m_prop_type = type;
        ip->owner_handle = hp->get_handle();
        if (ip->get_obj_type() == OT_ITEM && ip->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            weapon_stop_pickup_effect((weapon*)ip);
        }
        hp->set_child(ip, prop.character_prop_index, -1, false, true);
    }
}

// 0x00ABFA70 (:11989)
// Streams the human's resources in or out. In: the character's resources (the skin instance), x-ray, the head, the
// character proxy and the hit detection phantom (a capsule of 1.5 m around 0.5-1 m up), the state animation, the
// cloth, the hand, foot and limb bones with the IK joints, spine bending, the carried weapons on their carry
// points, the props of the human info (picked by chance the first time), the head morph targets, the breath timer,
// a hidden vehicle shown, the detour request, a render update and the light effects. Out: the ragdoll, the
// character's resources, the cloth, the head, the proxy and the phantom, the detour request; the dead are
// destroyed.
bool human::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        if (ragdoll_idx != -1)
        {
            havok_ragdoll_unbind_from_human(ragdoll_idx);
            ragdoll_idx = -1;
            ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
            ragdoll_source_human_handle = 0xFFFFFFFF;
            human_set_movement_mode(this, HMM_NONE, HMSM_NONE); // inlined in the binary
        }
        character_instance_resources_unloaded(char_inst);
        hflags.bones_transformed_this_frame = 0;
        char_inst->rdd.set_loaded(false);
        if (cloth_sim != nullptr)
        {
            cloth_sim_delete(cloth_sim);
            cloth_sim = nullptr;
        }
        if (rdd.is_loaded())
        {
            resource_dependent_data* data = rdd.get_data();
            if (data->head_manager_idx >= 0)
            {
                animlib_delete_anim_instance(data->head_aim_handle);
                if (data->head_renderable_instance_p != nullptr)
                {
                    data->head_renderable_instance_p->release();
                    data->head_renderable_instance_p = nullptr;
                }
            }
            if (data->controller != nullptr)
            {
                delete data->controller;
                data->controller = nullptr;
            }
            if (data->hit_detect_phantom != nullptr)
            {
                if ((data->hit_detect_phantom->getReferenceCount()) > 1)
                {
                    data->hit_detect_phantom->removeReference();
                }
                havok_remove_phantom(data->hit_detect_phantom, true);
                data->hit_detect_phantom = nullptr;
            }
        }
        if (nav_cell_detour_request_handle != 0xFFFFFFFF)
        {
            nav_cell_detour_remove_request(nav_cell_detour_request_handle);
            nav_cell_detour_request_handle = 0xFFFFFFFF;
        }
        rdd.set_loaded(false);
        if (is_killed())
        {
            World.destroy_object(this, false);
        }
        kill_light_effects(this);
        return true;
    }
    character* base_character = character_find(h_info->name, h_info->srid);
    if (Multiplayer == true && multi_game_mode_has_teams() && base_character->m_data != nullptr)
    {
        // the team's rim light color, scaled by the level's glow
        color team_color;
        multi_game_render_get_team_color(team_color, MTCT_RIM_LIGHTING, this);
        float glow = Mp_server_options.get_level()->glow_multiplier;
        if (!Multi_match_info.use_default_time_of_day)
        {
            glow = Mp_server_options.get_level()->alt_glow_multiplier;
        }
        glow = glow * 0.1f;
        rl_color_float rim;
        float b = (float)((double)(float)team_color.blue / 255.0 * (double)glow);
        if (b - 0.0f < 0.0f)
        {
            b = 0.0f;
        }
        if (b - 1.0f >= 0.0f)
        {
            b = 1.0f;
        }
        rim.green = (float)((double)(float)team_color.green / 255.0 * (double)glow);
        if (rim.green - 0.0f < 0.0f)
        {
            rim.green = 0.0f;
        }
        if (rim.green - 1.0f >= 0.0f)
        {
            rim.green = 1.0f;
        }
        rim.red = (float)((double)(float)team_color.red / 255.0 * (double)glow);
        if (rim.red - 0.0f < 0.0f)
        {
            rim.red = 0.0f;
        }
        if (rim.red - 1.0f >= 0.0f)
        {
            rim.red = 1.0f;
        }
        rim.blue = b;
        rim.alpha = 1.0f;
        character_mesh_modify_rimlight_colors(base_character->m_data, rim);
    }
    // Magic number: render category 0x12 (characters).
    if (!character_instance_resources_loaded(char_inst, base_character, 0x12))
    {
        return false;
    }
    if ((player*)this != Player)
    {
        human_set_xray_visible(this, true);
    }
    rdd.set_loaded(true);
    resource_dependent_data* data = rdd.get_data();
    const int head = human_hap_request_head(this);
    data->head_manager_idx = head;
    if (head != -1 && Head_managers[head].loaded && &Head_managers[head] != nullptr)
    {
        data->head_aim_handle = animlib_new_anim_instance_manager(Head_managers[head].head->rig, Head_managers[head].cp->bones_used_info.unique_id);
        animlib_set_parent_rig(data->head_aim_handle, char_inst->aim_handle);
        hflags.head_loaded = 1;
    }
    data->head_renderable_instance_p = nullptr;
    // Magic number: a new human transforms on its first frame.
    transform_frames_skipped = 50;
    player* pp = nullptr;
    if (get_obj_type() == OT_HUMAN && get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        pp = (player*)this;
        pp->havok_filter_group = Havok_filter->getNewSystemGroup();
    }
    const unsigned int filter = get_default_collision_filter(this);
    data->controller = new character_controller(get_handle(), filter);
    // the hit detection phantom: a capsule from 0.5 m to 1 m above the feet with a radius of 1.5 m
    hkVector4f vertex_a;
    vertex_a.set(0.0f, 0.5f, 0.0f, 0.0f);
    hkVector4f vertex_b;
    vertex_b.set(0.0f, 1.0f, 0.0f, 0.0f);
    hkpCapsuleShape* capsule = new hkpCapsuleShape(vertex_a, vertex_b, 1.5f);
    hkTransformf transform;
    transform.setIdentity();
    game_to_havok_vector(transform.getTranslation(), pos);
    data->hit_detect_phantom = new hkpSimpleShapePhantom(capsule, transform, COLLIDE_HUMAN_HIT_DETECT);
    havok_set_property(data->hit_detect_phantom, 0, this);
    havok_add_phantom(data->hit_detect_phantom, true);
    capsule->removeReference();
    animlib_clear_non_looping_animations(char_inst->aim_handle);
    human_set_and_process_animation_states(this);
    cloth_sim = cloth_sim_create(h_info->model_name, &pos, &orient, char_inst->aim_handle);
    if (cloth_sim != nullptr)
    {
        cloth_sim->half_collisions = 1;
    }
    equip_tag_index = animlib_find_character_tag(char_inst->aim_handle, "R-hand");
    equip_offhand_tag_index = animlib_find_character_tag(char_inst->aim_handle, "L-hand");
    left_foot_bone_index = animlib_find_character_bone(char_inst->aim_handle, "l-foot");
    right_foot_bone_index = animlib_find_character_bone(char_inst->aim_handle, "r-foot");
    root_bone_index = animlib_find_character_bone(char_inst->aim_handle, "root");
    left_shoulder_bone_index = animlib_find_character_bone(char_inst->aim_handle, "l-upperarmtwist1");
    right_shoulder_bone_index = animlib_find_character_bone(char_inst->aim_handle, "r-upperarmtwist1");
    left_hand_bone_index = equip_offhand_tag_index;
    right_hand_bone_index = equip_tag_index;
    left_hip_bone_index = animlib_find_character_bone(char_inst->aim_handle, "l-thigh");
    right_hip_bone_index = animlib_find_character_bone(char_inst->aim_handle, "r-thigh");
    left_foot_bone_index = animlib_find_character_bone(char_inst->aim_handle, "l-foot");
    right_foot_bone_index = animlib_find_character_bone(char_inst->aim_handle, "r-foot");
    ik_joints[ANIMATION_IK_LEFT_FOOT].anchor_tag = left_hip_bone_index;
    ik_joints[ANIMATION_IK_LEFT_FOOT].joint_tag = animlib_find_character_bone(char_inst->aim_handle, "l-calf");
    ik_joints[ANIMATION_IK_LEFT_FOOT].ik_tag = left_foot_bone_index;
    ik_joints[ANIMATION_IK_LEFT_FOOT].location = ANIMATION_IK_LEFT_FOOT;
    ik_joints[ANIMATION_IK_LEFT_FOOT].freeze_obj_handle = 0xFFFFFFFF;
    ik_joints[ANIMATION_IK_RIGHT_FOOT].anchor_tag = right_hip_bone_index;
    ik_joints[ANIMATION_IK_RIGHT_FOOT].joint_tag = animlib_find_character_bone(char_inst->aim_handle, "r-calf");
    ik_joints[ANIMATION_IK_RIGHT_FOOT].ik_tag = right_foot_bone_index;
    ik_joints[ANIMATION_IK_RIGHT_FOOT].location = ANIMATION_IK_RIGHT_FOOT;
    ik_joints[ANIMATION_IK_RIGHT_FOOT].freeze_obj_handle = 0xFFFFFFFF;
    ik_joints[ANIMATION_IK_LEFT_HAND].anchor_tag = left_shoulder_bone_index;
    ik_joints[ANIMATION_IK_LEFT_HAND].joint_tag = animlib_find_character_bone(char_inst->aim_handle, "l-foretwist");
    ik_joints[ANIMATION_IK_LEFT_HAND].ik_tag = equip_offhand_tag_index;
    ik_joints[ANIMATION_IK_LEFT_HAND].location = ANIMATION_IK_LEFT_HAND;
    ik_joints[ANIMATION_IK_LEFT_HAND].freeze_obj_handle = 0xFFFFFFFF;
    ik_joints[ANIMATION_IK_RIGHT_HAND].anchor_tag = right_shoulder_bone_index;
    ik_joints[ANIMATION_IK_RIGHT_HAND].joint_tag = animlib_find_character_bone(char_inst->aim_handle, "r-foretwist");
    ik_joints[ANIMATION_IK_RIGHT_HAND].ik_tag = equip_tag_index;
    ik_joints[ANIMATION_IK_RIGHT_HAND].location = ANIMATION_IK_RIGHT_HAND;
    ik_joints[ANIMATION_IK_RIGHT_HAND].freeze_obj_handle = 0xFFFFFFFF;
    human_spinebend_reset(this);
    if (pp == nullptr && h_info->flags.dan)
    {
        desired_equipped_inv_item = inventory;
        set_equipped_inv_item(inventory);
    }
    // the carried weapons go to their carry points (the equipped one is in the hand)
    for (inventory_item* iip = inventory; iip != nullptr;)
    {
        if (iip->weapon_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(iip->weapon_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && (weapon*)objp != get_equipped_weapon())
            {
                weapon* wp = (weapon*)objp;
                if (wp->w_info != nullptr)
                {
                    iip->attachment_prop = animlib_find_character_tag(char_inst->aim_handle, wp->w_info->attachment_point);
                    if (iip->attachment_prop == -1)
                    {
                        iip->attachment_prop = animlib_find_character_bone(char_inst->aim_handle, wp->w_info->attachment_point);
                    }
                }
                if (iip->attachment_prop >= 0)
                {
                    set_child(wp, iip->attachment_prop, wp->get_grip_tag_index(), false, true);
                    wp->hold_item(this, true);
                }
            }
        }
        iip = iip->next;
        if (iip == inventory)
        {
            break;
        }
    }
    if (!hflags.first_time_streamed)
    {
        // the human info's props: each general prop appears by its chance, then one of its props by theirs, then
        // one of that prop's sub props
        for (int gi = 0; gi < h_info->num_general_props; gi++)
        {
            const human_info::human_prop_general_info& general = h_info->general_props[gi];
            // Magic number: prop type 19 (the backpack) only in single player.
            if (!(!Multiplayer && !wrecking_crew_is_active()) && general.m_prop_type == (prop_type)19)
            {
                continue;
            }
            const float appear = fl_rand_range(0.0f, 1.0f);
            if (!(general.appearance_percent > 0.0f && appear <= general.appearance_percent))
            {
                continue;
            }
            float pick = fl_rand_range(0.0f, 1.0f);
            for (int pi = 0; pi < general.num_props; pi++)
            {
                const human_info::human_prop_info& prop = general.props[pi];
                pick = pick - prop.appearance_percent;
                if (prop.appearance_percent > 0.0f && pick <= 0.0f && prop.item != nullptr)
                {
                    obj_item_flags flags;
                    *(unsigned int*)&flags = 0;
                    flags.cast_drop_shadows = 1;
                    flags.cast_shadows = 1;
                    flags.character_prop = 1;
                    flags.update_attach_point = prop.item_prop_tag != nullptr;
                    flags.simulate = 1;
                    flags.no_physics = 1;
                    human_create_prop_item(this, prop, general.m_prop_type, flags);
                    float sub_pick = fl_rand_range(0.0f, 1.0f);
                    for (int si = 0; si < prop.num_sub_props; si++)
                    {
                        const human_info::human_prop_info& sub = prop.sub_props[si];
                        sub_pick = sub_pick - sub.appearance_percent;
                        if (sub.appearance_percent > 0.0f && sub_pick <= 0.0f && sub.item != nullptr)
                        {
                            // Original: these flags reuse a stack slot; only the bits set here are used.
                            obj_item_flags sub_flags;
                            *(unsigned int*)&sub_flags = 0;
                            sub_flags.no_physics = 1;
                            sub_flags.simulate = 1;
                            sub_flags.update_attach_point = sub.item_prop_tag != nullptr;
                            sub_flags.character_prop = 1;
                            sub_flags.cast_shadows = 1;
                            sub_flags.cast_drop_shadows = 1;
                            human_create_prop_item(this, sub, general.m_prop_type, sub_flags);
                        }
                    }
                    break;
                }
            }
        }
    }
    // the head morph targets (mouth, eyes and brows)
    memset(&head_morphs, 0, 0x7C);
    head_morphs.num_targets = 5;
    head_morphs.target[0].target_idx = 0x10;
    head_morphs.target[1].target_idx = 0x11;
    head_morphs.target[2].target_idx = 0x1A;
    head_morphs.target[3].target_idx = 0x1A;
    head_morphs.target[4].target_idx = 0x23;
    head_morphs.current_lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    // Magic number: the first breath within 4 s.
    breath_timer.set(int_rand_range(0, 4000));
    crouch_to_stand_test_ts.set(0);
    if (vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && vehicle_is_hidden((vehicle*)objp) && vehicle_is_resource_loaded(vehicle_handle) &&
            vehicle_all_passengers_have_resources_loaded(vehicle_handle))
        {
            vehicle_hide(vehicle_handle, false);
            vehicle_hide_all_passengers(vehicle_handle, false);
        }
    }
    nav_cell_detour_request_handle = nav_cell_detour_add_request(get_handle(), -1);
    hflags.first_time_streamed = 1;
    if (get_obj_type() != OT_NAVPOINT && get_obj_type() != OT_COVER_NODE)
    {
        // queue_render_update(this, flags, 0x10), inlined
        queue_render_update(this, obj_flags, 0x10 >> 1); // queue_render_update, inlined
    }
    if (h_info->light_effect_handle != 0xFFFFFFFF)
    {
        for (int i = 0; i < 2; i++)
        {
            if (h_info->light_tag_names[i] != nullptr)
            {
                light_tags[i] = animlib_find_character_tag(char_inst->aim_handle, h_info->light_tag_names[i]);
                if (light_tags[i] != -1 && game_clock_get_time_of_day() == GC_NIGHT)
                {
                    light_effects[i] = effects_play(h_info->light_effect_handle, get_handle(), light_tags[i], true, 0xFFFFFFFF, 1.0f, false);
                }
            }
        }
    }
    return true;
}

// 0x00AC84F0 (:12373)
void human::free_this()
{
    if (pf.path != nullptr)
    {
        bb_pf_free_path(&pf.path);
    }
    stop_voice(VOICE_LINE_PRIORITY_FORCE_ALWAYS);
    while (inventory != nullptr)
    {
        human_delete_item_from_inventory(this, inventory, true, true);
    }
    character_instance_delete(char_inst);
    equipped_inv_item = nullptr;
    cash = 0.0f;
    object::free_this();
}

// 0x00A07F90 (:12408)
void human::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x = bmin.x + pos.x;
    bmin.y = pos.y + bmin.y;
    bmin.z = pos.z + bmin.z;
    bmax.x = pos.x + bmax.x;
    bmax.y = pos.y + bmax.y;
    bmax.z = pos.z + bmax.z;
}

// 0x00A08060 (:12422)
// Magic numbers: the fixed local box of a human (1 m wide, 1.9 m tall).
void human::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -0.5f;
    bmin.y = 0.0f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 1.9f;
    bmax.z = 0.5f;
}

// 0x00A9AF70 (:12429)
void human::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object::update_pos_and_orient(update_pos, update_orient, set_havok_data);
    turnto_target = pos;
    if (set_havok_data && rdd.is_loaded())
    {
        rdd.get_data()->controller->set_pos(update_pos);
    }
}

// 0x00A080B0 (:12467)
// Magic number: a human weighs 90 kg.
float human::get_mass() const
{
    return 90.0f;
}

// 0x00AB1FB0 (:12472)
// When the children streamed in: props that follow an attachment tag go to their character prop point (found by
// the human info's prop or sub prop of the item), and shields are initialized.
void human::hierarchy_notify(hierarchy_notify_code code)
{
    if (code != HN_CHILDREN_STREAMED)
    {
        return;
    }
    for (object* child = child_ptr; child != nullptr;)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            if (ip->item_flags.update_attach_point)
            {
                bool done = false;
                for (int gi = 0; gi < h_info->num_general_props && !done; gi++)
                {
                    const human_info::human_prop_general_info& general = h_info->general_props[gi];
                    if (ip->m_prop_type != general.m_prop_type)
                    {
                        continue;
                    }
                    for (int pi = 0; pi < general.num_props && !done; pi++)
                    {
                        const human_info::human_prop_info& prop = general.props[pi];
                        const human_info::human_prop_info* match = nullptr;
                        if (ip->info == prop.item)
                        {
                            match = &prop;
                        }
                        else
                        {
                            for (int si = 0; si < prop.num_sub_props; si++)
                            {
                                if (ip->info == prop.sub_props[si].item)
                                {
                                    match = &prop.sub_props[si];
                                    break;
                                }
                            }
                        }
                        if (match == nullptr)
                        {
                            continue;
                        }
                        done = true;
                        if (match->item_prop_tag == nullptr)
                        {
                            // Original: returns without clearing the flag or visiting the other children.
                            return;
                        }
                        const int child_prop_point = ip->get_tag_index_from_name(match->item_prop_tag);
                        const int parent_prop_point = match->character_prop_index;
                        if (ip->get_parent() != get_handle() && !set_child(ip))
                        {
                            break;
                        }
                        if (ip->attach_info == nullptr)
                        {
                            ip->attach_info = Attach_info_pool.alloc(0);
                            if (ip->attach_info == nullptr)
                            {
                                break;
                            }
                        }
                        ip->attach_info->parent_prop_point = parent_prop_point;
                        ip->attach_info->child_prop_point = child_prop_point;
                        ip->attach_info->update_physics = 1;
                        ip->attach_info->updated = 0;
                        ip->attach_info->parent_handle = get_handle();
                        ip->attach_info->use_relative_transform = 0;
                    }
                }
                ip->item_flags.update_attach_point = 0;
            }
            // Magic number: 0x3000, the shield flags of an item info.
            if ((ip->info->flags & 0x3000) != 0 && !ip->item_flags.shield_initialized)
            {
                init_shield(ip);
            }
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A2E7D0 (:12553)
bool human_is_tied_up(const human* hp)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        return static_cast<const npc*>(hp)->npc_flags.tied_up != 0;
    }
    return false;
}

// 0x00A080C0 (:12569)
// The move direction flattened, the hint made perpendicular to it; a hint along the move direction becomes a random
// side.
void human::request_avoidance(const human* hp, const vector& request_dir, const vector& hint_dir)
{
    avoidance_move_dir = request_dir.copy_flatten_normalize_safe();
    avoidance_hint_dir = hint_dir;
    const vector& move = avoidance_move_dir;
    // The dot product is an unrounded double sum in rfg.exe; the rest rounds to float after every step.
    const float dot = (float)((double)move.y * (double)avoidance_hint_dir.y + (double)move.x * (double)avoidance_hint_dir.x + (double)move.z * (double)avoidance_hint_dir.z);
    avoidance_hint_dir.x = avoidance_hint_dir.x - move.x * dot;
    avoidance_hint_dir.y = avoidance_hint_dir.y - move.y * dot;
    avoidance_hint_dir.z = avoidance_hint_dir.z - move.z * dot;
    if (avoidance_hint_dir.is_null())
    {
        vector side;
        // Magic number: 51 in 100, the first side is slightly likelier.
        if (vrand() % 100 < 51)
        {
            side.x = move.z * UP_VECTOR.y - move.y * UP_VECTOR.z;
            side.y = move.x * UP_VECTOR.z - move.z * UP_VECTOR.x;
            side.z = move.y * UP_VECTOR.x - move.x * UP_VECTOR.y;
        }
        else
        {
            side.x = move.y * UP_VECTOR.z - move.z * UP_VECTOR.y;
            side.y = move.z * UP_VECTOR.x - move.x * UP_VECTOR.z;
            side.z = move.x * UP_VECTOR.y - move.y * UP_VECTOR.x;
        }
        avoidance_hint_dir = side;
    }
    else
    {
        avoidance_hint_dir.flatten_normalize_safe();
    }
    avoidance_request_human = hp != nullptr ? hp->get_handle() : 0xFFFFFFFF;
    avoidance_request_timer.set(2000); // Magic number: 2 s
    if (get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        static_cast<npc*>(this)->get_ai()->planner.request_replan();
    }
}

// 0x00A083C0 (:12600): no direction yet; update_avoidance works it out from the requester.
void human::request_avoidance(const unsigned int handlep)
{
    avoidance_move_dir = ZERO_VECTOR;
    avoidance_hint_dir = ZERO_VECTOR;
    avoidance_request_human = handlep;
    avoidance_request_timer.set(2000); // Magic number: 2 s
    if (get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        static_cast<npc*>(this)->get_ai()->planner.request_replan();
    }
}

// 0x00A53C40 (:12667)
vehicle* human_in_vehicle(const human* hp)
{
    if (hp == nullptr)
    {
        return nullptr;
    }
    vehicle* vp = nullptr;
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vp = (vehicle*)objp;
        }
    }
    return human_in_vehicle(hp, vp) ? vp : nullptr;
}

// 0x00A2E800 (:12676)
// Getting in or out of the vehicle (any interaction past reserving a seat, not being ejected).
bool human_in_vehicle_interaction(const human* hp, const vehicle* vp)
{
    if (hp == nullptr)
    {
        return false;
    }
    if (vp == nullptr)
    {
        if (hp->vehicle_handle == 0xFFFFFFFF)
        {
            return false;
        }
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            return false;
        }
        vp = (const vehicle*)objp;
    }
    else if (vp->get_handle() != hp->vehicle_handle)
    {
        return false;
    }
    if (hp->obj_flags.destroyed || vp->obj_flags.destroyed)
    {
        return false;
    }
    const int idx = ((vehicle*)vp)->vii.find_human(hp);
    if (idx < 0)
    {
        return false;
    }
    const vi_human_manager& m = vp->vii.humans[idx];
    return m.interaction != VI_INTERACTION_NONE && m.interaction != VI_INTERACTION_RESERVE && m.state != VI_STATE_EXIT_EJECT;
}

// 0x00A2E890 (:12721)
// Riding the vehicle: driving or sitting, or still entering it while the vehicle is not loaded (or stalled on its
// streaming).
bool human_in_vehicle(const human* hp, const vehicle* vp)
{
    if (hp == nullptr || vp == nullptr || hp->vehicle_handle != vp->get_handle() || hp->obj_flags.destroyed || vp->obj_flags.destroyed)
    {
        return false;
    }
    if (hp->cur_state != HS_VEHICLE_DRIVING && hp->cur_state != HS_VEHICLE_SITTING && vp->rdd.is_loaded())
    {
        const int idx = ((vehicle*)vp)->vii.find_human(hp);
        if (idx < 0 || vp->vii.humans[idx].state != VI_STATE_ENTER_STREAMING_STALL)
        {
            return false;
        }
    }
    return true;
}

// 0x00A2E930 (:12801)
// Entering or exiting the vehicle (the given interaction, or either).
bool human_in_vehicle_transition(const human* hp, const vehicle* vp, vi_interaction interaction)
{
    if (hp == nullptr)
    {
        return false;
    }
    if (vp == nullptr)
    {
        if (hp->vehicle_handle == 0xFFFFFFFF)
        {
            return false;
        }
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            return false;
        }
        vp = (const vehicle*)objp;
    }
    else if (vp->get_handle() != hp->vehicle_handle)
    {
        return false;
    }
    const int idx = ((vehicle*)vp)->vii.find_human(hp);
    if (idx >= 0)
    {
        const vi_human_manager& m = vp->vii.humans[idx];
        if ((int)m.interaction > VI_INTERACTION_RESERVE && (int)m.interaction < 4 && m.state != VI_STATE_EXIT_EJECT)
        {
            return interaction == VI_INTERACTION_NONE || m.interaction == interaction;
        }
    }
    return false;
}

// 0x00A2E9D0 (:12851)
bool human_in_players_vehicle(const human* const hp)
{
    if (hp != nullptr && hp->vehicle_handle != 0xFFFFFFFF)
    {
        return Player->vehicle_handle == hp->vehicle_handle;
    }
    return false;
}

// 0x00A085D0 (:12971)
bool human_is_ragdolled(const human* hp)
{
    const human_ragdoll_state rs = hp->ragdoll_state;
    return rs != HUMAN_RAGDOLL_STATE_BLEND_OUT && rs != HUMAN_RAGDOLL_STATE_NONE && rs != HUMAN_RAGDOLL_STATE_FLINCH &&
           (rs != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos);
}

// 0x00A2EA40 (:12986)
// A living human that is not ragdolled resists ragdolls with a backpack that takes no damage, at an ammo box, or with
// disallow_flinches_and_ragdolls (single player outside wrecking crew). No human counts as immune.
bool human_is_immune_to_ragdoll(const human* hp)
{
    if (hp == nullptr)
    {
        return true;
    }
    if (hp->is_killed() || human_is_ragdolled(hp))
    {
        return false;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((const player*)hp)->backpack != nullptr && ((const player*)hp)->backpack->get_backpack_abilities()->damage_taken_factor == 0.0f)
    {
        return true;
    }
    if (hp->cur_state == HS_AMMO_BOX)
    {
        return true;
    }
    return !Multiplayer && !wrecking_crew_is_active() && hp->hflags.disallow_flinches_and_ragdolls;
}

// 0x00A2EAE0 (:13018)
// Combat ready for at least 1.5 s more (not while getting in or out of a vehicle).
void human_set_combat_ready(human* hp, bool ready, int additional_time)
{
    if (hp == nullptr)
    {
        return;
    }
    if (human_in_vehicle_interaction(hp, nullptr) || !ready)
    {
        hp->combat.combat_ready_timestamp.invalidate();
        return;
    }
    // Magic number: 1500 ms.
    timestamp& ts = hp->combat.combat_ready_timestamp;
    if (!ts.valid() || ts.time_until() < additional_time + 1500)
    {
        ts.set(additional_time + 1500);
    }
}

// 0x00A7CC30 (:13203)
// The effect material and shape type of the walking surface under the human (a ray from 1 m above to 2 m below);
// the default material when there is none.
bool human_get_standing_on_material(const human* hp, unsigned char& mat_index, material_shape_type& shape_type)
{
    raycast_input in;
    in.start.x = hp->pos.x;
    in.start.y = hp->pos.y + 1.0f;
    in.start.z = hp->pos.z;
    in.end.x = hp->pos.x;
    in.end.y = hp->pos.y - 2.0f;
    in.end.z = in.start.z;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.m_filter_info = COLLIDE_RAY_WALKING_SURFACE_ONLY;
    raycast_output out;
    if (havok_raycast(in, out))
    {
        object* objp = World.object_from_handle(out.m_hits[0].hit_object_handle);
        if (objp != nullptr)
        {
            mat_index = out.m_hits[0].hit_effect_material;
            shape_type = rfg_material_get_shape_type(objp, out.m_hits[0].hit_havok_shape_key);
            return true;
        }
    }
    mat_index = Rfg_effect_material_default_index;
    shape_type = MATERIAL_EVENT_SHAPE_SOLID;
    return false;
}

// 0x00ACF520 (:13241)
// The human is dead: the death music (players), its voice stops, cover and ambient props are left, a vehicle
// passenger ragdolls out, the path is freed, the dead state; a ragdoll starts (or the death action plays where the
// death animation is not already playing), the dead collision filter, the hit phantom is removed (npcs), the corpse
// cleanup timer (3.5-4 min), a death or hard landing action, the movement stops, thrown (and in multiplayer
// equipped) weapons drop, nearby cover nodes count the death, and npcs post their death.
void human_switch_to_dead_state(human* hp)
{
    if (hp->get_sub_type() != OT_SUB_HUMAN_NPC && !Multiplayer)
    {
        gamemusic_event_occurred(GAMEMUSIC_PLAYER_DIED);
    }
    if (hp->voice_instance != -1)
    {
        audiolib_play_stop(hp->voice_instance);
        if (audiolib_play_get_state(-1, nullptr) != AUDIOLIB_ERR_NONE)
        {
            hp->voice_time_since_finish.set(0);
        }
        hp->voice_instance = -1;
    }
    human_combat_cover_abandon_node(hp);
    hp->hflags.maintain_ambient_props = 0;
    amb_prop_detach_props(hp, false);
    if (hp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle_interaction_ragdoll(hp, 0xFFFFFFFF);
        }
    }
    if (hp->pf.path != nullptr)
    {
        bb_pf_free_path(&hp->pf.path);
    }
    const human_states old_state = hp->cur_state;
    hp->hflags.override_default_anim_state = 0;
    hp->hflags.override_default_fire_anim = 0;
    hp->custom_fire_animation = ANIMATION_ACTION_NONE;
    hp->move_speed = -1.0f;
    hp->cur_state = HS_DEAD;
    hp->cur_movement_state = HMS_IDLE;
    const bool death_playing = vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_DEATH);
    bool ragdolled = !(hp->ragdoll_idx < 0 || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT);
    if (death_playing || !ragdolled)
    {
        if (!death_playing)
        {
            ragdolled = human_ragdoll_start(hp, true, 0xFFFFFFFF);
        }
        if (!ragdolled && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
        {
            object_set_visibility_plus_children(hp, false);
            if (hp->rdd.is_loaded())
            {
                const unsigned int filter = get_default_collision_filter(hp);
                hkpShapePhantom* phantom;
                if (hp->rdd.is_loaded() && (phantom = hp->rdd.get_data()->controller->m_phantom) != nullptr)
                {
                    phantom->getCollidableRw()->getBroadPhaseHandle()->m_collisionFilterInfo = filter;
                    havok_phantom_update_collision_filter(phantom, true);
                }
            }
        }
    }
    if (hp->rdd.is_loaded() && hp->rdd.get_data()->hit_detect_phantom != nullptr && hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        hkpSimpleShapePhantom* phantom = hp->rdd.get_data()->hit_detect_phantom;
        if ((phantom->getReferenceCount()) > 1)
        {
            phantom->removeReference();
        }
        havok_remove_phantom(hp->rdd.get_data()->hit_detect_phantom, true);
        hp->rdd.get_data()->hit_detect_phantom = nullptr;
    }
    if (localization_allow_ragdolls())
    {
        // Magic numbers: corpses are cleaned up after 210 to 240 s.
        hp->corpse_cleanup_timer.set_rand(210000, 240000);
    }
    else
    {
        hp->corpse_cleanup_timer.set(Human_corpse_cleanup_germany);
    }
    if (!death_playing && (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_FLINCH))
    {
        animlib_clear_non_looping_animations(hp->char_inst->aim_handle);
        // Magic number: play flags 0x4024.
        hp->char_inst->play_action(old_state == HS_FALL ? ANIMATION_ACTION_LAND_HARD_SLAM : ANIMATION_ACTION_DEATH, ANIM_ALIAS_LAND, 1.0f, 0x4024);
    }
    if (hp->ragdoll_idx == -1 && !(hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER))
    {
        if ((!hp->hflags.is_on_ladder || hp->get_sub_type() != OT_SUB_HUMAN_NPC) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            // human_set_movement_mode(hp, HMM_NONE, HMSM_NONE), inlined
            if (hp->movement_mode != HMM_NONE)
            {
                hp->steering_vector.zero();
                hp->previous_movement_mode = hp->movement_mode;
                hp->movement_mode = HMM_NONE;
            }
            const human_move_modes mode = hp->movement_mode;
            hp->movement_submode = HMSM_NONE;
            if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
            {
                hp->pf.dest_check_timer.invalidate();
                if (hp->pf.path != nullptr)
                {
                    bb_pf_free_path(&hp->pf.path);
                }
            }
        }
    }
    if (!Multiplayer)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            human_weapons_drop_thrown_weapon(hp);
        }
    }
    else
    {
        human_weapons_drop_thrown_weapon(hp);
        human_weapons_drop_equipped_weapon(hp);
    }
    // Magic number: cover nodes within 1 m count the death.
    const int num_nodes = World.get_num_objects<cover_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        cover_node* cn = World.get_object<cover_node>(i);
        if (vec_dist_squared(hp->pos, cn->pos) <= 1.0f)
        {
            cn->increment_death_count();
        }
    }
    object* self = World.object_from_handle(hp->get_handle());
    if (self == nullptr || self->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        human_dead_event ev;
        ev.human_handle = hp->get_handle();
        gem_post_event(ev);
        if (hp->nav_cell_detour_request_handle != 0xFFFFFFFF)
        {
            nav_cell_detour_remove_request(hp->nav_cell_detour_request_handle);
            hp->nav_cell_detour_request_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00A08750 (:13421)
void human_set_look_at_target(human* hp, const vector& target, bool should_aim_at, float spinebend_rate)
{
    if (hp == nullptr)
    {
        return;
    }
    hp->look_at_pos = target;
    hp->hflags.aim_at_pos = should_aim_at;
    if (spinebend_rate > 0.0f)
    {
        hp->look_at_speed = spinebend_rate;
    }
    hp->look_at_handle = 0xFFFFFFFF;
}

// 0x00A7CD70 (:13445)
// Looks at an object (with an offset in its frame); an npc starting to aim at the player begins its weapon drift,
// and its facial pose changes.
void human_set_look_at_target(human* hp, unsigned int handle, const vector& offset, bool should_aim_at, float spinebend_rate)
{
    if (handle == 0xFFFFFFFF || hp == nullptr)
    {
        return;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        object* target = World.object_from_handle(handle);
        if (target != nullptr && target->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            if ((should_aim_at || human_combat_is_manning_turret(hp)) && (hp->look_at_handle != handle || (!hp->hflags.aim_at_pos && should_aim_at)))
            {
                ((npc*)hp)->weapon_drift_start_acquire();
            }
            if (hp->facial_pose_ts.valid())
            {
                hp->facial_pose_ts.set(0);
            }
        }
    }
    hp->look_at_pos = offset;
    hp->look_at_handle = handle;
    hp->hflags.aim_at_pos = should_aim_at;
    if (spinebend_rate > 0.0f)
    {
        hp->look_at_speed = spinebend_rate;
    }
}

// 0x00A9B0E0 (:13485)
// The direction from the eyes to what the human looks at: a position, or an object (a human's head, another
// object's box center) plus the offset in its frame; very close to a human (under 0.8 m) the head height is assumed
// (1.7 m). Npcs do not look down more steeply than 60 degrees.
void human_get_look_at_vector_from_eye_pos(const human* hp, vector eye_pos, vector& look_at_vec)
{
    look_at_vec = hp->orient.fvec;
    if (hp->look_at_handle == 0xFFFFFFFF && hp->look_at_pos.is_null())
    {
        return;
    }
    vector offset = hp->look_at_pos;
    bool close = false;
    if (hp->look_at_handle == 0xFFFFFFFF)
    {
        look_at_vec = offset - eye_pos;
    }
    else
    {
        object* target = World.object_from_handle(hp->look_at_handle);
        if (target == nullptr)
        {
            return;
        }
        vector local_target;
        if (target->get_obj_type() == OT_HUMAN)
        {
            const float dx = hp->pos.x - target->pos.x;
            const float dz = hp->pos.z - target->pos.z;
            // Magic number: closer than 0.8 m (horizontally).
            const float dist = sqrtf(dx * dx + dz * dz);
            if (dist < 0.8f)
            {
                eye_pos.x = hp->pos.x;
                eye_pos.z = hp->pos.z;
            }
            close = dist < 0.8f;
            human_get_head_pos((human*)target, local_target);
            local_target = local_target - target->pos;
        }
        else
        {
            vector bmin;
            vector bmax;
            target->get_local_bbox(bmin, bmax);
            local_target.y = (bmin.y + bmax.y) * 0.5f;
            local_target.x = (bmax.x + bmin.x) * 0.5f;
            local_target.z = (bmin.z + bmax.z) * 0.5f;
        }
        offset.unrotate(target->orient);
        local_target.x = local_target.x + target->pos.x + offset.x;
        local_target.y = local_target.y + target->pos.y + offset.y;
        local_target.z = local_target.z + target->pos.z + offset.z;
        if (close)
        {
            // Magic number: 1.7 m, the eye height of a standing human.
            look_at_vec.x = local_target.x - hp->pos.x;
            look_at_vec.y = local_target.y - (hp->pos.y + 1.7f);
            look_at_vec.z = local_target.z - hp->pos.z;
        }
        else
        {
            look_at_vec = local_target - eye_pos;
        }
    }
    // Magic number: 0.577, the tangent of 30 degrees (60 degrees below the horizon).
    if (hp->get_sub_type() != OT_SUB_HUMAN_PLAYER && look_at_vec.y < 0.0f && sqrtf(look_at_vec.x * look_at_vec.x + look_at_vec.z * look_at_vec.z) / fabsf(look_at_vec.y) < 0.577f && !close)
    {
        look_at_vec = hp->orient.fvec;
    }
}

// 0x00AB6020 (:13558)
void human_get_look_at_vector(const human* hp, vector& look_at_vec)
{
    if (hp != nullptr)
    {
        vector eye_pos;
        human_get_eye_pos_orient(hp, eye_pos, nullptr);
        human_get_look_at_vector_from_eye_pos(hp, eye_pos, look_at_vec);
    }
}

// 0x00A7CE60 (:13575)
// The head bone's position when the bones were transformed this frame, else an estimate from the stance.
void human_get_head_pos(const human* hp, vector& head_pos)
{
    if (hp->hflags.bones_transformed_this_frame)
    {
        animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->head_bone_index, hp->pos, hp->orient, head_pos, nullptr);
        return;
    }
    if ((hp->cur_state == HS_VEHICLE_SITTING || hp->cur_state == HS_VEHICLE_DRIVING) && human_combat_get_manned_turret(hp) == nullptr)
    {
        // Magic number: 0.75 m up the human's up vector.
        head_pos.x = hp->pos.x + hp->orient.uvec.x * 0.75f;
        head_pos.y = hp->pos.y + hp->orient.uvec.y * 0.75f;
        head_pos.z = hp->pos.z + hp->orient.uvec.z * 0.75f;
    }
    // Original: the stance estimate overwrites the vehicle seat estimate above.
    human_get_head_pos_from_stance(hp, head_pos);
}

// 0x00AB2260 (:13604)
// The eye position (between the eye bones, or one of them) and orientation; without eye bones the head plus the eye
// offset in the head's frame; without instanced bones (or at bone LOD 2) the stance estimate looking at the
// look-at target.
void human_get_eye_pos_orient(const human* hp, vector& eye_pos, matrix* eye_orient)
{
    // function static of the original, initialized on first use ($S7 bit 0)
    static const vector head_eye_offset(0.0f, 0.08f, 0.12f);
    matrix43* matrices = nullptr;
    if (hp->char_inst->aim_handle != -1 && animlib_get_instanced_model_matrices(hp->char_inst->aim_handle, &matrices) && hp->bone_lod_level != 2 && matrices != nullptr)
    {
        const int eye_l = hp->h_info->eye_l_bone_index;
        const int eye_r = hp->h_info->eye_r_bone_index;
        if (eye_l != -1 || eye_r != -1)
        {
            const int aim_handle = hp->char_inst->aim_handle;
            if (aim_handle >= 0)
            {
                if (eye_l == -1)
                {
                    animlib_get_tag_world_coords(aim_handle, eye_r, hp->pos, hp->orient, eye_pos, eye_orient);
                    return;
                }
                if (eye_r == -1)
                {
                    animlib_get_tag_world_coords(aim_handle, eye_l, hp->pos, hp->orient, eye_pos, eye_orient);
                    return;
                }
                vector left;
                vector right;
                animlib_get_tag_world_coords(hp->char_inst->aim_handle, eye_l, hp->pos, hp->orient, left, eye_orient);
                animlib_get_tag_world_coords(hp->char_inst->aim_handle, eye_r, hp->pos, hp->orient, right, nullptr);
                eye_pos.x = (right.x + left.x) * 0.5f;
                eye_pos.y = (right.y + left.y) * 0.5f;
                eye_pos.z = (right.z + left.z) * 0.5f;
                return;
            }
        }
        matrix head_orient;
        matrix* orient = eye_orient != nullptr ? eye_orient : &head_orient;
        human_get_head_pos_orient(hp, eye_pos, *orient);
        eye_pos.x = eye_pos.x + orient->rvec.x * head_eye_offset.x + orient->uvec.x * head_eye_offset.y + orient->fvec.x * head_eye_offset.z;
        eye_pos.y = eye_pos.y + orient->rvec.y * head_eye_offset.x + orient->uvec.y * head_eye_offset.y + orient->fvec.y * head_eye_offset.z;
        eye_pos.z = eye_pos.z + orient->rvec.z * head_eye_offset.x + orient->uvec.z * head_eye_offset.y + orient->fvec.z * head_eye_offset.z;
        return;
    }
    human_get_head_pos_from_stance(hp, eye_pos);
    eye_pos.y = eye_pos.y + head_eye_offset.y;
    if (eye_orient != nullptr)
    {
        vector look;
        human_get_look_at_vector_from_eye_pos(hp, eye_pos, look);
        eye_orient->make(look);
    }
}

// 0x00A9B5D0 (:13701)
// The head position and orientation (looking at the look-at target): the head bone when transformed, the seat of a loaded
// vehicle, or the stance estimate.
void human_get_head_pos_orient(const human* hp, vector& head_pos, matrix& head_orient)
{
    if (hp->hflags.bones_transformed_this_frame)
    {
        animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->head_bone_index, hp->pos, hp->orient, head_pos, &head_orient);
        return;
    }
    bool seated = false;
    if ((hp->cur_state == HS_VEHICLE_DRIVING || hp->cur_state == HS_VEHICLE_SITTING) && hp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && ((vehicle*)objp)->rdd.is_loaded())
        {
            head_pos.x = hp->pos.x + hp->orient.uvec.x * 0.75f;
            head_pos.y = hp->pos.y + hp->orient.uvec.y * 0.75f;
            head_pos.z = hp->pos.z + hp->orient.uvec.z * 0.75f;
            head_orient = hp->orient;
            seated = true;
        }
    }
    if (!seated)
    {
        human_get_head_pos_from_stance(hp, head_pos);
        head_orient = hp->orient;
    }
    // Magic number: the eyes are 8 cm above the head estimate.
    head_pos.y = head_pos.y + 0.08f;
    vector look;
    human_get_look_at_vector_from_eye_pos(hp, head_pos, look);
    if (!look.is_null())
    {
        head_orient.make(look);
    }
}

// 0x00A9B7E0 (:13812)
void human_hide(human* hp, bool hide)
{
    if (hp == nullptr || (hp->hflags.hidden != 0) == hide)
    {
        return;
    }
    hp->hflags.hidden = hide;
    hp->set_is_visible(!hide);
    if (hp->rdd.is_loaded())
    {
        // get_default_collision_filter answers for the new hidden flag.
        const unsigned int filter = get_default_collision_filter(hp);
        hkpShapePhantom* phantom;
        if (hp->rdd.is_loaded() && (phantom = hp->rdd.get_data()->controller->m_phantom) != nullptr)
        {
            phantom->getCollidableRw()->getBroadPhaseHandle()->m_collisionFilterInfo = filter;
            havok_phantom_update_collision_filter(phantom, true);
        }
    }
}

// 0x00A7CF80 (:13850)
void human_set_cast_shadows(human* hp, bool cast_shadows, bool include_children)
{
    hp->hflags.casts_shadows = cast_shadows;
    hp->set_render_flags(8); // Magic number: the shadow settings changed (the same flag for both kinds).
    if (!include_children)
    {
        return;
    }
    for (object* child = hp->child_ptr; child != nullptr; child = child->child_next)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            static_cast<item*>(child)->item_flags.cast_shadows = cast_shadows;
            child->set_render_flags(8);
        }
        if (child->child_next == hp->child_ptr)
        {
            break; // the children are a ring
        }
    }
}

// 0x00A7D050 (:13874)
void human_set_cast_drop_shadows(human* hp, bool cast_drop_shadows, bool include_children)
{
    hp->hflags.casts_drop_shadows = cast_drop_shadows;
    hp->set_render_flags(8); // Magic number: the shadow settings changed (the same flag for both kinds).
    if (!include_children)
    {
        return;
    }
    for (object* child = hp->child_ptr; child != nullptr; child = child->child_next)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            static_cast<item*>(child)->item_flags.cast_drop_shadows = cast_drop_shadows;
            child->set_render_flags(8);
        }
        if (child->child_next == hp->child_ptr)
        {
            break; // the children are a ring
        }
    }
}

// 0x00A7D120 (:13892)
// The x-ray look of the human's item children (recursively); changed items update their render.
void xray_my_children(object* the_parent, bool xray_visible, unsigned char xray_material, const unsigned char xray_alpha, bool force_dirty)
{
    for (object* child = the_parent->child_ptr; child != nullptr;)
    {
        bool dirty = false;
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            dirty = force_dirty || ip->xray_material != xray_material || ip->xray_render_alpha != xray_alpha;
            ip->xray_material = xray_material;
            ip->xray_render_alpha = xray_alpha;
            ip->item_flags.xray_visible = xray_visible;
            if (dirty)
            {
                // queue_render_update(ip, flags, 0x10), inlined
                queue_render_update(child, child->obj_flags, 0x10 >> 1); // queue_render_update, inlined
            }
        }
        xray_my_children(child, xray_visible, xray_material, xray_alpha, dirty);
        child = child->child_next;
        if (child == the_parent->child_ptr)
        {
            return;
        }
    }
}

// 0x00A7D200 (:13919)
// The x-ray look of a human (and its items): friendly to the local player or an enemy.
void human_set_xray_visible(human* hp, bool xray_visible)
{
    const xray_teams team = Player == nullptr || Player == (player*)hp || human_teams_are_friendly(Player, hp) ? XRAY_TEAM_FRIENDLY : XRAY_TEAM_ENEMY;
    hp->xray_material = xray_effect_get_xray_material(team);
    hp->hflags.xray_visible = xray_visible;
    if (hp->get_obj_type() != OT_NAVPOINT && hp->get_obj_type() != OT_COVER_NODE)
    {
        queue_render_update(hp, hp->obj_flags, 0x10 >> 1); // queue_render_update, inlined
    }
    xray_my_children(hp, xray_visible, hp->xray_material, 0xFF, true);
}

// 0x00A53D90 (:13978)
// The character proxy collides by the human's state (get_default_collision_filter), or not at all.
void human_update_collision_filter(human* hp, bool enable_collisions)
{
    if (hp == nullptr || !hp->rdd.is_loaded())
    {
        return;
    }
    const unsigned int filter = enable_collisions ? get_default_collision_filter(hp) : COLLIDE_NO_COLLISION;
    hkpShapePhantom* phantom = hp->rdd.get_data()->controller->m_phantom;
    if (phantom != nullptr)
    {
        phantom->getCollidableRw()->getBroadPhaseHandle()->m_collisionFilterInfo = filter;
        havok_phantom_update_collision_filter(phantom, true);
    }
}

// 0x00A2EB50 (:14011)
// The character proxy's collision filter; false while the human has none.
bool human_get_collision_filter(unsigned int& filter_type, const human* hp)
{
    if (hp == nullptr || !hp->rdd.is_loaded())
    {
        return false;
    }
    const hkpShapePhantom* phantom = hp->rdd.get_data()->controller->m_phantom;
    if (phantom == nullptr)
    {
        return false;
    }
    filter_type = phantom->getCollidable()->getBroadPhaseHandle()->m_collisionFilterInfo;
    return true;
}

// 0x00A087C0 (:14052)
// Touching a mover or a vehicle.
bool human_contacting_non_stationary_object(const human* p_human)
{
    const object_contact_info* contacts = p_human != nullptr ? &p_human->contact_info : nullptr;
    for (contact_node* node = contacts->m_contact_list; node != nullptr;)
    {
        object* objp = World.object_from_handle(node->m_contacted_object);
        if (objp->get_obj_type() == OT_MOVER || objp->get_obj_type() == OT_VEHICLE)
        {
            return true;
        }
        node = node->next;
        if (node == contacts->m_contact_list)
        {
            return false;
        }
    }
    return false;
}

// The rotation that turns direction a onto direction b (both unit vectors): around their cross product by the
// arcsine of its length (the up axis and no angle when they are parallel), as ik_joint::do_ik builds it twice.
static inline void ik_rotation_between(const vector& a, const vector& b, vector& axis, float& angle)
{
    vector c;
    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - b.z * a.x;
    c.z = b.y * a.x - a.y * b.x;
    float len = sqrtf(c.x * c.x + c.y * c.y + c.z * c.z);
    if (len >= 1e-05f)
    {
        const float inv = 1.0f / len;
        axis.y = c.y * inv;
        axis.x = c.x * inv;
        axis.z = c.z * inv;
        if (len - 1.0f >= 0.0f)
        {
            len = 1.0f;
        }
        angle = (float)asin((double)len);
    }
    else
    {
        axis = UP_VECTOR;
        angle = 0.0f;
    }
}

// 0x00A08820 (:14088)
// Two-bone IK: solves the anchor-joint-end chain for the target (in the human's frame), then registers the anchor's
// rotation (force) and the joint's rotation as bone modifiers weighted by the IK strength.
void ik_joint::do_ik(human* hp, const vector& target_pos)
{
    vector anchor_pos;
    matrix anchor_orient;
    vector joint_pos;
    matrix joint_orient;
    vector end_pos;
    matrix end_orient;
    hp->get_tag_local_coords(anchor_tag, anchor_pos, &anchor_orient);
    hp->get_tag_local_coords(joint_tag, joint_pos, &joint_orient);
    hp->get_tag_local_coords(ik_tag, end_pos, &end_orient);
    vector new_joint;
    vector new_end;
    ik_2link_solve(anchor_pos, joint_pos, end_pos, target_pos, 0.0f, new_joint, new_end);
    int num_bones;
    int bone_list[101];
    // Magic number: up to 100 bones below the anchor.
    animlib_get_bone_tree_list(hp->char_inst->aim_handle, anchor_tag, 100, num_bones, bone_list);
    animlib_invalidate_cache_bone_list(hp->char_inst->aim_handle, num_bones, bone_list, true, true);
    // the lower bone's direction in the joint's frame
    const vector lower_local = (end_pos - joint_pos).copy_normalize().copy_rotate(joint_orient);
    const vector upper_old = (joint_pos - anchor_pos).copy_normalize();
    const vector upper_new = (new_joint - anchor_pos).copy_normalize();
    const vector lower_new = (new_end - new_joint).copy_normalize();
    vector axis;
    float angle;
    ik_rotation_between(upper_old, upper_new, axis, angle);
    matrix rot;
    rot.set(axis, angle);
    const matrix anchor_new = anchor_orient * rot;
    const matrix joint_new = joint_orient * anchor_orient.copy_transpose() * anchor_new;
    const vector lower_current = lower_local.copy_unrotate(joint_new);
    ik_rotation_between(lower_current, lower_new, axis, angle);
    const matrix joint_rot(axis, angle);
    const matrix joint_final = joint_new * joint_rot;
    if (ik_strength > 0.0f)
    {
        // Magic numbers: storage slots 9/10 + 2 per IK location.
        animlib_register_bone_modifier(hp->char_inst->aim_handle, BONE_MODIFIER_FORCE, anchor_tag, location * 2 + 9, const_cast<matrix&>(anchor_new), &anchor_orient, ik_strength);
        animlib_register_bone_modifier(hp->char_inst->aim_handle, BONE_MODIFIER_ROTATE, joint_tag, location * 2 + 10, const_cast<matrix&>(joint_final), const_cast<matrix*>(&joint_new), ik_strength);
    }
}

// 0x00A09070 (:14245)
// The last IK trigger of this joint's location that elapsed in the animation (or all of them once it finished).
ik_trigger* ik_joint::find_elapsed_ik_trigger(human* hp, anim* ik_animp, unsigned int handle)
{
    ik_trigger* found = nullptr;
    for (int i = 0; i < (int)ik_animp->num_ik_triggers; i++)
    {
        if ((animlib_has_frame_elapsed(hp->char_inst->aim_handle, handle, (unsigned int)ik_animp->ik_triggers[i].frame) && (unsigned int)ik_animp->ik_triggers[i].location == (unsigned int)location) ||
            (animlib_is_anim_finished(hp->char_inst->aim_handle, handle) && (unsigned int)ik_animp->ik_triggers[i].location == (unsigned int)location))
        {
            found = &ik_animp->ik_triggers[i];
        }
    }
    return found;
}

// 0x00A09110 (:14257)
// The IK trigger in effect for this joint: an elapsed one of the playing actions, else the first one of the run
// blend tree's (or the secondary state's) animation.
ik_trigger* ik_joint::find_ik_trigger(human* hp)
{
    v_animate* vp = hp->char_inst->vp;
    for (int i = 0; i < vp->max_simultaneous_anims; i++)
    {
        const v_anim& a = vp->anims[i];
        if (!a.used)
        {
            break;
        }
        if (!a.is_state && !a.is_pose && a.anim_handle != -1)
        {
            anim* ap = anim_get_anim_from_handle(a.anim_handle);
            if (ap != nullptr)
            {
                ik_trigger* trigger = find_elapsed_ik_trigger(hp, ap, a.instance);
                if (trigger != nullptr)
                {
                    return trigger;
                }
            }
        }
    }
    // Magic numbers: 0x409 and 0x40A, the state blend tree slots.
    anim* ap = anim_get_anim_from_handle(vanim_get_anim_handle(vp, 0x409));
    if (ap != nullptr)
    {
        for (int i = 0; i < (int)ap->num_ik_triggers; i++)
        {
            if ((unsigned int)ap->ik_triggers[i].location == (unsigned int)location)
            {
                return &ap->ik_triggers[i];
            }
        }
    }
    if (vp->secondary_state != nullptr)
    {
        ap = anim_get_anim_from_handle(vanim_get_anim_handle(vp, 0x40A));
        if (ap != nullptr)
        {
            for (int i = 0; i < (int)ap->num_ik_triggers; i++)
            {
                if ((unsigned int)ap->ik_triggers[i].location == (unsigned int)location)
                {
                    return &ap->ik_triggers[i];
                }
            }
        }
    }
    return nullptr;
}

// 0x00A7D2B0 (:14305)
// The IK target in the human's frame: the weapon's secondary grip (two-handed firearms, and the right hand on a melee
// weapon), the turret's grip tags, or the frozen position on an object.
bool ik_joint::get_ik_pos(human* hp, vector& ik_pos, human_ik_reason reason)
{
    switch (reason)
    {
    case IK_REASON_TWO_HANDED_WEAPON:
    {
        weapon* wp = hp->get_equipped_weapon();
        wp->adjust_transforms();
        if (wp->get_secondary_grip_tag_index() < 0)
        {
            return false;
        }
        wp->get_tag_world_coords(wp->get_secondary_grip_tag_index(), ik_pos, nullptr, nullptr, nullptr);
        break;
    }
    case IK_REASON_USING_TURRET:
    {
        turret* tp;
        if (vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_EQUIP) || (tp = human_combat_get_manned_turret(hp)) == nullptr)
        {
            return false;
        }
        // Magic numbers: turret tags 5 (right grip) and 4 (left grip).
        tp->get_tag_world_coords(location == ANIMATION_IK_RIGHT_HAND ? 5 : 4, ik_pos, nullptr, nullptr, nullptr);
        break;
    }
    case IK_REASON_RIGHT_HAND_TO_WEAPON:
    {
        hp->adjust_child_transforms(false, false);
        weapon* wp = hp->get_equipped_weapon();
        wp->get_tag_world_coords(wp->get_secondary_grip_tag_index(), ik_pos, nullptr, nullptr, nullptr);
        break;
    }
    case IK_REASON_GENERIC_OBJECT:
    {
        ik_pos = freeze_offset_pos;
        if (hp->get_handle() == freeze_obj_handle)
        {
            return true;
        }
        object* objp = World.object_from_handle(freeze_obj_handle);
        if (objp == nullptr)
        {
            freeze_obj_handle = 0xFFFFFFFF;
            return false;
        }
        if (objp->pos.is_valid() && hp->pos.is_valid() && objp->orient.is_valid() && hp->orient.is_valid())
        {
            vec_change_to_global_coords(ik_pos, objp->pos, objp->orient);
            vec_change_to_local_coords(ik_pos, hp->pos, hp->orient);
            return true;
        }
        return false;
    }
    default:
        return false;
    }
    ik_pos.x = ik_pos.x - hp->pos.x;
    ik_pos.y = ik_pos.y - hp->pos.y;
    ik_pos.z = ik_pos.z - hp->pos.z;
    ik_pos.rotate(hp->orient);
    return true;
}

// 0x00A7D4B0 (:14396)
// Whether this joint does IK this frame, with its target: the strength ramps by the animation's IK triggers; the
// left hand holds a two-handed firearm, the player's right hand its melee weapon, both hands a turret's grips, or a
// hand stays frozen on an object.
bool ik_joint::process(human* hp, vector& target_pos)
{
    ik_trigger* trigger = find_ik_trigger(hp);
    if (trigger != nullptr)
    {
        // Magic number: the default ramp of 2 per second.
        ik_rate_of_change = 2.0f;
        const float rate = trigger->blend_rate;
        if (rate == 0.0f)
        {
            ik_rate_of_change = 0.0f;
            ik_strength = trigger->enable ? 1.0f : 0.0f;
        }
        else
        {
            ik_rate_of_change = rate;
            if (!trigger->enable)
            {
                ik_rate_of_change = rate * -1.0f;
            }
        }
        float strength = ik_rate_of_change * Frametime + ik_strength;
        ik_strength = strength;
        if (strength - 0.0f < 0.0f)
        {
            strength = 0.0f;
        }
        if (strength - 1.0f >= 0.0f)
        {
            strength = 1.0f;
        }
        ik_strength = strength;
    }
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE)
    {
        return false;
    }
    weapon* wp = hp->get_equipped_weapon();
    if (wp != nullptr && wp->get_is_visible())
    {
        if (!hp->hflags.riot_shield || hp->shield_handle == 0xFFFFFFFF)
        {
            if (location == ANIMATION_IK_LEFT_HAND && human_weapons_is_two_handed_firearm_equipped(hp) && hp->get_equipped_inv_item() != nullptr && hp->equip_tag_index >= 0 &&
                hp->equip_offhand_tag_index >= 0)
            {
                if (trigger == nullptr)
                {
                    ik_strength = 1.0f;
                }
                return get_ik_pos(hp, target_pos, IK_REASON_TWO_HANDED_WEAPON);
            }
            if (location == ANIMATION_IK_RIGHT_HAND && human_weapons_is_melee_weapon_equipped(hp) && object_is_player(hp))
            {
                if (trigger == nullptr)
                {
                    ik_strength = 1.0f;
                }
                return get_ik_pos(hp, target_pos, IK_REASON_RIGHT_HAND_TO_WEAPON);
            }
            turret* tp = human_combat_get_manned_turret(hp);
            if (tp != nullptr && (location == ANIMATION_IK_LEFT_HAND || location == ANIMATION_IK_RIGHT_HAND) && (tp = human_combat_get_manned_turret(hp)) != nullptr &&
                !tp->t_info->flags.no_operator_animation && !tp->should_autoaim() &&
                ((location == ANIMATION_IK_LEFT_HAND && tp->has_left_hand_grip_tag()) || (location == ANIMATION_IK_RIGHT_HAND && tp->has_left_hand_grip_tag())))
            {
                // Original: the right hand also asks for the left hand's grip tag.
                if (hp->vehicle_handle == 0xFFFFFFFF && hp->cur_state != HS_TURRET)
                {
                    return false;
                }
                if (trigger == nullptr)
                {
                    ik_strength = 1.0f;
                }
                return get_ik_pos(hp, target_pos, IK_REASON_USING_TURRET);
            }
        }
        else
        {
            ik_strength = 0.0f;
        }
    }
    if (freeze_obj_handle != 0xFFFFFFFF)
    {
        ik_strength = 1.0f;
        return get_ik_pos(hp, target_pos, IK_REASON_GENERIC_OBJECT);
    }
    return false;
}

// 0x00A092B0 (:14507)
bool human_is_falling(const human* hp)
{
    return hp->movement_mode == HMM_FALL;
}

// 0x00A092D0 (:14512)
bool human_is_jumping(const human* hp)
{
    return hp->movement_mode == HMM_JUMP;
}

// 0x00A53DF0 (:14517)
// Jumping, falling or sliding; a ragdoll is in the air while it moves faster than half the safe fall speed.
bool human_is_in_air(const human* hp)
{
    const human_ragdoll_state rs = hp->ragdoll_state;
    if (rs == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_FLINCH ||
        (rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos))
    {
        if (hp->movement_mode == HMM_FALL || hp->movement_mode == HMM_JUMP)
        {
            return true;
        }
        return hp->rdd.is_loaded() && hp->rdd.get_data()->controller->flags.sliding;
    }
    const float safe = human_move_fall_get_safe_speed(hp, true);
    vector vel;
    havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
    return safe * 0.5f * safe * 0.5f < vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
}

// 0x00A2ED40 (:14970)
// The first child item of a prop type (and, given a name, whose character is that one).
item* human_get_prop(human* hp, prop_type ptype, const char* pNameToCheck)
{
    item* ip = (item*)hp->child_ptr;
    while (ip != nullptr)
    {
        if (ip->get_obj_type() == OT_ITEM && ip->m_prop_type == ptype)
        {
            if (pNameToCheck == nullptr)
            {
                return ip;
            }
            character_instance* ci = ip->char_inst;
            if (ci != nullptr && ci->rdd.is_loaded() && ci->rdd.get_data()->base_character != nullptr &&
                keen::compareString(ci->rdd.get_data()->base_character->name, pNameToCheck) == 0)
            {
                return ip;
            }
        }
        ip = (item*)ip->child_next;
        if (ip == (item*)hp->child_ptr)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x00AB2680 (:14992)
// Wears a prop item from the human's general props (matched by type and item info, or one of a prop's sub props):
// attached to its character prop point (rigged items share the human's rig), then each sub prop of the matched one
// is created by a roll in [0,1) against the running sum of the appearance percents.
bool human_wear_prop(human* hp, item* ip)
{
    const human_info* hi = hp->h_info;
    for (int g = 0; g < hi->num_general_props; g++)
    {
        const human_info::human_prop_general_info& general = hi->general_props[g];
        if (general.m_prop_type != ip->m_prop_type)
        {
            continue;
        }
        human_info::human_prop_info* match = nullptr;
        for (int i = 0; i < general.num_props; i++)
        {
            human_info::human_prop_info& prop = general.props[i];
            if (prop.item == ip->info)
            {
                match = &prop;
                break;
            }
            for (int s = 0; s < prop.num_sub_props; s++)
            {
                if (prop.sub_props[s].item == ip->info)
                {
                    match = &prop.sub_props[s];
                    break;
                }
            }
            // Original: a sub prop match does not stop the search; a later prop or sub prop of the same item wins.
        }
        if (match == nullptr)
        {
            continue;
        }
        int child_prop_point = -1;
        if (match->item_prop_tag != nullptr)
        {
            child_prop_point = ip->get_tag_index_from_name(match->item_prop_tag);
        }
        const int parent_prop_point = match->character_prop_index;
        if (ip->get_parent() == hp->get_handle() || hp->set_child(ip))
        {
            if (ip->attach_info == nullptr)
            {
                ip->attach_info = Attach_info_pool.alloc(0);
            }
            if (ip->attach_info != nullptr)
            {
                ip->attach_info->parent_prop_point = parent_prop_point;
                ip->attach_info->child_prop_point = child_prop_point;
                ip->attach_info->update_physics = 1;
                ip->attach_info->updated = 0;
                ip->attach_info->parent_handle = hp->get_handle();
                ip->attach_info->use_relative_transform = 0;
            }
        }
        // Magic number: 0x100, the rigged (shares the parent's skeleton) flag of an item info.
        if ((ip->info->flags & 0x100) != 0 && hp->rdd.is_loaded())
        {
            animlib_set_parent_rig(ip->char_inst->aim_handle, hp->char_inst->aim_handle);
        }
        if (match->num_sub_props < 1)
        {
            return true;
        }
        float roll = fl_rand_range(0.0f, 1.0f);
        for (int s = 0; s < match->num_sub_props; s++)
        {
            const human_info::human_prop_info& sub = match->sub_props[s];
            roll = roll - sub.appearance_percent;
            if (roll < 0.0f && sub.item != nullptr)
            {
                // Original: the flags are an uninitialized stack slot with only no_physics, simulate,
                // character_prop, cast_shadows, cast_drop_shadows (0x6406) and update_attach_point set and the
                // other used bits cleared; the bits above 17 are unused.
                obj_item_flags flags;
                flags.respawn = 0;
                flags.no_physics = 1;
                flags.simulate = 1;
                flags.props_created = 0;
                flags.item_held = 0;
                flags.no_save = 0;
                flags.preplaced = 0;
                flags.update_attach_point = sub.item_prop_tag != nullptr;
                flags.dropped_by_player = 0;
                flags.touched_last_frame = 0;
                flags.character_prop = 1;
                flags.attached_cloth_sim = 0;
                flags.doing_errode = 0;
                flags.cast_shadows = 1;
                flags.cast_drop_shadows = 1;
                flags.xray_visible = 0;
                flags.dont_dampen_impulse = 0;
                flags.shield_initialized = 0;
                human_create_prop_item(hp, sub, ip->m_prop_type, flags);
            }
        }
        return true;
    }
    return false;
}

// 0x00A2EDD0 (:15068)
bool human_freeze_ik(human* hp, ik_type body_part, object* obj)
{
    ik_joint& joint = hp->ik_joints[body_part];
    if (obj == nullptr)
    {
        joint.freeze_obj_handle = 0xFFFFFFFF;
        return false;
    }
    if (joint.freeze_obj_handle != 0xFFFFFFFF || !hp->char_inst->rdd.is_loaded())
    {
        return false;
    }
    // Original: both calls run and write the same vector, so the virtual call's answer is the one used.
    animlib_get_tag_local_coords(hp->char_inst->aim_handle, joint.ik_tag, joint.freeze_offset_pos, nullptr);
    hp->get_tag_local_coords(joint.ik_tag, joint.freeze_offset_pos, nullptr);
    if (joint.freeze_offset_pos.is_null())
    {
        return false;
    }
    joint.freeze_obj_handle = obj->get_handle();
    if (hp->get_handle() != joint.freeze_obj_handle)
    {
        vec_change_to_global_coords(joint.freeze_offset_pos, hp->pos, hp->orient);
        vec_change_to_local_coords(joint.freeze_offset_pos, obj->pos, obj->orient);
    }
    return true;
}

// 0x00A9B920 (:15112)
// Screams when thrown: falling (or flying as a ragdoll) faster than the safe fall speed (half of it squared for a
// ragdoll's speed; the player's scaled by the backpack), unless the player's jetpack or jump boost backpack is
// what lifts it.
void human_maybe_play_falling_voice(human* hp)
{
    voice_lines line = VOICE_LINE_NONE;
    float safe_speed;
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        float factor = 1.0f;
        if (hp->get_obj_type() == OT_HUMAN && ((player*)hp)->backpack != nullptr)
        {
            factor = ((player*)hp)->backpack->get_backpack_abilities()->fall_speed_safety_factor;
        }
        safe_speed = Player_safe_fall_speed * factor;
    }
    else
    {
        safe_speed = hp->hflags.jumping_from_building ? Npc_safe_jumping_fall_speed : Npc_safe_fall_speed;
    }
    const human_ragdoll_state rs = hp->ragdoll_state;
    const bool not_ragdolled = rs == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_FLINCH ||
                               (rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos);
    float value;
    float limit;
    if (not_ragdolled)
    {
        if (hp->movement_mode == HMM_FALL)
        {
            vector vel;
            if (hp->rdd.is_loaded())
            {
                hp->rdd.get_data()->controller->get_velocity(vel);
            }
            else
            {
                vel.zero();
            }
            value = vel.y;
            limit = -safe_speed;
            // The falling velocity is below minus the safe speed.
            if (limit > value)
            {
                line = VOICE_LINE_THROWN;
            }
        }
    }
    else
    {
        vector vel;
        havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
        value = vel.y * vel.y + vel.x * vel.x + vel.z * vel.z;
        // Magic number: half the safe speed, squared.
        limit = safe_speed * 0.5f * safe_speed * 0.5f;
        if (limit < value)
        {
            line = VOICE_LINE_THROWN;
        }
    }
    bool allowed = true;
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)hp)->backpack != nullptr && not_ragdolled)
    {
        player* pp = (player*)hp;
        multi_object_backpack* bp = pp->backpack;
        const multi_backpack_info::abilities* ab = bp->activated == true ? &bp->backpack_info->active_abilities : &bp->backpack_info->passive_abilities;
        // Magic number: a jetpack with 90% fuel or more that is not flying does not count.
        const bool jetpack = bp->backpack_info->jetpack == true && (pp->pflags.jetpack_started || bp->fuel < 0.9f);
        if (ab->jump_boost == true || jetpack)
        {
            allowed = false;
        }
    }
    if (line != VOICE_LINE_NONE)
    {
        const voice_lines current = hp->voice_time_since_finish.valid() ? VOICE_LINE_NONE : hp->situational_voice_line;
        if (current != line && allowed)
        {
            hp->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
}

// The render flags a human copies to its body and head skin renderables (inlined twice in human::update_render).
static void human_update_skin_flags(human* hp, rl_skin_instance* skin, bool stealthed)
{
    skin->set_casts_shadows(hp->hflags.casts_shadows != 0);
    skin->set_casts_drop_shadows(hp->hflags.casts_drop_shadows != 0);
    skin->set_xray_visible(hp->hflags.xray_visible != 0);
    skin->set_xray_render_params(hp->xray_material, hp->xray_render_alpha);
    skin->set_distortion_visible(stealthed);
}

// The stealth cut height (instance parameter w): from 0.3 m above the box down to 0.3 m below it as the stealth
// percentage rises; 100000 (nothing cut) without stealth. The 0.3 literals are doubles.
static float human_stealth_cut_height(human* hp, bool stealthed)
{
    if (!stealthed)
    {
        // Magic number: no cut.
        return 100000.0f;
    }
    vector bmin;
    vector bmax;
    hp->get_bbox(bmin, bmax);
    return (float)((((double)bmin.y - 0.3) - ((double)bmax.y + 0.3)) * (double)hp->m_stealth_percent + (double)bmax.y + 0.3);
}

// 0x00A9BB90 (:15159)
// The render update of a human: hidden unless _dont_use_me is set and its bones are instanced (and far humans in a
// vehicle, not manning a turret, hide with their child items). The body skin renderable gets the visibility, shadow,
// x-ray, stealth (distortion and cut height) and opacity settings, then character_instance::update_render with the
// LOD of the apparent distance (only the transform while hidden, which posts EVENT_HUMAN_STOPPED_BEING_RENDERED once).
// The head: its anim_lib instance first (parented to the body's rig; the update stops there), then a skin renderable
// of the base head within 20 m (with the head morphs) or the LOD head beyond, recreated when that choice changes,
// placed like the body with the head's bones and its LOD sub mesh. The child items get the opacity, and the single
// player backpack fades.
// Notes (decompiled behaviour, kept as is): the head anim_lib branch and a failed skin creation return without
// resetting camera_alpha_override; the LOD head lowers the LOD by one, to -1 (a read before the sub mesh table) when
// the body LOD is 0; the head morph container is used without a check.
void human::update_render(rl_renderer*)
{
    character_instance::resource_dependent_data* char_data = char_inst->rdd.get_data();
    if (!char_inst->rdd.is_loaded())
    {
        camera_alpha_override = 1.0f;
        return;
    }
    resource_dependent_data* data = rdd.get_data();
    bool hidden = !obj_flags._dont_use_me;
    matrix43* instanced = nullptr;
    if (!animlib_get_instanced_model_matrices(char_inst->aim_handle, &instanced))
    {
        hidden = true;
    }
    if ((player*)this == Player && Camera_third_person_lock)
    {
        set_opacity_plus_child_items(1.0f);
    }
    const float distance = render_distance.get_apparent_distance(pos);
    bool visible;
    // Magic number: humans in a vehicle hide beyond 60 m.
    if (distance <= 60.0f || vehicle_handle == 0xFFFFFFFF || combat.reserved_turret != 0xFFFFFFFF)
    {
        if (!hidden && hflags.in_vehicle_invisible)
        {
            hflags.in_vehicle_invisible = 0;
            for (object* child = child_ptr; child != nullptr;)
            {
                object_set_visibility_plus_children(child, true);
                child = child->child_next;
                if (child == child_ptr)
                {
                    break;
                }
            }
        }
        visible = !hidden;
    }
    else
    {
        hflags.in_vehicle_invisible = 1;
        hidden = true;
        for (object* child = child_ptr; child != nullptr;)
        {
            object_set_visibility_plus_children(child, false);
            child = child->child_next;
            if (child == child_ptr)
            {
                break;
            }
        }
        visible = false;
    }
    rl_skin_instance* body = char_data->renderable_instance_p;
    body->set_is_visible(visible); // inlined
    // Magic number: the robust comparison's epsilon.
    const bool stealthed = fl_greater_than_robust(m_stealth_percent, 0.0f, 0.0001f);
    human_update_skin_flags(this, body, stealthed);
    body->set_opacity(camera_alpha_override * render_alpha);
    body->set_casts_transparent_shadows(casts_transparent_shadows);
    vector4 params = body->get_instance_params();
    params.w = human_stealth_cut_height(this, stealthed);
    body->set_instance_params(params);
    int lod = 0;
    if (!hidden)
    {
        if (!cutscene_is_playing())
        {
            while (lod < character_instance::c_lod_distance_max && character_instance::ms_char_lod_distances[lod].dist < distance)
            {
                lod++;
            }
            // Magic number: the last LOD (3).
            if (lod > 2)
            {
                lod = 3;
            }
        }
        char_inst->update_render(pos, orient, approx_local_bmin, approx_local_bmax, lod, false);
    }
    else
    {
        char_inst->update_render(pos, orient, approx_local_bmin, approx_local_bmax, 0, true);
        if (hflags.was_rendered)
        {
            human_stopped_being_rendered_event ev;
            ev.target_handle = get_handle();
            gem_post_event(ev);
        }
    }
    hflags.was_rendered = hidden ? 0 : 1;
    rl_skin_instance* head = data->head_renderable_instance_p;
    if (head != nullptr)
    {
        head->set_is_visible(visible); // inlined
        human_update_skin_flags(this, head, stealthed);
        params = head->get_instance_params();
        params.w = human_stealth_cut_height(this, stealthed);
        head->set_instance_params(params);
    }
    const int head_idx = data->head_manager_idx;
    if (head_idx == -1 || !Head_managers[head_idx].loaded || hidden)
    {
        set_opacity_plus_child_items(render_alpha);
        set_casts_transparent_shadows_plus_child_items(casts_transparent_shadows);
        camera_alpha_override = 1.0f;
        return;
    }
    head_manager* hm = &Head_managers[head_idx];
    if (!hflags.head_loaded)
    {
        data->head_aim_handle = animlib_new_anim_instance_manager(hm->head->rig, hm->cp->bones_used_info.unique_id);
        animlib_set_parent_rig(data->head_aim_handle, char_inst->aim_handle);
        hflags.head_loaded = 1;
        return;
    }
    bool recreate = false;
    bool base_head;
    character_mesh* cmesh = hm->head->lod_mesh;
    // Magic number: the base head within 20 m.
    if (distance < 20.0f || cmesh == nullptr)
    {
        cmesh = hm->head->base_head->m_data;
        base_head = true;
        lod = 0;
        if (hflags.lod_head_loaded)
        {
            hflags.lod_head_loaded = 0;
            recreate = true;
        }
    }
    else
    {
        lod = lod - 1;
        base_head = false;
        if (!hflags.lod_head_loaded)
        {
            hflags.lod_head_loaded = 1;
            recreate = true;
        }
    }
    const int num_bones = animlib_get_num_bones(data->head_aim_handle);
    if (head != nullptr && recreate)
    {
        head->release();
        data->head_renderable_instance_p = nullptr;
        head = nullptr;
    }
    if (head == nullptr)
    {
        if (!rl_skin_instance::can_allocate_bones(num_bones))
        {
            return;
        }
        head = rl_skin_instance::create();
        data->head_renderable_instance_p = head;
        if (head == nullptr)
        {
            return;
        }
        const rl_material_map* map = hm->mat_p == nullptr ? cmesh->m_shared_header.m_material_map.to_pointer() : hm->mat_p->m_material_map.to_pointer();
        head->init(cmesh->m_shared_header.m_mesh.to_pointer(), map, (unsigned int)cmesh->m_lod_submesh_id.to_pointer()[0]);
        head->allocate_bones(num_bones);
        player* pp = World.object_cast<player>(this);
        if (!hflags.is_nano_effect_currently_applied)
        {
            if (pp != nullptr)
            {
                player_refresh_head_material_effect(pp);
            }
        }
        else
        {
            if (mat_fx_handle_for_head_skin != 0xFFFFFFFF)
            {
                material_effects_remove(mat_fx_handle_for_head_skin);
                mat_fx_handle_for_head_skin = 0xFFFFFFFF;
            }
            const int effect_id = material_effects_get_nano_rifle_matfx_id(human_is_dead(this));
            if (rdd.is_loaded() && data->head_renderable_instance_p != nullptr)
            {
                material_effect_parameters effect_params = material_effects_get_parameters(1, fade_time);
                effect_params.time_passed_ms = fade_timer.time_since();
                mat_fx_handle_for_head_skin = material_effects_apply(data->head_renderable_instance_p, effect_id, &effect_params);
                material_effects_force_finalize();
            }
        }
    }
    head->set_local_aabb(rl_aabb(approx_local_bmin, approx_local_bmax));
    head->set_position(pos);
    head->set_rotation(orient);
    head->release_morph_data();
    const matrix43* head_matrices = animlib_get_render_matrices(data->head_aim_handle);
    if (head_matrices == nullptr)
    {
        return;
    }
    head->set_bones_matricies(head_matrices, num_bones);
    game_render_scene_update(head);
    if (base_head)
    {
        const int head_morph = hm->head->head_morph;
        const int base_head_morph = hm->head->base_head_morph;
        if (head_morph != -1 && head_morph != base_head_morph)
        {
            head->activate_morph(mesh_morph_get_data(head_morph), 0, 1.0f);
        }
        if (base_head_morph != -1)
        {
            rl_morph_container* container = mesh_morph_get_data(base_head_morph);
            anim_morph_info* morph_info = nullptr;
            const int num_morphs = animlib_get_morph_info(char_inst->aim_handle, &morph_info);
            // The weights of the 40 morph targets: the body animation's morphs, then the head morphs (lip sync and
            // expressions) averaged in.
            // Notes (decompiled behaviour, changed): the original zeroes the weights only when there are body or head
            // morphs and otherwise activates whatever is on the stack; here they start at zero.
            float weights[0x28];
            memset(weights, 0, sizeof(weights));
            if (num_morphs != 0 || head_morphs.num_targets != 0)
            {
                if (head_morphs.current_lipsync_handle == INVALID_LIPSYNC_DATA_HANDLE)
                {
                    for (int i = 0; i < num_morphs; i++)
                    {
                        const int target = container->find_morph_target(morph_info[i].name_check);
                        if (target != -1)
                        {
                            weights[target] = morph_info[i].blend_amount;
                        }
                    }
                }
            }
            for (int i = 0; i < head_morphs.num_targets; i++)
            {
                const morph_target& t = head_morphs.target[i];
                if (weights[t.target_idx] == 0.0f)
                {
                    weights[t.target_idx] = t.blend;
                }
                else
                {
                    weights[t.target_idx] = (weights[t.target_idx] + t.blend) * 0.5f;
                }
            }
            for (int i = 0; i < 0x28; i++)
            {
                if (weights[i] != 0.0f)
                {
                    head->activate_morph(container, i, weights[i]);
                }
            }
        }
    }
    const int last_lod = (int)cmesh->m_num_lods - 1;
    if (last_lod <= lod)
    {
        lod = last_lod;
    }
    head->set_submesh_index((unsigned int)cmesh->m_lod_submesh_id.to_pointer()[lod]);
    head->set_opacity(camera_alpha_override * render_alpha);
    head->set_casts_transparent_shadows(casts_transparent_shadows);
    set_opacity_plus_child_items(render_alpha);
    set_casts_transparent_shadows_plus_child_items(casts_transparent_shadows);
    if (!Multiplayer && (player*)this == Player && get_obj_type() == OT_HUMAN && get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        ((player*)this)->update_backpack_fade();
    }
    camera_alpha_override = 1.0f;
}

// 0x00A2EE80 (:15461)
// Outlines the body and head on one layer (0..3).
bool human::enable_object_outline(int layer)
{
    if (char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr && (unsigned int)layer < 4)
    {
        char_inst->rdd.get_data()->renderable_instance_p->set_outline_enabled(layer); // inlined
    }
    if (rdd.is_loaded() && rdd.get_data()->head_renderable_instance_p != nullptr && (unsigned int)layer < 4)
    {
        rdd.get_data()->head_renderable_instance_p->set_outline_enabled(layer); // inlined
    }
    return true;
}

// 0x00A2EF00 (:15482)
bool human::disable_object_outline()
{
    if (char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr)
    {
        char_inst->rdd.get_data()->renderable_instance_p->set_outline_disabled(); // inlined
    }
    if (rdd.is_loaded() && rdd.get_data()->head_renderable_instance_p != nullptr)
    {
        rdd.get_data()->head_renderable_instance_p->set_outline_disabled(); // inlined
    }
    return true;
}

// 0x00A54700 (:15511)
// The child items and their child items (item::set_casts_transparent_shadows_plus_child_items inlined for the first
// level).
void human::set_casts_transparent_shadows_plus_child_items(bool do_cast)
{
    casts_transparent_shadows = do_cast;
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            ip->casts_transparent_shadows = do_cast;
            item* sub = (item*)ip->child_ptr;
            while (sub != nullptr)
            {
                if (sub->get_obj_type() == OT_ITEM)
                {
                    sub->set_casts_transparent_shadows_plus_child_items(do_cast);
                }
                sub = (item*)sub->child_next;
                if (sub == (item*)ip->child_ptr)
                {
                    break;
                }
            }
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A7E1D0 (:15527)
// The player looking through a scope is invisible; the child items also get the camera alpha override.
void human::set_opacity_plus_child_items(float alpha)
{
    if (this != nullptr && (player*)this == Player && ((player*)this)->zoom_state == PZS_SCOPE)
    {
        alpha = 0.0f;
    }
    render_alpha = alpha;
    item* ip = (item*)child_ptr;
    while (ip != nullptr)
    {
        if (ip->get_obj_type() == OT_ITEM)
        {
            ip->set_opacity_plus_child_items(camera_alpha_override * alpha);
        }
        ip = (item*)ip->child_next;
        if (ip == (item*)child_ptr)
        {
            return;
        }
    }
}

// 0x00ADBA90 (:16571)
// After the level loads: the player, the anim set and flesh materials, the vehicle impact cues and the ladder
// building damage event.
void human_level_init_post()
{
    No_npc_npc_damage = false;
    player_level_init_post();
    // Magic number: the first marauder retreat check after 5 s.
    Marauder_retreat_check_timer.set(5000);
    NPC_male_guerrilla_anim_set = anim_set_find("Male");
    Human_flesh_material = rfg_get_physical_material_index("Flesh");
    Human_flesh_effect_material = rfg_get_effect_material_index(Human_flesh_material);
    audiolib_cue_get_id(Human_vehicle_impact_foley[0], &Human_vehicle_impact[0]);
    audiolib_cue_get_id(Human_vehicle_impact_foley[1], &Human_vehicle_impact[1]);
    notify_info ni;
    ni.data = nullptr;
    ni.callback = ladder_building_damaged_callback;
    ni.type = EVENT_BUILDING_DAMAGED;
    gem_register_for_event(ni);
}

// 0x00A54870 (:16618)
// Moves a respawn point by up to half a meter (x and z) until no human stands exactly on it.
void human_validate_respawn_point(vector& respawn_pos)
{
    for (;;)
    {
        bool taken = false;
        for (int i = 0; i < World.get_num_objects<human>(); i++)
        {
            object* objp = World.get_object<human>(i);
            if (objp->pos.x == respawn_pos.x && objp->pos.z == respawn_pos.z)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            return;
        }
        respawn_pos.x = (fl_rand() - 0.5f) + respawn_pos.x;
        respawn_pos.z = (fl_rand() - 0.5f) + respawn_pos.z;
    }
}

// 0x00A09300 (:15503)
bool human_is_flinching(const human* hp)
{
    if (hp != nullptr && hp->char_inst != nullptr)
    {
        return hp->char_inst->is_playing_action(ANIM_ALIAS_FLINCH);
    }
    return false;
}

// 0x00AB2A70 (:16604)
void human_level_shutdown()
{
    player_level_shutdown_all();
    npc_free_all();
}

// 0x00A09320 (:16637)
bool human_is_in_cover(const human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    return hp->cur_state == HS_COVER;
}

// 0x00A06C30 (:153): empty in the shipped build (a debug check of positions; never invalid)
bool check_if_pos_is_invalid(const vector&, void*, const char*)
{
    return false;
}

// ---- hit locations, shields, stealth and inventory removal (multiplayer.md MP8)

// 0x00A06C70 (:1215)
static object_hit_location hit_location_from_body_part(character_body_part_id body_part_id)
{
    switch (body_part_id)
    {
    case CBP_TORSO:
        return HIT_TORSO;
    case CBP_UPPER_LEFT_LEG:
    case CBP_LOWER_LEFT_LEG:
        return HIT_LEFT_LEG;
    case CBP_UPPER_RIGHT_LEG:
    case CBP_LOWER_RIGHT_LEG:
        return HIT_RIGHT_LEG;
    case CBP_UPPER_LEFT_ARM:
    case CBP_LOWER_LEFT_ARM:
        return HIT_LEFT_ARM;
    case CBP_UPPER_RIGHT_ARM:
    case CBP_LOWER_RIGHT_ARM:
        return HIT_RIGHT_ARM;
    case CBP_PELVIS:
        return HIT_PELVIS;
    case CBP_HEAD:
        return HIT_HEAD;
    case CBP_UNDEFINED:
        return HIT_NONE;
    default:
        return HIT_GENERAL;
    }
}

// 0x00A06E90 (:1239)
// A sphere against a capped cylinder (the axis extended by the sphere's radius at both ends). Original: the test
// rejects every sphere whose offset from the closest axis point has a component along the axis (the offset is
// perpendicular except at the extended ends, where the caps are then tested).
bool ix_sphere_cylinder(const vector& sphere_pos, float sphere_radius, const vector& cylinder_base, const vector& cylinder_axis, float cylinder_height, float cylinder_radius)
{
    const vector top(cylinder_base.x + cylinder_axis.x * cylinder_height, cylinder_base.y + cylinder_axis.y * cylinder_height, cylinder_base.z + cylinder_axis.z * cylinder_height);
    const vector ext(cylinder_axis.x * sphere_radius, cylinder_axis.y * sphere_radius, cylinder_axis.z * sphere_radius);
    const vector seg_start(cylinder_base.x - ext.x, cylinder_base.y - ext.y, cylinder_base.z - ext.z);
    const vector seg_end(ext.x + top.x, ext.y + top.y, ext.z + top.z);
    vector closest;
    const float t = ix_closest_point_on_line_segment(closest, sphere_pos, seg_start, seg_end);
    const vector offset(sphere_pos.x - closest.x, sphere_pos.y - closest.y, sphere_pos.z - closest.z);
    const float along = (cylinder_axis.y * offset.y + cylinder_axis.x * offset.x + offset.z * cylinder_axis.z) - 0.0f;
    const float dist = sqrtf(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (1.1920929e-07f <= along || 1.1920929e-07f <= -along)
    {
        return false;
    }
    if (cylinder_radius < dist)
    {
        if (sphere_radius + cylinder_radius < dist)
        {
            return false;
        }
        if (t < sphere_radius || sphere_radius + cylinder_height < t)
        {
            const vector cap = sphere_radius <= t ? top : cylinder_base;
            const vector dir = offset.copy_normalize();
            const float dx = (cylinder_radius * dir.x + cap.x) - sphere_pos.x;
            const float dy = (cap.y + dir.y * cylinder_radius) - sphere_pos.y;
            const float dz = (cap.z + dir.z * cylinder_radius) - sphere_pos.z;
            if (sphere_radius * sphere_radius < dx * dx + dy * dy + dz * dz)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x00A7D740 (:14609)
// Where a line hits a human: its character's collision cylinders and spheres in the current pose (grown by the radius;
// the shield arm's cylinders are skipped while a shield is held), the nearest hit wins. Without bone matrices or
// collision shapes, a 1.9 m by 0.5 m cylinder at the human (a hit is the torso).
object_hit_location human_get_hit_location(const human* hp, const vector& start_pos, const vector& direction, vector& intersection, float radius, int* bone_index, vector* hit_normal)
{
    if (bone_index != nullptr)
    {
        *bone_index = 0;
    }
    if (hit_normal != nullptr)
    {
        *hit_normal = direction;
    }
    character_instance* ci = hp->char_inst;
    if (!ci->rdd.is_loaded())
    {
        return HIT_NONE;
    }
    const character_mesh* cmesh = ci->rdd.get_data()->base_character->m_data;
    matrix43* matrices;
    const bool have_matrices = animlib_get_instanced_model_matrices(ci->aim_handle, &matrices);
    const int num_spheres = cmesh->m_num_cspheres;
    const int num_cylinders = cmesh->m_num_ccylinders;
    bool shielded = hp->shield_handle != 0xFFFFFFFF;
    bool right_hand_shield = true;
    if (shielded)
    {
        const weapon_info* w_info = human_weapons_get_weapon_info(hp);
        if (w_info == nullptr)
        {
            shielded = false;
        }
        else
        {
            right_hand_shield = !w_info->flags.held_in_left_hand;
        }
    }
    float best = 3.4028235e+38f;
    int best_part = -1;
    if (!have_matrices || (num_spheres == 0 && num_cylinders == 0))
    {
        // Magic numbers: a standing human is 1.9 m tall and 0.5 m wide; a hit is HIT_GENERAL.
        return (object_hit_location)((int)ix_line_cylinder(start_pos, direction, hp->pos, hp->orient.uvec, 1.9f, 0.5f, intersection) + HIT_NONE);
    }
    line ray;
    ray.point = start_pos;
    ray.dir = direction;
    const int num_bones = animlib_get_num_bones(hp->char_inst->aim_handle);
    const character_mesh_ccylinder* cylinders = cmesh->m_ccylinders.to_pointer();
    for (int i = 0; i < num_cylinders; i++)
    {
        const character_mesh_ccylinder& cyl = cylinders[i];
        if (cyl.parent_index >= num_bones)
        {
            continue;
        }
        // The shield covers the forearm of the hand that does not hold the weapon.
        if (shielded && (right_hand_shield ? cyl.body_part_id == CBP_LOWER_LEFT_ARM : cyl.body_part_id == CBP_LOWER_RIGHT_ARM))
        {
            continue;
        }
        const matrix43& bone = matrices[cyl.parent_index];
        vector base = cyl.pos;
        base.unrotate(bone.m_rotation);
        base = vector(base.x + bone.m_translation.x, bone.m_translation.y + base.y, bone.m_translation.z + base.z);
        base.unrotate(hp->orient);
        base = vector(hp->pos.x + base.x, hp->pos.y + base.y, hp->pos.z + base.z);
        vector axis = cyl.axis;
        axis.unrotate(bone.m_rotation);
        axis.unrotate(hp->orient);
        axis.normalize_safe();
        base = vector(cyl.height * axis.x * 0.5f + base.x, axis.y * cyl.height * 0.5f + base.y, axis.z * cyl.height * 0.5f + base.z);
        vector hit;
        if (!ix_line_cylinder(start_pos, direction, base, axis, cyl.height + radius, cyl.radius + radius, hit))
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(start_pos, hit);
        if (dist_sq >= best)
        {
            continue;
        }
        line axis_line;
        axis_line.point = base;
        axis_line.dir = axis;
        vector on_axis;
        ix_closest_point_on_ray(hit, axis_line, on_axis);
        const vector normal = (hit - on_axis).copy_normalize_safe();
        intersection = vector(hit.x - normal.x * radius, hit.y - normal.y * radius, hit.z - normal.z * radius);
        best_part = (int)cyl.body_part_id;
        best = dist_sq;
        if (hit_normal != nullptr)
        {
            *hit_normal = normal;
        }
        if (bone_index != nullptr)
        {
            *bone_index = cyl.parent_index;
        }
    }
    const character_mesh_csphere* spheres = cmesh->m_cspheres.to_pointer();
    for (int i = 0; i < num_spheres; i++)
    {
        const character_mesh_csphere& sph = spheres[i];
        if (sph.parent_index >= num_bones)
        {
            continue;
        }
        const matrix43& bone = matrices[sph.parent_index];
        vector center = sph.pos;
        center.unrotate(bone.m_rotation);
        center = vector(center.x + bone.m_translation.x, bone.m_translation.y + center.y, bone.m_translation.z + center.z);
        center.unrotate(hp->orient);
        center = vector(hp->pos.x + center.x, hp->pos.y + center.y, hp->pos.z + center.z);
        ix_collision collision;
        if (!ix_ray_sphere(collision, ray, center, sph.radius + radius))
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(collision.hit_point, ray.point);
        if (dist_sq >= best)
        {
            continue;
        }
        best_part = (int)sph.body_part_id;
        best = dist_sq;
        const vector normal = (collision.hit_point - center).copy_normalize_safe();
        intersection = vector(collision.hit_point.x - normal.x * radius, collision.hit_point.y - normal.y * radius, collision.hit_point.z - normal.z * radius);
        if (hit_normal != nullptr)
        {
            *hit_normal = normal;
        }
        if (bone_index != nullptr)
        {
            *bone_index = sph.parent_index;
        }
    }
    if (best_part == -1)
    {
        return HIT_NONE;
    }
    return hit_location_from_body_part((character_body_part_id)best_part);
}

// 0x00A2EC40 (:14788)
// A hit in the human's frame moved into the hit bone's frame (its render matrix), so a decal there follows the bone;
// unchanged without the character's render matrices (VFX.md FX7).
void human_get_unanimated_hit_location(const human* hp, const vector& hit_pos, const vector& hit_normal, int hit_bone_index, vector& out_pos, vector& out_norm)
{
    if (hp->char_inst->rdd.is_loaded())
    {
        const matrix43* matrices = animlib_get_render_matrices(hp->char_inst->aim_handle);
        if (matrices != nullptr)
        {
            const matrix43& m = matrices[hit_bone_index];
            out_pos.set(hit_pos.x - m.m_translation.x, hit_pos.y - m.m_translation.y, hit_pos.z - m.m_translation.z);
            out_norm = hit_normal;
            // Original bug kept: the bone matrix carries the character's height scale, so this rotate is the inverse
            // only at scale 1. Other characters get bind_pos = scale^2 * true position (EDF_Elite_MP 1.055 and others).
            out_pos.rotate(m.m_rotation);
            out_norm.rotate(m.m_rotation);
            return;
        }
    }
    out_pos = hit_pos;
    out_norm = hit_normal;
}

// 0x00A53EF0 (:14818)
// The first collision cylinder (then sphere) of the human's pose a sphere touches; without bone matrices or shapes a
// 1.9 m by 0.5 m cylinder from 0.9 m up.
object_hit_location human_get_point_hit_location(const human* hp, const vector& position, float radius, int* bone_index)
{
    if (bone_index != nullptr)
    {
        *bone_index = 0;
    }
    character_instance* ci = hp->char_inst;
    if (!ci->rdd.is_loaded())
    {
        return HIT_NONE;
    }
    const character_mesh* cmesh = ci->rdd.get_data()->base_character->m_data;
    matrix43* matrices;
    const bool have_matrices = animlib_get_instanced_model_matrices(ci->aim_handle, &matrices);
    const int num_cylinders = cmesh->m_num_ccylinders;
    const int num_spheres = cmesh->m_num_cspheres;
    if (!have_matrices || (num_spheres == 0 && num_cylinders == 0))
    {
        const vector base(hp->pos.x + UP_VECTOR.x * 0.9f, hp->pos.y + UP_VECTOR.y * 0.9f, hp->pos.z + UP_VECTOR.z * 0.9f);
        return (object_hit_location)((int)ix_sphere_cylinder(position, radius, base, hp->orient.uvec, 1.9f, 0.5f) + HIT_NONE);
    }
    const int num_bones = animlib_get_num_bones(hp->char_inst->aim_handle);
    const character_mesh_ccylinder* cylinders = cmesh->m_ccylinders.to_pointer();
    for (int i = 0; i < num_cylinders; i++)
    {
        const character_mesh_ccylinder& cyl = cylinders[i];
        if (cyl.parent_index >= num_bones)
        {
            continue;
        }
        const matrix43& bone = matrices[cyl.parent_index];
        vector base = cyl.pos;
        base.unrotate(bone.m_rotation);
        base = vector(base.x + bone.m_translation.x, bone.m_translation.y + base.y, bone.m_translation.z + base.z);
        base.unrotate(hp->orient);
        base = vector(hp->pos.x + base.x, hp->pos.y + base.y, hp->pos.z + base.z);
        vector axis = cyl.axis;
        axis.unrotate(bone.m_rotation);
        axis.unrotate(hp->orient);
        axis.normalize_safe();
        base = vector(cyl.height * axis.x * 0.5f + base.x, axis.y * cyl.height * 0.5f + base.y, axis.z * cyl.height * 0.5f + base.z);
        if (ix_sphere_cylinder(position, radius, base, axis, cyl.height, cyl.radius))
        {
            if (bone_index != nullptr)
            {
                *bone_index = cyl.parent_index;
            }
            return hit_location_from_body_part((character_body_part_id)cyl.body_part_id);
        }
    }
    const character_mesh_csphere* spheres = cmesh->m_cspheres.to_pointer();
    for (int i = 0; i < num_spheres; i++)
    {
        const character_mesh_csphere& sph = spheres[i];
        if (sph.parent_index >= num_bones)
        {
            continue;
        }
        const matrix43& bone = matrices[sph.parent_index];
        vector center = sph.pos;
        center.unrotate(bone.m_rotation);
        center = vector(center.x + bone.m_translation.x, bone.m_translation.y + center.y, bone.m_translation.z + center.z);
        center.unrotate(hp->orient);
        const float dx = position.x - (hp->pos.x + center.x);
        const float dy = position.y - (hp->pos.y + center.y);
        const float dz = position.z - (hp->pos.z + center.z);
        if (sqrtf(dx * dx + dy * dy + dz * dz) <= sph.radius + radius)
        {
            if (bone_index != nullptr)
            {
                *bone_index = sph.parent_index;
            }
            return hit_location_from_body_part((character_body_part_id)sph.body_part_id);
        }
    }
    return HIT_NONE;
}

// 0x00A545B0 (:14927): the body part of the first collision cylinder (then sphere) on the bone.
object_hit_location human_get_hit_location_from_bone_index(const human* hp, int bone_index)
{
    if (bone_index <= 0 || hp == nullptr || !hp->char_inst->rdd.is_loaded())
    {
        return HIT_NONE;
    }
    const character_mesh* cmesh = hp->char_inst->rdd.get_data()->base_character->m_data;
    const int num_cylinders = cmesh->m_num_ccylinders;
    const int num_spheres = cmesh->m_num_cspheres;
    const int num_bones = animlib_get_num_bones(hp->char_inst->aim_handle);
    const character_mesh_ccylinder* cylinders = cmesh->m_ccylinders.to_pointer();
    for (int i = 0; i < num_cylinders; i++)
    {
        if (cylinders[i].parent_index < num_bones && cylinders[i].parent_index == bone_index)
        {
            return hit_location_from_body_part((character_body_part_id)cylinders[i].body_part_id);
        }
    }
    const character_mesh_csphere* spheres = cmesh->m_cspheres.to_pointer();
    for (int i = 0; i < num_spheres; i++)
    {
        if (spheres[i].parent_index < num_bones && spheres[i].parent_index == bone_index)
        {
            return hit_location_from_body_part((character_body_part_id)spheres[i].body_part_id);
        }
    }
    return HIT_NONE;
}

// 0x00A09340 (:16646): leaning out (the four lean submodes) or side firing.
bool human_is_leaning_from_cover(human* hp)
{
    return hp != nullptr && hp->cur_state == HS_COVER && ((unsigned int)(hp->movement_submode - HMSM_COVER_RIGHT_LEAN) < 4 || hp->hflags.side_firing);
}

// 0x00A08650 (:13167)
void human_override_default_animation_state(human* hp, bool _override)
{
    hp->custom_fire_animation = ANIMATION_ACTION_NONE;
    hp->hflags.override_default_anim_state = _override;
    hp->hflags.override_default_fire_anim = 0;
}

// 0x00A08610 (:12979): ragdolled, blending out, or on impact with the ragdoll's position.
bool human_is_ragdolled_or_getting_up(const human* hp)
{
    const human_ragdoll_state state = hp->ragdoll_state;
    if (state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return true;
    }
    return !(state == HUMAN_RAGDOLL_STATE_NONE || state == HUMAN_RAGDOLL_STATE_FLINCH || (state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos));
}

// 0x00A2DE80 (:6084): a virtual shield (item info flag 0x2000) is destroyed; a riot shield stays.
void human::destroy_virtual_shield()
{
    if (hflags.riot_shield)
    {
        return;
    }
    if (shield_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(shield_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM)
        {
            if ((((item*)objp)->info->flags & 0x2000) == 0)
            {
                return;
            }
            World.destroy_object(objp, false);
        }
    }
    shield_handle = 0xFFFFFFFF;
}

// 0x00A54760 (:15546): the stealth of the human, its child items and its inventory weapons (and theirs); stealth turns
// the diffraction on.
void human::set_stealth_plus_child_items(float percent)
{
    m_stealth_percent = percent;
    for (object* child = child_ptr; child != nullptr;)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            ((item*)child)->set_stealth_plus_child_items(m_stealth_percent);
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            break;
        }
    }
    for (inventory_item* iip = inventory; iip != nullptr;)
    {
        if (iip->weapon_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(iip->weapon_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
            {
                ((item*)objp)->set_stealth_plus_child_items(m_stealth_percent);
            }
        }
        iip = iip->next;
        if (iip == inventory)
        {
            break;
        }
    }
    if (0.01f <= m_stealth_percent)
    {
        game_render_set_stealth_diffraction_enabled(true);
    }
}

// 0x00AB5CC0 (:9815)
// Takes rounds out of an inventory item (on the server, or from the server's word on a client); an emptied item leaves
// the inventory: the equipped, desired and last weapons move on, a held object falls again, the server tells the
// clients, the local player's high resolution texture request goes, and the item returns to the free list.
bool human_remove_item_from_inventory(human* hp, inventory_item* iip, int how_many, bool from_server)
{
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return false;
    }
    iip->count -= how_many;
    if (iip->count > 0)
    {
        return false;
    }
    if (iip == hp->equipped_inv_item)
    {
        if (iip == hp->desired_equipped_inv_item)
        {
            hp->set_equipped_inv_item(hp->last_equipped_weapon);
            hp->desired_equipped_inv_item = hp->last_equipped_weapon;
            hp->last_equipped_weapon = hp->second_last_equipped_weapon;
            hp->second_last_equipped_weapon = nullptr;
        }
        else
        {
            hp->set_equipped_inv_item(hp->desired_equipped_inv_item);
        }
    }
    if (iip == hp->desired_equipped_inv_item)
    {
        hp->desired_equipped_inv_item = hp->equipped_inv_item;
    }
    if (iip == hp->last_equipped_weapon)
    {
        hp->last_equipped_weapon = hp->second_last_equipped_weapon;
        hp->second_last_equipped_weapon = nullptr;
    }
    if (iip == hp->second_last_equipped_weapon)
    {
        hp->second_last_equipped_weapon = nullptr;
    }
    if (hp->inventory == iip)
    {
        if (iip->next == iip)
        {
            iip->next = nullptr;
            iip->prev = nullptr;
            hp->inventory = nullptr;
            goto unlinked;
        }
        hp->inventory = iip->next;
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
unlinked:
    item* ip = nullptr;
    if (iip->weapon_handle != 0xFFFFFFFF)
    {
        ip = (item*)World.object_from_handle(iip->weapon_handle);
        if (ip == nullptr || ip->get_obj_type() != OT_ITEM || ip->get_sub_type() != OT_SUB_ITEM_WEAPON)
        {
            ip = nullptr;
        }
        else if (((weapon*)ip)->w_info->flags.non_inventory)
        {
            // A held object falls again (the carry force is taken off).
            const vector gravity(0.0f, -9.8f, 0.0f);
            havok_body_add_custom_force(ip->havok_handle, gravity);
            ip->set_opacity_plus_child_items(1.0f);
        }
    }
    if (Multiplayer == true && multi_is_server() && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        multi_game_player_remove_weapon((player*)hp, iip);
    }
    // Original: the weapon is not null-checked here.
    if (hp == Player && ip->info->special_flags.high_res_texture_requested)
    {
        ip->info->special_flags.high_res_texture_request_unload = true;
    }
    iip->weapon_handle = 0xFFFFFFFF;
    Num_inv_items--;
    iip->info = nullptr;
    if (Inv_item_free_list == nullptr)
    {
        iip->next = iip;
        iip->prev = iip;
        Inv_item_free_list = iip;
        return true;
    }
    iip->next = Inv_item_free_list;
    iip->prev = Inv_item_free_list->prev;
    Inv_item_free_list->prev->next = iip;
    Inv_item_free_list->prev = iip;
    return true;
}

// ---- pushing and ragdolling other humans (MyCharacterListener::characterInteractionCallback)

static float Character_collision_player_push_human_speed; // 0x03017C70, from tweak_table.xtbl
static tweak_table_item Tweak_table_item_Character_collision_player_push_human_speed("Character_collision_player_push_human_speed", &Character_collision_player_push_human_speed); // 0x034264D8

// A human leaving a vehicle (not ejected) in its vehicle interaction.
static bool human_is_exiting_vehicle(const human* hp)
{
    if (hp->vehicle_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(hp->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return false;
    }
    vehicle* vp = (vehicle*)objp;
    const int idx = vp->vii.find_human(hp);
    if (idx < 0)
    {
        return false;
    }
    const vi_human_manager& hm = vp->vii.humans[idx];
    return hm.interaction == VI_INTERACTION_EXIT && hm.state != VI_STATE_EXIT_EJECT;
}

// 0x00A2EF50 (:16677): never in multiplayer or Wrecking Crew; not humans that push others, EDF, marauders, the
// specialists or humans without flinches and ragdolls, nor npcs with a scripted behavior.
bool human_can_ordinarily_be_pushed(const human* hp)
{
    if (hp == nullptr || Multiplayer || wrecking_crew_is_active())
    {
        return false;
    }
    if (hp->hflags.pushes_other_humans || hp->current_team == HUMAN_TEAM_EDF || hp->current_team == HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    if (hp->h_info->flags.guerrilla_tech || hp->h_info->flags.guerrilla_rfc || hp->h_info->flags.old_coot || hp->hflags.disallow_flinches_and_ragdolls)
    {
        return false;
    }
    const npc* np = World.object_cast<npc>((object*)hp);
    return np == nullptr || np->get_ai()->custom_behavior == AI_SB_NONE;
}

// 0x00A7E250 (:16721): and not while attached to an animation, in a melee move, firing, on a turret, flinching,
// ragdolled, in or leaving a vehicle, trapped or oppressing.
bool human_can_currently_be_pushed(const human* hp)
{
    if (!human_can_ordinarily_be_pushed(hp))
    {
        return false;
    }
    if (hp->char_inst != nullptr && hp->char_inst->vp != nullptr && hp->char_inst->vp->attached_object != nullptr)
    {
        return false;
    }
    if (hp->combat.cur_melee_combat_move != nullptr)
    {
        return false;
    }
    const weapon* wp = hp->get_equipped_weapon();
    if (wp != nullptr && wp->w_info != nullptr && wp->w_info->is_firearm() && wp->is_trigger_pulled())
    {
        return false;
    }
    if (human_combat_get_manned_turret(hp) != nullptr)
    {
        return false;
    }
    if (hp->char_inst != nullptr && hp->char_inst->is_playing_action(ANIM_ALIAS_FLINCH))
    {
        return false;
    }
    if (hp->ragdoll_idx >= 0 || human_in_vehicle(hp) != nullptr || human_is_exiting_vehicle(hp))
    {
        return false;
    }
    const npc* np = World.object_cast<npc>((object*)hp);
    if (np != nullptr)
    {
        const action_list* plan = np->get_ai()->planner.plan;
        const ai_action* act = plan == nullptr ? nullptr : plan->act;
        if (act == &custom_trapped_action::the_instance || np->get_ai()->oppression_action != AI_OPPRESSION_NONE)
        {
            return false;
        }
    }
    return true;
}

// 0x00A7E3A0 (:16784): single player only; the player at Character_collision_player_push_human_speed, or a running
// npc at that speed, ragdolls a cowering human that can be pushed.
bool human_should_ragdoll_other_human(const human* hp, const human* other_hp)
{
    if (hp == nullptr || other_hp == nullptr || Multiplayer || wrecking_crew_is_active() || !human_can_currently_be_pushed(other_hp))
    {
        return false;
    }
    if (object_is_player(hp))
    {
        if (hp->actual_velocity_mag < Character_collision_player_push_human_speed)
        {
            return false;
        }
    }
    else if (hp->cur_movement_state < HMS_RUN || hp->actual_velocity_mag < Character_collision_player_push_human_speed)
    {
        return false;
    }
    if (other_hp->char_inst == nullptr)
    {
        return false;
    }
    return other_hp->char_inst->is_in_state(ANIMATION_STATE_COWER_STAND) || other_hp->char_inst->is_in_state(ANIMATION_STATE_COWER_CROUCH) ||
           other_hp->char_inst->is_in_state(ANIMATION_STATE_CROUCH_COWER_1) || other_hp->char_inst->is_in_state(ANIMATION_STATE_CROUCH_COWER_2);
}

// 0x00A7E490 (:16841): single player only. A human leaving a vehicle always pushes; so does a faster passenger of the
// same vehicle. Otherwise the other human must be pushable: the player pushes above
// Character_collision_player_push_human_speed; an npc that cannot ordinarily be pushed always pushes a running human;
// otherwise a moving human pushes when marked pushes_other_humans, else only non-guerrillas while running, at a faster
// movement state, or at the same one unless the other faces it while it does not face the other (dots against 0.3927).
bool human_should_push_other_human(const human* hp, const human* other_hp)
{
    if (hp == nullptr || other_hp == nullptr || Multiplayer)
    {
        return false;
    }
    if (wrecking_crew_is_active())
    {
        return false;
    }
    if (human_is_exiting_vehicle(hp))
    {
        return true;
    }
    if (hp->vehicle_handle != 0xFFFFFFFF && hp->vehicle_handle == other_hp->vehicle_handle && hp->move_speed > other_hp->move_speed)
    {
        return true;
    }
    if (!human_can_currently_be_pushed(other_hp))
    {
        return false;
    }
    if (object_is_player(hp))
    {
        return hp->actual_velocity_mag > Character_collision_player_push_human_speed;
    }
    if (!human_can_ordinarily_be_pushed(hp) && other_hp->cur_movement_state >= HMS_RUN)
    {
        return true;
    }
    const human_movement_states state = hp->cur_movement_state;
    if (state == HMS_IDLE)
    {
        return false;
    }
    if (hp->hflags.pushes_other_humans)
    {
        return true;
    }
    if (other_hp->current_team == HUMAN_TEAM_GUERILLA || state < HMS_RUN)
    {
        return false;
    }
    if (state > other_hp->cur_movement_state)
    {
        return true;
    }
    if (state != other_hp->cur_movement_state)
    {
        return false;
    }
    vector diff = hp->pos - other_hp->pos;
    diff.normalize();
    // Magic number: 0.3926991 = pi / 8.
    if (other_hp->orient.fvec * diff >= 0.3926991f && hp->orient.fvec * -diff <= 0.3926991f)
    {
        return false;
    }
    return true;
}

#if 0
// ---- not ported yet (the rest of the recovered stub list)
static const float COVER_NODE_POPUP_HEIGHT_ADJUSTMENT = 0.12f; // 0x01311870

// LTCG: arguments in EAX (not mapped yet).
static object_hit_location hit_location_from_body_part(character_body_part_id body_part_id); // 0x00A06C70 (:1215)
// LTCG: arguments in EAX (not mapped yet).
static human_voice_persona* get_test_voice_persona(human_teams team); // 0x00A52AD0 (:1250)
static bool human_should_process_controller(const human* hp); // 0x00A06CE0 (:1266)
// LTCG: arguments in EAX,ECX (not mapped yet).
static void human_process_controllers_integrate_st(human_integration_parameters* human_integration_params, unsigned int num_humans, float time_step); // 0x00A2E260 (:6512)
static float get_ik_blend_time(const human* hp, ik_type type); // 0x00A08810 (:14081)
#endif

// 0x00A2EA00 (:human.cpp)
bool human_is_dead(unsigned int handle)
{
    // An object that is gone, or not a human, counts as dead.
    if (handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && !objp->is_killed())
        {
            return false;
        }
    }
    return true;
}

// 0x00A08430 (:12614): true when the human that asked has gone.
bool human::update_avoidance()
{
    if (avoidance_move_dir.is_null())
    {
        const object* requester = World.object_from_handle(avoidance_request_human);
        if (requester == nullptr)
        {
            return true;
        }
        vector dir;
        dir.x = pos.x - requester->pos.x;
        dir.y = pos.y - requester->pos.y;
        dir.z = pos.z - requester->pos.z;
        dir.normalize();
        // Original: a requester that is not a human passes null here, (obj_type != 0) - 1 & obj.
        request_avoidance(requester->get_obj_type() == OT_HUMAN ? static_cast<const human*>(requester) : nullptr, dir, ZERO_VECTOR);
    }
    return false;
}

// 0x00A08500 (:12630)
void human::do_avoidance_pause(int ms)
{
    avoidance_original_movement_state = cur_movement_state;
    avoidance_pause_timer.set(ms);
    move_speed = -1.0f;
    cur_movement_state = HMS_IDLE;
}

// 0x00A2E7A0 (:11357)
bool human::is_playing_spot_line() const
{
    // Numeric in rfg.exe: the three spot lines, 76 to 78.
    return !voice_time_since_finish.valid() && 75 < (int)situational_voice_line && (int)situational_voice_line < 79;
}

// 0x005DC130 (.h:1096)
void human::acknowledge_human(int ack_ms)
{
    acknowledged_time.set(ack_ms);
}
