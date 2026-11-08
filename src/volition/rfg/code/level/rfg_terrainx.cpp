// Original: volition/rfg/code/level/rfg_terrainx.cpp
//
// Terrain loading on the game side (decompiled; the debug console commands are not implemented). Both loaders parse
// their CPU data in place: every et_ptr_offset of the file is resolved against the running offset, which also advances
// over the blocks parsed by the render library (materials, material maps, meshes) and Havok (collision models). The
// GPU data only holds mesh buffers.
// - terrain_load_terrain (.cterrain_pf, one per territory cell, version 31): header and name blocks, terrain_data, the
//   terrain materials and their map, the height field collision model, subzone slots, side map materials, path
//   finding data, stitch piece and invisible barrier collision models, the layer map (effect material per layer),
//   undergrowth, the minimap material, 3x3 low LOD patches, the low LOD material and map, occluders and (version 29+)
//   the stitch pieces with their own physics. The terrain is registered under the file name.
// - terrain_load_subzone (.ctmesh_pc, one per subzone, 3x3 per cell): terrain_subzone_data with decals, height mesh
//   patches, the subzone mesh, stitch pieces and their mesh, and road decal meshes with their materials. Its midpoint
//   decides when world::monitor_entering_zones instantiates it.
// - terrain_instantiate (from obj_zone::serialize) links the terrain to its zone object and creates the low LOD
//   renderables and stitch physics; terrain_instantiate_subzone links a loaded subzone into its terrain_data and
//   creates its renderables, roads and stitch pieces (with physics for files older than version 29).
//
// Notes (decompiled behaviour, kept as is):
// - terrain_load_terrain keeps the rfg_terrain it took from Terrains when a later step fails.
// - An already registered file name keeps the old entry in Terrain_filename_hash (the new terrain is still loaded).
// - LTCG: terrain_instantiate_stitch_physics takes the chunk base in EDI and the rotation in ESI.

#include "volition/rfg/code/level/rfg_terrainx.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_low_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/game_render_terrain.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/undergrowth.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- globals
object_pool<rfg_terrain_subzone,81,1> Terrain_subzones; // 0x02F97C60

// ---- file-static globals
static unsigned char Stitch_piece_physical_material_index_terrainx = 0xFF; // 0x01668711
static unsigned char Stitch_piece_effect_material_index_terrainx = 0xFF; // 0x01668712
static str_data_hash_table<rfg_terrain_subzone*,81,64,unsigned char> Terrain_subzone_filename_hash; // 0x02F9B058
static str_data_hash_table<rfg_terrain*,9,64,unsigned char> Terrain_filename_hash; // 0x02FD9280
static object_pool<rfg_terrain,9,1> Terrains; // 0x02FD9520

static const unsigned int TERRAIN_SIGNATURE = 0x52524554; // "TERR"
static const unsigned int SUBZONE_SIGNATURE = 0x5A425553; // "SUBZ"
static const unsigned int TERRAIN_VERSION = 31; // terrain and subzone files, older and newer ones are refused

// 0x0091FAB0
rfg_terrain_subzone::rfg_terrain_subzone()
{
}

// Advances offset to a multiple of align (a power of two).
static void align_offset(int& offset, int align)
{
    const int rest = offset % align;
    if (rest != 0)
    {
        offset += align - rest;
    }
}

// 0x009386B0 (:188)
static void terrain_mark_subzone_loaded(terrain_data* terrain_data_p, terrain_subzone_data* subzone_data_p)
{
    // The terrain points to the subzone, and the low LOD patch of the subzone stops rendering.
    if (terrain_data_p == nullptr || subzone_data_p == nullptr)
    {
        return;
    }
    terrain_data_p->m_subzones[subzone_data_p->subzone_index].from_pointer(subzone_data_p);
    terrain_data_p->m_low_lod_patches_p[subzone_data_p->subzone_index].m_renderable_p.to_pointer()->set_has_high_lod_loaded(true);
}

// 0x0092AF60 (:196)
static void terrain_mark_subzone_unloaded(terrain_data* terrain_data_p, terrain_subzone_data* subzone_data_p)
{
    if (subzone_data_p == nullptr || terrain_data_p == nullptr)
    {
        return;
    }
    terrain_data_p->m_subzones[subzone_data_p->subzone_index].set_offset(0);
    terrain_data_p->m_low_lod_patches_p[subzone_data_p->subzone_index].m_renderable_p.to_pointer()->set_has_high_lod_loaded(false);
}

// 0x0091B210 (:208)
static unsigned int terrain_instantiate_stitch_physics(c_chunk_base* base_chunk_p, object* object_p, const vector& position, const matrix& rotation)
{
    // A fixed body for the stitch piece's collision model, in the stitch piece material ("Stone-Indestructo").
    if (base_chunk_p == nullptr || base_chunk_p->mopp_handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    physics_create_info pci;
    pci.flags |= 0x1D;
    pci.shape_type = HAVOK_SHAPE_TYPE_WORLD;
    pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_FIXED;
    pci.motion_type = 5;
    pci.collision_type = COLLIDE_WORLD;
    pci.precalc_tensor.make_identity();
    pci.precalc_center_of_mass.zero();
    pci.mass = 1.0f;
    pci.friction = 1.0f;
    pci.obj_ptr = object_p;
    pci.restitution = 0.0f;
    if (Stitch_piece_physical_material_index_terrainx == 0xFF || Stitch_piece_effect_material_index_terrainx == 0xFF)
    {
        rfg_physical_material* material = rfg_get_physical_material("Stone-Indestructo");
        Stitch_piece_physical_material_index_terrainx = rfg_get_physical_material_index(material);
        Stitch_piece_effect_material_index_terrainx = rfg_get_effect_material_index(material->effect_material);
    }
    pci.pos = position;
    pci.orient = rotation;
    pci.physical_material = Stitch_piece_physical_material_index_terrainx;
    pci.cm_index = base_chunk_p->mopp_handle;
    pci.effect_material = Stitch_piece_effect_material_index_terrainx;
    return havok_body_create(pci);
}

// 0x009386F0 (:262)
bool terrain_instantiate_subzone(rfg_terrain_subzone* terrain_subzone_p, terrain_data* terrain_data_p)
{
    if (terrain_data_p == nullptr || terrain_data_p->m_object_p == nullptr || terrain_subzone_p == nullptr || terrain_subzone_p->instanciated)
    {
        return false;
    }
    terrain_subzone_data* subzone_data_p = terrain_subzone_p->subzone_data_p;
    terrain_sidemap_data* sidemap_data = terrain_data_p->m_sidemap_data.to_pointer();
    game_render_terrain_create_subzone_renderables(subzone_data_p, terrain_data_p->m_terrain_material_map_p.to_pointer(), sidemap_data);
    for (int i = 0; i < subzone_data_p->num_road_decal_meshes; i++)
    {
        game_render_terrain_create_road_mesh(&subzone_data_p->road_decal_meshes[i], terrain_data_p);
    }
    for (int i = 0; i < subzone_data_p->num_stitch_pieces; i++)
    {
        game_render_terrain_create_stitched_piece(&subzone_data_p->stitch_pieces[i], terrain_data_p, terrain_data_p->m_object_p, true, subzone_data_p);
    }
    for (int i = 0; i < subzone_data_p->num_stitch_pieces; i++)
    {
        terrain_stitch_instance_data& piece = subzone_data_p->stitch_pieces[i];
        game_render_terrain_create_stitched_piece(&piece, terrain_data_p, terrain_data_p->m_object_p, false, subzone_data_p);
        // Files before version 29 keep the stitch physics in the subzone (newer ones in the terrain, see
        // terrain_instantiate). Road pieces have none.
        if (subzone_data_p->m_header_version < 29)
        {
            const char* name = piece.stitch_chunk_name.to_pointer();
            if (v_stricmp(name, "_Road") == 0)
            {
                piece.havok_handle = 0xFFFFFFFF;
            }
            else
            {
                c_chunk_base* chunk = chunk_get_base_by_name(name);
                piece.havok_handle = terrain_instantiate_stitch_physics(chunk, terrain_data_p->m_object_p, piece.position, piece.rotation);
                hkpWorldObject* body = (hkpWorldObject*)havok_body_get_pointer(piece.havok_handle);
                if (body != nullptr)
                {
                    havok_set_property(body, 0xD, &piece);
                }
            }
        }
    }
    // The skirts of the stitch pieces render with the terrain's material map (rl_skirt_mesh_instance::m_terrain_material_map,
    // private, written at its offset 0x120 as the original does).
    for (int i = 0; i < subzone_data_p->num_stitch_pieces; i++)
    {
        terrain_stitch_instance_data& piece = subzone_data_p->stitch_pieces[i];
        for (int s = 0; s < piece.num_skirts; s++)
        {
            terrain_stitch_instance_data::skirt& skirt = piece.skirts_p[s];
            rl_skirt_mesh_instance* blend = skirt.blend_renderable_instance_p.to_pointer();
            if (blend != nullptr)
            {
                *(rl_material_map**)((unsigned char*)blend + 0x120) = terrain_data_p->m_terrain_material_map_p.to_pointer();
            }
            rl_skirt_mesh_instance* generated = skirt.generated_renderable_instance_p.to_pointer();
            if (generated != nullptr)
            {
                *(rl_material_map**)((unsigned char*)generated + 0x120) = terrain_data_p->m_terrain_material_map_p.to_pointer();
            }
        }
    }
    terrain_mark_subzone_loaded(terrain_data_p, subzone_data_p);
    terrain_subzone_p->instanciated = true;
    return true;
}

// 0x0092AFA0 (:338)
bool terrain_deinstantiate_subzone(rfg_terrain_subzone* terrain_subzone_p, terrain_data* terrain_data_p)
{
    if (terrain_subzone_p == nullptr || !terrain_subzone_p->instanciated)
    {
        return false;
    }
    terrain_subzone_data* subzone_data_p = terrain_subzone_p->subzone_data_p;
    game_render_terrain_release_subzone_renderables(subzone_data_p);
    for (int i = 0; i < subzone_data_p->num_road_decal_meshes; i++)
    {
        game_render_terrain_release_road_mesh(&subzone_data_p->road_decal_meshes[i]);
    }
    for (int i = 0; i < subzone_data_p->num_stitch_pieces; i++)
    {
        game_render_terrain_release_stitched_piece(&subzone_data_p->stitch_pieces[i]);
    }
    terrain_mark_subzone_unloaded(terrain_data_p, subzone_data_p);
    terrain_subzone_p->instanciated = false;
    return true;
}

// 0x00938970 (:370)
bool terrain_load_subzone(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (cpu_data == nullptr || cpu_data_size == 0 || gpu_data == nullptr || gpu_data_size == 0 || Terrain_subzones.get_free() < 1)
    {
        return false;
    }
    int gpu_offset = 0;
    const unsigned int signature = *(unsigned int*)cpu_data;
    const unsigned int version = *(unsigned int*)(cpu_data + 4);
    if (signature != SUBZONE_SIGNATURE)
    {
        return false;
    }
    if (version != TERRAIN_VERSION)
    {
        char message[256];
        sprintf(message, version < TERRAIN_VERSION ? "Terrain subzone '%s' is too old and needs to be re-saved" : "Terrain subzone '%s' is newer than the current version, which means you need to sync your code", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }

    // 0x14-byte header (the last word is the size of the name block that follows it).
    int offset = 0x14 + *(int*)(cpu_data + 0x10);
    align_offset(offset, 4);
    terrain_subzone_data* subzone = (terrain_subzone_data*)(cpu_data + offset);
    offset += sizeof(terrain_subzone_data);
    if (subzone->num_decals > 0)
    {
        subzone->decals.from_pointer((terrain_subzone_data::decal*)(cpu_data + offset));
        offset += subzone->num_decals * sizeof(terrain_subzone_data::decal);
        for (int i = 0; i < subzone->num_decals; i++)
        {
            terrain_subzone_data::decal& decal = subzone->decals[i];
            decal.terrain_patch_indices.from_pointer((int*)(cpu_data + offset));
            offset += decal.num_terrain_patches * 4;
        }
    }
    subzone->patches.from_pointer((terrain_subzone_data::patch*)(cpu_data + offset));
    offset += subzone->patch_count * sizeof(terrain_subzone_data::patch);
    align_offset(offset, 16);

    // The subzone mesh.
    LoadedMeshInfo mesh_info(nullptr, nullptr);
    mesh_info.name[0] = '\0';
    terrain_renderable_data& renderable = subzone->renderable_data;
    renderable.mesh_data_p.from_pointer(rl_mesh_data_load_validated(cpu_data, gpu_data, offset, gpu_offset, &mesh_info));
    renderable.renderable_p.from_pointer(rl_mesh::create());
    rl_mesh* mesh = renderable.renderable_p.to_pointer();
    if (mesh != nullptr)
    {
        mesh->init(renderable.mesh_data_p.to_pointer(), mesh_info, false);
        mesh->set_local_aabb(rl_aabb(renderable.aabb_min, renderable.aabb_max));
    }

    // Stitch pieces: each with its chunk name, skirts and stitched mesh instance slots.
    const unsigned int common_mark = Common_mempool.mark();
    align_offset(offset, 4);
    subzone->stitch_pieces.from_pointer((terrain_stitch_instance_data*)(cpu_data + offset));
    offset += subzone->num_stitch_pieces * sizeof(terrain_stitch_instance_data);
    for (int i = 0; i < subzone->num_stitch_pieces; i++)
    {
        terrain_stitch_instance_data& piece = subzone->stitch_pieces[i];
        piece.stitch_chunk_name.from_pointer((char*)(cpu_data + offset));
        offset += (int)strlen((const char*)(cpu_data + offset)) + 1;
        align_offset(offset, 4);
        piece.skirts_p.from_pointer((terrain_stitch_instance_data::skirt*)(cpu_data + offset));
        offset += piece.num_skirts * sizeof(terrain_stitch_instance_data::skirt);
        piece.stitched_meshes_p.from_pointer((et_ptr_offset<rl_mesh_instance,0>*)(cpu_data + offset));
        offset += piece.num_stitched_meshes * 4;
        align_offset(offset, 4);
    }
    // The stitch mesh, when present.
    const int has_stitch_mesh = *(int*)(cpu_data + offset);
    offset += 4;
    if (has_stitch_mesh != 0)
    {
        align_offset(offset, 16);
        align_offset(gpu_offset, 16);
        mesh_info.name[0] = '\0';
        subzone->stitch_mesh_data_p.from_pointer(rl_mesh_data_load_validated(cpu_data, gpu_data, offset, gpu_offset, &mesh_info));
        if (subzone->stitch_mesh_data_p.to_pointer() == nullptr)
        {
            return false;
        }
        subzone->stitch_renderable_p = rl_mesh::create();
        rl_mesh* stitch_mesh = subzone->stitch_renderable_p.to_pointer();
        if (stitch_mesh != nullptr)
        {
            stitch_mesh->init(subzone->stitch_mesh_data_p.to_pointer(), mesh_info, false);
        }
    }
    Common_mempool.restore_to_mark(common_mark);

    // Road decal meshes, each with its material and material map.
    align_offset(offset, 4);
    subzone->road_decal_meshes.from_pointer((road_mesh_data*)(cpu_data + offset));
    offset += subzone->num_road_decal_meshes * sizeof(road_mesh_data);
    for (int i = 0; i < subzone->num_road_decal_meshes; i++)
    {
        road_mesh_data& road = subzone->road_decal_meshes[i];
        align_offset(offset, 16);
        align_offset(gpu_offset, 16);
        mesh_info.name[0] = '\0';
        road.m_mesh_data_p.from_pointer(rl_mesh_data_load_validated(cpu_data, gpu_data, offset, gpu_offset, &mesh_info));
        road.m_mesh_p.from_pointer(rl_mesh::create());
        road.m_mesh_p.to_pointer()->init(road.m_mesh_data_p.to_pointer(), mesh_info, false);
        road.m_mesh_instances_p.from_pointer((et_ptr_offset<rl_renderable_instance,0>*)(cpu_data + offset));
        offset += road.m_num_mesh_instances * 4;
        align_offset(offset, 4);
        const char* texture_names = (const char*)(cpu_data + offset + 4);
        offset += 4 + *(int*)(cpu_data + offset);
        align_offset(offset, 16);
        rl_material_map_data* map_data = rl_material_map_data_load(cpu_data, offset);
        align_offset(offset, 16);
        rl_material_data* material_data = rl_material_data_load(cpu_data, offset, texture_names, true);
        if (material_data != nullptr)
        {
            road.m_material_p.from_pointer(rl_material::create());
            road.m_material_p.to_pointer()->set_material_data(material_data);
            road.m_material_handle = rl_material_library_register(road.m_material_p.to_pointer());
        }
        map_data->materials[0] = road.m_material_handle;
        road.m_material_map_p.from_pointer(rl_material_map::create());
        road.m_material_map_p.to_pointer()->set_map_data(map_data);
    }
    subzone->m_header_version = (int)version;

    rfg_terrain_subzone* terrain_subzone = Terrain_subzones.alloc(0);
    if (terrain_subzone == nullptr)
    {
        return false;
    }
    terrain_subzone->subzone_data_p = subzone;
    terrain_subzone->instanciated = false;
    // The midpoint of the subzone (3x3 per zone, row major), at height 0.
    vector inside(subzone->position.x + 1.0f, subzone->position.y + 0.0f, subzone->position.z + 1.0f);
    int grid_x;
    int grid_y;
    rfg_zonex_get_grid_coords(inside, grid_x, grid_y);
    vector zone_bmin;
    vector zone_bmax;
    rfg_zonex_get_zone_bbox(grid_x, grid_y, zone_bmin, zone_bmax);
    const int subzone_z = subzone->subzone_index / 3;
    const int subzone_x = subzone->subzone_index % 3;
    terrain_subzone->midpoint.x = ((float)(subzone_x + 1) * 170.33333f + zone_bmin.x + (float)subzone_x * 170.33333f + zone_bmin.x) * 0.5f;
    terrain_subzone->midpoint.y = 0.0f;
    terrain_subzone->midpoint.z = ((float)(subzone_z + 1) * 170.33333f + zone_bmin.z + (float)subzone_z * 170.33333f + zone_bmin.z) * 0.5f;
    Terrain_subzone_filename_hash.insert_key_data(filename, terrain_subzone);
    return true;
}

// Logs and fails a terrain load whose materials did not load.
static bool terrain_materials_failed(const char* filename)
{
    char message[256];
    sprintf(message, "Failed to load all materials in %s ", filename);
    return false;
}

// 0x00939180 (:621)
bool terrain_load_terrain(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (Terrains.get_free() < 1)
    {
        return false;
    }
    int gpu_offset = 0;
    terrain_header header;
    memcpy(&header, cpu_data, sizeof(terrain_header));
    if (header.signature != TERRAIN_SIGNATURE)
    {
        return false;
    }
    if (header.version != TERRAIN_VERSION)
    {
        char message[256];
        sprintf(message, header.version < TERRAIN_VERSION ? "Terrain '%s' is too old and needs to be re-saved" : "Terrain '%s' is newer than the current version, which means you need to sync your code", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }

    // Header, texture names, stitch piece names and infos, foliage mesh names, then the terrain data.
    int offset = sizeof(terrain_header);
    rfg_terrain* terrain = Terrains.alloc(0);
    offset += header.texture_names_size;
    align_offset(offset, 4);
    unsigned char* const stitch_names = cpu_data + offset;
    offset += header.stitch_piece_names_size + header.num_stitch_pieces * sizeof(terrain_stitch_info);
    align_offset(offset, 4);
    offset += header.fmesh_names_size;
    align_offset(offset, 4);
    terrain_data* data = (terrain_data*)(cpu_data + offset);
    terrain->terrain_data_p = data;
    offset += sizeof(terrain_data);
    align_offset(offset, 4);

    // Terrain materials: their texture names, the material map (with room for the side map handles appended at the
    // end) and the materials.
    const char* texture_names = (const char*)(cpu_data + offset + 4);
    offset += 4 + *(int*)(cpu_data + offset);
    align_offset(offset, 4);
    const int num_sidemap_handles = *(int*)(cpu_data + offset);
    offset += 4;
    align_offset(offset, 16);
    rl_material_map_data* map_data = rl_material_map_data_load(cpu_data, offset);
    offset += num_sidemap_handles * 4;
    align_offset(offset, 16);
    data->m_terrain_materials.from_pointer((et_ptr_offset<rl_material,0>*)(cpu_data + offset));
    offset += map_data->num_materials * 4;
    for (int i = 0; i < (int)map_data->num_materials; i++)
    {
        align_offset(offset, 16);
        rl_material_data* material_data = rl_material_data_load(cpu_data, offset, texture_names, true);
        if (material_data == nullptr)
        {
            return terrain_materials_failed(filename);
        }
        rl_material* material = rl_material::create();
        material->set_material_data(material_data);
        if (!material->validate_material(shader_type_terrain, 0))
        {
            VLIB_CRITICAL_FAILURE();
        }
        data->m_terrain_materials[i].from_pointer(material);
        map_data->materials[i] = rl_material_library_register(material);
    }
    data->m_num_terrain_materials = map_data->num_materials;

    // The height field collision model.
    if (data->m_shape_handle != 0xFFFFFFFF)
    {
        unsigned int chunk_size = 0;
        if (!havok_cm_manager_load_all(&chunk_size, cpu_data + offset, &data->m_shape_handle, 1, nullptr, filename))
        {
            data->m_shape_handle = 0xFFFFFFFF;
        }
        offset += chunk_size;
    }
    align_offset(offset, 4);
    data->m_subzones.from_pointer((et_ptr_offset<terrain_subzone_data,1>*)(cpu_data + offset));
    offset += data->m_num_subzones * 4;

    // Side map materials: the material pointers and handles, then one material per side map.
    if (data->m_num_sidemap_materials > 0)
    {
        terrain_sidemap_data* sidemap = (terrain_sidemap_data*)(cpu_data + offset);
        data->m_sidemap_data.from_pointer(sidemap);
        sidemap->materials.from_pointer((et_ptr_offset<rl_material,0>*)(cpu_data + offset + 8));
        sidemap->material_handles.from_pointer((unsigned int*)(cpu_data + offset + 8 + data->m_num_sidemap_materials * 4));
        offset += 8 + data->m_num_sidemap_materials * 8;
        for (int i = 0; i < data->m_num_sidemap_materials; i++)
        {
            align_offset(offset, 4);
            const char* sidemap_texture_names = (const char*)(cpu_data + offset + 4);
            offset += 4 + *(int*)(cpu_data + offset);
            align_offset(offset, 16);
            rl_material_data* material_data = rl_material_data_load(cpu_data, offset, sidemap_texture_names, true);
            if (material_data == nullptr)
            {
                return terrain_materials_failed(filename);
            }
            terrain_sidemap_data* sm = data->m_sidemap_data.to_pointer();
            sm->materials[i].from_pointer(rl_material::create());
            rl_material* material = sm->materials[i].to_pointer();
            material->set_material_data(material_data);
            sm->material_handles[i] = rl_material_library_register(material);
            if (!material->validate_material(shader_type_terrain, 0))
            {
                VLIB_CRITICAL_FAILURE();
            }
        }
    }

    // Path finding data: the block offsets in the file are absolute.
    align_offset(offset, 4);
    terrain_pf_data* pf = (terrain_pf_data*)(cpu_data + offset);
    data->m_pf_data_p.from_pointer(pf);
    pf->pf_data_block.from_pointer(cpu_data + pf->pf_data_block.get_offset());
    pf->pf_mopp_block.from_pointer(cpu_data + pf->pf_mopp_block.get_offset());
    pf->vehicle_pf_data_block.from_pointer(cpu_data + pf->vehicle_pf_data_block.get_offset());
    pf->vehicle_pf_mopp_block.from_pointer(cpu_data + pf->vehicle_pf_mopp_block.get_offset());
    offset += pf->block_size;
    if (pf->pf_data_block_size != 0)
    {
        rfg_pf_offsets_to_pointer((bb_pfg_layer*)pf->pf_data_block.to_pointer());
    }
    if (pf->vehicle_pf_data_block_size != 0)
    {
        rfg_pf_offsets_to_pointer((bb_pfg_layer*)pf->vehicle_pf_data_block.to_pointer());
    }

    // Stitch piece collision models (the first one is used) and the invisible barriers' models.
    const unsigned int common_mark = Common_mempool.mark();
    align_offset(offset, 4);
    const unsigned int num_stitch_models = *(unsigned int*)(cpu_data + offset);
    offset += 4;
    align_offset(offset, 16);
    unsigned int chunk_size = 0;
    unsigned int num_loaded = 0;
    unsigned int* stitch_indices = (unsigned int*)Common_mempool.alloc(num_stitch_models * 4, 0);
    if (!havok_cm_manager_load_all(&chunk_size, cpu_data + offset, stitch_indices, num_stitch_models, &num_loaded, filename))
    {
        char message[256];
        sprintf(message, "terrain_load_terrain: Failed to load the collision models for %s!\n", filename);
        keen::debug::nativePrintArgs(message);
    }
    offset += chunk_size;
    data->m_stitch_piece_cm_index = num_loaded == 0 ? 0xFFFFFFFF : stitch_indices[0];
    align_offset(offset, 4);
    data->m_invisible_barriers.from_pointer((invisible_barrier_data*)(cpu_data + offset));
    offset += data->m_num_invisible_barriers * sizeof(invisible_barrier_data);
    align_offset(offset, 16);
    unsigned int barrier_chunk_size = 0;
    unsigned int num_barrier_models = 0;
    unsigned int* barrier_indices = (unsigned int*)Common_mempool.alloc(data->m_num_invisible_barriers * 4, 0);
    if (!havok_cm_manager_load_all(&barrier_chunk_size, cpu_data + offset, barrier_indices, data->m_num_invisible_barriers, &num_barrier_models, filename))
    {
        char message[256];
        sprintf(message, "terrain_load_terrain: Failed to load the collision models for %s!\n", filename);
        keen::debug::nativePrintArgs(message);
    }
    for (int i = 0; i < data->m_num_invisible_barriers; i++)
    {
        data->m_invisible_barriers[i].cm_index = i < (int)num_barrier_models ? barrier_indices[i] : 0xFFFFFFFF;
    }
    offset += barrier_chunk_size;
    align_offset(offset, 4);
    Common_mempool.restore_to_mark(common_mark);
    align_offset(offset, 4);

    // Layer map: the effect material of each layer.
    terrain_layer_map& layer_map = data->m_layer_map;
    if (layer_map.data_size < 1)
    {
        layer_map.data_p.from_pointer(nullptr);
    }
    else
    {
        layer_map.data_p.from_pointer(cpu_data + offset);
        layer_map.material_names.from_pointer((et_ptr_offset<char,0>*)(cpu_data + offset + layer_map.data_size));
        offset += layer_map.data_size + layer_map.num_materials * 4;
        for (int i = 0; i < layer_map.num_materials; i++)
        {
            layer_map.material_names[i].from_pointer((char*)(cpu_data + offset));
            offset += (int)strlen((const char*)(cpu_data + offset)) + 1;
        }
        align_offset(offset, 4);
        layer_map.material_index.from_pointer((int*)(cpu_data + offset));
        offset += layer_map.num_materials * 4;
        for (int i = 0; i < layer_map.num_materials; i++)
        {
            layer_map.material_index[i] = rfg_get_effect_material_index(layer_map.material_names[i].to_pointer());
        }
    }
    align_offset(offset, 4);

    // Undergrowth: layers with their models, the 64x64 cell table, cell layers and single placements.
    if (data->m_num_ug_layers != 0)
    {
        undergrowth_layer_data* layers = (undergrowth_layer_data*)(cpu_data + offset);
        data->m_ug_layers.from_pointer(layers);
        offset += data->m_num_ug_layers * sizeof(undergrowth_layer_data);
        for (int i = 0; i < (int)data->m_num_ug_layers; i++)
        {
            layers[i].models.from_pointer((undergrowth_layer_model_data*)(cpu_data + offset));
            offset += layers[i].num_models * sizeof(undergrowth_layer_model_data);
        }
        for (int i = 0; i < (int)data->m_num_ug_layers; i++)
        {
            for (int m = 0; m < layers[i].num_models; m++)
            {
                layers[i].models[m].model_name.from_pointer((char*)(cpu_data + offset));
                offset += (int)strlen((const char*)(cpu_data + offset)) + 1;
            }
            align_offset(offset, 4);
        }
        data->m_ug_cell_data.from_pointer((undergrowth_cell_data*)(cpu_data + offset));
        data->m_ug_cell_layer_data.from_pointer((undergrowth_cell_layer_data*)(cpu_data + offset + 0x4000));
        offset += 0x4000 + data->m_num_ug_cell_layer_datas * sizeof(undergrowth_cell_layer_data);
        align_offset(offset, 4);
        single_undergrowth_cell_layer_data* singles = (single_undergrowth_cell_layer_data*)(cpu_data + offset);
        data->m_sug_cell_layer_data.from_pointer(singles);
        offset += data->m_num_ug_cell_layer_datas * sizeof(single_undergrowth_cell_layer_data);
        for (int i = 0; i < data->m_num_ug_cell_layer_datas; i++)
        {
            if (singles[i].num_single_ug == 0)
            {
                singles[i].single_ug.set_offset(0);
            }
            else
            {
                singles[i].single_ug.from_pointer((single_undergrowth_data*)(cpu_data + offset));
                offset += singles[i].num_single_ug * sizeof(single_undergrowth_data);
            }
        }
        undergrowth_cell_layer_data* cell_layers = data->m_ug_cell_layer_data.to_pointer();
        for (int i = 0; i < data->m_num_ug_cell_layer_datas; i++)
        {
            singles[i].num_extra_models.from_pointer((unsigned short*)(cpu_data + offset));
            offset += layers[cell_layers[i].layer_index].num_models * 2;
        }
        align_offset(offset, 4);
        // undergrowth_register_terrain_data, inlined (its create_zone call and the zone hash entry).
        undergrowth_register_terrain_data(terrain);
    }
    align_offset(offset, 4);

    // Minimap material.
    {
        const char* minimap_texture_names = (const char*)(cpu_data + offset + 4);
        offset += 4 + *(int*)(cpu_data + offset);
        align_offset(offset, 16);
        rl_material_data* material_data = rl_material_data_load(cpu_data, offset, minimap_texture_names, true);
        if (material_data == nullptr)
        {
            data->m_minimap_material_p.from_pointer(nullptr);
            data->m_minimap_material_handle = 0xFFFFFFFF;
        }
        else
        {
            data->m_minimap_material_p.from_pointer(rl_material::create());
            data->m_minimap_material_p.to_pointer()->set_material_data(material_data);
            data->m_minimap_material_handle = rl_material_library_register(data->m_minimap_material_p.to_pointer());
        }
    }

    // Low LOD patches and their material and map.
    data->m_low_lod_patches_p.from_pointer(game_render_terrain_load_lowlod(cpu_data, offset, gpu_data, gpu_offset, filename));
    align_offset(offset, 4);
    {
        const char* low_lod_texture_names = (const char*)(cpu_data + offset + 4);
        offset += 4 + *(int*)(cpu_data + offset);
        align_offset(offset, 16);
        rl_material_map_data* low_lod_map = rl_material_map_data_load(cpu_data, offset);
        align_offset(offset, 16);
        rl_material_data* material_data = rl_material_data_load(cpu_data, offset, low_lod_texture_names, true);
        if (material_data == nullptr)
        {
            data->m_low_lod_material_p.from_pointer(nullptr);
        }
        else
        {
            data->m_low_lod_material_p.from_pointer(rl_material::create());
            data->m_low_lod_material_p.to_pointer()->set_material_data(material_data);
            low_lod_map->materials[0] = rl_material_library_register(data->m_low_lod_material_p.to_pointer());
        }
        data->m_low_lod_material_map_p.from_pointer(rl_material_map::create());
        data->m_low_lod_material_map_p.to_pointer()->set_map_data(low_lod_map);
    }

    // Occluders and (version 29 and later) the stitch pieces with their own physics; their chunk names are offsets
    // into the stitch piece name block of the header.
    align_offset(offset, 4);
    data->m_occluders.from_pointer((terrain_occluder*)(cpu_data + offset));
    offset += data->m_num_occluders * sizeof(terrain_occluder);
    if (header.version < 29)
    {
        data->m_num_stitch_physics_instances = 0;
        data->m_stitch_physics_instances_p.from_pointer(nullptr);
    }
    else
    {
        align_offset(offset, 4);
        terrain_stitch_physics_instance* instances = (terrain_stitch_physics_instance*)(cpu_data + offset);
        data->m_stitch_physics_instances_p.from_pointer(instances);
        offset += data->m_num_stitch_physics_instances * sizeof(terrain_stitch_physics_instance);
        for (int i = 0; i < data->m_num_stitch_physics_instances; i++)
        {
            instances[i].m_stitch_chunk_name.from_pointer((char*)(stitch_names + instances[i].m_stitch_chunk_name.get_offset()));
        }
    }

    // The side map material handles go behind the terrain materials in the terrain's material map.
    for (int i = 0; i < num_sidemap_handles; i++)
    {
        terrain_sidemap_data* sidemap = data->m_sidemap_data.to_pointer();
        map_data->materials[map_data->num_materials] = sidemap->material_handles[i];
        map_data->num_materials++;
    }
    data->m_terrain_material_map_p.from_pointer(rl_material_map::create());
    data->m_terrain_material_map_p.to_pointer()->set_map_data(map_data);
    strncpy(terrain->filename, filename, sizeof(terrain->filename));
    rfg_terrain* registered = nullptr;
    if (!Terrain_filename_hash.get(filename, registered))
    {
        Terrain_filename_hash.insert_key_data(filename, terrain);
    }
    return true;
}

// 0x0093A0E0 (:1151)
void terrain_unload_subzone(const char* filename, unsigned int srid)
{
    rfg_terrain_subzone* terrain_subzone = nullptr;
    if (!Terrain_subzone_filename_hash.get(filename, terrain_subzone) || terrain_subzone == nullptr)
    {
        return;
    }
    terrain_subzone_data* subzone = terrain_subzone->subzone_data_p;
    rl_mesh* mesh = subzone->renderable_data.renderable_p.to_pointer();
    if (mesh != nullptr)
    {
        mesh->release();
        subzone->renderable_data.renderable_p.set_offset(0);
    }
    rl_mesh* stitch_mesh = subzone->stitch_renderable_p.to_pointer();
    if (stitch_mesh != nullptr)
    {
        stitch_mesh->release();
        subzone->stitch_renderable_p.set_offset(-1);
    }
    for (int i = 0; i < subzone->num_road_decal_meshes; i++)
    {
        road_mesh_data& road = subzone->road_decal_meshes[i];
        if (road.m_mesh_p.to_pointer() != nullptr)
        {
            road.m_mesh_p.to_pointer()->release();
            road.m_mesh_p.set_offset(0);
        }
        rl_material* material = road.m_material_p.to_pointer();
        if (material != nullptr)
        {
            rl_material_library_unregister(material);
            material->release();
            road.m_material_p.set_offset(0);
        }
        if (road.m_material_map_p.to_pointer() != nullptr)
        {
            road.m_material_map_p.to_pointer()->release();
            road.m_material_map_p.set_offset(0);
        }
    }
    Terrain_subzone_filename_hash.remove(filename);
    Terrain_subzones.free(terrain_subzone);
}

// 0x0093A250 (:1211)
void terrain_unload_terrain(const char* filename, unsigned int srid)
{
    rfg_terrain* terrain = nullptr;
    if (!Terrain_filename_hash.get(filename, terrain) || terrain == nullptr)
    {
        return;
    }
    terrain_data* data = terrain->terrain_data_p;
    for (int i = 0; i < (int)data->m_num_terrain_materials; i++)
    {
        rl_material_library_unregister(data->m_terrain_materials[i].to_pointer());
        data->m_terrain_materials[i].to_pointer()->release();
        data->m_terrain_materials[i].set_offset(0);
    }
    data->m_terrain_material_map_p.to_pointer()->release();
    data->m_terrain_material_map_p.set_offset(0);
    terrain_sidemap_data* sidemap = data->m_sidemap_data.to_pointer();
    if (sidemap != nullptr)
    {
        for (int i = 0; i < data->m_num_sidemap_materials; i++)
        {
            rl_material* material = data->m_sidemap_data.to_pointer()->materials[i].to_pointer();
            rl_material_library_unregister(material);
            material->release();
            data->m_sidemap_data.to_pointer()->materials[i].set_offset(0);
            data->m_sidemap_data.to_pointer()->material_handles[i] = 0xFFFFFFFF;
        }
    }
    if (data->m_num_ug_layers != 0)
    {
        undergrowth_unregister_terrain_data(terrain);
    }
    rl_material* minimap = data->m_minimap_material_p.to_pointer();
    if (minimap != nullptr)
    {
        rl_material_library_unregister(minimap);
        minimap->release();
        data->m_minimap_material_p.set_offset(0);
        data->m_minimap_material_handle = 0xFFFFFFFF;
    }
    for (int i = 0; i < 9; i++)
    {
        terrain_lowlod_patch& patch = data->m_low_lod_patches_p[i];
        if (patch.m_mesh_p.to_pointer() != nullptr)
        {
            patch.m_mesh_p.to_pointer()->release();
            patch.m_mesh_p.set_offset(0);
        }
    }
    if (data->m_low_lod_material_map_p.to_pointer() != nullptr)
    {
        data->m_low_lod_material_map_p.to_pointer()->release();
        data->m_low_lod_material_map_p.set_offset(0);
    }
    rl_material* low_lod_material = data->m_low_lod_material_p.to_pointer();
    if (low_lod_material != nullptr)
    {
        rl_material_library_unregister(low_lod_material);
        low_lod_material->release();
        data->m_low_lod_material_p.set_offset(0);
    }
    if (data->m_stitch_piece_cm_index != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(data->m_stitch_piece_cm_index);
        data->m_stitch_piece_cm_index = 0xFFFFFFFF;
    }
    if (data->m_shape_handle != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(data->m_shape_handle);
        data->m_shape_handle = 0xFFFFFFFF;
    }
    Terrain_filename_hash.remove(filename);
    Terrains.free(terrain);
}

// 0x0091B370 (:1354)
void terrain_system_init()
{
}

// 0x0092B030 (:1364)
rfg_terrain* terrain_find(const char* filename)
{
    rfg_terrain* terrain = nullptr;
    Terrain_filename_hash.get(filename, terrain);
    return terrain;
}

// 0x0091B380 (:1371)
terrain_loading_stats* terrain_get_loading_stats(const char* filename)
{
    return nullptr;
}

// 0x0092B070 (:1391)
bool terrain_instantiate(rfg_terrain* tp, object* objp)
{
    terrain_data* data = tp->terrain_data_p;
    data->m_object_p = objp;
    for (int i = 0; i < 9; i++)
    {
        game_render_terrain_create_low_lod_mesh(&data->m_low_lod_patches_p[i], data->m_low_lod_material_map_p.to_pointer(), objp);
    }
    if (data->m_stitch_physics_instances_p.to_pointer() != nullptr)
    {
        for (int i = 0; i < data->m_num_stitch_physics_instances; i++)
        {
            terrain_stitch_physics_instance& instance = data->m_stitch_physics_instances_p[i];
            char chunk_name[128];
            cf_drop_ext(chunk_name, 0x7F, instance.m_stitch_chunk_name.to_pointer());
            c_chunk_base* chunk = chunk_get_base_by_name(chunk_name);
            if (chunk == nullptr)
            {
                instance.m_havok_handle = 0xFFFFFFFF;
            }
            else
            {
                instance.m_havok_handle = terrain_instantiate_stitch_physics(chunk, objp, instance.m_position, instance.m_rotation);
                hkpWorldObject* body = (hkpWorldObject*)havok_body_get_pointer(instance.m_havok_handle);
                if (body != nullptr)
                {
                    havok_set_property(body, 0xE, &instance);
                }
            }
        }
    }
    return true;
}

// 0x0092B1D0 (:1432)
void terrain_deinstantiate(rfg_terrain* tp)
{
    terrain_data* data = tp->terrain_data_p;
    for (int i = 0; i < 9; i++)
    {
        game_render_terrain_release_low_lod_mesh(&data->m_low_lod_patches_p[i]);
    }
    if (data->m_stitch_physics_instances_p.to_pointer() != nullptr)
    {
        for (int i = 0; i < data->m_num_stitch_physics_instances; i++)
        {
            terrain_stitch_physics_instance& instance = data->m_stitch_physics_instances_p[i];
            if (instance.m_havok_handle != 0xFFFFFFFF)
            {
                havok_body_free(instance.m_havok_handle, false, true, true);
                instance.m_havok_handle = 0xFFFFFFFF;
            }
        }
    }
}
