// Original: volition/rfg/code/gamewide/game_stream2_register.cpp
//
// Registers RFG's allocators and streaming types (see game_stream2_register.h for the table). The registrations are
// copied into Stream2_mempool (Permanent_mempool). The callbacks of systems that are not implemented yet are seams
// (see the headless stand-ins).
//
// Notes (decompiled behaviour, kept as is):
// - A failed registration of the UI container, the Peg, wavebank or foley bank primitive, or the Landmark LOD
//   primitive logs and returns early; the soundbank failure logs the wavebank message. The other failures only log.
// - The Pegs registration reuses the record of the Chunks registration without clearing it (every field is
//   overwritten).
// - "Material effect" logs the Glass error message.

#include "volition/rfg/code/gamewide/game_stream2_register.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/game_render_terrain.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/interface/ui_memory_manager.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_loader.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"

#include "keen/base/trace.hpp"

#include <string.h>

// Pool the registrations are copied into (passed to stream2_register_*_type); statically initialised.
static mempool* Stream2_mempool = &Permanent_mempool; // 0x0165CC78

// Container registration (the record is cleared first).
static bool register_container(stream2_container_type_reg& reg, unsigned char type, const char* name, stream2_container_mode mode, unsigned char allocator, void (*load_cb)(unsigned int), void (*unload_cb)(unsigned int))
{
    memset(&reg, 0, sizeof(reg));
    reg.container_type = type;
    strcpy(reg.name, name);
    reg.mode = mode;
    reg.allocator_type = allocator;
    reg.load_cb = load_cb;
    reg.unload_cb = unload_cb;
    return stream2_register_container_type(reg, Stream2_mempool);
}

// 0x007C3840 (:127)
void game_stream2_register_allocators()
{
    // Indexed by game_stream_allocators.
    stream2_allocator* const allocators[GSA_NUM_ALLOCATOR_TYPES] = {
        nullptr,
        &Stream_world_allocator,
        &Stream_chunk_preload_allocator,
        &Stream_effect_preload_allocator,
        &Stream_effect_cutscene_allocator,
        &Stream_item_preload_allocator,
        &Stream_decal_preload_allocator,
        &Stream_cloth_sim_preload_allocator,
        &Stream_tod_allocator,
        &Stream_mp_effect_preload_allocator,
        &Stream_mp_item_preload_allocator,
        &Stream_player_allocator,
        &Stream_human_allocator,
        &Stream_large_weapon_allocator,
        &Stream_small_weapon_allocator,
        &Stream_vehicle_allocator,
        &Stream_large_layer_allocator,
        &Stream_small_layer_allocator,
        &Stream_human_voice_persona_allocator,
        &Stream_always_loaded_human_voice_persona_allocator,
        &Stream_audio_slot_allocator,
        &Stream_interface_allocator,
        &Fullscreen_map_stream_allocator,
        &Stream_interface_stack_allocator,
        &Stream_interface_slot_allocator,
        &Stream_mp_interface_preload_allocator,
        &Stream_mp_interface_slot_allocator,
        &Stream_material_effects_allocator,
        &Stream_permanent_allocator,
        &Stream_dlc_effect_preload_allocator,
        &Stream_dlc_item_preload_allocator,
    };
    for (int allocator_index = GSA_WORLD; allocator_index < GSA_NUM_ALLOCATOR_TYPES; allocator_index++)
    {
        stream2_register_allocator(allocator_index, allocators[allocator_index]);
    }
}

// 0x007C3970 (:172)
void game_stream2_register_types(vlib_platform plat)
{
    stream2_container_type_reg container;
    stream2_primitive_type_reg prim;

    container.container_type = GSC_LAYER_SMALL;
    strcpy(container.name, "Small Layer");
    container.mode = SCM_CONTAINER;
    container.allocator_type = GSA_SMALL_LAYER;
    if (!stream2_register_container_type(container, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering Managed Small Layer container type with streaming system!");
    }
    if (!register_container(container, GSC_LAYER_LARGE, "Large Layer", SCM_CONTAINER, GSA_LARGE_LAYER, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Managed Large Layer container type with streaming system!");
    }
    if (!register_container(container, GSC_EFFECTS_ENV, "Effects", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering VFX container type with streaming system!");
    }
    if (!register_container(container, GSC_EFFECTS_PRELOAD, "Effects Preload", SCM_PRIM, GSA_EFFECT_PRELOAD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering VFX preload container type with streaming system!");
    }
    if (!register_container(container, GSC_MP_EFFECTS, "Multiplayer Effects", SCM_PRIM, GSA_MP_EFFECT_PRELOAD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering cutscene effects container type with streaming system!");
    }
    if (!register_container(container, GSC_EFFECTS_DLC, "Effects DLC", SCM_PRIM, GSA_DLC_EFFECT_PRELOAD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering VFX preload container type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    prim.prim_type = GSP_VFX;
    strcpy(prim.name, "VFX");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = effects_template_load_cb;
    prim.unload_cb = effects_template_unload_cb;
    prim.cpu_ext[0] = ".cefct_pc";
    prim.gpu_ext[0] = ".gefct_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering VFX primitive type with streaming system!");
    }

    if (!register_container(container, GSC_AUDIO, "Game Audio", SCM_PRIM, GSA_AUDIO, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Game Audio container type with streaming system!");
    }
    if (!register_container(container, GSC_CLOTH_SIM, "Cloth simulation", SCM_PRIM, GSA_CLOTH_SIM_PRELOAD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering cloth sim container type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    strcpy(prim.name, "Cloth simulation");
    prim.prim_type = GSP_CLOTH_SIM;
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x20;
    prim.load_cb = cloth_sim_load_cb;
    prim.unload_cb = cloth_sim_unload_cb;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering cloth sim primitive type with streaming system!");
    }

    if (!register_container(container, GSC_DECALS, "Decals", SCM_PRIM, GSA_WORLD, decal_info_load, decal_info_unload))
    {
        keen::debug::nativePrintArgs("Error registering decal container type with streaming system!");
    }
    if (!register_container(container, GSC_DECALS_PRELOAD, "Preload Decals", SCM_PRIM, GSA_DECAL_PRELOAD, decal_info_load, decal_info_unload))
    {
        keen::debug::nativePrintArgs("Error registering decal preload container type with streaming system!");
    }
    if (!register_container(container, GSC_FSM, "Fullscreen Map", SCM_CONTAINER, GSA_FSM, fsm_loaded_callback, fsm_unloaded_callback))
    {
        keen::debug::nativePrintArgs("Error registering FSM container type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_VDOC;
    strcpy(prim.name, "VINT doc");
    prim.cpu_memalign = 4;
    prim.gpu_memalign = 4;
    prim.load_cb = interface_vint_doc_loaded_callback;
    prim.unload_cb = interface_vint_doc_unloaded_callback;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering VINT doc primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_LUA_SCRIPT;
    strcpy(prim.name, "LUA script");
    prim.cpu_memalign = 4;
    prim.gpu_memalign = 4;
    prim.load_cb = interface_script_loaded_callback;
    prim.unload_cb = interface_script_unloaded_callback;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering LUA script primitive type with streaming system!");
    }

    if (!register_container(container, GSC_UI, "UI", SCM_CONTAINER, GSA_INTERFACE_SLOT, interface_doc_loaded_callback, interface_doc_unloaded_callback))
    {
        keen::debug::nativePrintArgs("Error registering UI container type with streaming system!");
        return;
    }
    if (!register_container(container, GSC_UI_PEG, "UI PEG", SCM_PRIM, GSA_INTERFACE, interface_pegs_loaded_callback, interface_pegs_unloaded_callback))
    {
        keen::debug::nativePrintArgs("Error registering UI container type with streaming system!");
        return;
    }
    if (!register_container(container, GSC_CHUNK_PRELOAD, "Chunks Preload", SCM_PRIM, GSA_CHUNK_PRELOAD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Chunk Preload container type with streaming system!");
    }
    if (!register_container(container, GSC_CHUNK, "Chunks", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Chunk container type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.load_threaded = 1;
    prim.flags.alloc_split = 1;
    strcpy(prim.name, "Chunks");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.prim_type = GSP_CHUNK;
    prim.load_cb = chunk_base_load;
    prim.unload_cb = chunk_base_unload;
    prim.cpu_ext[0] = ".cchk_pc";
    prim.gpu_ext[0] = ".gchk_pc";
    prim.cpu_ext[1] = ".cstch_pc";
    prim.gpu_ext[1] = ".gstch_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering chunk primitive type with streaming system!");
    }

    if (!register_container(container, GSC_WORLD, "World", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering World container type with streaming system!");
    }
    if (!register_container(container, GSC_VOICE_PERSONA, "Voice Persona", SCM_CONTAINER, GSA_HUMAN_VOICE_PERSONA, human_voice_persona_stream_load, human_voice_persona_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering human voice persona container type with streaming system!");
    }
    if (!register_container(container, GSC_ALWAYS_LOADED_VOICE_PERSONA, "Always Loaded Voice Persona", SCM_CONTAINER, GSA_ALWAYS_LOADED_HUMAN_VOICE_PERSONA, human_voice_persona_stream_load, human_voice_persona_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering human voice persona container type with streaming system!");
    }
    if (!register_container(container, GSC_HUMAN, "Human", SCM_CONTAINER, GSA_HUMAN, human_info_stream_load, human_info_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Human container type with streaming system!");
    }
    if (!register_container(container, GSC_PLAYER, "Player", SCM_CONTAINER, GSA_PLAYER, human_info_stream_load, human_info_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Player container type with streaming system!");
    }
    if (!register_container(container, GSC_ITEMS, "Items", SCM_PRIM, GSA_WORLD, item_stream_load, item_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Items container type with streaming system!");
    }
    if (!register_container(container, GSC_ITEMS_PRELOAD, "Preload Items", SCM_PRIM, GSA_ITEM_PRELOAD, item_stream_load, item_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Preload Items container type with streaming system!");
    }
    if (!register_container(container, GSC_ITEMS_MP_PRELOAD, "MP Preload Items", SCM_PRIM, GSA_MP_ITEM_PRELOAD, item_stream_load, item_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Preload Items container type with streaming system!");
    }
    if (!register_container(container, GSC_ITEMS_DLC, "Items DLC", SCM_PRIM, GSA_DLC_ITEM_PRELOAD, item_stream_load, item_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Items DLC container type with streaming system!");
    }
    if (!register_container(container, GSC_WEAPON_LARGE, "Large Weapon", SCM_CONTAINER, GSA_LARGE_WEAPON, item_high_res_stream_load, item_high_res_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Large Weapon container type with streaming system!");
    }
    if (!register_container(container, GSC_WEAPON_SMALL, "Small Weapon", SCM_CONTAINER, GSA_SMALL_WEAPON, item_high_res_stream_load, item_high_res_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Small Weapon container type with streaming system!");
    }
    if (!register_container(container, GSC_SKYBOX, "Skybox", SCM_PRIM, GSA_TOD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Skybox container type with streaming system!");
    }
    if (!register_container(container, GSC_VEHICLE, "Vehicle", SCM_CONTAINER, GSA_VEHICLE, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Vehicle container type with streaming system!");
    }
    if (!register_container(container, GSC_FOLIAGE, "Foliage", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Foliage container type with streaming system!");
    }
    if (!register_container(container, GSC_GPS_PRELOAD, "GPS preload", SCM_PRIM, GSA_PERMANENT, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering GPS preload container type with streaming system!");
    }
    if (!register_container(container, GSC_GLASS, "Glass", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Glass container type with streaming system!");
    }

    prim.prim_type = GSP_PEG;
    strcpy(prim.name, "Pegs");
    prim.cpu_memalign = 4;
    prim.gpu_memalign = bm_get_peg_align_value(plat);
    prim.flags.debug_skippable = 1;
    prim.flags.alloc_split = 1;
    prim.flags.load_threaded = 1;
    prim.load_cb = peg_load_wrapper;
    prim.unload_cb = peg_unload_wrapper;
    prim.cpu_ext[0] = ".cpeg_pc";
    prim.gpu_ext[0] = ".gpeg_pc";
    prim.cpu_ext[1] = ".cvbm_pc";
    prim.gpu_ext[1] = ".gvbm_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering Peg primitive type with streaming system!");
        return;
    }

    memset(&prim, 0, sizeof(prim));
    strcpy(prim.name, "audio");
    prim.prim_type = GSP_WAVEBANK;
    prim.load_cb = gameaudio_soundbank_loaded_callback;
    prim.unload_cb = gameaudio_soundbank_unloaded_callback;
    prim.cpu_memalign = 0x800;
    prim.gpu_memalign = 0;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering Audio wavebank primitive type with streaming system!");
        return;
    }

    memset(&prim, 0, sizeof(prim));
    strcpy(prim.name, "audio");
    prim.prim_type = GSP_FOLEY_BANK;
    prim.load_cb = gameaudio_foley_bank_loaded_callback;
    prim.unload_cb = gameaudio_foley_bank_unloaded_callback;
    prim.cpu_memalign = 4;
    prim.gpu_memalign = 0;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering Audio foley bank primitive type with streaming system!");
        return;
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_VEHICLE_AUDIO;
    strcpy(prim.name, "Wavebank");
    prim.load_cb = gameaudio_file_vehicle_wavebank_loaded_callback;
    prim.unload_cb = gameaudio_file_vehicle_wavebank_unloaded_callback;
    prim.cpu_memalign = 0x800;
    prim.gpu_memalign = 0;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering Audio wavebank primitive type with streaming system!");
        return;
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    prim.prim_type = GSP_CHARACTER_MESH;
    strcpy(prim.name, "Character mesh");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = character_mesh_load_cb;
    prim.unload_cb = character_mesh_unload_cb;
    prim.cpu_ext[0] = ".ccmesh_pc";
    prim.gpu_ext[0] = ".gcmesh_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering character mesh primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_MATERIAL;
    strcpy(prim.name, "Material");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0;
    prim.load_cb = material_load;
    prim.unload_cb = material_unload;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering material primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_MESH_MORPH;
    strcpy(prim.name, "Mesh Morph");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = mesh_morph_load;
    prim.unload_cb = mesh_morph_unload;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering mesh morph primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    prim.prim_type = GSP_STATIC_MESH;
    strcpy(prim.name, "Static mesh");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = static_mesh_stream_load;
    prim.unload_cb = static_mesh_stream_unload;
    prim.cpu_ext[0] = ".csmesh_pc";
    prim.gpu_ext[0] = ".gsmesh_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering static mesh primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 0;
    prim.prim_type = GSP_FOLIAGE_MESH;
    strcpy(prim.name, "Foliage mesh");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = foliage_mesh_stream_load;
    prim.unload_cb = foliage_mesh_stream_unload;
    prim.cpu_ext[0] = ".cfmesh_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering foliage mesh primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    strcpy(prim.name, "Terrain");
    prim.prim_type = GSP_TERRAIN;
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = terrain_load_terrain;
    prim.unload_cb = terrain_unload_terrain;
    prim.cpu_ext[0] = ".cterrain_pc";
    prim.gpu_ext[0] = ".gterrain_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering terrain primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    prim.prim_type = GSP_TERRAIN_HIGH_LOD;
    strcpy(prim.name, "Terrain Hi-LOD");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = terrain_load_subzone;
    prim.unload_cb = terrain_unload_subzone;
    prim.cpu_ext[0] = ".ctmesh_pc";
    prim.gpu_ext[0] = ".gtmesh_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering chunk primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    strcpy(prim.name, "Zones");
    prim.prim_type = GSP_ZONE;
    prim.load_cb = zone_load_zone;
    prim.unload_cb = zone_unload_zone;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering zone primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_VOICE_PERSONA;
    strcpy(prim.name, "Voice Persona");
    prim.load_cb = human_voice_load_voice_persona_cb;
    prim.unload_cb = human_voice_unload_voice_persona_cb;
    prim.cpu_memalign = 4;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering human voice persona primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 0;
    prim.prim_type = GSP_VEHICLE;
    strcpy(prim.name, "Vehicles");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = vehicle_subinfo_load;
    prim.unload_cb = vehicle_subinfo_unload;
    prim.cpu_ext[0] = ".ccar_pc";
    prim.gpu_ext[0] = ".gcar_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering vehicle primitive type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.prim_type = GSP_ANIM_FILE;
    strcpy(prim.name, "Anim file");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0;
    prim.load_cb = anim_file_loaded_callback;
    prim.unload_cb = anim_file_unloaded_callback;
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering anim file primitive type with streaming system!");
    }

    if (!register_container(container, GSC_MATERIAL_EFFECT, "Material effect", SCM_PRIM, GSA_MATERIAL_EFFECT, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Glass container type with streaming system!");
    }
    if (!register_container(container, GSC_MATERIAL_PRELOAD, "Material preload", SCM_PRIM, GSA_PERMANENT, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Material Preload container type with streaming system!");
    }
    if (!register_container(container, GSC_SHARED_BACKPACK, "Backpack", SCM_PRIM, GSA_WORLD, item_stream_load, item_stream_unload))
    {
        keen::debug::nativePrintArgs("Error registering Shared Backpack container type with streaming system!");
    }
    if (!register_container(container, GSC_LANDMARK_LOD, "Landmark LODs", SCM_PRIM, GSA_WORLD, nullptr, nullptr))
    {
        keen::debug::nativePrintArgs("Error registering Landmark LOD container type with streaming system!");
    }

    memset(&prim, 0, sizeof(prim));
    prim.flags.alloc_split = 1;
    prim.prim_type = GSP_LANDMARK_LOD;
    strcpy(prim.name, "Landmark LOD");
    prim.cpu_memalign = 0x10;
    prim.gpu_memalign = 0x10;
    prim.load_cb = game_render_terrain_landmark_lod_stream;
    prim.unload_cb = game_render_terrain_landmark_lod_unstream;
    prim.cpu_ext[0] = ".clandmark_pc";
    prim.gpu_ext[0] = ".glandmark_pc";
    if (!stream2_register_primitive_type(prim, Stream2_mempool))
    {
        keen::debug::nativePrintArgs("Error registering landmark LOD primitive type with streaming system!");
    }
}
