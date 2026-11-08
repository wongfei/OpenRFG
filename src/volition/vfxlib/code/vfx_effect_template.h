// Original: volition/vfxlib/code/vfx_effect_template.h
// Implementation: volition/vfxlib/code/vfx_effect_template.cpp
//
// An effect template as serialized in the .vfx file (offsets fixed up to pointers on load): emitters, filters, meshes,
// lights, coronas and expressions (named functions applied with parameters to emitter or filter properties, looked up
// by name CRC in vfx_effect_expressions.cpp). Placement of vfx_expressions_set_global_variable (defined in
// vfx_effect_expressions.cpp, which has no header) inferred.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_particle_material.h"
#include "volition/render_lib/code/rl_renderable.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

class mempool;
class rl_class_factory_info;
class rl_handle;
class object_effect;
struct tracer;
enum weapon_tracer_type; // rfg's tracers.h, for the tracers' friend declarations (MSVC opaque enum, as in the Havok headers)

// size 0x20
class vfx_effect_template : private rl_base_object
{
public:
    enum mesh_type
    {
        MESH_TYPE_NORMAL = 1,
        MESH_TYPE_FACING = 2,
        MESH_TYPE_ROD = 4,
        MESH_TYPE_ADDITIVE = 8,
        MESH_TYPE_VOLFOG = 16,
        _MESH_TYPE_SIZEOF_INT = -1,
    };

    enum emitter_type
    {
        EMITTER_TYPE_SPHERE = 0,
        EMITTER_TYPE_PLANE = 1,
        EMITTER_TYPE_POINT = 2,
        EMITTER_TYPE_BOX = 3,
        EMITTER_TYPE_CYLINDER = 4,
        EMITTER_TYPE_MAX = 5,
    };

    enum filter_flags
    {
        FILTER_FLAGS_GLOBAL = 0,
        FILTER_FLAGS_PER_EMITTER = 1,
        FILTER_FLAGS_PER_EFFECT = 2,
    };

    enum filter_type
    {
        FILTER_TYPE_GRAVITY = 0,
        FILTER_TYPE_WIND = 1,
        FILTER_TYPE_MAGNET = 2,
        FILTER_TYPE_TURBULENCE = 3,
        FILTER_TYPE_VORTEX = 4,
        FILTER_TYPE_ORIENTOR = 5,
    };

    typedef int handle;
    struct expression_param;
    typedef void (*expression_function)(void*, expression_param*, int);

    // size 0x60
    struct light
    {
        et_sized_native_pointer<char> name; // 0x00
        short flags; // 0x08
        short type; // 0x0A
        int num_clr_keys; // 0x0C
        et_sized_native_pointer<vfx_vector_key> clr_keys; // 0x10
        int num_intensity_keys; // 0x18
        et_sized_native_pointer<vfx_float_key> intensity_keys; // 0x20
        int num_position_keys; // 0x28
        et_sized_native_pointer<vfx_vector_key> position_keys; // 0x30
        int num_fvec_keys; // 0x38
        et_sized_native_pointer<vfx_vector_key> fvec_keys; // 0x40
        float hotspot_size; // 0x48
        float hotspot_falloff_size; // 0x4C
        float atten_start; // 0x50
        float atten_end; // 0x54
        int parent_index; // 0x58
    };

    // size 0xB0
    struct mesh
    {
        et_sized_native_pointer<char> name; // 0x00
        unsigned int type; // 0x08
        int num_position_keys; // 0x0C
        et_sized_native_pointer<vfx_vector_key> position_keys; // 0x10
        int num_rotation_keys; // 0x18
        et_sized_native_pointer<vfx_quat_key> rotation_keys; // 0x20
        int num_scale_keys; // 0x28
        et_sized_native_pointer<vfx_vector_key> scale_keys; // 0x30
        int num_opacity_keys; // 0x38
        et_sized_native_pointer<vfx_float_key> opacity_keys; // 0x40
        et_sized_native_pointer<rl_mesh_data> mesh_data_p; // 0x48
        et_sized_native_pointer<rl_material_map_data> material_map_data_p; // 0x50
        et_sized_native_pointer<rl_material_map> material_map_p; // 0x58
        unsigned int num_materials; // 0x60
        et_sized_native_pointer<et_sized_native_pointer<rl_material> > materials; // 0x68
        et_sized_native_pointer<unsigned char> collision_model_data_p; // 0x70
        unsigned int shape_material_index; // 0x78
        float shape_volume; // 0x7C
        vector shape_center_of_mass; // 0x80
        matrix shape_inertia_tensor; // 0x8C
    };

    // size 0x3C8
    struct emitter
    {
        et_sized_native_pointer<char> name; // 0x00
        rl_particle_material_data material; // 0x08
        int type; // 0x38
        unsigned int class_id; // 0x3C
        bool is_iterative; // 0x40
        bool is_reverse_render; // 0x41
        bool is_half_res; // 0x42
        bool is_quarter_res; // 0x43
        bool is_always_on_top; // 0x44
        bool is_soft_render; // 0x45
        float interations_per_second; // 0x48
        bool texture_anim_random_frames; // 0x4C
        bool is_emit_node; // 0x4D
        bool inherit_velocity; // 0x4E
        bool local_space_emission; // 0x4F
        bool acceleration_enabled; // 0x50
        vector acceleration_start; // 0x54
        vector acceleration_end; // 0x60
        bool acceleration_variance_enabled; // 0x6C
        vector acceleration_start_variance; // 0x70
        vector acceleration_end_variance; // 0x7C
        bool use_color_mid_value; // 0x88
        bool is_color_variance_by_value; // 0x89
        bool use_color_saturation_value; // 0x8A
        rl_color_float color_start; // 0x8C
        rl_color_float color_start_variance; // 0x9C
        rl_color_float color_end; // 0xAC
        rl_color_float color_end_variance; // 0xBC
        rl_color_float color_mid_A; // 0xCC
        rl_color_float color_mid_A_variance; // 0xDC
        float color_mid_A_offset; // 0xEC
        rl_color_float color_mid_B; // 0xF0
        rl_color_float color_mid_B_variance; // 0x100
        float color_mid_B_offset; // 0x110
        float alpha_mid_A_offset; // 0x114
        float alpha_mid_B_offset; // 0x118
        float color_saturation_start; // 0x11C
        float color_saturation_start_variance; // 0x120
        float color_saturation_end; // 0x124
        float color_saturation_end_variance; // 0x128
        float color_saturation_mid_A; // 0x12C
        float color_saturation_mid_A_variance; // 0x130
        float color_saturation_mid_B; // 0x134
        float color_saturation_mid_B_variance; // 0x138
        bool use_additive_key_value; // 0x13C
        float additive_key_start; // 0x140
        float additive_key_start_variance; // 0x144
        float additive_key_end; // 0x148
        float additive_key_end_variance; // 0x14C
        float additive_key_mid_A; // 0x150
        float additive_key_mid_A_variance; // 0x154
        float additive_key_mid_B; // 0x158
        float additive_key_mid_B_variance; // 0x15C
        et_sized_native_pointer<matrix> color_correction_matrix; // 0x160
        int num_texture_anim_rate_keys; // 0x168
        et_sized_native_pointer<vfx_float_key> texture_anim_rate_keys; // 0x170
        bool rotation_enabled; // 0x178
        float weight_start; // 0x17C
        float weight_end; // 0x180
        float density; // 0x184
        float density_variance; // 0x188
        int num_emit_node_emitters; // 0x18C
        et_sized_native_pointer<et_sized_native_pointer<vfx_particle_system_emitter> > emit_node_emitters; // 0x190
        int num_tails; // 0x198
        int particle_type; // 0x19C
        int material_index; // 0x1A0
        float yaw_variance; // 0x1A4
        float pitch_variance; // 0x1A8
        int max_particles; // 0x1AC
        bool use_size_mid_value; // 0x1B0
        bool use_length_mid_value; // 0x1B1
        bool use_length; // 0x1B2
        bool use_lifetime_variance; // 0x1B3
        float lifetime; // 0x1B4
        float lifetime_variance; // 0x1B8
        float speed_variance; // 0x1BC
        float angular_speed_start_variance; // 0x1C0
        float angular_speed_end_variance; // 0x1C4
        bool is_random_start_rotation; // 0x1C8
        float lod_large_particle_fade_min; // 0x1CC
        float lod_large_particle_fade_max; // 0x1D0
        float lod_emission_distance; // 0x1D4
        float self_illum; // 0x1D8
        vector ambient_color; // 0x1DC
        vector diffuse_color; // 0x1E8
        float effect_material_percent; // 0x1F4
        float soft_thickness; // 0x1F8
        char pad[4]; // 0x1FC
        int num_position_keys; // 0x200
        et_sized_native_pointer<vfx_vector_key> position_keys; // 0x208
        int num_rotation_keys; // 0x210
        et_sized_native_pointer<vfx_quat_key> rotation_keys; // 0x218
        int num_scale_keys; // 0x220
        et_sized_native_pointer<vfx_vector_key> scale_keys; // 0x228
        int num_emission_rate_keys; // 0x230
        et_sized_native_pointer<vfx_float_key> emission_rate_keys; // 0x238
        int num_angular_speed_start_keys; // 0x240
        et_sized_native_pointer<vfx_float_key> angular_speed_start_keys; // 0x248
        int num_angular_speed_end_keys; // 0x250
        et_sized_native_pointer<vfx_float_key> angular_speed_end_keys; // 0x258
        int num_speed_keys; // 0x260
        et_sized_native_pointer<vfx_float_key> speed_keys; // 0x268
        int num_size_start_keys; // 0x270
        et_sized_native_pointer<vfx_float_key> size_start_keys; // 0x278
        int num_size_start_var_keys; // 0x280
        et_sized_native_pointer<vfx_float_key> size_start_var_keys; // 0x288
        int num_size_mida_keys; // 0x290
        et_sized_native_pointer<vfx_float_key> size_mida_keys; // 0x298
        int num_size_mida_var_keys; // 0x2A0
        et_sized_native_pointer<vfx_float_key> size_mida_var_keys; // 0x2A8
        int num_size_mida_offset_keys; // 0x2B0
        et_sized_native_pointer<vfx_float_key> size_mida_offset_keys; // 0x2B8
        int num_size_midb_keys; // 0x2C0
        et_sized_native_pointer<vfx_float_key> size_midb_keys; // 0x2C8
        int num_size_midb_var_keys; // 0x2D0
        et_sized_native_pointer<vfx_float_key> size_midb_var_keys; // 0x2D8
        int num_size_midb_offset_keys; // 0x2E0
        et_sized_native_pointer<vfx_float_key> size_midb_offset_keys; // 0x2E8
        int num_size_end_keys; // 0x2F0
        et_sized_native_pointer<vfx_float_key> size_end_keys; // 0x2F8
        int num_size_end_var_keys; // 0x300
        et_sized_native_pointer<vfx_float_key> size_end_var_keys; // 0x308
        int num_length_start_keys; // 0x310
        et_sized_native_pointer<vfx_float_key> length_start_keys; // 0x318
        int num_length_start_var_keys; // 0x320
        et_sized_native_pointer<vfx_float_key> length_start_var_keys; // 0x328
        int num_length_mida_keys; // 0x330
        et_sized_native_pointer<vfx_float_key> length_mida_keys; // 0x338
        int num_length_mida_var_keys; // 0x340
        et_sized_native_pointer<vfx_float_key> length_mida_var_keys; // 0x348
        int num_length_mida_offset_keys; // 0x350
        et_sized_native_pointer<vfx_float_key> length_mida_offset_keys; // 0x358
        int num_length_midb_keys; // 0x360
        et_sized_native_pointer<vfx_float_key> length_midb_keys; // 0x368
        int num_length_midb_var_keys; // 0x370
        et_sized_native_pointer<vfx_float_key> length_midb_var_keys; // 0x378
        int num_length_midb_offset_keys; // 0x380
        et_sized_native_pointer<vfx_float_key> length_midb_offset_keys; // 0x388
        int num_length_end_keys; // 0x390
        et_sized_native_pointer<vfx_float_key> length_end_keys; // 0x398
        int num_length_end_var_keys; // 0x3A0
        et_sized_native_pointer<vfx_float_key> length_end_var_keys; // 0x3A8
        et_sized_native_pointer<vfx_particle_system_emitter> particle_system_emitter_p; // 0x3B0
        int parent_index; // 0x3B8
        int unique_data_size; // 0x3BC
        et_sized_native_pointer<unsigned char> unique_data; // 0x3C0
    };

    // size 0x70
    struct filter
    {
        et_sized_native_pointer<char> name; // 0x00
        int type; // 0x08
        int flags; // 0x0C
        unsigned int class_id; // 0x10
        int parent_index; // 0x14
        int num_affected_emitters; // 0x18
        et_sized_native_pointer<int> affected_emitters; // 0x20
        int num_affected_meshes; // 0x28
        et_sized_native_pointer<int> affected_meshes; // 0x30
        int num_position_keys; // 0x38
        et_sized_native_pointer<vfx_vector_key> position_keys; // 0x40
        int num_rotation_keys; // 0x48
        et_sized_native_pointer<vfx_quat_key> rotation_keys; // 0x50
        et_sized_native_pointer<vfx_particle_system_filter> particle_system_filter_p; // 0x58
        int unique_data_size; // 0x60
        et_sized_native_pointer<unsigned char> unique_data; // 0x68
    };

    struct force_serialized_data_block; // no definition in PDB

    // size 0x10
    struct expression_param
    {
        bool m_variable; // 0x00
        unsigned int m_variable_crc; // 0x04
        et_sized_native_pointer<unsigned char> m_data_p_index; // 0x08
    };

    // size 0x18
    struct expression
    {
        unsigned int m_expression_function_name_crc; // 0x00
        unsigned int just_a_stub; // 0x04
        int object_index; // 0x08
        int m_num_expression_params; // 0x0C
        et_sized_native_pointer<expression_param> m_params; // 0x10
    };

    // size 0x1D8
    struct corona
    {
        et_sized_native_pointer<char> name; // 0x00
        rl_particle_material_data material; // 0x08
        int type; // 0x38
        et_sized_native_pointer<vfx_float_key> rotation_offset_keys; // 0x40
        int num_rotation_offset_keys; // 0x48
        int num_position_keys; // 0x4C
        bool is_linear_scaling; // 0x50
        bool is_linear_fading; // 0x51
        et_sized_native_pointer<vfx_vector_key> position_keys; // 0x58
        int num_rotation_keys; // 0x60
        et_sized_native_pointer<vfx_quat_key> rotation_keys; // 0x68
        int num_scale_keys; // 0x70
        et_sized_native_pointer<vfx_vector_key> scale_keys; // 0x78
        int num_radius_start_keys; // 0x80
        et_sized_native_pointer<vfx_float_key> radius_start_keys; // 0x88
        int num_radius_end_keys; // 0x90
        et_sized_native_pointer<vfx_float_key> radius_end_keys; // 0x98
        int num_width_scale_keys; // 0xA0
        et_sized_native_pointer<vfx_float_key> width_scale_keys; // 0xA8
        int num_visibility_radius_keys; // 0xB0
        et_sized_native_pointer<vfx_float_key> visibility_radius_keys; // 0xB8
        int num_color_start_keys; // 0xC0
        et_sized_native_pointer<vfx_vector4_key> color_start_keys; // 0xC8
        int num_color_end_keys; // 0xD0
        et_sized_native_pointer<vfx_vector4_key> color_end_keys; // 0xD8
        int num_position_offset_keys; // 0xE0
        et_sized_native_pointer<vfx_float_key> position_offset_keys; // 0xE8
        int num_saturation_keys; // 0xF0
        et_sized_native_pointer<vfx_float_key> saturation_keys; // 0xF8
        int num_rotation_scale_keys; // 0x100
        et_sized_native_pointer<vfx_float_key> rotation_scale_keys; // 0x108
        int num_intensity_scale_keys; // 0x110
        et_sized_native_pointer<vfx_float_key> intensity_scale_keys; // 0x118
        int num_intensity_exponential_decay_keys; // 0x120
        et_sized_native_pointer<vfx_float_key> intensity_exponential_decay_keys; // 0x128
        int num_opacity_exponential_decay_keys; // 0x130
        et_sized_native_pointer<vfx_float_key> opacity_exponential_decay_keys; // 0x138
        int num_radius_distance_start_keys; // 0x140
        et_sized_native_pointer<vfx_float_key> radius_distance_start_keys; // 0x148
        int num_radius_distance_end_keys; // 0x150
        et_sized_native_pointer<vfx_float_key> radius_distance_end_keys; // 0x158
        int num_color_distance_start_keys; // 0x160
        et_sized_native_pointer<vfx_float_key> color_distance_start_keys; // 0x168
        int num_color_distance_end_keys; // 0x170
        et_sized_native_pointer<vfx_float_key> color_distance_end_keys; // 0x178
        int num_radius_exponential_decay_keys; // 0x180
        et_sized_native_pointer<vfx_float_key> radius_exponential_decay_keys; // 0x188
        int num_view_fade_scale_keys; // 0x190
        et_sized_native_pointer<vfx_float_key> view_fade_scale_keys; // 0x198
        int num_view_fade_decay_keys; // 0x1A0
        et_sized_native_pointer<vfx_float_key> view_fade_decay_keys; // 0x1A8
        int num_opacity_visibility_decay_keys; // 0x1B0
        et_sized_native_pointer<vfx_float_key> opacity_visibility_decay_keys; // 0x1B8
        int num_radius_visibility_decay_keys; // 0x1C0
        et_sized_native_pointer<vfx_float_key> radius_visibility_decay_keys; // 0x1C8
        bool view_fade_intensity_enabled; // 0x1D0
        bool view_fade_opacity_enabled; // 0x1D1
        bool view_fade_radius_enabled; // 0x1D2
        bool pad0; // 0x1D3
    };

    // size 0x90
    struct vfx_data
    {
        int signature; // 0x00
        int version; // 0x04
        int num_expressions; // 0x08
        et_sized_native_pointer<expression> expressions; // 0x10
        et_sized_native_pointer<char> effect_name; // 0x18
        float duration; // 0x20
        float radius; // 0x24
        int num_bitmaps; // 0x28
        et_sized_native_pointer<et_sized_native_pointer<char> > bitmaps; // 0x30
        et_sized_native_pointer<char> mesh_bitmaps; // 0x38
        int num_meshes; // 0x40
        et_sized_native_pointer<mesh> meshes; // 0x48
        int num_emitters; // 0x50
        et_sized_native_pointer<emitter> emitters; // 0x58
        int num_lights; // 0x60
        et_sized_native_pointer<light> lights; // 0x68
        int num_filters; // 0x70
        et_sized_native_pointer<filter> filters; // 0x78
        int num_coronas; // 0x80
        et_sized_native_pointer<corona> coronas; // 0x88
    };

protected:
    vfx_effect_template(); // 0x00CA45C0 (:38)
    virtual ~vfx_effect_template(); // vtbl+0x00 0x00CB4EC0 (:46)
private:
    static const pool_list<vfx_effect_template*>& get_object_pool(); // 0x00C9D200 (.h:48)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D210 (.h:48)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CAEF40 (:34)
    static vfx_effect_template* create_placement(vfx_effect_template* _data_p); // 0x00CAF0A0 (:34)
    static vfx_effect_template* create(); // 0x00CAF030 (:34)
    static bool can_create(); // 0x00CA45B0 (:34)
    virtual void release(); // vtbl+0x04 0x00CAF0E0 (:34)
    void set_handle(int insthandle); // 0x00C9D220 (.h:429)
    int get_handle() const; // 0x00C9D230 (.h:430)
    float get_duration() const; // 0x00741DF0 (.h:432)
    float get_bounding_radius() const; // 0x00741E00 (.h:433)
    int get_mesh_count() const;
    int get_mesh_type(int mesh_index); // 0x00CA3130 (.h:519)
    vfx_vector_key* get_mesh_position_keys(int mesh_index); // 0x00CA3150 (.h:525)
    int get_mesh_position_keys_count(int mesh_index); // 0x00CA3170 (.h:531)
    vfx_quat_key* get_mesh_rotation_keys(int mesh_index); // 0x00CA3190 (.h:537)
    int get_mesh_rotation_keys_count(int mesh_index); // 0x00CA31B0 (.h:543)
    vfx_vector_key* get_mesh_scale_keys(int mesh_index); // 0x00CA31D0 (.h:549)
    int get_mesh_scale_keys_count(int mesh_index); // 0x00CA31F0 (.h:555)
    vfx_float_key* get_mesh_opacity_keys(int mesh_index); // 0x00CA3210 (.h:561)
    int get_mesh_opacity_keys_count(int mesh_index); // 0x00CA3230 (.h:567)
    int get_num_lights() const; // 0x00C9D240 (.h:447)
    light* get_light(int light_index) const; // 0x00CA3250 (.h:573)
    light* get_light_by_name(char*) const;
    int get_emitter_count() const; // 0x005D1840 (.h:451)
    vfx_particle_system_emitter* get_emitter(int emitter_index) const; // 0x005D7150 (.h:589)
    vfx_particle_system_emitter* get_emitter_by_name(char*) const;
    const char* get_emitter_name(int emitter_index) const; // 0x005D7170 (.h:607)
    int get_emitter_parent_index(int emitter_index) const; // 0x00CA3270 (.h:614)
    int get_filter_parent_index(int filter_index) const; // 0x00CA32A0 (.h:620)
    int get_filter_count() const; // 0x00C9D250 (.h:457)
    vfx_particle_system_filter* get_filter(int filter_index) const; // 0x00CAB8B0 (.h:626)
    filter* get_filter_by_name(char*) const;
    vfx_vector_key* get_filter_position_keys(int filter_index) const; // 0x00CA32C0 (.h:643)
    int get_filter_num_position_keys(int filter_index) const; // 0x00CA32E0 (.h:648)
    vfx_quat_key* get_filter_rotation_keys(int filter_index) const; // 0x00CA3300 (.h:653)
    int get_filter_num_rotation_keys(int filter_index) const; // 0x00CA3320 (.h:658)
    vfx_float_key* get_filter_force_keys(int filter_index) const; // 0x00CA3340 (.h:663)
    int get_filter_num_force_keys(int filter_index) const; // 0x00CA3360 (.h:669)
    vfx_float_key* get_filter_noise_keys(int filter_index) const; // 0x00CA3380 (.h:675)
    int get_filter_num_noise_keys(int filter_index) const; // 0x00CA33A0 (.h:681)
    vfx_float_key* get_filter_angular_velocity_keys(int filter_index) const; // 0x00CA33C0 (.h:687)
    int get_filter_num_angular_velocity_keys(int filter_index) const; // 0x00CA33E0 (.h:693)
    vfx_float_key* get_filter_angular_velocity_variance_keys(int filter_index) const; // 0x00CA3400 (.h:699)
    int get_filter_num_angular_velocity_variance_keys(int filter_index) const; // 0x00CA3420 (.h:705)
    bool get_filter_is_global(int filter_index) const; // 0x00CA3440 (.h:711)
    int* get_filter_affected_emitters(int filter_index) const; // 0x00CA3460 (.h:716)
    int get_filter_affected_emitters_count(int filter_index) const; // 0x00CA3480 (.h:721)
    int* get_filter_affected_meshes(int filter_index) const; // 0x00CA34A0 (.h:727)
    int get_filter_affected_meshes_count(int filter_index) const; // 0x00CA34C0 (.h:732)
    rl_handle get_shape_handle(int mesh_index) const; // 0x00CA34E0 (.h:749)
    unsigned int get_shape_material_index(int mesh_index) const; // 0x00CA3500 (.h:758)
    float get_shape_volume(int mesh_index) const; // 0x00CA3520 (.h:764)
    const vector& get_shape_center_of_mass(int mesh_index) const; // 0x00CA3540 (.h:769)
    const matrix& get_shape_inertia_tensor(int mesh_index) const; // 0x00CA3560 (.h:774)
    char* get_effect_name() const; // 0x005D7140 (.h:482)
    void set_peg_filename(const char* _peg_filename); // 0x00C9D260 (.h:483)
    const char* get_peg_filename() const; // 0x00C9D270 (.h:484)
    linked_list_p<rl_renderable>& get_mesh_renderables(); // 0x00C9D280 (.h:738)
    rl_material_map* get_material_map(int mesh_index) const; // 0x00CAB8D0 (.h:743)
    int get_expression_count() const; // 0x00741E10 (.h:780)
    const expression& get_expression(int expression_index) const; // 0x007476C0 (.h:785)
    bool serialize(vfx_data* vfx_data_p, unsigned char* gpu_data, bool load, int& approx_gpu_size); // 0x00CB87D0 (:53)
    static void (*get_expression_function(unsigned int function_name_crc))(void*, expression_param*, int); // 0x00CAEB30 (vfx_effect_expressions.cpp:245)
    static int get_expression_variable_index(unsigned int variable_name_crc); // 0x00CB4D50 (vfx_effect_expressions.cpp:252)
    bool get_is_emit_node_emitter(int emitter_index) const; // 0x00CA3580 (.h:791)
    int get_emit_node_emitter_count(int) const;
    vfx_particle_system_emitter* get_emit_node_emitter(int) const;
    int get_num_coronas() const; // 0x00C9D290 (.h:803)
    corona* get_corona(int corona_index) const; // 0x00CA35A0 (.h:808)
    corona* get_corona_by_name(char*) const;
private:
    void load_particle_material(rl_particle_material_data& material); // 0x00CAF160 (:307)
    bool init(); // 0x00CB4EE0 (:353)
    void finalize(); // 0x00CAF350 (:646)

public:
    static const unsigned int m___class_id; // 0x012C707C
    static rl_class_factory_info ms_class_factory_info; // 0x016F0EE0
protected:
    static pool_list<vfx_effect_template*> ms_object_pool; // 0x016F0F10
private:
    // rfg's weapons.cpp (weapon_create_arc_effect) edits the first emitter (friends are not recorded in rfg.pdb).
    friend void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    friend void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    // rfg's tracers.cpp primes the tracer effect's emitters and moves its particles directly (friends are not recorded
    // in rfg.pdb).
    friend void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void tracers_process();
    friend void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos);
    // vfx_effect reads the vfx data and mesh renderables directly (the .h accessors are inlined there).
    friend class vfx_effect;
    linked_list_p<rl_renderable> m_mesh_renderables; // 0x08
    vfx_data* m_vfx_data_p; // 0x10
    bool m_initialized; // 0x14
    int m_instance_handle; // 0x18
    const char* peg_filename; // 0x1C
};

void vfx_expressions_set_global_variable(unsigned int variable_name_crc, float value); // 0x00CB4DE0 (vfx_effect_expressions.cpp:267)
