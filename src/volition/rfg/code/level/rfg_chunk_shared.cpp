// Original: volition/rfg/code/level/rfg_chunk_shared.cpp
//
// Destructible building chunks (decompiled): the GSP_CHUNK load callback, the chunk pool and its name and uid lookups.
// A chunk uid is the pool index in the low 16 bits and the chunk's internal uid (a running counter) in the high 16,
// so a stale uid of a reused pool slot resolves to nullptr.
//
// Partial: the loose-file build path (chunk_create_container, chunk_create_preload_containers) and the debug console
// commands (unused_dcf_*) are not implemented.
//
// Notes (decompiled behaviour, kept as is):
// - chunk_base_load takes the pool slot and advances C_chunk_base_uid before it checks whether the chunk is already
//   loaded, so a duplicate load burns a uid.
// - chunk_load_collision_models ignores the result of havok_cm_manager_load_all; on failure the handles of the
//   previous chunk left in hkShapeHandleScratchMem are used.

#include "volition/rfg/code/level/rfg_chunk_shared.h"

#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_fp_kdtree.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world_internal.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/level/rfg_stream_layout_shared.h"
#include "volition/rfg/code/util/always_loaded_shared.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

static const unsigned int CHUNK_SIGNATURE = 0xB0CEEFA5;
static const unsigned int CHUNK_VERSION = 56; // RFGCHUNK_VERSION
static const unsigned char CONTAINER_TYPE_CHUNKS = 0x0F; // stream2 container type of streamed chunk containers
static const unsigned char CONTAINER_TYPE_CHUNKS_PRELOAD = 0x10; // the always loaded chunk containers

bool Break_on_old_chunk; // 0x02CEC14B
char hkShapeHandleScratchMem[131068]; // 0x02CCBB48, the cm index of each collision model of the chunk being loaded

// ---- file-static globals
static const char* Stitch_platform_ext[8] = {".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc", ".cstch_pc"}; // 0x01668718
static const char* Chunk_platform_ext[8] = {".cchk_pc", ".cchk_pc", ".cchk_pc", ".cchk_pc", ".cchk_pc", ".cchk_pc", ".cchk_pc", ".cchk_pc"}; // 0x01668738
static unsigned short C_chunk_base_uid; // 0x02CEC148
static bool Chunk_system_init; // 0x02CEC14A
static object_pool<c_chunk_base,352,1> Chunk_base; // 0x02FA0560
static str_data_hash_table<c_chunk_base*,352,32,unsigned short> Chunk_base_hash; // 0x02FD9BE0

// 0x0091FAC0 (compiler generated)
c_chunk_base::c_chunk_base()
{
}

// Offset of the next multiple of `alignment` (the original rounds with a signed remainder).
static int align_up(int offset, int alignment)
{
    int r = offset % alignment;
    return r == 0 ? offset : offset + (alignment - r);
}

// 0x00919F80 (:121)
// Larger chunks first.
static int chunk_size_sort(const void* a, const void* b)
{
    const c_chunk_base* chunk_a = *(c_chunk_base* const*)a;
    const c_chunk_base* chunk_b = *(c_chunk_base* const*)b;
    if (chunk_a->data_size < chunk_b->data_size)
    {
        return 1;
    }
    return chunk_a->data_size <= chunk_b->data_size ? 0 : -1;
}

// 0x00919FB0 (:173)
static rl_mesh* chunk_create_renderlib_mesh(rl_mesh_data* mesh_data, const LoadedMeshInfo& meshInfo)
{
    rl_mesh* mesh = rl_mesh::create();
    if (mesh != nullptr)
    {
        mesh->init(mesh_data, meshInfo, false);
    }
    return mesh;
}

// 0x009283D0 (:183)
// LTCG: pchunk in EDI.
static void chunk_delete_renderlib_mesh(c_chunk_base* pchunk)
{
    pchunk->mesh_p.to_pointer()->release();
    for (unsigned int i = 0; i < pchunk->num_rl_materials; i++)
    {
        rl_material* material = pchunk->rl_materials.to_pointer()[i].to_pointer();
        rl_material_library_unregister(material);
        material->release();
        pchunk->rl_materials.to_pointer()[i].from_pointer(nullptr);
    }
    pchunk->material_map_p.to_pointer()->release();
}

// 0x00928440 (:205)
// Loads the chunk's collision models and replaces the model numbers in the general objects, the destroyables and the
// mopp handle with cm indices.
static void chunk_load_collision_models(c_chunk_base* pchunk, unsigned char* data, int& offset)
{
    unsigned int cm_chunk_size = 0;
    unsigned int num_models = 0;
    unsigned int* handles = (unsigned int*)hkShapeHandleScratchMem;
    havok_cm_manager_load_all(&cm_chunk_size, data + offset, handles, 0x7FFF, &num_models, pchunk->filename);
    offset += cm_chunk_size;

    general_object_base* general_objects = pchunk->general_objects.to_pointer();
    for (int i = 0; i < pchunk->num_general_objects; i++)
    {
        general_object_base& go = general_objects[i];
        if (go.collision_models[0] != 0xFFFF)
        {
            go.collision_models[0] = (unsigned short)handles[go.collision_models[0]];
        }
        if (go.collision_models[1] != 0xFFFF)
        {
            go.collision_models[1] = (unsigned short)handles[go.collision_models[1]];
        }
        else if (go.collision_models[0] != 0xFFFF && go.mesh_subpieces[1] != -1)
        {
            // No second model: the second mesh subpiece uses the first one.
            go.collision_models[1] = go.collision_models[0];
        }
    }

    for (int i = 0; i < pchunk->num_base_destroyables; i++)
    {
        rfg_base_destroyable_shape* shape = pchunk->base_destroyables.to_pointer()[i].to_pointer();
        rfg_subpiece_base_extra_data* extra = shape->base_extra_data.to_pointer();
        for (int k = 0; k < shape->num_objects; k++)
        {
            if (extra[k].collision_model != 0xFFFF)
            {
                extra[k].collision_model = (unsigned short)handles[extra[k].collision_model];
            }
        }
        shape->init_subpiece_shapes();
    }

    if (pchunk->mopp_handle != 0xFFFFFFFF && pchunk->mopp_handle < num_models)
    {
        pchunk->mopp_handle = handles[pchunk->mopp_handle];
    }
}

// 0x00928580 (:279)
static void chunk_unload_collision_models(c_chunk_base* pchunk)
{
    general_object_base* general_objects = pchunk->general_objects.to_pointer();
    for (int i = 0; i < pchunk->num_general_objects; i++)
    {
        for (int m = 0; m < 2; m++)
        {
            if (general_objects[i].collision_models[m] != 0xFFFF)
            {
                havok_cm_manager_remove_from_list(general_objects[i].collision_models[m]);
                general_objects[i].collision_models[m] = 0xFFFF;
            }
        }
    }
    for (int i = 0; i < pchunk->num_base_destroyables; i++)
    {
        rfg_base_destroyable_shape* shape = pchunk->base_destroyables.to_pointer()[i].to_pointer();
        rfg_subpiece_base_extra_data* extra = shape->base_extra_data.to_pointer();
        for (int k = 0; k < shape->num_objects; k++)
        {
            if (extra[k].collision_model != 0xFFFF)
            {
                havok_cm_manager_remove_from_list(extra[k].collision_model);
                extra[k].collision_model = 0xFFFF;
            }
        }
    }
    if (pchunk->mopp_handle != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(pchunk->mopp_handle);
        pchunk->mopp_handle = 0xFFFFFFFF;
    }
}

// 0x009286A0 (:330)
// LTCG: chunk_base in ESI. Returns the size of the destruction section (the destroyables follow their pointer table).
static int chunk_load_destroyables(unsigned char* data, c_chunk_base* chunk_base)
{
    chunk_base->num_base_destroyables = *(int*)data;
    if (chunk_base->num_base_destroyables == 0)
    {
        chunk_base->base_destroyables.from_pointer(nullptr);
        return chunk_base->destruction_datasize;
    }
    chunk_base->base_destroyables.from_pointer((et_sized_native_pointer<rfg_base_destroyable_shape>*)(data + 4));
    int offset = chunk_base->num_base_destroyables * 8 + 4;
    for (int i = 0; i < chunk_base->num_base_destroyables; i++)
    {
        chunk_base->base_destroyables.to_pointer()[i].from_pointer(rfg_base_destroyable_shape_load(data, offset, chunk_base));
    }
    return offset;
}

// 0x00928710 (:358)
// LTCG: chunk_base in ESI.
static void chunk_unload_destroyables(c_chunk_base* chunk_base)
{
    for (int i = 0; i < chunk_base->num_base_destroyables; i++)
    {
        rfg_shape_delete_base(chunk_base->base_destroyables.to_pointer()[i].to_pointer());
    }
    chunk_base->num_base_destroyables = 0;
    chunk_base->base_destroyables.from_pointer(nullptr);
}

// 0x009360F0 (:410)
// LTCG: filename in EDX, gpu_data in ECX, base also in EDI. The CPU file starts with the c_chunk_base header (copied
// into base), then the sections listed in the header comment.
static bool chunk_base_load_internal(const char* filename, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size, c_chunk_base* base, bool init_render_data)
{
    if (gpu_data == nullptr || cpu_data == nullptr)
    {
        return false;
    }
    memcpy(base, cpu_data, sizeof(c_chunk_base));
    int gpu_offset = 0;
    base->data_size = cpu_data_size + gpu_data_size;
    strcpy(base->filename, filename);
    if (base->signature != CHUNK_SIGNATURE)
    {
        return false;
    }
    if (base->version != CHUNK_VERSION)
    {
        console_printf("RFGCHUNK_VERSION version does not match for chunk %s.  This chunk likely needs to be recrunched.\n", filename);
        if (Break_on_old_chunk)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        return false;
    }

    // Render mesh, after the header (0x290) padded to 0x2C0.
    int offset = 0x2C0;
    LoadedMeshInfo mesh_info(nullptr, nullptr);
    rl_mesh_data* mesh_data = rl_mesh_data_load_validated(cpu_data, gpu_data, offset, gpu_offset, &mesh_info);
    if (mesh_data == nullptr)
    {
        return false;
    }
    if (init_render_data)
    {
        rl_mesh* mesh = chunk_create_renderlib_mesh(mesh_data, mesh_info);
        base->mesh_p.from_pointer(mesh);
        if (mesh == nullptr)
        {
            return false;
        }
    }

    // Texture names.
    offset = align_up(offset, 0x10);
    base->texture_names_size = *(unsigned int*)(cpu_data + offset);
    offset += 4;
    base->texture_names.from_pointer((char*)(cpu_data + offset));
    offset = align_up(offset + base->texture_names_size, 0x10);

    // Material map and materials.
    rl_material_map_data* map_data = rl_material_map_data_load(cpu_data, offset);
    offset = align_up(offset, 0x10);
    base->rl_materials.from_pointer((et_sized_native_pointer<rl_material>*)(cpu_data + offset));
    offset += map_data->num_materials * 8;
    for (int i = 0; i < (int)map_data->num_materials; i++)
    {
        offset = align_up(offset, 0x10);
        rl_material_data* mat_data = rl_material_data_load(cpu_data, offset, base->texture_names.to_pointer(), init_render_data);
        if (mat_data == nullptr)
        {
            for (int k = 0; k < i; k++)
            {
                rl_material* material = base->rl_materials.to_pointer()[k].to_pointer();
                rl_material_library_unregister(material);
                material->release();
            }
            base->mesh_p.to_pointer()->release();
            return false;
        }
        if (init_render_data)
        {
            rl_material* material = rl_material::create();
            material->set_material_data(mat_data);
            base->rl_materials.to_pointer()[i].from_pointer(material);
            map_data->materials[i] = rl_material_library_register(material);
        }
    }
    base->num_rl_materials = map_data->num_materials;
    if (init_render_data)
    {
        rl_material_map* material_map = rl_material_map::create();
        base->material_map_p.from_pointer(material_map);
        material_map->set_map_data(map_data);
    }

    // General objects, each followed by its name.
    offset = align_up(offset, 0x40);
    if (base->num_general_objects < 1)
    {
        base->general_objects.from_pointer(nullptr);
    }
    else
    {
        base->general_objects.from_pointer((general_object_base*)(cpu_data + offset));
        offset += base->num_general_objects * (int)sizeof(general_object_base);
    }
    for (int i = 0; i < base->num_general_objects; i++)
    {
        char* name = (char*)(cpu_data + offset);
        base->general_objects.to_pointer()[i].name.from_pointer(name);
        offset += (int)strlen(name) + 1;
    }

    // Destroyables (the section is 128-aligned after a 64-aligned gap) and collision models.
    offset = align_up(offset, 0x40);
    offset = align_up(offset, 0x40);
    base->rbb_parent_lods.from_pointer(nullptr);
    base->rbb_shards.from_pointer(nullptr);
    offset = align_up(offset, 0x80);
    offset += chunk_load_destroyables(cpu_data + offset, base);
    offset = align_up(offset, 0x40);
    chunk_load_collision_models(base, cpu_data, offset);

    // Script points.
    offset = align_up(offset, 0x40);
    base->script_points.from_pointer((c_script_point*)(cpu_data + offset));
    offset += base->num_script_points * (int)sizeof(c_script_point);

    // Object identifiers.
    offset = align_up(offset, 0x40);
    base->object_identifiers.from_pointer((c_object_identifier*)(cpu_data + offset));
    offset += base->num_object_identifiers * (int)sizeof(c_object_identifier);

    // Object skirts, each followed by its weld edges.
    offset = align_up(offset, 0x40);
    base->object_skirts.from_pointer((c_object_skirt*)(cpu_data + offset));
    offset += base->num_object_skirts * (int)sizeof(c_object_skirt);
    for (int i = 0; i < base->num_object_skirts; i++)
    {
        c_object_skirt& skirt = base->object_skirts.to_pointer()[i];
        skirt.weld_edges.from_pointer((c_object_skirt_edge*)(cpu_data + offset));
        offset += skirt.num_weld_edges * (int)sizeof(c_object_skirt_edge);
    }

    // Light clip objects, each followed by its name.
    offset = align_up(offset, 0x40);
    base->light_clip_objects.from_pointer((c_light_clip_object*)(cpu_data + offset));
    offset += base->num_light_clip_objects * (int)sizeof(c_light_clip_object);
    for (int i = 0; i < base->num_light_clip_objects; i++)
    {
        char* name = (char*)(cpu_data + offset);
        base->light_clip_objects.to_pointer()[i].name.from_pointer(name);
        offset += (int)strlen(name) + 1;
    }

    // Kd trees of the destroyables (version 53 and later): a pointer table, then for each tree that exists its header
    // (0x14), nodes (16-aligned, 16 bytes each), leaves (16-aligned, 16 bytes each) and leaf map (2 bytes each).
    offset = align_up(offset, 0x40);
    if (base->version < 0x35)
    {
        base->base_destroyable_kdtrees.from_pointer(nullptr);
    }
    else
    {
        base->base_destroyable_kdtrees.from_pointer((et_sized_native_pointer<rl_fp_kdtree_base_data>*)(cpu_data + offset));
        offset += base->num_base_destroyables * 8;
        for (int i = 0; i < base->num_base_destroyables; i++)
        {
            et_sized_native_pointer<rl_fp_kdtree_base_data>& entry = base->base_destroyable_kdtrees.to_pointer()[i];
            if (entry.to_pointer() == nullptr)
            {
                continue;
            }
            rl_fp_kdtree_base_data* tree = (rl_fp_kdtree_base_data*)(cpu_data + offset);
            entry.from_pointer(tree);
            offset = align_up(offset + 0x14, 0x10);
            tree->nodes.from_pointer((rl_fp_kdtree_node*)(cpu_data + offset));
            offset = align_up(offset + tree->num_nodes * 0x10, 0x10);
            tree->leaves.from_pointer((rl_fp_kdtree_leaf*)(cpu_data + offset));
            offset += tree->num_leaves * 0x10;
            tree->leaf_map.from_pointer((unsigned short*)(cpu_data + offset));
            offset += tree->num_leaves * 2;
        }
    }
    base->inst_ref_count = 0;
    return true;
}

// 0x00928750 (:716)
void chunk_base_unload_internal(c_chunk_base* chunk_base, bool delete_render_data)
{
    chunk_base->internal_uid = 0xFFFF;
    chunk_unload_collision_models(chunk_base);
    chunk_unload_destroyables(chunk_base);
    if (delete_render_data)
    {
        chunk_delete_renderlib_mesh(chunk_base);
    }
}

// 0x009287C0 (:753)
// Reads the chunk container table and preloads the always loaded chunk containers.
void chunk_system_init()
{
    if (Chunk_system_init)
    {
        return;
    }
    Chunk_system_init = true;
    if (Use_packfiles)
    {
        char filename[64];
        cf_add_ext(filename, sizeof(filename), "chunk_containers", ".asm_pc", false);
        stream2_read_container_data(filename, &Common_mempool, &Permanent_mempool, false, false, false);
    }
    else
    {
        chunk_create_preload_containers(&Permanent_mempool, &Common_mempool, PLATFORM_PC, false);
    }
    stream2_container* srid = Stream_containers;
    while (srid != nullptr)
    {
        if (srid->cdef->container_type == CONTAINER_TYPE_CHUNKS_PRELOAD)
        {
            stream2_preload((unsigned int)srid);
        }
        srid = srid->next;
        if (srid == Stream_containers)
        {
            break;
        }
    }
}

// 0x00919FE0 (:787)
void chunk_system_shutdown()
{
    if (!Chunk_system_init)
    {
        return;
    }
    Chunk_system_init = false;
    stream2_container* srid = Stream_containers;
    while (srid != nullptr)
    {
        stream2_container* next = srid->next != Stream_containers ? srid->next : nullptr;
        if (srid->cdef->container_type == CONTAINER_TYPE_CHUNKS_PRELOAD)
        {
            stream2_release((unsigned int)srid);
        }
        srid = next;
    }
}

// 0x009215E0 (:807)
// The asset assembler's chunk_containers system: one preloaded container per chunk of preload_chunks.xtbl, each name
// also added to the always-loaded files (the build path without preload files, stub_impl.md S3).
void chunk_create_preload_containers(mempool_base* pool, mempool_base* tmp_pool, vlib_platform plat, bool verify_only)
{
    assasm_start("chunk_containers", tmp_pool, pool, false, nullptr, false, false, true, false);
    xml_element* root = xml_table_parse("preload_chunks.xtbl");
    if (root == nullptr)
    {
        // original: the assembler system stays open
        return;
    }
    for (xml_element* element = xml_find(root, "preload_chunks"); element != nullptr; element = xml_find_next(root, element, "preload_chunks"))
    {
        xml_element* chunk = xml_find(element, "Chunk");
        if (chunk == nullptr)
        {
            continue;
        }
        xml_element* chunk_filename = xml_find(chunk, "Filename");
        if (chunk_filename == nullptr)
        {
            continue;
        }
        char name[64];
        cf_add_ext(name, sizeof(name), chunk_filename->text, Chunk_platform_ext[plat], false);
        chunk_create_container(name, plat, true, pool, tmp_pool);
        always_loaded_add_file(name);
    }
    assasm_finish(nullptr, verify_only, true, false);
    xml_table_close(root);
}

// 0x0091A030 (:841)
// The stream2 container of a chunk (.rfgchunkx) or stitch piece (.rfgstitchx) unless it exists: the chunk file and the
// textures its .tlist rule lists for the platform.
void chunk_create_container(const char* filename, vlib_platform plat, bool preload, mempool_base* pool, mempool_base* tmp_pool)
{
    bool stitch = false;
    const char* container_ext = ".rfgchunkx";
    const char* ext = cf_get_ext(filename);
    if (ext != nullptr && v_stricmp(ext, Stitch_platform_ext[plat]) == 0)
    {
        container_ext = ".rfgstitchx";
        stitch = true;
    }
    char container_name[64];
    cf_add_ext(container_name, sizeof(container_name), filename, container_ext, false);
    if (stream2_container_find(container_name) != 0)
    {
        return;
    }
    // container type 0x0F (0x10 preloaded), primitive type 2 (chunk) with allocator 1 (2 preloaded)
    stream2_container_start(container_name, pool, (unsigned char)(preload + 0x0F), !preload, false);
    stream2_container_add_prim(filename, Layout_category_name[stitch ? 3 : 4], 2, (unsigned char)(preload + 1), true, 0, nullptr);
    char tlist_name[64];
    cf_add_ext(tlist_name, sizeof(tlist_name), filename, ".tlist", false);
    mempool_mark_scope scope(tmp_pool);
    xml_element* root = xml_parse(tlist_name, tmp_pool);
    xml_element* rule = root != nullptr ? xml_find(root, "rule") : nullptr;
    for (xml_element* platform = rule != nullptr ? xml_find(rule, "platform") : nullptr; platform != nullptr; platform = xml_find_next(rule, platform, "platform"))
    {
        if (vlib_find_platform(platform->text) != plat)
        {
            continue;
        }
        for (xml_element* source = xml_find(platform, "source"); source != nullptr; source = xml_find_next(platform, source, "source"))
        {
            if (v_stricmp(cf_get_ext(source->text), Vbm_platform_ext[plat]) == 0)
            {
                // primitive type 1 (peg), allocator 1; flag 8 for a file of the preload packfiles
                const bool preload_file = assasm_is_preload_file(source->text);
                stream2_container_add_prim(source->text, Layout_category_name[5], 1, 1, false, preload_file ? 8 : 0, nullptr);
            }
        }
        break;
    }
    stream2_container_finish(false, false);
}

// 0x0091A270 (:904)
// Deletes the streamed chunk containers of the level.
void chunk_level_shutdown()
{
    stream2_container* srid = Stream_containers;
    while (srid != nullptr)
    {
        stream2_container* next = srid->next != Stream_containers ? srid->next : nullptr;
        if (srid->cdef->container_type == CONTAINER_TYPE_CHUNKS)
        {
            stream2_container_delete((unsigned int)srid);
        }
        srid = next;
    }
}

// 0x00936860 (:920)
bool chunk_base_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (Chunk_base.get_free() < 1)
    {
        return false;
    }
    c_chunk_base* base = Chunk_base.alloc(0);
    base->internal_uid = C_chunk_base_uid++;
    char name[128];
    cf_drop_ext(name, 0x7F, filename);
    if (!Chunk_base_hash.exists(name) &&
        chunk_base_load_internal(filename, cpu_data, cpu_data_size, gpu_data, gpu_data_size, base, true))
    {
        strcpy(base->name, name);
        Chunk_base_hash.insert_key_data(name, base);
        return true;
    }
    Chunk_base.free(base);
    return false;
}

// 0x00930960 (:964)
void chunk_base_unload(const char* filename, unsigned int srid)
{
    char name[64];
    cf_drop_ext(name, 0x40, filename);
    c_chunk_base* chunk_base = chunk_get_base_by_name(filename);
    if (chunk_base != nullptr)
    {
        chunk_base_unload_internal(chunk_base, true);
        Chunk_base_hash.remove(name);
        Chunk_base.free(chunk_base);
    }
}

// 0x009216F0 (:1058)
c_chunk_base* chunk_get_base_by_uid(unsigned int uid)
{
    if (uid == 0xFFFFFFFF || (uid & 0xFFFF) >= Chunk_base.get_capacity())
    {
        return nullptr;
    }
    c_chunk_base* base = Chunk_base.get_entry((int)(uid & 0xFFFF));
    return base->internal_uid == (unsigned short)(uid >> 16) ? base : nullptr;
}

// 0x00921740 (:1075)
unsigned int chunk_get_base_uid(const c_chunk_base* chunk_base)
{
    if (chunk_base == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return ((unsigned int)Chunk_base.get_index(chunk_base) & 0xFFFF) | ((unsigned int)chunk_base->internal_uid << 16);
}

// The error path of the two object lookups: names the zone of objp after the message (the zone is found from the
// grid position in the object's handle, world::get_world_zone_by_object_handle inlined).
static void chunk_append_zone_name(char* message, world_zone* zone)
{
    if (zone == nullptr || zone->zone_objp == nullptr)
    {
        return;
    }
    char suffix[256];
    sprintf(suffix, ": %s\n", zone->zone_objp->get_name());
    strcat(message, suffix);
}

// 0x00921780 (:1084)
// The general object (non-destructible piece) with the given object identifier, or -1.
int chunk_get_general_object_index(const c_chunk_base* chunk, unsigned int uid, object* objp)
{
    const c_object_identifier* ids = chunk->object_identifiers.to_pointer();
    for (unsigned int i = 0; i < chunk->num_object_identifiers; i++)
    {
        if (ids[i].unique_id != uid)
        {
            continue;
        }
        char message[512];
        if (ids[i].is_destroyable == 0)
        {
            if (ids[i].object_index >= chunk->num_general_objects)
            {
                console_printf("Found non-destroyable object in chunk %s with a bad index: %d, num_general_objects: %d. Please fix chunk", chunk->name, ids[i].object_index, chunk->num_general_objects);
                return -1;
            }
            return ids[i].object_index;
        }
        sprintf(message, "Found destroyable in %s while expecting a non-destroyable. Please resave zone", chunk->name);
        chunk_append_zone_name(message, objp != nullptr ? World.get_world_zone_by_object_handle(objp->get_handle()) : nullptr);
        console_printf("%s", message);
        return -1;
    }
    return -1;
}

// 0x00928880 (:1120)
// The destroyable with the given object identifier, or nullptr.
rfg_base_destroyable_shape* chunk_get_destroyable_object(const c_chunk_base* chunk, unsigned int uid, object* objp)
{
    const c_object_identifier* ids = chunk->object_identifiers.to_pointer();
    for (unsigned int i = 0; i < chunk->num_object_identifiers; i++)
    {
        if (ids[i].unique_id != uid)
        {
            continue;
        }
        char message[512];
        if (ids[i].is_destroyable != 0)
        {
            if (ids[i].object_index >= chunk->num_base_destroyables)
            {
                console_printf("Found destroyable object in chunk %s with a bad index: %d, num_base_destroyables: %d. Please fix chunk", chunk->name, ids[i].object_index, chunk->num_base_destroyables);
                return nullptr;
            }
            return chunk->base_destroyables.to_pointer()[ids[i].object_index].to_pointer();
        }
        sprintf(message, "Found non-destroyable object in chunk %s when we were expecting a destroyable. Please resave zone", chunk->name);
        chunk_append_zone_name(message, objp != nullptr ? World.get_world_zone_by_object_handle(objp->get_handle()) : nullptr);
        console_printf("%s", message);
        return nullptr;
    }
    return nullptr;
}

// 0x009218E0 (:1153)
const char* chunk_get_object_name(unsigned int chunk_uid, unsigned int obj_uid)
{
    const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr)
    {
        return nullptr;
    }
    const c_object_identifier* ids = chunk->object_identifiers.to_pointer();
    for (unsigned int i = 0; i < chunk->num_object_identifiers; i++)
    {
        if (ids[i].unique_id == obj_uid)
        {
            return ids[i].name;
        }
    }
    return nullptr;
}

// 0x00928A00 (:1175)
c_chunk_base* chunk_get_base_by_name(const char* name)
{
    char key[128];
    cf_drop_ext(key, 0x7F, name);
    c_chunk_base* chunk = nullptr;
    Chunk_base_hash.get(key, chunk);
    return chunk;
}

// 0x00928AA0 (:1187)
int chunk_get_num_loaded()
{
    return (int)Chunk_base.get_capacity() - Chunk_base.get_free();
}

// 0x00928AB0 (:1193)
unsigned int chunk_get_base_uid_by_index(int index)
{
    c_chunk_base* chunk = Chunk_base.get_first(0);
    int i = 0;
    for (; i < index; i++)
    {
        if (chunk == nullptr)
        {
            return 0xFFFFFFFF;
        }
        chunk = Chunk_base.get_next(chunk);
    }
    if (chunk != nullptr && index <= i)
    {
        return chunk_get_base_uid(chunk);
    }
    return 0xFFFFFFFF;
}

// 0x00928B40 (:1207)
// Instance buffer plus render instance size of one destroyable; num_shards is its piece count.
int chunk_get_destroyable_instantiation_size(const c_chunk_base* chunk, rfg_base_destroyable_shape* shape, int& num_shards)
{
    const int data_size = shape->inst_data.to_pointer()->data_size;
    num_shards = shape->num_objects;
    const int num_sub_instances = shape->num_dlods + shape->num_objects;
    bool dynamic_kdtree = true;
    et_sized_native_pointer<rl_fp_kdtree_base_data>* kdtrees = chunk->base_destroyable_kdtrees.to_pointer();
    if (kdtrees != nullptr)
    {
        for (int i = 0; i < chunk->num_base_destroyables; i++)
        {
            if (chunk->base_destroyables.to_pointer()[i].to_pointer() == shape)
            {
                dynamic_kdtree = kdtrees[i].to_pointer() == nullptr;
                break;
            }
        }
    }
    return rl_destroyable_model_instance::calculate_memory_cost(num_sub_instances, dynamic_kdtree) + data_size;
}

// 0x00928BD0 (:1230)
int chunk_get_instantiation_size(const c_chunk_base* chunk, int& num_shards)
{
    int render_size = 0;
    int data_size = 0;
    num_shards = 0;
    for (int i = 0; i < chunk->num_base_destroyables; i++)
    {
        rfg_base_destroyable_shape* shape = chunk->base_destroyables.to_pointer()[i].to_pointer();
        data_size += shape->inst_data.to_pointer()->data_size;
        et_sized_native_pointer<rl_fp_kdtree_base_data>* kdtrees = chunk->base_destroyable_kdtrees.to_pointer();
        const bool dynamic_kdtree = kdtrees == nullptr || kdtrees[i].to_pointer() == nullptr;
        render_size += rl_destroyable_model_instance::calculate_memory_cost(shape->num_dlods + shape->num_objects, dynamic_kdtree);
        num_shards += shape->num_objects;
    }
    return render_size + data_size;
}

// 0x00928C60 (:1252)
int chunk_get_instantiation_size(const char* filename, int& num_shards, vlib_platform plat)
{
    num_shards = 0;
    char name[256];
    cf_drop_path(name, 0x100, filename);
    c_chunk_base* chunk = chunk_get_base_by_name(name);
    if (chunk != nullptr)
    {
        return chunk_get_instantiation_size(chunk, num_shards);
    }
    keen::debug::nativePrintArgs("chunk_get_instantiation_size(filename) should never be called from a non-tools build, it is slow");
    return 0;
}
