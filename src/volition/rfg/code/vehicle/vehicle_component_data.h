// Original: volition/rfg/code/vehicle/vehicle_component_data.h
//
// A detachable vehicle component (door, wheel, panel, turret) as stored in .ccar_pc: type, durability, hierarchy,
// transform, bone, submesh, tags, lights, optional hinge and the deformation offsets. Placement inferred.
#pragma once

#include "volition/rfg/code/geom/mesh_tags.h"
#include "volition/rfg/code/vehicle/vehicle_cm_dynamics_info.h"
#include "volition/vlib/code/math/quaternion.h"

// size 0x70
struct vehicle_mesh_light
{
    unsigned int type; // 0x00
    matrix rotation; // 0x04
    vector position; // 0x28
    int off_mesh_id; // 0x34
    et_sized_native_pointer<mesh_tags> off_mesh_tags_offset; // 0x38
    mesh_tags off_mesh_tags; // 0x40
    int on_mesh_id; // 0x50
    et_sized_native_pointer<mesh_tags> on_mesh_tags_offset; // 0x58
    mesh_tags on_mesh_tags; // 0x60
};

// size 0x128
class vehicle_component_data
{
public:
    // size 0x1C
    struct hinge
    {
        vector m_axis; // 0x00
        vector m_origin; // 0x0C
        float m_max_angle; // 0x18
    };

    unsigned int m_type; // 0x00
    unsigned int m_durability; // 0x04
    unsigned int m_mass_type; // 0x08
    unsigned int m_flags; // 0x0C
    unsigned int m_parent_index; // 0x10
    unsigned int m_num_children; // 0x14
    et_sized_native_pointer<unsigned int> m_children_indices; // 0x18
    matrix m_rotation; // 0x20
    vector m_position; // 0x44
    char bone_name[40]; // 0x50
    int m_submesh_id; // 0x78
    et_sized_native_pointer<mesh_tags> m_tags_offset; // 0x80
    mesh_tags m_tags; // 0x88
    unsigned int m_num_lights; // 0x98
    et_sized_native_pointer<vehicle_mesh_light> m_lights; // 0xA0
    et_sized_native_pointer<hinge const > m_hinge; // 0xA8
    quaternion m_deformed_rotation_offset; // 0xB0
    vector m_deformed_position_offset; // 0xC0
    et_sized_native_pointer<unsigned char> m_deformed_morph_target; // 0xD0
    unsigned int m_parent_sub_piece_start_index; // 0xD8
    unsigned int m_parent_num_sub_pieces; // 0xDC
    vehicle_cm_dynamics_info m_cm_dynamics_info; // 0xE0
};

// vehicle_component_data::m_type holds the hash of a name (str_hash_string), so these are dynamically initialized.
extern unsigned int VCT_MISC; // 0x0381A64C "Misc."
extern unsigned int VCT_FRONT_WHEEL; // 0x037701BC "Front Wheel"
extern unsigned int VCT_REAR_WHEEL; // 0x037701D0 "Rear Wheel"
extern unsigned int VCT_TREAD_WHEEL_GROUND; // 0x03770BA0 "Tread Wheel Ground"
extern unsigned int VCT_TREAD_WHEEL_IDLER; // 0x0377CC38 "Tread Wheel Idler"
extern unsigned int VCT_DOOR; // 0x03770BA8 "Door"
extern unsigned int VCT_STEERING_WHEEL; // 0x03770158 "Steering Wheel"
extern unsigned int VCT_TRACK_SPLINE; // 0x0377CC3C "Track Spline"
extern unsigned int VCT_TREAD_SEGMENT; // 0x03770BAC "Tread Segment"
extern unsigned int VCT_MUFFLER; // 0x03770204 "Exhaust Pipe"
extern unsigned int VCT_SPOTLIGHT; // 0x03770214 "Spotlight"
extern unsigned int VCT_DOWN_THRUSTER; // 0x0384FC0C "Down Thruster"
extern unsigned int VCT_TURRET; // 0x037701E4 "Turret Component"
extern unsigned int VCT_AXLE_3_WHEEL; // 0x03770110 "Axel Wheel 3"
extern unsigned int VCT_AXLE_4_WHEEL; // 0x0377015C "Axel Wheel 4"
extern unsigned int VCT_SWING_ARM; // 0x0383D36C "Swing Arm"
extern unsigned int VCT_STEERING_JOINT; // 0x0386BFB4 "Steering Joint"
extern unsigned int VCT_WHEEL_HUB; // 0x03770138 "Wheel Hub"
extern unsigned int VCT_WHEEL_COLLISION; // 0x0386BF44 "Wheel Collision"
extern unsigned int VCT_WING; // 0x037701C0 "Wing"
extern unsigned int VCT_WHEEL_BUMPER; // 0x03770108 "Wheel_Bumpers"
extern unsigned int VCT_CHASSIS_COLLISION; // 0x0386BFF4 "Chassis_Collision"
extern unsigned int VCT_HEADLIGHT; // 0x03770BA4 "Headlight_Mesh"
extern unsigned int VCT_FOGLIGHT; // 0x037701E8 "Foglight_Mesh"
extern unsigned int VCT_TAILLIGHT; // 0x037700F4 "Taillight_Mesh"
extern unsigned int VCT_BRAKELIGHT; // 0x0386BF28 "Brakelight_Mesh"
extern unsigned int VCT_REVERSELIGHT; // 0x0381A5CC "Reverselight_Mesh"
extern unsigned int VCT_EMERGENCYLIGHT; // 0x0386BFF8 "Emergencylight_Mesh"
extern unsigned int VCT_STROBELIGHT; // 0x037701E0 "Strobelight_Mesh"
extern unsigned int VCT_TAILLIGHTG; // 0x0386BF48 "Taillight_Glare"
extern unsigned int VCT_BRAKELIGHTG; // 0x0377010C "Brakelight_Glare"
extern unsigned int VCT_EMERGENCYLIGHTG; // 0x037700F8 "Emergencylight_Glare"
extern unsigned int VCT_REVERSELIGHTG; // 0x0381A648 "Reverselight_Glare"
