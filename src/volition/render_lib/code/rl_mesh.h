// Original: volition/render_lib/code/rl_mesh.h
// Implementation: volition/render_lib/code/rl_mesh.cpp
//
// rl_mesh, a renderable static mesh: mesh data, render blocks (one per material) and its keen RenderGeometry.
#pragma once

#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_renderable.h"

class mempool;
class rl_class_factory_info;
struct rl_render_block;
class rl_serial_block;
class vector;

// size 0x90
class rl_mesh : public rl_renderable
{
    // Access inferred from rl_renderer::render.
    friend class rl_renderer;
    // Access inferred (stub_impl.md S3): the deformation and the recompile cache read the mesh data.
    friend class rl_deformable_mesh_instance;
    friend class rl_destroyable_model_instance;
protected:
    rl_mesh(); // 0x004E9390 (:11)
    virtual ~rl_mesh(); // vtbl+0x00 0x004E94A0 (:17)
private:
    static const pool_list<rl_mesh*>& get_object_pool(); // 0x004BA480 (.h:30)
public:
    static void set_objects_max(int objects_max); // 0x007B9980 (.h:30)
    static int get_objects_max(); // 0x004BA490 (.h:30)
    static int get_objects_used();
    static int get_objects_free(); // 0x006E6C80 (.h:30)
    static void factory_init(mempool& factory_mempool); // 0x00511610 (:7)
    static rl_mesh* create_placement(rl_mesh* _data_p); // 0x00511710 (:7)
    static rl_mesh* create(); // 0x00527E20 (:7)
    static bool can_create(); // 0x004E9380 (:7)
    virtual void release(); // vtbl+0x04 0x00511830 (:7)
    void init(rl_mesh_data_runtime* new_data, const char* pDebugName, bool); // 0x00511A10 (:72)
    void init(rl_mesh_data* new_data, const LoadedMeshInfo& meshInfo, bool); // 0x00527E80 (:66)
    const rl_mesh_data_runtime* get_mesh_data() const; // 0x004BA4B0 (.h:38)
    rl_mesh_data_runtime* get_mesh_data(); // 0x004BA4A0 (.h:37)
    rl_render_block* get_base_render_block() const; // 0x004BA4E0 (.h:73)
    void get_subpiece_local_bb(int index, vector& bmin, vector& bmax) const; // 0x004C2DA0 (:55)
    unsigned int get_uid() const; // 0x004BA4C0 (.h:44)
    unsigned int get_max_material_index();
    virtual void serialize(rl_serial_block&, bool); // vtbl+0x08 0x004E9490 (.h:48)
    const keen::RenderGeometry& getRenderGeometry() const; // 0x004BA4D0 (.h:50)
protected:
    void set_mesh_data(rl_mesh_data_runtime* new_data); // 0x005118F0 (:33)

public:
    static const unsigned int m___class_id; // 0x012B80D8
    static rl_class_factory_info ms_class_factory_info; // 0x01514DB4
protected:
    static pool_list<rl_mesh*> ms_object_pool; // 0x01514DE4
private:
    bool m_data_set; // 0x30
    rl_mesh_data_runtime m_data; // 0x34
    rl_render_block* m_base_render_block; // 0x54
    unsigned int m_max_material_index; // 0x58
    unsigned int m_uid; // 0x5C
    keen::RenderGeometry m_geometry; // 0x60
};
