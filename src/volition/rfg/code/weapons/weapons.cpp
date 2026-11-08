// Original: volition/rfg/code/weapons/weapons.cpp
//
// Partial (player_minimal.md P3, P4, P5): weapons_system_init (the firing patterns and the weapons table), the
// weapon_info lookups and the weapon object (creation and streaming, ammo, reload, the trigger state and the frame).
// Firing, bullets and the special weapons are not ported. Compiled in rfg_weapons_unity.cpp, first of the weapons
// sources because the firing patterns file reads its Weapon_firing_patterns.
// The external interface is declared in weapons.h.

#include "volition/rfg/code/weapons/weapons.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/rfg/code/ai/combat/combat_common.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapon_firing_patterns.h"
#include "volition/rfg/code/weapons/weapons_table_parser.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/tracers.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_create.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/humans/human_fire.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"

#include <stdio.h>

#include "keen/base/trace.hpp"

// ---- globals and file statics compiled since P3
// 0x0133AD70, indexed by weapon_class_type (the Weapon_Class names of weapons.xtbl)
const char* const Weapon_class_names[17] = {
    "bullet", "launcher", "thrown", "thrown_charge", "thrown_mine", "thrown_grenade", "melee", "melee_sledgehammer", "melee_gutter",
    "grinder", "arc_welder", "nano_rifle", "mass_driver", "harpoon", "singularity_bomb", "repair", "flame",
};
// 0x0133ADB4, indexed by weapon_ammo_type
const char* const Weapon_ammo_type_names[8] = {"bullet", "projectile", "thrown", "melee", "electricity", "repair", "blanks", "flame"};
int Num_weapon_infos; // 0x03882C94
weapon_info* Weapon_infos; // 0x03882C9C
static unsigned int Nano_fade_effect = 0xFFFFFFFF; // 0x016EBC6C
static unsigned int Repair_fade_effect = 0xFFFFFFFF; // 0x016EBC70
static int Dropped_weapon_material_effect_id = -1; // 0x016EBC74
static int Nano_material_effect; // 0x03882CB8
static weapon_firing_pattern* Weapon_firing_patterns; // 0x03882CD0

// weapon_firing_patterns.cpp, later in the unity TU
static void weapon_firing_patterns_table_parse(bool refresh, int dlc_id); // 0x00BDFBE0

// 0x00BE2900 (:3267)
void weapons_system_init()
{
    if (Weapon_firing_patterns == nullptr)
    {
        weapon_firing_patterns_table_parse(false, -1);
    }
    Weapon_infos = nullptr;
    Num_weapon_infos = 0;
    weapons_read_table(false, false, -1);
    Nano_fade_effect = effects_get_handle("wep_nanorifle_chunk_a");
    Nano_material_effect = material_effects_get_reconstructor_shards_id();
    Repair_fade_effect = effects_get_handle("wep_reconstructor_chunk_a");
    Dropped_weapon_material_effect_id = material_effects_find_by_name("weapon_sheen");
}

// 0x00BDCB80 (:3308)
unsigned int weapons_get_weapon_info_handle(const char* name)
{
    return weapons_get_weapon_info_handle(checksum_stri(name, 0));
}

// 0x00BD9CD0 (:3314): the index in Weapon_infos.
unsigned int weapons_get_weapon_info_handle(checksum_stri name_crc)
{
    for (int i = 0; i < Num_weapon_infos; i++)
    {
        if (Weapon_infos[i].name_crc == name_crc)
        {
            return (unsigned int)i;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00BD9D40 (:3338): the handles run 0 to Num_weapon_infos - 1
unsigned int weapons_get_first_weapon_info_handle()
{
    if (Num_weapon_infos == 0)
    {
        return 0xFFFFFFFF;
    }
    return Weapon_infos != nullptr ? 0 : 0xFFFFFFFF;
}

// 0x00BD9D60 (:3347)
unsigned int weapons_get_last_weapon_info_handle()
{
    if (Num_weapon_infos != 0 && &Weapon_infos[Num_weapon_infos - 1] != nullptr)
    {
        return (unsigned int)(Num_weapon_infos - 1);
    }
    return 0xFFFFFFFF;
}

// 0x00BD9DA0 (:3356)
unsigned int weapons_get_next_weapon_info_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF || handle + 1 >= (unsigned int)Num_weapon_infos)
    {
        return 0xFFFFFFFF;
    }
    return handle + 1;
}

// 0x00BD9DC0 (:3365)
unsigned int weapons_get_prev_weapon_info_handle(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && handle != 0)
    {
        return handle - 1;
    }
    return 0xFFFFFFFF;
}

// 0x00BD9D10 (:3325)
unsigned int weapons_get_weapon_info_handle(const weapon_info* w_info)
{
    if (w_info == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return (unsigned int)(w_info - Weapon_infos);
}

// 0x00BDCBD0 (:3374)
const weapon_info* weapons_get_weapon_info(const char* name)
{
    return weapons_get_weapon_info(checksum_stri(name, 0));
}

// 0x00BDCC30 (:3379)
const weapon_info* weapons_get_weapon_info(checksum_stri name_crc)
{
    const unsigned int handle = weapons_get_weapon_info_handle(name_crc);
    return handle != 0xFFFFFFFF ? &Weapon_infos[handle] : nullptr;
}

// 0x00BD9DE0 (:3384)
const weapon_info* weapons_get_weapon_info(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && (int)handle < Num_weapon_infos)
    {
        return &Weapon_infos[handle];
    }
    return nullptr;
}

// 0x00BD9E10 (:3395)
const weapon_info* weapons_get_weapon_info(const item_info* weapon_obj_item_info)
{
    for (int i = 0; i < Num_weapon_infos; i++)
    {
        if (Weapon_infos[i].weapon_obj_item_info == weapon_obj_item_info)
        {
            return &Weapon_infos[i];
        }
    }
    return nullptr;
}

// 0x00BD9E40 (:3409)
const weapon_info* weapons_get_weapon_info(const inv_item_info* weapon_inv_item_info)
{
    for (int i = 0; i < Num_weapon_infos; i++)
    {
        if (Weapon_infos[i].weapon_inv_item_info == weapon_inv_item_info)
        {
            return &Weapon_infos[i];
        }
    }
    return nullptr;
}

// ---- the weapon object (player_minimal.md P4: creation, streaming and the ammo state; firing is P5)
static bool Unlimited_magazine_ammo; // 0x03882CBC (debug cheat, never set)
static object_pool<weapon,256,1> weaponpool; // 0x03882D10
// read from rfg.exe: pool usage 0x30140 bytes, 256 weapons
static obj_class_instantiator weapon_instantiator("weapon", alloc_weapon, types_weapon, bpo_index_weapon, classname_weapon, weapon::pool_remaining, 0x30140, 256); // 0x03882CFC

// ---- class registration (:382)
// 0x005DC720 (.h:415)
const object_type weapon::get_type_static()
{
    return OT_ITEM;
}

// 0x005DC730 (.h:415)
const object_sub_type weapon::get_subtype_static()
{
    return OT_SUB_ITEM_WEAPON;
}

// 0x00BD90D0 (.h:415)
const char* weapon::get_classname_static()
{
    return "weapon";
}

// 0x00BD90E0 (.h:415)
int weapon::get_bpo_index_static()
{
    return 0;
}

// 0x00BE0F80 (.h:415)
const char* weapon::get_classname() const
{
    return "weapon";
}

static float Saved_refire_delay; // 0x03882CC4

// 0x00BD95A0 (:340): the debug console command jetter_rocket_delay <number>: the jetter rocket launcher's refire delay,
// the old value restored by the next call. Magic number: 0x10, a float argument (optional).
void unused_dcf_jetter_rocket_delay()
{
    if (Console_help == 0 && Console_command != 0 && console_get_arg(0x10, 1))
    {
        weapon_info* info = nullptr;
        for (int i = 0; i < Num_weapon_infos; i++)
        {
            if (v_stricmp(Weapon_infos[i].name, "turret_jetter_rocket") == 0)
            {
                info = &Weapon_infos[i];
            }
        }
        if (info == nullptr)
        {
            console_output("can't find jetter rocket launcher.", nullptr, false);
            return;
        }
        char text[256];
        if (Saved_refire_delay != 0.0f)
        {
            sprintf(text, "restoring jetter delay to %f\n", (double)Saved_refire_delay);
            info->default_refire_delay = Saved_refire_delay;
            Saved_refire_delay = 0.0f;
        }
        else
        {
            Saved_refire_delay = info->default_refire_delay;
            info->default_refire_delay = Console_arg_float;
            sprintf(text, "changing refire delay from %f to %f\n", (double)Saved_refire_delay, (double)Console_arg_float);
        }
        console_output(text, nullptr, false);
        return;
    }
    console_output("jetter_rocket_delay <number> to set the jetter rocket launcher delay", nullptr, false);
}

// 0x00BD9770 (:382)
void types_weapon(signed char& type, signed char& subtype)
{
    type = OT_ITEM;
    subtype = OT_SUB_ITEM_WEAPON;
}

// 0x00BE0EF0 (:382)
object* alloc_weapon()
{
    return weaponpool.alloc(0);
}

// 0x00BD9780 (:382)
int bpo_index_weapon()
{
    return 0;
}

// 0x00BD9790 (:382)
const char* classname_weapon()
{
    return "weapon";
}

// 0x00BD97A0 (:382)
unsigned int weapon::get_classname_hash()
{
    return weapon_instantiator.this_hash;
}

// 0x00BDB820 (:382)
int weapon::pool_remaining()
{
    return weaponpool.get_free();
}

// 0x00BE4F00 (:382)
void weapon::pool_free()
{
    weaponpool.free(this);
}

// 0x00BE0F00 (:383)
weapon::weapon()
{
    // the timestamps, fire_info and arc_welder_best_point are set by their constructors
}

// 0x00BE5030 (:698)
// Load: the weapon info (by pointer, item name or weapon type); multiplayer level weapons need weapon respawning
// (or are the king of the hill repair tool) and must not be locked by the server options. Then the item, the flags,
// the ammo (melee: one round; else full magazines of the info's start count, reserve below the maximum, or the
// saved counts), refire delay, timers and effect state, a weapon persona; preplaced weapons are synced by a
// multiplayer server. Save: the ammo, flags, refire delay and the timers left.
bool weapon::serialize(obj_prop_block& pblock, bool load)
{
    if (load)
    {
        const weapon_info* info_ptr;
        if (pblock.get_optional<const weapon_info*>(PROP_WEAPON_INFO, info_ptr))
        {
            w_info = info_ptr;
        }
        else
        {
            char name[256];
            if (!pblock.get_optional(PROP_ITEM_INFO_NAME, name))
            {
                pblock.get(PROP_WEAPON_TYPE, name);
            }
            w_info = weapons_get_weapon_info(name);
        }
        if (w_info == nullptr)
        {
            keen::debug::nativePrintArgs("weapon::serialize w_info failed");
        }
        if (w_info == nullptr)
        {
            return false;
        }
        // Magic number: handles with 0xFF in their third byte are not level weapons.
        if (Multiplayer == true && ((get_handle() >> 16) & 0xFF) != 0xFF)
        {
            if (Mp_server_options.generic_options.weapon_respawn_speed == MGWRS_NONE)
            {
                if (w_info->name == nullptr || multi_gameplay_mode_get() != MGPM_KING_OF_THE_HILL || v_stricmp(w_info->name, "repair_tool") != 0)
                {
                    return false;
                }
            }
            else
            {
                // Magic numbers: the unique ids of the lockable weapons (weapons.xtbl).
                multi_game_lockable_weapons locked = MGLW_ROCKET_LAUNCHER;
                bool lockable = true;
                switch (w_info->unique_id)
                {
                case 0x20: locked = MGLW_ROCKET_LAUNCHER; break;
                case 0x48: locked = MGLW_THERMOBARIC_ROCKET; break;
                case 0x49: locked = MGLW_REMOTE_CHARGES; break;
                case 0x4A: locked = MGLW_NANO_RIFLE; break;
                case 0x5B: locked = MGLW_PISTOL; break;
                case 0x5C: locked = MGLW_ARC_WELDER; break;
                case 0x5F: locked = MGLW_PEACEKEEPER; break;
                case 0x60: locked = MGLW_SNOWBALL_GUN; break;
                case 0x61: locked = MGLW_RAIL_DRIVER; break;
                case 0x62: locked = MGLW_SNIPER_RIFLE; break;
                case 0x63: locked = MGLW_ENFORCER; break;
                case 0x64: locked = MGLW_ASSAULT_RIFLE; break;
                case 0x65: locked = MGLW_SHOTGUN; break;
                case 0x69: locked = MGLW_SING_BOMB; break;
                case 0x71: locked = MGLW_GRINDER; break;
                case 0x75: locked = MGLW_PROX_MINE; break;
                default: lockable = false; break;
                }
                if (lockable && multi_game_settings_is_weapon_locked(&Mp_server_options, locked))
                {
                    return false;
                }
            }
        }
    }
    if (!item::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set<int>(PROP_WEAPON_ROUNDS_IN_MAGAZINE, rounds_in_magazine);
        pblock.set<int>(PROP_WEAPON_ROUNDS_IN_RESERVE, rounds_in_reserve);
        pblock.set<weapon_flags>(PROP_WEAPON_FLAGS, flags);
        pblock.set<float>(PROP_WEAPON_REFIRE_DELAY_TIME, refire_delay_time);
        if (reload_delay_timer.valid())
        {
            pblock.set<int>(PROP_WEAPON_RELOAD_DELAY_TIME, reload_delay_timer.time_until());
        }
        if (last_fired_timer.valid())
        {
            pblock.set<int>(PROP_WEAPON_LAST_FIRED_TIMER, last_fired_timer.time_until());
        }
        return true;
    }
    if (!pblock.get_optional<weapon_flags>(PROP_WEAPON_FLAGS, flags))
    {
        *(unsigned char*)&flags = 0;
    }
    flags.processed_since_last_trigger_pull = false;
    if (w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        rounds_in_magazine = 1;
        rounds_in_reserve = 0;
    }
    else
    {
        if (w_info->magazine_start_num == 0)
        {
            rounds_in_magazine = 0;
            rounds_in_reserve = 0;
        }
        else
        {
            rounds_in_magazine = w_info->magazine_size;
            const int start_reserve = (w_info->magazine_start_num - 1) * (int)w_info->magazine_size;
            const int max_reserve = get_max_rounds() - (int)w_info->magazine_size;
            rounds_in_reserve = start_reserve < max_reserve ? start_reserve : max_reserve;
        }
        pblock.get_optional<int>(PROP_WEAPON_ROUNDS_IN_MAGAZINE, rounds_in_magazine);
        pblock.get_optional<int>(PROP_WEAPON_ROUNDS_IN_RESERVE, rounds_in_reserve);
    }
    float refire_delay;
    if (!pblock.get_optional<float>(PROP_WEAPON_REFIRE_DELAY_TIME, refire_delay))
    {
        refire_delay = w_info->npc_refire_delay;
    }
    refire_delay_time = refire_delay;
    for (int i = 0; i < 5; i++)
    {
        cb[i].func = nullptr;
        cb[i].user_handle = 0xFFFFFFFF;
    }
    secondary_rockets_to_fire = 0;
    num_rounds_fired = 0;
    last_fired_timer.invalidate();
    arc_welder_post_arc_timer.invalidate();
    multi_rocket_trigger_down_timer.invalidate();
    int ms;
    if (pblock.get_optional<int>(PROP_WEAPON_LAST_FIRED_TIMER, ms))
    {
        last_fired_timer.set(ms);
    }
    reload_delay_timer.invalidate();
    if (pblock.get_optional<int>(PROP_WEAPON_RELOAD_DELAY_TIME, ms))
    {
        reload_delay_timer.set(ms);
    }
    tracer_count = 0;
    current_spread = w_info->min_spread;
    muzzle_flash_intensity = 0.0f;
    muzzle_flash_effect_handle = 0xFFFFFFFF;
    muzzle_smoke_effect_handle = 0xFFFFFFFF;
    special_effect_handle = 0xFFFFFFFF;
    secondary_special_effect_handle = 0xFFFFFFFF;
    muzzle_smoke_timestamp.invalidate();
    one_shot_delay_timestamp.set(0);
    weapon_hit_sound_timestamp.set(0);
    weapon_fire_sound_timestamp.set(0);
    turret_handle = 0xFFFFFFFF;
    fire_sound_alr_id = -1;
    secondary_sound_id = -1;
    special_sound_id = -1;
    melee_combat_in_combo = false;
    overheated = false;
    overheat_percent = 0.0f;
    overheated_effect_handle = 0xFFFFFFFF;
    grinder_speed_percent = 0.0f;
    grinder_heat_effect = 0.0f;
    // Original: the entries are written without being added (the farray stays empty).
    arc_welder_best_point[0].hit_handle = 0xFFFFFFFF;
    arc_welder_best_point[1].hit_handle = 0xFFFFFFFF;
    arc_welder_best_point[2].hit_handle = 0xFFFFFFFF;
    mat_fx_handle[0] = 0xFFFFFFFF;
    mat_fx_handle[1] = 0xFFFFFFFF;
    mat_fx_handle[2] = 0xFFFFFFFF;
    mat_fx_handle[3] = 0xFFFFFFFF;
    current_mat_fx = 0xFFFFFFFF;
    mat_fx_clip_handle = 0xFFFFFFFF;
    mat_fx_handle_for_clip = 0xFFFFFFFF;
    grip_tag_index = -1;
    set_new_weapon_persona(0xFFFFFFFF);
    if (Multiplayer == true && multi_is_server() && item_flags.preplaced)
    {
        remote_obj_flags.remote_object = 1;
        // Magic number: remote object type 3 (weapons).
        multi_core_object_create_remote_object(3, get_handle());
    }
    return true;
}

// 0x00BDBAD0 (:928)
// Load: the weapon's tags. Unload: a playing pickup effect stops.
bool weapon::stream(bool load)
{
    if (!item::stream(load))
    {
        return false;
    }
    if (load)
    {
        muzzle_tag_index = get_tag_index_from_name("muzzle_flash");
        grip_tag_index = get_tag_index_from_name("weapon_handle");
        secondary_grip_tag_index = get_tag_index_from_name("secondary_handle");
        destruction_tag_index = get_tag_index_from_name("destruction");
        secondary_destruction_tag_index = get_tag_index_from_name("destruction2");
        melee_blood_effect_tag_index = get_tag_index_from_name("blood");
        return true;
    }
    if (flags.playing_pickup_effect)
    {
        weapon_stop_material_effect(this);
        flags.playing_pickup_effect = false;
    }
    return true;
}

// 0x00BE1480
// The item, then the looping sound, the muzzle flash, smoke and special effects (stopped), and the arc welder's
// loops (or the special sound).
void weapon::free_this()
{
    item::free_this();
    if (fire_sound_alr_id != -1)
    {
        // Magic number: a 100 ms fade.
        audiolib_att_loop_rel_stop(fire_sound_alr_id, 100);
        fire_sound_alr_id = -1;
    }
    if (muzzle_flash_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(muzzle_flash_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->stop(object_effect::object_effect::SS_STOP_EMITTING);
            muzzle_flash_effect_handle = 0xFFFFFFFF;
        }
    }
    if (muzzle_smoke_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(muzzle_smoke_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->deattach_from_host();
            effect->stop(object_effect::object_effect::SS_STOP_EMITTING);
        }
    }
    muzzle_smoke_effect_handle = 0xFFFFFFFF;
    unsigned int* effects[3] = {&overheated_effect_handle, &special_effect_handle, &secondary_special_effect_handle};
    for (int i = 0; i < 3; i++)
    {
        if (*effects[i] != 0xFFFFFFFF)
        {
            object_effect* effect = (object_effect*)World.object_from_handle(*effects[i]);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                effect->stop(object_effect::object_effect::SS_STOP_EMITTING);
            }
        }
        *effects[i] = 0xFFFFFFFF;
    }
    if (w_info->weapon_class == WEAPON_CLASS_ARC_WELDER)
    {
        int* sounds[2] = {&special_sound_id, &fire_sound_alr_id};
        for (int i = 0; i < 2; i++)
        {
            if (*sounds[i] != -1)
            {
                // Magic number: a 500 ms fade.
                audiolib_att_loop_rel_stop(*sounds[i], 500);
                *sounds[i] = -1;
            }
        }
        return;
    }
    if (special_sound_id != -1)
    {
        audiolib_play_stop(special_sound_id);
        special_sound_id = -1;
    }
}

// 0x00BD9820 (:1011): true without a callback.
bool weapon::do_callback(weapon_callback_idx cb_idx) const
{
    if (cb[cb_idx].func == nullptr)
    {
        return true;
    }
    return cb[cb_idx].func(cb_idx, const_cast<weapon*>(this), cb[cb_idx].user_handle);
}

// 0x00BD9850 (:1028)
bool weapon::has_callback(weapon_callback_idx cb_idx) const
{
    return cb[cb_idx].func != nullptr;
}

// 0x00BD9870 (:1039)
void weapon::set_callback(weapon_callback_idx cb_idx, bool (*func)(weapon_callback_idx, weapon*, unsigned int), unsigned int user_handle)
{
    cb[cb_idx].func = func;
    cb[cb_idx].user_handle = user_handle;
}

// 0x00BD9890 (:1051)
bool weapon::get_callback(weapon_callback_idx cb_idx, bool (**funcptr)(weapon_callback_idx, weapon*, unsigned int), unsigned int* handleptr)
{
    if (funcptr != nullptr)
    {
        *funcptr = cb[cb_idx].func;
    }
    if (handleptr != nullptr)
    {
        *handleptr = cb[cb_idx].user_handle;
    }
    return cb[cb_idx].func != nullptr;
}

// 0x00BD98D0 (:1072)
void weapon::clear_callback(weapon_callback_idx cb_idx)
{
    cb[cb_idx].func = nullptr;
    cb[cb_idx].user_handle = 0xFFFFFFFF;
}

// 0x00BD98F0 (:1083)
// Non-inventory weapons count as owned only while they have an owner and have not been fired.
bool weapon::is_owned() const
{
    if (w_info != nullptr && w_info->flags.non_inventory)
    {
        return owner_handle != 0xFFFFFFFF && !last_fired_timer.valid();
    }
    return item::is_owned();
}

// 0x00BDCF60 (:5043)
// Unlimited during the demolitions master activity's start and shutdown, 999 in wrecking crew; a player's weapon:
// the upgraded maximum (single player, with the extra ammo upgrade) scaled by the backpack's ammo factor.
int weapon::get_max_rounds() const
{
    const base_activity_states state = Demolitions_master_activity.get_activity_state(); // inlined
    if (activity_get_mode() == ACTIVITY_MODE_NONE && (state == BASE_ACTIVITY_STATE_STARTING || state == BASE_ACTIVITY_STATE_SHUTDOWN))
    {
        return 0x7FFFFFFF;
    }
    if (wrecking_crew_is_active())
    {
        // Magic number: 999 rounds in wrecking crew.
        return 999;
    }
    player* pp = World.object_from_handle<player>(owner_handle);
    int max_rounds = w_info->max_rounds;
    if (pp != nullptr)
    {
        if (!Multiplayer && w_info->max_rounds_upgrade != 0 && upgrade_weapon_has_extra_ammo(w_info))
        {
            max_rounds = w_info->max_rounds_upgrade;
        }
        if (pp->backpack != nullptr)
        {
            max_rounds = (int)(pp->backpack->get_backpack_abilities()->ammo_factor * (float)max_rounds);
        }
    }
    return max_rounds;
}

// 0x00BDD030 (:5093)
// The item's render update; a grinder also updates its effects' heat.
void weapon::update_render(rl_renderer* pRenderer)
{
    item::update_render(pRenderer);
    if (w_info->weapon_class == WEAPON_CLASS_GRINDER)
    {
        grinder_update_render();
    }
}

// 0x00BE2980 (:4706)
// A random persona of the weapon's info, unless another one is used less by the other weapons of the same info (twice
// for those of npcs in the squad).
void weapon::set_new_weapon_persona(unsigned int squad_handle)
{
    if (w_info->num_weapon_personas == 0)
    {
        weapon_persona = -1;
        return;
    }
    // Magic number: 16 personas at most.
    int uses[16] = {};
    const int num = World.get_num_objects<weapon>();
    for (int i = 0; i < num; i++)
    {
        const weapon* wp = World.get_object<weapon>(i);
        if (wp->w_info != w_info || wp == this || wp->weapon_persona == -1)
        {
            continue;
        }
        const int persona = wp->weapon_persona;
        uses[persona]++;
        if (squad_handle != 0xFFFFFFFF && wp->owner_handle != 0xFFFFFFFF)
        {
            const npc* np = World.object_from_handle<npc>(wp->owner_handle);
            if (np != nullptr && np->get_ai() != nullptr && np->get_ai()->get_squad_handle() == squad_handle)
            {
                uses[persona]++;
            }
        }
    }
    int pick = int_rand_range(0, w_info->num_weapon_personas - 1);
    for (int i = 0; i < w_info->num_weapon_personas; i++)
    {
        if (uses[i] < uses[pick])
        {
            pick = i;
        }
    }
    weapon_persona = pick;
}

// 0x00BE2E50 (:5074)
void weapon::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_infop)
{
    explode();
}

// 0x00BE2E60 (:5079): the item's hit; the sledgehammer blows the weapon up.
bool weapon::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_infop)
{
    item::process_melee_hit(attacker, damage, impulse, hit_pos, hit_normal, shape_key, w_infop);
    if (w_infop.weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
    {
        explode();
        return true;
    }
    return false;
}

// 0x00BDA080 (:5102): the info's sounds (the fire sound twice).
void weapon::precache_audio_cues()
{
    if (w_info == nullptr)
    {
        return;
    }
    if (w_info->fire_sound != -1)
    {
        audiolib_cue_precache(w_info->fire_sound);
        audiolib_cue_precache(w_info->fire_sound);
    }
    if (w_info->secondary_sound != -1)
    {
        audiolib_cue_precache(w_info->secondary_sound);
    }
    if (w_info->reload_sound != -1)
    {
        audiolib_cue_precache(w_info->reload_sound);
    }
    if (w_info->no_ammo_sound != -1)
    {
        audiolib_cue_precache(w_info->no_ammo_sound);
    }
    if (w_info->special_sound != -1)
    {
        audiolib_cue_precache(w_info->special_sound);
    }
    if (w_info->upgrade_sound != -1)
    {
        audiolib_cue_precache(w_info->upgrade_sound);
    }
}

// 0x00BDA180 (:6006): the weapon's material effects (and the clip's) go. Returns whether one was playing.
bool weapon_stop_material_effect(weapon* wp)
{
    bool stopped = false;
    if (wp != nullptr)
    {
        for (int i = 0; i < 4; i++)
        {
            if (wp->mat_fx_handle[i] != 0xFFFFFFFF)
            {
                material_effects_remove(wp->mat_fx_handle[i]);
                wp->mat_fx_handle[i] = 0xFFFFFFFF;
                stopped = true;
            }
        }
        if (wp->mat_fx_handle_for_clip != 0xFFFFFFFF)
        {
            material_effects_remove(wp->mat_fx_handle_for_clip);
            wp->mat_fx_handle_for_clip = 0xFFFFFFFF;
            stopped = true;
        }
    }
    // Original: written through wp even when it is null.
    wp->current_mat_fx = 0xFFFFFFFF;
    return stopped;
}

// 0x00BE0F90
weapon::~weapon()
{
}

// 0x006FB890 (.h:740): the item's properties and the weapon info.
obj_weapon_props::obj_weapon_props(const weapon_info* w_info, const item_info* info, const vector& pos, const matrix& orient, obj_item_flags& flags, unsigned int collision_type,
                                   float mass_override)
    : obj_item_props(info, pos, orient, flags, collision_type, mass_override, INVALID_SALVAGE_MATERIAL)
{
    set<const weapon_info*>(PROP_WEAPON_INFO, w_info);
}

// 0x0098E5D0 (.h:729): the item's properties by name and the weapon info by name.
obj_weapon_props::obj_weapon_props(char* w_info_name, char* item_info_name, const vector& pos, const matrix& orient, obj_item_flags& flags, unsigned int collision_type, float mass_override)
    : obj_item_props(item_info_name, pos, orient, flags, collision_type, mass_override, INVALID_SALVAGE_MATERIAL)
{
    set(PROP_WEAPON_INFO_NAME, w_info_name);
}

// 0x006F6AA0: the property block's own destructor releases the data.
obj_weapon_props::~obj_weapon_props()
{
}

// 0x00BDBC10 (:1133)
// Turret weapons (unless the turret's ammo is limited: a player's primary weapon counts), multiplayer and wrecking
// crew rules, npcs (except a hostage activity's npcs in action state 3 throwing), backpack abilities, the repair tool
// and melee weapons, and the cheat for the player's weapons.
bool weapon::has_unlimited_magazine_ammo() const
{
    player* pp = World.object_from_handle<player>(owner_handle);
    turret* tp = World.object_from_handle<turret>(turret_handle);
    if (tp != nullptr)
    {
        if (tp->get_weapon_inst() == this)
        {
            if (!tp->limited_ammo_primary() || pp == nullptr) // inlined
            {
                return true;
            }
        }
        else if (tp->get_secondary_weapon_inst() == this && !tp->limited_ammo_secondary()) // inlined
        {
            return true;
        }
    }
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        human* hp = World.object_from_handle<human>(owner_handle);
        // Magic number: action state 3 (a hostage activity's npc).
        if (hp != nullptr &&
            ((w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN && hp->action_node_data.existing_action_state == 3 && !object_is_player(owner_handle)) || hp->hflags.activity_ha_hostage))
        {
            return true;
        }
    }
    if (pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->unlimited_ammo)
    {
        return true;
    }
    if ((Multiplayer && multi_game_player_has_unlimited_magazine_ammo(pp)) || w_info->weapon_class == WEAPON_CLASS_REPAIR || w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        return true;
    }
    if (wrecking_crew_is_active())
    {
        return wrecking_crew_has_unlimited_magazine_ammo();
    }
    return Unlimited_magazine_ammo && object_is_player(owner_handle);
}

// 0x00BD9EC0 (:3963): an equipped weapon is not deleted.
void weapon::equip()
{
    deletion_timer.invalidate();
}

// 0x00BD9ED0 (:3969)
void weapon::equip_non_inventory_item(const human* hp)
{
    set_owner(hp->get_handle(), false);
    last_fired_timer.invalidate();
}

// 0x00BDA040 (:4697): not within twice the out-of-ammo reload delay after firing.
bool weapon::can_unequip() const
{
    if (last_fired_timer.valid() && last_fired_timer.time_since() < w_info->out_of_ammo_reload_delay * 2)
    {
        return false;
    }
    return true;
}

// 0x00BDCEE0 (:5029): a player's magazines scale with the backpack's ammo factor.
int weapon::get_magazine_size() const
{
    const int magazine_size = w_info->magazine_size;
    player* pp = World.object_from_handle<player>(owner_handle);
    if (pp != nullptr && pp->backpack != nullptr)
    {
        return (int)(pp->backpack->get_backpack_abilities()->ammo_factor * (float)magazine_size);
    }
    return magazine_size;
}

// 0x00BDA1F0 (:6061)
void weapon_stop_pickup_effect(weapon* wp)
{
    if (wp->is_playing_pickup_effect()) // inlined
    {
        weapon_stop_material_effect(wp);
        wp->set_playing_pickup_effect(false); // inlined
    }
}

// ---- the weapon object's frame, ammo, reload and trigger state (player_minimal.md P5; firing is not ported)

// ---- file statics compiled since P5
static bool Use_weapon_material_effects = true; // 0x016EBAB9
static bool Use_dropped_weapon_material_effects = true; // 0x016EBABA
static float Shiny_weapon_radius = 12.0f; // 0x016EBC58, the distance from the local player that makes a dropped weapon shine
static bool Weapon_spread_recoil = true; // 0x016EBC78
static bool Unlimited_ammo; // 0x03882CB6
static float Autoaim_radius; // 0x03882CC0, from tweak_table.xtbl
static tweak_table_item Tweak_table_item_Autoaim_radius("Autoaim_radius", &Autoaim_radius); // 0x038B2ED0
// Registered in rfg.exe, read by nothing there.
static float Pseudoseek_search_for_human_target_strength; // 0x03882CA0
static tweak_table_item Tweak_table_item_Pseudoseek_search_for_human_target_strength("Pseudoseek_search_for_human_target_strength", &Pseudoseek_search_for_human_target_strength); // 0x03882CE4
static float Pseudoseek_cone_distance; // 0x03882CA4
static tweak_table_item Tweak_table_item_Pseudoseek_cone_distance("Pseudoseek_cone_distance", &Pseudoseek_cone_distance); // 0x038B2F08
static float Pseudoseek_cone_max_dot_prod; // 0x03882C98
static tweak_table_item Tweak_table_item_Pseudoseek_cone_max_dot_prod("Pseudoseek_cone_max_dot_prod", &Pseudoseek_cone_max_dot_prod); // 0x03882CF0
static float Pseudoseek_cone_offset; // 0x03882CA8
static tweak_table_item Tweak_table_item_Pseudoseek_cone_offset("Pseudoseek_cone_offset", &Pseudoseek_cone_offset); // 0x038B2F3C
static checksum_stri EDF_PISTOL_NAME_CRC("edf_pistol", 0); // 0x03882D00, a dynamic initializer

// 0x00BDB930 (:611): a weapon's material effect follows its clip when the clip changes (the reload swaps it).
void do_frame_weapon_material_effect_post(weapon* wp)
{
    if (wp == nullptr || wp->current_mat_fx == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int clip_handle = wp->get_clip_handle();
    if (clip_handle == wp->mat_fx_clip_handle)
    {
        return;
    }
    if (wp->mat_fx_handle_for_clip != 0xFFFFFFFF)
    {
        material_effects_remove(wp->mat_fx_handle_for_clip);
        wp->mat_fx_handle_for_clip = 0xFFFFFFFF;
    }
    wp->mat_fx_clip_handle = clip_handle;
    if (clip_handle == 0xFFFFFFFF)
    {
        return;
    }
    item* clip = (item*)World.object_from_handle(clip_handle);
    if (clip == nullptr || clip->get_obj_type() != OT_ITEM)
    {
        return;
    }
    item::resource_dependent_data* rdd = clip->rdd.is_loaded() ? clip->rdd.get_data() : nullptr;
    material_effect_parameters params = material_effects_get_parameters(0, -1);
    // A weapon in stealth does not run the effect on its time. The test is a relative epsilon comparison with 0:
    // stealth above 0.0001 times max(1, |stealth|).
    const float stealth = wp->m_stealth_percent;
    float scale = fabsf(stealth);
    if ((float)((double)scale - 0.0) < 0.0f)
    {
        scale = 0.0f;
    }
    if ((float)(1.0 - (double)scale) >= 0.0f)
    {
        scale = 1.0f;
    }
    // Magic number: 0.0001, the epsilon.
    if ((float)((double)scale * 0.0001 + 0.0) < stealth)
    {
        params.use_time_in_w = false;
    }
    if (rdd != nullptr && rdd->smesh_instance_p != nullptr)
    {
        wp->mat_fx_handle_for_clip = material_effects_apply((rl_renderable_instance*)rdd->smesh_instance_p, (int)wp->current_mat_fx, &params);
    }
    character_instance* clip_char = clip->char_inst;
    if (clip_char != nullptr && clip_char->rdd.is_loaded() && clip_char->rdd.get_data()->renderable_instance_p != nullptr)
    {
        wp->mat_fx_handle_for_clip = material_effects_apply((rl_renderable_instance*)clip_char->rdd.get_data()->renderable_instance_p, (int)wp->current_mat_fx, &params);
    }
}

// 0x00BD9930 (:1094)
bool weapon::uses_ammo() const
{
    if (w_info->weapon_class == WEAPON_CLASS_REPAIR)
    {
        return false;
    }
    return w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE;
}

// 0x00BDBB90 (:1108): the rules of the game mode, wrecking crew, the unlimited ammo cheat and the debug switch.
bool weapon::has_unlimited_ammo() const
{
    if (Multiplayer)
    {
        return multi_game_player_has_unlimited_ammo(World.object_from_handle<player>(owner_handle));
    }
    if (w_info->weapon_class == WEAPON_CLASS_REPAIR || w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        return true;
    }
    if (wrecking_crew_is_active())
    {
        return wrecking_crew_has_unlimited_reserve_ammo();
    }
    return stats_cheat_enabled(CHEATS_UNLIMITED_AMMO) || Unlimited_ammo;
}

// 0x00BE6D20 (:1209)
void weapon::fill_ammo()
{
    add_ammo(get_max_rounds());
}

// 0x00BE56B0 (:1217)
// Adds rounds to the reserve (up to the maximum less a magazine) and the rest to the magazine. An empty thermobaric
// launcher that gets ammo gets its rocket prop back. Returns the rounds added.
int weapon::add_ammo(int rounds)
{
    if (rounds_in_magazine == 0 && rounds > 0 && w_info != nullptr && v_stricmp(w_info->name, "thermobaric_rockets") == 0)
    {
        for (int i = 0; i < info->num_props; i++)
        {
            if (info->props[i].type != PROP_TYPE_CLIP)
            {
                continue;
            }
            const item_info* prop_info = object_item_get_info(info->props[i].prop_name);
            // Magic numbers: the prop item's flags: bits 1, 2, 13 and 14 set, bits 0, 3-12 and 15-17 clear. The original
            // leaves the higher bits of its stack variable as they were; here they are clear.
            obj_item_flags proj_item_flags;
            *(unsigned int*)&proj_item_flags = 0x6006;
            obj_item_props props(prop_info, pos, orient, proj_item_flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
            object* prop = World.create_object<item>(props, 0xFFFFFFFF);
            if (prop != nullptr)
            {
                ((item*)prop)->m_prop_type = PROP_TYPE_CLIP;
                const int tag = get_tag_index_from_name("therm_rocket_projectile_bone");
                set_child(prop, tag, -1, false, true);
            }
        }
    }
    const int reserve = rounds_in_reserve;
    const int to_reserve = rounds <= get_max_rounds() - reserve - get_magazine_size() ? rounds : get_max_rounds() - reserve - get_magazine_size();
    rounds_in_reserve += to_reserve;
    const int max_reserve = get_max_rounds() - get_magazine_size();
    if (rounds_in_reserve < 0)
    {
        rounds_in_reserve = 0;
    }
    else if (max_reserve < rounds_in_reserve)
    {
        rounds_in_reserve = max_reserve;
    }
    int to_magazine = get_magazine_size() - rounds_in_magazine;
    if (rounds - to_reserve <= to_magazine)
    {
        to_magazine = rounds - to_reserve;
    }
    if (to_magazine > 0)
    {
        rounds_in_magazine += to_magazine;
        const int magazine_size = get_magazine_size();
        if (rounds_in_magazine < 0)
        {
            rounds_in_magazine = 0;
        }
        else if (magazine_size < rounds_in_magazine)
        {
            rounds_in_magazine = magazine_size;
        }
    }
    return to_magazine + to_reserve;
}

// 0x00BE2670 (:2700): the ready callback agrees and the refire delay has passed.
bool weapon::ready_to_fire() const
{
    if (cb[WEAPON_CB_IDX_READY_TO_FIRE].func != nullptr && !cb[WEAPON_CB_IDX_READY_TO_FIRE].func(WEAPON_CB_IDX_READY_TO_FIRE, const_cast<weapon*>(this), cb[WEAPON_CB_IDX_READY_TO_FIRE].user_handle))
    {
        return false;
    }
    return get_weapon_info_refire_delay() <= refire_delay_time;
}

// 0x00BD9BC0 (:2716): while the one shot delay runs, the shot is delayed.
bool weapon::do_one_shot_delay()
{
    if (!one_shot_delay_timestamp.elapsed())
    {
        flags.delay_fire = true;
        return true;
    }
    return false;
}

// 0x00BDC910 (:2814): the weapon's own reload animation, in multiplayer at the speed of the owner's reload.
void weapon::start_reload(int delay_time)
{
    reload_delay_timer.set(delay_time);
    if (char_inst == nullptr)
    {
        return;
    }
    float speed_multiplier = 1.0f;
    if (Multiplayer == true && owner_handle != 0xFFFFFFFF)
    {
        object* owner = World.object_from_handle(owner_handle);
        if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            const int length_ms = (int)((double)vanim_get_anim_length(((human*)owner)->char_inst->vp, ANIM_ALIAS_RELOAD) * 1000.0);
            speed_multiplier = (float)length_ms / (float)delay_time;
        }
    }
    // Magic number: 0x20, the action flag of the reload.
    char_inst->play_action(ANIMATION_ACTION_RELOAD, ANIM_ALIAS_NONE, speed_multiplier, 0x20);
}

// 0x00BE26D0 (:2839)
bool weapon::can_fire() const
{
    if (flags.trigger_down)
    {
        return false;
    }
    if (cb[WEAPON_CB_IDX_CAN_FIRE].func != nullptr && !cb[WEAPON_CB_IDX_CAN_FIRE].func(WEAPON_CB_IDX_CAN_FIRE, const_cast<weapon*>(this), cb[WEAPON_CB_IDX_CAN_FIRE].user_handle))
    {
        return false;
    }
    if (w_info->trigger_type != WEAPON_TRIGGER_TYPE_AUTOMATIC && !ready_to_fire())
    {
        return false;
    }
    if (reload_delay_timer.valid() || overheated)
    {
        return false;
    }
    return has_ammo();
}

// 0x00BDC9E0 (:2897): a player's refire delay (scaled by the backpack), an npc's, or the default without an owner.
float weapon::get_weapon_info_refire_delay() const
{
    if (owner_handle != 0xFFFFFFFF)
    {
        object* owner = World.object_from_handle(owner_handle);
        if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            const multi_object_backpack* backpack = ((player*)owner)->backpack;
            if (backpack != nullptr)
            {
                // The x87 product is rounded to float.
                return (float)((double)w_info->default_refire_delay * (double)backpack->get_backpack_abilities()->refire_delay_factor);
            }
            return w_info->default_refire_delay;
        }
    }
    return w_info->npc_refire_delay;
}

// 0x00BE2740 (:2871): clamped to 0..1; reaching 1 overheats the weapon and starts its overheated effect at the muzzle.
// Returns whether the weapon is overheated.
bool weapon::set_overheat_percent(float new_percent)
{
    if (new_percent - 0.0f < 0.0f)
    {
        new_percent = 0.0f;
    }
    if (0.0f <= new_percent - 1.0f)
    {
        new_percent = 1.0f;
    }
    overheat_percent = new_percent;
    if (new_percent == 1.0f && !overheated)
    {
        overheated = true;
        if (w_info->overheated_effect != 0xFFFFFFFF)
        {
            vector muzzle_pos;
            matrix muzzle_orient;
            get_muzzle_transform(muzzle_pos, &muzzle_orient);
            overheated_effect_handle = effects_play(w_info->overheated_effect, muzzle_pos, muzzle_orient, true, get_handle(), muzzle_tag_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            if (overheated_effect_handle != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(overheated_effect_handle);
                if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
                {
                    ((object_effect*)objp)->set_expression_variable(EFFECTS_VAR_WEAPON_HEAT_PERCENT, 1.0f);
                }
            }
        }
    }
    return overheated;
}

// 0x00BD9BF0 (:2914)
void weapon::cancel_reload()
{
    reload_delay_timer.invalidate();
}

// 0x00BD9C00 (:2947)
// Milliseconds since the last shot (0 while an automatic weapon's trigger is held), -1 before the first.
int weapon::time_since_fired() const
{
    if (!last_fired_timer.valid())
    {
        return -1;
    }
    if (w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC && flags.trigger_down)
    {
        return 0;
    }
    return last_fired_timer.time_since();
}

// 0x00BE2850 (:2919)
// The magazine is refilled from the reserve; npcs (and weapons with unlimited ammo) always have two magazines in
// reserve. An EDF pistol's last reserve shows the ammo tool tip.
void weapon::do_reload()
{
    if (has_unlimited_ammo() || !object_is_player(owner_handle))
    {
        const int two_magazines = get_magazine_size() * 2;
        rounds_in_reserve = two_magazines < rounds_in_reserve ? rounds_in_reserve : two_magazines;
    }
    rounds_in_reserve += rounds_in_magazine;
    int load = rounds_in_reserve;
    const int magazine_size = get_magazine_size();
    if (magazine_size < load)
    {
        load = magazine_size;
    }
    rounds_in_reserve -= load;
    rounds_in_magazine = load;
    if (!Multiplayer && rounds_in_reserve == 0 && w_info != nullptr && w_info->name_crc == EDF_PISTOL_NAME_CRC)
    {
        ui_tool_tips_add_tip("EDF_Ammo", 0, nullptr);
    }
    reload_delay_timer.invalidate();
}

// 0x00BDCA70 (:2974): the muzzle tag of a loaded firearm, else the weapon's position (melee weapons have none).
void weapon::get_muzzle_transform(vector& muzzle_pos, matrix* muzzle_orient) const
{
    if (!w_info->is_firearm())
    {
        return;
    }
    if (rdd.is_loaded())
    {
        get_tag_world_coords(muzzle_tag_index, muzzle_pos, muzzle_orient, nullptr, nullptr);
        return;
    }
    muzzle_pos = pos;
    if (muzzle_orient != nullptr)
    {
        *muzzle_orient = orient;
    }
}

// 0x00BDCB10 (:3122)
void weapon::get_spread(float& min_spread, float& max_spread) const
{
    if (owner_handle != 0xFFFFFFFF)
    {
        object* owner = World.object_from_handle(owner_handle);
        if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
        {
            human_weapons_get_spread(min_spread, max_spread, (human*)owner, w_info);
            return;
        }
    }
    min_spread = w_info->min_spread;
    max_spread = w_info->max_spread;
}

// 0x00BDCC80 (:3977): the muzzle smoke stops.
void weapon::unequip()
{
    muzzle_smoke_timestamp.invalidate();
    if (muzzle_smoke_effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* effect = World.object_from_handle(muzzle_smoke_effect_handle);
    if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
    {
        ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
        muzzle_smoke_effect_handle = 0xFFFFFFFF;
    }
}

// 0x00BDCCD0 (:3989): the muzzle smoke's speed follows the muzzle. Static in rfg.exe; external here for its friend
// declaration in object_effect.h.
void muzzle_smoke_update_effect_cb(object_effect* effect_p, void* data_p)
{
    const unsigned int handle = (unsigned int)data_p;
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON || !effect_p->rdd.is_loaded() ||
        effect_p->rdd.get_data()->vfx_effect_p == nullptr)
    {
        return;
    }
    vector muzzle;
    objp->get_tag_world_coords(((weapon*)objp)->get_muzzle_tag_index(), muzzle, nullptr, nullptr, nullptr);
    muzzle.x = muzzle.x - effect_p->pos.x;
    muzzle.y = muzzle.y - effect_p->pos.y;
    muzzle.z = muzzle.z - effect_p->pos.z;
    effect_p->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, sqrtf(muzzle.x * muzzle.x + muzzle.y * muzzle.y + muzzle.z * muzzle.z) / Frametime);
}

// 0x00BD9C50 (:2961)
weapon_range_categories weapon::get_range_category(float distance) const
{
    if (distance < w_info->min_engagement_dist)
    {
        return WEAPON_RANGE_CLOSE;
    }
    if (distance <= w_info->max_engagement_dist)
    {
        return WEAPON_RANGE_OPTIMAL;
    }
    return w_info->max_range < distance ? WEAPON_OUT_OF_RANGE : WEAPON_RANGE_LONG;
}

// 0x00BD9F00 (:4429): the press and release callbacks; pulling starts the prefire delay.
void weapon::set_trigger_pulled(bool pulled)
{
    if (pulled)
    {
        if (!flags.trigger_down)
        {
            if (cb[WEAPON_CB_IDX_PRESS_TRIGGER].func != nullptr)
            {
                cb[WEAPON_CB_IDX_PRESS_TRIGGER].func(WEAPON_CB_IDX_PRESS_TRIGGER, this, cb[WEAPON_CB_IDX_PRESS_TRIGGER].user_handle);
            }
            refire_delay_time = refire_delay_time - w_info->prefire_delay;
        }
        flags.trigger_down = true;
        return;
    }
    if (flags.trigger_down && cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func != nullptr)
    {
        cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func(WEAPON_CB_IDX_RELEASE_TRIGGER, this, cb[WEAPON_CB_IDX_RELEASE_TRIGGER].user_handle);
    }
    flags.trigger_down = false;
    num_rounds_fired = 0;
}

// 0x00BD9FA0 (:4486): the attached clip prop.
unsigned int weapon::get_clip_handle()
{
    object* first = child_ptr;
    for (object* child = first; child != nullptr;)
    {
        if (child->get_obj_type() == OT_ITEM && ((item*)child)->m_prop_type == PROP_TYPE_CLIP)
        {
            return child->get_handle();
        }
        child = child->child_next;
        if (child == first)
        {
            return 0xFFFFFFFF;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00BDCDF0 (:4683): not while faded out, nor for the local player looking through a scope.
bool weapon::can_play_muzzle_flash() const
{
    if (render_alpha != 1.0f && !casts_transparent_shadows)
    {
        return false;
    }
    if (owner_handle != 0xFFFFFFFF)
    {
        object* owner = World.object_from_handle(owner_handle);
        if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER && object_is_local_player(owner) && player_is_scoped((player*)owner))
        {
            return false;
        }
    }
    return true;
}

// 0x00BD9FE0 (:4502): not for a faded weapon, unless it casts transparent shadows.
bool weapon::should_play_visual_effects() const
{
    return render_alpha == 1.0f || casts_transparent_shadows;
}

// 0x00BDA010 (:4589): a weapon linked to its operator (null ends it; the owner stays).
void weapon::set_linked(const human* operator_hp)
{
    if (operator_hp == nullptr)
    {
        flags.is_linked = false;
        return;
    }
    flags.is_linked = true;
    owner_handle = operator_hp->get_handle();
}

// 0x00BE2B50 (:4754)
// The looping fire sound (the local player's in 2D unless the camera is locked third person, an npc's from its
// persona) and the muzzle flash; the muzzle smoke stops while firing.
void weapon::start_looping_effects()
{
    if (!w_info->is_firearm())
    {
        return;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    get_muzzle_transform(muzzle_pos, &muzzle_orient);
    if (w_info->weapon_class != WEAPON_CLASS_ARC_WELDER && fire_sound_alr_id == -1)
    {
        int cue_id = w_info->fire_sound;
        const vector* sound_pos = nullptr;
        const vector* sound_orient = nullptr;
        bool play = true;
        if (!object_is_local_player(owner_handle) || cue_id == -1 || Camera_third_person_lock)
        {
            if (weapon_persona == -1 && w_info->num_weapon_personas > 0)
            {
                set_new_weapon_persona(0xFFFFFFFF);
            }
            if (weapon_persona == -1)
            {
                play = false;
            }
            else
            {
                sound_orient = &muzzle_orient.fvec;
                sound_pos = &muzzle_pos;
                cue_id = w_info->npc_fire_sounds[weapon_persona];
            }
        }
        if (play)
        {
            audiolib_att_loop_rel_start(cue_id, &fire_sound_alr_id, false, sound_pos, sound_orient);
        }
    }
    if (!can_play_muzzle_flash() || w_info->muzzle_flash_effect == 0xFFFFFFFF)
    {
        return;
    }
    object* flash = muzzle_flash_effect_handle != 0xFFFFFFFF ? World.object_from_handle(muzzle_flash_effect_handle) : nullptr;
    if (flash == nullptr || flash->get_obj_type() != OT_EFFECT)
    {
        muzzle_flash_effect_handle = effects_play(w_info->muzzle_flash_effect, muzzle_pos, muzzle_orient, true, get_handle(), muzzle_tag_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        if (muzzle_flash_effect_handle != 0xFFFFFFFF)
        {
            World.object_from_handle(muzzle_flash_effect_handle);
        }
    }
    // Magic number: the muzzle smoke starts 200 ms after the last shot.
    muzzle_smoke_timestamp.set(200);
    if (muzzle_smoke_effect_handle != 0xFFFFFFFF)
    {
        object* smoke = World.object_from_handle(muzzle_smoke_effect_handle);
        if (smoke != nullptr && smoke->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)smoke)->stop(object_effect::SS_STOP_EMITTING);
            muzzle_smoke_effect_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00BDCE60 (:4823): the arc welder's sound only stops hard.
void weapon::end_looping_effects(bool hard_stop)
{
    if ((w_info->weapon_class != WEAPON_CLASS_ARC_WELDER || hard_stop) && fire_sound_alr_id != -1)
    {
        // Magic number: a 100 ms fade out.
        audiolib_att_loop_rel_stop(fire_sound_alr_id, 100);
        fire_sound_alr_id = -1;
    }
    if (muzzle_flash_effect_handle != 0xFFFFFFFF)
    {
        object* flash = World.object_from_handle(muzzle_flash_effect_handle);
        if (flash != nullptr && flash->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)flash)->stop((object_effect::stop_styles)hard_stop);
            muzzle_flash_effect_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00BEC530 (:4849)
// The refire timer runs (from minus the prefire delay up to the refire delay), the special weapons process, and the
// trigger state of the frame is remembered.
void weapon::do_frame_pre()
{
    // fl_max / fl_min through the double difference, as in the binary.
    float delay = refire_delay_time;
    const float min_delay = -w_info->prefire_delay;
    if ((float)((double)delay - (double)min_delay) < 0.0f)
    {
        delay = min_delay;
    }
    const float refire_delay = get_weapon_info_refire_delay();
    // The x87 difference is stored as float before the test.
    if ((float)(delay - refire_delay) >= 0.0f)
    {
        delay = refire_delay;
    }
    refire_delay_time = delay;
    refire_delay_time = (float)((double)delay + (double)Frametime);
    switch (w_info->weapon_class)
    {
    case WEAPON_CLASS_ARC_WELDER:
        process_arc_welder();
        break;
    case WEAPON_CLASS_REPAIR:
        process_repair();
        break;
    case WEAPON_CLASS_FLAME:
        process_flame();
        break;
    default:
        break;
    }
    flags.processed_since_last_trigger_pull = flags.trigger_down;
}

// 0x00BE6790 (:4872)
// A finished reload loads the magazine; a thrown weapon takes its next round; the overheat cools down; the spread
// recovers towards the minimum (npcs hostile to the local player scale it by the difficulty); the looping fire sound
// follows the muzzle and the muzzle flash fades; the local player's muzzle smoke starts after firing; the pickup shine
// and the clip's material effect follow.
void weapon::do_frame_post()
{
    if (reload_delay_timer.valid() && reload_delay_timer.elapsed())
    {
        do_reload();
    }
    if (w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN && rounds_in_magazine == 0)
    {
        int load = rounds_in_reserve;
        if (load > 0)
        {
            const int magazine_size = get_magazine_size();
            if (magazine_size < load)
            {
                load = magazine_size;
            }
            rounds_in_reserve -= load;
            rounds_in_magazine = load;
        }
        else if (has_unlimited_ammo())
        {
            rounds_in_magazine = 1;
        }
    }
    float heat = (float)((double)overheat_percent - (double)Frametime / (double)w_info->overheat_cool_down_time);
    overheat_percent = heat;
    if ((float)((double)heat - 0.0) < 0.0f)
    {
        heat = 0.0f;
    }
    if ((float)((double)heat - 1.0) >= 0.0f)
    {
        heat = 1.0f;
    }
    overheat_percent = heat;
    if (overheated)
    {
        object_effect* effect = nullptr;
        if (overheated_effect_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(overheated_effect_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
            {
                effect = (object_effect*)objp;
                effect->set_expression_variable(EFFECTS_VAR_WEAPON_HEAT_PERCENT, overheat_percent);
            }
        }
        if (overheat_percent == 0.0f)
        {
            overheated = false;
            if (effect != nullptr)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            overheated_effect_handle = 0xFFFFFFFF;
        }
    }
    if (!Weapon_spread_recoil)
    {
        current_spread = 0.0f;
    }
    else
    {
        float min_spread;
        float max_spread;
        get_spread(min_spread, max_spread);
        const unsigned short to_min_spread = w_info->to_min_spread;
        if (to_min_spread == 0)
        {
            current_spread = min_spread;
        }
        else
        {
            // The recovery per frame: the spread range over to_min_spread milliseconds, in double.
            const float recovery = (float)((double)Frametime * 1000.0 / (double)to_min_spread * ((double)max_spread - (double)min_spread));
            if ((double)min_spread > (double)current_spread)
            {
                current_spread = min_spread;
            }
            else
            {
                float spread = (float)((double)current_spread - (double)recovery);
                current_spread = spread;
                if ((float)((double)spread - (double)min_spread) < 0.0f)
                {
                    spread = min_spread;
                }
                current_spread = spread;
            }
            if (owner_handle != 0xFFFFFFFF)
            {
                object* owner = World.object_from_handle(owner_handle);
                if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && player_get_local_player() != nullptr && !local_player_is_friendly((human*)owner))
                {
                    // The x87 product is rounded to float.
                    current_spread = (float)((double)difficulty_get_value(DIFFICULTY_ENEMY_WEAPON_SPREAD, NUM_DIFFICULTY_LEVELS) * (double)current_spread);
                }
            }
        }
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    get_muzzle_transform(muzzle_pos, &muzzle_orient);
    int sound_id = fire_sound_alr_id;
    if (sound_id != -1 || (w_info->weapon_class == WEAPON_CLASS_ARC_WELDER && (sound_id = special_sound_id) != -1))
    {
        audiolib_att_loop_rel_set_pos(sound_id, &muzzle_pos, nullptr);
    }
    if (0.0f < muzzle_flash_intensity)
    {
        // Magic number: the muzzle flash fades in 1/20 s.
        float intensity = (float)((double)muzzle_flash_intensity - (double)Frametime * 20.0);
        muzzle_flash_intensity = intensity;
        if ((float)((double)intensity - 0.0) < 0.0f)
        {
            intensity = 0.0f;
        }
        muzzle_flash_intensity = intensity;
    }
    if (object_is_local_player(owner_handle) && (render_alpha == 1.0f || casts_transparent_shadows) && muzzle_smoke_timestamp.valid() && muzzle_smoke_timestamp.elapsed())
    {
        muzzle_smoke_timestamp.invalidate();
        // Magic number: the smoke comes after half of the bursts.
        if (fl_rand() <= 0.5f)
        {
            get_muzzle_transform(muzzle_pos, &muzzle_orient);
            object_effect* old_smoke = World.object_from_handle<object_effect>(muzzle_smoke_effect_handle);
            if (old_smoke != nullptr)
            {
                old_smoke->stop(object_effect::SS_STOP_EMITTING);
            }
            if (w_info->muzzle_smoke_effect != 0xFFFFFFFF)
            {
                muzzle_smoke_effect_handle =
                    effects_play(w_info->muzzle_smoke_effect, muzzle_pos, muzzle_orient, false, get_handle(), muzzle_tag_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                object_effect* smoke = World.object_from_handle<object_effect>(muzzle_smoke_effect_handle);
                if (smoke != nullptr)
                {
                    smoke->set_pre_update_cb(muzzle_smoke_update_effect_cb); // inlined
                    smoke->set_update_cb_data((void*)get_handle()); // inlined
                }
            }
        }
    }
    if (render_alpha != 1.0f && !casts_transparent_shadows && muzzle_flash_effect_handle != 0xFFFFFFFF)
    {
        object* flash = World.object_from_handle(muzzle_flash_effect_handle);
        if (flash != nullptr && flash->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)flash)->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    do_frame_pickup_effect_post();
    do_frame_weapon_material_effect_post(this);
}

// 0x00BE2D20 (:4995): a dropped weapon shines while the living local player is within Shiny_weapon_radius.
void weapon::do_frame_pickup_effect_post()
{
    if (is_owned() || !obj_flags.streamed)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (obj_flags.destroyed || pp == nullptr || pp->is_killed())
    {
        if (flags.playing_pickup_effect)
        {
            weapon_stop_material_effect(this);
            flags.playing_pickup_effect = false;
        }
        return;
    }
    const float dx = pos.x - pp->pos.x;
    const float dy = pos.y - pp->pos.y;
    const float dz = pos.z - pp->pos.z;
    if (Shiny_weapon_radius * Shiny_weapon_radius <= dz * dz + dy * dy + dx * dx)
    {
        if (flags.playing_pickup_effect)
        {
            weapon_stop_pickup_effect(this);
        }
    }
    else if (!flags.playing_pickup_effect)
    {
        weapon_start_pickup_effect(this);
    }
}

// 0x00BDD1B0 (:5178): the player's equip action stops when the weapon being put away hits something, and the weapon
// stays equipped. Magic number: a 6 frame ramp out.
void cancel_unequip_on_collide(weapon* wp)
{
    player* pp = World.object_from_handle<player>(wp->get_owner_handle());
    if (pp == nullptr || pp->get_equipped_weapon() != wp || !pp->hflags.playing_unequip_anim)
    {
        return;
    }
    pp->char_inst->stop_action(ANIM_ALIAS_EQUIP, 6, false);
    inventory_item* last = pp->last_equipped_weapon;
    pp->last_equipped_weapon = pp->desired_equipped_inv_item;
    pp->second_last_equipped_weapon = last;
    pp->desired_equipped_inv_item = pp->get_equipped_inv_item();
}

// 0x00BDA140 (:5589)
void weapons_set_unlimited_ammo(bool unlimited_ammo)
{
    Unlimited_ammo = unlimited_ammo;
}

// 0x00BDD920 (:5596): the player's weapon override or the default, an npc's weapon's own; multiplayer adds the
// rubber banding bonus.
float weapons_get_autoaim_radius(const human* hp)
{
    if (hp == nullptr)
    {
        return Autoaim_radius;
    }
    const player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)hp : nullptr;
    float radius = Autoaim_radius;
    const weapon* wp = hp->get_equipped_weapon();
    if (wp != nullptr)
    {
        if (pp == nullptr)
        {
            radius = wp->w_info->npc_autoaim;
        }
        else if (wp->w_info->autoaim_override != 0.0f)
        {
            radius = wp->w_info->autoaim_override;
        }
    }
    if (Multiplayer && pp != nullptr)
    {
        radius = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_AUTO_AIM) + radius;
    }
    return radius;
}

// 0x00BDA160 (:5633)
int weapons_get_nano_material_effect(bool& invert_render)
{
    invert_render = true;
    return Nano_material_effect;
}

// 0x00BDA170 (:5639)
unsigned int weapons_get_repair_fade_effect()
{
    return Repair_fade_effect;
}

// 0x00BDD9C0 (:5880)
// Applies a material effect to the weapon's renderables: a dropped weapon's mesh, a mine's or charge's (and the
// charge in the owner's other hand), or the character mesh, the mesh and the attached props (the clip separately), up
// to four handles.
void weapon_start_material_effect(weapon* wp, human* hp_owner, unsigned int effect, material_effect_parameters mat_fx_params)
{
    if (wp == nullptr || wp->current_mat_fx == effect || weapon_stop_material_effect(wp) || !Use_weapon_material_effects || effect == 0xFFFFFFFF)
    {
        return;
    }
    unsigned int count = 0;
    wp->current_mat_fx = effect;
    item::resource_dependent_data* rdd = wp->rdd.is_loaded() ? wp->rdd.get_data() : nullptr;
    if (wp->get_owner_handle() == 0xFFFFFFFF && rdd != nullptr && rdd->pickup_smesh_instance_p != nullptr)
    {
        wp->mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)rdd->pickup_smesh_instance_p, (int)effect, &mat_fx_params);
        return;
    }
    const weapon_class_type weapon_class = wp->w_info->weapon_class;
    if (weapon_class == WEAPON_CLASS_THROWN_MINE)
    {
        if (rdd != nullptr && rdd->smesh_instance_p != nullptr)
        {
            wp->mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)rdd->smesh_instance_p, (int)effect, &mat_fx_params);
        }
        return;
    }
    if (weapon_class == WEAPON_CLASS_THROWN_CHARGE)
    {
        if (rdd != nullptr && rdd->smesh_instance_p != nullptr)
        {
            wp->mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)rdd->smesh_instance_p, (int)effect, &mat_fx_params);
            count = 1;
        }
        if (hp_owner == nullptr || !wp->w_info->flags.draw_proj_in_opposite_hand)
        {
            return;
        }
        item* charge = World.object_from_handle<item>(hp_owner->offhand_projectile_handle);
        if (charge == nullptr)
        {
            wp->current_mat_fx = 0xFFFFFFFF;
            return;
        }
        if (charge->rdd.is_loaded() && charge->rdd.get_data()->smesh_instance_p != nullptr)
        {
            if (count > 3)
            {
                return;
            }
            wp->mat_fx_handle[count] = material_effects_apply((rl_renderable_instance*)charge->rdd.get_data()->smesh_instance_p, (int)effect, &mat_fx_params);
            count++;
        }
        character_instance* charge_char = charge->char_inst;
        if (charge_char != nullptr && charge_char->rdd.is_loaded() && charge_char->rdd.get_data()->renderable_instance_p != nullptr && count < 4)
        {
            wp->mat_fx_handle[count] = material_effects_apply((rl_renderable_instance*)charge_char->rdd.get_data()->renderable_instance_p, (int)effect, &mat_fx_params);
        }
        return;
    }
    bool applied = false;
    character_instance* ci = wp->char_inst;
    if (ci != nullptr && ci->rdd.is_loaded() && ci->rdd.get_data()->renderable_instance_p != nullptr)
    {
        wp->mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)ci->rdd.get_data()->renderable_instance_p, (int)effect, &mat_fx_params);
        count = 1;
        applied = true;
    }
    if (rdd == nullptr || rdd->smesh_instance_p == nullptr)
    {
        if (!applied)
        {
            return;
        }
    }
    else
    {
        // Original: the bound here is 15, not the 3 of the handle array (count is at most 1 here).
        if (count > 15)
        {
            return;
        }
        wp->mat_fx_handle[count] = material_effects_apply((rl_renderable_instance*)rdd->smesh_instance_p, (int)effect, &mat_fx_params);
        count++;
    }
    wp->mat_fx_clip_handle = wp->get_clip_handle();
    object* first = wp->child_ptr;
    if (first == nullptr)
    {
        return;
    }
    for (object* child = first; (int)count < 4;)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            item* child_item = (item*)child;
            if (child_item->rdd.is_loaded() && child_item->rdd.get_data()->smesh_instance_p != nullptr)
            {
                const unsigned int handle = material_effects_apply((rl_renderable_instance*)child_item->rdd.get_data()->smesh_instance_p, (int)effect, &mat_fx_params);
                if (child->get_handle() == wp->mat_fx_clip_handle)
                {
                    wp->mat_fx_handle_for_clip = handle;
                }
                else
                {
                    wp->mat_fx_handle[count] = handle;
                    count++;
                }
            }
            character_instance* child_char = child_item->char_inst;
            if (child_char != nullptr && child_char->rdd.is_loaded() && child_char->rdd.get_data()->renderable_instance_p != nullptr)
            {
                if ((int)count > 3)
                {
                    return;
                }
                const unsigned int handle = material_effects_apply((rl_renderable_instance*)child_char->rdd.get_data()->renderable_instance_p, (int)effect, &mat_fx_params);
                if (child->get_handle() == wp->mat_fx_clip_handle)
                {
                    wp->mat_fx_handle_for_clip = handle;
                }
                else
                {
                    wp->mat_fx_handle[count] = handle;
                    count++;
                }
            }
        }
        child = child->child_next;
        if (child == wp->child_ptr || child == nullptr)
        {
            return;
        }
    }
}

// 0x00BDDCE0 (:6028): the shine of a dropped weapon (not the sledgehammer, non-inventory or turret weapons).
void weapon_start_pickup_effect(weapon* wp)
{
    if (wp == nullptr || wp->w_info == nullptr || wp->w_info->flags.non_inventory || wp->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER ||
        !Use_dropped_weapon_material_effects || wp->is_playing_pickup_effect() || wp->turret_handle != 0xFFFFFFFF)
    {
        return;
    }
    weapon_start_material_effect(wp, nullptr, Dropped_weapon_material_effect_id, material_effects_get_parameters(0, -1));
    wp->set_playing_pickup_effect(true);
}

// ---- firing (multiplayer.md MP8): the bullet, the penetrating bullet, projectiles and thrown weapons, the bullet hit
// and its effects, tracers, the repair and flame weapons (their per-frame effects, process_repair and process_flame,
// are weapons.md WP4). The arc welder's firing (fire_electricity) is in arc_welder.cpp.
static const float PI2 = 6.283185f; // 0x0133AC28
static const float PI_OVER_2 = 1.570796f; // 0x0133AC2C
static float Repair_range = 3.0f; // 0x016EBC48
static float Repair_radius = 1.0f; // 0x016EBC4C
static float Flame_range = 4.0f; // 0x016EBC50
static float Flame_radius = 2.5f; // 0x016EBC54
static int Sequential_damage_time = 2000; // 0x016EBC5C
static float Sequential_damage_factor = 0.4f; // 0x016EBC60
float Player_bullet_flight_sound_radius = 4.0f; // 0x016EBC64 (placed in player.cpp by the stub list; read only here)
static float Npc_bullet_flight_sound_radius = 4.0f; // 0x016EBC68
static bool Tracers_enabled = true; // 0x016EBC79
static bool Fire_bullets_from_camera = true; // 0x016EBC7A
// No initial value: only tweak_table.xtbl sets it (without an entry the bullet flyby sound of fire_bullet never plays).
static float Bullet_flyby_sound_dist; // 0x03882C90
// The firing tweaks; tweak_table.xtbl overrides the values above.
static tweak_table_item Tweak_table_item_Repair_range("Repair_range", &Repair_range); // 0x038B2E50
static tweak_table_item Tweak_table_item_Repair_radius("Repair_radius", &Repair_radius); // 0x038B2EF0
static tweak_table_item Tweak_table_item_Bullet_flyby_sound_dist("Bullet_flyby_sound_dist", &Bullet_flyby_sound_dist); // 0x038B2F14
static tweak_table_item Tweak_table_item_Player_bullet_flight_sound_radius("Player_bullet_flight_sound_radius", &Player_bullet_flight_sound_radius); // 0x038B2F30
static tweak_table_item Tweak_table_item_Npc_bullet_flight_sound_radius("Npc_bullet_flight_sound_radius", &Npc_bullet_flight_sound_radius); // 0x038B2EFC
static tweak_table_item Tweak_table_item_Sequential_damage_time("Sequential_damage_time", &Sequential_damage_time); // 0x03882D04
static tweak_table_item Tweak_table_item_Sequential_damage_factor("Sequential_damage_factor", &Sequential_damage_factor); // 0x03882CD8
static bool Unlimited_ai_thrown_weapons; // 0x03882CB7
static bool Autoaim_debug; // 0x03882CBE
static bool Arc_debug_effect; // 0x03882CBF, debug: sample the arc spline (its debug draw is stripped from retail)
// The cosine of 5 degrees: a forced aim direction is used when it is within 5 degrees of the muzzle's.
static const float AIM_OVERRIDE_DOT = (float)cos((double)0.0872665f); // 0x03882CD4 (dynamic initializer 0x0108EA50)
static int planting_sound = -1; // 0x016EBD68 (function static of weapon_projectile_throw_collide)
static int singularity_planting_sound = -1; // 0x016EBD6C (function static of weapon_projectile_throw_collide)

// 0x00BE0EA0 (:162): a bullet's ray collects the humans it passes (bullets go through them; the hit is decided by the
// human hit locations afterwards).
inline bullet_callback_info::bullet_callback_info(const object* attacker_)
    : attacker(attacker_)
    , attacker_hp(attacker_ != nullptr && attacker_->get_obj_type() == OT_HUMAN ? (const human*)attacker_ : nullptr)
{
}

// 0x00BE2970
inline bullet_callback_info::~bullet_callback_info()
{
}

// 0x00BDB830 (:397): an npc hears the bullet; the local player hears a flyby at most every 250 ms.
void propagate_bullet_whizz_sound(const human* attacker, human* target, const vector& sound_loc, const weapon_info* w_info)
{
    if (target != nullptr && target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ((npc*)target)->get_ai()->perception.bullet_sound_heard(attacker, w_info, sound_loc);
        return;
    }
    if (!object_is_local_player(target))
    {
        return;
    }
    static timestamp player_bullet_whizz_timestamp; // 0x038B2F50
    static int cue_id; // 0x038B2F48
    static bool cue_inited; // 0x038B2F4C
    if (!cue_inited)
    {
        audiolib_cue_get_id("WEP_ROUND_FLYBY", &cue_id);
        cue_inited = true;
    }
    if (player_bullet_whizz_timestamp.valid() && !player_bullet_whizz_timestamp.elapsed())
    {
        return;
    }
    player_bullet_whizz_timestamp.set(250);
    if (audiolib_cue_is_id_valid(cue_id))
    {
        vector look;
        human_get_look_at_vector(attacker, look);
        audiolib_cue_play_3d(cue_id, &sound_loc, &look, nullptr, nullptr);
    }
}

// 0x00BE0FB0 (:440, LTCG: end_pos in EAX, the rest on the stack)
// Every living human the bullet passes close to (more than 3 m from both ends; players within
// Player_bullet_flight_sound_radius, npcs within Npc_bullet_flight_sound_radius of the head) hears it.
static void propagate_bullet_flight_sounds(const vector& start_pos, const vector& end_pos, const weapon_fire_info& fire_info, const object* attacker)
{
    static vector APROX_HEAD_POS_OFFSET(0.0f, 1.2f, 0.0f); // 0x038B2F58
    const float player_radius_sq = Player_bullet_flight_sound_radius * Player_bullet_flight_sound_radius;
    const float npc_radius_sq = Npc_bullet_flight_sound_radius * Npc_bullet_flight_sound_radius;
    const human* attacker_hp = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN ? (const human*)attacker : nullptr;
    vector dir = end_pos - start_pos;
    const float length = dir.mag_and_normalize_safe();
    if (length <= 3.0f)
    {
        return;
    }
    const float half = (length - 3.0f) * 0.5f;
    const vector middle(start_pos.x + dir.x * half, dir.y * half + start_pos.y, start_pos.z + dir.z * half);
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* target = World.get_object<human>(i);
        if (target == attacker || target->is_killed())
        {
            continue;
        }
        const vector head = target->pos + APROX_HEAD_POS_OFFSET;
        if (vec_dist_squared(middle, head) >= half * half)
        {
            continue;
        }
        const float along = (head.y - start_pos.y) * dir.y + (head.x - start_pos.x) * dir.x + (head.z - start_pos.z) * dir.z;
        if (along < 0.0f || along > length - 3.0f)
        {
            continue;
        }
        const vector closest(start_pos.x + dir.x * along, start_pos.y + dir.y * along, start_pos.z + dir.z * along);
        const float dist_sq = vec_dist_squared(head, closest);
        if (object_is_player(target) ? dist_sq < player_radius_sq : dist_sq < npc_radius_sq)
        {
            propagate_bullet_whizz_sound(attacker_hp, target, closest, fire_info.w_info);
        }
    }
}

// 0x00BE4F40 (:549)
// The bullet ray's object filter: the attacker, humans the attacker may not hit, and (wrecking crew) the local player's
// vehicles are skipped. Humans are collected instead of hit (not a ragdoll that may not be shown, nor one inside the
// attacker's vehicle); with the shatter flag only destroyable movers that can shatter are hit.
static bool process_bullet_callback(object* hit_objp, void* info)
{
    bullet_callback_info* cb_info = (bullet_callback_info*)info;
    if (hit_objp == cb_info->attacker)
    {
        return false;
    }
    if (cb_info->attacker_hp != nullptr && human_combat_hit_human(hit_objp, cb_info->attacker_hp))
    {
        return false;
    }
    if (wrecking_crew_is_active() && cb_info->attacker == player_get_local_player() && hit_objp->get_sub_type() == OT_SUB_HUMAN_PLAYER &&
        !hit_objp->obj_flags._dont_use_me)
    {
        return false;
    }
    human* hp = hit_objp != nullptr && hit_objp->get_obj_type() == OT_HUMAN ? (human*)hit_objp : nullptr;
    if (hp == nullptr)
    {
        // Ray flag 2 (every bullet sets it): general movers with gm_flags bit 0x80 stop the bullet's filter here.
        if ((havok_game_filter::m_current_ray_flags & 2) == 0 || hit_objp == nullptr || hit_objp->get_obj_type() != OT_MOVER ||
            hit_objp->get_sub_type() != OT_SUB_MOVER_GENERAL || (((general_mover*)hit_objp)->gm_flags & 0x80) == 0)
        {
            return true;
        }
        return false;
    }
    if (hp->ragdoll_idx == -1 || localization_allow_ragdolls() || !human_is_dead(hp))
    {
        bool in_attacker_vehicle = false;
        if (cb_info->attacker_hp != nullptr && cb_info->attacker_hp->vehicle_handle != 0xFFFFFFFF)
        {
            vehicle* vp = (vehicle*)World.object_from_handle(cb_info->attacker_hp->vehicle_handle);
            in_attacker_vehicle = vp != nullptr && vp->get_obj_type() == OT_VEHICLE && vehicle_human_in_vehicle(vp, hp);
        }
        if (!in_attacker_vehicle)
        {
            cb_info->hit_humans.add_unique(hp);
        }
    }
    return false;
}

// 0x00BE7E90 (:3459)
// Casts the bullet (the local player's penetrating weapons go through weapons_test_penetrating_bullet). The start is
// pulled 0.4 m back (only the fire info's for a player). A ray around the muzzle (0.6 m each way) checks that the
// muzzle is not inside a wall. The humans the ray passes are tested against their hit locations; the nearest hit in
// front wins, else the best aligned living hostile becomes the nearest human (for the autoaim).
bool weapons_test_bullet(weapon_fire_info& fire_info, const object* attacker, bool ignore_destroyable_shatter, float override_radius, vector muzzle_pos)
{
    if (fire_info.w_info != nullptr && fire_info.w_info->flags.penetrating_bullets && object_is_local_player(attacker))
    {
        sorted_list<weapon_fire_info, 50, float> hit_list(true);
        weapons_test_penetrating_bullet(fire_info, hit_list, attacker, false);
        return ((const player*)attacker)->penetrating_aim_target != 0xFFFFFFFF;
    }
    raycast_input in;
    in.start = fire_info.start_pos;
    in.end = fire_info.end_pos;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_flags = 0;
    raycast_input muzzle_in;
    muzzle_in.radius = 0.0f;
    muzzle_in.filter_params = nullptr;
    muzzle_in.filter_callback = nullptr;
    muzzle_in.m_filter_info = COLLIDE_RAY_DEFAULT;
    muzzle_in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    muzzle_in.m_flags = 0;
    raycast_output out;
    raycast_output muzzle_out;
    if (override_radius != 0.0f)
    {
        in.radius = override_radius;
    }
    const vector fire_start = fire_info.start_pos;
    // Magic number: 0.4 m back along the bullet.
    vector back = fire_start - fire_info.end_pos;
    back.normalize_safe();
    back *= 0.4f;
    fire_info.start_pos += back;
    if (!object_is_player(attacker))
    {
        in.start += back;
    }
    if (vec_dist_squared(in.start, in.end) < 0.0001f)
    {
        return false;
    }
    const bool no_human_attacker = attacker == nullptr || attacker->get_obj_type() != OT_HUMAN;
    const human* attacker_hp = no_human_attacker ? nullptr : (const human*)attacker;
    in.m_flags = ignore_destroyable_shatter ? 0x12 : 2;
    bullet_callback_info cb_info(attacker);
    in.filter_params = &cb_info;
    in.filter_callback = process_bullet_callback;
    muzzle_in.start = in.start;
    muzzle_in.end = in.end;
    muzzle_in.filter_params = in.filter_params;
    muzzle_in.radius = in.radius;
    muzzle_in.filter_callback = process_bullet_callback;
    muzzle_in.m_filter_info = in.m_filter_info;
    muzzle_in.m_flags = in.m_flags;
    muzzle_in.m_hit_collector_type = in.m_hit_collector_type;
    const bool hit = havok_raycast(in, out);
    if (hit)
    {
        float best = 10000.0f;
        for (int i = 0; i < out.m_hits.getSize(); i++)
        {
            const raycast_output::hit_info& h = out.m_hits[i];
            if (vec_dist(in.start, h.hit_pos) < best)
            {
                best = vec_dist(in.start, h.hit_pos);
                object* objp = World.object_from_handle(h.hit_object_handle);
                fire_info.hit_pos = h.hit_pos;
                fire_info.hit_handle = h.hit_object_handle;
                fire_info.hit_shape_key = h.hit_havok_shape_key;
                fire_info.hit_alt_body_idx = objp->get_alt_hk_body_handle_index(h.hit_havok_handle);
                fire_info.hit_normal = h.hit_normal;
                fire_info.hit_physical_material = h.hit_physical_material;
                fire_info.hit_effect_material = h.hit_effect_material;
            }
        }
    }
    else
    {
        fire_info.hit_pos = in.end;
    }
    if (!muzzle_pos.is_null())
    {
        bullet_callback_info muzzle_cb_info(attacker);
        // Magic number: the muzzle check reaches 0.6 m behind and in front of the muzzle.
        vector dir = fire_info.end_pos - muzzle_pos;
        muzzle_in.filter_params = &muzzle_cb_info;
        muzzle_in.filter_callback = process_bullet_callback;
        dir.normalize_safe();
        muzzle_in.start = vector(muzzle_pos.x - dir.x * 0.6f, muzzle_pos.y - dir.y * 0.6f, muzzle_pos.z - dir.z * 0.6f);
        muzzle_in.end = vector(dir.x * 0.6f + muzzle_pos.x, dir.y * 0.6f + muzzle_pos.y, dir.z * 0.6f + muzzle_pos.z);
        if (havok_raycast(muzzle_in, muzzle_out))
        {
            float best = 10000.0f;
            // Original bug kept: the loop runs over the main ray's hit count while reading the muzzle ray's hits
            // (checked in the asm), and the result is whether the main ray hit.
            for (int i = 0; i < out.m_hits.getSize(); i++)
            {
                const raycast_output::hit_info& h = muzzle_out.m_hits.begin()[i];
                if (vec_dist(muzzle_in.start, h.hit_pos) < best)
                {
                    best = vec_dist(muzzle_in.start, h.hit_pos);
                    object* objp = World.object_from_handle(h.hit_object_handle);
                    fire_info.hit_pos = h.hit_pos;
                    fire_info.hit_handle = h.hit_object_handle;
                    fire_info.hit_shape_key = h.hit_havok_shape_key;
                    fire_info.hit_alt_body_idx = objp->get_alt_hk_body_handle_index(h.hit_havok_handle);
                    fire_info.hit_normal = h.hit_normal;
                    fire_info.hit_physical_material = h.hit_physical_material;
                    fire_info.hit_effect_material = h.hit_effect_material;
                }
            }
            return out.m_hits.getSize() > 0;
        }
    }
    fire_info.nearest_human = 0xFFFFFFFF;
    vector best_fire_pos = ZERO_VECTOR;
    vector best_target_pos = ZERO_VECTOR;
    vector best_hit_point = ZERO_VECTOR;
    vector best_normal = ZERO_VECTOR;
    object_hit_location best_location = HIT_NONE;
    int best_bone = 0;
    human* nearest = nullptr;
    human* best_hp = nullptr;
    float best_dist_sq = vec_dist_squared(in.start, fire_info.hit_pos);
    float best_dot = 0.0f;
    vector dir;
    vec_normalized_dir(dir, in.end, in.start);
    const int num_humans = cb_info.hit_humans.size();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = cb_info.hit_humans[i];
        vector intersection;
        vector normal;
        int bone;
        const object_hit_location location = human_get_hit_location(hp, in.start, dir, intersection, 0.0f, &bone, &normal);
        if (location == HIT_NONE)
        {
            intersection = hp->pos;
        }
        if (!ix_is_normal_facing(intersection, dir, fire_start))
        {
            continue;
        }
        if (location == HIT_NONE)
        {
            if (best_hp == nullptr && hp != nullptr && !hp->is_killed() && (no_human_attacker || human_teams_get_disposition(hp, attacker_hp) < TEAM_DISPOSITION_NONE))
            {
                vector center;
                hp->get_center_pos(center);
                vector to_center;
                vec_normalized_dir(to_center, center, in.start);
                const float dot = dir.x * to_center.x + to_center.y * dir.y + to_center.z * dir.z;
                if (best_dot < dot)
                {
                    best_dot = dot;
                    nearest = hp;
                }
            }
        }
        else
        {
            const float dist_sq = vec_dist_squared(in.start, intersection);
            if (dist_sq < best_dist_sq)
            {
                best_fire_pos = in.start;
                best_target_pos = in.end;
                best_hit_point = intersection;
                best_location = location;
                best_bone = bone;
                best_normal = normal;
                best_hp = hp;
                best_dist_sq = dist_sq;
            }
        }
    }
    if (best_hp != nullptr)
    {
        best_hp->raycast_hit_info.fire_pos = best_fire_pos;
        best_hp->raycast_hit_info.target_pos = best_target_pos;
        best_hp->raycast_hit_info.hit_point = best_hit_point;
        best_hp->raycast_hit_info.hit_location = best_location;
        best_hp->raycast_hit_info.hit_bone = best_bone;
        fire_info.hit_pos = best_hit_point;
        fire_info.hit_handle = best_hp->get_handle();
        fire_info.hit_normal = best_normal;
        fire_info.hit_location = best_location;
        fire_info.hit_bone = best_bone;
        fire_info.hit_shape_key = 0xFFFFFFFF;
        fire_info.hit_physical_material = best_hp->get_physical_material(0xFFFFFFFF, &fire_info.hit_pos, nullptr);
        fire_info.hit_effect_material = best_hp->get_effect_material(fire_info.hit_shape_key, &fire_info.hit_pos, nullptr);
        return true;
    }
    if (nearest != nullptr)
    {
        fire_info.nearest_human = nearest->get_handle();
    }
    return hit;
}

// 0x00BE5C80 (:3748)
// Casts a penetrating bullet (all hits): every hit up to the first hit of an OT 0x13 object (a shield wall) goes into the
// hit list by distance, and so do the humans the ray passes whose hit location is in front. The local player
// remembers the nearest living human (or object) hit as the penetrating aim target.
void weapons_test_penetrating_bullet(weapon_fire_info& fire_info, sorted_list<weapon_fire_info, 50, float>& hit_list, const object* attacker, bool ignore_destroyable_shatter)
{
    raycast_input in;
    in.radius = 0.0f;
    in.start = fire_info.start_pos;
    in.end = fire_info.end_pos;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    in.m_flags = ignore_destroyable_shatter ? 0x12 : 2;
    raycast_output out;
    unsigned int target_handle = 0xFFFFFFFF;
    vector target_pos = ZERO_VECTOR;
    const vector fire_start = fire_info.start_pos;
    const bool human_attacker = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN;
    const human* attacker_hp = human_attacker ? (const human*)attacker : nullptr;
    // Magic number: 0.4 m back along the bullet.
    vector back = fire_start - fire_info.end_pos;
    back.normalize_safe();
    back *= 0.4f;
    fire_info.start_pos += back;
    if (!object_is_player(attacker))
    {
        in.start += back;
    }
    bullet_callback_info cb_info(attacker);
    in.filter_params = &cb_info;
    in.filter_callback = process_bullet_callback;
    havok_raycast(in, out);
    const int num_hits = out.m_hits.getSize();
    fire_info.hit_pos = in.end;
    fire_info.nearest_human = 0xFFFFFFFF;
    float stop_dist_sq = 3.4028235e+38f;
    for (int i = 0; i < num_hits; i++)
    {
        const raycast_output::hit_info& h = out.m_hits[i];
        object* objp = World.object_from_handle(h.hit_object_handle);
        // Object type 0x13 stops a penetrating bullet.
        if (objp->get_obj_type() == 0x13)
        {
            const float dist_sq = vec_dist_squared(in.start, h.hit_pos);
            if (stop_dist_sq - dist_sq >= 0.0f)
            {
                stop_dist_sq = dist_sq;
            }
            fire_info.hit_pos = h.hit_pos;
        }
    }
    for (int i = num_hits - 1; i >= 0; i--)
    {
        const raycast_output::hit_info& h = out.m_hits[i];
        const float dist_sq = vec_dist_squared(in.start, h.hit_pos);
        if (dist_sq <= stop_dist_sq)
        {
            weapon_fire_info entry;
            entry.w_info = fire_info.w_info;
            entry.start_pos = fire_info.start_pos;
            entry.end_pos = fire_info.end_pos;
            entry.hit_pos = h.hit_pos;
            entry.hit_shape_key = h.hit_havok_shape_key;
            entry.hit_physical_material = h.hit_physical_material;
            entry.hit_handle = h.hit_object_handle;
            entry.hit_normal = h.hit_normal;
            entry.hit_effect_material = h.hit_effect_material;
            // Original: the other fields of the entry are left as they were (the copies of the list keep them).
            hit_list.add(entry, dist_sq);
        }
    }
    vector dir;
    vec_normalized_dir(dir, in.end, in.start);
    human* nearest = nullptr;
    float best_dot = 0.0f;
    bool hit_human = false;
    if (num_hits > 0)
    {
        const weapon_fire_info* first = hit_list.get_first();
        target_handle = first->hit_handle;
        target_pos = first->hit_pos;
    }
    const int num_humans = cb_info.hit_humans.size();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = cb_info.hit_humans[i];
        vector intersection;
        vector normal;
        int bone;
        const object_hit_location location = human_get_hit_location(hp, in.start, dir, intersection, 0.0f, &bone, &normal);
        if (location == HIT_NONE)
        {
            intersection = hp->pos;
        }
        if (!ix_is_normal_facing(intersection, dir, fire_start))
        {
            continue;
        }
        if (location == HIT_NONE)
        {
            if (!hit_human && hp != nullptr && !hp->is_killed() && (!human_attacker || human_teams_get_disposition(hp, attacker_hp) < TEAM_DISPOSITION_NONE))
            {
                vector center;
                hp->get_center_pos(center);
                vector to_center;
                vec_normalized_dir(to_center, center, in.start);
                const float dot = dir.x * to_center.x + to_center.y * dir.y + to_center.z * dir.z;
                if (best_dot < dot)
                {
                    nearest = hp;
                    best_dot = dot;
                }
            }
            continue;
        }
        const float dist_sq = vec_dist_squared(in.start, intersection);
        if (dist_sq >= stop_dist_sq)
        {
            continue;
        }
        weapon_fire_info entry;
        entry.end_pos = fire_info.end_pos;
        entry.w_info = fire_info.w_info;
        entry.start_pos = fire_info.start_pos;
        entry.hit_handle = hp->get_handle();
        entry.hit_bone = bone;
        entry.hit_pos = intersection;
        entry.hit_normal = normal;
        hit_human = true;
        entry.hit_shape_key = 0xFFFFFFFF;
        entry.hit_location = location;
        // Original: the materials are looked up with the fire info's shape key and hit position.
        entry.hit_physical_material = hp->get_physical_material(fire_info.hit_shape_key, &fire_info.hit_pos, nullptr);
        entry.hit_effect_material = hp->get_effect_material(fire_info.hit_shape_key, &fire_info.hit_pos, nullptr);
        hp->raycast_hit_info.fire_pos = fire_info.start_pos;
        hp->raycast_hit_info.target_pos = fire_info.end_pos;
        hp->raycast_hit_info.hit_point = intersection;
        hp->raycast_hit_info.hit_location = location;
        hp->raycast_hit_info.hit_bone = bone;
        hit_list.add(entry, dist_sq);
        if (!hp->is_killed())
        {
            object* current = target_handle != 0xFFFFFFFF ? World.object_from_handle(target_handle) : nullptr;
            if (current != nullptr && current->get_obj_type() == OT_HUMAN && vec_dist_squared(in.start, current->pos) <= dist_sq)
            {
                continue;
            }
            target_handle = hp->get_handle();
            target_pos = intersection;
        }
    }
    if (num_humans > 0 && !hit_human && nearest != nullptr)
    {
        fire_info.nearest_human = nearest->get_handle();
    }
    if (target_handle != 0xFFFFFFFF && object_is_local_player(attacker))
    {
        player* pp = (player*)attacker;
        pp->penetrating_aim_pos = target_pos;
        pp->penetrating_aim_target = target_handle;
    }
}

// 0x00BE1700 (:1269, LTCG: owner in EAX, target in EDI, fire_info in ESI)
// Within 25 degrees of the shot, a random tag of the target (or a point 0.95-1.52 m above its feet) is brought towards
// the bullet line by at most the autoaim radius (0.7-1.2 of the distance at random).
static bool get_autoaim_corrected_point(vector& corrected_point, const human* target, const human* owner, const weapon_fire_info& fire_info)
{
    float radius = weapons_get_autoaim_radius(owner);
    if (radius <= 0.0f)
    {
        return false;
    }
    vector dir = fire_info.end_pos - fire_info.start_pos;
    dir.normalize_safe();
    vector center;
    target->get_center_pos(center);
    vector to_center;
    vec_normalized_dir(to_center, center, fire_info.start_pos);
    // Magic number: within acos(0.9), about 25 degrees.
    if (dir.y * to_center.y + dir.x * to_center.x + dir.z * to_center.z < 0.9f)
    {
        return false;
    }
    vector tag_pos;
    // Magic numbers: tags 1-27 of the human rig.
    if (!target->get_tag_world_coords(int_rand_range(1, 27), tag_pos, nullptr, nullptr, nullptr))
    {
        const vector up = UP_VECTOR * 1.9f;
        const float t = fl_rand_range(0.5f, 0.8f);
        tag_pos = vector(target->pos.x + up.x * t, target->pos.y + up.y * t, target->pos.z + up.z * t);
    }
    vector closest;
    ix_closest_point_on_line_segment(closest, tag_pos, fire_info.start_pos, fire_info.end_pos);
    vector to_tag = tag_pos - closest;
    const float dist = to_tag.mag_and_normalize_safe();
    const float scale = fl_rand_range(0.7f, 1.2f);
    if (radius - scale * dist >= 0.0f)
    {
        radius = scale * dist;
    }
    corrected_point = vector(radius * to_tag.x + closest.x, to_tag.y * radius + closest.y, to_tag.z * radius + closest.z);
    return true;
}

// 0x00BE9DF0 (:1318)
// A miss (or a hit on a non-human) is bent towards the nearest human the ray passed: an npc only at its target and
// from a turret or in combat modes 4 and 5. The bullet is cast again.
bool weapon::bullet_autoaim(const human* owner, bool& bullet_hit)
{
    if (owner == nullptr)
    {
        return false;
    }
    if (bullet_hit && fire_info.hit_handle != 0xFFFFFFFF)
    {
        object* hit_objp = World.object_from_handle(fire_info.hit_handle);
        if (hit_objp != nullptr && hit_objp->get_obj_type() == OT_HUMAN)
        {
            return false;
        }
    }
    if (fire_info.nearest_human == 0xFFFFFFFF)
    {
        return false;
    }
    human* target = (human*)World.object_from_handle(fire_info.nearest_human);
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    if (owner->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        if (target->get_handle() != owner->combat.target_handle)
        {
            return false;
        }
        if (human_combat_get_manned_turret(owner) == nullptr)
        {
            // Attention modes 4 and 5 of the npc's perception.
            const int mode = (int)((const npc*)owner)->get_ai()->perception.attention.mode;
            if (mode != 4 && mode != 5)
            {
                return false;
            }
        }
    }
    vector corrected;
    if (!get_autoaim_corrected_point(corrected, target, owner, fire_info))
    {
        return false;
    }
    vector dir = corrected - fire_info.start_pos;
    dir = dir.copy_normalize_safe();
    const vector start = fire_info.start_pos;
    const float dx = start.x - fire_info.end_pos.x;
    const float dy = start.y - fire_info.end_pos.y;
    const float dz = start.z - fire_info.end_pos.z;
    const float length = sqrtf(dx * dx + dy * dy + dz * dz);
    fire_info.end_pos = vector(start.x + dir.x * length, start.y + dir.y * length, start.z + dir.z * length);
    object* owner_objp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner_objp = World.object_from_handle(get_owner_handle());
        if (owner_objp != nullptr && owner_objp->get_obj_type() != OT_HUMAN)
        {
            owner_objp = nullptr;
        }
    }
    bullet_hit = weapons_test_bullet(fire_info, owner_objp, false, 0.0f, ZERO_VECTOR);
    if (Autoaim_debug && bullet_hit)
    {
        World.object_from_handle<human>(fire_info.hit_handle); // the debug draw is stripped from the retail build
    }
    return true;
}

// 0x00BD9950 (:1452, LTCG: the result in ESI): a matrix with the given up vector and a random heading.
static matrix make_random_matrix_given_uvec(const vector& uvec)
{
    matrix m(IDENTITY_MATRIX);
    m.uvec = uvec.copy_normalize_safe();
    m.rvec = vector(m.uvec.y * m.fvec.z - m.uvec.z * m.fvec.y, m.fvec.x * m.uvec.z - m.uvec.x * m.fvec.z, m.uvec.x * m.fvec.y - m.fvec.x * m.uvec.y).copy_normalize_safe();
    m.fvec = vector(m.uvec.z * m.rvec.y - m.uvec.y * m.rvec.z, m.uvec.x * m.rvec.z - m.uvec.z * m.rvec.x, m.uvec.y * m.rvec.x - m.uvec.x * m.rvec.y).copy_normalize_safe();
    m.rotate_around_local_y(fl_rand_range(0.0f, PI2));
    return m;
}

// 0x00BDBDC0 (:1463)
// The hit's sparks and decal. In multiplayer only for bullets this machine simulates; not on the local player in first
// person. A backpack's additional and replacement hit effects (the attacker's, then the victim's) come first.
void weapons_create_bullet_hit_effects(human* attacker, const weapon_fire_info& fire_info, float damage)
{
    object* objp = World.object_from_handle(fire_info.hit_handle);
    player* attacker_pp = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)attacker : nullptr;
    player* victim_pp = objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)objp : nullptr;
    if (Multiplayer && !fire_info.mp_flags.sim_bullet)
    {
        return;
    }
    if (!object_is_local_player(objp) || Rfg_camera.mode != CAMERA_FIRST_PERSON_MODE)
    {
        bool play_spark = true;
        bool skip_victim = false;
        if (attacker_pp != nullptr && attacker_pp->backpack != nullptr)
        {
            const multi_backpack_info::abilities* abilities = attacker_pp->backpack->get_backpack_abilities();
            if (abilities->additional_bullet_hit_effect_other != nullptr)
            {
                effects_play(abilities->additional_bullet_hit_effect_other, fire_info.hit_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            if (abilities->replacement_bullet_hit_effect_other_player != nullptr && victim_pp != nullptr)
            {
                effects_play(attacker_pp->backpack->get_backpack_abilities()->replacement_bullet_hit_effect_other_player, fire_info.hit_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1,
                             0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                play_spark = false;
            }
        }
        if (victim_pp != nullptr && victim_pp->backpack != nullptr)
        {
            const multi_backpack_info::abilities* abilities = victim_pp->backpack->get_backpack_abilities();
            if (abilities->additional_bullet_hit_effect_self != nullptr)
            {
                effects_play(abilities->additional_bullet_hit_effect_self, fire_info.hit_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            if (victim_pp->backpack->get_backpack_abilities()->replacement_bullet_hit_effect_self != nullptr)
            {
                effects_play(victim_pp->backpack->get_backpack_abilities()->replacement_bullet_hit_effect_self, fire_info.hit_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF,
                             1.0f, false, EFFECTS_CATEGORY_NONE);
                skip_victim = true;
            }
        }
        if (play_spark && !skip_victim)
        {
            const material_shape_type shape_type = rfg_material_get_shape_type(objp, fire_info.hit_shape_key);
            bullet_spark_create(fire_info.hit_pos, fire_info.hit_normal, fire_info.w_info, fire_info.hit_effect_material, shape_type, objp, fire_info.hit_shape_key, -1.0f);
        }
    }
    if (fire_info.w_info->flags.penetrating_bullets && fire_info.w_info->special_hit_effect != 0xFFFFFFFF)
    {
        const matrix orient = make_random_matrix_given_uvec(fire_info.hit_normal);
        effects_play(fire_info.w_info->special_hit_effect, fire_info.hit_pos, orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || fire_info.w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE)
    {
        object_decal_create_bullet_impact_decal(objp, attacker, fire_info.w_info, fire_info.hit_pos, fire_info.hit_normal, damage, &fire_info);
    }
}

// 0x00BE1AF0 (:2193)
// The damage for the target category (npc, player, vehicle, vehicle with a player): the low scale damage up to the high
// scale distance (blended from the low scale distance), then falling to 0 at twice the maximum range.
float weapons_get_bullet_damage(const weapon_info* w_info, const object* target, float distance)
{
    const int category = weapons_get_damage_category(target);
    const float high_damage = w_info->high_scale_damage.damage[category];
    const float max_range = w_info->max_range;
    const float high_scale = w_info->high_scale_damage.scale_value;
    float fall_start = max_range;
    if (high_scale - max_range >= 0.0f)
    {
        fall_start = high_scale;
    }
    float fall_end = max_range * 2.0f;
    if (high_scale - fall_end >= 0.0f)
    {
        fall_end = high_scale;
    }
    if (distance <= fall_start)
    {
        return fl_parameter(distance, w_info->low_scale_damage.scale_value, high_scale, w_info->low_scale_damage.damage[category], high_damage, true);
    }
    return fl_parameter(distance, fall_start, fall_end, high_damage, 0.0f, true);
}

// 0x00BDC8A0 (:2237): npc 0, player 1, vehicle (or none) 2, vehicle with a player 3.
weapon_damage_category weapons_get_damage_category(const object* target)
{
    if (target == nullptr)
    {
        return WEAPON_DAMAGE_CATEGORY_VEHICLE;
    }
    if (target->get_obj_type() == OT_VEHICLE)
    {
        return (weapon_damage_category)(vehicle_player_in_vehicle((const vehicle*)target) + WEAPON_DAMAGE_CATEGORY_VEHICLE);
    }
    return (weapon_damage_category)object_is_player(target);
}

// 0x00BE59D0 (:1540, LTCG: fire_info in ESI)
// The damage by distance; in single player a player hit again soon takes less (Sequential_damage_factor). Effects,
// the hit itself, and for the local attacker in multiplayer the bullet hit packet and the hit statistics.
static void do_bullet_hit(human* attacker, const weapon_fire_info& fire_info)
{
    object* target = World.object_from_handle(fire_info.hit_handle);
    const weapon_info* w_info = fire_info.w_info;
    vector dir = fire_info.end_pos - fire_info.start_pos;
    dir.normalize_safe();
    const float dx = fire_info.hit_pos.x - fire_info.start_pos.x;
    const float dy = fire_info.hit_pos.y - fire_info.start_pos.y;
    const float dz = fire_info.hit_pos.z - fire_info.start_pos.z;
    player* attacker_pp = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)attacker : nullptr;
    player* victim = target != nullptr && target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)target : nullptr;
    float damage = weapons_get_bullet_damage(w_info, target, sqrtf(dx * dx + dy * dy + dz * dz));
    if (victim != nullptr && !Multiplayer && damage > 0.0f)
    {
        float percent = victim->previous_bullet_hit.percent();
        if (victim->pflags.do_sprint)
        {
            percent *= 0.75f;
        }
        if (percent < 1.0f && percent > 0.0f)
        {
            damage = (1.0f - Sequential_damage_factor * percent) * damage;
        }
        if (damage <= 0.0f)
        {
            damage = 0.0f;
        }
        else
        {
            victim->previous_bullet_hit.set(Sequential_damage_time);
        }
    }
    // Original: the target is not null-checked here (a hit always has one).
    if (target->get_obj_type() != OT_HUMAN || damage > 0.0f)
    {
        weapons_create_bullet_hit_effects(attacker, fire_info, damage);
    }
    weapons_process_bullet_hit(attacker, fire_info, damage);
    if (Multiplayer && attacker != nullptr && object_is_local_player(attacker))
    {
        if (fire_info.mp_flags.send_packet)
        {
            multi_gw_send_bullet_hit(attacker, fire_info, damage);
        }
        if (victim != nullptr && fire_info.mp_flags.hit_player)
        {
            multi_game_stats_record_hit(attacker_pp, victim, w_info, DBT_BULLET);
        }
    }
}

// 0x00BEC320 (:5423)
// The repair tool's beam while a player holds the trigger: the looping special effect Repair_range in front of the
// adjusted muzzle, along the player's camera (moved each frame), and the secondary effect once per pull. Released, the
// beam stops. The repair itself is fire_repair.
void weapon::process_repair()
{
    if (!flags.trigger_down)
    {
        if (special_effect_handle != 0xFFFFFFFF)
        {
            object_effect* effect = (object_effect*)World.object_from_handle(special_effect_handle);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        special_effect_handle = 0xFFFFFFFF;
        return;
    }
    if (get_owner_handle() == 0xFFFFFFFF)
    {
        return;
    }
    object* owner = World.object_from_handle(get_owner_handle());
    if (owner == nullptr || owner->get_obj_type() != OT_HUMAN || owner->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    vector adjusted_pos;
    vector adjusted_fvec;
    vector muzzle_pos;
    get_adjusted_muzzle_transform(adjusted_pos, adjusted_fvec, muzzle_pos);
    const vector beam_pos(adjusted_pos.x + Repair_range * adjusted_fvec.x, adjusted_pos.y + Repair_range * adjusted_fvec.y, adjusted_pos.z + Repair_range * adjusted_fvec.z);
    const matrix* camera_orient = nullptr;
    player_get_camera_pos_and_orient((player*)owner, nullptr, &camera_orient);
    object* effect = special_effect_handle != 0xFFFFFFFF ? World.object_from_handle(special_effect_handle) : nullptr;
    if (effect == nullptr || effect->get_obj_type() != OT_EFFECT)
    {
        special_effect_handle = effects_play(w_info->special_effect, beam_pos, *camera_orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    else
    {
        effect->update_pos_and_orient(beam_pos, *camera_orient, true);
    }
    if (!flags.processed_since_last_trigger_pull)
    {
        effects_play(w_info->secondary_special_effect, beam_pos, *camera_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00BDD520 (:5477)
// The flamethrower's effects on the muzzle tag: the secondary one (the pilot light) always loops; the special one (the
// flame) loops while the trigger is down. The damage and ignition are fire_flamethrower.
void weapon::process_flame()
{
    object* pilot = secondary_special_effect_handle != 0xFFFFFFFF ? World.object_from_handle(secondary_special_effect_handle) : nullptr;
    if (pilot == nullptr || pilot->get_obj_type() != OT_EFFECT)
    {
        secondary_special_effect_handle = effects_play(w_info->secondary_special_effect, get_handle(), muzzle_tag_index, true, 0xFFFFFFFF, 1.0f, false);
    }
    if (!flags.trigger_down)
    {
        if (special_effect_handle != 0xFFFFFFFF)
        {
            object_effect* flame = (object_effect*)World.object_from_handle(special_effect_handle);
            if (flame != nullptr && flame->get_obj_type() == OT_EFFECT)
            {
                flame->stop(object_effect::SS_STOP_EMITTING);
            }
        }
        special_effect_handle = 0xFFFFFFFF;
        return;
    }
    if (special_effect_handle != 0xFFFFFFFF)
    {
        object* flame = World.object_from_handle(special_effect_handle);
        if (flame != nullptr && flame->get_obj_type() == OT_EFFECT)
        {
            return;
        }
    }
    special_effect_handle = effects_play(w_info->special_effect, get_handle(), muzzle_tag_index, true, 0xFFFFFFFF, 1.0f, false);
}

// 0x00BDD610 (:5504)
// Explosive bullets explode at the hit (on the server); the nano rifle fades items out and plays its hit sounds (and
// a player's damage scales with the nano enhancer upgrade in single player). Then the object takes the hit. Not when the
// hit object is attached to the attacker, nor in multiplayer for bullets this machine does not simulate.
void weapons_process_bullet_hit(human* attacker, const weapon_fire_info& fire_info, float damage)
{
    if (Multiplayer && !fire_info.mp_flags.sim_bullet)
    {
        return;
    }
    object* objp = World.object_from_handle(fire_info.hit_handle);
    // Original: the hit object is not null-checked here.
    if (attacker != nullptr && objp->get_parent() == attacker->get_handle())
    {
        return;
    }
    const weapon_info* w_info = fire_info.w_info;
    const explosion_info* e_info = w_info->m_explosion_info;
    if (e_info != nullptr)
    {
        if (w_info->m_ai_explosion_info != nullptr && attacker != nullptr && attacker->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            e_info = w_info->m_ai_explosion_info;
        }
        if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
        {
            int sound;
            const object_type type = (object_type)objp->get_obj_type();
            if (type == OT_HUMAN)
            {
                if (!Multiplayer && !wrecking_crew_is_active() && object_is_player(attacker))
                {
                    damage = upgrade_get_nano_enhancer() * damage;
                }
                sound = fire_info.w_info->secondary_sound;
            }
            else if (type == OT_ITEM)
            {
                item* ip = (item*)objp;
                // Item info flags 0x3000: a shield.
                if ((ip->info->flags & 0x3000) != 0)
                {
                    human* holder = World.object_from_handle<human>(ip->get_owner_handle());
                    if (holder != nullptr && holder->has_personal_shield())
                    {
                        human_weapons_drop_shield(holder);
                    }
                    material_effect_parameters params;
                    const int matfx = material_effects_get_nano_rifle_matfx_id(true);
                    params.world_pos = fire_info.hit_pos;
                    ip->special_flags.fading_from_nano = 1;
                    params.flags = 1;
                    // Magic number: the nano fade lasts 2.5 s.
                    params.lifetime_ms = 2500;
                    params.use_local_pos = true;
                    params.use_time_in_w = true;
                    ip->deletion_timer.set(2500);
                    ip->start_nano_rifle_fade_out(params, matfx);
                }
                sound = fire_info.w_info->special_sound;
            }
            else if (type == OT_VEHICLE)
            {
                if (!Multiplayer && !wrecking_crew_is_active() && object_is_player(attacker))
                {
                    damage = upgrade_get_nano_enhancer() * damage;
                }
                sound = fire_info.w_info->upgrade_sound;
            }
            else
            {
                sound = fire_info.w_info->special_sound;
            }
            audiolib_cue_play_3d(sound, &fire_info.hit_pos, nullptr, nullptr, nullptr);
        }
        if (!Multiplayer || multi_is_server())
        {
            vector dir = fire_info.hit_pos - fire_info.start_pos;
            dir = dir.copy_normalize_safe();
            const vector back = -dir;
            matrix orient;
            orient.make(back);
            orient.rotate_around_local_x(PI_OVER_2);
            explosion_create(e_info, attacker, attacker, fire_info.hit_pos, orient, &dir, fire_info.w_info, false);
        }
    }
    if (objp != nullptr)
    {
        objp->process_bullet_hit(attacker, damage, fire_info);
    }
}

// 0x00BE3530 (:5645)
// The arc welder's beam. Like the tracers it drives vfxlib by hand: the effect's first emitter gets 300 particles and a
// 0.2-0.3 s lifetime, then the effect is stepped (0.01 s at a time) along a Catmull-Rom spline from the muzzle to the
// end, through two control points 8 m beyond each end (jittered by up to 3 m), with a random sideways jitter per step,
// and stops emitting at the end. Without a loaded visual the effect object is destroyed at once.
void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal)
{
    if (effect == 0xFFFFFFFF)
    {
        return;
    }
    vector dir = vector(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z).copy_normalize_safe();
    matrix arc_orient;
    arc_orient.make_quick(vector(-dir.x, -dir.y, -dir.z));
    const unsigned int handle = effects_play(effect, start_pos, arc_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    object_effect* arc = (object_effect*)objp;
    if (!arc->rdd.is_loaded() || arc->rdd.get_data()->vfx_effect_p == nullptr)
    {
        World.destroy_object(arc, false);
        return;
    }
    vfx_effect_template::emitter* emitters = arc->rdd.get_data()->vfx_effect_p->m_effect_template_p->m_vfx_data_p->emitters;
    vfx_particle_system_emitter* emitter = emitters[0].particle_system_emitter_p;
    emitter->max_particles = 300; // Magic number: 300 particles
    emitter->lifetime = fl_rand_range(0.2f, 0.3f); // Magic numbers: a 0.2-0.3 s lifetime
    arc->update(0.02f); // Magic number: a 20 ms prime step
    arc->pos = vector(arc->pos.x + dir.x * 0.02f, dir.y * 0.02f + arc->pos.y, arc->pos.z + dir.z * 0.02f);
    // Magic numbers: the outer control points 8 m beyond the ends, jittered by up to 3 m.
    vector pts[4];
    pts[0] = vector(start_pos.x - muzzle_normal.x * 8.0f, start_pos.y - muzzle_normal.y * 8.0f, start_pos.z - muzzle_normal.z * 8.0f);
    pts[1] = start_pos;
    pts[2] = end_pos;
    pts[3] = vector(end_pos.x + dir.x * 8.0f, end_pos.y + dir.y * 8.0f, end_pos.z + dir.z * 8.0f);
    vector jitter;
    jitter.rand_sphere();
    pts[0] = vector(jitter.x * 3.0f + pts[0].x, jitter.y * 3.0f + pts[0].y, jitter.z * 3.0f + pts[0].z);
    jitter.rand_sphere();
    pts[3] = vector(jitter.x * 3.0f + pts[3].x, jitter.y * 3.0f + pts[3].y, jitter.z * 3.0f + pts[3].z);
    catmull_rom_spline spline(4, pts);
    float u = 1.0f;
    for (;;)
    {
        arc->update(0.01f); // Magic number: 10 ms steps
        const vector to_end(end_pos.x - arc->pos.x, end_pos.y - arc->pos.y, end_pos.z - arc->pos.z);
        vector step;
        step.rand_around_dir(to_end.copy_normalize_safe(), 0.0f, 1.0f);
        const float step_size = fl_rand_range(0.0f, 0.06f); // Magic numbers: up to 6 cm sideways
        step = vector(step_size * step.x, step.y * step_size, step_size * step.z);
        u = fl_rand_range(0.03f, 0.08f) + u; // Magic numbers: 3-8% of the span per step
        if (u - 1.0f < 0.0f)
        {
            u = 1.0f;
        }
        if (u - 2.0f >= 0.0f)
        {
            u = 2.0f;
        }
        spline.get_point(arc->pos, u);
        if (u >= 2.0f)
        {
            break;
        }
        arc->pos = vector(arc->pos.x + step.x, step.y + arc->pos.y, arc->pos.z + step.z);
    }
    arc->update(0.02f);
    if (Arc_debug_effect)
    {
        // The debug draw of the spline is stripped from retail; only the samples are left.
        for (int i = 0; i < 10; i++)
        {
            vector sample;
            spline.get_point(sample, (float)i / 10.0f + 1.0f);
        }
    }
    arc->stop(object_effect::SS_STOP_EMITTING);
}

// 0x00BE3CC0 (:5772). No callers left in rfg.exe. A single curved tracer: like weapon_create_arc_effect it drives vfxlib by
// hand, stepping the effect along a Catmull-Rom spline from the muzzle to the end. Magic numbers: a 0.35 s lifetime and
// 300 particles; the control points leave along the muzzle normal plus the aim, and run 8 m past the ends. Without a
// loaded visual it reads through null, as the original.
void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal)
{
    if (effect == 0xFFFFFFFF)
    {
        return;
    }
    const vector dir = vector(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z).copy_normalize_safe();
    matrix tracer_orient;
    tracer_orient.make_quick(vector(-dir.x, -dir.y, -dir.z));
    const unsigned int handle = effects_play(effect, start_pos, tracer_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    object_effect* tracer = (object_effect*)objp;
    vfx_effect* visual = tracer->rdd.is_loaded() ? tracer->rdd.get_data()->vfx_effect_p : nullptr;
    vfx_effect_template::emitter* emitters = visual->m_effect_template_p->m_vfx_data_p->emitters;
    vfx_particle_system_emitter* emitter = emitters[0].particle_system_emitter_p;
    emitter->lifetime = 0.35f;
    emitter->max_particles = 300;
    tracer->update(0.01f); // Magic number: a 10 ms prime step
    tracer->pos = vector(tracer->pos.x + dir.x * 0.02f, tracer->pos.y + dir.y * 0.02f, tracer->pos.z + dir.z * 0.02f); // 2 cm off the muzzle
    vector bend = vector(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z).copy_normalize_safe();
    bend = vector(bend.x + muzzle_normal.x, muzzle_normal.y + bend.y, muzzle_normal.z + bend.z);
    const float length = vec_dist(start_pos, end_pos);
    vector pts[5];
    pts[0] = vector(start_pos.x - bend.x * 8.0f * 0.5f, start_pos.y - bend.y * 8.0f * 0.5f, start_pos.z - bend.z * 8.0f * 0.5f);
    pts[1] = start_pos;
    pts[2] = vector(start_pos.x + bend.x * 0.5f * length * 0.75f, bend.y * 0.5f * length * 0.75f + start_pos.y, start_pos.z + bend.z * 0.5f * length * 0.75f);
    pts[3] = end_pos;
    pts[4] = vector(end_pos.x + dir.x * 8.0f, end_pos.y + dir.y * 8.0f, end_pos.z + dir.z * 8.0f);
    catmull_rom_spline spline(5, pts);
    float u = 1.0f;
    do
    {
        tracer->update(0.005f); // Magic number: 5 ms steps
        u = fl_rand_range(0.03f, 0.08f) + u; // Magic numbers: 3-8% of the span per step
        if (u - 1.0f < 0.0f)
        {
            u = 1.0f;
        }
        if (0.0f <= u - 3.0f)
        {
            u = 3.0f;
        }
        spline.get_point(tracer->pos, u);
    } while (u < 3.0f);
    tracer->update(0.01f);
    if (Arc_debug_effect)
    {
        // The debug draw of the spline is stripped from retail; only the samples are left. Magic number: 0.3 samples a
        // metre (pieces_multiplier, 0x016EBD5C).
        const double far_part = (double)vec_dist(pts[2], end_pos);
        const int pieces = (int)(((double)vec_dist(start_pos, pts[2]) + far_part) * (double)0.3f);
        for (int i = 0; i < pieces; i++)
        {
            vector sample;
            spline.get_point(sample, (float)((double)(float)i / (double)(float)pieces + 1.0));
        }
    }
    tracer->stop(object_effect::SS_STOP_EMITTING);
}

// 0x00BDC110 (:1602): a curved tracer through two random points along the bullet (a backpack may replace the effect).
void weapon_create_curved_tracer_base(const human* hp, const vector& muzzle_pos, const vector& end_pos, const weapon_info* w_info, weapon_tracer_type tracer_type)
{
    const player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)hp : nullptr;
    vector dir = end_pos - muzzle_pos;
    dir = dir.copy_normalize_safe();
    const float length = vec_dist(muzzle_pos, end_pos);
    // Magic numbers: the bend grows with the length up to 1.5 (at 60 m).
    float bend = length / 40.0f;
    if (bend - 1.5f >= 0.0f)
    {
        bend = 1.5f;
    }
    vector offset1;
    offset1.rand_around_dir(dir, 0.0f, 0.0f);
    float t = fl_rand_range(0.3f, 0.5f);
    offset1 = vector(t * offset1.x * bend, offset1.y * t * bend, offset1.z * t * bend);
    vector offset2;
    offset2.rand_around_dir(dir, 0.0f, 0.0f);
    t = fl_rand_range(0.2f, 0.5f);
    offset2 = vector(offset2.x * t * bend, offset2.y * t * bend, offset2.z * t * bend);
    vector along = dir * length;
    t = fl_rand_range(0.25f, 0.35f);
    const vector p1(muzzle_pos.x + along.x * t + offset1.x, along.y * t + muzzle_pos.y + offset1.y, muzzle_pos.z + along.z * t + offset1.z);
    along = dir * length;
    t = fl_rand_range(0.65f, 0.8f);
    const vector p2(muzzle_pos.x + along.x * t + offset2.x, along.y * t + muzzle_pos.y + offset2.y, muzzle_pos.z + along.z * t + offset2.z);
    if (pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->bullet_tracer != nullptr)
    {
        curved_tracers_create(pp->get_handle(), muzzle_pos, p1, p2, end_pos, effects_get_handle(pp->backpack->get_backpack_abilities()->bullet_tracer), tracer_type);
        return;
    }
    curved_tracers_create(hp->get_handle(), muzzle_pos, p1, p2, end_pos, w_info->tracer_effect, tracer_type);
}

// 0x00BDC710 (:1638): every tracer_frequency-th bullet of a visible weapon leaves a curved tracer.
void weapon::create_curved_tracers(const human* hp, const vector& muzzle_pos, const vector& end_pos, weapon_tracer_type tracer_type)
{
    tracer_count++;
    if (Tracers_enabled && (render_alpha == 1.0f || casts_transparent_shadows))
    {
        const unsigned char frequency = w_info->tracer_frequency;
        if (frequency != 0 && frequency <= tracer_count)
        {
            tracer_count = 0;
            weapon_create_curved_tracer_base(hp, muzzle_pos, end_pos, w_info, tracer_type);
        }
    }
}

// 0x00BDC780 (:1650): a backpack may replace the tracer effect (the rail one for rail tracers).
void weapon_create_tracer_base(const human* hp, const vector& muzzle_pos, const vector& end_pos, const weapon_info* w_info, weapon_tracer_type tracer_type)
{
    const player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)hp : nullptr;
    unsigned int effect = w_info->tracer_effect;
    if (pp != nullptr && pp->backpack != nullptr)
    {
        const multi_backpack_info::abilities* abilities = pp->backpack->get_backpack_abilities();
        const effect_info* info = tracer_type == WEAPON_TRACER_RAIL ? abilities->rail_tracer : abilities->bullet_tracer;
        if (info != nullptr)
        {
            effect = effects_get_handle(info);
        }
    }
    tracers_create(hp->get_handle(), muzzle_pos, end_pos, effect, tracer_type);
}

// 0x00BDC830 (:1679): every tracer_frequency-th bullet of a visible weapon leaves a tracer.
void weapon::create_tracers(const human* hp, const vector& muzzle_pos, const vector& end_pos, weapon_tracer_type tracer_type)
{
    tracer_count++;
    if (Tracers_enabled && (render_alpha == 1.0f || casts_transparent_shadows))
    {
        const unsigned char frequency = w_info->tracer_frequency;
        if (frequency != 0 && frequency <= tracer_count)
        {
            tracer_count = 0;
            weapon_create_tracer_base(hp, muzzle_pos, end_pos, w_info, tracer_type);
        }
    }
}

// 0x00BD9AF0 (:1691). No callers left in rfg.exe. In multiplayer only automatic weapons of the other players leave
// tracers; every shot does otherwise.
static bool bullet_should_create_tracer(const human* shooter, const weapon_info* w_info)
{
    if (Multiplayer && !object_is_local_player(shooter))
    {
        return w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC;
    }
    return true;
}

// 0x00BD9B20 (:1708, LTCG: w_info in EAX, fire_info in EDI)
// The local player's tracer goes to the other machines: always for a single shot weapon, for an automatic only when the
// bullet hit a player it may hurt (the local player per the checks; see the original's last test).
static bool bullet_should_send_tracer_packet(const human* shooter, const weapon_info* w_info, const weapon_fire_info& fire_info)
{
    if (!Multiplayer || !object_is_local_player(shooter) || w_info->tracer_frequency == 0)
    {
        return false;
    }
    if (w_info->trigger_type == WEAPON_TRIGGER_TYPE_SINGLE)
    {
        return true;
    }
    if (w_info->trigger_type != WEAPON_TRIGGER_TYPE_AUTOMATIC)
    {
        return false;
    }
    object* victim = World.object_from_handle(fire_info.hit_handle);
    // Original: the victim must be the local player, so an automatic's tracer is never sent by its shooter.
    return victim != nullptr && multi_game_weapon_bullet_strike_is_important_to_sync(fire_info, victim) && multi_gw_can_hit_target(shooter, victim) && object_is_local_player(victim);
}

// The multiplayer flags of a simulated bullet hit (inlined in fire_bullet and fire_penetrating_bullet): the hit is
// sent when it matters (the local attacker's hits on what it may hurt), and simulated here when the attacker is local or
// the hit is not one the shooter's machine reports.
static void weapons_set_mp_fire_flags(weapon_fire_info& fire_info, const player* owner_pp, const object* hit_objp, bool trigger_single)
{
    fire_info.mp_flags.flag_data = 0;
    const bool hit_player = object_is_player(hit_objp);
    const bool important = multi_game_weapon_bullet_strike_is_important_to_sync(fire_info, hit_objp) && multi_gw_can_hit_target(owner_pp, hit_objp);
    const bool local = object_is_local_player(owner_pp);
    const bool sim = local || !important || !hit_player;
    const bool send = local && important;
    fire_info.mp_flags.local_fire = object_is_local_player(owner_pp);
    fire_info.mp_flags.trigger_single = trigger_single;
    const bool hit = object_is_local_player(owner_pp) || (multi_game_weapon_bullet_strike_is_important_to_sync(fire_info, hit_objp) && multi_gw_can_hit_target(owner_pp, hit_objp));
    fire_info.mp_flags.hit_player = hit;
    fire_info.mp_flags.sim_bullet = sim;
    fire_info.mp_flags.send_packet = send;
}

// 0x00BEA0C0 (:1751)
// Casts the bullet (bent by the autoaim), hits what it hits, and continues through shatterable glass (the ray restarts
// where it leaves the glass piece's box). The owner remembers a hostile hit as its combat target; npcs hear the strike;
// an EDF bullet passing the local player plays the flyby. Then the tracer (in multiplayer only the local player's and
// automatics) and the tracer packet.
void weapon::fire_bullet(const vector& start, const vector& end, const vector& muzzle_pos)
{
    fire_info.init(start, end, w_info);
    flags.fire_info_is_new = true;
    human* owner = nullptr;
    player* owner_pp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner == nullptr || owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
        else if (owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            owner_pp = (player*)owner;
        }
    }
    unsigned int last_glass_handle = 0xFFFFFFFF;
    unsigned int last_glass_key = 0xFFFFFFFF;
    bool hit_hostile = false;
    bool through_glass;
    do
    {
        through_glass = false;
        bool hit = weapons_test_bullet(fire_info, owner, false, 0.0f, muzzle_pos);
        if (owner != nullptr)
        {
            bullet_autoaim(owner, hit);
        }
        propagate_bullet_flight_sounds(fire_info.start_pos, fire_info.hit_pos, fire_info, owner);
        if (!hit || (last_glass_handle == fire_info.hit_handle && last_glass_key == fire_info.hit_shape_key))
        {
            break;
        }
        object* hit_objp = World.object_from_handle(fire_info.hit_handle);
        if (Multiplayer)
        {
            weapons_set_mp_fire_flags(fire_info, owner_pp, hit_objp, w_info->trigger_type == WEAPON_TRIGGER_TYPE_SINGLE);
        }
        do_bullet_hit(owner, fire_info);
        if (hit_objp != nullptr && hit_objp->get_obj_type() == OT_MOVER && hit_objp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* rmp = (rfg_mover*)hit_objp;
            if (rmp->is_shatterable(fire_info.hit_shape_key) && !rmp->flags.invulnerable)
            {
                rfg_destroyable_shape* shape = rmp->rdd.is_loaded() ? rmp->rdd.get_data()->shape : nullptr;
                const rfg_subpiece_base* piece = (rfg_subpiece_base*)shape->parent->base_objects + fire_info.hit_shape_key;
                vector local_end = end;
                vector local_hit = fire_info.hit_pos;
                vec_change_to_local_coords(local_end, rmp->pos, rmp->orient);
                vec_change_to_local_coords(local_hit, rmp->pos, rmp->orient);
                vector back;
                vec_normalized_dir(back, local_hit, local_end);
                vector exit_pos;
                if (ix_ray_boundingbox(exit_pos, piece->prel_bmin, piece->prel_bmax, local_end, back))
                {
                    last_glass_handle = fire_info.hit_handle;
                    last_glass_key = fire_info.hit_shape_key;
                    vec_change_to_global_coords(exit_pos, rmp->pos, rmp->orient);
                    fire_info.init(exit_pos, end, w_info);
                    through_glass = true;
                }
            }
        }
        if (!hit_hostile)
        {
            human* hit_hp = nullptr;
            if (fire_info.hit_handle != 0xFFFFFFFF)
            {
                hit_hp = (human*)World.object_from_handle(fire_info.hit_handle);
                if (hit_hp != nullptr && hit_hp->get_obj_type() != OT_HUMAN)
                {
                    hit_hp = nullptr;
                }
            }
            if (owner_pp != nullptr && hit_hp != nullptr && human_teams_get_disposition(owner_pp, hit_hp) == TEAM_DISPOSITION_HOSTILE)
            {
                hit_hostile = true;
            }
        }
        if (!through_glass)
        {
            if (weapon_hit_sound_timestamp.elapsed())
            {
                npc_sound_info_bullet_strike sound(owner, w_info, fire_info.hit_pos);
                npc_sound_info::queue<npc_sound_info_bullet_strike>(sound);
                // Magic number: a strike is heard at most every 333 ms.
                weapon_hit_sound_timestamp.set(333);
            }
            if (owner_pp != nullptr)
            {
                player_update_combat_target(owner_pp, hit_objp);
            }
        }
        player* local_pp = player_get_local_player();
        // Original: the owner is not null-checked here.
        if (local_pp != nullptr && owner != local_pp && fire_info.hit_handle != local_pp->get_handle() && owner->current_team == HUMAN_TEAM_EDF)
        {
            const vector up = UP_VECTOR * 1.9f;
            const float t = fl_rand_range(0.5f, 0.8f);
            const vector head(up.x * t + local_pp->pos.x, local_pp->pos.y + up.y * t, local_pp->pos.z + up.z * t);
            vector closest;
            ix_closest_point_on_line_segment(closest, head, fire_info.start_pos, fire_info.end_pos);
            const int cue = w_info->flyby_sound;
            if ((closest.x - head.x) * (closest.x - head.x) + (closest.y - head.y) * (closest.y - head.y) + (closest.z - head.z) * (closest.z - head.z) <
                    Bullet_flyby_sound_dist * Bullet_flyby_sound_dist &&
                cue != -1 && cue != 0)
            {
                audiolib_cue_play_3d(cue, &closest, nullptr, nullptr, nullptr);
            }
        }
    } while (through_glass);
    vector tracer_start = muzzle_pos;
    weapon_tracer_type tracer_type;
    if (w_info->flags.auto_aim_curved_trail)
    {
        tracer_type = WEAPON_TRACER_ENFORCER;
        if (owner_pp == nullptr || Rfg_camera.mode == CAMERA_SLEW_MODE)
        {
            matrix muzzle_orient;
            get_muzzle_transform(tracer_start, &muzzle_orient);
        }
    }
    else
    {
        tracer_type = WEAPON_TRACER_DEFAULT;
        if (w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
        {
            tracer_type = WEAPON_TRACER_NANO;
        }
        else if (w_info->flags.can_scope && w_info->weapon_anim_group == ANIMATION_GROUP_SNIPER)
        {
            if (player_is_scoped(owner_pp))
            {
                // Magic number: a scoped sniper's tracer starts 0.15 m lower.
                tracer_start = vector(tracer_start.x - UP_VECTOR.x * 0.15f, tracer_start.y - UP_VECTOR.y * 0.15f, tracer_start.z - UP_VECTOR.z * 0.15f);
            }
            tracer_type = WEAPON_TRACER_SNIPER;
        }
    }
    if (!Multiplayer || object_is_local_player(owner) || w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC)
    {
        if (w_info->flags.auto_aim_curved_trail)
        {
            create_curved_tracers(owner, tracer_start, fire_info.hit_pos, tracer_type);
        }
        else
        {
            create_tracers(owner, tracer_start, fire_info.hit_pos, tracer_type);
        }
    }
    if (bullet_should_send_tracer_packet(owner, w_info, fire_info))
    {
        matrix muzzle_orient;
        if (!get_tag_world_coords(muzzle_tag_index, fire_info.start_pos, &muzzle_orient, &pos, &orient))
        {
            fire_info.start_pos = muzzle_pos;
        }
        if (owner_pp != nullptr && player_is_scoped(owner_pp))
        {
            vector dir = fire_info.end_pos - fire_info.start_pos;
            dir = dir.copy_normalize_safe();
            // Magic number: a scoped tracer starts 0.95 m out.
            fire_info.start_pos = vector(fire_info.start_pos.x + dir.x * 0.95f, dir.y * 0.95f + fire_info.start_pos.y, dir.z * 0.95f + fire_info.start_pos.z);
        }
        multi_gw_send_tracer(owner, fire_info);
    }
}

// 0x00BE6D40 (:1947)
// The rail driver: a first cast from the camera (through humans and glass) picks the aim point (the first human hit, or
// the first hit 1.2 m away); the real cast from the muzzle towards it collects every hit, and each takes the bullet.
// The last hit ends the tracer (a rail one for the rail driver).
void weapon::fire_penetrating_bullet(const vector& start, const vector& end, const vector& muzzle_pos)
{
    weapon_fire_info aim_info;
    aim_info.start_pos = start;
    aim_info.w_info = w_info;
    aim_info.end_pos = end;
    flags.fire_info_is_new = true;
    aim_info.hit_handle = 0xFFFFFFFF;
    human* owner = nullptr;
    player* owner_pp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner == nullptr || owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
        else if (owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            owner_pp = (player*)owner;
        }
    }
    bool have_aim = false;
    bool aim_human = false;
    bool updated_target = false;
    {
        sorted_list<weapon_fire_info, 50, float> aim_list(true);
        weapons_test_penetrating_bullet(aim_info, aim_list, owner, true);
        for (const weapon_fire_info* entry = aim_list.get_first(); entry != nullptr; entry = aim_list.get_next(entry))
        {
            const float dx = entry->hit_pos.x - start.x;
            const float dy = entry->hit_pos.y - start.y;
            const float dz = entry->hit_pos.z - start.z;
            // Magic number: hits within 1.2 m of the camera are skipped.
            if (dy * dy + dx * dx + dz * dz < 1.44f)
            {
                continue;
            }
            object* objp = World.object_from_handle(entry->hit_handle);
            if (objp->get_obj_type() == OT_HUMAN)
            {
                have_aim = true;
                aim_human = true;
                aim_info = *entry;
                break;
            }
            if (!have_aim)
            {
                aim_info = *entry;
                have_aim = true;
            }
        }
    }
    vector aim_dir;
    if (have_aim)
    {
        aim_dir = vector(aim_info.hit_pos.x - muzzle_pos.x, aim_info.hit_pos.y - muzzle_pos.y, aim_info.hit_pos.z - muzzle_pos.z);
    }
    else
    {
        aim_dir = vector(end.x - muzzle_pos.x, end.y - muzzle_pos.y, end.z - muzzle_pos.z);
    }
    aim_dir = aim_dir.copy_normalize_safe();
    // Magic number: the real cast reaches 250 m from the muzzle.
    const vector real_end(muzzle_pos.x + aim_dir.x * 250.0f, muzzle_pos.y + aim_dir.y * 250.0f, muzzle_pos.z + aim_dir.z * 250.0f);
    fire_info.init(muzzle_pos, real_end, w_info);
    sorted_list<weapon_fire_info, 50, float> hit_list(true);
    weapons_test_penetrating_bullet(fire_info, hit_list, owner, false);
    propagate_bullet_flight_sounds(fire_info.start_pos, fire_info.hit_pos, fire_info, owner);
    int num_hostiles = 0;
    for (sorted_list_element<weapon_fire_info, float>* elem = (sorted_list_element<weapon_fire_info, float>*)hit_list.get_first(); elem != nullptr;)
    {
        object* objp = World.object_from_handle(elem->data.hit_handle);
        if (objp->get_obj_type() == OT_HUMAN && owner != nullptr &&
            human_teams_get_disposition(human_teams_get_team(objp), owner->current_team) == TEAM_DISPOSITION_HOSTILE)
        {
            num_hostiles++;
        }
        if (Multiplayer)
        {
            // Original: the trigger single flag is always set here.
            weapons_set_mp_fire_flags(elem->data, owner_pp, objp, true);
        }
        do_bullet_hit(owner, elem->data);
        if (!aim_human && objp->get_obj_type() == OT_HUMAN)
        {
            aim_human = true;
            fire_info = elem->data;
            have_aim = true;
        }
        else if (!have_aim)
        {
            // The aim pass's results carry over: the first hit only counts when the aim pass found nothing.
            fire_info = elem->data;
            have_aim = true;
        }
        if (owner_pp != nullptr && !updated_target)
        {
            updated_target = player_update_combat_target(owner_pp, objp);
        }
        elem = (sorted_list_element<weapon_fire_info, float>*)hit_list.get_next(&elem->data);
    }
    if (Multiplayer && owner_pp != nullptr && num_hostiles > 1)
    {
        multi_game_xp_trigger_event(owner_pp->mp_data, MXE_LINED_UP, 1, 0xFF, -1, false);
    }
    const weapon_fire_info* last = hit_list.size() > 0 ? hit_list.get_last() : &fire_info;
    const weapon_tracer_type tracer_type = (weapon_tracer_type)(strcmp(w_info->name, "rail_driver") == 0);
    fire_info.hit_pos = last->hit_pos;
    vector tracer_start = muzzle_pos;
    if (owner_pp != nullptr && player_is_scoped(owner_pp))
    {
        tracer_start = vector(muzzle_pos.x - UP_VECTOR.x * 0.15f, muzzle_pos.y - UP_VECTOR.y * 0.15f, muzzle_pos.z - UP_VECTOR.z * 0.15f);
    }
    if (!Multiplayer || object_is_local_player(owner) || w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC)
    {
        tracer_count++;
        if (Tracers_enabled && (render_alpha == 1.0f || casts_transparent_shadows))
        {
            const unsigned char frequency = w_info->tracer_frequency;
            if (frequency != 0 && frequency <= tracer_count)
            {
                tracer_count = 0;
                weapon_create_tracer_base(owner, tracer_start, fire_info.hit_pos, w_info, tracer_type);
            }
        }
    }
    if (Multiplayer && object_is_local_player(owner) && w_info->tracer_frequency != 0)
    {
        bool send = w_info->trigger_type == WEAPON_TRIGGER_TYPE_SINGLE;
        if (w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC)
        {
            object* victim = World.object_from_handle(fire_info.hit_handle);
            send = victim != nullptr && multi_game_weapon_bullet_strike_is_important_to_sync(fire_info, victim) && multi_gw_can_hit_target(owner, victim) &&
                   object_is_local_player(victim);
        }
        if (send)
        {
            fire_info.start_pos = muzzle_pos;
            multi_gw_send_tracer(owner, fire_info);
        }
    }
    if (weapon_hit_sound_timestamp.elapsed())
    {
        npc_sound_info_bullet_strike sound(owner, w_info, last->hit_pos);
        npc_sound_info::queue<npc_sound_info_bullet_strike>(sound);
        weapon_hit_sound_timestamp.set(333);
    }
}

// 0x00BDC8E0 (:2519): a thrown weapon does not stick to parented or constrained projectiles; in multiplayer only to valid
// collision targets.
static bool allow_collide_callback(object* hit_objp, void* filter_params)
{
    if (hit_objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && (hit_objp->child_constraint_ptr != nullptr || hit_objp->get_parent() != 0xFFFFFFFF))
    {
        return false;
    }
    if (Multiplayer)
    {
        return projectile_is_valid_mp_collision_target(hit_objp);
    }
    return true;
}

// 0x00BE77D0 (:2534)
// A mine or singularity bomb is planted where a 0.2 m sphere from the thrower's head down to its feet hits (not on a
// human), with the planting sound.
bool weapon_projectile_throw_collide(const vector& start_pos, const vector& end_pos, projectile* proj)
{
    raycast_input in;
    in.start = start_pos;
    in.end = end_pos;
    in.m_filter_info = proj->collision_type;
    in.filter_params = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.radius = 0.2f;
    in.filter_callback = allow_collide_callback;
    raycast_output out;
    if (!havok_spherecast(in, out))
    {
        return false;
    }
    const raycast_output::hit_info& h = out.m_hits[0];
    if (h.hit_object_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(h.hit_object_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            return false;
        }
    }
    const matrix* orient_override = nullptr;
    matrix identity;
    if (proj->w_info->weapon_class != WEAPON_CLASS_SINGULARITY_BOMB)
    {
        if (planting_sound != -1 || audiolib_cue_get_id("WEP_CHARGE_PLANT", &planting_sound) == AUDIOLIB_ERR_NONE)
        {
            proj->m_foley_collision_instance = audiolib_cue_play_3d(planting_sound, &h.hit_pos, nullptr, nullptr, nullptr);
        }
    }
    else
    {
        if (singularity_planting_sound != -1 || audiolib_cue_get_id("WEP_SINGULARITY_ARM", &singularity_planting_sound) == AUDIOLIB_ERR_NONE)
        {
            proj->m_foley_collision_instance = audiolib_cue_play_3d(singularity_planting_sound, &h.hit_pos, nullptr, nullptr, nullptr);
        }
        identity.make_identity();
        orient_override = &identity;
    }
    proj->attach(World.object_from_handle(h.hit_object_handle), (int)h.hit_havok_shape_key, h.hit_normal, h.hit_pos, -1, 0.0f, orient_override);
    return true;
}

// 0x00BE79A0 (:2574)
// Throws the weapon: a non-inventory item (a held object) leaves the hand with the velocity and a random spin, and the
// player re-equips its last weapon; anything else creates a projectile (on the server, or on the thrower's client for
// the weapons clients create, with a client create id), mines and singularity bombs are planted at once. A thrown
// round is used up (players and, without Unlimited_ai_thrown_weapons, marauders and other npcs).
projectile* weapon::launch_thrown_weapon(const vector& start_pos, const matrix& start_orient, const vector& start_velocity)
{
    projectile* proj = nullptr;
    human* owner = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
    }
    const bool client_creates = Multiplayer == true && !multi_is_server() && multi_game_client_creates_weapon(w_info) &&
                                player_get_local_player()->get_handle() == get_owner_handle();
    const bool server_creates = Multiplayer == true && multi_is_server() &&
                                (!multi_game_client_creates_weapon(w_info) || player_get_local_player()->get_handle() == get_owner_handle());
    bool uses_ammo = false;
    if (!Multiplayer || object_is_local_player(owner))
    {
        if (object_is_player(owner))
        {
            uses_ammo = !has_unlimited_ammo() && !has_unlimited_magazine_ammo();
        }
        else if (owner->current_team != HUMAN_TEAM_MARAUDER)
        {
            uses_ammo = !Unlimited_ai_thrown_weapons;
        }
        else if (!Unlimited_ai_thrown_weapons)
        {
            uses_ammo = !has_unlimited_ammo() && !has_unlimited_magazine_ammo();
        }
    }
    if (!Multiplayer || server_creates || client_creates)
    {
        obj_item_flags proj_item_flags;
        memset(&proj_item_flags, 0, sizeof(proj_item_flags));
        proj_item_flags.cast_drop_shadows = 1;
        proj_item_flags.cast_shadows = 1;
        proj_item_flags.simulate = 1;
        if (w_info->flags.non_inventory)
        {
            inventory_item* ip = owner->get_equipped_inv_item();
            owner->set_equipped_inv_item(nullptr);
            if (owner->desired_equipped_inv_item == ip)
            {
                owner->desired_equipped_inv_item = nullptr;
            }
            enable_physics();
            object_set_visibility_plus_children(this, true);
            last_fired_timer.set(0);
            matrix new_orient(start_orient);
            const vector back = -start_velocity.copy_normalize();
            new_orient.make_ruvec(Rfg_camera.mode == CAMERA_SLEW_MODE ? owner->orient.rvec : Rfg_camera.real_orient.rvec, back);
            update_pos_and_orient(pos, new_orient, true);
            // Magic numbers: a spin of up to 0.25 around each axis.
            const float spin_z = fl_rand_range(-0.25f, 0.25f);
            const float spin_y = fl_rand_range(-0.25f, 0.25f);
            const vector spin(fl_rand_range(-0.25f, 0.25f), spin_y, spin_z);
            human_remove_item_from_inventory(owner, ip, ip->count, false);
            havok_body_apply_linear_velocity(havok_handle, start_velocity);
            havok_body_apply_angular_impulse(havok_handle, spin);
            if (!object_is_player(owner))
            {
                return nullptr;
            }
            player* pp = owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)owner : nullptr;
            inventory_item* next_item = pp->desired_equipped_inv_item;
            if (next_item == nullptr)
            {
                next_item = pp->last_equipped_weapon;
            }
            else if (next_item == pp->last_equipped_weapon)
            {
                pp->last_equipped_weapon = pp->second_last_equipped_weapon;
                pp->second_last_equipped_weapon = nullptr;
            }
            player_equip_item(pp, next_item, nullptr, false);
            return nullptr;
        }
        if (uses_ammo && rounds_in_reserve + rounds_in_magazine < 1)
        {
            return nullptr;
        }
        unsigned int create_id = 0xFFFFFFFF;
        if (client_creates)
        {
            multi_game_get_client_create_unique_id(create_id);
        }
        obj_projectile_props props(owner->get_handle(), w_info->projectile_info.obj_item_info, proj_item_flags, w_info, start_pos, start_orient, start_velocity, ZERO_VECTOR, create_id);
        proj = World.create_object<projectile>(props, 0xFFFFFFFF);
        if (proj != nullptr)
        {
            owner->combat.last_projectile_handle = proj->get_handle();
            // Action 0x271: the thrower's charge arming; the projectile plays state 0x66.
            if (proj->char_inst != nullptr && owner->char_inst->is_playing_action((animation_action)0x271))
            {
                proj->char_inst->set_state(index_alias(0x66, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.0f, false);
            }
            const weapon_class_type weapon_class = w_info->weapon_class;
            if (weapon_class == WEAPON_CLASS_THROWN_MINE || weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                vector head = ZERO_VECTOR;
                human_get_head_pos(owner, head);
                const vector feet(head.x, owner->pos.y, head.z);
                weapon_projectile_throw_collide(head, feet, proj);
            }
        }
    }
    if (uses_ammo)
    {
        if (rounds_in_magazine + rounds_in_reserve < 1)
        {
            return nullptr;
        }
        if (rounds_in_reserve > 0)
        {
            rounds_in_reserve--;
            return proj;
        }
        rounds_in_magazine--;
    }
    return proj;
}

// 0x00BE1C30 (:2271)
// Creates the projectile with the start speed along the shot plus the owner's velocity across it (a vehicle rider gets
// the recoil impulse). In multiplayer only on the machine that creates it (the server, or the shooter's client for
// the weapons clients create). Multi rocket launchers (single player) and the grinder lift the shot; a player's multi
// rocket shot takes its rockets out of the magazine.
void weapon::fire_projectile(const vector& start, const vector& end)
{
    vector dir;
    vec_normalized_dir(dir, end, start);
    matrix proj_orient;
    proj_orient.make_quick(dir);
    const float speed = w_info->projectile_info.start_speed;
    vector velocity(dir.x * speed, dir.y * speed, dir.z * speed);
    // Projectile flag 0x4000: the projectile keeps the weapon's orientation.
    if ((w_info->projectile_info.flags & 0x4000) != 0)
    {
        proj_orient = orient;
    }
    human* owner = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
    }
    const bool local = object_is_local_player(owner);
    const bool client_creates = Multiplayer == true && !multi_is_server() && local && multi_game_client_creates_weapon(w_info);
    const bool server_creates = Multiplayer == true && multi_is_server() && (local || !multi_game_client_creates_weapon(w_info));
    if (owner != nullptr && owner->vehicle_handle != 0xFFFFFFFF)
    {
        const float recoil = -w_info->recoil_impulse;
        const vector impulse(recoil * dir.x, dir.y * recoil, dir.z * recoil);
        object* vp = World.object_from_handle(owner->vehicle_handle);
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE && vp->havok_handle != 0xFFFFFFFF)
        {
            havok_body_apply_point_impulse(vp->havok_handle, impulse, start);
        }
    }
    vector owner_vel = ZERO_VECTOR;
    if (owner != nullptr)
    {
        if (owner->vehicle_handle == 0xFFFFFFFF)
        {
            human_controller_get_vel(owner, owner_vel);
        }
        else
        {
            vehicle_get_velocity(owner->vehicle_handle, owner_vel);
        }
    }
    // Original: the owner is not null-checked here.
    if (owner->char_inst->get_animation_group() == ANIMATION_GROUP_THERMOBARIC_ROCKET_LAUNCHER)
    {
        // The thermobaric launcher's loaded rocket (a child item whose field after the object base is 1) goes.
        object* child = child_ptr;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_ITEM && *(const int*)((const char*)child + sizeof(object)) == 1)
            {
                World.destroy_object(child, false);
            }
            child = child->child_next;
            if (child == child_ptr)
            {
                break;
            }
        }
    }
    line shot;
    shot.point = start;
    shot.dir = dir;
    const vector moved(start.x + owner_vel.x, start.y + owner_vel.y, start.z + owner_vel.z);
    vector on_shot;
    ix_closest_point_on_ray(moved, shot, on_shot);
    const vector across(on_shot.x - start.x, on_shot.y - start.y, on_shot.z - start.z);
    velocity = vector(across.x + velocity.x, across.y + velocity.y, across.z + velocity.z);
    if (Multiplayer && !server_creates && !client_creates)
    {
        return;
    }
    obj_item_flags proj_item_flags;
    memset(&proj_item_flags, 0, sizeof(proj_item_flags));
    proj_item_flags.cast_drop_shadows = 1;
    proj_item_flags.cast_shadows = 1;
    proj_item_flags.simulate = 1;
    unsigned int create_id = 0xFFFFFFFF;
    if (client_creates)
    {
        multi_game_get_client_create_unique_id(create_id);
    }
    matrix start_orient(proj_orient);
    bool lift_orient = false;
    float orient_lift = 0.0f;
    float velocity_lift = 0.0f;
    bool lift = true;
    if ((w_info->projectile_info.flags & 1) == 0 || Multiplayer)
    {
        if (w_info->weapon_class != WEAPON_CLASS_GRINDER)
        {
            lift = false;
        }
        else
        {
            // Magic numbers: the grinder's blade leaves 0.02 rad up, the blade turned 0.3 rad.
            velocity_lift = -0.02f;
            orient_lift = -0.3f;
            lift_orient = true;
        }
    }
    else
    {
        // Magic number: single player multi rockets leave 0.2 rad up.
        velocity_lift = -0.2f;
    }
    if (lift)
    {
        const float speed_now = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
        matrix m;
        m.make(velocity);
        m.rotate_around_local_x(velocity_lift);
        velocity = vector(m.fvec.x * speed_now, m.fvec.y * speed_now, m.fvec.z * speed_now);
        if (lift_orient)
        {
            start_orient.rotate_around_local_x(orient_lift);
        }
    }
    obj_projectile_props props(get_owner_handle(), w_info->projectile_info.obj_item_info, proj_item_flags, w_info, start, start_orient, velocity, end, create_id);
    projectile* proj = World.create_object<projectile>(props, 0xFFFFFFFF);
    if (proj != nullptr)
    {
        object* owner_objp = World.object_from_handle(get_owner_handle());
        if (object_is_player(owner_objp) && (proj->w_info->projectile_info.flags & 1) != 0 && upgrade_get_multi_rockets() > 1 &&
            rounds_in_magazine < proj->get_secondary_rockets_to_fire())
        {
            proj->set_secondary_rockets_to_fire(rounds_in_magazine);
        }
        if (!has_unlimited_magazine_ammo())
        {
            rounds_in_magazine -= proj->get_secondary_rockets_to_fire();
        }
        owner->combat.last_projectile_handle = proj->get_handle();
    }
}

// 0x00BE2310 (:2429)
// The reconstructor: repairs destruction around the point (on the server) and damages the other players there.
void weapon::fire_repair(const vector& repair_pos)
{
    human* owner = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner != nullptr && (owner->get_obj_type() != OT_HUMAN || owner->get_sub_type() != OT_SUB_HUMAN_PLAYER))
        {
            owner = nullptr;
        }
    }
    if (!Multiplayer || multi_is_server())
    {
        rfg_dyn_repair_sphere(repair_pos, Repair_radius, (int)(w_info->default_refire_delay * 1000.0f), owner);
    }
    else
    {
        object_is_local_player(owner); // result unused in rfg.exe
    }
    unsigned int handles[16];
    const int count = World.collect_objects<player>(repair_pos, Repair_radius, handles, 16, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        if (handles[i] == 0xFFFFFFFF)
        {
            continue;
        }
        human* hp = (human*)World.object_from_handle(handles[i]);
        if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER || hp == owner || !multi_gw_can_hit_target(owner, hp))
        {
            continue;
        }
        const float damage = human_damage(hp, owner, w_info->high_scale_damage.damage[1], DBT_BULLET, w_info, nullptr, true, false);
        if (object_is_local_player(hp) && !hp->is_killed())
        {
            camera_shake_play(w_info->secondary_camera_shake, 1.0f, false);
            human_combat_process_hit_indicator(hp, owner != nullptr ? &owner->pos : nullptr, nullptr, damage);
        }
    }
}

// 0x00BE24E0 (:2485): the flamethrower damages and ignites the humans in its flame.
void weapon::fire_flamethrower(const vector& flame_pos)
{
    human* attacker = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        attacker = (human*)World.object_from_handle(get_owner_handle());
        if (attacker != nullptr && (attacker->get_obj_type() != OT_HUMAN || attacker->get_sub_type() != OT_SUB_HUMAN_PLAYER))
        {
            attacker = nullptr;
        }
    }
    unsigned int handles[16];
    const int count = World.collect_objects<human>(flame_pos, Flame_radius, handles, 16, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        if (handles[i] == 0xFFFFFFFF)
        {
            continue;
        }
        human* hp = (human*)World.object_from_handle(handles[i]);
        if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp == attacker || !multi_gw_can_hit_target(attacker, hp))
        {
            continue;
        }
        const float damage = human_damage(hp, attacker, w_info->high_scale_damage.damage[0], DBT_BULLET, w_info, nullptr, true, false);
        if (localization_allow_humans_on_fire())
        {
            human_fire_ignite(hp, attacker);
        }
        if (object_is_local_player(hp) && !hp->is_killed())
        {
            camera_shake_play(w_info->secondary_camera_shake, 1.0f, false);
            human_combat_process_hit_indicator(hp, attacker != nullptr ? &attacker->pos : nullptr, nullptr, damage);
        }
    }
}

// 0x00BE65A0 (:4600)
// The fire sound (the local player's plainly, others' 3d through their weapon persona) and the muzzle flash; a new
// shot stops the muzzle smoke.
void weapon::play_single_fire_effects()
{
    if (!w_info->is_firearm())
    {
        return;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    get_muzzle_transform(muzzle_pos, &muzzle_orient);
    if (object_is_local_player(get_owner_handle()) && !Camera_third_person_lock)
    {
        audiolib_cue_play(w_info->fire_sound, nullptr, nullptr);
    }
    else
    {
        if (weapon_persona == -1 && w_info->num_weapon_personas > 0)
        {
            set_new_weapon_persona(0xFFFFFFFF);
        }
        if (weapon_persona != -1)
        {
            audiolib_cue_play_3d(w_info->npc_fire_sounds[weapon_persona], &muzzle_pos, &muzzle_orient.fvec, nullptr, nullptr);
        }
    }
    object_effect* flash = nullptr;
    if (muzzle_flash_effect_handle != 0xFFFFFFFF)
    {
        flash = (object_effect*)World.object_from_handle(muzzle_flash_effect_handle);
        if (flash != nullptr && flash->get_obj_type() != OT_EFFECT)
        {
            flash = nullptr;
        }
    }
    object_effect* smoke = nullptr;
    if (muzzle_smoke_effect_handle != 0xFFFFFFFF)
    {
        smoke = (object_effect*)World.object_from_handle(muzzle_smoke_effect_handle);
        if (smoke != nullptr && smoke->get_obj_type() != OT_EFFECT)
        {
            smoke = nullptr;
        }
    }
    if (render_alpha == 1.0f || casts_transparent_shadows)
    {
        if (!can_play_muzzle_flash() || w_info->muzzle_flash_effect == 0xFFFFFFFF)
        {
            return;
        }
        if (!w_info->flags.looping_effects || flash == nullptr)
        {
            if (!w_info->flags.looping_effects && flash != nullptr)
            {
                flash->stop(object_effect::SS_STOP_EMITTING);
            }
            muzzle_flash_effect_handle = effects_play(w_info->muzzle_flash_effect, muzzle_pos, muzzle_orient, w_info->flags.looping_effects, get_handle(), muzzle_tag_index,
                                                      0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        muzzle_smoke_timestamp.set(200);
    }
    if (smoke != nullptr)
    {
        smoke->stop(object_effect::SS_STOP_EMITTING);
        muzzle_smoke_effect_handle = 0xFFFFFFFF;
    }
}

// 0x00BE8CF0 (:4512)
// Per shot: the fire effects (or the looping ones), the weapon's fire animation, for the local player the camera shake,
// the scope blur and the recoil kick (halved when zoomed; the heading kick's range is 0), npcs hear the shot (players'
// every 333 ms, npcs' every second), and the muzzle flash light.
void weapon::play_individual_fire_effects()
{
    human* owner = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
    }
    if (w_info->trigger_type == WEAPON_TRIGGER_TYPE_SINGLE || !w_info->flags.looping_effects)
    {
        play_single_fire_effects();
    }
    else if (muzzle_flash_effect_handle == 0xFFFFFFFF && (render_alpha == 1.0f || casts_transparent_shadows))
    {
        start_looping_effects();
    }
    if (rdd.is_loaded() && char_inst != nullptr)
    {
        char_inst->play_action(ANIMATION_ACTION_STAND_FIRE, ANIM_ALIAS_FIRE, 1.0f, 0);
    }
    if (object_is_local_player(owner))
    {
        player* pp = owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)owner : nullptr;
        if (w_info->fire_camera_shake != nullptr)
        {
            camera_shake_play(w_info->fire_camera_shake, 1.0f, w_info->fire_camera_shake_ignore_disabled);
        }
        if (player_is_scoped(pp))
        {
            camera_view_do_scoped_fire_blur();
        }
        float kick = w_info->recoil_camera_kick;
        if (human_in_vehicle(owner) == nullptr && kick > 0.0f)
        {
            if (player_is_zoomed(pp))
            {
                kick *= 0.5f;
            }
            const float pitch_added = Rfg_camera.lookaround_params.rotvel_added_p;
            Rfg_camera.lookaround_params.rotvel_added_p = pitch_added - fl_rand_range(0.07f, 0.13f) * kick;
            Rfg_camera.lookaround_params.rotvel_added_h = fl_rand_range(-0.0f, 0.0f) * kick + Rfg_camera.lookaround_params.rotvel_added_h;
            if (!Rfg_camera.lookaround_params.recoiling)
            {
                Rfg_camera.lookaround_params.recoiling = true;
                Rfg_camera.lookaround_params.recoil_start_pitch = Rfg_camera.lookaround_params.pitch;
            }
        }
    }
    if (owner != nullptr && w_info->firing_sound_radius > 0.0f && weapon_fire_sound_timestamp.elapsed())
    {
        npc_sound_info_firearm sound(owner, w_info);
        npc_sound_info::queue<npc_sound_info_firearm>(sound);
        // Magic numbers: a player's shots are heard every 333 ms, an npc's every second.
        weapon_fire_sound_timestamp.set(object_is_player(owner) ? 333 : 1000);
    }
    muzzle_smoke_timestamp.set(200);
    if (can_play_muzzle_flash())
    {
        muzzle_flash_intensity = 1.8f;
    }
}

// 0x00BEAAF0 (:2997)
// Where a shot starts and goes. Players fire from the camera (the local one's, a remote one's replicated camera) moved
// out to the player's distance, towards the camera's facing; while blind firing, from the muzzle. Without
// Fire_bullets_from_camera a scoped player fires from the camera too, others from the muzzle towards the aim point. A
// turret's weapon aims along its muzzle, except for the local player in turret mode, who aims where the camera's line to
// the aim point hits.
void weapon::get_adjusted_muzzle_transform(vector& adjusted_muzzle_pos, vector& adjusted_muzzle_fvec, vector& muzzle_pos) const
{
    matrix muzzle_orient;
    get_muzzle_transform(muzzle_pos, &muzzle_orient);
    player* pp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        pp = (player*)World.object_from_handle(get_owner_handle());
        if (pp != nullptr && (pp->get_obj_type() != OT_HUMAN || pp->get_sub_type() != OT_SUB_HUMAN_PLAYER))
        {
            pp = nullptr;
        }
    }
    vector aim;
    auto from_camera = [&](const vector& cam_pos, const vector& cam_fvec, bool copy_normalize) {
        adjusted_muzzle_pos = cam_pos;
        adjusted_muzzle_fvec = copy_normalize ? cam_fvec.copy_normalize_safe() : cam_fvec;
        const float fx = adjusted_muzzle_fvec.x;
        const float fz = adjusted_muzzle_fvec.z;
        const float px = pp->pos.x - adjusted_muzzle_pos.x;
        const float pz = pp->pos.z - adjusted_muzzle_pos.z;
        const float dist = sqrtf(pz * pz + px * px);
        const float flat = sqrtf(fx * fx + fz * fz);
        adjusted_muzzle_pos = vector(adjusted_muzzle_pos.x + (fx * dist) / flat, adjusted_muzzle_pos.y + (adjusted_muzzle_fvec.y * dist) / flat, adjusted_muzzle_pos.z + (fz * dist) / flat);
        adjusted_muzzle_fvec.normalize_safe();
    };
    const bool blind_firing = pp == nullptr || (!pp->combat.combat_blindfire_timestamp.elapsed() && pp->combat.combat_blindfire_timestamp.valid());
    if (!Fire_bullets_from_camera)
    {
        if (blind_firing)
        {
            adjusted_muzzle_pos = muzzle_pos;
            adjusted_muzzle_fvec = muzzle_orient.fvec;
            adjusted_muzzle_fvec.normalize_safe();
            return;
        }
        if (object_is_local_player(pp) && Rfg_camera.mode == CAMERA_SLEW_MODE)
        {
            adjusted_muzzle_pos = muzzle_pos;
            adjusted_muzzle_fvec = muzzle_orient.fvec.copy_normalize_safe();
            return;
        }
        if (player_is_scoped(pp))
        {
            if (object_is_local_player(pp))
            {
                from_camera(Rfg_camera.real_pos, Rfg_camera.real_orient.fvec, false);
            }
            else
            {
                from_camera(pp->mp_camera_pos, pp->mp_camera_orient.fvec, false);
            }
            return;
        }
        adjusted_muzzle_pos = muzzle_pos;
        if (turret_handle != 0xFFFFFFFF)
        {
            adjusted_muzzle_fvec = muzzle_orient.fvec;
            adjusted_muzzle_fvec.normalize_safe();
            return;
        }
        aim = pp->aim_pos - muzzle_pos;
    }
    else
    {
        if (blind_firing)
        {
            adjusted_muzzle_pos = muzzle_pos;
            adjusted_muzzle_fvec = muzzle_orient.fvec;
            adjusted_muzzle_fvec.normalize_safe();
            return;
        }
        if (w_info->ammo_type == WEAPON_AMMO_TYPE_BULLET && turret_handle == 0xFFFFFFFF)
        {
            if (object_is_local_player(pp))
            {
                from_camera(Rfg_camera.real_pos, Rfg_camera.real_orient.fvec, true);
            }
            else
            {
                from_camera(pp->mp_camera_pos, pp->mp_camera_orient.fvec, true);
            }
            return;
        }
        adjusted_muzzle_pos = muzzle_pos;
        object* turret_objp = nullptr;
        if (turret_handle != 0xFFFFFFFF)
        {
            turret_objp = World.object_from_handle(turret_handle);
            if (turret_objp != nullptr && turret_objp->get_obj_type() != OT_TURRET)
            {
                turret_objp = nullptr;
            }
        }
        vehicle* vp = human_in_vehicle(pp);
        if (turret_objp == nullptr)
        {
            aim = pp->aim_pos - muzzle_pos;
        }
        else
        {
            if (!object_is_local_player(pp) || (camera_get_mode() != CAMERA_TURRET_MODE && (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)))
            {
                adjusted_muzzle_fvec = muzzle_orient.fvec;
                adjusted_muzzle_fvec.normalize_safe();
                return;
            }
            vector closest;
            ix_closest_point_on_line_segment(closest, muzzle_pos, Rfg_camera.real_pos, pp->aim_pos);
            weapon_fire_info aim_info(closest, pp->aim_pos, ((turret*)turret_objp)->t_info->w_info);
            if (!weapons_test_bullet(aim_info, pp, true, 0.0f, ZERO_VECTOR))
            {
                adjusted_muzzle_fvec = muzzle_orient.fvec;
                adjusted_muzzle_fvec.normalize_safe();
                return;
            }
            aim = aim_info.hit_pos - muzzle_pos;
        }
    }
    adjusted_muzzle_fvec = aim;
    adjusted_muzzle_fvec.normalize_safe();
}

// 0x00BEB270 (:4220)
// One round: the timers, the refire delay, overheating (with its effect), the spread recoil (enemies of the local player
// spread more by difficulty) and the magazine. Each shot of the round leaves the adjusted muzzle along a random
// direction in the current spread (shotguns spread their shots evenly around the centre); a forced aim direction near
// the muzzle's replaces the first. Then the attack is registered and the fire effects play.
void weapon::fire(vector aim_override)
{
    last_fired_timer.set(0);
    num_rounds_fired++;
    refire_delay_time = (float)((double)refire_delay_time - (double)get_weapon_info_refire_delay());
    overheat_percent = w_info->overheat_percent_per_shot + overheat_percent;
    if ((stats_cheat_enabled(CHEATS_COOL_TURRETS) || wrecking_crew_is_active()) && turret_handle != 0xFFFFFFFF)
    {
        object* turret_objp = World.object_from_handle(turret_handle);
        if (turret_objp != nullptr && turret_objp->get_obj_type() == OT_TURRET)
        {
            overheat_percent = 0.0f;
        }
    }
    float heat = overheat_percent;
    if (overheat_percent - 0.0f < 0.0f)
    {
        heat = 0.0f;
    }
    if (heat - 1.0f >= 0.0f)
    {
        heat = 1.0f;
    }
    overheat_percent = heat;
    // Only set when the weapon overheats; fire_electricity receives it and does not use it.
    vector overheat_muzzle_pos = ZERO_VECTOR;
    if (heat == 1.0f)
    {
        overheated = true;
        if (w_info->overheated_effect != 0xFFFFFFFF)
        {
            matrix muzzle_orient;
            get_muzzle_transform(overheat_muzzle_pos, &muzzle_orient);
            overheated_effect_handle = effects_play(w_info->overheated_effect, overheat_muzzle_pos, muzzle_orient, true, get_handle(), muzzle_tag_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            if (overheated_effect_handle != 0xFFFFFFFF)
            {
                object_effect* effect = (object_effect*)World.object_from_handle(overheated_effect_handle);
                if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
                {
                    effect->set_expression_variable(EFFECTS_VAR_WEAPON_HEAT_PERCENT, 1.0f);
                }
            }
        }
    }
    if (Weapon_spread_recoil && w_info->to_max_spread != 0)
    {
        float min_spread;
        float max_spread;
        get_spread(min_spread, max_spread);
        float spread = (max_spread - min_spread) / (float)w_info->to_max_spread + current_spread;
        current_spread = spread;
        if (spread - min_spread < 0.0f)
        {
            spread = min_spread;
        }
        if (spread - max_spread >= 0.0f)
        {
            spread = max_spread;
        }
        current_spread = spread;
        if (get_owner_handle() != 0xFFFFFFFF)
        {
            human* hp = (human*)World.object_from_handle(get_owner_handle());
            if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && player_get_local_player() != nullptr && !local_player_is_friendly(hp))
            {
                current_spread = difficulty_get_value(DIFFICULTY_ENEMY_WEAPON_SPREAD, NUM_DIFFICULTY_LEVELS) * current_spread;
            }
        }
    }
    if (!has_unlimited_magazine_ammo())
    {
        rounds_in_magazine--;
    }
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        player* pp = (player*)World.object_from_handle(get_owner_handle());
        if (pp != nullptr && pp->get_obj_type() == OT_HUMAN && pp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            pp->last_fire_time.set(0);
            if (rounds_in_reserve + rounds_in_magazine == 0 && player_is_scoped(pp))
            {
                player_set_zoom_state(pp, PZS_OFF, false);
            }
        }
    }
    vector shot_pos;
    vector shot_fvec;
    vector muzzle_pos;
    get_adjusted_muzzle_transform(shot_pos, shot_fvec, muzzle_pos);
    const float angle = fl_rand_range(0.0f, PI2);
    const float radius = fl_rand_range(0.0f, current_spread);
    const float cos_r = (float)cos((double)angle) * radius;
    const float sin_r = (float)sin((double)angle) * radius;
    const matrix spread_m(sin_r, 0.0f, cos_r);
    vector shot_dir = shot_fvec.copy_unrotate(spread_m);
    const bool skip_first = w_info->shots_per_round > 3;
    float even_rotation = 0.0f;
    vector even_center;
    if (w_info->flags.use_even_spread && w_info->fire_cone_dot < 1.0f)
    {
        even_center = shot_dir;
        even_rotation = fl_rand_range(0.0f, PI_OVER_2);
        shot_dir.rotate_around_vector(orient.rvec, -fl_acos(w_info->fire_cone_dot));
    }
    for (int i = 0; i < (int)w_info->shots_per_round; i++)
    {
        vector dir = shot_dir;
        float cone_dot = w_info->fire_cone_dot;
        if (cone_dot < 1.0f)
        {
            if (w_info->flags.use_even_spread)
            {
                if (i == 0 && skip_first)
                {
                    dir = even_center;
                }
                else
                {
                    dir.rotate_around_vector(even_center, ((float)i * PI2) / (float)(int)((unsigned int)w_info->shots_per_round - (unsigned int)skip_first) + even_rotation);
                }
                cone_dot = w_info->even_spread_accuracy_dot;
            }
            dir.rand_around_dir(dir, cone_dot, 1.0f);
        }
        if (aim_override.x != ZERO_VECTOR.x || aim_override.y != ZERO_VECTOR.y || aim_override.z != ZERO_VECTOR.z)
        {
            if (AIM_OVERRIDE_DOT <= shot_fvec.x * aim_override.x + shot_fvec.y * aim_override.y + shot_fvec.z * aim_override.z)
            {
                dir = aim_override;
            }
            aim_override = ZERO_VECTOR;
        }
        switch (w_info->ammo_type)
        {
        case WEAPON_AMMO_TYPE_BULLET:
            if (w_info->flags.can_scope && !get_tag_world_coords(muzzle_tag_index, muzzle_pos, nullptr, nullptr, nullptr))
            {
                muzzle_pos = pos;
            }
            // Magic number: bullets reach 250 m.
            if (w_info->flags.penetrating_bullets)
            {
                const vector end(dir.x * 250.0f + shot_pos.x, shot_pos.y + dir.y * 250.0f, shot_pos.z + dir.z * 250.0f);
                fire_penetrating_bullet(shot_pos, end, muzzle_pos);
            }
            else if (Fire_bullets_from_camera || w_info->flags.can_scope)
            {
                const vector end(dir.x * 250.0f + shot_pos.x, dir.y * 250.0f + shot_pos.y, dir.z * 250.0f + shot_pos.z);
                fire_bullet(shot_pos, end, muzzle_pos);
            }
            else
            {
                shot_pos = vector(shot_pos.x - shot_dir.x * 0.6f, shot_pos.y - shot_dir.y * 0.6f, shot_pos.z - shot_dir.z * 0.6f);
                const vector end(dir.x * 250.0f + shot_pos.x, dir.y * 250.0f + shot_pos.y, dir.z * 250.0f + shot_pos.z);
                fire_bullet(shot_pos, end, shot_pos);
            }
            break;
        case WEAPON_AMMO_TYPE_PROJECTILE:
        {
            object* owner_objp = get_owner_handle() != 0xFFFFFFFF ? World.object_from_handle(get_owner_handle()) : nullptr;
            if (owner_objp == nullptr || owner_objp->get_obj_type() != OT_HUMAN || owner_objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
            {
                const vector end(dir.x * 250.0f + shot_pos.x, dir.y * 250.0f + shot_pos.y, dir.z * 250.0f + shot_pos.z);
                fire_projectile(shot_pos, end);
            }
            else
            {
                // A player's projectile flies as far as its aim point (a turret's aim position for a turret weapon).
                object* turret_objp = turret_handle != 0xFFFFFFFF ? World.object_from_handle(turret_handle) : nullptr;
                if (turret_objp != nullptr && turret_objp->get_obj_type() != OT_TURRET)
                {
                    turret_objp = nullptr;
                }
                vector target = ((player*)owner_objp)->aim_pos;
                if (turret_objp != nullptr)
                {
                    ((turret*)turret_objp)->get_target_pos(target);
                }
                const float dist = sqrtf((target.x - shot_pos.x) * (target.x - shot_pos.x) + (target.y - shot_pos.y) * (target.y - shot_pos.y) +
                                         (target.z - shot_pos.z) * (target.z - shot_pos.z));
                const vector end(dir.x * dist + shot_pos.x, dir.y * dist + shot_pos.y, dir.z * dist + shot_pos.z);
                fire_projectile(shot_pos, end);
            }
            break;
        }
        case WEAPON_AMMO_TYPE_ELECTRICITY:
            fire_electricity(shot_pos, overheat_muzzle_pos);
            break;
        case WEAPON_AMMO_TYPE_REPAIR:
        {
            const vector repair_pos(shot_fvec.x * Repair_range + shot_pos.x, shot_fvec.y * Repair_range + shot_pos.y, shot_fvec.z * Repair_range + shot_pos.z);
            fire_repair(repair_pos);
            break;
        }
        case WEAPON_AMMO_TYPE_FLAME:
        {
            overheat_muzzle_pos = vector(shot_fvec.x * Flame_range + shot_pos.x, shot_fvec.y * Flame_range + shot_pos.y, shot_fvec.z * Flame_range + shot_pos.z);
            fire_flamethrower(overheat_muzzle_pos);
            break;
        }
        default:
            break;
        }
    }
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        player* pp = (player*)World.object_from_handle(get_owner_handle());
        if (pp != nullptr && pp->get_obj_type() == OT_HUMAN && pp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            const weapon_ammo_type ammo = w_info->ammo_type;
            gameplay_register_player_attack(pp, w_info, ammo == WEAPON_AMMO_TYPE_PROJECTILE || ammo == WEAPON_AMMO_TYPE_THROWN ? DBT_NONE : DBT_BULLET, false);
            if (Multiplayer && object_is_local_player(pp))
            {
                multi_game_stats_record_shot(pp, w_info, false);
            }
            player_fires_weapon_or_detonates_event event;
            gem_post_event(event);
        }
    }
    play_individual_fire_effects();
    if (cb[WEAPON_CB_IDX_POST_FIRE].func != nullptr)
    {
        cb[WEAPON_CB_IDX_POST_FIRE].func(WEAPON_CB_IDX_POST_FIRE, this, cb[WEAPON_CB_IDX_POST_FIRE].user_handle);
    }
}

// 0x00BEC0F0 (:4446)
// Single player multi rockets (the upgrade): holding the trigger queues more rockets every 500 ms (with a sound and a
// camera shake) up to the upgrade's count or the magazine; on release or when full, the rockets go.
bool weapon::process_multi_rocket_trigger_down()
{
    if (rounds_in_magazine < 1 && (rounds_in_reserve < 1 || w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN) && !has_unlimited_magazine_ammo())
    {
        return false;
    }
    bool fire_now = false;
    if (multi_rocket_trigger_down_timer.elapsed())
    {
        const int queued = secondary_rockets_to_fire;
        if (queued == upgrade_get_multi_rockets() - 1 || queued == rounds_in_magazine - 1)
        {
            fire_now = true;
        }
    }
    if (!fire_now && rounds_in_magazine != 1 && controls_is_button_down(CBA_OFC_ATTACK_PRIMARY, Player_controller_index))
    {
        if (multi_rocket_trigger_down_timer.valid())
        {
            if (multi_rocket_trigger_down_timer.elapsed())
            {
                secondary_rockets_to_fire++;
                multi_rocket_trigger_down_timer.invalidate();
            }
            flags.delay_fire = true;
            return false;
        }
        audiolib_cue_play_3d(w_info->special_sound, &pos, nullptr, nullptr, nullptr);
        if (w_info->secondary_camera_shake != nullptr)
        {
            camera_shake_play(w_info->secondary_camera_shake, 1.0f, false);
        }
        multi_rocket_trigger_down_timer.set(500);
        flags.delay_fire = true;
        return false;
    }
    if (!ready_to_fire())
    {
        if (flags.trigger_down && cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func != nullptr)
        {
            cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func(WEAPON_CB_IDX_RELEASE_TRIGGER, this, cb[WEAPON_CB_IDX_RELEASE_TRIGGER].user_handle);
        }
        flags.trigger_down = false;
        num_rounds_fired = 0;
        secondary_rockets_to_fire = 0;
        multi_rocket_trigger_down_timer.invalidate();
        return true;
    }
    if (!flags.trigger_down)
    {
        if (cb[WEAPON_CB_IDX_PRESS_TRIGGER].func != nullptr)
        {
            cb[WEAPON_CB_IDX_PRESS_TRIGGER].func(WEAPON_CB_IDX_PRESS_TRIGGER, this, cb[WEAPON_CB_IDX_PRESS_TRIGGER].user_handle);
        }
        refire_delay_time = refire_delay_time - w_info->prefire_delay;
    }
    flags.trigger_down = true;
    fire(ZERO_VECTOR);
    secondary_rockets_to_fire = 0;
    multi_rocket_trigger_down_timer.invalidate();
    return true;
}

// 0x00BE15F0 (:1255)
// A dropped weapon holds 0.5-1.5 of a magazine (scaled by the weapon's dropped ammo scale, at least one round) in the
// magazine and the rest of the difficulty's pickup amount in reserve.
void weapon::adjust_dropped_ammo()
{
    const int total = (int)difficulty_get_value(DIFFICULTY_PLAYER_AMMO_PICKUP, NUM_DIFFICULTY_LEVELS);
    const float scale = fl_rand_range(0.5f, 1.5f);
    const float dropped_scale = w_info->dropped_ammo_scale;
    int magazine = (int)((float)get_magazine_size() * scale * dropped_scale);
    if (magazine < 1)
    {
        magazine = 1;
    }
    rounds_in_magazine = magazine;
    const int magazine_size = get_magazine_size();
    if (rounds_in_magazine < 0)
    {
        rounds_in_magazine = 0;
    }
    else if (magazine_size < rounds_in_magazine)
    {
        rounds_in_magazine = magazine_size;
    }
    rounds_in_reserve = total - magazine;
    const int max_rounds = get_max_rounds();
    const int size = get_magazine_size();
    if (rounds_in_reserve < 0)
    {
        rounds_in_reserve = 0;
        return;
    }
    if (max_rounds - size < rounds_in_reserve)
    {
        rounds_in_reserve = max_rounds - size;
    }
}

// 0x00BDD060 (:5129)
// A thrown non-inventory item's impact: without a collision event (a human hit) the human takes 100 x the multiplier
// (a quarter when shielded) from the local player, unless it is the local player; with one an rfg mover hit takes it
// as level object damage at the contact (from the owner when that is a human).
void weapon::collision_damage(const collision_event* event, object* other_obj, float damage_multiplier)
{
    if (event == nullptr)
    {
        human* hp = other_obj != nullptr && other_obj->get_obj_type() == OT_HUMAN ? (human*)other_obj : nullptr;
        player* attacker = player_get_local_player();
        if (hp == nullptr || hp == attacker || attacker == nullptr)
        {
            return;
        }
        float damage = damage_multiplier * 100.0f; // Magic number: 100 hit points a unit
        if (hp->is_shielded(pos, false))
        {
            damage = damage * 0.25f; // Magic number: a quarter through a shield
        }
        human_damage(hp, attacker, damage, DBT_NONE, w_info, attacker, true, false);
        return;
    }
    object_mover* mover = other_obj != nullptr && other_obj->get_obj_type() == OT_MOVER ? (object_mover*)other_obj : nullptr;
    const human* owner = nullptr;
    if (owner_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(owner_handle);
        owner = objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (const human*)objp : nullptr;
    }
    if (mover == nullptr)
    {
        return;
    }
    const collision_event::collision_info& collision = event->m_collisions[0];
    unsigned int shape_key = 0xFFFFFFFF;
    if (event->m_object_a == this && event->m_object_b != nullptr)
    {
        shape_key = collision.m_shapekey_b;
    }
    else if (event->m_object_a != nullptr)
    {
        shape_key = collision.m_shapekey_a;
    }
    mover->apply_damage(damage_multiplier * 100.0f, DBT_LEVEL_OBJECT, collision.m_collision_point, collision.m_collision_normal, owner, shape_key, false);
}

// 0x00BE2EB0 (:5202)
// Only for a non-inventory item (a propane tank): in multiplayer only against nothing, a remote object, an rfg mover
// or a ladder (type 19). A human touched without an event: damage beyond 5 m/s (a tenth of the speed); an impact
// explosive with no fuse (that the server sees) ragdolls a living remote human beyond 14 m/s (unshielded), flinches it
// beyond 9 (large) or 1; an attaching one sticks to the human's hit bone. With an event: an impact explosive sums the
// contacts' normal speeds and explodes beyond 15 against a moving vehicle (5 m/s), else damages beyond 10; the foley
// sound plays beyond 0.5.
void weapon::collide(const collision_event* event, object* other_obj)
{
    if (obj_flags.destroyed)
    {
        return;
    }
    if (Multiplayer && !(other_obj == nullptr || other_obj->remote_obj_flags.remote_object || World.object_cast<rfg_mover>(other_obj) != nullptr || other_obj->get_obj_type() == 19)) // Magic number: object type 19, a ladder
    {
        return;
    }
    if (!w_info->flags.non_inventory)
    {
        return;
    }
    const bool impact_explosive = w_info->projectile_info.fuse_time == 0 && (w_info->projectile_info.flags & 4) != 0; // 0x4: detonate on impact
    if (event == nullptr)
    {
        if (other_obj == nullptr || other_obj->get_obj_type() != OT_HUMAN || havok_handle == 0xFFFFFFFF)
        {
            return;
        }
        human* hp = (human*)other_obj;
        vector velocity = ZERO_VECTOR;
        havok_body_get_linear_velocity(havok_handle, velocity);
        const float speed = velocity.mag();
        if (speed > 5.0f) // Magic number: 5 m/s
        {
            collision_damage(nullptr, hp, speed / 10.0f);
        }
        vector intersection = ZERO_VECTOR;
        vector hit_normal = ZERO_VECTOR;
        int bone_index = -1;
        const vector to_human = hp->pos - pos;
        const object_hit_location hit_location = human_get_hit_location(hp, pos, to_human.copy_flatten_normalize(), intersection, 0.5f, &bone_index, &hit_normal); // Magic number: 0.5 m
        if (impact_explosive)
        {
            if ((Multiplayer && !multi_is_server()) || object_is_local_player(hp) || hp->char_inst->is_playing_action(ANIM_ALIAS_FLINCH) || human_is_dead(hp))
            {
                return;
            }
            const vector push(-hit_normal.x, -hit_normal.y, -hit_normal.z);
            if (speed > 14.0f && !hp->is_shielded(pos, false)) // Magic numbers: 14, 9 and 1 m/s
            {
                human_ragdoll_start(hp, false, 0xFFFFFFFF);
                havok_ragdoll_apply_point_velocity(hp->ragdoll_idx, push, intersection);
                return;
            }
            if (speed > 9.0f)
            {
                human_combat_play_flinch(hp, pos, push, hit_location, 100);
                return;
            }
            if (speed > 1.0f)
            {
                human_combat_play_flinch(hp, pos, push, hit_location, 0);
            }
            return;
        }
        const unsigned int proj_flags = w_info->projectile_info.flags;
        // 0x2: attach on impact, 0x400: dont attach to humans
        if ((proj_flags & 2) != 0 && get_parent() == 0xFFFFFFFF && (proj_flags & 0x400) == 0 && hit_location != HIT_NONE)
        {
            pos = intersection;
            if (hit_normal.is_valid() && !hit_normal.is_null())
            {
                orient.make_quick(vector(-hit_normal.x, -hit_normal.y, -hit_normal.z));
            }
            hp->set_child(this, bone_index, -1, true, true);
        }
        return;
    }
    float impact_speed = 0.0f;
    if (impact_explosive)
    {
        for (unsigned int i = 0; i < event->m_num_collisions; i++)
        {
            impact_speed = (float)fabs((double)event->m_collisions[i].m_relative_normal_velocity) + impact_speed;
        }
        if (impact_speed > 15.0f && other_obj != nullptr && other_obj->get_obj_type() == OT_VEHICLE && event->m_collisions[0].m_b_point_velocity.mag_squared() > 25.0f) // Magic numbers: 15 m/s, a vehicle at 5 m/s
        {
            explode();
        }
        else if (impact_speed > 10.0f) // Magic number: 10 m/s
        {
            collision_damage(event, other_obj, impact_speed / 10.0f);
        }
    }
    else
    {
        impact_speed = (float)fabs((double)event->m_collisions[0].m_relative_normal_velocity);
    }
    if (info->m_foley_collision != -1 && !audiolib_is_playing(m_foley_collision_instance, true) && event->m_num_collisions != 0 && impact_speed > 0.5f && audiolib_allow_collision_sounds()) // Magic number: 0.5 m/s
    {
        m_foley_collision_instance = audiolib_cue_play_3d(info->m_foley_collision, &event->m_collisions[0].m_collision_point, nullptr, &event->m_collisions[0].m_relative_point_velocity, nullptr);
    }
}

// 0x00BDD240 (:5305)
// An explosive non-inventory item (a propane tank) explodes: it leaves its carrier's inventory (the carrier, a player,
// re-equips), the server destroys it, and the explosion goes off upwards with a random heading (an npc's with the
// weapon's ai explosion).
void weapon::explode()
{
    if (obj_flags.destroyed || w_info->m_explosion_info == nullptr || !w_info->flags.non_inventory)
    {
        return;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && strcmp(w_info->name, "propane_tank") == 0)
    {
        stats_increment_by_one(STATS_HYDROGEN_TANKS_BLOWN_UP);
    }
    if (is_owned())
    {
        human* hp = nullptr;
        if (get_owner_handle() != 0xFFFFFFFF)
        {
            hp = (human*)World.object_from_handle(get_owner_handle());
            if (hp != nullptr && hp->get_obj_type() != OT_HUMAN)
            {
                hp = nullptr;
            }
        }
        const unsigned int weapon_handle = get_handle();
        // Original: the carrier is not null-checked.
        inventory_item* ip = hp->inventory;
        while (ip != nullptr && ip->weapon_handle != weapon_handle)
        {
            ip = ip->next;
            if (ip == hp->inventory)
            {
                ip = nullptr;
                break;
            }
        }
        inventory_item* desired = hp->desired_equipped_inv_item;
        if (desired != nullptr && desired->weapon_handle == weapon_handle && hp->hflags.playing_equip_anim)
        {
            hp->char_inst->stop_action((animation_action)0x3E9, 3, true);
            hp->hflags.playing_equip_anim = 0;
        }
        inventory_item* equipped = hp->get_equipped_inv_item();
        if (equipped != nullptr && equipped->weapon_handle == weapon_handle && equipped != desired && hp->hflags.playing_unequip_anim)
        {
            hp->char_inst->stop_action((animation_action)0x3E9, 3, true);
            hp->hflags.playing_unequip_anim = 0;
        }
        if (ip != nullptr)
        {
            human_remove_item_from_inventory(hp, ip, ip->count, false);
        }
        if (hp->get_equipped_inv_item() == nullptr && hp->desired_equipped_inv_item == nullptr && object_is_player(hp))
        {
            if (hp->last_equipped_weapon == nullptr)
            {
                hp->desired_equipped_inv_item = inv_item_get_best_weapon(hp, true);
            }
            else
            {
                hp->desired_equipped_inv_item = hp->last_equipped_weapon;
                hp->last_equipped_weapon = hp->second_last_equipped_weapon;
                hp->second_last_equipped_weapon = nullptr;
            }
        }
    }
    if (!Multiplayer || multi_is_server())
    {
        set_is_visible(false);
        World.destroy_object(this, false);
    }
    matrix exp_orient(IDENTITY_MATRIX);
    vector up = orient.uvec;
    exp_orient.make(up);
    exp_orient.rotate_around_local_x(PI_OVER_2);
    exp_orient.rotate_around_local_y(fl_rand_range(0.0f, PI2));
    object* owner_objp = World.object_from_handle(get_owner_handle());
    const explosion_info* e_info = w_info->m_explosion_info;
    if (w_info->m_ai_explosion_info != nullptr && !object_is_player(owner_objp))
    {
        e_info = w_info->m_ai_explosion_info;
    }
    explosion_create(e_info, this, owner_objp, pos, exp_orient, &up, w_info, false);
}

// 0x00BD97E0 (:679)
void weapon_fire_info::init(const vector& start_pos_in, const vector& end_pos_in, const weapon_info* w_info_in)
{
    start_pos = start_pos_in;
    end_pos = end_pos_in;
    w_info = w_info_in;
    hit_handle = 0xFFFFFFFF;
}

// 0x00BD9E70 (:3423): -5 stands for unique id 38; 255 is none.
const weapon_info* weapons_get_weapon_info_from_weapon_unique_id(int weapon_unique_id)
{
    if (weapon_unique_id == 0xFF)
    {
        return nullptr;
    }
    if (weapon_unique_id == -5)
    {
        weapon_unique_id = 0x26;
    }
    for (int i = 0; i < Num_weapon_infos; i++)
    {
        if (Weapon_infos[i].unique_id == weapon_unique_id)
        {
            return &Weapon_infos[i];
        }
    }
    return nullptr;
}

// 0x00BD9EB0 (:3446)
float weapons_get_projectile_gravity(const weapon_info* w_info)
{
    return w_info->projectile_info.gravity;
}

// 0x00BDA150 (:5628)
unsigned int weapons_get_nano_fade_effect()
{
    return Nano_fade_effect;
}

// 0x00BD97B0 (:409): the grinder with the explosive upgrade, or an explosion that damages vehicles.
bool weapon_info::is_explosive() const
{
    if (weapon_class == WEAPON_CLASS_GRINDER)
    {
        return upgrade_has_grinder_explosive();
    }
    return m_explosion_info != nullptr && m_explosion_info->m_vehicle_max_damage_hitpoints != 0;
}

// 0x00BD9CC0 (:3292)
void weapons_level_init_post()
{
}

// 0x00BD9BA0 (:2262)
const weapon_fire_info* weapon::get_new_fire_info()
{
    if (!flags.fire_info_is_new)
    {
        return nullptr;
    }
    flags.fire_info_is_new = false; // handed out once
    return &fire_info;
}
