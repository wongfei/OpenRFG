// Original: volition/rfg/code/objects/humans/human_combat.cpp
//
// Partial (player_minimal.md P3, P5; multiplayer.md MP8): the melee table (melee.xtbl: the moves and their data per
// animation set), the move lookups and the combat runtime (melee swings, the hit sweep, flinches), and the damage to
// humans: bullet, melee and explosion hits, gibs and the nano rifle fade. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in human_combat.h.

#include "volition/rfg/code/objects/humans/human_combat.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_fire.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/aim_drift.h"
#include "volition/render_lib/code/rl_decal_parameters.h"

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <new>
#include <stdio.h>

// ---- globals and file statics compiled since P3
unsigned char Riot_shield_melee_group = 0xFF; // 0x01694365, invalid melee group
unsigned char Marauder_block_effect_idx = 0xFF; // 0x01694366, invalid effect material
unsigned int Explosion_gibs_effect = 0xFFFFFFFF; // 0x01694380, invalid effect handle
static human_melee_combat_move* Melee_combat_moves; // 0x0302F04C
static human_melee_combat_move* Dlc_melee_combat_moves; // 0x0302F050
static unsigned int Num_melee_combat_moves; // 0x0302F054
static bool Melee_in_dlc; // 0x0302F05F
static void* Melee_move_buffer; // 0x0302F060

// 0x00A0F260 (P4)
// The timestamps' constructors set them invalid (-1).
human_combat::human_combat()
{
}

// 0x00A9CB80
human_melee_combat_move::human_melee_combat_move()
    : name(nullptr)
{
}

// 0x00A7EC60
human_melee_combat_move::~human_melee_combat_move()
{
}

// 0x00A54920 (:982, LTCG: movep, move_id and reload on the stack, root in EDI, pool in ECX)
// One AnimSet entry of a move: the move data of that animation set, added to the move's table (or refreshed on a
// reload).
static void parse_melee_combat_move(human_melee_combat_move* movep, xml_element* root, human_melee_combat_move_id move_id, bool reload, mempool* pool)
{
    char anim_set_name[64];
    cf_drop_ext(anim_set_name, sizeof(anim_set_name), xml_table_get_req_string_ref(root, "AnimSetName"));
    human_melee_combat_move_data* move_data = nullptr;
    if (reload)
    {
        movep->move_data_table.get(anim_set_name, move_data);
        if (move_data == nullptr)
        {
            return;
        }
    }
    else
    {
        human_melee_combat_move_data* existing = nullptr;
        if (movep->move_data_table.get(anim_set_name, existing))
        {
            return;
        }
        move_data = (human_melee_combat_move_data*)pool->alloc(sizeof(human_melee_combat_move_data), 4);
        if (move_data == nullptr)
        {
            return;
        }
        movep->move_data_table.insert_key_data(anim_set_name, move_data);
        const unsigned int size = (unsigned int)strlen(anim_set_name) + 1;
        move_data->anim_set_name = (char*)pool->alloc(size, 1);
        strncpy_safe(move_data->anim_set_name, anim_set_name, size);
    }
    move_data->move_id = move_id;
    move_data->attack_animation = vanim_lookup_action(xml_table_get_req_string_ref(root, "AttackAnim"));
    const char* attack_point = xml_table_get_req_string_ref(root, "AttackPoint");
    if (v_stricmp(attack_point, "Right hand") == 0)
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_RIGHT_HAND;
    }
    else if (v_stricmp(attack_point, "Left hand") == 0)
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_LEFT_HAND;
    }
    else if (v_stricmp(attack_point, "Right foot") == 0)
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_RIGHT_FOOT;
    }
    else if (v_stricmp(attack_point, "Left foot") == 0)
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_LEFT_FOOT;
    }
    else if (v_stricmp(attack_point, "Weapon") == 0)
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_WEAPON;
    }
    else
    {
        move_data->attack_point = HUMAN_MELEE_ATTACK_POINT_INVALID;
        // The original formats into a thread-local buffer.
        char message[256];
        sprintf(message, "Unknown AttackPoint in melee.tbl entry %s", movep->name);
        keen::debug::nativePrintArgs(message);
    }
    xml_table_get_req_float(move_data->cast_radius, root, "HitRadius");
    move_data->flags = human_melee_combat_move_data::move_flags();
    xml_element* flags = xml_table_find_req(root, "Flags");
    for (xml_element* flag = xml_find(flags, "Flag"); flag != nullptr; flag = xml_find_next(flags, flag, "Flag"))
    {
        const char* flag_name = xml_table_get_req_string_ref(flag, nullptr);
        if (v_stricmp("always knock down", flag_name) == 0)
        {
            move_data->flags.always_knock_down = true;
        }
        else if (v_stricmp("npc knockback", flag_name) == 0)
        {
            move_data->flags.npc_knockback = true;
        }
        else if (v_stricmp("structural damage", flag_name) == 0)
        {
            move_data->flags.structural_damage = true;
        }
        else if (v_stricmp("no orient correction", flag_name) == 0)
        {
            move_data->flags.no_orient_correction = true;
        }
    }
    xml_element* secondary = xml_table_find_opt(root, "SecondaryImpact");
    if (secondary == nullptr)
    {
        move_data->secondary_cast_range = 1.0f;
        move_data->secondary_cast_height = 1.0f;
    }
    else
    {
        move_data->flags.secondary_impact = true;
        xml_table_get_req_float(move_data->secondary_cast_range, secondary, "HitRange");
        xml_table_get_req_float(move_data->secondary_cast_height, secondary, "HitHeight");
    }
    xml_element* ai = xml_table_find_opt(root, "AI");
    if (ai == nullptr)
    {
        move_data->ai_min_dist = 0.0f;
        move_data->ai_max_dist = 1.0f;
    }
    else
    {
        xml_table_get_req_float(move_data->ai_min_dist, ai, "MinAttackDist");
        xml_table_get_req_float(move_data->ai_max_dist, ai, "MaxAttackDist");
    }
    xml_element* damage = xml_table_find_req(root, "Damage");
    xml_table_get_req_float(move_data->default_damage, damage, "DefaultDamage");
    move_data->damage_to_npcs = move_data->default_damage;
    move_data->damage_to_players_from_npc = move_data->default_damage;
    move_data->damage_to_vehicles = move_data->default_damage;
    move_data->damage_to_structures = move_data->default_damage;
    xml_table_get_opt_float(move_data->damage_to_npcs, damage, "NPCDamage");
    xml_table_get_opt_float(move_data->damage_to_players_from_npc, damage, "PlayerDamageFromNPC");
    xml_table_get_opt_float(move_data->damage_to_vehicles, damage, "VehicleDamage");
    xml_table_get_opt_float(move_data->damage_to_structures, damage, "StructureDamage");
    if (!xml_table_get_opt_float(move_data->damage_to_players_from_player, damage, "PlayerDamageFromPlayer"))
    {
        move_data->damage_to_players_from_player = move_data->damage_to_players_from_npc;
    }
    xml_table_get_req_float(move_data->impact_impulse, root, "ImpactForce");
    xml_table_get_req_float(move_data->ragdoll_impulse, root, "RagdollImpulse");
    xml_table_get_req_float(move_data->away_impulse_scale, root, "AwayImpulseScale");
    move_data->impact_camera_shake = camera_shake_get(xml_table_get_req_string_ref(root, "CameraShake"));
}

// 0x00AB2A80 (:1119)
// melee.xtbl (or a DLC's dlcNN_melee.xtbl into the level pool): one human_melee_combat_move per MeleeMove, with the
// move data of each AnimSet. The move ids count only the moves that have a MoveDataList (original: the name of a move
// without one is overwritten by the next move).
void parse_melee_table(int dlc_id, bool reload)
{
    Melee_in_dlc = dlc_id != -1;
    mempool* pool;
    char filename[256];
    if (Melee_in_dlc)
    {
        pool = &Level_mempool;
        sprintf(filename, "dlc%02d_%s", dlc_id, "melee.xtbl");
        if (!cf_exist(filename))
        {
            // Without the DLC table the base table is read into the level pool.
            strcpy(filename, "melee.xtbl");
        }
    }
    else
    {
        pool = &Permanent_mempool;
        strcpy(filename, "melee.xtbl");
    }
    xml_element* table = xml_table_parse(filename);
    const unsigned int count = (unsigned int)xml_table_count(table, "MeleeMove");
    if (reload)
    {
        // The original does not close the table on this return.
        if (count != Num_melee_combat_moves)
        {
            return;
        }
    }
    else
    {
        Num_melee_combat_moves = count;
        if (!Melee_in_dlc && Melee_move_buffer != nullptr)
        {
            xml_table_close(table);
            return;
        }
        // The original allocates count * 0xDC bytes for the new[] array and writes its count in front of it, so the
        // array runs 4 bytes past the block. The allocation here includes the count.
        unsigned int* buffer = (unsigned int*)pool->alloc(sizeof(unsigned int) + count * sizeof(human_melee_combat_move), 0);
        if (buffer == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            Num_melee_combat_moves = 0;
            return;
        }
        *buffer = Num_melee_combat_moves;
        human_melee_combat_move* moves = (human_melee_combat_move*)(buffer + 1);
        for (unsigned int i = 0; i < count; i++)
        {
            new (&moves[i]) human_melee_combat_move();
        }
        if (Melee_in_dlc)
        {
            Dlc_melee_combat_moves = moves;
        }
        else
        {
            Melee_move_buffer = buffer;
            Dlc_melee_combat_moves = nullptr;
            Melee_combat_moves = moves;
        }
    }
    int move_index = 0;
    for (xml_element* move = xml_find(table, "MeleeMove"); move != nullptr; move = xml_find_next(table, move, "MeleeMove"))
    {
        const char* name = xml_table_get_req_string_ref(move, "Name");
        human_melee_combat_move* moves = Melee_in_dlc ? Dlc_melee_combat_moves : Melee_combat_moves;
        if (!reload)
        {
            const unsigned int size = (unsigned int)strlen(name) + 1;
            char* name_copy = (char*)pool->alloc(size, 1);
            strncpy_safe(name_copy, name, size);
            moves[move_index].name = name_copy;
        }
        xml_element* data_list = xml_find(move, "MoveDataList");
        if (data_list != nullptr)
        {
            for (xml_element* anim_set = xml_find(data_list, "AnimSet"); anim_set != nullptr; anim_set = xml_find_next(data_list, anim_set, "AnimSet"))
            {
                parse_melee_combat_move(&moves[move_index], anim_set, (human_melee_combat_move_id)move_index, reload, pool);
            }
            move_index++;
        }
    }
    xml_table_close(table);
}

// 0x00AB2DA0 (:1418): human_system_init has the same steps inlined and is the caller at boot.
void human_combat_system_init()
{
    parse_melee_table(-1, false);
    Human_melee_blood_decal_id = decal_info_find("Test_decal_blood");
    Explosion_gibs_effect = effects_get_handle("char_blood_gibs_a");
    Riot_shield_melee_group = rfg_get_melee_group_index("Riot_shield");
    Marauder_block_effect_idx = rfg_get_effect_material_index("Steel");
}

// 0x00A09C00 (:3505): by move name (case insensitive), in the DLC table while one is loaded.
human_melee_combat_move_id human_combat_lookup_combat_move(const char* name)
{
    if (name == nullptr)
    {
        return INVALID_MELEE_COMBAT_MOVE_ID;
    }
    for (unsigned int move_id = 0; move_id < Num_melee_combat_moves; move_id++)
    {
        const human_melee_combat_move* moves = Melee_in_dlc ? Dlc_melee_combat_moves : Melee_combat_moves;
        if (v_stricmp(moves[move_id].name, name) == 0)
        {
            return (human_melee_combat_move_id)move_id;
        }
    }
    return INVALID_MELEE_COMBAT_MOVE_ID;
}

// 0x00A55510 (:3533): the move's data for the human's animation set, else for its parent sets.
const human_melee_combat_move_data* human_combat_move_data_from_index(const human* hp, human_melee_combat_move_id move_id)
{
    if (hp == nullptr || hp->h_info == nullptr || (int)move_id < 0 || move_id == INVALID_MELEE_COMBAT_MOVE_ID || hp->char_inst == nullptr)
    {
        return nullptr;
    }
    human_melee_combat_move* moves = Melee_in_dlc ? Dlc_melee_combat_moves : Melee_combat_moves;
    for (anim_set* set = hp->char_inst->vp->animation_set; set != nullptr; set = set->parent)
    {
        human_melee_combat_move_data* move_data = nullptr;
        moves[move_id].move_data_table.get(set->name, move_data);
        if (move_data != nullptr)
        {
            return move_data;
        }
    }
    return nullptr;
}

// 0x00A09C70 (:3589)
unsigned int human_combat_get_num_melee_moves()
{
    return Num_melee_combat_moves;
}

// ---- the combat runtime (player_minimal.md P5): melee swings, the hit sweep, flinches

// ---- file statics compiled since P5
static float Super_hammer_ragdoll_impulse = 2.5f; // 0x01694334
static float Super_hammer_vehicle_impulse = 15.0f; // 0x01694338
static float Super_hammer_damage = 3.0f; // 0x0169433C
static float Super_hammer_upward_bonus = 0.3f; // 0x01694340
static float Damage_multiplier_npc_offscreen = 0.1f; // 0x01694348
static float Damage_multiplier_npc_onscreen = 0.75f; // 0x0169434C
static float Damage_multiplier_npc_reduced = 0.1f; // 0x01694350
static float Secondary_melee_angle = 0.5f; // 0x01694354
static float Mp_melee_structure_multiplier = 1.0f; // 0x01694358
static float Damage_multiplier_melee_heavy_armor = 0.4f; // 0x0169435C
static float Melee_continuous_decay_min_percent = 0.2f; // 0x0169436C
static int Melee_continuous_decay_hit_count = 100; // 0x01694370
static float Melee_running_distance; // 0x03017C64
static float Max_large_flinch_damage; // 0x0302365C
static float Camera_melee_turn_time; // 0x03023860
static float Mp_head_damage_multiplier; // 0x03024E60
static float Camera_melee_turn_angle; // 0x03024E64
static float Camera_melee_turn_distance; // 0x03024E68
static float Min_large_flinch_damage; // 0x03024E88
static float Mp_leg_damage_multiplier; // 0x030266A4
// A debug console toggle (the dcf is not ported): the name of each move the player plays goes to the HUD.
static bool Show_player_melee_combat_move_names; // 0x0302F05D

// The tweak table items of this file (tweak_table.xtbl overrides them; until it is read the values above stay).
static tweak_table_item Tweak_table_item_Damage_multiplier_npc_offscreen("Damage_multiplier_npc_offscreen", &Damage_multiplier_npc_offscreen); // 0x031A017C
static tweak_table_item Tweak_table_item_Damage_multiplier_npc_onscreen("Damage_multiplier_npc_onscreen", &Damage_multiplier_npc_onscreen); // 0x03067FB4
static tweak_table_item Tweak_table_item_Damage_multiplier_npc_reduced("Damage_multiplier_npc_reduced", &Damage_multiplier_npc_reduced); // 0x033C67D0
static tweak_table_item Tweak_table_item_Mp_melee_structure_multiplier("Mp_melee_structure_multiplier", &Mp_melee_structure_multiplier); // 0x0316966C
static tweak_table_item Tweak_table_item_Damage_multiplier_melee_heavy_armor("Damage_multiplier_melee_heavy_armor", &Damage_multiplier_melee_heavy_armor); // 0x031A2B14
static tweak_table_item Tweak_table_item_Mp_head_damage_multiplier("Mp_head_damage_multiplier", &Mp_head_damage_multiplier); // 0x034309C8
static tweak_table_item Tweak_table_item_Mp_leg_damage_multiplier("Mp_leg_damage_multiplier", &Mp_leg_damage_multiplier); // 0x033B1FA8
static tweak_table_item Tweak_table_item_Camera_melee_turn_angle("Camera_melee_turn_angle", &Camera_melee_turn_angle); // 0x0304B0D8
static tweak_table_item Tweak_table_item_Camera_melee_turn_time("Camera_melee_turn_time", &Camera_melee_turn_time); // 0x03500C74
static tweak_table_item Tweak_table_item_Camera_melee_turn_distance("Camera_melee_turn_distance", &Camera_melee_turn_distance); // 0x03425EA0
// Registered in rfg.exe, read by nothing there.
static float Camera_melee_max_turn_rate; // 0x0301AE48
static tweak_table_item Tweak_table_item_Camera_melee_max_turn_rate("Camera_melee_max_turn_rate", &Camera_melee_max_turn_rate); // 0x0339FB40
static tweak_table_item Tweak_table_item_Melee_running_distance("Melee_running_distance", &Melee_running_distance); // 0x033B4E5C
static tweak_table_item Tweak_table_item_Min_large_flinch_damage("Min_large_flinch_damage", &Min_large_flinch_damage); // 0x034309D4
static tweak_table_item Tweak_table_item_Max_large_flinch_damage("Max_large_flinch_damage", &Max_large_flinch_damage); // 0x0304B0C0

// The filter parameters of the melee spherecast (process_combat_hit_callback). TU-private, unnamed in the PDB; the
// layout is the binary's stack block.
// size 0x24
struct melee_hit_filter_info
{
    human* hp; // 0x00, the attacker
    vector start; // 0x04, the cast start
    vector dir; // 0x10, the normalized cast direction
    float radius; // 0x1C, the cast radius
    bool collide_ground; // 0x20, false for the secondary impact cast: zones (the ground) do not count
};

// 0x00A7E6F0 (:365, LTCG: hp in ESI). Static in rfg.exe; external here for its friend declarations in the ai headers.
bool human_combat_can_play_large_flinch_or_stumble(const human* hp)
{
    if (hp->cur_stance == HST_PRONE || (hp->cur_stance == HST_CROUCH && hp->cur_movement_state == HMS_IDLE))
    {
        return false;
    }
    const human_move_modes mode = hp->movement_mode;
    if (mode == HMM_FALL || mode == HMM_JUMP || mode == HMM_KNOCKDOWN || hp->animation_flags.back || hp->animation_flags.backpedal ||
        hp->cur_state == HS_COVER || human_combat_is_manning_turret(hp) || hp->action_node_data.current_action_node != 0xFFFFFFFF ||
        human_in_vehicle_interaction(hp, nullptr) || hp->char_inst->is_playing_action(ANIM_ALIAS_DODGE) ||
        hp->char_inst->is_playing_action(ANIM_ALIAS_EXPLOSION_KNOCKDOWN))
    {
        return false;
    }
    const npc* np = World.object_cast<npc>(const_cast<human*>(hp));
    if (np != nullptr)
    {
        if (np->get_ai()->master_handle != 0xFFFFFFFF)
        {
            return false;
        }
        if (np->hflags.activity_ha_hostage)
        {
            const action_list* plan = np->get_ai()->planner.plan;
            const ai_action* act = plan != nullptr ? plan->act : nullptr;
            if (act == &goto_vehicle_action::the_instance || act == &enter_vehicle_action::the_instance)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x00A095D0 (:442)
animation_action human_combat_get_small_flinch_animation(const human* hp, flinch_directions flinch_dir, object_hit_location hit_location)
{
    // Magic numbers: the flinch actions come in groups of four, one per flinch direction: 0x1F0 crouching, 0x1E4 head,
    // 0x1E8 torso and arms, 0x1EC legs.
    if (hp->cur_stance == HST_CROUCH)
    {
        return (animation_action)(flinch_dir + 0x1F0);
    }
    if (hp->cur_stance == HST_PRONE && hp->hflags.in_fetal_position)
    {
        return ANIMATION_ACTION_FETAL_FLINCH;
    }
    switch (hit_location)
    {
    case HIT_GENERAL:
    case HIT_TORSO:
    case HIT_LEFT_ARM:
    case HIT_RIGHT_ARM:
        return (animation_action)(flinch_dir + 0x1E8);
    case HIT_HEAD:
        return (animation_action)(flinch_dir + 0x1E4);
    default:
        return (animation_action)(flinch_dir + 0x1EC);
    }
}

// 0x00A09650 (:486)
animation_action human_combat_get_large_flinch_animation(const human* hp, flinch_directions flinch_dir, object_hit_location hit_location, bool in_combat)
{
    // Magic numbers: groups of four as above; in combat 0x1D7 head, 0x1DB torso and arms, 0x1DF legs; out of combat
    // 0x1F8 upper body, 0x1F4 legs.
    if (in_combat)
    {
        switch (hit_location)
        {
        case HIT_GENERAL:
        case HIT_TORSO:
        case HIT_LEFT_ARM:
        case HIT_RIGHT_ARM:
            return (animation_action)(flinch_dir + 0x1DB);
        case HIT_HEAD:
            return (animation_action)(flinch_dir + 0x1D7);
        default:
            return (animation_action)(flinch_dir + 0x1DF);
        }
    }
    switch (hit_location)
    {
    case HIT_GENERAL:
    case HIT_TORSO:
    case HIT_LEFT_ARM:
    case HIT_RIGHT_ARM:
    case HIT_HEAD:
        return (animation_action)(flinch_dir + 0x1F8);
    default:
        return (animation_action)(flinch_dir + 0x1F4);
    }
}

// 0x00A096E0 (:591): the hit direction in the human's frame, by 45 degree sectors.
flinch_directions human_combat_get_flinch_direction(const human* hp, const vector& dir)
{
    vector local_dir;
    local_dir = dir.copy_rotate(hp->orient);
    local_dir.flatten_normalize_safe();
    // Magic number: 0.70710677, cos(45 degrees).
    if (local_dir.z < -0.70710677f)
    {
        return FLINCH_DIR_FRONT;
    }
    if (0.70710677f < local_dir.z)
    {
        return FLINCH_DIR_BACK;
    }
    return local_dir.x <= 0.70710677f ? FLINCH_DIR_RIGHT : FLINCH_DIR_LEFT;
}

// 0x00A7E800 (:616)
void human_combat_play_flinch(human* hp, const vector& pos, const vector& hit_dir, object_hit_location hit_location, unsigned int large_flinch_percent)
{
    if (Multiplayer && hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    if (hp->h_info->flags.old_coot)
    {
        human_combat_play_coot_flinch(hp, hit_dir);
        return;
    }
    if (hp->hflags.disallow_flinches_and_ragdolls)
    {
        return;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && ((const npc*)hp)->npc_flags.tied_up)
    {
        return;
    }
    const human_states state = hp->cur_state;
    if (state == HS_VEHICLE_ENTERING || state == HS_VEHICLE_SITTING || state == HS_VEHICLE_DRIVING || state == HS_VEHICLE_EXITING || human_is_ragdolled(hp))
    {
        return;
    }
    if (hp->cur_stance == HST_PRONE && !hp->hflags.in_fetal_position)
    {
        return;
    }
    if (human_is_dead(hp) || hp->action_node_data.current_action_node != 0xFFFFFFFF)
    {
        return;
    }
    npc* np = World.object_cast<npc>(hp);
    if (np != nullptr)
    {
        // Magic number: 0xF2, the stunned animation the flinch would interrupt.
        if ((np->get_ai() != nullptr && np->get_ai()->get_stunned_animation() == 0xF2) || np->npc_flags.use_corpse_vehicle_exit)
        {
            return;
        }
    }
    const flinch_directions flinch_dir = human_combat_get_flinch_direction(hp, hit_dir);
    bool can_flinch = false;
    bool large = false;
    const bool can_large = human_combat_can_play_large_flinch_or_stumble(hp);
    const bool in_combat = human_is_combat_ready(hp);
    if (!hp->combat.flinch_timestamp.valid() || hp->combat.flinch_timestamp.elapsed())
    {
        can_flinch = true;
        if (can_large && (int_rand_chance(large_flinch_percent) || hp->current_team == HUMAN_TEAM_CIVILIAN || !in_combat))
        {
            large = true;
        }
    }
    // Body armor shrugs off the small flinches.
    if ((hp->h_info->flags.body_armor && !large) || !can_flinch)
    {
        return;
    }
    const animation_action anim_index = large ? human_combat_get_large_flinch_animation(hp, flinch_dir, hit_location, in_combat)
                                              : human_combat_get_small_flinch_animation(hp, flinch_dir, hit_location);
    hp->char_inst->stop_action(ANIM_ALIAS_FIRE, 2, false);
    hp->char_inst->stop_action(ANIM_ALIAS_THROW, 2, false);
    if (anim_index == ANIMATION_ACTION_NONE)
    {
        return;
    }
    // Magic number: 0x2000, the action flag a small flinch plays with (a large one plays without it).
    hp->char_inst->play_action(anim_index, ANIM_ALIAS_FLINCH, 1.0f, large ? 0 : 0x2000);
    hp->char_inst->stop_action(ANIM_ALIAS_FACIAL_MORPH, 3, false);
    hp->char_inst->play_action(ANIMATION_ACTION_FACIAL_FLINCH, ANIM_ALIAS_FACIAL_MORPH, 1.0f, 0);
    // The x87 length times the double 1000.0, truncated.
    hp->combat.flinch_timestamp.set((int)((double)vanim_get_anim_length(hp->char_inst->vp, anim_index) * 1000.0));
    if (np != nullptr)
    {
        ai_system* ai = np->get_ai();
        ai->maybe_switch_to_wounded_animation(flinch_dir, hit_location);
        if (large)
        {
            ai->start_stumble(anim_index);
        }
    }
}

// 0x00A09750 (:736): the old coot (the Mars rancher) has one flinch per side and none from behind.
void human_combat_play_coot_flinch(human* hp, const vector& hit_dir)
{
    vector local_dir;
    local_dir = hit_dir.copy_rotate(hp->orient);
    local_dir.flatten_normalize_safe();
    animation_action anim;
    // Magic number: 0.70710677, cos(45 degrees); front and back play the same flinch.
    if (local_dir.z < -0.70710677f || 0.70710677f < local_dir.z)
    {
        anim = ANIMATION_ACTION_OLD_COOT_FLINCH;
    }
    else if (0.70710677f < local_dir.x)
    {
        anim = ANIMATION_ACTION_OLD_COOT_FLINCH_LEFT;
    }
    else
    {
        anim = ANIMATION_ACTION_OLD_COOT_FLINCH_RIGHT;
    }
    hp->char_inst->play_action(anim, ANIM_ALIAS_FLINCH, 1.0f, 0x2000);
    hp->char_inst->stop_action(ANIM_ALIAS_FACIAL_MORPH, 3, false);
    hp->char_inst->play_action(ANIMATION_ACTION_FACIAL_FLINCH, ANIM_ALIAS_FACIAL_MORPH, 1.0f, 0);
}

// 0x00A09840 (:763, LTCG: hp in EAX)
static void human_combat_clear_combat_move(human* hp)
{
    hp->combat.cur_melee_combat_move = nullptr;
}

// 0x00A7EAE0 (:778, LTCG: attacker in EDI, the rest on the stack)
// The world positions of the move's attack points: the weapon's two attack tags (count 2 when the second exists), or
// the hand or foot tag of the human. Without a move or a weapon the original reports an Int3 and uses the equip tag.
static bool get_melee_attack_pos(const human* attacker, vector& pos1, vector& pos2, int& count)
{
    count = 0;
    // human::get_equipped_weapon, inlined.
    object* weapon_obj = attacker->get_equipped_weapon();
    int tag_index;
    const human_melee_combat_move_data* move = attacker->combat.cur_melee_combat_move;
    if (move == nullptr || weapon_obj == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/rfg/code/objects/humans/human_combat.cpp", 847);
        tag_index = attacker->equip_tag_index;
    }
    else if (move->attack_point == HUMAN_MELEE_ATTACK_POINT_WEAPON)
    {
        // The weapon's attack tags: the first two that exist of the destruction tags, then the muzzle and grip tags.
        const weapon* wp = (const weapon*)weapon_obj;
        int second = -1;
        int first = wp->get_destruction_tag_index();
        if (first == -1)
        {
            first = wp->get_secondary_destruction_tag_index();
        }
        else
        {
            second = wp->get_secondary_destruction_tag_index();
        }
        if (first == -1)
        {
            first = wp->get_muzzle_tag_index();
            if (first == -1)
            {
                first = wp->get_grip_tag_index();
            }
            else
            {
                second = wp->get_grip_tag_index();
            }
            if (first == -1)
            {
                return false;
            }
        }
        count = 1;
        weapon_obj->get_tag_world_coords(first, pos1, nullptr, nullptr, nullptr);
        if (second != -1)
        {
            count = 2;
            weapon_obj->get_tag_world_coords(second, pos2, nullptr, nullptr, nullptr);
        }
        return true;
    }
    else if (move->attack_point == HUMAN_MELEE_ATTACK_POINT_LEFT_HAND)
    {
        tag_index = attacker->equip_offhand_tag_index;
    }
    else if (move->attack_point == HUMAN_MELEE_ATTACK_POINT_RIGHT_FOOT)
    {
        tag_index = attacker->right_foot_bone_index;
    }
    else if (move->attack_point == HUMAN_MELEE_ATTACK_POINT_LEFT_FOOT)
    {
        tag_index = attacker->left_foot_bone_index;
    }
    else
    {
        tag_index = attacker->equip_tag_index;
    }
    count = 1;
    animlib_get_tag_world_coords(attacker->char_inst->aim_handle, tag_index, attacker->pos, attacker->orient, pos1, nullptr);
    return true;
}

// 0x00A09850 (:1237, LTCG: attacker in ESI, move_data in EDX, target in ECX, result in XMM0)
// The move's damage for the target's kind, decayed by the objects this swing already hit (down to
// Melee_continuous_decay_min_percent after Melee_continuous_decay_hit_count hits). The binary computes it in double.
static float human_combat_get_melee_damage(const human* attacker, const human_melee_combat_move_data* move_data, const object* target)
{
    if (move_data == nullptr)
    {
        // Magic number: 10, the damage without a move.
        return 10.0f;
    }
    float damage;
    switch (target != nullptr ? (int)target->get_obj_type() : -1)
    {
    case OT_HUMAN:
        if (target->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            damage = attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? move_data->damage_to_players_from_player : move_data->damage_to_players_from_npc;
        }
        else
        {
            damage = move_data->damage_to_npcs;
        }
        break;
    case OT_MOVER:
        damage = Multiplayer ? (float)((double)move_data->damage_to_structures * (double)Mp_melee_structure_multiplier) : move_data->damage_to_structures;
        break;
    case OT_VEHICLE:
        damage = move_data->damage_to_vehicles;
        break;
    default:
        damage = move_data->default_damage;
        break;
    }
    float decay = (float)((double)(float)attacker->combat.hit_object_count / (double)(float)Melee_continuous_decay_hit_count);
    if ((float)((double)decay - 0.0) < 0.0f)
    {
        decay = 0.0f;
    }
    if ((float)((double)decay - 1.0) >= 0.0f)
    {
        decay = 1.0f;
    }
    const double min_percent = (double)Melee_continuous_decay_min_percent;
    return (float)(((1.0 - min_percent) * (1.0 - (double)decay) + min_percent) * (double)damage);
}

// 0x00A09970 (:1444)
void human_combat_init(human_combat* hcp)
{
    hcp->cur_melee_combat_move = nullptr;
    hcp->hold_trigger_timestamp.invalidate();
    hcp->shoot_enemy_timestamp.invalidate();
    hcp->melee_attack_interval_timer.invalidate();
    hcp->move_to_melee_timestamp.invalidate();
    hcp->combat_ready_timestamp.invalidate();
    hcp->dodge_timestamp.set(0);
    hcp->combat_maneuver_timestamp.invalidate();
    hcp->no_path_to_target_timestamp.set(0);
    hcp->last_killed_time_ms = -1;
    hcp->melee_delay_timer.invalidate();
    hcp->beaten_up_timer.invalidate();
    hcp->pursuit_timestamp.invalidate();
    hcp->last_pursuit_timestamp.invalidate();
    hcp->current_flank_offset = 0.0f;
    hcp->flank_delta = 0.0f;
    hcp->last_hit_by_handle = 0xFFFFFFFF;
    hcp->last_hit_by_other_handle = 0xFFFFFFFF;
    hcp->last_hit_by_weapon_handle = 0xFFFFFFFF;
    hcp->last_hit_by_other_weapon_handle = 0xFFFFFFFF;
    hcp->last_hit_by_damage_type = DBT_NONE;
    hcp->last_hit_by_other_damage_type = DBT_NONE;
    hcp->last_hit_time_ms = -1;
    hcp->last_hit_by_other_time_ms = -1;
    hcp->last_killed_by_handle = 0xFFFFFFFF;
    hcp->last_killed_by_other_handle = 0xFFFFFFFF;
    hcp->last_killed_by_other_time_ms = -1;
    hcp->last_killed_pos = ZERO_VECTOR;
    hcp->last_kill_handle = 0xFFFFFFFF;
    hcp->last_kill_other_handle = 0xFFFFFFFF;
    // Original: last_killed_time_ms is reset twice and last_kill_time_ms is left as it was.
    hcp->last_killed_time_ms = -1;
    hcp->last_kill_other_time_ms = -1;
    hcp->target_handle = 0xFFFFFFFF;
    hcp->targets_building_handle = 0xFFFFFFFF;
    hcp->throw_grenade_timestamp.invalidate();
    hcp->throw_velocity = ZERO_VECTOR;
    hcp->thrown_weapon = 0xFFFFFFFF;
    hcp->thrown_weapon_released = false;
    hcp->last_projectile_handle = 0xFFFFFFFF;
    hcp->reserved_turret = 0xFFFFFFFF;
    hcp->reserved_cover_node = 0xFFFFFFFF;
    hcp->not_undercover_timestamp.set(0);
    hcp->pain_sound_timestamp.set(0);
}

// 0x00AD13B0 (:1535)
// One melee hit on one object: knockdowns and shoves for npcs, the damage and impulse through the target's
// process_melee_hit, and the swing stops (human_melee_cancel_attack) when the target takes no damage.
void human_combat_unified_melee_hit(object* target, human* attacker, const vector& hit_pos, const vector& hit_normal, const vector& hit_direction, const weapon_info* w_info,
                                    const human_melee_combat_move_data* attacker_cmp, unsigned int hit_shape_key, bool play_effect, bool do_check_swing)
{
    human* hp = target != nullptr && target->get_obj_type() == OT_HUMAN ? (human*)target : nullptr;
    if (attacker_cmp == nullptr)
    {
        return;
    }
    // Original: target is dereferenced unchecked from here on.
    if (target->get_obj_type() == OT_ITEM)
    {
        turret* tp = turret_from_item((item*)target);
        if (tp != nullptr)
        {
            target = tp;
        }
    }
    if (hp != nullptr)
    {
        vector shield_check_pos;
        if (attacker_cmp->flags.secondary_impact)
        {
            shield_check_pos = attacker->pos;
            shield_check_pos.y = attacker_cmp->secondary_cast_height + attacker->pos.y;
        }
        else
        {
            vector head_pos;
            human_get_head_pos(hp, head_pos);
            shield_check_pos.x = (hp->pos.x + head_pos.x) * 0.5f;
            shield_check_pos.y = (hp->pos.y + head_pos.y) * 0.5f;
            shield_check_pos.z = (hp->pos.z + head_pos.z) * 0.5f;
        }
        if (hp->is_shielded(shield_check_pos, false))
        {
            return;
        }
        if (target != nullptr && target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            npc* np = (npc*)target;
            if (np->hflags.activity_ha_hostage && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER && attacker->combat.target_handle != 0xFFFFFFFF)
            {
                return;
            }
            if (!attacker_cmp->flags.always_knock_down || np->is_invulnerable())
            {
                if (human_is_ragdolled(np) || np->hflags.in_fetal_position)
                {
                    np->combat.beaten_up_timer.set(0);
                }
                if (attacker_cmp->flags.npc_knockback)
                {
                    np->get_ai()->register_shove(attacker);
                }
            }
            else
            {
                human_ragdoll_start(np, false, attacker->get_handle());
                // Magic number: 1500 ms knocked down.
                np->knockdown_timestamp.set(1500);
                np->combat.beaten_up_timer.set(0);
            }
        }
    }
    float damage = human_combat_get_melee_damage(attacker, attacker_cmp, target);
    float impulse_scale = target->get_obj_type() == OT_HUMAN ? attacker_cmp->ragdoll_impulse : attacker_cmp->impact_impulse;
    vector bonus = ZERO_VECTOR;
    if (stats_cheat_enabled(CHEATS_SUPER_HAMMER))
    {
        const float cheat_impulse = target->get_obj_type() == OT_HUMAN ? Super_hammer_ragdoll_impulse : Super_hammer_vehicle_impulse;
        impulse_scale = cheat_impulse * impulse_scale;
        damage = Super_hammer_damage * damage;
        bonus.y = Super_hammer_upward_bonus;
    }
    const float away = attacker_cmp->away_impulse_scale;
    attacker->combat.hit_object_count++;
    // Each step rounds to float, as in the binary.
    vector impulse;
    impulse.x = ((hit_direction.x + bonus.x) + attacker->orient.fvec.x * away) * impulse_scale;
    impulse.y = ((hit_direction.y + bonus.y) + attacker->orient.fvec.y * away) * impulse_scale;
    impulse.z = ((hit_direction.z + bonus.z) + attacker->orient.fvec.z * away) * impulse_scale;
    if (Multiplayer == true && !multi_is_server() && attacker == Player && target->get_obj_type() == OT_MOVER && target->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_mover_init_damage_applied();
    }
    const bool damaged = target->process_melee_hit(attacker, damage, impulse, hit_pos, hit_normal, hit_shape_key, *w_info);
    if ((!damaged || (!Multiplayer && !wrecking_crew_is_active() && target->get_obj_type() == OT_HUMAN && ((human*)target)->h_info != nullptr &&
                      ((human*)target)->h_info->flags.body_armor)) &&
        do_check_swing)
    {
        if (attacker->get_handle() == Rfg_camera.target_handle && w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            camera_shake_play("hammer_stop", 1.0f, false);
        }
        // A player's swing stops at anything; an npc's swing goes through zones (the ground).
        if (attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER || target->get_obj_type() != OT_ZONE)
        {
            human_melee_cancel_attack(attacker);
            // Magic number: 500 ms before the next swing.
            attacker->combat.melee_delay_timer.set(500);
        }
    }
    if (attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        multi_object_backpack* backpack = ((player*)attacker)->backpack;
        if (backpack != nullptr)
        {
            if (backpack->get_backpack_abilities()->shockwave == true)
            {
                backpack->attempt_shockwave(hit_pos);
            }
            backpack = ((player*)attacker)->backpack;
            if (backpack->get_backpack_abilities()->ripple == true)
            {
                backpack->attempt_ripple(hit_pos);
            }
        }
    }
    if (Multiplayer == true && attacker == Player && (target->get_obj_type() != OT_MOVER || target->get_sub_type() != OT_SUB_MOVER_RFG || rfg_mover_get_damage_applied()))
    {
        multi_gw_send_melee_hit(attacker, target->get_handle(), w_info, hit_pos, hit_normal, hit_direction, hit_shape_key, attacker_cmp->move_id);
    }
}

// 0x00A7EC70 (:1697): the spherecast filter. Humans count where the cast meets their hit capsules; the attacker, its
// own items and (for the secondary impact) zones do not.
static bool process_combat_hit_callback(object* hit_objp, void* info)
{
    const melee_hit_filter_info* filter = (const melee_hit_filter_info*)info;
    if (hit_objp != nullptr && hit_objp->get_obj_type() == OT_HUMAN)
    {
        if (hit_objp == filter->hp || ((human*)hit_objp)->hflags.cant_hit_with_melee)
        {
            return false;
        }
        vector intersection;
        return human_get_hit_location((human*)hit_objp, filter->start, filter->dir, intersection, filter->radius, nullptr, nullptr) != HIT_NONE;
    }
    // Original: a null object is dereferenced here; the cast never reports one.
    if (!filter->collide_ground && hit_objp->get_obj_type() == OT_ZONE)
    {
        return false;
    }
    if (hit_objp->get_obj_type() == OT_ITEM && hit_objp->get_parent() == filter->hp->get_handle())
    {
        return false;
    }
    return true;
}

// 0x00AD1930 (:1741)
// Sweeps a sphere of the move's cast radius along ray_in and hits each object once: the hit closest along the cast for
// every object, humans only near the first hit. Every hit object takes the melee hit; the first one per swing plays the
// melee material effect, and a hit shakes the camera.
void human_combat_process_hits(human* hp, raycast_input& ray_in, bool ignore_ground)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    if (w_info == nullptr || hp->combat.cur_melee_combat_move == nullptr)
    {
        return;
    }
    const float radius = hp->combat.cur_melee_combat_move->cast_radius;
    melee_hit_filter_info filter;
    filter.start = ray_in.start;
    vector delta;
    delta.x = ray_in.end.x - ray_in.start.x;
    delta.y = ray_in.end.y - ray_in.start.y;
    delta.z = ray_in.end.z - ray_in.start.z;
    vector dir;
    dir = delta.copy_normalize_safe();
    filter.dir = dir;
    filter.radius = radius;
    filter.collide_ground = !ignore_ground;
    filter.hp = hp;
    ray_in.filter_callback = process_combat_hit_callback;
    ray_in.filter_params = &filter;
    raycast_output out;
    ray_in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    ray_in.radius = radius;
    int effects_played = 0;
    if (havok_spherecast(ray_in, out))
    {
        const vector ray_dir = filter.dir;
        const int num_hits = out.m_hits.getSize();
        // The first hit along the cast (the binary compares through fl_min's double difference).
        float first_fraction = 3.4028235e+38f;
        for (int i = 0; i < num_hits; i++)
        {
            const float f = out.m_hits[i].hit_fraction;
            if ((float)((double)first_fraction - (double)f) >= 0.0f)
            {
                first_fraction = f;
            }
        }
        for (int i = 0; i < num_hits; i++)
        {
            raycast_output::hit_info& hit = out.m_hits[i];
            object* victim = World.object_from_handle(hit.hit_object_handle);
            if (victim == nullptr)
            {
                continue;
            }
            const weapon* equipped = hp->get_equipped_weapon();
            if (equipped != nullptr && victim->get_handle() == equipped->get_handle())
            {
                continue;
            }
            human* victim_hp = nullptr;
            if (victim->get_obj_type() == OT_HUMAN)
            {
                victim_hp = (human*)victim;
                // Magic number: humans more than 0.1 past the first hit fraction are not hit (compared in double).
                if ((double)hit.hit_fraction > (double)first_fraction + (double)0.1f)
                {
                    hit.hit_object_handle = 0xFFFFFFFF;
                    continue;
                }
            }
            // An npc does not hit the ground it swings into (normals within 45 degrees of up).
            if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && victim->get_obj_type() == OT_ZONE && 0.70710677f < hit.hit_normal * UP_VECTOR) ||
                !multi_gw_can_hit_target(hp, victim))
            {
                hit.hit_object_handle = 0xFFFFFFFF;
                continue;
            }
            if (victim->get_sub_type() != OT_SUB_MOVER_RFG)
            {
                // Only the closest hit of each object counts (ties go to the earlier hit).
                bool closer = false;
                for (int j = 0; j < num_hits; j++)
                {
                    const raycast_output::hit_info other(out.m_hits[j]);
                    if (j != i && other.hit_object_handle == hit.hit_object_handle &&
                        (other.hit_fraction < hit.hit_fraction || (j < i && other.hit_fraction == hit.hit_fraction)))
                    {
                        closer = true;
                        break;
                    }
                }
                if (closer)
                {
                    hit.hit_object_handle = 0xFFFFFFFF;
                    continue;
                }
            }
            if (Multiplayer && object_is_local_player(hp) && object_is_player(victim))
            {
                multi_game_stats_record_hit((player*)hp, (player*)victim, w_info, DBT_MELEE);
            }
            if (victim->get_sub_type() == OT_SUB_MOVER_RFG && object_is_local_player(hp))
            {
                activity_convoy_record_player_hit(hit.hit_pos, hit.hit_object_handle);
            }
            vector hit_pos = hit.hit_pos;
            int hit_bone = -1;
            if (victim_hp != nullptr)
            {
                if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)hp)->get_ai()->get_human_target() != victim)
                {
                    continue;
                }
                human::raycast_hit_info_data& rhi = victim_hp->raycast_hit_info;
                rhi.fire_pos = ray_in.start;
                rhi.target_pos = ray_in.end;
                const object_hit_location location = human_get_hit_location(victim_hp, rhi.fire_pos, ray_dir, hit_pos, radius, &hit_bone, nullptr);
                vector closest;
                ix_closest_point_on_line_segment(closest, hit_pos, ray_in.start, ray_in.end);
                if (vec_dist(closest, hit_pos) <= radius || closest == hit_pos)
                {
                    rhi.hit_point = hit_pos;
                    rhi.hit_location = location;
                    rhi.hit_bone = hit_bone;
                }
                else
                {
                    if (!victim->is_killed())
                    {
                        continue;
                    }
                    rhi.hit_point = rhi.target_pos;
                    rhi.hit_location = HIT_NONE;
                    rhi.hit_bone = -1;
                }
            }
            // The hit direction: the cast direction's component across the line from the attacker's center to the hit.
            vector center;
            hp->get_center_pos(center);
            // Each cross product term is computed in double and rounded once, as in the binary.
            const float rx = hit_pos.x - center.x;
            const float ry = hit_pos.y - center.y;
            const float rz = hit_pos.z - center.z;
            const float cx = (float)((double)ry * (double)ray_dir.z - (double)rz * (double)ray_dir.y);
            const float cy = (float)((double)rz * (double)ray_dir.x - (double)ray_dir.z * (double)rx);
            const float cz = (float)((double)ray_dir.y * (double)rx - (double)ry * (double)ray_dir.x);
            vector hit_direction;
            hit_direction.x = (float)((double)cy * (double)rz - (double)cz * (double)ry);
            hit_direction.y = (float)((double)cz * (double)rx - (double)rz * (double)cx);
            hit_direction.z = (float)((double)ry * (double)cx - (double)cy * (double)rx);
            hit_direction.normalize_safe();
            const vector hit_normal(-hit_direction.x, -hit_direction.y, -hit_direction.z);
            // A hit that cancels the swing plays its effect as the player's.
            const bool was_cancelling = hp->hflags.cancelling_melee_attack;
            bool cancelled = false;
            human_combat_unified_melee_hit(victim, hp, hit_pos, hit_normal, hit_direction, w_info, hp->combat.cur_melee_combat_move, hit.hit_havok_shape_key,
                                           !hp->combat.played_hit_effect, true);
            if (!was_cancelling && hp->hflags.cancelling_melee_attack)
            {
                cancelled = true;
            }
            if (!hp->combat.played_hit_effect && (victim_hp == nullptr || !victim_hp->is_invulnerable()))
            {
                const material_shape_type shape_type = rfg_material_get_shape_type(victim, hit.hit_havok_shape_key);
                const bool player_effect = cancelled || (object_is_player(hp) && !hp->combat.played_hit_effect);
                matrix effect_orient;
                effect_orient.make_quick(hit_direction);
                unsigned char melee_group = w_info->melee_group_index;
                if (hp->hflags.riot_shield)
                {
                    melee_group = Riot_shield_melee_group;
                }
                if (hit_bone != -1)
                {
                    victim_hp->get_tag_world_coords(hit_bone, hit_pos, nullptr, nullptr, nullptr);
                }
                unsigned char effect_material;
                bool skip = false;
                if (victim_hp != nullptr && human_combat_is_blocking_melee(victim_hp))
                {
                    effect_material = Marauder_block_effect_idx;
                }
                else
                {
                    if (!localization_allow_ragdoll_interaction() && victim_hp != nullptr && victim_hp->is_killed())
                    {
                        skip = true;
                    }
                    effect_material = hit.hit_effect_material;
                }
                if (!skip && rfg_effect_candidate_create(effect_material, hit_pos, MATERIAL_EVENT_MELEE, MATERIAL_EVENT_SIZE_SMALL, shape_type, player_effect, 0, &effect_orient,
                                                         victim_hp != nullptr ? nullptr : victim, 0xFFFFFFFF, 0xFF, melee_group))
                {
                    effects_played++;
                    hp->combat.played_hit_effect = true;
                    continue;
                }
            }
            effects_played++;
        }
        if (effects_played > 0 && hp->get_handle() == Rfg_camera.target_handle && hp->combat.cur_melee_combat_move != nullptr)
        {
            camera_shake_play(hp->combat.cur_melee_combat_move->impact_camera_shake, 1.0f, false);
        }
    }
}

// 0x00AD2320 (:1999, LTCG: last_continuous_pos in ESI, pos in EDI, hp on the stack)
// Sweeps one attack point from where it was last frame to where it is now (nothing on the first frame of a swing).
static void process_continuous_attack_point(human* hp, vector& pos, vector& last_continuous_pos)
{
    if (!last_continuous_pos.is_null())
    {
        raycast_input ray;
        ray.start = last_continuous_pos;
        ray.end = pos;
        ray.radius = 0.0f;
        ray.filter_params = nullptr;
        ray.filter_callback = nullptr;
        ray.m_filter_info = COLLIDE_RAY_DEFAULT;
        ray.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        ray.m_flags = 0;
        human_combat_process_hits(hp, ray, false);
    }
    last_continuous_pos = pos;
}

// 0x00A54EF0 (:2017, LTCG: hp in ESI, the rest on the stack)
// Steers the swing: towards the camera (players) or the human's facing, then towards the nearest enemy in reach, and
// on the start of a swing (init) picks the in-air or running variant of the attack.
static void human_turn_towards_melee_target(human* hp, bool init, bool from_server, bool force_running)
{
    if (hp == nullptr || hp->combat.cur_melee_combat_move == nullptr)
    {
        return;
    }
    const human_melee_combat_move_data* move = hp->combat.cur_melee_combat_move;
    player* pp;
    vector facing;
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        pp = (player*)hp;
        if (Multiplayer && pp != Player)
        {
            facing = pp->mp_camera_orient.fvec.copy_flatten_normalize_safe();
        }
        else
        {
            facing = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
        }
    }
    else
    {
        facing = hp->orient.fvec;
        pp = nullptr;
    }
    bool running = false;
    bool turned_local = false;
    human* target = nullptr;
    float target_dist = -1.0f;
    if (pp == nullptr)
    {
        target = human_combat_get_target(hp);
    }
    else
    {
        // The nearest living enemy human.
        const int count = World.get_num_objects<human>();
        for (int i = 0; i < count; i++)
        {
            human* hp2 = World.get_object<human>(i);
            if (hp2->get_handle() == hp->get_handle())
            {
                continue;
            }
            const float dist = vec_dist(hp->pos, hp2->pos);
            if ((dist < target_dist || target_dist == -1.0f) && !human_teams_are_friendly(hp, hp2) && !hp2->is_killed())
            {
                target_dist = vec_dist(hp->pos, hp2->pos);
                target = hp2;
            }
        }
    }
    if (!move->flags.no_orient_correction)
    {
        hp->block_steering_vector = facing;
    }
    if (target != nullptr)
    {
        vector to_target;
        to_target.x = target->pos.x - hp->pos.x;
        to_target.y = target->pos.y - hp->pos.y;
        to_target.z = target->pos.z - hp->pos.z;
        const float angle = vec_delta_ang(facing, to_target, hp->orient.uvec);
        // Original: the capped angle is discarded; the test below uses the raw one.
        fl_angle_cap_radians(angle, -3.14159274f);
        if ((double)target_dist < (double)Camera_melee_turn_distance && (double)fabsf(angle) < (double)Camera_melee_turn_angle)
        {
            if (!move->flags.no_orient_correction)
            {
                vector delta;
                delta.x = target->pos.x - hp->pos.x;
                delta.y = target->pos.y - hp->pos.y;
                delta.z = target->pos.z - hp->pos.z;
                vector steer;
                steer = delta.copy_flatten_normalize_safe();
                hp->block_steering_vector = steer;
                matrix steer_orient;
                steer_orient.make_quick(steer);
                vanim_set_action_starting_orient(hp->char_inst->vp, hp->pos, hp->orient, hp->pos, steer_orient, ANIM_ALIAS_COMBAT_MOVE, min_correction_diff);
                if (pp != nullptr && object_is_local_player(pp))
                {
                    if (init)
                    {
                        // Magic number: 7, the camera transition flags of the melee turn.
                        camera_trans_start_linear_transition(Camera_melee_turn_time, 7, true);
                    }
                    if (Rfg_camera.flags & 2)
                    {
                        Rfg_camera.lookaround_params.heading = vec_delta_ang(FORWARD_VECTOR, steer, UP_VECTOR);
                    }
                }
            }
            if (object_is_local_player(hp))
            {
                if (!init)
                {
                    return;
                }
                turned_local = true;
                running = (double)target_dist > (double)Melee_running_distance;
            }
        }
    }
    if (!init)
    {
        return;
    }
    const vector& move_dir = hp->velocity.is_null() ? hp->orient.fvec : hp->velocity;
    vector dir = move_dir;
    const float turn = vec_delta_ang(dir, facing, UP_VECTOR);
    hp->mp_flags.set_running_attack(0);
    multi_game_player_set_mp_flag(hp, PMPF_RUNNING_ATTACK, 0);
    if (from_server || turned_local)
    {
        const bool run = from_server ? force_running : running;
        multi_game_player_set_mp_flag(hp, PMPF_RUNNING_ATTACK, run);
        hp->mp_flags.set_running_attack(run);
    }
    else if ((pp == nullptr && (int)hp->cur_movement_state >= HMS_WALK) ||
             // Magic numbers: faster than 2.4 m/s within one radian of the facing, and faster than (double)3.4f or
             // carrying with both hands.
             (2.4f < hp->move_speed && (float)fabs((double)turn) < 1.0f && ((double)(float)3.4f < (double)hp->move_speed || hp->char_inst->vp->default_group_index == ANIMATION_GROUP_2H_CARRY)))
    {
        multi_game_player_set_mp_flag(hp, PMPF_RUNNING_ATTACK, 1);
        hp->mp_flags.set_running_attack(1);
    }
    // Magic number: 200 ms in the air makes the attack the air variant.
    if (hp->air_time.time_since() > 200)
    {
        if (anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE))->num_variants != 0)
        {
            hp->block_movement_type = pp != nullptr ? BLOCK_MOVEMENT_ORIENT : BLOCK_MOVEMENT_POS;
        }
        vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, ANIM_VARIANT_AIR, true, false);
        hp->hflags.in_air_melee = 1;
        return;
    }
    if (hp->mp_flags.get_running_attack())
    {
        if (anim_get_anim_from_handle(vanim_get_anim_handle(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE))->num_variants != 0)
        {
            hp->block_movement_type = pp != nullptr ? BLOCK_MOVEMENT_POS_AND_ORIENT : BLOCK_MOVEMENT_POS;
        }
        vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, ANIM_VARIANT_FORWARD, true, false);
    }
}

// 0x00AD23A0 (:2166)
// The frame of a swing: steering, the jump to fall switch, the cancel (melee delay), and between the attack's impact
// triggers the sweep of each attack point; the secondary impact trigger casts once along the facing (or camera).
void human_combat_process_melee(human* hp)
{
    if (hp->combat.cur_melee_combat_move == nullptr)
    {
        hp->hflags.invulnerable_to_debris = 0;
        return;
    }
    if (!hp->hflags.in_air_melee)
    {
        human_turn_towards_melee_target(hp, false, false, false);
    }
    const unsigned int handle = hp->char_inst->get_action_anim_instance_handle(ANIM_ALIAS_COMBAT_MOVE);
    const float mix = animlib_get_current_mix_percent(hp->char_inst->aim_handle, handle);
    // Magic number: a swing ramping in below half mix and frozen (speed 0) releases the movement block.
    if (0.0f < mix && mix < 0.5f && hp->block_movement_anim == ANIM_ALIAS_COMBAT_MOVE && animlib_get_current_speed_multiplier(hp->char_inst->aim_handle, handle) == 0.0f)
    {
        hp->block_movement_anim = -1;
    }
    // An unsupported jump becomes a fall.
    if (!(hp->rdd.is_loaded() ? hp->rdd.get_data() : nullptr)->controller->flags.is_supported && hp->cur_state == HS_JUMP)
    {
        hp->cur_state = HS_FALL;
        // Original: cur_state was just set to HS_FALL, so the cover test is always false.
        if ((hp->get_sub_type() != OT_SUB_HUMAN_NPC || hp->is_killed() || hp->cur_state != HS_COVER) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
            hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            if (hp->movement_mode != HMM_FALL)
            {
                hp->previous_movement_mode = hp->movement_mode;
                hp->movement_mode = HMM_FALL;
            }
            const human_move_modes mode = hp->movement_mode;
            hp->movement_submode = HMSM_FALL_NORMAL;
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
    if ((hp->combat.melee_delay_timer.valid() && !hp->combat.melee_delay_timer.elapsed()) || hp->hflags.cancelling_melee_attack)
    {
        // A cancelled sledgehammer swing ends when its impact reaction (the recoil) has played.
        const weapon* wp = hp->get_equipped_weapon();
        if ((wp != nullptr && wp->w_info->weapon_class != WEAPON_CLASS_SLEDGEHAMMER) || !hp->char_inst->is_playing_action(ANIMATION_ACTION_IMPACT_FROM_FRONT))
        {
            hp->hflags.cancelling_melee_attack = 0;
            hp->hflags.do_continuous_melee_damage = 0;
            // Magic number: 12, the blend out of the cancelled swing.
            hp->char_inst->stop_action(ANIM_ALIAS_COMBAT_MOVE, 12, true);
        }
        return;
    }
    if (handle == 0xFFFFFFFF)
    {
        hp->hflags.invulnerable_to_debris = 0;
        hp->combat.cur_melee_combat_move = nullptr;
        return;
    }
    if (hp->combat.cur_melee_combat_move == nullptr)
    {
        return;
    }
    if (hp->hflags.do_continuous_melee_damage && animlib_trigger_has_elapsed(hp->char_inst->aim_handle, Trigger_impact_start, handle, -1) &&
        !animlib_trigger_has_elapsed(hp->char_inst->aim_handle, Trigger_impact_end, handle, -1))
    {
        hp->adjust_child_transforms(false, false);
        hp->hflags.invulnerable_to_debris = 1;
        if (hp->get_equipped_weapon() == nullptr)
        {
            return;
        }
        vector pos1;
        vector pos2;
        int count = 0;
        if (!get_melee_attack_pos(hp, pos1, pos2, count))
        {
            return;
        }
        process_continuous_attack_point(hp, pos1, hp->combat.last_continuous_pos);
        if (count == 2)
        {
            process_continuous_attack_point(hp, pos2, hp->combat.last_continuous_pos_2);
        }
    }
    if (!animlib_trigger_just_elapsed(hp->char_inst->aim_handle, Trigger_secondary_impact, handle, -1))
    {
        return;
    }
    const human_melee_combat_move_data* move = hp->combat.cur_melee_combat_move;
    if (move == nullptr)
    {
        return;
    }
    if (!move->flags.secondary_impact)
    {
        // Original: the message is formatted into a per-thread text buffer and never shown.
        char message[512];
        sprintf(message, "Melee animation for move %s in animation set %s has secondary impact trigger, but melee table entry lacks secondary impact data.",
                (Melee_in_dlc ? Dlc_melee_combat_moves : Melee_combat_moves)[move->move_id].name, move->anim_set_name);
    }
    raycast_input ray;
    ray.radius = 0.0f;
    ray.filter_params = nullptr;
    ray.filter_callback = nullptr;
    ray.m_filter_info = COLLIDE_RAY_DEFAULT;
    ray.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    ray.m_flags = 0;
    vector dir;
    const bool local_player = object_is_local_player(hp);
    if (local_player && Rfg_camera.mode == CAMERA_FIRST_PERSON_MODE)
    {
        ray.start = Rfg_camera.real_pos;
        dir = Rfg_camera.real_orient.fvec;
    }
    else
    {
        if (hp->combat.cur_melee_combat_move == nullptr)
        {
            return;
        }
        ray.start = hp->pos;
        ray.start.y = (float)((double)hp->combat.cur_melee_combat_move->secondary_cast_height + (double)hp->pos.y);
        dir = hp->orient.fvec;
        if (object_is_player(hp))
        {
            // Pitch the facing towards the camera's (a remote player's camera) by up to Secondary_melee_angle.
            const vector& look = local_player ? Rfg_camera.real_orient.fvec : ((player*)hp)->mp_camera_orient.fvec;
            float angle = vec_delta_ang(hp->orient.fvec, look, hp->orient.rvec);
            const float min_angle = -Secondary_melee_angle;
            if ((float)((double)angle - (double)min_angle) < 0.0f)
            {
                angle = min_angle;
            }
            if ((float)((double)angle - (double)Secondary_melee_angle) >= 0.0f)
            {
                angle = Secondary_melee_angle;
            }
            vector ahead;
            ahead.x = ray.start.x + hp->orient.fvec.x;
            ahead.y = hp->orient.fvec.y + ray.start.y;
            ahead.z = hp->orient.fvec.z + ray.start.z;
            vector rotated;
            vec_rot_point_around_line(rotated, ahead, angle, ray.start, hp->orient.rvec);
            dir.x = rotated.x - ray.start.x;
            dir.y = rotated.y - ray.start.y;
            dir.z = rotated.z - ray.start.z;
        }
    }
    const float range = move->secondary_cast_range;
    ray.end.x = dir.x * range + ray.start.x;
    ray.end.y = dir.y * range + ray.start.y;
    ray.end.z = dir.z * range + ray.start.z;
    human_combat_process_hits(hp, ray, true);
}

// 0x00A09AD0 (:3235)
void human_combat_reset_melee_delay(human* hp)
{
    hp->combat.melee_delay_timer.invalidate();
}

// 0x00A9CF80 (:3243)
// Starts the weapon's move for the attack type (the gutter's two-step combos remap the secondary and tertiary).
bool human_combat_attack_melee_armed(human* hp, human_melee_attack_types attack_type, bool from_server, bool force_running)
{
    weapon* wp = nullptr;
    if (hp != nullptr)
    {
        wp = hp->get_equipped_weapon();
        if (wp == nullptr)
        {
            return false;
        }
    }
    // Original: a null hp is dereferenced here.
    if ((hp->combat.cur_melee_combat_move != nullptr && !wp->melee_combat_in_combo) || human_weapons_get_weapon_info(hp) == nullptr)
    {
        return false;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    human_melee_combat_move_id move_id = human_weapons_melee_get_combat_move(hp, w_info, attack_type);
    if (w_info->weapon_anim_group == ANIMATION_GROUP_GUTTER)
    {
        // Magic numbers: 0xD and 0x1B, the gutter's combo moves, followed by the weapon's secondary or tertiary
        // (the tertiary's id minus one).
        if (move_id == 0xD)
        {
            move_id = hp->cur_stance == HST_CROUCH ? w_info->crouching_secondary_melee_attack : w_info->standing_secondary_melee_attack;
        }
        else if (move_id == 0x1B)
        {
            move_id = (human_melee_combat_move_id)((hp->cur_stance == HST_CROUCH ? w_info->crouching_tertiary_melee_attack : w_info->standing_tertiary_melee_attack) - 1);
        }
    }
    if (move_id == INVALID_MELEE_COMBAT_MOVE_ID)
    {
        return false;
    }
    const human_melee_combat_move_data* move = human_combat_move_data_from_index(hp, move_id);
    if (move == nullptr)
    {
        return false;
    }
    wp->melee_combat_in_combo = false;
    return human_combat_play_combat_move(hp, move, from_server, force_running);
}

// 0x00A09AE0 (:3291)
bool human_combat_playing_combat_move(const human* hp)
{
    return hp->combat.cur_melee_combat_move != nullptr;
}

// 0x00A7ECF0 (:3299)
// Plays the move's attack (the cover attack in cover) on the human and its weapon and starts the swing state; a
// human in the air without ground below swings the air variant.
bool human_combat_play_combat_move(human* hp, const human_melee_combat_move_data* move, bool from_server, bool force_running)
{
    if (hp->block_movement_anim != -1 && hp->released_movement_anim != -1)
    {
        return false;
    }
    // human::get_equipped_weapon, inlined.
    object* weapon_obj = hp->get_equipped_weapon();
    const weapon_info* cover_info = nullptr;
    // Original: weapon_obj is dereferenced unchecked when the player in cover holds a melee weapon.
    const bool cover_attack = hp == Player && hp->cur_state == HS_COVER && (cover_info = human_weapons_get_weapon_info(hp)) != nullptr &&
                              cover_info->ammo_type == WEAPON_AMMO_TYPE_MELEE && !((weapon*)weapon_obj)->w_info->flags.disable_player_cover;
    animation_action cover_anim = (animation_action)-1;
    if (cover_attack)
    {
        cover_anim = human_combat_get_cover_melee_anim(hp);
        if (cover_anim == (animation_action)-1)
        {
            return false;
        }
    }
    if (move == nullptr)
    {
        return false;
    }
    const animation_action attack = cover_anim != (animation_action)-1 ? cover_anim : move->attack_animation;
    // Magic number: 0x20, the action flag of the attack.
    hp->char_inst->play_action(attack, ANIM_ALIAS_COMBAT_MOVE, 1.0f, 0x20);
    hp->block_movement_anim = ANIM_ALIAS_COMBAT_MOVE;
    weapon* wp = hp->get_equipped_weapon();
    if (wp != nullptr && wp->char_inst != nullptr)
    {
        wp->char_inst->play_action(attack, ANIM_ALIAS_COMBAT_MOVE, 1.0f, 0);
    }
    bool melee_weapon;
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp->equipped_inv_item != nullptr && hp->equipped_inv_item->info != nullptr &&
        hp->equipped_inv_item->info->w_info != nullptr && hp->equipped_inv_item->info->w_info->flags.non_inventory)
    {
        melee_weapon = true;
    }
    else
    {
        melee_weapon = wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE;
    }
    if (melee_weapon)
    {
        // A melee weapon leaves the vehicle interaction and the combat ready pose.
        human_in_vehicle_interaction(hp, nullptr);
        hp->combat.combat_ready_timestamp.invalidate();
    }
    hp->combat.cur_melee_combat_move = move;
    hp->combat.last_continuous_pos.zero();
    hp->combat.last_continuous_pos_2.zero();
    hp->hflags.do_continuous_melee_damage = 1;
    hp->combat.played_hit_effect = false;
    hp->combat.hit_object_count = 0;
    if (cover_anim == (animation_action)-1 && !melee_weapon)
    {
        // Magic number: 5 s more in the combat ready pose after a firearm's melee.
        human_set_combat_ready(hp, true, 5000);
    }
    if (Show_player_melee_combat_move_names && hp == Player)
    {
        hud_message_add((Melee_in_dlc ? Dlc_melee_combat_moves : Melee_combat_moves)[move->move_id].name, 4.0f, nullptr, nullptr, nullptr);
    }
    if (cover_anim != (animation_action)-1)
    {
        return true;
    }
    if (human_is_in_air(hp))
    {
        // In the air: the air variant unless there is ground within 0.6 m below while not rising.
        bool air = false;
        raycast_input ray;
        ray.radius = 0.0f;
        ray.start = hp->pos;
        // Magic numbers: from 0.2 m above the feet down 0.6 m.
        ray.start.y = hp->pos.y + 0.2f;
        ray.end.x = hp->pos.x - hp->orient.uvec.x * 0.6f;
        ray.end.y = (hp->pos.y + 0.2f) - hp->orient.uvec.y * 0.6f;
        ray.end.z = hp->pos.z - hp->orient.uvec.z * 0.6f;
        ray.filter_params = nullptr;
        ray.filter_callback = nullptr;
        ray.m_flags = 0;
        ray.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        ray.m_filter_info = COLLIDE_RAY_DEFAULT;
        raycast_output out;
        // Magic number: rising faster than 1 m/s counts as in the air.
        if (1.0f < hp->actual_velocity.y || !havok_raycast(ray, out))
        {
            air = true;
        }
        if (air)
        {
            vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, ANIM_VARIANT_AIR, true, false);
            hp->hflags.in_air_melee = 1;
            hp->block_movement_type = BLOCK_MOVEMENT_ORIENT;
            player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
            vector steer;
            if (!Multiplayer || pp == nullptr)
            {
                steer = Rfg_camera.real_orient.fvec.copy_flatten_normalize();
            }
            else
            {
                steer = pp->mp_camera_orient.fvec.copy_flatten_normalize();
            }
            hp->block_steering_vector = steer;
            hp->block_steering_heading_offset = 0.0f;
            hp->block_movement_anim = ANIM_ALIAS_COMBAT_MOVE;
            return true;
        }
    }
    human_turn_towards_melee_target(hp, true, from_server, force_running);
    return true;
}

// 0x00A09AF0 (:3422): the fire action of the current cover pose, which the melee attack plays in cover.
animation_action human_combat_get_cover_melee_anim(const human* hp)
{
    if (!hp->hflags.crouch_cover)
    {
        switch (hp->movement_submode)
        {
        case HMSM_COVER_RIGHT_LEAN:
            return hp->cur_stance != HST_CROUCH ? ANIMATION_ACTION_COVER_STAND_RIGHT_LEAN_FIRE : ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEAN_FIRE;
        case HMSM_COVER_LEFT_LEAN:
            return hp->cur_stance != HST_CROUCH ? ANIMATION_ACTION_COVER_STAND_LEFT_LEAN_FIRE : ANIMATION_ACTION_COVER_CROUCH_LEFT_LEAN_FIRE;
        case HMSM_COVER_RIGHT_LEANUP:
            return ANIMATION_ACTION_COVER_STAND_RIGHT_LEAN_FIRE;
        case HMSM_COVER_LEFT_LEANUP:
            return ANIMATION_ACTION_COVER_STAND_LEFT_LEAN_FIRE;
        case HMSM_COVER_RIGHT_BLINDFIRE:
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            return ANIMATION_ACTION_COVER_STAND_RIGHT_BLIND_FIRE;
        case HMSM_COVER_LEFT_BLINDFIRE:
        case HMSM_COVER_LEFT_BLINDFIRE2:
            return ANIMATION_ACTION_COVER_STAND_LEFT_BLIND_FIRE;
        default:
            break;
        }
    }
    else
    {
        switch (hp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_RIGHT_LEANUP:
            return ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEANUP_FIRE;
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_LEAN:
            return ANIMATION_ACTION_COVER_CROUCH_RIGHT_LEAN_FIRE;
        case HMSM_COVER_LEFT:
        case HMSM_COVER_LEFT_LEANUP:
            return ANIMATION_ACTION_COVER_CROUCH_LEFT_LEANUP_FIRE;
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_LEAN:
            return ANIMATION_ACTION_COVER_CROUCH_LEFT_LEAN_FIRE;
        case HMSM_COVER_RIGHT_BLINDFIRE:
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            return ANIMATION_ACTION_COVER_CROUCH_BLIND_RIGHT_FIRE;
        case HMSM_COVER_LEFT_BLINDFIRE:
        case HMSM_COVER_LEFT_BLINDFIRE2:
            return ANIMATION_ACTION_COVER_CROUCH_BLIND_LEFT_FIRE;
        default:
            break;
        }
    }
    return ANIMATION_ACTION_NONE;
}

// 0x00A2F8F0 (:3563)
bool human_is_player_in_stealth(const human* hp)
{
    return Player == hp && Player->backpack != nullptr && Player->backpack->is_stealth_active();
}

// 0x00A2F920 (:3577): the combat target, except a stealthed player for EDF and marauders in the main game.
human* human_combat_get_target(const human* hp)
{
    human* target = nullptr;
    const unsigned int handle = hp->combat.target_handle;
    if (handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            target = (human*)objp;
        }
    }
    if (!Multiplayer && !wrecking_crew_is_active() && dlc_get_current_id() == -1 && target != nullptr && human_is_player_in_stealth(target) &&
        (hp->current_team == HUMAN_TEAM_EDF || hp->current_team == HUMAN_TEAM_MARAUDER))
    {
        return nullptr;
    }
    return target;
}

// 0x00A09C80 (:3594)
bool human_combat_is_blind_firing(const human* hp)
{
    const human_move_submodes submode = hp->movement_submode;
    return submode == HMSM_COVER_RIGHT_BLINDFIRE || submode == HMSM_COVER_LEFT_BLINDFIRE || submode == HMSM_COVER_RIGHT_BLINDFIRE2 ||
           submode == HMSM_COVER_LEFT_BLINDFIRE2;
}

// 0x00A2F9A0 (:3602): the HUD hit indicator of the player, from the hit position or the impulse.
void human_combat_process_hit_indicator(human* target, const vector* pos, const vector* impulse, float damage)
{
    if (target != nullptr && target == Player && target->is_killed())
    {
        return;
    }
    vector dir;
    if (pos == nullptr)
    {
        if (impulse == nullptr)
        {
            return;
        }
    }
    else
    {
        // Original: target is dereferenced unchecked here.
        dir.x = target->pos.x - pos->x;
        dir.y = target->pos.y - pos->y;
        dir.z = target->pos.z - pos->z;
        impulse = &dir;
    }
    // Magic number: 50 damage is a full indicator.
    hud_register_hit(*impulse, (float)((double)damage / 50.0));
}

// 0x00A2FA60 (:3642): whether a hit object is the human itself, its vehicle, its shield or the turret it operates.
bool human_combat_hit_human(const object* hit_objp, const human* hp)
{
    if (hit_objp == nullptr)
    {
        return false;
    }
    if (hp == hit_objp || hp->vehicle_handle == hit_objp->get_handle() || hp->shield_handle == hit_objp->get_handle())
    {
        return true;
    }
    const turret* tp;
    if (hit_objp->get_obj_type() == OT_ITEM)
    {
        tp = turret_from_item((item*)hit_objp);
    }
    else if (hit_objp->get_obj_type() == OT_TURRET)
    {
        tp = (const turret*)hit_objp;
    }
    else
    {
        return false;
    }
    return tp != nullptr && (tp->current_operator == hp->get_handle() || (tp->get_parent() == hp->vehicle_handle && hp->vehicle_handle != 0xFFFFFFFF));
}

// 0x00A09D10 (:3812)
bool human_combat_is_blocking_melee(const human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    return hp->char_inst->is_playing_action(ANIMATION_ACTION_BLOCK_FLINCH_HIGH) || hp->char_inst->is_playing_action(ANIMATION_ACTION_BLOCK_FLINCH_LOW) ||
           hp->char_inst->get_animation_state() == ANIMATION_STATE_WALK_BLOCK;
}

// ---- damage to humans (multiplayer.md MP8): bullet, melee and explosion hits, gibs, the nano rifle fade
static bool Allow_flinches = true; // 0x01694364
static bool Player_flinch; // 0x0302F05B, the player never flinches from hits
static bool Allow_damage_knockdown; // 0x0302F059, melee knockdowns of humans without hits left (off in retail)
static float Damage_multiplier_friendly_fire = 0.5f; // 0x0169432C
// float Damage_multiplier_player_damage_friendly (0x01694330) is used here too; placed in human_collide.cpp, earlier in the unity TU.
static float Vehicle_armored_passenger_explosion_damage_multiplier = 0.2f; // 0x01694374
static float Vehicle_passenger_explosion_damage_multiplier = 0.6f; // 0x01694378
static float Explosion_gibs_dist = 0.35f; // 0x0169437C, how far the explosion may be from the corpse's bbox
static bool Explosion_gibs_debug; // a debug toggle (the label it formats is never shown)
// These get their values from tweak_table.xtbl only.
static float Shielded_explosion_radius_modifier; // 0x0301AE70
static float Turret_gunner_explosion_damage_modifier; // 0x0301A234
static float Armored_explosion_damage_modifier; // 0x0301AFC8
static float Shielded_explosion_damage_modifier; // 0x03023864
static float Shielded_explosion_crouching_damage_modifier; // 0x030266BC
static float Shielded_explosion_crouching_radius_modifier; // 0x030269CC
float Super_debris_human_impulse = 2.0f; // 0x01694344, the super debris cheat's impulse scale on humans
// Placed here: vehicle_interaction.cpp is not compiled yet.
float Vi_impulse_ragdoll = 200.0f; // 0x016BA524, impulse above which a human in a vehicle interaction ragdolls

static tweak_table_item Tweak_table_item_Damage_multiplier_friendly_fire("Damage_multiplier_friendly_fire", &Damage_multiplier_friendly_fire); // 0x03065A30
static tweak_table_item Tweak_table_item_Damage_multiplier_player_damage_friendly("Damage_multiplier_player_damage_friendly", &Damage_multiplier_player_damage_friendly); // 0x033D616C
static tweak_table_item Tweak_table_item_Shielded_explosion_damage_modifier("Shielded_explosion_damage_modifier", &Shielded_explosion_damage_modifier); // 0x030FCE38
static tweak_table_item Tweak_table_item_Shielded_explosion_radius_modifier("Shielded_explosion_radius_modifier", &Shielded_explosion_radius_modifier); // 0x0303CAA0
static tweak_table_item Tweak_table_item_Shielded_explosion_crouching_damage_modifier("Shielded_explosion_crouching_damage_modifier", &Shielded_explosion_crouching_damage_modifier); // 0x03169660
static tweak_table_item Tweak_table_item_Shielded_explosion_crouching_radius_modifier("Shielded_explosion_crouching_radius_modifier", &Shielded_explosion_crouching_radius_modifier); // 0x0305F830
static tweak_table_item Tweak_table_item_Turret_gunner_explosion_damage_modifier("Turret_gunner_explosion_damage_modifier", &Turret_gunner_explosion_damage_modifier); // 0x033D6160
static tweak_table_item Tweak_table_item_Armored_explosion_damage_modifier("Armored_explosion_damage_modifier", &Armored_explosion_damage_modifier); // 0x033B4E50
static tweak_table_item Tweak_table_item_Vehicle_armored_passenger_explosion_damage_multiplier("Vehicle_armored_passenger_explosion_damage_multiplier", &Vehicle_armored_passenger_explosion_damage_multiplier); // 0x030FCE00
static tweak_table_item Tweak_table_item_Vehicle_passenger_explosion_damage_multiplier("Vehicle_passenger_explosion_damage_multiplier", &Vehicle_passenger_explosion_damage_multiplier); // 0x030F7E6C
// Placed with its variable: rfg.exe's item is vehicle_interaction.cpp's (not compiled yet).
static tweak_table_item Tweak_table_item_Vi_impulse_ragdoll("Vi_impulse_ragdoll", &Vi_impulse_ragdoll); // 0x0386BF38

// 0x00A05920 (global_event_data.h:242)
player_damaged_by_explosion_event::player_damaged_by_explosion_event()
    : event_data(EVENT_PLAYER_DAMAGED_BY_EXPLOSION)
{
}

// 0x00AD1060 (:870): the damage of a hit and the flinch it causes. No damage from a human flagged no_damage, nor
// between npcs while No_npc_npc_damage is set; an npc that is hit loses its aim (the drift's acquire time restarts).
// Whether the hit counts: damage was dealt and (in multiplayer without friendly fire) the two are not allies.
static bool process_take_hit_common(human* target, human* attacker, const weapon_info* w_info, float damage, const object* owner, damaged_by_types damage_type, bool silent)
{
    const player* local_player = Player;
    bool reset_drift = true;
    if (attacker != nullptr)
    {
        if (target == nullptr || attacker->hflags.no_damage)
        {
            damage = 0.0f;
        }
        else if (attacker->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            if (target->get_sub_type() != OT_SUB_HUMAN_NPC)
            {
                reset_drift = false;
            }
            else if (No_npc_npc_damage)
            {
                damage = 0.0f;
            }
        }
    }
    // Original: target is dereferenced unchecked here when attacker is null.
    if (reset_drift && target->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        const weapon_info* target_w_info = human_weapons_get_weapon_info(target);
        if (target_w_info != nullptr)
        {
            ((npc*)target)->get_drift_info().acquire_time.set(target_w_info->npc_aim_drift_profile->acquire_time);
        }
    }
    if (damage > 0.0f)
    {
        const float dealt = human_damage(target, attacker, damage, damage_type, w_info, owner, true, silent);
        if (Allow_flinches && (dealt > 0.0f || target->h_info->flags.guerrilla_tech || target->h_info->flags.guerrilla_rfc || target->h_info->flags.old_coot) &&
            (Player_flinch || target != local_player) && !target->is_killed() && target->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE && target->knockdown_hits > 0.0f &&
            (object_is_player(target) || damage_type != DBT_EXPLOSION))
        {
            vector hit_dir = target->raycast_hit_info.hit_point - target->raycast_hit_info.fire_pos;
            if (hit_dir.is_null())
            {
                if (attacker == nullptr)
                {
                    hit_dir.x = -target->orient.fvec.x;
                    hit_dir.y = -target->orient.fvec.y;
                    hit_dir.z = -target->orient.fvec.z;
                }
                else
                {
                    hit_dir = (target->pos - attacker->pos).copy_normalize_safe();
                }
            }
            else
            {
                hit_dir.normalize();
            }
            // Magic number: the large flinch share in percent (x87 double product, truncated).
            const unsigned int large_flinch_percent = (unsigned int)(long long)((double)fl_parameter(damage, Min_large_flinch_damage, Max_large_flinch_damage, true) * 100.0);
            const object_hit_location hit_location = damage_type == DBT_EXPLOSION ? HIT_GENERAL : target->raycast_hit_info.hit_location;
            human_combat_play_flinch(target, target->raycast_hit_info.hit_point, hit_dir, hit_location, large_flinch_percent);
        }
    }
    if (Multiplayer == true && attacker != nullptr && target != attacker && human_teams_are_allies(target, attacker) && !Mp_server_options.generic_options.friendly_fire)
    {
        return false;
    }
    return damage > 0.0f;
}

// 0x00A54E00 (:1383): the x-ray kill check. A penetrating test bullet along the same line: something that is not a
// human comes first.
bool bullet_probably_passed_through_a_wall(const weapon_fire_info& fire_info, const player* killer)
{
    weapon_fire_info test_info;
    test_info.init(fire_info.start_pos, fire_info.end_pos, fire_info.w_info);
    sorted_list<weapon_fire_info, 50, float> hits(true);
    weapons_test_penetrating_bullet(test_info, hits, killer, true);
    if (hits.get_first() != nullptr)
    {
        const object* first = World.object_from_handle(hits.get_first()->hit_handle);
        if (first == nullptr || first->get_obj_type() != OT_HUMAN)
        {
            return true;
        }
    }
    return false;
}

// 0x00AD37B0 (:2667)
void human::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    // Original: attacker is dereferenced unchecked in the npc check, the ragdoll start and the nano fade.
    if (get_sub_type() == OT_SUB_HUMAN_NPC && attacker->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        damage = (is_offscreen() || attacker->is_offscreen() ? Damage_multiplier_npc_offscreen : Damage_multiplier_npc_onscreen) * damage;
        if (get_obj_type() == OT_HUMAN && get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)this)->npc_flags.reduced_npc_damage)
        {
            damage = Damage_multiplier_npc_reduced * damage;
        }
    }
    if (!Multiplayer && !wrecking_crew_is_active() && human_teams_are_friendly(this, attacker))
    {
        damage = (attacker != nullptr && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? Damage_multiplier_player_damage_friendly : Damage_multiplier_friendly_fire) * damage;
    }
    if (get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
        if (human_in_vehicle(this, vp) && vp != nullptr)
        {
            return;
        }
        // In cover the hit is reduced by how much it comes from the cover side.
        if (cur_state == HS_COVER && !human_is_leaning_from_cover(this))
        {
            const vector dir = (fire_info.hit_pos - fire_info.start_pos).copy_flatten_normalize_safe();
            float facing = ((player*)this)->cover_vector * dir;
            if (facing - 0.0f < 0.0f)
            {
                facing = 0.0f;
            }
            damage = (1.0f - facing) * damage;
        }
    }
    const bool was_killed = is_killed();
    const vector shot_dir = (fire_info.end_pos - fire_info.start_pos).copy_normalize();
    const object_hit_location hit_location = fire_info.hit_location;
    bool headshot_kill = false;
    float multiplier = 0.0f;
    bool scale = false;
    if (!Multiplayer)
    {
        if (hit_location == HIT_HEAD && get_sub_type() != OT_SUB_HUMAN_PLAYER)
        {
            if (fire_info.w_info != nullptr)
            {
                damage = fire_info.w_info->headshot_multiplier * damage;
            }
            if (hit_points <= damage)
            {
                headshot_kill = true;
            }
        }
        else if (hit_location == HIT_LEFT_LEG || hit_location == HIT_RIGHT_LEG)
        {
            // Original: the multiplayer leg multiplier applies to single player legs too (shared code path).
            multiplier = Mp_leg_damage_multiplier;
            scale = true;
        }
    }
    else if (hit_location == HIT_HEAD)
    {
        if (fire_info.w_info != nullptr)
        {
            multiplier = fire_info.w_info->headshot_multiplier;
            scale = true;
        }
    }
    else if (hit_location == HIT_LEFT_LEG || hit_location == HIT_RIGHT_LEG)
    {
        multiplier = Mp_leg_damage_multiplier;
        scale = true;
    }
    if (scale)
    {
        damage = multiplier * damage;
    }
    // Magic number: civilians shot by a terrorizing npc die at once.
    if (attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        // Original: compares with &terrorize_action::the_instance, which is private here; get_instance answers the same
        // address (and an empty plan never matches).
        const ai_action* act = ((npc*)attacker)->get_ai()->planner.get_current_action();
        if (act != nullptr && act == terrorize_action::get_instance() && current_team == HUMAN_TEAM_CIVILIAN)
        {
            damage = 100000.0f;
        }
    }
    if (process_take_hit_common(this, attacker, fire_info.w_info, damage, nullptr, DBT_BULLET, headshot_kill))
    {
        if (get_handle() == Rfg_camera.target_handle && attacker != nullptr)
        {
            const vector* attacker_pos = &attacker->pos;
            if (((player*)this != Player || !is_killed()) && attacker_pos != nullptr)
            {
                // Magic number: 50 damage is a full indicator.
                hud_register_hit(pos - *attacker_pos, damage / 50.0f);
            }
            if (!is_invulnerable())
            {
                vector from_attacker = pos - *attacker_pos;
                from_attacker.normalize();
                camera_shake* shake = fire_info.w_info->player_hit_camera_shake;
                if (shake == nullptr)
                {
                    camera_shake_play(from_attacker * orient.rvec <= 0.0f ? "npc_ar_right" : "npc_ar_left", 1.0f, false);
                }
                else
                {
                    camera_shake_play(shake, 1.0f, false);
                }
            }
        }
        if (hit_points == 0.0f)
        {
            const bool killed_now = !was_killed && is_killed();
            const vector splatter_dir = (fire_info.end_pos - fire_info.hit_pos).copy_normalize_safe();
            // Original: the blood splatter of a bullet hit is made as a melee one.
            object_decal_create_blood_splatter_decal(this, attacker, fire_info.hit_pos, splatter_dir, killed_now, DBT_MELEE);
        }
    }
    if (attacker != nullptr)
    {
        // Magic number: 900 ms of not being undercover after attacking.
        attacker->combat.not_undercover_timestamp.set(900);
    }
    if (ragdoll_state == HUMAN_RAGDOLL_STATE_FLINCH)
    {
        vector dir = fire_info.hit_pos - fire_info.start_pos;
        dir.normalize_safe();
        // Magic numbers: a 3 m ray and 4 times the shot's ragdoll force on the flinching ragdoll.
        havok_ragdoll_cast_ray(ragdoll_idx, fire_info.hit_pos, dir * 3.0f + fire_info.hit_pos, fire_info.w_info->ragdoll_force_shoot * 4.0f);
    }
    bool killed = false;
    if (is_killed())
    {
        killed = true;
        if (was_killed)
        {
            // Shooting a corpse pushes its ragdoll.
            if (ragdoll_idx != -1)
            {
                if (!localization_allow_ragdolls() && ragdoll_state == HUMAN_RAGDOLL_STATE_POSE_SAVED)
                {
                    return;
                }
                // Magic number: an on impact ragdoll shot after its first 4 frames counts as shot.
                if (ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
                {
                    const int frame = vanim_get_anim_frame(char_inst->vp, ANIM_ALIAS_DEATH);
                    if (frame < 0 || frame > 4)
                    {
                        hflags.ragdoll_shot = 1;
                    }
                }
                if (localization_allow_ragdoll_interaction())
                {
                    havok_ragdoll_apply_point_impulse(ragdoll_idx, shot_dir * fire_info.w_info->ragdoll_force_shoot, fire_info.hit_pos);
                }
            }
        }
        else
        {
            bool forced_by_server = false;
            // Magic number: ragdoll_chance is in percent.
            if (fl_rand_range(0.0f, 100.0f) < fire_info.w_info->ragdoll_chance || Multiplayer)
            {
                forced_by_server = Multiplayer == true && multi_is_server();
            }
            else if (vehicle_handle == 0xFFFFFFFF && !human_combat_is_manning_turret(this) && !object_is_player(this) &&
                     (ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) && cur_state != HS_FALL && ground_obj_handle != 0xFFFFFFFF)
            {
                // A death animation by where the hit was (crouched ones 12 actions later), blending into an on impact
                // ragdoll.
                vector dir = fire_info.hit_pos - fire_info.start_pos;
                dir.normalize_safe();
                const flinch_directions flinch_dir = human_combat_get_flinch_direction(this, dir);
                int action;
                switch (fire_info.hit_location)
                {
                case HIT_GENERAL:
                case HIT_TORSO:
                case HIT_LEFT_ARM:
                case HIT_RIGHT_ARM:
                    action = 0x241; // the torso death actions
                    break;
                case HIT_HEAD:
                    action = 0x245; // the head death actions
                    break;
                default:
                    action = 0x23D; // the leg death actions
                    break;
                }
                const int crouch_offset = cur_stance == HST_CROUCH ? 0xC : 0;
                // Magic numbers: animlib flag 0x40; an on impact ragdoll for 10 s from 0.1 m/s.
                vanim_play_action(char_inst->vp, index_alias(crouch_offset + action + flinch_dir, ANIM_ALIAS_DEATH), 0x40, 1.0f, 1.0f);
                human_ragdoll_on_impact(this, 10.0f, 0.1f, false);
                goto nano_fade;
            }
            else
            {
                forced_by_server = Multiplayer == true && multi_is_server();
            }
            if (human_ragdoll_start(this, forced_by_server, attacker->get_handle()))
            {
                havok_ragdoll_apply_point_impulse(ragdoll_idx, shot_dir * fire_info.w_info->ragdoll_force_shoot, fire_info.hit_pos);
                if (Multiplayer == true)
                {
                    if (multi_is_server() && object_is_player(this))
                    {
                        multi_game_player_ragdoll_point_impulse(World.object_cast<player>(this), fire_info.hit_pos - pos, shot_dir * fire_info.w_info->ragdoll_force_shoot);
                    }
                    if (Multiplayer == true && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER && attacker != this && human_teams_are_enemies(attacker, this))
                    {
                        if (fire_info.hit_location == HIT_HEAD)
                        {
                            multi_game_xp_trigger_event(((player*)attacker)->mp_data, MXE_HEADSHOT_KILL, 1, 0xFF, -1, false);
                        }
                        if (fire_info.w_info != nullptr && fire_info.w_info->flags.penetrating_bullets && bullet_probably_passed_through_a_wall(fire_info, (player*)attacker))
                        {
                            multi_game_xp_trigger_event(((player*)attacker)->mp_data, MXE_X_RAY_KILL, 1, 0xFF, -1, false);
                        }
                    }
                }
            }
        }
    }
nano_fade:
    if (fire_info.w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
    {
        start_nano_rifle_fade_out(killed, attacker, false, fire_info.hit_pos);
    }
}

// 0x00AD2A10 (:2339)
bool human::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    // The ragdoll impulses push at the hit's height on the human's axis.
    const vector push_pos(pos.x, hit_pos.y, pos.z);
    if (attacker == nullptr)
    {
        return false;
    }
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        npc* np = get_obj_type() == OT_HUMAN && get_sub_type() == OT_SUB_HUMAN_NPC ? (npc*)this : nullptr;
        if (human_combat_maybe_block_melee(this, attacker))
        {
            return false;
        }
        if (attacker->get_sub_type() == OT_SUB_HUMAN_NPC && get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            damage = (is_offscreen() || ((human*)attacker)->is_offscreen() ? Damage_multiplier_npc_offscreen : Damage_multiplier_npc_onscreen) * damage;
            if (np != nullptr && np->npc_flags.reduced_npc_damage)
            {
                damage = Damage_multiplier_npc_reduced * damage;
            }
        }
        if (h_info->flags.body_armor)
        {
            damage = Damage_multiplier_melee_heavy_armor * damage;
        }
        // Original: attacker is treated as a human without a type check (here and below).
        if (human_teams_are_friendly(this, (human*)attacker))
        {
            damage = (object_is_player(attacker) ? Damage_multiplier_player_damage_friendly : Damage_multiplier_friendly_fire) * damage;
        }
    }
    const bool was_killed = is_killed();
    bool backstab = false;
    const bool target_is_player = get_sub_type() == OT_SUB_HUMAN_PLAYER;
    if (attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER && !was_killed && !target_is_player)
    {
        // Magic numbers: a hit from behind (within 60 degrees of the back) does 4 times the damage, silently.
        if (orient.fvec * (pos - attacker->pos).copy_normalize_safe() >= 0.5f)
        {
            damage = damage * 4.0f;
            backstab = true;
        }
    }
    if (!human_teams_are_friendly((human*)attacker, current_team))
    {
        // Magic number: 900 ms of not being undercover after attacking.
        ((human*)attacker)->combat.not_undercover_timestamp.set(900);
    }
    // One damaging melee hit a second; hits in between only push the ragdoll.
    if (combat.melee_damage_timer.valid() && !combat.melee_damage_timer.elapsed())
    {
        if (!localization_allow_ragdoll_interaction() && is_killed())
        {
            return true;
        }
        // Magic number: 1% of the impulse.
        havok_ragdoll_apply_point_impulse(ragdoll_idx, impulse * 0.01f, push_pos);
        return true;
    }
    combat.melee_damage_timer.set(1000);
    if (!process_take_hit_common(this, (human*)attacker, &w_info, damage, nullptr, DBT_MELEE, backstab))
    {
        return true;
    }
    if (get_handle() == Rfg_camera.target_handle)
    {
        vector from_attacker = pos - attacker->pos;
        from_attacker.normalize();
        camera_shake_play(from_attacker * Rfg_camera.real_orient.rvec <= 0.0f ? "npc_melee_right" : "npc_melee_left", 1.0f, false);
    }
    // Original: also tests &attacker->pos against null.
    if (Player != nullptr && get_handle() == Player->get_handle() && ((player*)this != Player || !is_killed()))
    {
        // Magic number: 50 damage is a full indicator.
        hud_register_hit(pos - attacker->pos, damage / 50.0f);
    }
    if (!is_invulnerable() && localization_allow_blood())
    {
        rl_decal_parameters decal;
        decal_info_get(&decal, Human_melee_blood_decal_id);
        decal.m_pos = hit_pos;
        decal.m_transform.make_quick(hit_normal);
        const unsigned int decal_handle = object_decal_apply(this, decal, 0.0f, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
        object_decal_player_track_blood_decal(this, decal_handle);
        // The splatter goes down along the push.
        vector splatter_dir = impulse.copy_flatten_normalize_safe();
        splatter_dir.y = -1.0f;
        splatter_dir.normalize_safe();
        object_decal_create_blood_splatter_decal(this, (human*)attacker, hit_pos, splatter_dir, !was_killed && human_is_dead(this), DBT_MELEE);
    }
    if ((ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) && !is_killed())
    {
        // An enemy's hit on a player always knocks down; others need their knockdown hits used up.
        if (target_is_player && human_teams_are_enemies((human*)attacker, this))
        {
            knockdown_hits = 0.0f;
        }
        if (knockdown_hits > 0.0f)
        {
            return true;
        }
        if (target_is_player && is_invulnerable())
        {
            return true;
        }
        if (Allow_damage_knockdown)
        {
            human_ragdoll_start(this, false, attacker->get_handle());
            havok_ragdoll_apply_point_impulse(ragdoll_idx, impulse, push_pos);
        }
        knockdown_hits = (float)max_knockdown_hits;
        return true;
    }
    // Magic number: 7% of the impulse pushes a ragdoll.
    if (is_killed() && !was_killed)
    {
        human_ragdoll_start(this, Multiplayer == true && multi_is_server(), 0xFFFFFFFF);
        havok_ragdoll_apply_point_impulse(ragdoll_idx, impulse * 0.07f, push_pos);
        if (Multiplayer == true && multi_is_server() && object_is_player(this))
        {
            multi_game_player_ragdoll_point_impulse(World.object_cast<player>(this), push_pos - pos, impulse * 0.07f);
        }
        return true;
    }
    if (ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        if (!was_killed)
        {
            return true;
        }
        human_ragdoll_start(this, false, 0xFFFFFFFF);
    }
    else if (ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT && (get_sub_type() == OT_SUB_HUMAN_NPC || human_is_dead(this)))
    {
        human_ragdoll_start(this, false, attacker->get_handle());
    }
    if (ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || !localization_allow_ragdoll_interaction())
    {
        return true;
    }
    havok_ragdoll_apply_point_impulse(ragdoll_idx, impulse * 0.07f, push_pos);
    return true;
}

// 0x00A9CEE0 (:2570): the end of a nano fade on a human's mesh. A human killed by the nano rifle is hidden for good.
static void material_effect_completion_callback(void* cb_data, rl_renderable_instance* ri)
{
    nano_callback_info* info = (nano_callback_info*)cb_data;
    object* objp = info->target_handle != 0xFFFFFFFF ? World.object_from_handle(info->target_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        human* hp = (human*)objp;
        info->human_nano_index = -1;
        hp->hflags.is_nano_effect_currently_applied = 0;
        if (info->killed_by_nano)
        {
            object_set_visibility_plus_children(objp, false);
            // Inlined render flag 2 (set_render_flags).
            objp->set_render_flags(2);
            hp->render_alpha = 0.0f;
        }
    }
    info->human_nano_index = -1;
    info->killed_by_nano = false;
    info->target_handle = 0xFFFFFFFF;
}

// 0x00AD33A0 (:2591): the nano rifle's fade (single player only). A death fades the whole human out (3.7 s, with
// nano effects on six bones); a hit fades its spot (1.5 s). Up to 16 fades run per human.
void human::start_nano_rifle_fade_out(bool death_full_fade, human* attacker, bool apply_default_damage, const vector& world_hit_pos)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    static const weapon_info* nano_rifle_info = weapons_get_weapon_info("nano_rifle");
    static float default_damage = nano_rifle_info->low_scale_damage.damage[0];
    // Original: this is checked for null after being used above (Multiplayer path aside).
    if (death_full_fade && this != nullptr && !is_killed())
    {
        process_take_hit_common(this, attacker, nano_rifle_info, hit_points, nullptr, DBT_BULLET, false);
    }
    else if (apply_default_damage)
    {
        process_take_hit_common(this, attacker, nano_rifle_info, default_damage, nullptr, DBT_BULLET, false);
    }
    const bool full_fade = death_full_fade || is_killed();
    // Magic numbers: fade times in ms.
    const int fade_ms = full_fade ? 3700 : 1500;
    const int effect_id = material_effects_get_nano_rifle_matfx_id(full_fade);
    if (full_fade)
    {
        if (hflags.fading_out_from_nano)
        {
            return;
        }
        hflags.fading_in = 0;
        hflags.fading_out = 1;
        hflags.fading_out_from_nano = 1;
        fade_timer.set(0);
        fade_time = fade_ms;
    }
    hflags.is_nano_effect_currently_applied = 1;
    material_effect_parameters params;
    params.lifetime_ms = -1;
    params.time_passed_ms = 0;
    params.flags = 0;
    params.cb_data = nullptr;
    params.update_render_done_cb = nullptr;
    params.world_pos = ZERO_VECTOR;
    params.use_local_pos = false;
    params.use_time_in_w = true;
    if (full_fade)
    {
        const int bones[6] = { h_info->pelvis_bone_index, h_info->head_bone_index, h_info->calf_l_bone_index, h_info->calf_r_bone_index, left_hand_bone_index, right_hand_bone_index };
        for (int i = 0; i < 6; i++)
        {
            effects_play(weapons_get_nano_fade_effect(), get_handle(), bones[i], false, get_handle(), 1.0f, false);
        }
    }
    params.world_pos = world_hit_pos;
    params.flags = 1;
    params.use_local_pos = true;
    params.use_time_in_w = true;
    params.update_render_done_cb = material_effect_completion_callback;
    nano_cb_info[nano_index].target_handle = get_handle();
    nano_cb_info[nano_index].killed_by_nano = full_fade;
    nano_cb_info[nano_index].human_nano_index = nano_index;
    params.cb_data = &nano_cb_info[nano_index];
    params.lifetime_ms = fade_ms;
    rl_renderable_instance* body = char_inst->rdd.is_loaded() ? (rl_renderable_instance*)char_inst->rdd.get_data()->renderable_instance_p : nullptr;
    nano_mat_fx[nano_index] = material_effects_apply(body, effect_id, &params);
    // Magic number: 16 fade slots (signed remainder, as compiled).
    nano_index = (nano_index + 1) % 16;
    if (nano_cb_info[nano_index].human_nano_index != -1)
    {
        material_effects_remove(nano_mat_fx[nano_index]);
    }
    params.update_render_done_cb = nullptr;
    params.cb_data = nullptr;
    rl_renderable_instance* head = rdd.is_loaded() ? (rl_renderable_instance*)rdd.get_data()->head_renderable_instance_p : nullptr;
    mat_fx_handle_for_head_skin = material_effects_apply(head, effect_id, &params);
    // The held items fade along; on a death the non weapon items are deleted after the fade (3.7 s).
    for (object* child = child_ptr; child != nullptr;)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            item* ip = (item*)child;
            if (!full_fade)
            {
                ip->start_nano_rifle_fade_out(params, effect_id);
            }
            else if (child->get_sub_type() != OT_SUB_ITEM_WEAPON)
            {
                ip->special_flags.fading_from_nano = 1;
                ip->deletion_timer.set(3700);
                ip->start_nano_rifle_fade_out(params, effect_id);
            }
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A9CB90 (:1309): a dead human next to the explosion (within Explosion_gibs_dist of its bbox) turns into gibs. Not
// players, not "no gibs" explosions.
static bool human_combat_maybe_gibify(human* hp, const vector& exp_pos, const explosion_info* e_info)
{
    if (!localization_explosion_gibs_allowed() || (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER) || (e_info->flags & 4) != 0 || (hp != nullptr && !hp->is_killed()))
    {
        return false;
    }
    // Original: hp is dereferenced unchecked from here.
    vector bmin;
    vector bmax;
    if (hp->ragdoll_idx == -1)
    {
        hp->get_local_bbox(bmin, bmax);
    }
    else
    {
        havok_ragdoll_get_local_bbox(bmin, bmax, hp->ragdoll_idx, hp->pos, hp->orient);
    }
    vector local_pos = exp_pos - hp->pos;
    local_pos.rotate(hp->orient);
    if (!ix_sphere_in_box(local_pos, Explosion_gibs_dist, bmin, bmax))
    {
        return false;
    }
    hp->set_is_visible(false);
    const vector center((bmax.x + bmin.x) * 0.5f + hp->pos.x, hp->pos.y + (bmin.y + bmax.y) * 0.5f, hp->pos.z + (bmin.z + bmax.z) * 0.5f);
    vector dir = center - exp_pos;
    // The effect's percent: 1 at the explosion, 0 from 1 m away.
    float percent = 1.0f - dir.mag_and_normalize_safe();
    if (percent - 0.0f < 0.0f)
    {
        percent = 0.0f;
    }
    matrix effect_orient;
    effect_orient.make_quick(dir);
    object_effect* effect = World.object_from_handle<object_effect>(effects_play(Explosion_gibs_effect, center, effect_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE));
    if (effect != nullptr)
    {
        effect->set_expression_variable(EFFECTS_VAR_PERCENT, percent);
    }
    hp->hflags.was_gibbed = 1;
    if (Explosion_gibs_debug)
    {
        // Original: the distance label is formatted without its argument and never used.
        char label[32];
        _snprintf_safe(label, sizeof(label), "%.2f");
    }
    return true;
}

// 0x00AD4440 (:3190): an explosion ragdolls the human (the server forces it for the dead in multiplayer) and throws it;
// the server sends a player's throw to the clients.
void human_combat_apply_explosion_ragdoll_impulse(human* hp, const vector& impulse, const vector& angular_impulse, const human* perp)
{
    const unsigned int perp_handle = perp == nullptr ? 0xFFFFFFFF : perp->get_handle();
    const bool forced = Multiplayer && multi_is_server() && (hp == nullptr || hp->is_killed());
    if (!human_ragdoll_start(hp, forced, perp_handle))
    {
        return;
    }
    // Magic number: impulses become velocities divided by 90 (the ragdoll's mass).
    if (stats_cheat_enabled(CHEATS_SUPER_DEBRIS))
    {
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, impulse / 90.0f, true);
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, impulse / 90.0f, true);
    }
    else
    {
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, impulse / 90.0f, true);
    }
    vector angular = angular_impulse / 90.0f;
    float mag = angular.mag_and_normalize_safe();
    // Magic number: at most 4 pi rad/s.
    if (12.566371f - mag < 0.0f)
    {
        mag = 12.566371f;
    }
    angular = angular * mag;
    // Magic number: body part filter 0x3B.
    havok_ragdoll_apply_angular_velocity(hp->ragdoll_idx, angular, 0x3B);
    if (Multiplayer == true && multi_is_server() && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        multi_game_player_ragdoll(hp->get_obj_type() == OT_HUMAN ? (player*)hp : nullptr, impulse / 90.0f, angular);
    }
}

// 0x00ADBB30 (:2942): the damage and push of an explosion on a human. Damage falls off from the max at the center to
// the min at the radius and is 0 out to the knockdown radius; shields, turrets, armour, vehicles, difficulty and the
// player's backpack scale it. The push knocks down or ragdolls; corpses may gib; incendiary explosions ignite. The
// server sends the hit to the clients.
void human_combat_process_explosion_hit(human* target, human* attacker, const weapon_info* w_info, const vector& explosion_pos, const explosion_info* e_info, const object* owner, bool server_update)
{
    if (e_info == nullptr)
    {
        return;
    }
    // An expanding explosion deals its damage in parts, one per break.
    float damage_scale = 1.0f;
    if (e_info->time_between_breaks != 0 && e_info->expanding_explosion_duration != 0)
    {
        damage_scale = (float)e_info->time_between_breaks / (float)e_info->expanding_explosion_duration;
    }
    // The explosion is measured to the middle of a standing human, or 0.5 m above a lying one.
    vector target_pos;
    const human_ragdoll_state rs = target->ragdoll_state;
    const bool upright_ragdoll = rs == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_FLINCH ||
                                 (rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !target->hflags.ragdoll_on_impact_use_ragdoll_pos);
    if (upright_ragdoll && target->cur_stance != HST_PRONE)
    {
        vector head;
        human_get_head_pos(target, head);
        target_pos = (target->pos + head) * 0.5f;
    }
    else
    {
        target_pos.x = target->pos.x;
        target_pos.y = target->pos.y + 0.5f;
        target_pos.z = target->pos.z;
    }
    float dist = vec_dist(explosion_pos, target_pos);
    // A riot shield facing the explosion.
    if (target->hflags.riot_shield && target->shield_handle != 0xFFFFFFFF)
    {
        item* shield = World.object_from_handle<item>(target->shield_handle);
        const human_ragdoll_state rs2 = target->ragdoll_state;
        if (shield != nullptr &&
            (rs2 == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs2 == HUMAN_RAGDOLL_STATE_NONE || rs2 == HUMAN_RAGDOLL_STATE_FLINCH ||
             (rs2 == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !target->hflags.ragdoll_on_impact_use_ragdoll_pos)))
        {
            vector head;
            human_get_head_pos(target, head);
            const vector to_explosion = (explosion_pos - (head + target->pos) * 0.5f).copy_normalize_safe();
            // Item flag 0x2000: the shield's face is its uvec, not its fvec.
            const vector facing = ((shield->info->flags >> 13) & 1) == 0 ? shield->orient.fvec.copy_normalize_safe() : shield->orient.uvec.copy_normalize_safe();
            if (to_explosion * facing >= MIN_HUMAN_IS_SHIELDED_DOT)
            {
                const bool crouching = target->cur_stance == HST_CROUCH;
                dist = dist / (crouching ? Shielded_explosion_crouching_radius_modifier : Shielded_explosion_radius_modifier);
                damage_scale = (crouching ? Shielded_explosion_crouching_damage_modifier : Shielded_explosion_damage_modifier) * damage_scale;
            }
        }
    }
    if (human_combat_get_manned_turret(target) != nullptr)
    {
        damage_scale = Turret_gunner_explosion_damage_modifier * damage_scale;
    }
    const bool body_armor = target->h_info->flags.body_armor;
    if (body_armor)
    {
        damage_scale = Armored_explosion_damage_modifier * damage_scale;
    }
    bool protected_in_vehicle;
    if (human_in_protected_vehicle_pos(target) ||
        (target->vehicle_handle != 0xFFFFFFFF && Player->vehicle_handle == target->vehicle_handle && human_in_vehicle_interaction(target, nullptr)))
    {
        protected_in_vehicle = true;
        vehicle* vp = nullptr;
        if (target->vehicle_handle != 0xFFFFFFFF)
        {
            // The player's passengers take no explosion damage.
            if (Player->vehicle_handle == target->vehicle_handle)
            {
                damage_scale = 0.0f;
            }
            vp = World.object_from_handle<vehicle>(target->vehicle_handle);
        }
        // Original: vp is dereferenced unchecked when the human is not in a vehicle.
        if (vehicle_is_tank(vp))
        {
            damage_scale = 0.0f;
        }
        else
        {
            damage_scale = (vp->info->default_team == HUMAN_TEAM_EDF ? Vehicle_armored_passenger_explosion_damage_multiplier : Vehicle_passenger_explosion_damage_multiplier) * damage_scale;
        }
    }
    else
    {
        protected_in_vehicle = false;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && target->get_sub_type() == OT_SUB_HUMAN_NPC && Player != nullptr && human_teams_are_allies(target->current_team, Player->current_team))
    {
        damage_scale = e_info->player_damage_mult * damage_scale;
    }
    const float max_damage = (float)e_info->m_human_max_damage_hitpoints;
    const float min_damage = (float)e_info->m_human_min_damage_hitpoints;
    float radius = e_info->m_radius;
    float knockdown_radius = e_info->m_knockdown_radius;
    // A player's backpack scales the area of its explosions (the active abilities while activated).
    if (attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)attacker)->backpack != nullptr)
    {
        const multi_object_backpack* backpack = ((player*)attacker)->backpack;
        const float area_factor = backpack->get_backpack_abilities()->area_damage_dealt_factor;
        radius = area_factor * radius;
        knockdown_radius = area_factor * knockdown_radius;
    }
    float damage = 0.0f;
    float impulse_mag = 0.0f;
    vector angular_impulse = ZERO_VECTOR;
    vector push_dir;
    vec_normalized_dir(push_dir, target_pos, explosion_pos);
    if (dist <= radius)
    {
        damage = fl_parameter(dist, 0.0f, radius, max_damage, min_damage, false);
    }
    else if (dist <= knockdown_radius)
    {
        damage = fl_parameter(dist, radius, knockdown_radius, min_damage, 0.0f, false);
    }
    damage = damage * damage_scale;
    // Magic number: the super debris cheat triples explosion damage to humans.
    if (stats_cheat_enabled(CHEATS_SUPER_DEBRIS))
    {
        damage = damage * 3.0f;
    }
    if (target->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        damage = e_info->player_damage_mult * damage;
        if (!Multiplayer)
        {
            damage = difficulty_get_value(DIFFICULTY_EXPLOSION_DAMAGE, NUM_DIFFICULTY_LEVELS) * damage;
        }
    }
    else if (activity_house_arrest_is_human_hostage(target))
    {
        damage = e_info->player_damage_mult * damage;
    }
    else
    {
        // The player's squad takes the player's share.
        const object* objp = target->get_handle() != 0xFFFFFFFF ? World.object_from_handle(target->get_handle()) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC && ((const npc*)objp)->get_ai() != nullptr &&
            ((const npc*)objp)->get_ai()->is_in_player_squad(true) != nullptr)
        {
            damage = e_info->player_damage_mult * damage;
        }
    }
    if (damage > 0.0f)
    {
        if (e_info->time_between_breaks == 0)
        {
            impulse_mag = fl_parameter(damage, max_damage, 0.0f, e_info->m_impulse_magnitude, 0.0f, false);
        }
        process_take_hit_common(target, attacker, w_info, damage, owner, DBT_EXPLOSION, false);
        if (Player != nullptr && target->get_handle() == Player->get_handle() && ((player*)target != Player || !target->is_killed()))
        {
            // Magic number: 50 damage is a full indicator.
            hud_register_hit(push_dir * impulse_mag, damage / 50.0f);
        }
        if (Multiplayer && target->get_sub_type() == OT_SUB_HUMAN_PLAYER && attacker != nullptr && (player*)attacker == Player)
        {
            multi_game_stats_record_hit((player*)attacker, (player*)target, w_info, DBT_EXPLOSION);
        }
        angular_impulse = (UP_VECTOR ^ push_dir) * impulse_mag;
        if ((player*)target == Player)
        {
            player_damaged_by_explosion_event event;
            event.info_handle = explosion_get_handle_from_ptr(e_info);
            gem_post_event(event);
        }
    }
    if (stats_cheat_enabled(CHEATS_SUPER_DEBRIS))
    {
        impulse_mag = Super_debris_human_impulse * impulse_mag;
    }
    const bool no_ragdoll = target->h_info->flags.guerrilla_tech || target->h_info->flags.guerrilla_rfc || target->h_info->flags.old_coot || target->hflags.disallow_flinches_and_ragdolls;
    bool push = false;
    if (human_in_vehicle_interaction(target, nullptr))
    {
        // Getting in or out of a vehicle: a strong damaging push ragdolls, unless the vehicle is flagged no_ragdoll.
        if (!no_ragdoll && impulse_mag > Vi_impulse_ragdoll && !protected_in_vehicle && damage != 0.0f)
        {
            const vehicle* vp = human_in_vehicle(target);
            push = vp == nullptr || !vp->m_flags.no_ragdoll;
        }
    }
    else if (!no_ragdoll)
    {
        if (!target->is_killed())
        {
            if (target->get_sub_type() != OT_SUB_HUMAN_NPC)
            {
                // A local player flagged to ragdoll on the next explosion does (with its own get up time).
                if (damage > 0.0f)
                {
                    player* pp = World.object_cast<player>(target);
                    if (object_is_local_player(pp) && pp->pflags.ragdoll_on_next_explosion)
                    {
                        pp->pflags.ragdoll_on_next_explosion = 0;
                        human_combat_apply_explosion_ragdoll_impulse(pp, push_dir * impulse_mag, angular_impulse, attacker);
                        if (pp->ragdoll_override_get_up_time != -1)
                        {
                            pp->knockdown_timestamp.set(pp->ragdoll_override_get_up_time);
                            pp->ragdoll_override_get_up_time = -1;
                        }
                    }
                }
            }
            else
            {
                // Npcs knock down above 100 (2000 in body armour), else flinch (armoured ones only when damaged).
                ai_system* ai = ((npc*)target)->get_ai();
                if (!body_armor)
                {
                    if (impulse_mag > 100.0f)
                    {
                        ai->play_explosion_knockdown(impulse_mag, push_dir, angular_impulse, e_info->always_ragdoll);
                    }
                    else
                    {
                        ai->play_explosion_flinch(impulse_mag, push_dir);
                    }
                }
                else if (impulse_mag > 2000.0f)
                {
                    ai->play_explosion_knockdown(impulse_mag, push_dir, angular_impulse, e_info->always_ragdoll);
                }
                else if (damage > 0.0f)
                {
                    ai->play_explosion_flinch(impulse_mag, push_dir);
                }
            }
        }
        else if (target->get_sub_type() != OT_SUB_HUMAN_NPC || !body_armor || damage >= 500.0f)
        {
            // Magic number: armoured npc corpses gib only from 500 damage.
            push = !human_combat_maybe_gibify(target, explosion_pos, e_info);
        }
        else
        {
            push = true;
        }
    }
    if (push)
    {
        human_combat_apply_explosion_ragdoll_impulse(target, push_dir * impulse_mag, angular_impulse, attacker);
    }
    // Explosion flag 1: incendiary.
    if ((!Multiplayer || multi_is_server()) && (e_info->flags & 1) != 0 && !target->h_info->flags.nonflammable && localization_allow_humans_on_fire())
    {
        human_fire_ignite(target, attacker);
    }
    if (Multiplayer == true && multi_is_server())
    {
        const human* owner_human = owner != nullptr && owner->get_obj_type() == OT_HUMAN ? (const human*)owner : nullptr;
        multi_gw_send_explosion_hit(attacker, target, owner_human, explosion_pos, w_info, e_info);
    }
}

// ---- not recovered yet
// 0x00A09CB0 (:3619)
bool human_combat_allowed_to_damage_target(const human* hp, const human* target)
{
    if (hp == nullptr || target == nullptr || hp->hflags.no_damage != 0)
    {
        return false;
    }
    return hp->get_sub_type() != OT_SUB_HUMAN_NPC || target->get_sub_type() != OT_SUB_HUMAN_NPC || !No_npc_npc_damage;
}

// 0x00A555A0 (:3681)
human* human_combat_get_hit_human(const object* hit_objp)
{
    if (hit_objp == nullptr)
    {
        return nullptr;
    }
    const vehicle* vp = nullptr;
    switch (hit_objp->get_obj_type())
    {
    case OT_HUMAN:
        return const_cast<human*>(static_cast<const human*>(hit_objp));
    case OT_VEHICLE:
        vp = reinterpret_cast<const vehicle*>(hit_objp);
        break;
    case OT_ITEM:
    case OT_TURRET:
    {
        const turret* tp = hit_objp->get_obj_type() == OT_ITEM ? turret_from_item((const item*)hit_objp) : (const turret*)hit_objp;
        if (tp == nullptr)
        {
            return nullptr;
        }
        human* gunner = human_combat_turret_get_operator(tp);
        if (gunner != nullptr)
        {
            return gunner;
        }
        vp = World.object_from_handle<vehicle>(tp->get_parent());
        break;
    }
    default:
        return nullptr;
    }
    if (vp == nullptr)
    {
        return nullptr;
    }
    const int num_seats = vehicle_get_num_seats(vp);
    for (int seat = VSI_DRIVER; seat < num_seats; seat++)
    {
        human* passenger = vehicle_passenger_in_seat(vp, static_cast<vehicle_seat_index>(seat));
        if (passenger != nullptr)
        {
            return passenger;
        }
    }
    return nullptr;
}

// 0x00A7F1A0 (:3750)
// A marauder holding a melee weapon may block a melee attack from the front.
bool human_combat_maybe_block_melee(const object* victim, const object* attacker)
{
    if (victim == nullptr || attacker == nullptr || Multiplayer || wrecking_crew_is_active())
    {
        return false;
    }
    npc* np = victim->get_obj_type() == OT_HUMAN && victim->get_sub_type() == OT_SUB_HUMAN_NPC ? (npc*)victim : nullptr;
    human* attacker_hp = attacker->get_obj_type() == OT_HUMAN ? (human*)attacker : nullptr;
    if (np == nullptr || attacker_hp == nullptr || np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    const weapon* wp = np->get_equipped_weapon();
    if (wp == nullptr || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        return false;
    }
    vector to_attacker;
    to_attacker.x = attacker->pos.x - victim->pos.x;
    to_attacker.y = attacker->pos.y - victim->pos.y;
    to_attacker.z = attacker->pos.z - victim->pos.z;
    // Magic number: cos 30 degrees.
    if (!((double)(victim->orient.fvec * to_attacker.copy_flatten_normalize()) > (double)0.8660254f))
    {
        return false;
    }
    if (np->get_ai()->planner.get_current_action() != melee_block_action::get_instance() &&
        (np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE) || !int_rand_chance(50)))
    {
        return false;
    }
    if (attacker_hp->char_inst->is_playing_action(ANIMATION_ACTION_MELEE) || attacker_hp->char_inst->is_playing_action(ANIMATION_ACTION_MELEE3))
    {
        np->char_inst->play_action(ANIMATION_ACTION_BLOCK_FLINCH_LOW, ANIM_ALIAS_NONE, 1.0f, 0);
    }
    else
    {
        np->char_inst->play_action(ANIMATION_ACTION_BLOCK_FLINCH_HIGH, ANIM_ALIAS_NONE, 1.0f, 0);
    }
    return true;
}

#if 0
static animation_action Override_animation; // 0x0301A238
static bool Override_attack; // 0x0302F064
// Not referenced in rfg.exe (inlined or dead).
static void mp_should_send_melee_hit_init(const object* target, const object* attacker); // 0x00A2F880 (:1283)
static bool mp_should_send_melee_hit(const object* target, const object* attacker); // 0x00A2F8C0 (:1295)
#endif
