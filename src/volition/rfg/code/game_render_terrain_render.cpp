// Original: volition/rfg/code/game_render_terrain.cpp (render half)
//
// The renderables of the streamed terrain (render_minimal.md V4): the height mesh patches of a high LOD subzone, its
// stitch pieces (chunk meshes with their skirts) and road meshes, and the low LOD patches of a territory cell. The
// TU's CPU part (game_render_terrain_load_lowlod, level init/shutdown) stays in game_render_terrain.cpp in the
// GPU-free rfg library; this half is compiled into rfg_render. It also streams the landmark LODs (lod.md L1): the
// "Landmark LODs" container's .clandmark_pc/.glandmark_pc pairs, one mesh with one material each, drawn in the scene's
// landmark LOD partition. The retail data ships none (every territory's "Landmark LODs" container is empty).
//
// Every renderable is a terrain entity (FLAGS_TERRAIN) that casts no shadows, drop shadows or terrain shadows, except
// the stitch piece meshes of chunks without skirts, which cast shadows when their box is at least 1 m^3.

#include "volition/rfg/code/game_render_terrain.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_height_mesh_instance.h"
#include "volition/render_lib/code/rl_low_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_skirt_mesh_instance.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_terrainx_shared.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// Stitch piece meshes are batchable (on in retail; unused_dcf_R_stitch_batching_toggle flips it).
bool R_stitch_batch = true; // 0x01668713
// The stitch piece meshes alive (game_render_terrain_debug warns at the maximum).
int Num_instanciated_stitch_meshes = 0; // 0x01D59C30

// The streamed landmark LODs by the hash of their file name (str_hash_string), for the unstream.
static data_hash_table<unsigned int,terrain_landmark_lod*,&hash_uint,100,unsigned char> Landmark_lod_filename_hash(false); // 0x0163F1D8

// Magic number: the stitch piece mesh maximum.
static const int MAX_STITCH_MESHES = 3000;
// Magic numbers: the landmark LOD header's signature and version.
static const unsigned int LANDMARK_LOD_SIGNATURE = 0xAB16B10B;
static const unsigned int LANDMARK_LOD_VERSION = 1;

// Not in the binary: the shadow and terrain flags every terrain renderable gets (flags & 0xFABF | 8, inlined).
static void set_terrain_render_flags(rl_renderable_instance* instance_p, bool casts_shadows)
{
    instance_p->set_casts_shadows(casts_shadows);
    instance_p->set_casts_drop_shadows(false);
    instance_p->set_casts_terrain_shadows(false);
    instance_p->set_is_terrain(true);
}

// 0x005D9AC0 (:80)
// The stitch piece's renderables. Road pieces ("_Road") are only made with do_roads, the others only without; the
// others need their chunk. Each general object of the chunk becomes a mesh instance at the piece's transform. Each
// skirt of the piece (as many as both the piece and the chunk have) becomes a generated skirt (a sub mesh of the
// subzone's stitch mesh at the skirt position); a skirt the chunk has an object skirt for also gets a blend skirt with
// its root mesh instance (the chunk mesh's skirt sub mesh at the piece's transform).
// Notes (decompiled behaviour, kept as is):
// - A road piece whose instance data lists stitched meshes reads the chunk name and mesh count through a null chunk.
// - The mismatch message is formatted and dropped, and the mesh loop runs over the chunk's general objects, not the
//   clamped count.
// - When a blend skirt or its root mesh cannot be created, the one that was created is not released.
void game_render_terrain_create_stitched_piece(terrain_stitch_instance_data* instance_data_p, terrain_data* terrain_data_p, object* objp, bool do_roads, terrain_subzone_data* subzone_data_p)
{
    const char* chunk_name = instance_data_p->stitch_chunk_name.to_pointer();
    c_chunk_base* chunk_p = nullptr;
    if (v_stricmp(chunk_name, "_Road") == 0)
    {
        if (!do_roads)
        {
            return;
        }
    }
    else
    {
        if (do_roads)
        {
            return;
        }
        chunk_p = chunk_get_base_by_name(chunk_name);
        if (chunk_p == nullptr)
        {
            return;
        }
    }
    bool chunk_has_no_skirts = false;
    int num_general_objects = 0;
    if (chunk_p != nullptr)
    {
        chunk_has_no_skirts = chunk_p->num_object_skirts == 0;
        num_general_objects = chunk_p->num_general_objects;
    }
    if (instance_data_p->num_stitched_meshes != num_general_objects)
    {
        char message[256];
        sprintf(message, "Error instantiating stitch piece %s, stitched piece was changed and checked in without resaving every zone it exists in.", chunk_p->name);
        if (chunk_p->num_general_objects <= instance_data_p->num_stitched_meshes)
        {
            instance_data_p->num_stitched_meshes = chunk_p->num_general_objects;
        }
    }
    for (int i = 0; i < num_general_objects; i++)
    {
        const general_object_base& general_object = chunk_p->general_objects.to_pointer()[i];
        if (Num_instanciated_stitch_meshes >= MAX_STITCH_MESHES)
        {
            continue;
        }
        et_ptr_offset<rl_mesh_instance,0>& slot = instance_data_p->stitched_meshes_p[i];
        rl_mesh_instance* mesh_instance_p = rl_mesh_instance::create();
        if (mesh_instance_p == nullptr)
        {
            slot.from_pointer(nullptr);
            continue;
        }
        Num_instanciated_stitch_meshes++;
        // Magic number: the minimum box volume (1 m^3) of a shadow casting stitch mesh.
        const double volume = (double)(general_object.bmax.x - general_object.bmin.x) * (double)(general_object.bmax.y - general_object.bmin.y) * (double)(general_object.bmax.z - general_object.bmin.z);
        const bool casts_shadows = chunk_has_no_skirts && (float)volume >= 1.0f;
        mesh_instance_p->init(chunk_p->mesh_p.to_pointer(), chunk_p->material_map_p.to_pointer(), general_object.mesh_subpieces[0]);
        mesh_instance_p->set_position(instance_data_p->position);
        mesh_instance_p->set_rotation(instance_data_p->rotation);
        mesh_instance_p->calculate_local_aabb();
        mesh_instance_p->set_is_batchable(R_stitch_batch);
        set_terrain_render_flags(mesh_instance_p, casts_shadows);
        game_render_scene_update(mesh_instance_p);
        slot.from_pointer(mesh_instance_p);
    }
    const int num_chunk_skirts = chunk_p != nullptr ? chunk_p->num_object_skirts : instance_data_p->num_skirts;
    for (int s = 0; s < (instance_data_p->num_skirts < num_chunk_skirts ? instance_data_p->num_skirts : num_chunk_skirts); s++)
    {
        terrain_stitch_instance_data::skirt& skirt = instance_data_p->skirts_p[s];
        const c_object_skirt* object_skirt_p = chunk_p != nullptr ? &chunk_p->object_skirts.to_pointer()[s] : nullptr;
        if (object_skirt_p != nullptr)
        {
            rl_mesh_instance* root_p = rl_mesh_instance::create();
            rl_skirt_mesh_instance* blend_p = rl_skirt_mesh_instance::create();
            if (blend_p == nullptr || root_p == nullptr)
            {
                skirt.blend_renderable_instance_p.from_pointer(nullptr);
                skirt.generated_renderable_instance_p.from_pointer(nullptr);
                continue;
            }
            root_p->init(chunk_p->mesh_p.to_pointer(), chunk_p->material_map_p.to_pointer(), object_skirt_p->rl_submesh_idx);
            root_p->set_position(instance_data_p->position);
            root_p->set_rotation(instance_data_p->rotation);
            root_p->calculate_local_aabb();
            game_render_scene_update(root_p);
            set_terrain_render_flags(root_p, false);
            root_p->set_is_batchable(R_stitch_batch);
            blend_p->init(chunk_p->mesh_p.to_pointer(), nullptr, object_skirt_p->rl_submesh_idx);
            blend_p->set_position(instance_data_p->position);
            blend_p->set_rotation(instance_data_p->rotation);
            blend_p->calculate_local_aabb();
            game_render_scene_update(blend_p);
            blend_p->set_terrain_material_map(terrain_data_p->m_terrain_material_map_p.to_pointer());
            blend_p->set_blending_enabled(true);
            blend_p->setRootMeshInstance(root_p);
            set_terrain_render_flags(blend_p, false);
            blend_p->set_is_batchable(false);
            skirt.blend_renderable_instance_p.from_pointer(blend_p);
        }
        rl_skirt_mesh_instance* generated_p = rl_skirt_mesh_instance::create();
        if (generated_p == nullptr)
        {
            skirt.generated_renderable_instance_p.from_pointer(nullptr);
            continue;
        }
        generated_p->init(subzone_data_p->stitch_renderable_p.to_pointer(), nullptr, skirt.generated_skirt_submesh_idx);
        generated_p->set_position(skirt.position);
        generated_p->set_rotation(IDENTITY_MATRIX);
        generated_p->calculate_local_aabb();
        game_render_scene_update(generated_p);
        generated_p->set_terrain_material_map(terrain_data_p->m_terrain_material_map_p.to_pointer());
        set_terrain_render_flags(generated_p, false);
        generated_p->set_blending_enabled(false);
        skirt.generated_renderable_instance_p.from_pointer(generated_p);
    }
}

// 0x005DA1F0 (:235)
// The piece's physics body, its skirts (a blend skirt with its root mesh) and its meshes.
// Notes (decompiled behaviour, kept as is): a released mesh slot is set to offset 0, which points at the slot itself.
void game_render_terrain_release_stitched_piece(terrain_stitch_instance_data* instance_data_p)
{
    if (instance_data_p->havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(instance_data_p->havok_handle, false, true, true);
        instance_data_p->havok_handle = 0xFFFFFFFF;
    }
    for (int s = 0; s < instance_data_p->num_skirts; s++)
    {
        terrain_stitch_instance_data::skirt& skirt = instance_data_p->skirts_p[s];
        rl_skirt_mesh_instance* blend_p = skirt.blend_renderable_instance_p.to_pointer();
        if (blend_p != nullptr)
        {
            blend_p->getRootMeshInstance()->release();
            blend_p->release();
            skirt.blend_renderable_instance_p.from_pointer(nullptr);
        }
        rl_skirt_mesh_instance* generated_p = skirt.generated_renderable_instance_p.to_pointer();
        if (generated_p != nullptr)
        {
            generated_p->release();
            skirt.generated_renderable_instance_p.from_pointer(nullptr);
        }
    }
    for (int i = 0; i < instance_data_p->num_stitched_meshes; i++)
    {
        et_ptr_offset<rl_mesh_instance,0>& slot = instance_data_p->stitched_meshes_p[i];
        rl_mesh_instance* mesh_instance_p = slot.to_pointer();
        if (mesh_instance_p != nullptr)
        {
            mesh_instance_p->release();
            Num_instanciated_stitch_meshes--;
            slot.set_offset(0);
        }
    }
}

// 0x005DA320 (:268)
// One mesh instance per sub mesh of the road mesh, at the road position, in the road visibility list.
void game_render_terrain_create_road_mesh(road_mesh_data* road_data_p, terrain_data* terrain_data_p)
{
    for (int i = 0; i < road_data_p->m_num_mesh_instances; i++)
    {
        rl_mesh_instance* mesh_instance_p = rl_mesh_instance::create();
        if (mesh_instance_p == nullptr)
        {
            continue;
        }
        road_data_p->m_mesh_instances_p[i].from_pointer(mesh_instance_p);
        mesh_instance_p->init(road_data_p->m_mesh_p.to_pointer(), road_data_p->m_material_map_p.to_pointer(), i);
        mesh_instance_p->set_position(road_data_p->m_position);
        mesh_instance_p->set_rotation(IDENTITY_MATRIX);
        mesh_instance_p->calculate_local_aabb();
        // Magic number: the road visibility list.
        mesh_instance_p->set_visibility_list_index(6);
        game_render_scene_update(mesh_instance_p);
        set_terrain_render_flags(mesh_instance_p, false);
    }
}

// 0x005D6C70 (:296)
void game_render_terrain_release_road_mesh(road_mesh_data* road_data_p)
{
    for (int i = 0; i < road_data_p->m_num_mesh_instances; i++)
    {
        et_ptr_offset<rl_renderable_instance,0>& slot = road_data_p->m_mesh_instances_p[i];
        rl_renderable_instance* instance_p = slot.to_pointer();
        if (instance_p != nullptr)
        {
            instance_p->release();
            slot.set_offset(0);
        }
    }
}

// 0x005DA470 (:307)
// The patch's low LOD instance (sub mesh 0 of its mesh) at the patch position with the patch box.
void game_render_terrain_create_low_lod_mesh(terrain_lowlod_patch* patch_data_p, rl_material_map* material_map_p, object* objp)
{
    rl_low_lod_mesh_instance* instance_p = rl_low_lod_mesh_instance::create();
    if (instance_p == nullptr)
    {
        return;
    }
    patch_data_p->m_renderable_p.from_pointer(instance_p);
    instance_p->init(patch_data_p->m_mesh_p.to_pointer(), material_map_p, 0);
    instance_p->set_position(patch_data_p->m_position);
    instance_p->set_rotation(IDENTITY_MATRIX);
    instance_p->set_local_aabb(rl_aabb(patch_data_p->m_aabb_min, patch_data_p->m_aabb_max));
    game_render_scene_update(instance_p);
}

// 0x005D6CC0 (:330)
void game_render_terrain_release_low_lod_mesh(terrain_lowlod_patch* patch_data_p)
{
    rl_low_lod_mesh_instance* instance_p = patch_data_p->m_renderable_p.to_pointer();
    if (instance_p != nullptr)
    {
        instance_p->release();
        patch_data_p->m_renderable_p.set_offset(0);
    }
}

// 0x005DA9B0 (:411)
// One height mesh instance per patch of the subzone (when the subzone has its mesh): a sub mesh of the subzone mesh
// with the terrain material map, its vertices relative to the subzone position, at the patch transform with the
// patch box. sidemap_data is not used.
void game_render_terrain_create_subzone_renderables(terrain_subzone_data* subzone_data_p, rl_material_map* material_map_p, terrain_sidemap_data* sidemap_data)
{
    rl_mesh* mesh_p = subzone_data_p->renderable_data.renderable_p.to_pointer();
    if (mesh_p == nullptr)
    {
        return;
    }
    for (int i = 0; i < subzone_data_p->patch_count; i++)
    {
        terrain_subzone_data::patch& patch = subzone_data_p->patches[i];
        rl_height_mesh_instance* instance_p = rl_height_mesh_instance::create();
        patch.instance_p.from_pointer(instance_p);
        if (instance_p == nullptr)
        {
            continue;
        }
        instance_p->set_renderable(mesh_p);
        instance_p->set_sub_mesh_index(patch.submesh_index);
        instance_p->set_material_map(material_map_p);
        instance_p->set_target_position(subzone_data_p->position);
        instance_p->set_local_aabb(rl_aabb(patch.local_aabb_min, patch.local_aabb_max));
        instance_p->set_position(patch.position);
        instance_p->set_rotation(patch.rotation);
        set_terrain_render_flags(instance_p, false);
        game_render_scene_update(instance_p);
    }
}

// 0x005D6CE0 (:451)
void game_render_terrain_release_subzone_renderables(terrain_subzone_data* subzone_data_p)
{
    for (int i = 0; i < subzone_data_p->patch_count; i++)
    {
        terrain_subzone_data::patch& patch = subzone_data_p->patches[i];
        rl_height_mesh_instance* instance_p = patch.instance_p.to_pointer();
        if (instance_p != nullptr)
        {
            instance_p->release();
            patch.instance_p.set_offset(0);
        }
    }
}

// 0x005DACD0
// The landmark file: the header (4-aligned), one mesh (CPU and GPU parts), the material's texture names (4-aligned:
// a byte count, then the names), the material map data and the material data (each 16-aligned). The material is
// registered in the material library and its handle becomes the map's material 0; the mesh takes the header's box.
// LTCG: cpu_offset in ECX, gpu_data_p in EDX, the rest on the stack; pName is not passed (the mesh info gets no name).
// Notes (decompiled behaviour, kept as is):
// - A header with the wrong signature or version is skipped over (cpu_offset moves past it) before the null return.
// - When rl_mesh::create fails, the mesh slot is null and the texture names, map and material are still loaded.
// - rl_material_map::create is not checked. Without material data the map's material 0 keeps the file's value.
terrain_landmark_lod* load_landmark_lod(unsigned char* cpu_data_p, int& cpu_offset, unsigned char* gpu_data_p, int& gpu_offset, const char* pName)
{
    cpu_offset += (cpu_offset & 3) == 0 ? 0 : 4 - (cpu_offset & 3);
    terrain_landmark_lod* header = (terrain_landmark_lod*)(cpu_data_p + cpu_offset);
    cpu_offset += sizeof(terrain_landmark_lod);
    if (header->m_sig != LANDMARK_LOD_SIGNATURE || header->m_version != LANDMARK_LOD_VERSION)
    {
        return nullptr;
    }
    LoadedMeshInfo mesh_info(nullptr, nullptr);
    header->m_mesh_data_p.from_pointer(rl_mesh_data_load_validated(cpu_data_p, gpu_data_p, cpu_offset, gpu_offset, &mesh_info));
    if (header->m_mesh_data_p.to_pointer() == nullptr)
    {
        return nullptr;
    }
    header->m_mesh_p.from_pointer(rl_mesh::create());
    rl_mesh* mesh_p = header->m_mesh_p.to_pointer();
    if (mesh_p != nullptr)
    {
        mesh_p->init(header->m_mesh_data_p.to_pointer(), mesh_info, false);
        mesh_p->set_local_aabb(rl_aabb(header->m_aabb_min, header->m_aabb_max));
    }
    cpu_offset += (cpu_offset & 3) == 0 ? 0 : 4 - (cpu_offset & 3);
    const char* texture_names = (const char*)(cpu_data_p + cpu_offset + 4);
    cpu_offset += 4 + *(int*)(cpu_data_p + cpu_offset);
    // The 16-byte alignments use the signed remainder (the offsets are never negative).
    cpu_offset += cpu_offset % 16 == 0 ? 0 : 16 - cpu_offset % 16;
    rl_material_map_data* map_data = rl_material_map_data_load(cpu_data_p, cpu_offset);
    cpu_offset += cpu_offset % 16 == 0 ? 0 : 16 - cpu_offset % 16;
    rl_material_data* material_data = rl_material_data_load(cpu_data_p, cpu_offset, texture_names, true);
    if (material_data == nullptr)
    {
        header->m_material_p.from_pointer(nullptr);
    }
    else
    {
        header->m_material_p.from_pointer(rl_material::create());
        header->m_material_p.to_pointer()->set_material_data(material_data);
        map_data->materials[0] = rl_material_library_register(header->m_material_p.to_pointer());
    }
    header->m_material_map_p.from_pointer(rl_material_map::create());
    header->m_material_map_p.to_pointer()->set_map_data(map_data);
    return header;
}

// 0x005D6D20 (:531)
// The mesh, the material map and the material (unregistered first). The mesh data stays in the stream's buffer.
// Notes (decompiled behaviour, kept as is): a released slot is set to offset 0, which points at the slot itself.
void unload_landmark_lod(terrain_landmark_lod* landmark_header)
{
    rl_mesh* mesh_p = landmark_header->m_mesh_p.to_pointer();
    if (mesh_p != nullptr)
    {
        mesh_p->release();
        landmark_header->m_mesh_p.set_offset(0);
    }
    rl_material_map* material_map_p = landmark_header->m_material_map_p.to_pointer();
    if (material_map_p != nullptr)
    {
        material_map_p->release();
        landmark_header->m_material_map_p.set_offset(0);
    }
    rl_material* material_p = landmark_header->m_material_p.to_pointer();
    if (material_p != nullptr)
    {
        rl_material_library_unregister(material_p);
        material_p->release();
        landmark_header->m_material_p.set_offset(0);
    }
}

// 0x005DAEB0 (:553)
// The stream2 load callback of GSP_LANDMARK_LOD: loads the landmark from the start of the CPU and GPU data and makes
// its instance (sub mesh 0 with the landmark's material map) at the header's position, unrotated, with the header's
// box, in the scene. The header is recorded by the file name's hash for the unstream. srid and the sizes are not used.
// Notes (decompiled behaviour, kept as is): a second landmark with the same file name hash (or a full table) is not
// recorded, so its unstream releases nothing.
bool game_render_terrain_landmark_lod_stream(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    int cpu_offset = 0;
    int gpu_offset = 0;
    terrain_landmark_lod* landmark_header = load_landmark_lod(cpu_data, cpu_offset, gpu_data, gpu_offset, filename);
    if (landmark_header == nullptr)
    {
        // The original formats the message into the thread's scratch string (TLS block + 0x38) first.
        keen::debug::nativePrintArgs("Failed to load landmark LOD %s", filename);
        return false;
    }
    landmark_header->m_renderable_p.from_pointer(rl_landmark_lod_mesh_instance::create());
    rl_landmark_lod_mesh_instance* instance_p = landmark_header->m_renderable_p.to_pointer();
    if (instance_p == nullptr)
    {
        unload_landmark_lod(landmark_header);
        return false;
    }
    instance_p->init(landmark_header->m_mesh_p.to_pointer(), landmark_header->m_material_map_p.to_pointer(), 0);
    instance_p->set_position(landmark_header->m_position);
    instance_p->set_rotation(IDENTITY_MATRIX);
    instance_p->set_local_aabb(rl_aabb(landmark_header->m_aabb_min, landmark_header->m_aabb_max));
    game_render_scene_update(instance_p);
    // Original bug kept: a second landmark with the same name hash is not recorded (add fails), so its unstream
    // finds the first landmark's header or nothing and never releases this one.
    Landmark_lod_filename_hash.insert_key_data(str_hash_string(filename), landmark_header);
    return true;
}

// 0x005D6D90 (:594)
// The stream2 unload callback: releases the landmark's instance and its render objects.
void game_render_terrain_landmark_lod_unstream(const char* filename, unsigned int srid)
{
    terrain_landmark_lod* landmark_header = nullptr;
    Landmark_lod_filename_hash.get(str_hash_string(filename), landmark_header);
    if (landmark_header == nullptr)
    {
        return;
    }
    rl_landmark_lod_mesh_instance* instance_p = landmark_header->m_renderable_p.to_pointer();
    if (instance_p != nullptr)
    {
        instance_p->release();
        landmark_header->m_renderable_p.set_offset(0);
    }
    unload_landmark_lod(landmark_header);
    Landmark_lod_filename_hash.remove(str_hash_string(filename));
}
