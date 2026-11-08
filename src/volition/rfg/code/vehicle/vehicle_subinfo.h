// Original: volition/rfg/code/vehicle/vehicle_subinfo.h
//
// The streamed part of a vehicle: the .ccar_pc / .gcar_pc pair (GSP_VEHICLE, load callback vehicle_subinfo_load in
// vehicle_loader.cpp). The CPU file is a vehicle_subinfo followed by its data (et_sized_native_pointer members are
// patched after loading): seat-to-door links, tag deformations, collision models and MOPPs, constraint templates, the
// render mesh and materials, morph targets, chassis tags and mass properties, lights, splines (treads) and
// m_num_components vehicle_component_data (variable length array at the end). Placement inferred.
#pragma once

#include "volition/rfg/code/vehicle/vehicle_component_data.h"

class hkpMoppCode;
struct constraint_template;
class rl_mesh;
class rl_material_map;
class rl_material;
class rl_morph_container;

// size 0x238
class vehicle_subinfo
{
public:
    // size 0x8
    struct seat_to_door_link_info
    {
        unsigned int m_num_seats; // 0x00
        unsigned int m_door_component_indices[1]; // 0x04
    };

    // size 0x1C
    struct mesh_tag_deformation_offset
    {
        quaternion m_rotation_offset; // 0x00
        vector m_position_offset; // 0x10
    };

    // size 0x20
    struct vehicle_spline_data
    {
        unsigned int num_control_points; // 0x00
        vector center; // 0x04
        et_sized_native_pointer<vector> control_points; // 0x10
        et_sized_native_pointer<int> attached_component_index; // 0x18
    };

    unsigned int m_version; // 0x00
    et_sized_native_pointer<seat_to_door_link_info> m_seat_to_door_links; // 0x08
    unsigned int m_num_tag_deformations; // 0x10
    et_sized_native_pointer<mesh_tag_deformation_offset> m_mesh_tag_deformation_offsets; // 0x18
    et_sized_native_pointer<unsigned char> m_collision_model_data; // 0x20
    unsigned int m_num_mopps; // 0x28
    et_sized_native_pointer<et_sized_native_pointer<hkpMoppCode const > > m_mopp_pointer_table; // 0x30
    unsigned int m_num_constraint_templates; // 0x38
    et_sized_native_pointer<constraint_template const > m_constraint_templates; // 0x40
    et_sized_native_pointer<rl_mesh> m_vehicle_mesh; // 0x48
    et_sized_native_pointer<rl_material_map> m_material_map; // 0x50
    et_sized_native_pointer<et_sized_native_pointer<rl_material> > m_materials; // 0x58
    unsigned int m_num_materials; // 0x60
    et_sized_native_pointer<char const > m_texture_names; // 0x68
    et_sized_native_pointer<rl_morph_container> m_morph_target; // 0x70
    unsigned int m_chassis_flags; // 0x78
    int m_chassis_submesh_id; // 0x7C
    et_sized_native_pointer<mesh_tags> m_chassis_tags_offset; // 0x80
    mesh_tags m_chassis_tags; // 0x88
    vehicle_cm_dynamics_info m_cm_dynamics_info; // 0x98
    et_sized_native_pointer<unsigned char> m_deformed_morph_target; // 0xE0
    unsigned int m_num_lights; // 0xE8
    et_sized_native_pointer<vehicle_mesh_light> m_lights; // 0xF0
    unsigned int m_num_splines; // 0xF8
    float m_tread_width; // 0xFC
    et_sized_native_pointer<vehicle_spline_data> m_spline_data; // 0x100
    unsigned int m_is_walker; // 0x108
    unsigned int m_num_components; // 0x10C
    vehicle_component_data m_components[1]; // 0x110
};
