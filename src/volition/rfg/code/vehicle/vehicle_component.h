// Original: volition/rfg/code/vehicle/vehicle_component.h
// Implementation: volition/rfg/code/vehicle/vehicle_component.cpp
//
// A breakable part of a vehicle (door, wheel, turret mount, panel...) described by its vehicle_component_data: its own
// Havok body or shapes in the chassis (havok_handle, constraint), hit points, deformable renderable instance, and its
// transform in chassis and world space. Doors, tread wheels and suspensions keep extra state in m_type_attribute.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/vehicle/vehicle_component_data.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

struct hkContainerHeapAllocator;
class hkpConstraintInstance;
class hkpMoppCode;
struct mesh_deform_sphere_link;
class object;
class rl_deformable_mesh_instance;
class vehicle;
// size 0xC
struct vehicle_component_accident_info
{
    vehicle* m_vehicle_a; // 0x00
    vehicle* m_vehicle_b; // 0x04
    bool m_visible_damage; // 0x08
};
class vehicle_component_data;
enum vehicle_component_durability_types;
class vehicle_subinfo;
struct weapon_info;

// size 0xB0
class vehicle_component
{
public:
    enum adi_states
    {
        ADI_NOT_ANIMATING = 0,
        ADI_BEGIN_OPENING = 1,
        ADI_OPENING = 2,
        ADI_OPENED = 3,
        ADI_BEGIN_CLOSING = 4,
        ADI_CLOSING = 5,
        ADI_CLOSED = 6,
    };

    // size 0x10
    struct door_attr
    {
        float m_t_actual; // 0x00
        float m_t; // 0x04
        float m_t_velocity; // 0x08
        adi_states m_state; // 0x0C
    };

    // size 0xC
    struct tread_wheel_attr
    {
        int spline_index; // 0x00
        float wheel_radius; // 0x04
        float wheel_rotation; // 0x08
    };

    // size 0x20
    struct suspension_attr
    {
        int wheel_index; // 0x00
        float pivot_pos_mag; // 0x04
        float pivot_pos[3]; // 0x08
        float wheel_bias[3]; // 0x14
    };

    // size 0x20
    union m_type_attribute_union
    {
        door_attr door; // 0x00
        tread_wheel_attr tread_wheel; // 0x00
        suspension_attr suspension; // 0x00
    };

    void set_orient_pos_in_world_space(vehicle* vp, const matrix& r, const vector& p); // 0x00B65FB0 (:1782)
    void set_orient_pos_in_chassis_space(const matrix& r, const vector& p); // 0x00B660C0 (:1803)
    const matrix& get_rotation() const;
    const vector& get_position() const; // 0x00B62E00 (.h:112)
    const vector& get_chassis_space_position() const; // 0x00A05A80 (.h:118)
    const matrix& get_chassis_space_rotation() const; // 0x00A05A90 (.h:124)
    const vector& get_world_space_position() const; // 0x00A05AA0 (.h:130)
    const matrix& get_world_space_rotation() const; // 0x00AEA000 (.h:136)
    unsigned int get_type() const; // 0x00AEA010 (.h:142)
    vehicle_component_durability_types get_durability() const; // 0x00B62E10 (.h:148)
    unsigned int get_flags() const; // 0x00943030 (.h:154)
    void set_flags(unsigned int flags); // 0x00B62E20 (.h:160)
    void clear_flags(unsigned int flags); // 0x00B62E30 (.h:168)
    const char* get_bone_name() const; // 0x00B62E40 (.h:175)
    const vehicle_component_data* get_data() const; // 0x00A05AB0 (.h:181)
    void init(const vehicle_component_data& data, const vehicle_subinfo& subinfo, const vehicle* vp, bool reinitialization); // 0x00B8D840 (:379)
    void setup(vehicle* vp); // 0x00B8D960 (:422)
    void set_hitpoints(const vehicle* vp, bool reset_to_max); // 0x00B7A720 (:561)
    void set_hitpoints_to_durability(const vehicle* vp, int durability); // 0x00B7A7E0 (:593)
    void deinit(); // 0x00B65E10 (:617)
    void update_chassis_space(const vehicle* vp); // 0x00B7A850 (:627)
    void free_rigid_body_and_constraints(vehicle* vp); // 0x00B8DDE0 (:655)
    void free_constraint(); // 0x00B65E30 (:735)
    float get_squared_distance(const vehicle* vp, const vector* point) const; // 0x00B7A960 (:753)
    void get_local_bbox(const vehicle* const vp, vector& local_bmin, vector& local_bmax) const; // 0x00B7AA40 (:774)
    bool apply_damage(vehicle* vp, object* attacker, int hit_points, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info); // 0x00BBFA90 (:795)
    bool process_damage(vehicle* vp, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info); // 0x00BBBBE0 (:928)
    bool process_damage_zero_hit_points(vehicle* vp, damaged_by_types from_type, const vector* damage_velocity, const vector* damage_hit_position, const vector* damage_hit_normal, const weapon_info* w_info); // 0x00BB7610 (:968)
    void update_animatable_door(float time_step, vehicle* vp); // 0x00BAA6E0 (:1133)
    void process_loose_door(float time_step, vehicle* vp); // 0x00B8E090 (:1311)
    float get_unlatched_door_t_value(const vehicle* vp) const; // 0x00B7AA70 (:1332)
    unsigned int get_cm_index() const; // 0x00B65E50 (:1379)
    const hkpMoppCode* get_mopp_code() const; // 0x00B7ACB0 (:1388)
    const vector& get_center_of_mass() const; // 0x00B65E60 (:1397)
    const matrix& get_inertia_tensor() const; // 0x00B65E70 (:1406)
    float calc_material_mass(vehicle* vp) const; // 0x00B65E80 (:1417)
    unsigned int get_parent_sub_piece_start_index() const; // 0x00B65F90 (:1462)
    unsigned char get_physical_material() const; // 0x00B65F60 (:1445)
    unsigned char get_effect_material() const; // 0x00B65F70 (:1451)
    unsigned int get_shape_key(const vehicle* vp) const; // 0x00B8E100 (:1467)
    unsigned int get_first_shape_key(const vehicle* vp) const; // 0x00B7ACC0 (:1472)
    unsigned int get_last_shape_key(const vehicle* vp) const; // 0x00B7AD10 (:1490)
    unsigned int get_node_num_shapes(const vehicle* vp) const; // 0x00B7AD70 (:1509)
    unsigned int get_comp_num_shapes(const vehicle* vp) const; // 0x00B7ADC0 (:1534)
    unsigned int get_parent_num_sub_pieces() const; // 0x00B65FA0 (:1568)
    bool create_renderable_instance(vehicle* vp); // 0x00B7AE80 (:1575)
    void destroy_renderable_instance(vehicle* vp); // 0x00B8E110 (:1689)
    bool create_renderable_instance_subtree(vehicle* vp); // 0x00B7B4A0 (:1704)
    void destroy_renderable_instance_subtree(vehicle* vp); // 0x00B8E150 (:1729)
    void destroy_renderable_instance_absolute(vehicle* vp); // 0x00B7B520 (:1749)
    vehicle_component(); // 0x00B64610

    unsigned int havok_handle; // 0x00
    hkpConstraintInstance* m_constraint; // 0x04
    unsigned short m_max_hit_points; // 0x08
    unsigned short m_hit_points; // 0x0A
    unsigned int m_last_hit_by_handle; // 0x0C
    unsigned int m_debris_object; // 0x10
    unsigned int m_original_collision_filter; // 0x14
    rl_deformable_mesh_instance* m_renderable_instance_p; // 0x18
    short m_renderable_instance_refs; // 0x1C
    short m_sub_instance_idx; // 0x1E
    mesh_deform_sphere_link* m_mesh_deform_pending_list; // 0x20
    unsigned char m_pos_orient_deform_weight; // 0x24
    unsigned char m_numb_shapes; // 0x25
    bool m_type_attribute_valid; // 0x26
    m_type_attribute_union m_type_attribute; // 0x28
private:
    vehicle_component_data* m_data; // 0x48
    unsigned int m_flags; // 0x4C
    matrix m_ws_rotation; // 0x50
    vector m_ws_position; // 0x74
    matrix m_cs_rotation; // 0x80
    vector m_cs_position; // 0xA4
};

void vehicle_components_system_init(); // 0x00B66120 (:1815)
void vehicle_components_system_shutdown(); // 0x00B66140 (:1820)
void vehicle_components_level_init_pre(); // 0x00B66150 (:1826)
void vehicle_components_level_shutdown(); // 0x00B66160 (:1835)
bool vehicle_component_is_wheel(const vehicle_component_data* component); // 0x00B66170 (:1845)
unsigned int vehicle_component_get_wheel_index(const vehicle* vp, const vehicle_component* p_component); // 0x00B7B5D0 (:1865)
void vehicle_component_add_user_properties(vehicle_component* component, const vehicle* vp); // 0x00B7B610 (:1890)
void vehicle_component_get_hierarchy_collision(hkArray<unsigned int,hkContainerHeapAllocator>& out_collision_filters, const vehicle_component* p_component, const vehicle* vp); // 0x00B97C90 (:1919)
void vehicle_component_set_hierarchy_collision(vehicle_component* component, vehicle* vp, unsigned int filter_info); // 0x00B9E410 (:2046)
void vehicle_component_create_rigid_body(vehicle_component* component, vehicle* vp, bool collide_with_vehicle); // 0x00B9E680 (:2166)
void vehicle_component_create_hinge(vehicle_component* component, const vehicle* vp); // 0x00B8E1D0 (:2354)
void vehicle_component_unlatch_hinged(vehicle_component* component, vehicle* vp); // 0x00B9EBB0 (:2506)
void vehicle_component_latch_hinged(vehicle_component* component, vehicle* vp); // 0x00B9EC90 (:2600)
vehicle_component* vehicle_component_find_child_door(const vehicle_component* component, const vehicle* vp, int idx); // 0x00B7B7F0 (:2699)
void vehicle_component_open_door(vehicle_component* component, vehicle* vp, float open_duration); // 0x00B9ED00 (:2711)
void vehicle_component_close_door(vehicle_component* component, vehicle* vp, float close_duration); // 0x00B9ED60 (:2747)
bool vehicle_component_door_is_fully_open(const vehicle_component* component); // 0x00B661A0 (:2783)
bool vehicle_component_door_is_fully_closed(const vehicle_component* component); // 0x00B661C0 (:2797)
float vehicle_component_get_door_open_pct(const vehicle_component* component, const vehicle* vp); // 0x00B661F0 (:2814)
void vehicle_component_shatter_glass(vehicle_component* p_component, vehicle* vp, const vector* damage_hit_position, const vector* damage_hit_normal, const vector* damage_velocity); // 0x00BA0C10 (:3468)
void vehicle_calc_ws_pos_orient_for_components(vehicle* vp, bool force_update); // 0x00B7BD10 (:3704)
void vehicle_calc_cs_pos_orient_for_components(vehicle* vp); // 0x00B7BDA0 (:3741)
bool vehicle_component_is_attached(const vehicle* vp, unsigned int component_id); // 0x00B7BED0 (:3770)
unsigned int vehicle_get_axle_id(const vehicle_component_data* component); // 0x00B66210 (:3790)
unsigned int vehicle_get_component_hit_points(unsigned int v_handle, unsigned int component); // 0x00B7BF00 (:3814)
void vehicle_set_component_hit_points(unsigned int v_handle, unsigned int component, unsigned int hit_points); // 0x00B7BF40 (:3834)
bool vehicle_get_component_unlatched(unsigned int v_handle, unsigned int component_index); // 0x00B7BF90 (:3855)
bool vehicle_get_component_detached(unsigned int v_handle, unsigned int component_index); // 0x00B7BFE0 (:3886)
void vehicle_set_component_detached(unsigned int v_handle, unsigned int component_index, bool detached); // 0x00BA0D40 (:3915)
void vehicle_components_process_damage(const vehicle_component_accident_info*& accidents, unsigned int& num_accidents); // 0x00BBBC90 (:3962)
bool vehicle_component_enable_collision_with_humans(vehicle_component* component, bool enable); // 0x00B66270 (:4012)

// 0x00B62E00 (.h:112)
inline const vector& vehicle_component::get_position() const
{
    return m_data->m_position;
}

// 0x00A05A80 (.h:118)
inline const vector& vehicle_component::get_chassis_space_position() const
{
    return m_cs_position;
}

// 0x00A05A90 (.h:124)
inline const matrix& vehicle_component::get_chassis_space_rotation() const
{
    return m_cs_rotation;
}

// 0x00A05AA0 (.h:130)
inline const vector& vehicle_component::get_world_space_position() const
{
    return m_ws_position;
}

// 0x00AEA000 (.h:136)
inline const matrix& vehicle_component::get_world_space_rotation() const
{
    return m_ws_rotation;
}

// 0x00AEA010 (.h:142)
inline unsigned int vehicle_component::get_type() const
{
    return m_data->m_type;
}

// 0x00B62E10 (.h:148)
inline vehicle_component_durability_types vehicle_component::get_durability() const
{
    return (vehicle_component_durability_types)m_data->m_durability;
}

// 0x00943030 (.h:154)
inline unsigned int vehicle_component::get_flags() const
{
    return m_flags;
}

// 0x00B62E20 (.h:160)
inline void vehicle_component::set_flags(unsigned int flags)
{
    m_flags |= flags;
}

// 0x00B62E30 (.h:168)
inline void vehicle_component::clear_flags(unsigned int flags)
{
    m_flags &= ~flags;
}

// 0x00B62E40 (.h:175)
inline const char* vehicle_component::get_bone_name() const
{
    return m_data->bone_name;
}

// 0x00A05AB0 (.h:181)
inline const vehicle_component_data* vehicle_component::get_data() const
{
    return m_data;
}

// 0x00B64610
inline vehicle_component::vehicle_component()
{
}
