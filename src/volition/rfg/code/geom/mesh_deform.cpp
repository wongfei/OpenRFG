// Original: volition/rfg/code/geom/mesh_deform.cpp
//
// Mesh deformation (vehicle dents): deform spheres saved per mesh from a shared free list, and their effect on vertex
// weights.

#include "volition/rfg/code/geom/mesh_deform.h"

#include "volition/vlib/code/math/floating.h"

#include <math.h>

static mesh_deform_sphere_link* Md_deform_spheres_free_list; // 0x025BCF18
static mesh_deform_sphere_link Md_deform_spheres[1000]; // 0x0260DED0

// 0x007F4130
mesh_deform_sphere_link::mesh_deform_sphere_link()
{
}

// 0x007F4140 (:95)
// Every sphere back on the free list, in array order (the list add inlined, unrolled by five).
void mesh_deform_level_init_post()
{
    Md_deform_spheres_free_list = nullptr;
    for (int i = 0; i < 1000; i++)
    {
        mesh_deform_sphere_link* link = &Md_deform_spheres[i];
        if (Md_deform_spheres_free_list == nullptr)
        {
            link->m_next = link;
            link->m_prev = link;
            Md_deform_spheres_free_list = link;
        }
        else
        {
            link->m_prev = Md_deform_spheres_free_list->m_prev;
            link->m_next = Md_deform_spheres_free_list;
            Md_deform_spheres_free_list->m_prev->m_next = link;
            Md_deform_spheres_free_list->m_prev = link;
        }
    }
}

static float mesh_deform_clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v < 1.0f ? v : 1.0f);
}

// 0x007F2650 (.h:88)
unsigned char mesh_deform_weight_convert(float weight)
{
    return (unsigned char)fl_uround(mesh_deform_clamp01(weight) * 255.0f);
}

// 0x007F26B0 (.h:96)
float mesh_deform_weight_convert(const unsigned char& weight)
{
    // Magic number: 1/255.
    return mesh_deform_clamp01((float)weight * 0.003921569f);
}

// 0x007F4230 (:113): every vertex in the sphere gains weight by (1 - distance / radius) * intensity, up to the sphere's maximum.
void mesh_deform_process(unsigned char* p_weights, const mesh_deform_sphere* deformation_sphere, const mesh_deform_vertex_info* vertex_info)
{
    const float radius = deformation_sphere->m_radius;
    const float max_weight = mesh_deform_weight_convert(deformation_sphere->m_max_deformation);
    const unsigned char* vertex = (const unsigned char*)vertex_info->m_vertices;
    for (unsigned int i = 0; i < vertex_info->m_num_vertices; i++, vertex += vertex_info->m_vertex_stride)
    {
        if (p_weights[i] >= deformation_sphere->m_max_deformation)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(deformation_sphere->m_position, *(const vector*)vertex);
        if (!(dist_sq < radius * radius))
        {
            continue;
        }
        const float t = mesh_deform_clamp01(sqrtf(dist_sq) * (1.0f / radius));
        float w = mesh_deform_weight_convert(p_weights[i]) + (1.0f - t) * deformation_sphere->m_intensity;
        if (w >= max_weight)
        {
            w = max_weight;
        }
        p_weights[i] = mesh_deform_weight_convert(w);
    }
}

static void mesh_deform_list_add(mesh_deform_sphere_link*& list, mesh_deform_sphere_link* link)
{
    if (list == nullptr)
    {
        link->m_next = link;
        link->m_prev = link;
        list = link;
    }
    else
    {
        link->m_prev = list->m_prev;
        link->m_next = list;
        list->m_prev->m_next = link;
        list->m_prev = link;
    }
}

static void mesh_deform_list_remove(mesh_deform_sphere_link*& list, mesh_deform_sphere_link* link)
{
    if (list == link && link->m_next == link)
    {
        list = nullptr;
    }
    else
    {
        if (list == link)
        {
            list = link->m_next;
        }
        link->m_prev->m_next = link->m_next;
        link->m_next->m_prev = link->m_prev;
    }
    link->m_next = nullptr;
    link->m_prev = nullptr;
}

// 0x007F4460 (:166)
bool mesh_deform_save_deform_sphere(mesh_deform_sphere_link** deform_sphere_list, const mesh_deform_sphere* deformation_sphere)
{
    mesh_deform_sphere_link* link = Md_deform_spheres_free_list;
    if (link == nullptr)
    {
        return false;
    }
    mesh_deform_list_remove(Md_deform_spheres_free_list, link);
    link->m_deform_sphere.m_position = deformation_sphere->m_position;
    link->m_deform_sphere.m_radius = deformation_sphere->m_radius;
    link->m_deform_sphere.m_intensity = deformation_sphere->m_intensity;
    link->m_deform_sphere.m_max_deformation = deformation_sphere->m_max_deformation;
    mesh_deform_list_add(*deform_sphere_list, link);
    return true;
}

// 0x007F4530 (:190)
void mesh_deform_free_deform_spheres(mesh_deform_sphere_link** deform_sphere_list)
{
    while (*deform_sphere_list != nullptr)
    {
        mesh_deform_sphere_link* link = *deform_sphere_list;
        mesh_deform_list_remove(*deform_sphere_list, link);
        mesh_deform_list_add(Md_deform_spheres_free_list, link);
    }
}

// 0x007F45D0 (:210)
bool mesh_deform_remove_smallest_deform_sphere(mesh_deform_sphere_link** deform_sphere_list)
{
    mesh_deform_sphere_link* first = *deform_sphere_list;
    if (first == nullptr)
    {
        return false;
    }
    mesh_deform_sphere_link* smallest = nullptr;
    float smallest_radius = -1.0f;
    mesh_deform_sphere_link* link = first;
    do
    {
        if (smallest_radius < 0.0f || link->m_deform_sphere.m_radius < smallest_radius)
        {
            smallest_radius = link->m_deform_sphere.m_radius;
            smallest = link;
        }
        link = link->m_next;
    } while (link != first && link != nullptr);
    mesh_deform_list_remove(*deform_sphere_list, smallest);
    mesh_deform_list_add(Md_deform_spheres_free_list, smallest);
    return true;
}
