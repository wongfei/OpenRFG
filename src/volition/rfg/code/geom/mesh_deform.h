// Original: volition/rfg/code/geom/mesh_deform.h
// Implementation: volition/rfg/code/geom/mesh_deform.cpp
//
// Mesh deformation (vehicle dents): deform spheres saved per mesh from a shared free list (Md_deform_spheres, 1000), the
// vertex weights packed into bytes, and the per-frame processing.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0x18
struct mesh_deform_sphere
{
    mesh_deform_sphere(const mesh_deform_sphere& __that); // 0x00B66370
    mesh_deform_sphere(); // 0x007F2640 (.h:22)

    vector m_position; // 0x00
    float m_radius; // 0x0C
    float m_intensity; // 0x10
    unsigned char m_max_deformation; // 0x14
};

// size 0x20
struct mesh_deform_sphere_link
{
    mesh_deform_sphere_link(); // 0x007F4130

    mesh_deform_sphere m_deform_sphere; // 0x00
    mesh_deform_sphere_link* m_next; // 0x18
    mesh_deform_sphere_link* m_prev; // 0x1C
};

// size 0xC
struct mesh_deform_vertex_info
{
    const float* m_vertices; // 0x00
    unsigned int m_num_vertices; // 0x04
    unsigned int m_vertex_stride; // 0x08
};

// 0x007F2640 (.h:22)
inline mesh_deform_sphere::mesh_deform_sphere()
    : m_max_deformation(0xFF)
{
}

unsigned char mesh_deform_weight_convert(float weight); // 0x007F2650 (.h:88)
float mesh_deform_weight_convert(const unsigned char& weight); // 0x007F26B0 (.h:96)
void mesh_deform_level_init_post(); // 0x007F4140 (:95)
void mesh_deform_process(unsigned char* p_weights, const mesh_deform_sphere* deformation_sphere, const mesh_deform_vertex_info* vertex_info); // 0x007F4230 (:113)
bool mesh_deform_save_deform_sphere(mesh_deform_sphere_link** deform_sphere_list, const mesh_deform_sphere* deformation_sphere); // 0x007F4460 (:166)
void mesh_deform_free_deform_spheres(mesh_deform_sphere_link** deform_sphere_list); // 0x007F4530 (:190)
bool mesh_deform_remove_smallest_deform_sphere(mesh_deform_sphere_link** deform_sphere_list); // 0x007F45D0 (:210)
