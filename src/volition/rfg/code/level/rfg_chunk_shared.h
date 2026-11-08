// Original: volition/rfg/code/level/rfg_chunk_shared.h
// Implementation: volition/rfg/code/level/rfg_chunk_shared.cpp
//
// Destructible building chunks (.cchk_pc / .gchk_pc; stitch pieces .cstch_pc / .gstch_pc), shared with the tools
// (decompiled from chunk_base_load / chunk_base_load_internal).
// chunk_base_load is the GSP_CHUNK load callback: it takes a c_chunk_base from Chunk_base (object_pool of 352, file
// static), gives it the next internal uid and loads it with chunk_base_load_internal, then registers it in
// Chunk_base_hash under the file name without extension. The CPU file starts with a copy of c_chunk_base (signature
// 0xB0CEEFA5, version 56) followed by sections aligned to 16/64/128: the render mesh (rl_mesh_data, created as rl_mesh
// when render data is wanted), texture names, the material map and materials, the general objects (one per piece that
// is not destructible: name, bounds, initial transform, two collision model handles), the destroyables
// (rfg_base_destroyable_shape, rfg_shapes.h), the collision models (Havok shapes; handles remapped through
// hkShapeHandleScratchMem), script points, object identifiers with snap points, object skirts and light clip objects,
// then the kd trees of the destroyables. c_chunk_base::flags bits are chunk_flags.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class object;
class mempool_base;
class rl_mesh;
class rl_material;
class rl_material_map;
struct rl_fp_kdtree_base_data;
class rfg_base_destroyable_shape;
struct rbb_tree;

enum chunk_flags
{
    CHUNK_FLAG_CASTS_SHADOWS = 0,
    CHUNK_FLAG_REGENERATE = 1,
    CHUNK_FLAG_SHOW_ON_MAP = 2,
    CHUNK_FLAG_FORCE_DYNAMIC = 3,
    CHUNK_FLAG_DISABLE_COLLAPSE_EFFECT = 4,
    CHUNK_FLAG_CASTS_DROP_SHADOWS = 5,
    CHUNK_FLAG_REGROW_ON_STREAM = 6,
    CHUNK_FLAG_INHERIT_DAMAGE_PCT = 7,
    CHUNK_FLAG_INVULNERABLE = 8,
    CHUNK_FLAG_PLUME_ON_DEATH = 9,
    CHUNK_FLAG_ONE_OF_MANY = 10,
    CHUNK_FLAG_MINING = 11,
    CHUNK_FLAG_SUPPLY_CRATE = 12,
    CHUNK_FLAG_TOUCH_TERRAIN = 13,
    CHUNK_FLAG_KIOSK = 14,
    CHUNK_FLAG_PROPAGANDA = 15,
    CHUNK_FLAG_NO_COVER = 16,
    CHUNK_FLAG_WORLD_ANCHOR = 17,
    CHUNK_FLAG_DYNAMIC_LINK = 18,
    CHUNK_FLAG_BUILDING = 19,
    CHUNK_FLAG_CHILD_GIVES_CONTROL = 20,
    NUM_CHUNK_FLAGS = 21,
};

// size 0x60
struct general_object_base
{
    et_sized_native_pointer<char> name; // 0x00
    vector bmin; // 0x08
    vector bmax; // 0x14
    vector initial_pos; // 0x20
    matrix initial_orient; // 0x2C
    unsigned short collision_models[2]; // 0x50
    unsigned short flags; // 0x54
    short mesh_subpieces[2]; // 0x56
    unsigned char padding[2]; // 0x5A
};

// size 0x74
struct c_script_point
{
    matrix43 transform; // 0x00
    char name[32]; // 0x30
    char sub_piece_name[32]; // 0x50
    unsigned int destroyable_idx; // 0x70
};

// size 0x208
struct c_object_identifier
{
    unsigned int unique_id; // 0x00
    char name[24]; // 0x04
    int object_index; // 0x1C
    unsigned int is_destroyable; // 0x20
    unsigned int num_snap_points; // 0x24
    matrix43 snap_points[10]; // 0x28
};

// size 0x30
struct c_object_skirt_edge
{
    vector point[2]; // 0x00
    vector norm[2]; // 0x18
};

// size 0x10
struct c_object_skirt
{
    c_object_skirt();
    ~c_object_skirt();

    unsigned short rl_submesh_idx; // 0x00
    int num_weld_edges; // 0x04
    et_sized_native_pointer<c_object_skirt_edge> weld_edges; // 0x08
};

// size 0x58
struct c_light_clip_object
{
    et_sized_native_pointer<char> name; // 0x00
    vector bmin; // 0x08
    vector bmax; // 0x14
    vector pos; // 0x20
    matrix orient; // 0x2C
    unsigned short submesh_idx; // 0x50
    unsigned short object_identifier_index; // 0x52
};

// size 0x290
struct c_chunk_base
{
    c_chunk_base(); // 0x0091FAC0

    unsigned int signature; // 0x00
    unsigned int version; // 0x04
    unsigned int source_rfgchunkx_version; // 0x08
    unsigned int render_data_checksum; // 0x0C
    unsigned int render_cpu_data_offset; // 0x10
    unsigned int render_cpu_data_size; // 0x14
    unsigned int collision_model_checksum; // 0x18
    unsigned int collision_model_data_offset; // 0x1C
    unsigned int collision_model_data_size; // 0x20
    unsigned int destruction_checksum; // 0x24
    unsigned int destruction_offset; // 0x28
    unsigned int destruction_datasize; // 0x2C
    char name[128]; // 0x30
    char filename[128]; // 0xB0
    vector bmin; // 0x130
    vector bmax; // 0x13C
    et_sized_native_pointer<rl_mesh> mesh_p; // 0x148
    et_sized_native_pointer<rl_material_map> material_map_p; // 0x150
    et_sized_native_pointer<et_sized_native_pointer<rl_material> > rl_materials; // 0x158
    unsigned int num_rl_materials; // 0x160
    int num_general_objects; // 0x164
    et_sized_native_pointer<general_object_base> general_objects; // 0x168
    int num_base_destroyables; // 0x170
    et_sized_native_pointer<et_sized_native_pointer<rfg_base_destroyable_shape> > base_destroyables; // 0x178
    et_sized_native_pointer<rbb_tree> rbb_parent_lods; // 0x180
    et_sized_native_pointer<et_sized_native_pointer<c_chunk_base> > next; // 0x188
    et_sized_native_pointer<et_sized_native_pointer<c_chunk_base> > prev; // 0x190
    unsigned short internal_uid; // 0x198
    unsigned short inst_ref_count; // 0x19A
    et_sized_native_pointer<unsigned char> stream_data; // 0x1A0
    int data_size; // 0x1A8
    int num_script_points; // 0x1AC
    et_sized_native_pointer<c_script_point> script_points; // 0x1B0
    et_sized_native_pointer<rbb_tree> rbb_shards; // 0x1B8
    unsigned int num_object_identifiers; // 0x1C0
    et_sized_native_pointer<c_object_identifier> object_identifiers; // 0x1C8
    int num_object_skirts; // 0x1D0
    et_sized_native_pointer<c_object_skirt> object_skirts; // 0x1D8
    unsigned int mopp_handle; // 0x1E0
    unsigned int texture_names_size; // 0x1E4
    et_sized_native_pointer<char> texture_names; // 0x1E8
    int num_light_clip_objects; // 0x1F0
    et_sized_native_pointer<c_light_clip_object> light_clip_objects; // 0x1F8
    unsigned int render_gpu_data_offset; // 0x200
    unsigned int render_gpu_data_size; // 0x204
    et_sized_native_pointer<et_sized_native_pointer<rl_fp_kdtree_base_data> > base_destroyable_kdtrees; // 0x208
    et_sized_native_pointer<unsigned char> physics_data; // 0x210
    bool physics_data_is_loaded; // 0x218
    unsigned char future_use[115]; // 0x219
};

extern bool Break_on_old_chunk; // 0x02CEC14B

void unused_dcf_show_loaded_chunks(); // 0x009282A0 (:134)
void chunk_base_unload_internal(c_chunk_base* chunk_base, bool delete_render_data); // 0x00928750 (:716)
void chunk_system_init(); // 0x009287C0 (:753)
void chunk_system_shutdown(); // 0x00919FE0 (:787)
void chunk_create_preload_containers(mempool_base* pool, mempool_base* tmp_pool, vlib_platform plat, bool verify_only); // 0x009215E0 (:807)
void chunk_create_container(const char* filename, vlib_platform plat, bool preload, mempool_base* pool, mempool_base* tmp_pool); // 0x0091A030 (:841)
void chunk_level_shutdown(); // 0x0091A270 (:904)
bool chunk_base_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00936860 (:920)
void chunk_base_unload(const char* filename, unsigned int srid); // 0x00930960 (:964)
c_chunk_base* chunk_get_base_by_uid(unsigned int uid); // 0x009216F0 (:1058)
unsigned int chunk_get_base_uid(const c_chunk_base* chunk_base); // 0x00921740 (:1075)
int chunk_get_general_object_index(const c_chunk_base* chunk, unsigned int uid, object* objp); // 0x00921780 (:1084)
rfg_base_destroyable_shape* chunk_get_destroyable_object(const c_chunk_base* chunk, unsigned int uid, object* objp); // 0x00928880 (:1120)
const char* chunk_get_object_name(unsigned int chunk_uid, unsigned int obj_uid); // 0x009218E0 (:1153)
c_chunk_base* chunk_get_base_by_name(const char* name); // 0x00928A00 (:1175)
int chunk_get_num_loaded(); // 0x00928AA0 (:1187)
unsigned int chunk_get_base_uid_by_index(int index); // 0x00928AB0 (:1193)
int chunk_get_destroyable_instantiation_size(const c_chunk_base* chunk, rfg_base_destroyable_shape* shape, int& num_shards); // 0x00928B40 (:1207)
int chunk_get_instantiation_size(const c_chunk_base* chunk, int& num_shards); // 0x00928BD0 (:1230)
int chunk_get_instantiation_size(const char* filename, int& num_shards, vlib_platform plat); // 0x00928C60 (:1252)
void unused_dcf_find_chunk(); // 0x0091A2B0 (:1391)
void unused_dcf_find_chunk_texture(); // 0x0091A3C0 (:1434)

// c_chunk_base's constructor is compiler generated; the placement of the chunk file structures here is inferred.
