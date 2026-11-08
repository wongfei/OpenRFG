// Original: volition/render_lib/code/rl_clone_mesh_instance.h
// Implementation: volition/render_lib/code/rl_clone_mesh_instance.cpp
//
// rl_clone_mesh_instance, many copies of one mesh drawn instanced (foliage, debris), managed in rl_clone_mesh_pool
// with per-element data, LODs, fade ranges, wind and splines.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_fp_vector.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

class mempool;
class mempool_base;
class rl_batched_pass;
class rl_camera;
class rl_class_factory_info;
class rl_clone_mesh_element;
struct rl_clone_mesh_info;
class rl_clone_mesh_instance;
struct rl_clone_mesh_lod_desc;
struct rl_clone_mesh_wind_info;
struct rl_clone_spline_info;
class rl_decal;
class rl_material_map;
class rl_mesh;
class rl_renderer;
struct rl_sub_instance;

enum rl_clone_mesh_element_type
{
    RLCMET_INSTANCES = 0,
    RLCMET_SPLINE = 1,
    NUM_RL_CLONE_MESH_ELEMENT_TYPES = 2,
};

// size 0x1C
class rl_clone_mesh_pool : public rl_base_object
{
    friend void rl_clone_mesh_take_census(const rl_clone_mesh_pool* pool, struct rl_clone_mesh_census& census); // not in the binary
protected:
    rl_clone_mesh_pool(); // 0x004BBAD0 (.h:337)
    virtual ~rl_clone_mesh_pool(); // vtbl+0x00 0x004BBAF0 (.h:343)
private:
    static const pool_list<rl_clone_mesh_pool*>& get_object_pool(); // 0x004BBA90 (.h:195)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BBAA0 (.h:195)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00509FB0 (:203)
    static rl_clone_mesh_pool* create_placement(rl_clone_mesh_pool*);
    static rl_clone_mesh_pool* create(); // 0x005216D0 (:203)
    static bool can_create(); // 0x004E2F60 (:203)
    virtual void release(); // vtbl+0x04 0x005379C0 (:203)
    void init(int max_num_instances, rl_clone_mesh_element* instance_array); // 0x004BC3E0 (:233)
    void init(int max_num_instances, mempool_base* pool); // 0x004E2F70 (:219)
    rl_clone_mesh_element* alloc_clones(int num_clones); // 0x004BC400 (:250)
    void free_clones(rl_clone_mesh_element* clones, int num_clones); // 0x004BC420 (:266)
    bool can_alloc(int num_clones); // 0x0091BE20 (.h:205)
    void add_instance(rl_clone_mesh_instance* new_inst); // 0x004BC490 (:307)
    void remove_instance(rl_clone_mesh_instance* inst); // 0x004BC4F0 (:316)
    int get_max_clone_instances() const;

    static const unsigned int m___class_id; // 0x012BECBC
    static rl_class_factory_info ms_class_factory_info; // 0x015147D8
protected:
    static pool_list<rl_clone_mesh_pool*> ms_object_pool; // 0x01514808
private:
    int m_max_clone_instances; // 0x08
    int m_available_clones; // 0x0C
    rl_clone_mesh_element* m_clones_array; // 0x10
    bool m_dyn_alloc; // 0x14
    rl_clone_mesh_instance* m_instance_list; // 0x18
};

// size 0x8
struct rl_clone_mesh_fade_range
{
    float start; // 0x00
    float end; // 0x04
};

// size 0x160
class rl_clone_mesh_instance : public rl_renderable_instance
{
    // Access inferred: the pool keeps its instances in a ring (next, prev) and moves their clones (m_elements).
    friend class rl_clone_mesh_pool;
    friend void rl_clone_mesh_take_census(const rl_clone_mesh_pool* pool, struct rl_clone_mesh_census& census); // not in the binary
protected:
    rl_clone_mesh_instance(); // 0x00548F00 (.h:353)
    virtual ~rl_clone_mesh_instance(); // vtbl+0x00 0x00548F70 (.h:364)
private:
    static const pool_list<rl_clone_mesh_instance*>& get_object_pool(); // 0x004BBAB0 (.h:234)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BBAC0 (.h:234)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050A0B0 (:331)
    static rl_clone_mesh_instance* create_placement(rl_clone_mesh_instance*);
    static rl_clone_mesh_instance* create(); // 0x00548FC0 (:331)
    static bool can_create(); // 0x004E2FD0 (:331)
    virtual void release(); // vtbl+0x04 0x00537A40 (:331)
protected:
    void init(rl_clone_mesh_instance* sibling, rl_clone_mesh_element* split_clones, int num_split_clones, rl_clone_mesh_element_type element_type, int num_clones_per_segment); // 0x0050A600 (:408)
public:
    bool init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_clone_mesh_pool* parent_pool, rl_clone_mesh_element* clone_element_p, int num_clone_elements, rl_clone_mesh_element_type element_type, float spline_tension, int num_clones_per_segment); // 0x0050A1B0 (:344)
    void set_fade_range(const rl_clone_mesh_fade_range& fade_range); // 0x0084BC60 (.h:239)
    void set_lod_descs(const rl_clone_mesh_lod_desc* lod_descs, int num_lods); // 0x0084BC80 (.h:240)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x00521740 (:720)
    virtual void add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids); // vtbl+0x1C 0x004BC570 (:429)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x004BC580 (:446)
    virtual void on_visible_for_frame(rl_renderer& renderer); // vtbl+0x0C 0x00548F60 (.h:260)
    static void set_wind_info(const rl_clone_mesh_wind_info& wind_info); // 0x004BC7D0 (:1043)
    void set_wind_reference_height(float height); // 0x0091BE30 (.h:263)
    void set_wind_animated(bool is_animated); // 0x0091BE50 (.h:264)
    const rl_clone_mesh_element* get_clones(int& num_clones); // 0x0091BE60 (.h:267)
    void delete_clones(int* clone_indices, int num_clones); // 0x004BC720 (:553)
    rl_clone_mesh_instance* split_clones(int* clone_indices, int num_clones); // 0x00549020 (:570)
    virtual void set_material_map(const rl_material_map* mat_p); // vtbl+0x4C 0x004BC790 (:594)
    virtual const rl_material_map* get_material_map(); // vtbl+0x50 0x00548F50 (.h:273)
protected:
    void determine_lod_range(int& min_lod, int& max_lod, rl_renderer& renderer); // 0x0050A990 (:464)
    void calculate_instance_params(int lod_index); // 0x004BC590 (:514)
    int get_num_clones(); // 0x004E2FE0 (:602)
    void get_clone(int clone_index, rl_clone_mesh_info& _info); // 0x004E3020 (:613)
private:
    void process_internal(rl_renderer& renderer); // 0x0050AAF0 (:1028)
    void process_internal_lod(rl_renderer& renderer, int lod_index); // 0x004BC7C0 (:668)

public:
    static const unsigned int m___class_id; // 0x012BECC0
    static rl_class_factory_info ms_class_factory_info; // 0x01514820
protected:
    static pool_list<rl_clone_mesh_instance*> ms_object_pool; // 0x01514850
    rl_clone_mesh_element_type m_element_type; // 0x110
    rl_clone_mesh_element* m_elements; // 0x114
    int m_num_elements; // 0x118
    const rl_material_map* m_mat_map; // 0x11C
    rl_clone_mesh_fade_range m_fade_range; // 0x120
    int m_num_lods; // 0x128
    const rl_clone_mesh_lod_desc* m_lod_descs; // 0x12C
    float m_wind_reference_height; // 0x130
    bool m_wind_animated; // 0x134
    int m_num_clones_per_segment; // 0x138
    float m_spline_tension; // 0x13C
    rl_clone_mesh_pool* m_parent_pool; // 0x140
    rl_clone_mesh_instance* next; // 0x144
    rl_clone_mesh_instance* prev; // 0x148
private:
    int m_min_lod; // 0x14C
    int m_max_lod; // 0x150
    unsigned int m_last_processed_frame; // 0x154
};

// size 0x20
class rl_clone_mesh_element
{
public:
    rl_clone_mesh_element(); // 0x004BBA60 (.h:62)
    void set(const rl_clone_spline_info& _info); // 0x0084BC20 (.h:151)
    void set(const rl_clone_mesh_info& _info); // 0x00922F10 (.h:88)
    void get(rl_clone_spline_info& _info); // 0x004BBA70 (.h:165)
    void get(rl_clone_mesh_info& _info) const; // 0x004E2AD0 (.h:118)

    union
    {
        struct
        {
            rl_fp_vector8 data0; // 0x00
            rl_fp_vector8 data1; // 0x10
        };
        struct
        {
            vector4 knot_pos; // 0x00
            vector4 knot_data; // 0x10
        };
    };
};

// size 0x44
struct rl_clone_mesh_info
{
    rl_clone_mesh_info(); // 0x004BC560

    vector pos; // 0x00
    matrix orient; // 0x0C
    rl_color_float color; // 0x30
    float rand_value; // 0x40
};

// size 0x18
struct rl_clone_mesh_lod_desc
{
    rl_clone_mesh_fade_range fade_in_range; // 0x00
    rl_clone_mesh_fade_range fade_out_range; // 0x08
    unsigned int submesh_index; // 0x10
    unsigned int flags; // 0x14
};

// size 0x60
struct rl_clone_mesh_wind_info
{
    rl_clone_mesh_wind_info(); // 0x0091BEB0

    vector control_vectors[8]; // 0x00
};

// size 0x10
struct rl_clone_spline_info
{
    rl_clone_spline_info(); // 0x004BC7B0

    vector knot_pos; // 0x00
    float knot_parameter_offset; // 0x0C
};

// size 0xC
struct split_base_array_params
{
    int* array_size; // 0x00
    rl_sub_instance** sub_insts_array; // 0x04
    rl_decal** decal_list; // 0x08
};

// size 0x8
struct split_clone_array_params
{
    int* array_size; // 0x00
    rl_clone_mesh_element** clone_array; // 0x04
};

// Not in the binary (lod.md L2 test census): a pool's instances and what the latest render drew of them (the
// instances processed on the latest frame any of them was, by LOD: an instance in its cross-fade counts for each LOD
// it draws).
struct rl_clone_mesh_census
{
    int instances;
    int clones;
    int pool_clones_used; // the pool's clones in use (max - available)
    int pool_clones_max;
    int drawn_instances;
    int drawn_by_lod[4]; // instances drawing LOD 0-3 (LODs beyond 3 count as 3)
    int clones_by_lod[4]; // their clones
};

void rl_clone_mesh_take_census(const rl_clone_mesh_pool* pool, rl_clone_mesh_census& census);

// ---- inline members (no .cpp home): the undergrowth placement (undergrowth.cpp, in the GPU-free rfg library) packs
// its clones with them, so they stay in the header rather than in rl_clone_mesh_instance.cpp.

// 0x004BBA60 (.h:62)
inline rl_clone_mesh_element::rl_clone_mesh_element()
{
}

// 0x00922F10 (.h:88)
// An instance clone: the orientation's three rows and the colour's red, green and blue with 13 fraction bits, the
// position (in the instance's space) and the random value with 10. The alpha is not stored.
inline void rl_clone_mesh_element::set(const rl_clone_mesh_info& _info)
{
    _compress_fp_vector8<13, 13>(data0, vector4(_info.orient.rvec.x, _info.orient.rvec.y, _info.orient.rvec.z, _info.color.red),
                                 vector4(_info.orient.uvec.x, _info.orient.uvec.y, _info.orient.uvec.z, _info.color.green));
    _compress_fp_vector8<13, 10>(data1, vector4(_info.orient.fvec.x, _info.orient.fvec.y, _info.orient.fvec.z, _info.color.blue),
                                 vector4(_info.pos.x, _info.pos.y, _info.pos.z, _info.rand_value));
}

// 0x004E2AD0 (.h:118)
// Notes (decompiled behaviour, kept as is): the colour's alpha is left as it was.
inline void rl_clone_mesh_element::get(rl_clone_mesh_info& _info) const
{
    vector4 v;
    _decompress_fp_vector8_high<13>(v, data0);
    _info.orient.rvec.set(v.x, v.y, v.z);
    _info.color.red = v.w;
    _decompress_fp_vector8_low<13>(v, data0);
    _info.orient.uvec.set(v.x, v.y, v.z);
    _info.color.green = v.w;
    _decompress_fp_vector8_high<13>(v, data1);
    _info.orient.fvec.set(v.x, v.y, v.z);
    _info.color.blue = v.w;
    _decompress_fp_vector8_low<10>(v, data1);
    _info.pos.set(v.x, v.y, v.z);
    _info.rand_value = v.w;
}

// 0x0084BC20 (.h:151)
// A spline knot: its position and its parameter offset.
inline void rl_clone_mesh_element::set(const rl_clone_spline_info& _info)
{
    knot_pos.set(_info.knot_pos.x, _info.knot_pos.y, _info.knot_pos.z, 0.0f);
    knot_data.set(_info.knot_parameter_offset, 0.0f, 0.0f, 0.0f);
}

// 0x004BBA70 (.h:165)
inline void rl_clone_mesh_element::get(rl_clone_spline_info& _info)
{
    _info.knot_pos.set(knot_pos.x, knot_pos.y, knot_pos.z);
    _info.knot_parameter_offset = knot_data.x;
}

// 0x004BC560
// Uninitialized.
inline rl_clone_mesh_info::rl_clone_mesh_info()
{
}

// 0x0091BEB0
// Uninitialized.
inline rl_clone_mesh_wind_info::rl_clone_mesh_wind_info()
{
}

// 0x004BC7B0
// Uninitialized.
inline rl_clone_spline_info::rl_clone_spline_info()
{
}
