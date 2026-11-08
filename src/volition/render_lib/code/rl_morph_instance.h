// Original: volition/render_lib/code/rl_morph_instance.h
// Implementation: volition/render_lib/code/rl_morph_instance.cpp
//
// rl_morph_instance and rl_dynamic_morph_instance, morph target weights applied to a mesh instance (rl_active_morph
// per target). Placement of the morph enums inferred.
#pragma once

#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/memory/object_pool.h"

struct cpu_job;
union cpu_job_arg;
class mempool;
class rl_class_factory_info;
class rl_morph_container;
struct rl_morph_target_compressed;
struct rl_morph_vert_uncompressed;
class rl_renderer;

enum rl_morph_mode
{
    invalid_morph_mode = 0,
    per_vertex_weight = 1,
    per_object_weight = 2,
};

// size 0x8
struct rl_active_morph
{
    unsigned char* m_morph_target_data_p; // 0x00
    float blend_percent; // 0x04
};

// size 0x40
class rl_dynamic_morph_instance : public rl_base_object
{
    // Access inferred: rl_skin_instance::activate_morph sets the vertex counts and its draws inline morph_valid
    // (friends are not recorded in rfg.pdb).
    friend class rl_skin_instance;
protected:
    rl_dynamic_morph_instance(); // 0x0054BE20 (:387)
    virtual ~rl_dynamic_morph_instance(); // vtbl+0x00 0x0054BE80 (:396)
private:
    static const pool_list<rl_dynamic_morph_instance*>& get_object_pool(); // 0x004BDC00 (.h:57)
public:
    static void set_objects_max(int objects_max); // 0x007C1F30 (.h:57)
    static int get_objects_max(); // 0x004BDC10 (.h:57)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x005131A0 (:384)
    static rl_dynamic_morph_instance* create_placement(rl_dynamic_morph_instance* _data_p); // 0x0054F570 (:384)
    static rl_dynamic_morph_instance* create(); // 0x0054F510 (:384)
    static bool can_create(); // 0x004E9CA0 (:384)
    virtual void release(); // vtbl+0x04 0x00513290 (:384)
protected:
    bool add_morph_contribution(rl_morph_vert_uncompressed* morph_dest_p, rl_morph_target_compressed* morph_target_p, float blend_wt); // 0x00512CF0 (:57)
    void process_morphs_internal(rl_morph_vert_uncompressed* destination_buffer_p); // 0x0053FD90 (:437)
public:
    void process_active_morphs(rl_renderer& renderer); // 0x0053FF20 (:493)
    static void process_morph_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x0054BED0 (:427)
    void clear_active_morphs(); // 0x0053FD50 (:407)
    bool add_active_morph(rl_morph_container* container_p, int target, float weight); // 0x00529410 (:412)
    bool render_setup(rl_renderer& renderer); // 0x00513310 (:575)
    bool render_teardown(rl_renderer& renderer); // 0x00513360 (:607)
    bool morph_valid(); // 0x004C3820 (:618)
    void init(int num_verts); // 0x004C3800 (:401)

    static const unsigned int m___class_id; // 0x012BEDC0
    static rl_class_factory_info ms_class_factory_info; // 0x01514ED4
protected:
    static pool_list<rl_dynamic_morph_instance*> ms_object_pool; // 0x01514F04
    object_pool_client<rl_active_morph,1> m_dynamic_morph_bucket; // 0x08
    unsigned int m_morph_data_size; // 0x28
    unsigned int m_num_morph_verts; // 0x2C
    bool m_dynamicVertexDataValid; // 0x30
    keen::VertexBufferData m_dynamicVertexData; // 0x34
};

// size 0x30
class rl_morph_instance : public rl_base_object
{
    // Access inferred (stub_impl.md S3): rl_deformable_mesh_instance writes the dent weights.
    friend class rl_deformable_mesh_instance;
protected:
    rl_morph_instance(); // 0x004C3750 (:185)
    virtual ~rl_morph_instance(); // vtbl+0x00 0x004E9C00 (:196)
private:
    static const pool_list<rl_morph_instance*>& get_object_pool(); // 0x004BDC20 (.h:91)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BDC30 (.h:91)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00512F60 (:182)
    static rl_morph_instance* create_placement(rl_morph_instance* _data_p); // 0x004E9BB0 (:182)
    static rl_morph_instance* create(); // 0x005292D0 (:182)
    static bool can_create(); // 0x004E9BF0 (:182)
    virtual void release(); // vtbl+0x04 0x00513060 (:182)
    void init(rl_morph_mode mode, rl_morph_container* container); // 0x004C3780 (:211)
    rl_morph_container* get_morph_container(); // 0x004C37D0 (:265)
    unsigned char* get_morph_weights(); // 0x004C37C0 (:256)
    bool alloc_morph_weights(); // 0x00513130 (:228)
    int increment_ref_count(); // 0x004BDC40 (.h:101)
    int decrement_ref_count(); // 0x004BDC50 (.h:106)
    void build_frame_weights(rl_renderer& renderer); // 0x004E9C40 (:270)
    bool render_setup(rl_renderer& renderer); // 0x00529350 (:314)
    bool render_teardown(rl_renderer& renderer); // 0x005293D0 (:353)
    bool is_valid_for_rendering(); // 0x004C37E0 (:379)

    static const unsigned int m___class_id; // 0x012BEDC4
    static rl_class_factory_info ms_class_factory_info; // 0x01514E8C
protected:
    static pool_list<rl_morph_instance*> ms_object_pool; // 0x01514EBC
    int m_ref_count; // 0x08
    rl_morph_mode m_mode; // 0x0C
    rl_morph_container* m_morph_container_p; // 0x10
    unsigned char* m_morph_weights_p; // 0x14
    int m_num_weights; // 0x18
    bool m_dynamicVertexDataValid; // 0x1C
    keen::VertexBufferData m_dynamicVertexData; // 0x20
    unsigned int m_last_build_frame; // 0x2C
};

void rl_d3d_morph_run_jobs(); // 0x004C3840 (:623)
void rl_d3d_morph_wait_for_complete(); // 0x004C3850 (:641)
