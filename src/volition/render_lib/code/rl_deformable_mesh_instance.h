// Original: volition/render_lib/code/rl_deformable_mesh_instance.h
// Implementation: volition/render_lib/code/rl_deformable_mesh_instance.cpp
//
// rl_deformable_mesh_instance, a mesh instance deformed by spheres (dents in vehicles).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_camera;
class rl_class_factory_info;
class rl_decal;
struct rl_deformation_sphere;
class rl_morph_instance;
class rl_renderer;

// size 0x150
class rl_deformable_mesh_instance : public rl_compound_mesh_instance
{
protected:
    rl_deformable_mesh_instance(); // 0x00549860 (:230)
    virtual ~rl_deformable_mesh_instance(); // vtbl+0x00 0x005498D0 (:241)
private:
    static const pool_list<rl_deformable_mesh_instance*>& get_object_pool(); // 0x004BDBC0 (.h:47)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BDBD0 (.h:47)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050C7D0 (:226)
    static rl_deformable_mesh_instance* create_placement(rl_deformable_mesh_instance*);
    static rl_deformable_mesh_instance* create(); // 0x0054E6C0 (:226)
    static bool can_create(); // 0x004E4090 (:226)
    virtual void release(); // vtbl+0x04 0x00539730 (:226)
    virtual void render(rl_renderer& renderer, rl_camera& camera, unsigned int flags); // vtbl+0x10 0x005397B0 (:272)
    virtual void render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags); // vtbl+0x14 0x00539870 (:288)
    virtual void render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p); // vtbl+0x40 0x00539930 (:333)
    virtual void set_shader_types(rl_renderer& renderer); // vtbl+0x54 0x0050C8D0 (:348)
    virtual void set_morph_instance(rl_morph_instance* instance_p); // vtbl+0x60 0x004E40A0 (:362)
    virtual rl_morph_instance* get_morph_instance(); // vtbl+0x64 0x004BDC70 (:401)
    bool queue_deform_sphere(const vector& position, float radius, float intensity, unsigned char max_deformation, int subpiece); // 0x0050C920 (:436)
    void process_pending_spheres(); // 0x005399F0 (:408)
private:
    void process_deform_sphere_for_submesh(rl_deformation_sphere* sphere_p, int submesh_idx); // 0x00523800 (:76)
    void process_deform_sphere(rl_deformation_sphere* sphere_p); // 0x00539230 (:52)

public:
    static const unsigned int m___class_id; // 0x012BEDB8
    static rl_class_factory_info ms_class_factory_info; // 0x015149A8
protected:
    static pool_list<rl_deformable_mesh_instance*> ms_object_pool; // 0x015149D8
private:
    rl_morph_instance* m_morph_instance_p; // 0x140
    rl_deformation_sphere* m_pending_deformation_spheres; // 0x144
public:
    rl_deformable_mesh_instance* pending_prev; // 0x148
    rl_deformable_mesh_instance* pending_next; // 0x14C
};

// size 0x24
struct rl_deformation_sphere
{
    rl_deformation_sphere(); // 0x004BDBB0 (.h:25)

    vector m_position; // 0x00
    float m_radius; // 0x0C
    float m_intensity; // 0x10
    unsigned char m_max_deformation; // 0x14
    int m_subpiece; // 0x18
    rl_deformation_sphere* prev; // 0x1C
    rl_deformation_sphere* next; // 0x20
};

// 0x004BDBB0 (.h:25)
inline rl_deformation_sphere::rl_deformation_sphere()
    : m_max_deformation(0xFF)
{
}

void rl_deformation_process_pending_morphs(); // 0x00539A30 (:467)
