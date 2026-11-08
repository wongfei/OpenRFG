// Original: volition/rfg/code/gamewide/downloadable_content.cpp
//
// Downloadable content bundles. The Re-Mars-tered build ships its DLC in the game build directory: the bundle scan
// (dlc_get_bundles_sync, run when entering multiplayer, the DLC menu or Wrecking Crew) mounts each of the 3 bundles'
// misc packfile in turn and records what its tables add (multiplayer levels, single player levels, Wrecking Crew, the
// bagman modes, morale off); the multiplayer mask of the bundles goes to the platform layer.
// A DLC level (dlc_level_init / dlc_level_shutdown) mounts the bundle's packfiles, loads its interface and territory
// and, in single player, every table the bundle adds (melee, minimap icons, animations, effects, items, explosions,
// weapons, turrets, personas, humans, vehicles, spawn groups, action nodes, activities, missions, audio, gameplay
// properties, upgrades, ambient spawns, level object properties, reinforcements, music, stats, the map, tweaks) over
// marks of their pools and counts, which the shutdown restores. The DLC menu lists the single player DLC levels
// (dlc_get_single_player_levels); the console override (Console_dlc_override) loads one bundle by id. The bundles'
// custom DLC values (custom_dlc_data.cpp) switch off salvage, morale and tool tips, replace crates with totems and
// change the despawn priority.
// The file is part of the gamewide unity TU (rfg_gamewide_unity.cpp): it reads gameseq.cpp's state stack and defines
// Current_bundle_id and Dlc_level_mempool_mark for gamestate_gameplay.cpp.
//
// Not ported: the file statics with no caller that rfg.exe did not inline anywhere either (dlc_set_mp_mask,
// dlc_amb_spawn_info_shutdown, dlc_player_reinforcement_shutdown, dlc_level_objects_shutdown, dlc_territories_shutdown,
// dlc_action_nodes_shutdown, dlc_audio_shutdown): dlc_level_shutdown calls the subsystems' own DLC shutdowns instead.
//
// Notes (decompiled behaviour, kept as is):
// - dlc_items_init unloads the items_containers peg (the base game's), not dlc_items_containers.
// - dlc_level_init returns without restoring the standard file access or locking Permanent_mempool when a misc or
//   interface packfile does not mount, and without locking it when the audio language is not a language (or past
//   the table); the DLC personas and voices packfiles are always the English ones.
// - dlc_missions_init unlocks Permanent_mempool without having locked it.
// - dlc_get_morale_disabled_load_game reads DLC_bundle_info[-1] when no bundle has the current id.
// - dlc_path_callback answers past the bundles while Console_dlc_override is not 0 (its default is -1).

#include "volition/rfg/code/gamewide/downloadable_content.h"

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/custom_dlc_data.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/preload.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/game_rig.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_shared.h"
#include "volition/rfg/code/interface/ui_document_view_dlc.h"
#include "volition/rfg/code/interface/ui_memory_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_heads_and_props.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_item_shared.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/managed_script_group.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_data.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_cover.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser.h"
#include "volition/rfg/code/weapons/aim_drift.h"
#include "volition/rfg/code/weapons/weapon_firing_patterns.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

#include <stdio.h>

// size 0x85: one bundle found by the scan
struct dlc_bundle_info
{
    char id; // 0x00: the bundle number of its file names (dlcp01: 1)
    bool contains_mp; // 0x01: dlcNN_mp_levels.xtbl
    bool contains_sp; // 0x02: dlcNN_sp_levels.xtbl
    bool contains_wc; // 0x03: dlcNN_wrecking_crew.xtbl
    bool morale_disabled; // 0x04: the bundle's custom DLC values turn morale off
    char path[128]; // 0x05: the directory of the bundle's packfiles
};

// size 0x8
struct dlc_info
{
    int bundles_to_create; // 0x00
    int bundles_created; // 0x04
};

// size 0xC: the streaming containers a DLC level loads into a temporary list (released on shutdown)
struct dlc_temp_container_list
{
    unsigned int* srid_list; // 0x00
    int srid_count; // 0x04
    int srid_size; // 0x08
};

// ---- globals
// The DLC lists of the spawn, squad and vehicle cover tables (their parsers add the DLC entries; the DLC shutdown
// frees them). Placement inferred: next to this file's Dlc_persistent_srids.
farray<human_spawn_group_info*,22> Dlc_human_spawn_group_list; // 0x0165E0C0
farray<vehicle_spawn_group_info*,32> Dlc_vehicle_spawn_group_list; // 0x0165E128
farray<spawn_resource_data*,22> Dlc_spawn_resource_data_list; // 0x0165E1B8
farray<squad_definition*,22> Dlc_npc_squad_def_list; // 0x0165E220
farray<vehicle_cover_info*,5> Dlc_vehicle_cover_info_list; // 0x0165E288
// The packfiles of a DLC level after its misc and interface packfiles (dlcpNN_<name>.vpp_pc).
const char* Dlc_packfiles[8] = {"effects", "items", "anims", "activities", "missions", "humans", "cloth_sim", "vehicles_r"}; // 0x0165CAE0
int Num_item_infos_mark; // 0x02532C28
unsigned int Num_vint_doc_stream_data_mark; // 0x025337C8
// The DLC effects' preload pool (dlc_level_init / dlc_level_shutdown; visual_effects_parse_table loads into it).
// Placement inferred from its dynamic initializer's neighbours and users.
rfg_split_mempool DLC_effect_preload_mempool; // 0x02534A20

// ---- file-static globals
// rfg.pdb lists DLC_bundle_info, Num_bundles and DLC_pre_level_bundle_cpu_mark as file statics of
// game_save_generic.cpp, and Current_bundle_id and Dlc_level_mempool_mark of gamestate_gameplay.cpp (all one unity TU in
// rfg.exe); placed here, the first file of the unity TU that uses them. The others: placement inferred.
static int Console_dlc_override = -1; // 0x0165CB00, initial data FFFFFFFF
static int Current_bundle_id = -1; // 0x0165CB04, initial data FFFFFFFF
static int Managed_script_handle_mark = -1; // 0x0165CB08, initial data FFFFFFFF
static farray<unsigned int,10> Dlc_persistent_srids; // 0x0165E2AC
static dlc_bundle_info DLC_bundle_info[32]; // 0x02279E40
static dlc_temp_container_list DLC_temp_containers[10]; // 0x0227AF38
static dlc_info DLC_info; // 0x0250551C
static int Num_inv_item_info_mark; // 0x02532B94
static bool Has_demons_dlc; // 0x025337BC
static bool Dlc_corrupt; // 0x025337BD
static bool Dlc_sp_level_init; // 0x025337BE
static bool Dlc_should_restart; // 0x025337BF
static int Num_dlc_packfiles; // 0x025337C0
static unsigned int Dlc_level_mempool_mark; // 0x025337CC
static unsigned int Num_bundles; // 0x025337D0
static int DLC_pre_level_bundle_cpu_mark; // 0x025337D4
static int DLC_pre_level_bundle_gpu_mark; // 0x025337D8
static int DLC_level_bundle_cpu_mark; // 0x025337DC
static int DLC_level_bundle_gpu_mark; // 0x025337E0
static int Num_dlc_srid_lists; // 0x025337E4
static int Num_dlc_srid_list_mark; // 0x025337E8
static unsigned int All_mission_infos_mark; // 0x025337EC
static unsigned int Master_human_info_list_mark; // 0x025337F0
static unsigned int Human_master_record_mark; // 0x025337F4
static unsigned int Vehicle_master_record_mark; // 0x025337F8
static int Num_weapon_firing_patterns_mark; // 0x02533808
static int Num_weapon_infos_mark; // 0x0253380C
static int Num_aim_drift_profiles_mark; // 0x02533810
static int Num_turret_infos_mark; // 0x02533814
static int Num_explosion_infos_mark; // 0x02533818
static int Vehicle_info_mark; // 0x0253381C
static int Num_character_heads_mark; // 0x02533820
static unsigned char* DLC_effect_preload_buffer; // 0x02533824
static unsigned char* DLC_effect_preload_vram_buffer; // 0x02533828
static int Dlc_interface_mempool_cpu_mark; // 0x02533838
static int Dlc_interface_mempool_gpu_mark; // 0x0253383C
static unsigned int Dlc_bundles_loaded; // 0x02533840

// The custom DLC values of this file (custom_dlc_data.cpp; a bundle's dlcNN_custom_dlc_values.xtbl sets them on level
// init, the level shutdown resets them). Each registers itself in a dynamic initializer.
static bool Disable_salvage; // 0x02533830
static bool Disable_morale; // 0x02533831
static bool Disable_tooltips; // 0x02533832
static bool Replace_crates_with_totems; // 0x02533833
static int Custom_despawn_priority; // 0x02533834, an npc_despawn_type
static custom_dlc_bool Custom_DLC_boolDisable_salvage("Disable_salvage", Disable_salvage); // 0x02534AC4
static custom_dlc_bool Custom_DLC_boolDisable_morale("Disable_morale", Disable_morale); // 0x025356C8
static custom_dlc_bool Custom_DLC_boolDisable_tooltips("Disable_tooltips", Disable_tooltips); // 0x02534B40
static custom_dlc_bool Custom_DLC_boolReplace_crates_with_totems("Replace_crates_with_totems", Replace_crates_with_totems); // 0x025356B4
static custom_dlc_int Custom_DLC_intCustom_despawn_priority("Custom_despawn_priority", Custom_despawn_priority); // 0x02535E7C

// Magic number: 0x40, the mempool flag of a split pool (a GPU sub-pool beside the CPU one).
static const unsigned char MEMPOOL_FLAG_SPLIT = 0x40;

// 0x007B9EE0 (:354)
void dlc_cat_file_name(const char* src_name, char* dest_name)
{
    sprintf(dest_name, "dlc%02d_%s", Current_bundle_id, src_name);
}

// 0x007B9F00 (:359)
void dlc_set_current_id(int id)
{
    Current_bundle_id = id;
}

// 0x007B9F10 (:364)
int dlc_get_current_id()
{
    return Current_bundle_id;
}

// 0x007B9F20 (:390)
static void dlc_level_mempool_clear()
{
    Level_mempool.set_thread_ownership();
    Level_mempool.clear();
    Dlc_level_mempool_mark = 0;
}

// 0x007B9F40 (:397)
static void dlc_amb_spawn_info_init()
{
    amb_spawn_info_system_init(Current_bundle_id);
}

// 0x007B9F60 (:407)
static void dlc_player_reinforcement_init()
{
    rfc_system_init(Current_bundle_id);
}

// 0x007B9F80 (:418)
static void dlc_level_objects_init()
{
    lopm_system_init(&Permanent_mempool, Current_bundle_id);
}

// 0x007B9FB0 (:428)
// The bundle's items over the item preload pool's DLC mark; the items whose high resolution texture is set give it to
// their props' items (but the remote charge).
void dlc_items_init(int dlc_id)
{
    // Original: the base game's items peg.
    bm_unload_peg("items_containers");
    Item_preload_pool.set_thread_ownership();
    if ((Item_preload_pool.get_flags() & MEMPOOL_FLAG_SPLIT) == 0)
    {
        Item_preload_pool.split_mempool::restore_to_mark(Dlc_item_preload_cpu_mark);
    }
    else
    {
        Item_preload_pool.restore_to_mark_cpu(Dlc_item_preload_cpu_mark);
        Item_preload_pool.restore_to_mark_gpu(Dlc_item_preload_gpu_mark);
    }
    Num_inv_item_info_mark = Num_inv_item_info;
    if (dlc_id != -1)
    {
        inv_item_parse(dlc_id);
    }
    Num_item_infos_mark = Num_item_infos;
    item_parse_table_shared(&Common_mempool, &Item_preload_pool, &Item_preload_pool, false, PLATFORM_PC, false, dlc_id);
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].high_res_srid == 0)
        {
            continue;
        }
        for (int j = 0; j < Item_infos[i].num_props; j++)
        {
            item_info* prop_info = object_item_get_info(Item_infos[i].props[j].prop_name);
            if (prop_info != nullptr && v_stricmp(prop_info->name, "remote_charge") != 0)
            {
                prop_info->high_res_srid = Item_infos[i].high_res_srid;
            }
        }
    }
}

// 0x007BA0D0 (:487)
static void dlc_weapons_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "weapons.xtbl");
    if (cf_exist(filename))
    {
        Num_weapon_firing_patterns_mark = Num_weapon_firing_patterns;
        Num_weapon_infos_mark = Num_weapon_infos;
        Num_aim_drift_profiles_mark = Num_aim_drift_profiles;
        weapon_firing_patterns_table_init(Current_bundle_id);
        weapons_read_table(false, false, Current_bundle_id);
    }
}

// 0x007BA170 (:501)
// The interface's counts and its permanent pool, before a bundle's interface is loaded.
void dlc_interface_mark()
{
    Num_document_lookup_entries_mark = Num_document_lookup_entries;
    Num_vint_doc_stream_data_mark = Num_vint_doc_stream_data;
    Num_sheets_data_mark = Num_sheets;
    Num_bitmap_sheets_data_mark = Num_bitmap_sheets;
    Num_bitmap_images_data_mark = Num_bitmap_images;
    Interface_perm_mempool.set_thread_ownership();
    if ((Interface_perm_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) != 0)
    {
        Dlc_interface_mempool_cpu_mark = (int)Interface_perm_mempool.mark_cpu();
        Dlc_interface_mempool_gpu_mark = (int)Interface_perm_mempool.mark_gpu();
        return;
    }
    Dlc_interface_mempool_cpu_mark = (int)Interface_perm_mempool.static_mempool_base::mark();
}

// 0x007BA1F0 (:521)
// The bundle's vint project, bitmap sheets and documents.
void dlc_interface_init(int bundle_id)
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", bundle_id, "rfg.vint_proj");
    if (cf_exist(filename))
    {
        Interface_perm_mempool.set_thread_ownership();
        vint_reparse_project(filename, &Common_mempool);
        ui_reload_bitmat_sheet_table_file(bundle_id);
        ui_document_load_shared(&Common_mempool, &Interface_perm_mempool, Use_packfiles, PLATFORM_PC, false, bundle_id);
    }
}

// 0x007BA290 (:536)
// Back to dlc_interface_mark's counts and pool, when a bundle's interface was loaded.
void dlc_interface_shutdown()
{
    if (Num_vint_doc_stream_data_mark == 0)
    {
        return;
    }
    Num_vint_doc_stream_data = Num_vint_doc_stream_data_mark;
    Num_vint_doc_stream_data_mark = 0;
    Interface_perm_mempool.set_thread_ownership();
    if ((Interface_perm_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) != 0)
    {
        Interface_perm_mempool.restore_to_mark_cpu((unsigned int)Dlc_interface_mempool_cpu_mark);
        Interface_perm_mempool.restore_to_mark_gpu((unsigned int)Dlc_interface_mempool_gpu_mark);
    }
    else
    {
        Interface_perm_mempool.split_mempool::restore_to_mark((unsigned int)Dlc_interface_mempool_cpu_mark);
    }
    vint_restore_document_lookup_entries();
    ui_reload_bitmat_sheet_table_file(-1);
}

// 0x007CB050 (:554)
// The animation system reset and reloaded with the bundle's animations (the base game's for -1), then the rigs, the
// characters and the vehicles' base bone sets.
void dlc_anim_init(int bundle_id)
{
    animlib_system_init();
    animlib_reset_anim_system();
    anim_reset();
    Animation_mempool.set_thread_ownership();
    Anim_blend_tree_mempool.set_thread_ownership();
    Animation_mempool.clear();
    Anim_blend_tree_mempool.clear();
    Rig_mempool.set_thread_ownership();
    Rig_mempool.clear();
    anim_system_init();
    vanim_dlc_reset(bundle_id);
    cloth_sim_dlc_reset(bundle_id);
    char filename[256];
    if (bundle_id == -1)
    {
        if (!Use_packfiles && !Game_preload)
        {
            preload_all(-1);
        }
        else
        {
            _snprintf_safe(filename, sizeof(filename), "%s.%s", "/gamebuild\\anims", "vpp_pc");
            anim_load_packfile(filename);
        }
    }
    else if (!Use_packfiles && !Game_preload)
    {
        preload_all(bundle_id);
    }
    else
    {
        _snprintf_safe(filename, sizeof(filename), "%s%s%02d_anims.%s", dlc_path_callback((unsigned int)dlc_bundle_id_to_index(bundle_id)), "dlcp", bundle_id, "vpp_pc"); // inlined in the binary
        anim_load_packfile(filename);
    }
    game_rig_system_init();
    character_reload();
    for (int i = 0; i < Vehicle_info_pool.size(); i++)
    {
        const vehicle_info& info = Vehicle_info_pool[i];
        if (info.rig_name != nullptr)
        {
            animlib_register_entity_base_bone_set(animlib_get_loaded_rig(info.rig_name), info.bones_used_info);
        }
    }
}

// 0x007BA310 (:615)
static void dlc_anim_shutdown()
{
    Num_cloth_simulation_srids = 0;
}

// 0x007BA320 (:623)
static void dlc_items_shutdown()
{
    Num_inv_item_info = Num_inv_item_info_mark;
    Num_item_infos = Num_item_infos_mark;
    bm_unload_peg("dlc_items_containers");
}

// 0x007BA350 (:630)
static void dlc_weapons_shutdown()
{
    Num_weapon_firing_patterns = Num_weapon_firing_patterns_mark;
    Num_weapon_infos = Num_weapon_infos_mark;
    Num_aim_drift_profiles = Num_aim_drift_profiles_mark;
}

// 0x007CB230 (:637)
// The bundle's character table over marks of the human infos, the spawn resource records and the heads.
static void dlc_humans_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "character.xtbl");
    if (cf_exist(filename))
    {
        Human_hap_string_pool.set_thread_ownership();
        Human_info_string_pool.set_thread_ownership();
        Human_info_string_pool.mark();
        Human_master_record_mark = srm_human_resource_dlc_mark();
        Vehicle_master_record_mark = srm_vehicle_resource_dlc_mark();
        Master_human_info_list_mark = (unsigned int)Master_human_info_list.size();
        Rig_mempool.set_thread_ownership();
        Num_character_heads_mark = Num_character_heads;
        parse_human_table(false, &Level_mempool, Current_bundle_id);
    }
}

// 0x007F0920 (:654)
static void dlc_humans_shutdown()
{
    unsigned int count = (unsigned int)Master_human_info_list.size();
    while (Master_human_info_list_mark < count)
    {
        Master_human_info_list.remove((int)count - 1);
        count--;
    }
    srm_human_resource_dlc_restore_to_mark(Human_master_record_mark);
    srm_vehicle_resource_dlc_restore_to_mark(Vehicle_master_record_mark);
    Human_info_string_pool.set_thread_ownership();
    Human_info_string_pool.restore_to_mark(0);
    Num_character_heads = Num_character_heads_mark;
}

// 0x007BA380 (:667)
void dlc_localization_init(int bundle_id)
{
}

// 0x007BA390 (:673)
void dlc_localization_shutdown()
{
}

// 0x007CB2F0 (:677)
static void dlc_vehicles_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "vehicles.xtbl");
    if (cf_exist(filename))
    {
        Dlc_vehicle_cover_info_list.remove_all();
        Vehicle_info_string_pool.set_thread_ownership();
        Vehicle_info_mark = Vehicle_info_pool.size();
        vehicle_interaction_dlc_init(Current_bundle_id);
        vehicle_parse_table(Vehicle_info_pool, &Level_mempool, Current_bundle_id);
        build_resource_information();
    }
}

// 0x007E8D30 (:691)
static void dlc_vehicles_shutdown()
{
    for (int i = 0; i < Dlc_vehicle_cover_info_list.size(); i++)
    {
        Vehicle_cover_info_pool.free(Dlc_vehicle_cover_info_list[i]);
    }
    Vehicle_info_pool.resize(Vehicle_info_mark);
    vehicle_interaction_dlc_shutdown();
}

// 0x007BA3A0 (:702)
static void dlc_turret_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "turrets.xtbl");
    if (cf_exist(filename))
    {
        Num_turret_infos_mark = Num_turret_infos;
        turret_info_read_table(false, Current_bundle_id);
    }
}

// 0x007BA410 (:712)
static void dlc_turret_shutdown()
{
    Num_turret_infos = Num_turret_infos_mark;
}

// 0x007BA420 (:717)
static void dlc_explosions_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "explosions.xtbl");
    if (cf_exist(filename))
    {
        Num_explosion_infos_mark = Num_explosion_infos;
        explosion_parse_table(Current_bundle_id);
    }
}

// 0x007BA490 (:727)
static void dlc_explosions_shutdown()
{
    Num_explosion_infos = Num_explosion_infos_mark;
}

// 0x007BA4A0 (:732)
static void dlc_action_nodes_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "action_node_types.xtbl");
    if (cf_exist(filename))
    {
        Action_node_string_pool.set_thread_ownership();
        action_node_system_init();
        load_action_node_definitions(Current_bundle_id);
    }
}

// 0x007BA540 (:755)
static void dlc_effects_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "vfx.xtbl");
    if (cf_exist(filename))
    {
        Num_visual_effect_types_dlc_mark = Num_visual_effect_types;
        visual_effects_parse_table(Current_bundle_id);
        Num_effect_types_dlc_mark = (unsigned int)Num_effect_types;
        effects_parse_table_shared(Current_bundle_id);
    }
}

// 0x007BA5D0 (:774)
static void dlc_effects_shutdown()
{
    stream2_release(Dlc_effects_preload_srid);
    stream2_container_delete(Dlc_effects_preload_srid);
    Num_visual_effect_types = Num_visual_effect_types_dlc_mark;
    Num_effect_types = (int)Num_effect_types_dlc_mark;
}

// 0x007CB390 (:788)
// The squad loadouts and progression reset, then the bundle's spawn groups, spawn resources and squads.
static void dlc_spawn_init()
{
    char filename[256];
    squad_loadout_system_init(-1);
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "squads.xtbl");
    squad_progression_system_init(-1);
    if (cf_exist(filename))
    {
        Spawn_group_string_pool.set_thread_ownership();
        Squad_name_string_pool.set_thread_ownership();
        Spawn_group_string_pool.mark();
        Squad_name_string_pool.mark();
        Dlc_human_spawn_group_list.remove_all();
        Dlc_vehicle_spawn_group_list.remove_all();
        Dlc_npc_squad_def_list.remove_all();
        load_human_spawn_groups(Current_bundle_id);
        load_vehicle_spawn_groups(Current_bundle_id);
        sog_load_dlc_default_spawn_group();
        spawn_load_resource_data(Current_bundle_id);
        squad_loadout_system_init(Current_bundle_id);
        squad_progression_system_init(Current_bundle_id);
        squad_definitions_init(Current_bundle_id);
    }
}

// 0x007E8D90 (:823)
static void dlc_spawn_shutdown()
{
    Spawn_group_string_pool.set_thread_ownership();
    Squad_name_string_pool.set_thread_ownership();
    Spawn_group_string_pool.restore_to_mark(0);
    Squad_name_string_pool.restore_to_mark(0);
    for (int i = 0; i < Dlc_human_spawn_group_list.size(); i++)
    {
        Human_spawn_group_pool.free(Dlc_human_spawn_group_list[i]);
    }
    for (int i = 0; i < Dlc_vehicle_spawn_group_list.size(); i++)
    {
        Vehicle_spawn_group_pool.free(Dlc_vehicle_spawn_group_list[i]);
    }
    spawn_res_dlc_unload();
    for (int i = 0; i < Dlc_npc_squad_def_list.size(); i++)
    {
        Squad_hash_table.remove(Dlc_npc_squad_def_list[i]->name);
        Npc_squad_def_pool.free(Dlc_npc_squad_def_list[i]);
    }
    sog_clear_dlc_default_spawn_group();
}

// 0x007CB480 (:851)
static void dlc_missions_init()
{
    char filename[256];
    Level_mempool.lock();
    dlc_persona_mission_init();
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "missions.xtbl");
    if (cf_exist(filename))
    {
        Mission_name_string_pool.set_thread_ownership();
        Managed_layer_name_string_pool.set_thread_ownership();
        Mission_name_string_pool.mark();
        Managed_layer_name_string_pool.mark();
        All_mission_infos_mark = (unsigned int)All_mission_infos.size();
        Dlc_spawn_resource_data_list.remove_all();
        mission_info_load_table(Current_bundle_id);
        mission_unlock_load_table(Current_bundle_id);
        unlock_default_missions();
        init_dlc_objectives(Current_bundle_id);
    }
    // Original: Permanent_mempool was not locked here.
    Permanent_mempool.unlock();
    Level_mempool.unlock();
}

// 0x007F09E0 (:894)
// The managed layer streaming containers, the DLC's mission and layer names, its managed layer sets and missions and
// its objectives.
static void dlc_missions_shutdown()
{
    dlc_managed_layer_srid_removal();
    Mission_name_string_pool.set_thread_ownership();
    Managed_layer_name_string_pool.set_thread_ownership();
    Mission_name_string_pool.restore_to_mark(0);
    Managed_layer_name_string_pool.restore_to_mark(0);
    Managed_layer_set_pool.thread_access.enter();
    managed_layer_set* layer_set = Managed_layer_set_pool.get_first(0);
    while (layer_set != nullptr)
    {
        managed_layer_set* next = Managed_layer_set_pool.get_next(layer_set);
        if (layer_set->is_dlc())
        {
            Managed_layer_set_pool.free(layer_set);
        }
        layer_set = next;
    }
    Managed_layer_set_pool.thread_access.leave();
    unsigned int count = (unsigned int)All_mission_infos.size();
    while (All_mission_infos_mark < count)
    {
        All_mission_infos.remove((int)count - 1);
        count--;
    }
    remove_dlc_objectives();
}

// 0x007BA610 (:921)
static void dlc_activity_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "activity.xtbl");
    if (cf_exist(filename))
    {
        Managed_script_handle_mark = Managed_script_handle_free;
        Managed_layer_name_string_pool.set_thread_ownership();
        base_activity_load_table(Current_bundle_id);
        activity_cont_attack_dlc_init(Current_bundle_id);
        house_arrest_system_init(Current_bundle_id);
        load_raid_data(Current_bundle_id);
        load_area_defense_table(false, Current_bundle_id);
        demolitions_master_system_init(Current_bundle_id);
        vrail_data_system_init(Current_bundle_id);
        load_delivery_data(Current_bundle_id);
    }
}

// 0x007CB560 (:941)
// The activities' DLC data, then the script groups created since the mark (newest first, out of the table).
static void dlc_activity_shutdown()
{
    base_activity_unload_dlc();
    house_arrest_unload_dlc();
    raid_data_unload_dlc();
    area_defense_unload_dlc();
    demolitions_master_unload_dlc();
    vrail_data_unload_dlc();
    delivery_data_unload_dlc();
    int group_handle = Managed_script_handle_free;
    while (Managed_script_handle_mark < group_handle)
    {
        group_handle--;
        Managed_script_table->remove(managed_script_get_name(group_handle));
    }
    Managed_script_handle_free = Managed_script_handle_mark;
}

// 0x007BA6E0 (:961)
// The bundle's personas (with the English language suffix) and persona flags, then the voice line disables.
static void dlc_persona_init()
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "rfg_voice_personas.xtbl");
    if (cf_exist(filename))
    {
        char lang_suffix[256];
        keen::formatString(lang_suffix, sizeof(lang_suffix), "%s", keen::getLanguageIdString(keen::LanguageId_En_Us));
        keen::changeStringToLowercase(lang_suffix);
        keen::replaceStringCharacters(lang_suffix, '-', '_');
        human_voice_persona::parse_personas_table_shared(filename, &Common_mempool, &Level_mempool, Use_packfiles, PLATFORM_PC, false, true, lang_suffix);
    }
    sprintf(filename, "dlc%02d_%s", Current_bundle_id, "rfg_voice_persona_flags.xtbl");
    if (cf_exist(filename))
    {
        parse_personas_flags_table(filename, true);
    }
    human_voice_line_disable_init(&Level_mempool);
}

// 0x007BA7F0 (:985)
static void dlc_persona_shutdown()
{
    human_voice_persona::dlc_persona_shutdown();
    human_voice_line_disable_shutdown();
}

// 0x007BA800 (:992)
static void dlc_always_loaded_persona_init()
{
    human_voice_persona_do_game_preload(false, true);
}

// 0x007BA810 (:997)
static void dlc_always_loaded_persona_shutdown()
{
    human_voice_persona_do_game_preload(false, false);
}

// 0x007BA820 (:1002)
static void dlc_audio_init(int dlc_bundle)
{
    audiolib_dlc_init(dlc_bundle, &Level_mempool, keen::LanguageId_En_Us);
}

// 0x007BA850 (:1014)
static void dlc_gpm_init()
{
    gpm_system_init(&Permanent_mempool, Current_bundle_id);
}

// 0x007BA870 (:1019)
static void dlc_upgrade_init()
{
    upgrade_system_init(Current_bundle_id);
}

// 0x007BA880 (:1024)
void dlc_upgrade_shutdown()
{
    upgrade_system_init(-1);
}

// 0x007BA890 (:1029)
void dlc_territories_init(unsigned char* table_data)
{
    territory_shared_system_init(&Permanent_mempool, Current_bundle_id, table_data);
}

// 0x007BA8B0 (:1036)
void dlc_system_shutdown()
{
    Num_bundles = 0;
}

// 0x007BA8C0 (:1042)
void dlc_open_marketplace()
{
}

// 0x007BA8D0 (:1047)
void dlc_system_init()
{
    Num_bundles = 0;
}

// 0x007BA8E0 (:1052)
void dlc_check_media_changed()
{
}

// 0x007BA8F0 (:1057)
bool dlc_media_unavailable()
{
    return false;
}

// 0x007BA900 (:1070)
unsigned int dlc_count_callback()
{
    return Num_bundles;
}

// 0x007BA910 (:1081): null for an index past the bundles (unless the console override is not 0)
const char* dlc_path_callback(unsigned int index)
{
    if (Num_bundles <= index && Console_dlc_override == 0)
    {
        return nullptr;
    }
    return DLC_bundle_info[index].path;
}

// 0x007BA940 (:1087)
// Permanent_mempool marked (and left unlocked) before a bundle's interface and territory are loaded into it.
void dlc_level_init_pre()
{
    Permanent_mempool.set_thread_ownership();
    Permanent_mempool.unlock();
    if ((Permanent_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) != 0)
    {
        DLC_pre_level_bundle_cpu_mark = (int)Permanent_mempool.mark_cpu();
        DLC_pre_level_bundle_gpu_mark = (int)Permanent_mempool.mark_gpu();
        return;
    }
    DLC_pre_level_bundle_cpu_mark = (int)Permanent_mempool.static_mempool_base::mark();
}

// 0x007BA990 (:1106)
// The bundle's territory and voice personas, and Permanent_mempool back to dlc_level_init_pre's mark.
void dlc_level_shutdown_post()
{
    if (DLC_pre_level_bundle_cpu_mark <= 0)
    {
        return;
    }
    territory_shared_system_dlc_shutdown();
    dlc_voice_persona_shutdown_on_exit();
    Permanent_mempool.unlock();
    if ((Permanent_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) == 0)
    {
        Permanent_mempool.split_mempool::restore_to_mark((unsigned int)DLC_pre_level_bundle_cpu_mark);
    }
    else
    {
        Permanent_mempool.restore_to_mark_cpu((unsigned int)DLC_pre_level_bundle_cpu_mark);
        Permanent_mempool.restore_to_mark_gpu((unsigned int)DLC_pre_level_bundle_gpu_mark);
    }
    Permanent_mempool.lock();
    DLC_pre_level_bundle_cpu_mark = 0;
}

// 0x007BAA00 (:1127)
void dlc_mp_level_init()
{
    if (Dlc_level_mempool_mark != 0)
    {
        Level_mempool.restore_to_mark(Dlc_level_mempool_mark);
    }
    Dlc_level_mempool_mark = Level_mempool.mark();
}

// 0x007EF1B0 (:1135)
// The level's bundle (the multiplayer level's, or the world's in single player; nothing for -1). With Common_mempool
// empty: the first time, the bundle's interface, loading screen images and territory over a Permanent_mempool mark,
// and its misc and interface packfiles; then its other packfiles (3 when preloading, 8 with packfiles, plus the
// English personas and voices) and, in single player, every table it adds.
void dlc_level_init()
{
    int bundle_id;
    if (Multiplayer)
    {
        if (!Mp_server_options.get_level()->is_downloadable_content)
        {
            return;
        }
        bundle_id = Mp_server_options.get_level()->dlc_bundle_id;
        World.dlc_bundle_id = Mp_server_options.get_level()->dlc_bundle_id;
    }
    else
    {
        bundle_id = World.dlc_bundle_id;
        if (World.dlc_bundle_id == -1)
        {
            return;
        }
    }
    if (bundle_id == -1)
    {
        return;
    }
    Current_bundle_id = bundle_id;
    // space_max() is the pool's max_size.
    if (Common_mempool.space_max() != Common_mempool.space_free())
    {
        return;
    }
    Common_mempool.set_thread_ownership();
    Permanent_mempool.set_thread_ownership();
    Permanent_mempool.unlock();
    Level_mempool.restore_to_mark(Dlc_level_mempool_mark);
    char packfile[256];
    if (DLC_pre_level_bundle_cpu_mark == 0)
    {
        dlc_level_mempool_clear(); // inlined in the binary
        dlc_level_init_pre();
        dlc_init_console_override();
        dlc_interface_mark();
        loading_screen_parse_images_table(Current_bundle_id);
        dlc_interface_init(Current_bundle_id);
        territory_shared_system_init(&Permanent_mempool, Current_bundle_id, nullptr);
        Num_downloaded_levels = 1;
        if (Game_preload || Use_packfiles)
        {
            game_enable_standard_access();
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_misc.%s", dlc_path_callback((unsigned int)dlc_bundle_id_to_index(Current_bundle_id)), "dlcp", Current_bundle_id, "vpp_pc");
            if (!packfile_temp_add(packfile, nullptr, false, false))
            {
                // Original: returns with the standard access on and Permanent_mempool unlocked.
                return;
            }
            game_disable_standard_access();
            if (Use_packfiles)
            {
                game_enable_standard_access();
                _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_interface.%s", dlc_path_callback((unsigned int)dlc_bundle_id_to_index(Current_bundle_id)), "dlcp", Current_bundle_id, "vpp_pc");
                if (!packfile_temp_add(packfile, nullptr, false, false))
                {
                    // Original: as above.
                    return;
                }
                game_disable_standard_access();
            }
        }
        DLC_pre_level_bundle_cpu_mark = 1;
    }
    Num_dlc_srid_list_mark = Num_dlc_srid_lists;
    if ((Permanent_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) == 0)
    {
        DLC_level_bundle_cpu_mark = (int)Permanent_mempool.static_mempool_base::mark();
    }
    else
    {
        DLC_level_bundle_cpu_mark = (int)Permanent_mempool.mark_cpu();
        DLC_level_bundle_gpu_mark = (int)Permanent_mempool.mark_gpu();
    }
    bool load_packfiles = true;
    if (Game_preload)
    {
        // Magic number: 3, the effects, items and anims packfiles of a preloading build.
        Num_dlc_packfiles = 3;
    }
    else if (Use_packfiles)
    {
        Num_dlc_packfiles = 8;
    }
    else
    {
        load_packfiles = false;
    }
    if (load_packfiles)
    {
        const unsigned int bundle_index = (unsigned int)dlc_bundle_id_to_index(Current_bundle_id);
        game_enable_standard_access();
        for (int i = 0; i < Num_dlc_packfiles; i++)
        {
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_%s.%s", dlc_path_callback(bundle_index), "dlcp", Current_bundle_id, Dlc_packfiles[i], "vpp_pc");
            if (!packfile_temp_add(packfile, nullptr, false, false))
            {
                game_disable_standard_access();
                return;
            }
        }
        if (Use_packfiles)
        {
            const keen::LanguageId audio_language = localization_get_audio_language();
            // Original: an audio language past the table (0x31 and up) or none returns with the standard access on
            // and Permanent_mempool unlocked.
            if ((int)audio_language >= 0x31 || audio_language == (keen::LanguageId)-1)
            {
                return;
            }
            // Original: the personas and voices are always the English ones.
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_personas_%s.%s", dlc_path_callback(bundle_index), "dlcp", Current_bundle_id, localization_get_language_name(keen::LanguageId_En_Us), "vpp_pc");
            keen::changeStringToLowercase(packfile);
            keen::replaceStringCharacters(packfile, '-', '_');
            if (!packfile_temp_add(packfile, nullptr, false, false))
            {
                game_disable_standard_access();
                return;
            }
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_voices_%s.%s", dlc_path_callback(bundle_index), "dlcp", Current_bundle_id, localization_get_language_name(keen::LanguageId_En_Us), "vpp_pc");
            keen::changeStringToLowercase(packfile);
            keen::replaceStringCharacters(packfile, '-', '_');
            if (!packfile_temp_add(packfile, nullptr, false, false))
            {
                game_disable_standard_access();
                return;
            }
        }
        game_disable_standard_access();
    }
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        Dlc_sp_level_init = true;
        DLC_effect_preload_vram_buffer = nullptr;
        // Magic numbers: the DLC effect preload pool, 32 KB from Level_mempool (16-byte aligned).
        DLC_effect_preload_buffer = (unsigned char*)Level_mempool.alloc(0x8000, 0x10);
        DLC_effect_preload_mempool.init_rfg_split_mempool("dlc effect preload", DLC_effect_preload_buffer, 0x8000, 4, DLC_effect_preload_vram_buffer, 0, 0x10, 8);
        DLC_effect_preload_mempool.clear();
        Stream_dlc_effect_preload_allocator.init(&DLC_effect_preload_mempool);
        custom_dlc_data_level_init(Current_bundle_id);
        parse_melee_table(Current_bundle_id, false);
        minimap_load_icon_table(Current_bundle_id);
        dlc_anim_init(Current_bundle_id);
        dlc_effects_init();
        dlc_items_init(Current_bundle_id);
        dlc_explosions_init();
        dlc_weapons_init();
        dlc_turret_init();
        dlc_persona_init();
        dlc_voice_persona_level_init(Current_bundle_id, &Level_mempool);
        dlc_humans_init();
        dlc_vehicles_init();
        dlc_spawn_init();
        dlc_action_nodes_init();
        dlc_activity_init();
        dlc_missions_init();
        dlc_audio_init(Current_bundle_id); // inlined in the binary
        dlc_always_loaded_persona_init(); // inlined in the binary
        dlc_gpm_init(); // inlined in the binary
        dlc_upgrade_init(); // inlined in the binary
        dlc_amb_spawn_info_init(); // inlined in the binary
        dlc_level_objects_init(); // inlined in the binary
        dlc_player_reinforcement_init(); // inlined in the binary
        gamemusic_dlc_music_init(Current_bundle_id);
        stats_system_init(Current_bundle_id);
        stats_reset_specific(true, false);
        fullscreen_map_terr_info_init(Current_bundle_id);
        tweak_table_load(Current_bundle_id);
    }
    Permanent_mempool.lock();
}

// 0x007F0B00 (:1377)
// A single player DLC level's tables and temporary containers released, its pools and counts back to their marks.
void dlc_level_shutdown()
{
    if (!Dlc_sp_level_init)
    {
        return;
    }
    int bundle_id;
    if (Multiplayer)
    {
        if (!Mp_server_options.get_level()->is_downloadable_content)
        {
            return;
        }
        bundle_id = Mp_server_options.get_level()->dlc_bundle_id;
        World.dlc_bundle_id = Mp_server_options.get_level()->dlc_bundle_id;
    }
    else
    {
        bundle_id = World.dlc_bundle_id;
        if (World.dlc_bundle_id == -1)
        {
            return;
        }
    }
    if (bundle_id == -1)
    {
        return;
    }
    Current_bundle_id = bundle_id;
    for (int i = Num_dlc_srid_list_mark; i < Num_dlc_srid_lists; i++)
    {
        const dlc_temp_container_list& list = DLC_temp_containers[i];
        for (int j = 0; j < list.srid_count; j++)
        {
            const unsigned int srid = list.srid_list[j];
            if (!((stream2_container*)srid)->is_idle())
            {
                stream2_release(srid);
            }
            stream2_container_delete(srid);
        }
    }
    Num_dlc_srid_lists = Num_dlc_srid_list_mark;
    Dlc_sp_level_init = false;
    gamemusic_dlc_music_shutdown();
    lopm_dlc_shutdown();
    amb_spawn_info_dlc_shutdown();
    rfc_dlc_shutdown();
    audiolib_dlc_shutdown();
    dlc_activity_shutdown();
    dlc_missions_shutdown();
    dlc_spawn_shutdown();
    dlc_vehicles_shutdown();
    dlc_humans_shutdown();
    dlc_persona_shutdown(); // inlined in the binary
    dlc_always_loaded_persona_shutdown(); // inlined in the binary
    dlc_voice_persona_level_shutdown();
    dlc_turret_shutdown(); // inlined in the binary
    dlc_weapons_shutdown(); // inlined in the binary
    dlc_explosions_shutdown(); // inlined in the binary
    dlc_items_shutdown(); // inlined in the binary
    dlc_effects_shutdown();
    dlc_anim_shutdown(); // inlined in the binary
    custom_dlc_data_level_shutdown();
    DLC_effect_preload_mempool.set_thread_ownership();
    DLC_effect_preload_mempool.clear();
    DLC_effect_preload_buffer = nullptr;
    DLC_effect_preload_vram_buffer = nullptr;
    DLC_effect_preload_mempool.init_rfg_split_mempool("dlc effects preload", nullptr, 0, 4, nullptr, 0, 4, 8);
    Permanent_mempool.unlock();
    if ((Permanent_mempool.get_flags() & MEMPOOL_FLAG_SPLIT) != 0)
    {
        Permanent_mempool.restore_to_mark_cpu((unsigned int)DLC_level_bundle_cpu_mark);
        Permanent_mempool.restore_to_mark_gpu((unsigned int)DLC_level_bundle_gpu_mark);
    }
    else
    {
        Permanent_mempool.split_mempool::restore_to_mark((unsigned int)DLC_level_bundle_cpu_mark);
    }
    Permanent_mempool.lock();
}

// 0x007BAA30 (:1486)
// A new temporary container list of num_containers entries from the pool (at most 11 lists).
bool dlc_allocate_temp_container_list(int num_containers, mempool_base* pool)
{
    // Magic number: 10, the last list index (Original: the check lets an 11th list past the 10 entries).
    if (Num_dlc_srid_lists > 10)
    {
        return false;
    }
    unsigned int* srid_list = (unsigned int*)pool->alloc((unsigned int)num_containers * 4, 0);
    const int index = Num_dlc_srid_lists;
    DLC_temp_containers[index].srid_list = srid_list;
    if (srid_list == nullptr)
    {
        return false;
    }
    DLC_temp_containers[index].srid_size = num_containers;
    DLC_temp_containers[index].srid_count = 0;
    Num_dlc_srid_lists = index + 1;
    return true;
}

// 0x007BAA90 (:1497)
// Into the newest list, while it has room.
bool dlc_store_temp_srid(unsigned int srid)
{
    if (Num_dlc_srid_lists <= 0)
    {
        return false;
    }
    dlc_temp_container_list& list = DLC_temp_containers[Num_dlc_srid_lists - 1];
    if (list.srid_count >= list.srid_size)
    {
        return false;
    }
    list.srid_list[list.srid_count] = srid;
    list.srid_count++;
    return true;
}

// 0x007BAAD0 (:1507)
// Every list's containers released and deleted.
void dlc_clear_temp_srid()
{
    for (int i = 0; i < Num_dlc_srid_lists; i++)
    {
        const dlc_temp_container_list& list = DLC_temp_containers[i];
        for (int j = 0; j < list.srid_count; j++)
        {
            const unsigned int srid = list.srid_list[j];
            if (!((stream2_container*)srid)->is_idle())
            {
                stream2_release(srid);
            }
            stream2_container_delete(srid);
        }
    }
    Num_dlc_srid_lists = 0;
}

// 0x007BAB40 (:1529)
int dlc_bundle_index_to_id(unsigned int index)
{
    return DLC_bundle_info[index].id;
}

// 0x007BAB60 (:1534): -1 when no bundle has the id
int dlc_bundle_id_to_index(int id)
{
    for (int i = 0; i < (int)Num_bundles; i++)
    {
        if (DLC_bundle_info[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

// 0x007BAB90 (:1555)
bool dlc_bundle_contains_mp(unsigned int index)
{
    return DLC_bundle_info[index].contains_mp;
}

// 0x007BABB0 (:1560)
bool dlc_bundle_contains_sp(unsigned int index)
{
    return DLC_bundle_info[index].contains_sp;
}

// 0x007BABD0 (:1565)
bool dlc_bundle_contains_wc(unsigned int index)
{
    return DLC_bundle_info[index].contains_wc;
}

// 0x007CB650 (:1570)
// Console: the first single player DLC level.
void unused_dcf_load_dlc_level()
{
    if (Console_command == 0)
    {
        return;
    }
    int level_count;
    dlc_level_info* level_list;
    dlc_get_single_player_levels(level_count, level_list);
    if (level_count > 0)
    {
        dlc_load_level(level_list);
    }
    dlc_remove_dlc_packfiles();
    dlc_level_mempool_clear(); // inlined in the binary
}

// 0x007BABF0 (:1585)
// The single player levels of every bundle not loaded yet (its misc and interface packfiles mounted with packfiles):
// read from dlcNN_sp_levels.xtbl into a list of 32 from Level_mempool, with the bundle's loading screen images and
// interface (the interface marked before the first one).
void dlc_get_single_player_levels(int& level_count, dlc_level_info*& level_list)
{
    level_count = 0;
    bool interface_marked = false;
    dlc_level_mempool_clear(); // inlined in the binary
    loading_screen_dlc_restore();
    dlc_interface_shutdown();
    // Magic number: 32 levels (0x3080 bytes).
    level_list = (dlc_level_info*)Level_mempool.alloc(32 * sizeof(dlc_level_info), 0);
    for (unsigned int index = 0; index < Num_bundles; index++)
    {
        const dlc_bundle_info& bundle = DLC_bundle_info[index];
        const int dlc_id = bundle.id;
        const unsigned int bit = 1u << (bundle.id & 0x1F);
        if (!bundle.contains_sp || (bit & Dlc_bundles_loaded) != 0)
        {
            continue;
        }
        Dlc_bundles_loaded |= bit;
        if (Use_packfiles)
        {
            packfile_set_thread_ownership();
            game_enable_standard_access();
            char packfile[256];
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_misc.%s", dlc_path_callback(index), "dlcp", dlc_id, "vpp_pc"); // inlined in the binary
            if (!packfile_add(packfile, nullptr, false, false))
            {
                game_disable_standard_access();
                continue;
            }
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_interface.%s", dlc_path_callback(index), "dlcp", dlc_id, "vpp_pc"); // inlined in the binary
            if (!packfile_add(packfile, nullptr, false, false))
            {
                game_disable_standard_access();
                continue;
            }
            game_disable_standard_access();
        }
        char table_name[256];
        sprintf(table_name, "dlc%02d_sp_levels.xtbl", dlc_id);
        xml_element* table = xml_table_parse(table_name);
        if (table != nullptr)
        {
            for (xml_element* root = xml_table_find_opt(table, "sp_level_list"); root != nullptr; root = xml_table_find_next(table, root, "sp_level_list"))
            {
                dlc_level_info& level = level_list[level_count];
                xml_table_get_req_string(level.name, 0x40, root, "Name");
                xml_table_get_req_string(level.display_name, 0x40, root, "Display_name");
                xml_table_get_req_string(level.file_name, 0x40, root, "Filename");
                xml_table_get_req_string(level.description, 0x40, root, "Description");
                xml_table_get_req_string(level.image_file, 0x40, root, "level_image_name");
                xml_table_get_req_filename(level.intro_video, 0x40, root, "Intro_video");
                level.dlc_bundle_id = dlc_id;
                level_count++;
            }
            xml_table_close(table);
        }
        loading_screen_parse_images_table(dlc_id);
        if (!interface_marked)
        {
            dlc_interface_mark();
            interface_marked = true;
        }
        dlc_interface_init(dlc_id);
    }
    Dlc_level_mempool_mark = Level_mempool.mark();
}

// 0x007BAF30 (:1698)
// Every packfile mounted last whose name starts with "dlc" (newest first).
void dlc_remove_dlc_packfiles()
{
    Dlc_bundles_loaded = 0;
    if (!Use_packfiles)
    {
        return;
    }
    packfile_set_thread_ownership();
    const char* name = packfile_get_last_packfile_name();
    while (name != nullptr && v_strnicmp(name, "dlc", 3) == 0)
    {
        packfile_remove(name);
        name = packfile_get_last_packfile_name();
    }
}

// 0x007BAF80 (:1722)
void dlc_free_single_player_levels()
{
    dlc_remove_dlc_packfiles();
    dlc_level_mempool_clear(); // inlined in the binary
}

// 0x007BAFB0 (:1731)
// The level's zone file as a new game in the bundle.
void dlc_load_level(dlc_level_info* level)
{
    char filename[64];
    cf_add_ext(filename, sizeof(filename), level->file_name, ".rfgzone_pc", false);
    World.set_pending_territory_load(filename, true, false, false, level->dlc_bundle_id, false);
}

// The multiplayer mask of the bundles: a bit per bundle id with multiplayer content (inlined in dlc_get_mp_mask and
// dlc_get_bundles_sync). Original: ids up to 32 count, and id 32 shifts by 32 & 0x1F (bit 0).
static unsigned int dlc_mp_mask_of_bundles()
{
    unsigned int mask = 0;
    for (unsigned int i = 0; i < Num_bundles; i++)
    {
        if (DLC_bundle_info[i].contains_mp && DLC_bundle_info[i].id <= 32)
        {
            mask |= 1u << (DLC_bundle_info[i].id & 0x1F);
        }
    }
    return mask;
}

// 0x007BB010 (:1740)
unsigned int dlc_get_mp_mask()
{
    return dlc_mp_mask_of_bundles();
}

// 0x007BB050 (:1760)
int dlc_get_override_id()
{
    return Console_dlc_override;
}

// 0x007BB060 (:1767)
bool dlc_should_restart()
{
    return Dlc_should_restart;
}

// 0x007BB070 (:1772)
void dlc_should_restart(bool restart)
{
    Dlc_should_restart = restart;
}

// 0x007CB6B0 (:1779)
// Back in the main menu after a requested restart: the DLC view again.
void dlc_restart()
{
    if (Stack_top_index > -1 && State_stack[Stack_top_index] == GS_MAINMENU && Dlc_should_restart)
    {
        Dlc_should_restart = false;
        ui_view_dlc_show();
    }
}

// 0x007ECE50 (:1789)
// A new console override id becomes the only bundle (from the game build directory with packfiles, the data
// directories without), recorded like the bundle scan does.
void dlc_init_console_override()
{
    // Placement inferred: a function static of the binary (0x0165F048), not in rfg.pdb.
    static int Last_console_dlc_override = -1;
    if (Console_dlc_override == -1 || Console_dlc_override == Last_console_dlc_override)
    {
        return;
    }
    Bagman_dlc_unlocked = false;
    Last_console_dlc_override = Console_dlc_override;
    Num_bundles = 1;
    DLC_bundle_info[0].id = (char)Console_dlc_override;
    char packfile[256];
    if (Use_packfiles || Game_preload)
    {
        sprintf(DLC_bundle_info[0].path, "/gamebuild/");
        if (Use_packfiles || Game_preload)
        {
            packfile_set_thread_ownership();
            game_enable_standard_access();
            _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_misc.%s", dlc_path_callback(0), "dlcp", Console_dlc_override, "vpp_pc"); // inlined in the binary
            packfile_add(packfile, nullptr, false, false);
            game_disable_standard_access();
        }
    }
    else
    {
        DLC_bundle_info[0].path[0] = '\0';
    }
    char table[256];
    sprintf(table, "dlc%02d_mp_levels.xtbl", Console_dlc_override);
    DLC_bundle_info[0].contains_mp = cf_exist(table);
    sprintf(table, "dlc%02d_sp_levels.xtbl", Console_dlc_override);
    DLC_bundle_info[0].contains_sp = cf_exist(table);
    sprintf(table, "dlc%02d_wrecking_crew.xtbl", Console_dlc_override);
    DLC_bundle_info[0].contains_wc = cf_exist(table);
    sprintf(table, "dlc%02d_bagman_unlock.xtbl", Console_dlc_override);
    Bagman_dlc_unlocked = cf_exist(table);
    custom_dlc_data_level_init(Console_dlc_override);
    DLC_bundle_info[0].morale_disabled = Disable_morale;
    custom_dlc_data_level_shutdown();
    if (Use_packfiles || Game_preload)
    {
        packfile_remove(packfile);
    }
}

// 0x007D7FD0 (:1852)
// A managed layer's streaming container of the DLC, kept while there is room (released with the DLC missions).
void dlc_managed_layer_store_srid(unsigned int srid)
{
    if (Dlc_persistent_srids.size() < Dlc_persistent_srids.capacity())
    {
        Dlc_persistent_srids.add(srid);
    }
}

// 0x007CB6E0 (:1859)
void dlc_managed_layer_srid_removal()
{
    for (int i = 0; i < Dlc_persistent_srids.size(); i++)
    {
        const unsigned int srid = Dlc_persistent_srids[i];
        if (!((stream2_container*)srid)->is_idle())
        {
            stream2_release(srid);
        }
        stream2_container_delete(srid);
    }
    Dlc_persistent_srids.remove_all();
}

// 0x007BB080 (:1875)
bool find_content_on_controller(unsigned int controller_index)
{
    return false;
}

// 0x007BB090 (:1884)
bool dlc_find_new_content(bool first_run_only, bool check_all_controllers)
{
    return false;
}

// 0x007BB0A0 (:1890)
bool dlc_found_corrupt_content()
{
    return Dlc_corrupt;
}

// 0x007ED030 (:1896): each bundle whose misc packfile mounts is recorded with what its tables add; the packfile is
// removed again. magic number: 3 bundles (dlcp01-dlcp03 in the game build directory).
void dlc_get_bundles_sync()
{
    Dlc_corrupt = false;
    Bagman_dlc_unlocked = false;
    if (!Use_packfiles)
    {
        return;
    }
    packfile_set_thread_ownership();
    DLC_info.bundles_created = 0;
    DLC_info.bundles_to_create = 3;
    game_enable_standard_access();
    for (int id = 1; id - 1 < DLC_info.bundles_to_create; id++)
    {
        char packfile[128];
        sprintf(packfile, "/gamebuild/dlcp%02d_misc.%s", id, "vpp_pc");
        if (!packfile_add(packfile, nullptr, false, false))
        {
            continue;
        }
        dlc_bundle_info& bundle = DLC_bundle_info[DLC_info.bundles_created];
        char table[128];
        sprintf(table, "dlc%02d_mp_levels.xtbl", id);
        bundle.contains_mp = cf_exist(table);
        sprintf(table, "dlc%02d_sp_levels.xtbl", id);
        bundle.contains_sp = cf_exist(table);
        if (bundle.contains_sp)
        {
            Has_demons_dlc = true;
        }
        sprintf(table, "dlc%02d_wrecking_crew.xtbl", id);
        bundle.contains_wc = cf_exist(table);
        sprintf(table, "dlc%02d_bagman_unlock.xtbl", id);
        if (cf_exist(table))
        {
            Bagman_dlc_unlocked = true;
        }
        custom_dlc_data_level_init(id);
        bundle.morale_disabled = Disable_morale;
        custom_dlc_data_level_shutdown();
        packfile_remove(packfile);
        bundle.id = (char)id;
        sprintf(bundle.path, "/gamebuild/");
        DLC_info.bundles_created++;
    }
    game_disable_standard_access();
    Num_bundles = (unsigned int)DLC_info.bundles_created;
    multi_core_platform_set_user_dlc_mask(dlc_mp_mask_of_bundles());
}

// 0x007BB0B0 (:1976)
bool dlc_get_morale_disabled()
{
    return Disable_morale;
}

// 0x007BB0C0 (:1981)
// The current bundle's flag as the scan recorded it (Original: DLC_bundle_info[-1] when no bundle has the id).
bool dlc_get_morale_disabled_load_game()
{
    return DLC_bundle_info[dlc_bundle_id_to_index(Current_bundle_id)].morale_disabled; // inlined in the binary
}

// 0x007BB110 (:1987)
bool dlc_get_salvage_disabled()
{
    return Disable_salvage;
}

// 0x007BB120 (:1992)
bool dlc_get_tooltips_disabled()
{
    return Disable_tooltips;
}

// 0x007BB130 (:1997)
bool dlc_replace_crates_with_totems()
{
    return Replace_crates_with_totems;
}

// 0x007BB140 (:2002)
npc_despawn_type dlc_custom_despawn_priority()
{
    return (npc_despawn_type)Custom_despawn_priority;
}

// 0x007BB150 (:2007)
void dlc_subtitles()
{
    ui_set_subtitles(Subtitles_enabled);
}

// 0x007BB160 (:2012)
void dlc_load_all_ui_data()
{
}

// 0x007BB170 (:2017)
void dlc_exit_mp()
{
    dlc_remove_dlc_packfiles();
    dlc_interface_shutdown();
    dlc_level_mempool_clear(); // inlined in the binary
    Current_bundle_id = -1;
}

// 0x007BB1B0 (:2029)
void dlc_exit_wc()
{
    dlc_remove_dlc_packfiles();
    dlc_interface_shutdown();
    dlc_level_mempool_clear(); // inlined in the binary
    Current_bundle_id = -1;
}

// 0x007BB1F0 (:2041): the bundle's misc and interface packfiles (dlcpNN_misc, dlcpNN_interface) from its directory
void dlc_add_bundle(int bundle_id, int bundle_index)
{
    Dlc_bundles_loaded |= 1u << (bundle_id & 0x1F);
    if (!Use_packfiles)
    {
        return;
    }
    char packfile[256];
    _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_misc.%s", dlc_path_callback((unsigned int)bundle_index), "dlcp", bundle_id, "vpp_pc");
    packfile_add(packfile, nullptr, false, false);
    _snprintf_safe(packfile, sizeof(packfile), "%s%s%02d_interface.%s", dlc_path_callback((unsigned int)bundle_index), "dlcp", bundle_id, "vpp_pc");
    packfile_add(packfile, nullptr, false, false);
}

// 0x007BB300 (:2062)
void dlc_check_for_content_removal()
{
}
