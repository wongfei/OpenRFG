// Original: volition/rfg/code/vehicle/vehicle_deformation.cpp
//
// Dents: a hit becomes a deform sphere, kept in chassis space on the vehicle (and replicated in multiplayer), queued for
// the chassis and component meshes, and weighted onto the tags and part positions. Compiled in the vehicle unity TU.

#include "volition/rfg/code/vehicle/vehicle_deformation.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/suspension/hkpvehiclesuspension.h"
#include "volition/rfg/code/geom/mesh_deform.h"
#include "volition/rfg/code/vehicle/automobile.h"

bool Vehicle_deformation_enabled = true; // 0x016BA489

// 0x00B7C2A0 (:108) (LTCG: vp in ECX, p_deformation_sphere in EAX)
static void vehicle_deform_chassis(vehicle* vp, const mesh_deform_sphere* p_deformation_sphere)
{
    mesh_deform_sphere local_sphere;
    local_sphere.m_radius = p_deformation_sphere->m_radius;
    local_sphere.m_intensity = p_deformation_sphere->m_intensity;
    local_sphere.m_max_deformation = p_deformation_sphere->m_max_deformation;
    local_sphere.m_position.set(p_deformation_sphere->m_position.x - vp->pos.x, p_deformation_sphere->m_position.y - vp->pos.y, p_deformation_sphere->m_position.z - vp->pos.z);
    local_sphere.m_position.rotate(vp->orient);
    mesh_deform_save_deform_sphere(&vp->m_mesh_deform_pending_list, &local_sphere);
    const vehicle_subinfo* sub_info = vp->rdd.get_data()->sub_info;
    if (sub_info->m_num_tag_deformations != 0) {
        // the chassis tags move with the dent
        int num_tags;
        const mesh_tag* tags = sub_info->m_chassis_tags.get_tags(num_tags);
        mesh_deform_vertex_info vertices;
        vertices.m_num_vertices = sub_info->m_chassis_tags.get_num_tags();
        vertices.m_vertices = &tags->m_transform.m_translation.x;
        vertices.m_vertex_stride = sizeof(mesh_tag);
        mesh_deform_process(vp->m_tag_deform_weights, &local_sphere, &vertices);
    }
}

// 0x00B8EE60 (:148) (LTCG: ap in EDI, p_component in EDX)
static void vehicle_deform_wheel(automobile* ap, const vehicle_component* p_component)
{
    const unsigned int wheel = vehicle_component_get_wheel_index(ap, p_component); // inlined in rfg.exe
    vector hardpoint;
    automobile_calculate_wheel_hardpoint(&hardpoint, p_component, ap);
    hkVector4f hk_hardpoint;
    game_to_havok_vector(hk_hardpoint, hardpoint);
    ap->a_rdd.get_data()->m_hk_vehicle_instance->m_suspension->m_wheelParams[wheel].m_hardpointChassisSpace = hk_hardpoint;
}

// 0x00BA0DF0 (:175) (LTCG: vp in ECX, p_deformation_sphere in EDX)
static void vehicle_deform_components(vehicle_component* const* damaged_components, unsigned int num_damaged_components, vehicle* vp, const mesh_deform_sphere* p_deformation_sphere)
{
    for (unsigned int i = 0; i < num_damaged_components; i++) {
        vehicle_component* component = damaged_components[i];
        if ((component->get_flags() & VCF_DETACHED) != 0) {
            continue;
        }
        if ((component->get_flags() & VCF_GLASS) != 0) {
            vehicle_component_shatter_glass(component, vp, NULL, NULL, NULL);
            continue;
        }
        component->set_flags(VCF_DEFORMED_HINGE);
        mesh_deform_sphere local_sphere;
        local_sphere.m_radius = p_deformation_sphere->m_radius;
        local_sphere.m_max_deformation = p_deformation_sphere->m_max_deformation;
        local_sphere.m_intensity = p_deformation_sphere->m_intensity;
        // in its own space if it has its own mesh instance, else the chassis's
        const vector& origin = component->m_renderable_instance_p != NULL ? component->get_world_space_position() : vp->pos;
        const matrix& rotation = component->m_renderable_instance_p != NULL ? component->get_world_space_rotation() : vp->orient;
        local_sphere.m_position.set(p_deformation_sphere->m_position.x - origin.x, p_deformation_sphere->m_position.y - origin.y, p_deformation_sphere->m_position.z - origin.z);
        local_sphere.m_position.rotate(rotation);
        mesh_deform_save_deform_sphere(&component->m_mesh_deform_pending_list, &local_sphere);
    }
    // the weight of the parts' own transforms, at most one each
    vector positions[66];
    vehicle_component* moved[66];
    unsigned char weights[66];
    unsigned int num_moved = 0;
    for (unsigned int i = 0; i < num_damaged_components; i++) {
        vehicle_component* component = damaged_components[i];
        if ((component->get_flags() & VCF_DETACHED) == 0 && (component->get_flags() & (VCF_DEFORM_POSITION | VCF_DEFORM_ROTATION)) != 0) {
            positions[num_moved] = component->get_world_space_position();
            moved[num_moved] = component;
            weights[num_moved] = component->m_pos_orient_deform_weight;
            num_moved++;
        }
    }
    mesh_deform_vertex_info vertices;
    vertices.m_vertices = &positions[0].x;
    vertices.m_vertex_stride = sizeof(vector);
    vertices.m_num_vertices = num_moved;
    mesh_deform_process(weights, p_deformation_sphere, &vertices);
    automobile* ap = (automobile*)vp;
    for (unsigned int i = 0; i < num_moved; i++) {
        vehicle_component* component = moved[i];
        if (weights[i] != 0 && component->m_pos_orient_deform_weight == 0) {
            // no longer the chassis mesh's: it now moves on its own
            component->create_renderable_instance_subtree(vp);
        }
        component->m_pos_orient_deform_weight = weights[i];
        if ((component->get_flags() & VCF_UNLATCHED) != 0) {
            vehicle_deform_unlatched_hinge(component, vp);
        }
        if (vehicle_component_is_wheel(component->get_data())) {
            vehicle_deform_wheel(ap, component); // inlined in rfg.exe
        }
    }
}

// 0x00BA1120 (:311)
void vehicle_deform(vehicle* vp, vehicle_component* const* damaged_components, unsigned int num_damaged_components, const vector* position, float radius, int hit_points, damaged_by_types damage_from)
{
    if (damage_from == DBT_NONE || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        return;
    }
    mesh_deform_sphere sphere;
    // 85 at the vehicle's whole hit points
    sphere.m_intensity = ((float)hit_points * 85.0f) / (float)(unsigned int)vp->info->m_max_hitpoints;
    sphere.m_position = *position;
    sphere.m_max_deformation = 0xFF;
    sphere.m_radius = radius;
    if (damage_from == DBT_BULLET || damage_from == DBT_MELEE) {
        sphere.m_max_deformation = (unsigned char)fl_uround(127.5f); // half
    }
    sphere.m_position.x -= vp->pos.x;
    sphere.m_position.y -= vp->pos.y;
    sphere.m_position.z -= vp->pos.z;
    sphere.m_position.rotate(vp->orient);
    // the vehicle keeps its 20 largest
    if (19 < vp->m_num_mesh_deform_spheres) {
        mesh_deform_remove_smallest_deform_sphere(&vp->m_mesh_deform_sphere_list);
        vp->m_num_mesh_deform_spheres--;
    }
    if (mesh_deform_save_deform_sphere(&vp->m_mesh_deform_sphere_list, &sphere)) {
        vp->m_num_mesh_deform_spheres++;
        sphere.m_position = *position;
        vehicle_deform_chassis(vp, &sphere);
        vehicle_deform_components(damaged_components, num_damaged_components, vp, &sphere);
    }
}

// 0x00B7C3B0 (:381)
void vehicle_deform_unlatched_hinge(vehicle_component* component, const vehicle* vp)
{
    if ((component->get_flags() & VCF_DEFORM_POSITION) == 0) {
        return;
    }
    hkpLimitedHingeConstraintData* hinge = (hkpLimitedHingeConstraintData*)component->m_constraint->m_data;
    float weight = (float)component->m_pos_orient_deform_weight * 0.003921569f; // 1 / 255
    if (weight - 0.0f < 0.0f) {
        weight = 0.0f;
    }
    if (0.0f <= weight - 1.0f) {
        weight = 1.0f;
    }
    const vehicle_component_data* comp_data = component->get_data();
    const vehicle_component_data::hinge* hinge_data = (const vehicle_component_data::hinge*)comp_data->m_hinge;
    vector pivot(comp_data->m_deformed_position_offset.x * weight + hinge_data->m_origin.x, comp_data->m_deformed_position_offset.y * weight + hinge_data->m_origin.y, comp_data->m_deformed_position_offset.z * weight + hinge_data->m_origin.z);
    if (comp_data->m_parent_index != 0xFFFFFFFF) {
        const vehicle_component& parent = vp->rdd.get_data()->m_components[comp_data->m_parent_index];
        pivot.unrotate(parent.get_chassis_space_rotation());
        pivot.x += parent.get_chassis_space_position().x;
        pivot.y += parent.get_chassis_space_position().y;
        pivot.z += parent.get_chassis_space_position().z;
    }
    game_to_havok_vector(hinge->m_atoms.m_transforms.m_transformA.getTranslation(), pivot);
}

// 0x00B7C5D0 (:436)
void vehicle_deform_update_render(vehicle* vp)
{
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    mesh_deform_sphere_link* link = vp->m_mesh_deform_pending_list;
    if (link != NULL) {
        do {
            data->m_renderable_instance_p->queue_deform_sphere(link->m_deform_sphere.m_position, link->m_deform_sphere.m_radius, link->m_deform_sphere.m_intensity, link->m_deform_sphere.m_max_deformation, 0);
            link = link->m_next;
        } while (link != vp->m_mesh_deform_pending_list && link != NULL);
        mesh_deform_free_deform_spheres(&vp->m_mesh_deform_pending_list);
    }
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        link = component.m_mesh_deform_pending_list;
        if (link == NULL) {
            continue;
        }
        do {
            // a part without its own instance dents its sub piece of the chassis's
            if (component.m_renderable_instance_p != NULL) {
                component.m_renderable_instance_p->queue_deform_sphere(link->m_deform_sphere.m_position, link->m_deform_sphere.m_radius, link->m_deform_sphere.m_intensity, link->m_deform_sphere.m_max_deformation, -1);
            }
            else {
                data->m_renderable_instance_p->queue_deform_sphere(link->m_deform_sphere.m_position, link->m_deform_sphere.m_radius, link->m_deform_sphere.m_intensity, link->m_deform_sphere.m_max_deformation, component.m_sub_instance_idx);
            }
            link = link->m_next;
        } while (link != component.m_mesh_deform_pending_list && link != NULL);
        mesh_deform_free_deform_spheres(&component.m_mesh_deform_pending_list);
        component.m_mesh_deform_pending_list = NULL;
    }
}

// 0x00B663A0 (:497)
void vehicle_deform_free_data(vehicle* vp)
{
    if (vp->m_mesh_deform_sphere_list != NULL) {
        mesh_deform_free_deform_spheres(&vp->m_mesh_deform_sphere_list);
        vp->m_num_mesh_deform_spheres = 0;
    }
    if (vp->m_mesh_deform_pending_list != NULL) {
        mesh_deform_free_deform_spheres(&vp->m_mesh_deform_pending_list);
    }
}

// 0x00BA12F0 (:518)
void vehicle_deform_add_sphere_mpsync(unsigned int v_handle, const mesh_deform_sphere* deform_sphere)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    if (vp == NULL) {
        return;
    }
    if (19 < vp->m_num_mesh_deform_spheres) {
        if (!mesh_deform_remove_smallest_deform_sphere(&vp->m_mesh_deform_sphere_list)) {
            return;
        }
        vp->m_num_mesh_deform_spheres--;
    }
    mesh_deform_save_deform_sphere(&vp->m_mesh_deform_sphere_list, deform_sphere);
    vp->m_num_mesh_deform_spheres++;
    // the replicated sphere is in chassis space: back to the world for the parts it reaches
    mesh_deform_sphere world_sphere;
    world_sphere.m_position = deform_sphere->m_position;
    world_sphere.m_max_deformation = deform_sphere->m_max_deformation;
    world_sphere.m_radius = deform_sphere->m_radius;
    world_sphere.m_intensity = deform_sphere->m_intensity;
    world_sphere.m_position.unrotate(vp->orient);
    world_sphere.m_position.x += vp->pos.x;
    world_sphere.m_position.y += vp->pos.y;
    world_sphere.m_position.z += vp->pos.z;
    const float radius_sqr = world_sphere.m_radius * world_sphere.m_radius;
    vehicle::resource_dependent_data* data = vp->rdd.get_data();
    vehicle_component* hit_components[66];
    unsigned int num_hit = 0;
    for (unsigned int i = 0; i < data->m_num_components; i++) {
        vehicle_component& component = data->m_components[i];
        if ((component.get_flags() & (VCF_DETACHED | VCF_GLASS)) == 0 && component.get_squared_distance(vp, &world_sphere.m_position) < radius_sqr) {
            if (num_hit == 66) {
                break;
            }
            hit_components[num_hit++] = &component;
        }
    }
    vehicle_deform_chassis(vp, &world_sphere);
    vehicle_deform_components(hit_components, num_hit, vp, &world_sphere);
}

// 0x00B7C6C0 (:598)
unsigned int vehicle_get_deform_sphere_count(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? vp->m_num_mesh_deform_spheres : 0;
}

// 0x00B7C6F0 (:615)
mesh_deform_sphere_link** vehicle_get_mesh_deform_sphere_link(unsigned int v_handle)
{
    vehicle* vp = vehicle_from_handle(v_handle);
    return vp != NULL ? &vp->m_mesh_deform_sphere_list : NULL;
}
