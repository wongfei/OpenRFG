// Original: volition/rfg/code/gamewide/game_stream2_register.h
// Implementation: volition/rfg/code/gamewide/game_stream2_register.cpp
//
// Registration of RFG's streaming types (decompiled from game_stream2_register_types, rfg_init_stage_2_prepare).
// Allocators (game_stream2_register_allocators): index i of game_stream_allocators gets Stream_<name>_allocator, for
// example GSA_WORLD -> Stream_world_allocator (stream2_dynamic_allocator); the preload slots are mempool allocators,
// GSA_PLAYER/HUMAN/weapons/VEHICLE/layers/voice personas/AUDIO/FSM/INTERFACE_SLOT/INTERFACE_MP_SLOT slot allocators,
// GSA_INTERFACE_STACK the system allocator.
//
// Primitive types (prim_type: name, CPU/GPU alignment, flags, load callback, extensions):
//   GSP_PEG            "Pegs"             4/peg align  skippable,split,threaded  peg_load_wrapper  .cpeg_pc/.gpeg_pc, .cvbm_pc/.gvbm_pc
//   GSP_CHUNK          "Chunks"           16/16  split,threaded  chunk_base_load            .cchk_pc/.gchk_pc, .cstch_pc/.gstch_pc
//   GSP_ZONE           "Zones"            -      -               zone_load_zone
//   GSP_TERRAIN        "Terrain"          16/16  split           terrain_load_terrain       .cterrain_pc/.gterrain_pc
//   GSP_TERRAIN_HIGH_LOD "Terrain Hi-LOD" 16/16  split           terrain_load_subzone       .ctmesh_pc/.gtmesh_pc
//   GSP_STATIC_MESH    "Static mesh"      16/16  split           static_mesh_stream_load    .csmesh_pc/.gsmesh_pc
//   GSP_CHARACTER_MESH "Character mesh"   16/16  split           character_mesh_load_cb     .ccmesh_pc/.gcmesh_pc
//   GSP_FOLIAGE_MESH   "Foliage mesh"     16/16  -               foliage_mesh_stream_load   .cfmesh_pc
//   GSP_MATERIAL       "Material"         16/0   -               material_load
//   GSP_MESH_MORPH     "Mesh Morph"       16/16  -               mesh_morph_load
//   GSP_CLOTH_SIM      "Cloth simulation" 16/32  -               cloth_sim_load_cb
//   GSP_VEHICLE        "Vehicles"         16/16  -               vehicle_subinfo_load       .ccar_pc/.gcar_pc
//   GSP_VEHICLE_AUDIO  "Wavebank"         2048/0 -               gameaudio_file_vehicle_wavebank_loaded_callback
//   GSP_VFX            "VFX"              16/16  split           effects_template_load_cb   .cefct_pc/.gefct_pc
//   GSP_WAVEBANK       "audio"            2048/0 -               gameaudio_soundbank_loaded_callback
//   GSP_FOLEY_BANK     "audio"            4/0    -               gameaudio_foley_bank_loaded_callback
//   GSP_VOICE_PERSONA  "Voice Persona"    4/0    -               human_voice_load_voice_persona_cb
//   GSP_ANIM_FILE      "Anim file"        16/0   -               anim_file_loaded_callback
//   GSP_VDOC           "VINT doc"         4/4    -               interface_vint_doc_loaded_callback
//   GSP_LUA_SCRIPT     "LUA script"       4/4    -               interface_script_loaded_callback
//   GSP_LANDMARK_LOD   "Landmark LOD"     16/16  split           game_render_terrain_landmark_lod_stream  .clandmark_pc/.glandmark_pc
//   (GSP_LOCALIZATION is not registered.)
// Container types (container_type: name, mode, allocator, load callback):
//   GSC_GLASS "Glass", GSC_EFFECTS_ENV "Effects", GSC_CHUNK "Chunks", GSC_WORLD "World", GSC_FOLIAGE "Foliage",
//     GSC_LANDMARK_LOD "Landmark LODs": SCM_PRIM, GSA_WORLD
//   GSC_DECALS "Decals": SCM_PRIM, GSA_WORLD, decal_info_load
//   GSC_ITEMS "Items", GSC_SHARED_BACKPACK "Backpack": SCM_PRIM, GSA_WORLD, item_stream_load
//   GSC_ITEMS_PRELOAD "Preload Items" (GSA_ITEM_PRELOAD), GSC_ITEMS_MP_PRELOAD "MP Preload Items" (GSA_MP_ITEM_PRELOAD),
//     GSC_ITEMS_DLC "Items DLC" (GSA_DLC_ITEM_PRELOAD): SCM_PRIM, item_stream_load
//   GSC_EFFECTS_PRELOAD "Effects Preload", GSC_MP_EFFECTS "Multiplayer Effects", GSC_EFFECTS_DLC "Effects DLC",
//     GSC_CHUNK_PRELOAD "Chunks Preload", GSC_DECALS_PRELOAD "Preload Decals" (decal_info_load), GSC_CLOTH_SIM
//     "Cloth simulation", GSC_SKYBOX "Skybox" (GSA_TOD), GSC_AUDIO "Game Audio", GSC_MATERIAL_EFFECT "Material effect",
//     GSC_MATERIAL_PRELOAD "Material preload" and GSC_GPS_PRELOAD "GPS preload" (GSA_PERMANENT): SCM_PRIM
//   GSC_HUMAN "Human" (GSA_HUMAN), GSC_PLAYER "Player" (GSA_PLAYER): SCM_CONTAINER, human_info_stream_load
//   GSC_WEAPON_LARGE "Large Weapon", GSC_WEAPON_SMALL "Small Weapon": SCM_CONTAINER, item_high_res_stream_load
//   GSC_VEHICLE "Vehicle" (GSA_VEHICLE), GSC_LAYER_SMALL "Small Layer", GSC_LAYER_LARGE "Large Layer": SCM_CONTAINER
//   GSC_VOICE_PERSONA "Voice Persona", GSC_ALWAYS_LOADED_VOICE_PERSONA "Always Loaded Voice Persona": SCM_CONTAINER,
//     human_voice_persona_stream_load
//   GSC_FSM "Fullscreen Map" (fsm_loaded_callback), GSC_UI "UI" (GSA_INTERFACE_SLOT, interface_doc_loaded_callback):
//     SCM_CONTAINER; GSC_UI_PEG "UI PEG": SCM_PRIM, GSA_INTERFACE, interface_pegs_loaded_callback
//   (GSC_ENV, GSC_STITCH and GSC_HUMAN_HEAD are not registered.)
// Each preload/slot container type uses the allocator of the same name (game_stream_allocators).
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

void game_stream2_register_allocators(); // 0x007C3840 (:127)
void game_stream2_register_types(vlib_platform plat); // 0x007C3970 (:172)
