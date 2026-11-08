// Original: volition/rfg/code/objects/object_debris.h
// Implementation: volition/rfg/code/objects/object_debris.cpp
//
// object_debris, a loose physical piece (broken vehicle parts, retrievable salvage) created from a debris_create_info
// (mesh, physics properties, collision, optional precomputed inertia). object_debris_process runs once per frame.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class hkpShape;
class human;
struct lod_info;
class rl_renderable_instance;
class rl_renderer;
struct static_mesh;
struct weapon_fire_info;
struct weapon_info;

// The copy constructor is compiler generated (0x00A174C0).
// size 0xBC
struct debris_create_info
{
    debris_create_info(); // 0x00743320 (.h:43)

    vector m_position; // 0x00
    matrix m_orientation; // 0x0C
    float m_mass; // 0x30
    float m_friction; // 0x34
    float m_restitution; // 0x38
    float m_linear_damp; // 0x3C
    float m_angular_damp; // 0x40
    unsigned char m_physical_material; // 0x44
    unsigned char m_effect_material; // 0x45
    unsigned int m_mesh_srid; // 0x48
    rl_renderable_instance* m_renderable_instance_p; // 0x4C
    static_mesh* m_mesh; // 0x50
    int m_submesh_id; // 0x54
    unsigned int m_num_lod_infos; // 0x58
    const lod_info* m_lod_info; // 0x5C
    unsigned int m_collision_type; // 0x60
    unsigned int m_cm_index; // 0x64
    hkpShape* m_shape; // 0x68
    int m_impact_foley_id; // 0x6C
    unsigned int m_delay_before_first_impact_foley; // 0x70
    unsigned int m_vehicle_handle; // 0x74
    unsigned int m_component_index; // 0x78
    bool m_is_persistent; // 0x7C
    bool m_is_retrievable; // 0x7D
    int m_retrievable_material_type; // 0x80
    int m_deformed_morph_target; // 0x84
    bool m_has_precalc_inertia; // 0x88
    matrix m_precalc_tensor; // 0x8C
    vector m_precalc_center_of_mass; // 0xB0
};

// size 0x19C
class object_debris : public object
{
public:
    object_debris(); // 0x00A17160 (:129)
    static const object_type get_type_static(); // 0x007115E0 (.h:129)
    static const object_sub_type get_subtype_static(); // 0x007115F0 (.h:129)
    static const char* get_classname_static(); // 0x00A13EC0 (.h:129)
    static int get_bpo_index_static(); // 0x00A13ED0 (.h:129)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A171B0 (.h:129)
    static unsigned int get_classname_hash(); // 0x00A17150 (:128)
    static int pool_remaining(); // 0x00A3AA90 (:128)
    virtual void pool_free(); // vtbl+0x80 0x00A8FFB0 (:128)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA65D0 (:245)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A175D0 (:360)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A62A10 (:373)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8FFC0 (:445)
public:
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A171D0 (:138)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A62970 (:154)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00A17220 (:169)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00A17790 (:656)
    void create_havok_body(const debris_create_info* dci); // 0x00A172F0 (:182)
    void free_havok_body(); // 0x00A17490 (:229)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A3AAC0 (:476)
    virtual ~object_debris(); // vtbl+0x00 0x00A171C0

    int debris_flags; // 0x9C
    timestamp m_min_existance_time; // 0xA0
    bool m_is_persistent; // 0xA4
    timestamp m_retrievable_delay; // 0xA8
    int m_retrievable_material_type; // 0xAC
    int fade_ms; // 0xB0
    timestamp m_fade_stamp; // 0xB4
    bool fading_from_nano; // 0xB8
    unsigned int m_num_lod_infos; // 0xBC
    const lod_info* m_lod_info; // 0xC0
    int m_impact_foley_id; // 0xC4
    timestamp m_impact_foley_delay; // 0xC8
    timestamp m_effect_end; // 0xCC
    unsigned int m_effect_instance; // 0xD0
    debris_create_info m_create_info; // 0xD4
    rl_renderable_instance* m_renderable_instance_p; // 0x190
    static_mesh* m_mesh; // 0x194
    int m_submesh_id; // 0x198
};

// size 0xC
class obj_debris_props : public obj_prop_block
{
public:
    obj_debris_props(const debris_create_info* ci); // 0x00A3A990 (:109)
    ~obj_debris_props(); // 0x00744010

};

void unused_dcf_clear_debris(); // 0x00A628A0 (:70)
const char* classname_object_debris(); // 0x00A17140 (:128)
object* alloc_object_debris(); // 0x00A62960 (:128)
int bpo_index_object_debris(); // 0x00A17130 (:128)
void types_object_debris(signed char& type, signed char& subtype); // 0x00A17120 (:128)
void object_debris_mark_for_fade(object_debris* d_obj, int time_until_fade, int fade_time, bool from_nano); // 0x00A175E0 (:535)
bool object_debris_is_fading(object_debris* d_obj); // 0x00A17640 (:549)
void object_debris_process(); // 0x00A62B20 (:556)
void object_debris_process_explosion_hit(object_debris* d_obj, const vector* linear_impulse, const vector* angular_impulse, bool can_delete); // 0x00A17660 (:622)
void object_debris_play_collision_sound(object_debris* d_obj, const vector* position, float speed_along_normal); // 0x00A177B0 (:670)
void object_debris_system_init(); // 0x00A17850 (:708)
void object_debris_system_shutdown(); // 0x00A17870 (:717)
void object_debris_level_init_post(); // 0x00A17890 (:728)
void object_debris_level_shutdown(); // 0x00A62CB0 (:734)
