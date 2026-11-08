// Original: volition/vfxlib/code/vfx_particle_system_emitter.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter.cpp
//
// The emitter base: emission rate, lifetime, speed, size, color, opacity and rotation curves (keys), particle type
// (parametric or iterative), the filters it uses and the virtual shape interface the box, cylinder, plane, point and
// sphere emitters implement (birth position and velocity). Most accessors are inline in vfx_particle_system_emitter.inl.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_particle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/util/varray.h"

class rl_renderable_particles;
struct vfx_float_key;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;
struct vfx_quat_key;
struct vfx_vector_key;
class object_effect;
struct tracer;
enum weapon_tracer_type; // rfg's tracers.h, for the tracers' friend declarations (MSVC opaque enum, as in the Havok headers)

// size 0x2D0
class vfx_particle_system_emitter : public rl_base_object
{
protected:
    vfx_particle_system_emitter(); // 0x00CAF5A0 (:111)
    virtual ~vfx_particle_system_emitter(); // vtbl+0x00 0x00CAF930 (:116)
public:
    virtual void init(); // vtbl+0x08 0x00C9F0A0 (:121)
    virtual void spawn_particle(vfx_particle_base*, vfx_particle_system_emitter_instance*) = 0; // vtbl+0x0C
    virtual void initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x10 0x00C9F0C0 (:133)
    void set_particle_type(rl_particle_base::render_type _particle_type); // 0x00C9C0E0 (.h:72)
    rl_particle_base::render_type get_particle_type(); // 0x00C9C0F0 (.h:73)
    void set_is_iterative(bool _is_iterative); // 0x00C9C960 (vfx_particle_system_emitter.inl:331)
    bool get_is_iterative(); // 0x00741DD0 (vfx_particle_system_emitter.inl:336)
    void set_iterations_per_sec(float _iterations_per_sec); // 0x00C9C970 (vfx_particle_system_emitter.inl:342)
    float get_iterations_per_sec(); // 0x00741DE0 (vfx_particle_system_emitter.inl:347)
    void set_yaw_variance(float _yaw_variance); // 0x00C9C720 (vfx_particle_system_emitter.inl:3)
    float get_yaw_variance();
    void set_pitch_variance(float _pitch_variance); // 0x00C9C740 (vfx_particle_system_emitter.inl:13)
    float get_pitch_variance();
    float get_speed(float elapsed_time); // 0x005D4400 (vfx_particle_system_emitter.inl:58)
    float get_angular_speed_start(float elapsed_time); // 0x005D43A0 (vfx_particle_system_emitter.inl:24)
    float get_angular_speed_end(float elapsed_time); // 0x005D43D0 (vfx_particle_system_emitter.inl:29)
    float get_angular_speed_start_variance(); // 0x005D1770 (vfx_particle_system_emitter.inl:35)
    void set_angular_speed_start_variance(float _angular_speed_start_variance); // 0x00C9C760 (vfx_particle_system_emitter.inl:40)
    float get_angular_speed_end_variance(); // 0x005D1780 (vfx_particle_system_emitter.inl:46)
    void set_angular_speed_end_variance(float _angular_speed_end_variance); // 0x00C9C780 (vfx_particle_system_emitter.inl:51)
    bool get_is_rotation_enabled(); // 0x00C9C7D0 (vfx_particle_system_emitter.inl:74)
    void set_is_rotation_enabled(bool _is_rotation_enabled); // 0x00C9C7E0 (vfx_particle_system_emitter.inl:79)
    void set_is_random_start_rotation(bool _is_random_start_rotation); // 0x00C9C100 (.h:94)
    bool get_is_random_start_rotation(); // 0x00C9C110 (.h:95)
    void set_max_particles(int _max_particles); // 0x006E1830 (vfx_particle_system_emitter.inl:301)
    int get_max_particles(); // 0x00C9C930 (vfx_particle_system_emitter.inl:296)
    float get_lifetime(float elapsed_time); // 0x005D1810 (vfx_particle_system_emitter.inl:306)
    void set_lifetime(float _lifetime); // 0x006E1840 (vfx_particle_system_emitter.inl:313)
    float get_lifetime_variance(float elapsed_time); // 0x005D1820 (vfx_particle_system_emitter.inl:319)
    void set_lifetime_variance(float _lifetime_variance); // 0x00C9C940 (vfx_particle_system_emitter.inl:326)
    void set_use_lifetime_variance(bool _use_lifetime_variance); // 0x00C9C120 (.h:102)
    bool get_use_lifetime_variance(); // 0x00C9C130 (.h:103)
    void set_is_reverse_render(bool _is_reverse_render); // 0x00C9C980 (vfx_particle_system_emitter.inl:353)
    bool get_is_reverse_render();
    float get_emission_rate(float elapsed_time); // 0x005D4630 (vfx_particle_system_emitter.inl:989)
    float get_texture_anim_rate(float elapsed_time); // 0x00CA3100 (vfx_particle_system_emitter.inl:1016)
    void set_use_size_variance(bool _use_size_variance); // 0x00C9C140 (.h:109)
    bool get_use_size_variance(); // 0x00C9C150 (.h:110)
    void set_use_size_mid_value(bool _use_size_mid_value); // 0x00C9C160 (.h:111)
    bool get_use_size_mid_value(); // 0x00C9C170 (.h:112)
    void set_use_length_mid_value(bool _use_length_mid_value); // 0x00C9C180 (.h:113)
    bool get_use_length_mid_value(); // 0x00C9C190 (.h:114)
    void set_use_length(bool _use_length); // 0x00C9C1A0 (.h:116)
    bool get_use_length(); // 0x00C9C1B0 (.h:117)
    void set_use_length_variance(bool);
    bool get_use_length_variance(); // 0x00C9C1C0 (.h:119)
    float get_size_start(float elapsed_time); // 0x005D4430 (vfx_particle_system_emitter.inl:85)
    float get_size_end(float elapsed_time); // 0x005D4470 (vfx_particle_system_emitter.inl:90)
    float get_size_start_variance() const;
    float get_size_start_variance(float elapsed_time); // 0x005D44B0 (vfx_particle_system_emitter.inl:95)
    float get_size_end_variance() const;
    float get_size_end_variance(float elapsed_time); // 0x005D44F0 (vfx_particle_system_emitter.inl:100)
    float get_size_mid_A(float elapsed_time); // 0x005D4530 (vfx_particle_system_emitter.inl:105)
    float get_size_mid_B(float elapsed_time); // 0x005D45B0 (vfx_particle_system_emitter.inl:121)
    float get_size_mid_A_variance(float elapsed_time); // 0x005D4570 (vfx_particle_system_emitter.inl:110)
    float get_size_mid_B_variance(float elapsed_time); // 0x005D45F0 (vfx_particle_system_emitter.inl:126)
    float get_size_mid_A_offset(float elapsed_time); // 0x00CA2E80 (vfx_particle_system_emitter.inl:115)
    float get_size_mid_B_offset(float elapsed_time); // 0x00CA2EB0 (vfx_particle_system_emitter.inl:131)
    float get_length_start(float elapsed_time); // 0x00CA2EE0 (vfx_particle_system_emitter.inl:148)
    float get_length_end(float elapsed_time); // 0x00CA2F10 (vfx_particle_system_emitter.inl:153)
    float get_length_start_variance() const;
    float get_length_start_variance(float elapsed_time); // 0x00CA2F40 (vfx_particle_system_emitter.inl:158)
    float get_length_end_variance() const;
    float get_length_end_variance(float elapsed_time); // 0x00CA2F70 (vfx_particle_system_emitter.inl:163)
    float get_length_mid_A(float elapsed_time); // 0x00CA2FA0 (vfx_particle_system_emitter.inl:168)
    float get_length_mid_B(float elapsed_time); // 0x00CA3030 (vfx_particle_system_emitter.inl:184)
    float get_length_mid_A_variance(float elapsed_time); // 0x00CA2FD0 (vfx_particle_system_emitter.inl:173)
    float get_length_mid_B_variance(float elapsed_time); // 0x00CA3060 (vfx_particle_system_emitter.inl:189)
    float get_length_mid_A_offset(float elapsed_time); // 0x00CA3000 (vfx_particle_system_emitter.inl:178)
    float get_length_mid_B_offset(float elapsed_time); // 0x00CA3090 (vfx_particle_system_emitter.inl:194)
    void set_use_color_variance(bool _use_color_variance); // 0x00C9C1D0 (.h:145)
    bool get_use_color_variance(); // 0x00C9C1E0 (.h:146)
    void set_is_color_variance_by_value(bool _is_color_variance_by_value); // 0x00C9C1F0 (.h:147)
    bool get_is_color_variance_by_value(); // 0x00C9C200 (.h:148)
    void set_use_color_mid_value(bool _use_color_mid_value); // 0x00C9C210 (.h:149)
    bool get_use_color_mid_value(); // 0x00C9C220 (.h:150)
    void set_use_color_saturation_value(bool _use_color_saturation_value); // 0x00C9C230 (.h:151)
    bool get_use_color_saturation_value(); // 0x00C9C240 (.h:152)
    void set_use_additive_key_value(bool _use_additive_key_value); // 0x00C9C250 (.h:153)
    bool get_use_additive_key_value(); // 0x00C9C260 (.h:154)
    const rl_color_float& get_color_start(); // 0x005D1790 (vfx_particle_system_emitter.inl:201)
    void set_color_start(const rl_color_float& _color_start); // 0x00C9C810 (vfx_particle_system_emitter.inl:206)
    const rl_color_float& get_color_end(); // 0x005D17A0 (vfx_particle_system_emitter.inl:211)
    void set_color_end(const rl_color_float& _color_end); // 0x00C9C830 (vfx_particle_system_emitter.inl:216)
    const rl_color_float& get_color_start_variance(); // 0x005D17B0 (vfx_particle_system_emitter.inl:221)
    void set_color_start_variance(const rl_color_float& _color_start_variance); // 0x00C9C850 (vfx_particle_system_emitter.inl:226)
    const rl_color_float& get_color_end_variance(); // 0x005D17C0 (vfx_particle_system_emitter.inl:231)
    void set_color_end_variance(const rl_color_float& _color_end_variance); // 0x00C9C870 (vfx_particle_system_emitter.inl:236)
    const rl_color_float& get_color_mid_A(); // 0x005D17D0 (vfx_particle_system_emitter.inl:242)
    void set_color_mid_A(const rl_color_float& _color_mid_A); // 0x00C9C890 (vfx_particle_system_emitter.inl:247)
    const rl_color_float& get_color_mid_B(); // 0x005D17E0 (vfx_particle_system_emitter.inl:252)
    void set_color_mid_B(const rl_color_float& _color_mid_B); // 0x00C9C8B0 (vfx_particle_system_emitter.inl:257)
    const rl_color_float& get_color_mid_A_variance(); // 0x005D17F0 (vfx_particle_system_emitter.inl:262)
    void set_color_mid_A_variance(const rl_color_float& _color_mid_A_variance); // 0x00C9C8D0 (vfx_particle_system_emitter.inl:267)
    const rl_color_float& get_color_mid_B_variance(); // 0x005D1800 (vfx_particle_system_emitter.inl:272)
    void set_color_mid_B_variance(const rl_color_float& _color_mid_B_variance); // 0x00C9C8F0 (vfx_particle_system_emitter.inl:277)
    void set_color_mid_A_offset(float _color_mid_A_offset); // 0x00C9C270 (.h:171)
    float get_color_mid_A_offset(); // 0x00C9C290 (.h:172)
    void set_color_mid_B_offset(float _color_mid_B_offset); // 0x00C9C2A0 (.h:173)
    float get_color_mid_B_offset(); // 0x00C9C2C0 (.h:174)
    void set_alpha_mid_A_offset(float _alpha_mid_A_offset); // 0x00C9C2D0 (.h:175)
    float get_alpha_mid_A_offset(); // 0x00C9C2F0 (.h:176)
    void set_alpha_mid_B_offset(float _alpha_mid_B_offset); // 0x00C9C300 (.h:177)
    float get_alpha_mid_B_offset(); // 0x00C9C320 (.h:178)
    void set_color_saturation_start(float _color_saturation_start); // 0x00C9C330 (.h:180)
    float get_color_saturation_start(); // 0x00C9C350 (.h:181)
    void set_color_saturation_start_variance(float _color_saturation_start_variance); // 0x00C9C360 (.h:182)
    float get_color_saturation_start_variance(); // 0x00C9C380 (.h:183)
    void set_color_saturation_end(float _color_saturation_end); // 0x00C9C390 (.h:184)
    float get_color_saturation_end(); // 0x00C9C3B0 (.h:185)
    void set_color_saturation_end_variance(float _color_saturation_end_variance); // 0x00C9C3C0 (.h:186)
    float get_color_saturation_end_variance(); // 0x00C9C3E0 (.h:187)
    void set_color_saturation_mid_A(float _color_saturation_mid_A); // 0x00C9C3F0 (.h:188)
    float get_color_saturation_mid_A(); // 0x00C9C410 (.h:189)
    void set_color_saturation_mid_A_variance(float _color_saturation_mid_A_variance); // 0x00C9C420 (.h:190)
    float get_color_saturation_mid_A_variance(); // 0x00C9C440 (.h:191)
    void set_color_saturation_mid_B(float _color_saturation_mid_B); // 0x00C9C450 (.h:192)
    float get_color_saturation_mid_B(); // 0x00C9C470 (.h:193)
    void set_color_saturation_mid_B_variance(float _color_saturation_mid_B_variance); // 0x00C9C480 (.h:194)
    float get_color_saturation_mid_B_variance(); // 0x00C9C4A0 (.h:195)
    void set_additive_key_start(float _additive_key_start); // 0x00C9C4B0 (.h:197)
    float get_additive_key_start(); // 0x00C9C4D0 (.h:198)
    void set_additive_key_start_variance(float _additive_key_start_variance); // 0x00C9C4E0 (.h:199)
    float get_additive_key_start_variance(); // 0x00C9C500 (.h:200)
    void set_additive_key_end(float _additive_key_end); // 0x00C9C510 (.h:201)
    float get_additive_key_end(); // 0x00C9C530 (.h:202)
    void set_additive_key_end_variance(float _additive_key_end_variance); // 0x00C9C540 (.h:203)
    float get_additive_key_end_variance(); // 0x00C9C560 (.h:204)
    void set_additive_key_mid_A(float _additive_key_mid_A); // 0x00C9C570 (.h:205)
    float get_additive_key_mid_A(); // 0x00C9C590 (.h:206)
    void set_additive_key_mid_A_variance(float _additive_key_mid_A_variance); // 0x00C9C5A0 (.h:207)
    float get_additive_key_mid_A_variance(); // 0x00C9C5C0 (.h:208)
    void set_additive_key_mid_B(float _additive_key_mid_B); // 0x00C9C5D0 (.h:209)
    float get_additive_key_mid_B(); // 0x00C9C5F0 (.h:210)
    void set_additive_key_mid_B_variance(float _additive_key_mid_B_variance); // 0x00C9C600 (.h:211)
    float get_additive_key_mid_B_variance(); // 0x00C9C620 (.h:212)
    float get_speed_variance(); // 0x00C9C7A0 (vfx_particle_system_emitter.inl:63)
    void set_speed_variance(float _speed_variance); // 0x00C9C7B0 (vfx_particle_system_emitter.inl:68)
    void set_local_space_emission(bool _local_space_emission); // 0x00C9C630 (.h:218)
    bool get_local_space_emission(); // 0x005D1760 (.h:219)
    void set_weight_start(float weight_start); // 0x00C9C640 (.h:221)
    float get_weight_start(); // 0x00C9C650 (.h:222)
    void set_weight_end(float weight_end); // 0x00C9C660 (.h:223)
    float get_weight_end(); // 0x00C9C670 (.h:224)
    void set_weight_scale(float weight_scale); // 0x00C9C680 (.h:225)
    float get_weight_scale(); // 0x00C9C690 (.h:226)
    void set_use_weight_variance(bool);
    bool get_use_weight_variance();
    void set_density(float _density); // 0x00C9C6A0 (.h:231)
    float get_density(); // 0x00C9C6B0 (.h:232)
    void set_density_variance(float _density_variance); // 0x00C9C6C0 (.h:233)
    float get_density_variance(); // 0x00C9C6D0 (.h:234)
    void set_use_density_variance(bool _use_density_variance); // 0x00C9C6E0 (.h:235)
    bool get_use_density_variance(); // 0x00C9C6F0 (.h:236)
    void set_bounce(float);
    float get_bounce();
    void set_bounce_variance(float);
    float get_bounce_variance();
    void set_use_bounce_variance(bool);
    bool get_use_bounce_variance();
    void set_num_tails(int _num_tails); // 0x00C9C700 (.h:245)
    int get_num_tails(); // 0x00C9C710 (.h:246)
    void set_max_tails(int);
    int get_max_tails();
    bool get_use_accel_values(); // 0x00C9C910 (vfx_particle_system_emitter.inl:284)
    void set_use_accel_values(bool _use_accel_values); // 0x00C9C920 (vfx_particle_system_emitter.inl:289)
    bool get_use_accel_mid_value();
    void set_use_accel_mid_value(bool);
    const vector& get_acceleration_start(); // 0x00C9C990 (vfx_particle_system_emitter.inl:376)
    void set_acceleration_start(const vector& _acceleration_start); // 0x00C9C9A0 (vfx_particle_system_emitter.inl:381)
    const vector& get_acceleration_end(); // 0x00C9C9C0 (vfx_particle_system_emitter.inl:386)
    void set_acceleration_end(const vector& _acceleration_end); // 0x00C9C9D0 (vfx_particle_system_emitter.inl:391)
    const vector& get_acceleration_mid();
    void set_acceleration_mid(const vector&);
    bool get_use_accel_variance(); // 0x00C9C9F0 (vfx_particle_system_emitter.inl:409)
    void set_use_accel_variance(bool _use_accel_variance); // 0x00C9CA00 (vfx_particle_system_emitter.inl:414)
    const vector& get_acceleration_start_variance(); // 0x00C9CA10 (vfx_particle_system_emitter.inl:422)
    void set_acceleration_start_variance(const vector& _acceleration_start_variance); // 0x00C9CA20 (vfx_particle_system_emitter.inl:427)
    const vector& get_acceleration_end_variance(); // 0x00C9CA40 (vfx_particle_system_emitter.inl:432)
    void set_acceleration_end_variance(const vector& _acceleration_end_variance); // 0x00C9CA50 (vfx_particle_system_emitter.inl:437)
    const vector& get_acceleration_mid_variance();
    void set_acceleration_mid_variance(const vector&);
    void get_local_position(vector& result, float elapsed_time); // 0x00C9D040 (vfx_particle_system_emitter.inl:1021)
    void get_local_rotation(matrix& result, float elapsed_time); // 0x00C9D080 (vfx_particle_system_emitter.inl:1027)
    void get_local_scale(vector& result, float elapsed_time); // 0x00C9D120 (vfx_particle_system_emitter.inl:1040)
    void set_position_keys(vfx_vector_key* _position_keys, int _num_position_keys); // 0x00C9CA70 (vfx_particle_system_emitter.inl:461)
    vfx_vector_key* get_position_keys();
    void set_rotation_keys(vfx_quat_key* _rotation_keys, int _num_rotation_keys); // 0x00C9CA90 (vfx_particle_system_emitter.inl:473)
    vfx_quat_key* get_rotation_keys();
    void set_scale_keys(vfx_vector_key* _scale_keys, int _num_scale_keys); // 0x00C9CAC0 (vfx_particle_system_emitter.inl:488)
    vfx_vector_key* get_scale_keys();
    float get_scale() const;
    void set_scale(float scale); // 0x00CA30C0 (vfx_particle_system_emitter.inl:505)
    float get_scale_x() const;
    void set_scale_x(float scale_x); // 0x00C9CAE0 (vfx_particle_system_emitter.inl:518)
    float get_scale_y() const;
    void set_scale_y(float scale_y); // 0x00C9CB00 (vfx_particle_system_emitter.inl:530)
    float get_scale_z() const;
    void set_scale_z(float scale_z); // 0x00C9CB20 (vfx_particle_system_emitter.inl:542)
    float get_opacity_scale() const; // 0x005D1830 (vfx_particle_system_emitter.inl:549)
    void set_opacity_scale(float opacity_scale); // 0x00C9CB40 (vfx_particle_system_emitter.inl:554)
    float get_opacity_start() const;
    void set_opacity_start(float opacity_start); // 0x00C9CB60 (vfx_particle_system_emitter.inl:565)
    float get_opacity_mid_A() const;
    void set_opacity_mid_A(float opacity_mid_A); // 0x00C9CB70 (vfx_particle_system_emitter.inl:576)
    float get_opacity_mid_B() const;
    void set_opacity_mid_B(float opacity_mid_B); // 0x00C9CB90 (vfx_particle_system_emitter.inl:587)
    float get_opacity_end() const;
    void set_opacity_end(float opacity_end); // 0x00C9CBB0 (vfx_particle_system_emitter.inl:598)
    void set_emission_rate_keys(vfx_float_key* _emission_rate_keys, int _num_emission_rate_keys); // 0x00C9CBC0 (vfx_particle_system_emitter.inl:604)
    vfx_float_key* get_emission_rate_keys(); // 0x006E1860 (vfx_particle_system_emitter.inl:610)
    int get_num_emission_rate_keys() const; // 0x00C9CBE0 (vfx_particle_system_emitter.inl:615)
    void set_angular_speed_start_keys(vfx_float_key* _angular_speed_start_keys, int _num_angular_speed_start_keys); // 0x00C9CBF0 (vfx_particle_system_emitter.inl:620)
    vfx_float_key* get_angular_speed_start_keys(); // 0x00C9CC30 (vfx_particle_system_emitter.inl:633)
    void set_angular_speed_end_keys(vfx_float_key* _angular_speed_end_keys, int _num_angular_speed_end_keys); // 0x00C9CC10 (vfx_particle_system_emitter.inl:626)
    vfx_float_key* get_angular_speed_end_keys(); // 0x00C9CC40 (vfx_particle_system_emitter.inl:638)
    void set_speed_keys(vfx_float_key* _speed_keys, int _num_speed_keys); // 0x00C9CC50 (vfx_particle_system_emitter.inl:644)
    vfx_float_key* get_speed_keys(); // 0x00C9CC70 (vfx_particle_system_emitter.inl:650)
    void set_texture_anim_rate_keys(vfx_float_key* texture_anim_rate_keys, int num_texture_anim_rate_keys); // 0x00C9D1A0 (vfx_particle_system_emitter.inl:1070)
    vfx_float_key* get_texture_anim_rate_keys() const; // 0x00C9D1C0 (vfx_particle_system_emitter.inl:1076)
    void set_size_start_keys(vfx_float_key* _size_start_keys, int _num_size_start_keys); // 0x00C9CC80 (vfx_particle_system_emitter.inl:657)
    vfx_float_key* get_size_start_keys(); // 0x00C9CCA0 (vfx_particle_system_emitter.inl:663)
    void set_size_start_var_keys(vfx_float_key* _size_start_var_keys, int _num_size_start_var_keys); // 0x00C9CCB0 (vfx_particle_system_emitter.inl:668)
    vfx_float_key* get_size_start_var_keys();
    void set_size_mida_keys(vfx_float_key* _size_mida_keys, int _num_size_mida_keys); // 0x00C9CCD0 (vfx_particle_system_emitter.inl:679)
    vfx_float_key* get_size_mida_keys(); // 0x00C9CCF0 (vfx_particle_system_emitter.inl:685)
    void set_size_mida_var_keys(vfx_float_key* _size_mida_var_keys, int _num_size_mida_var_keys); // 0x00C9CD00 (vfx_particle_system_emitter.inl:690)
    vfx_float_key* get_size_mida_var_keys();
    void set_size_mida_offset_keys(vfx_float_key* _size_mida_offset_keys, int _num_size_mida_offset_keys); // 0x00C9CD20 (vfx_particle_system_emitter.inl:701)
    vfx_float_key* get_size_mida_offset_keys();
    void set_size_midb_keys(vfx_float_key* _size_midb_keys, int _num_size_midb_keys); // 0x00C9CD40 (vfx_particle_system_emitter.inl:712)
    vfx_float_key* get_size_midb_keys(); // 0x00C9CD60 (vfx_particle_system_emitter.inl:718)
    void set_size_midb_var_keys(vfx_float_key* _size_midb_var_keys, int _num_size_midb_var_keys); // 0x00C9CD70 (vfx_particle_system_emitter.inl:723)
    vfx_float_key* get_size_midb_var_keys();
    void set_size_midb_offset_keys(vfx_float_key* _size_midb_offset_keys, int _num_size_midb_offset_keys); // 0x00C9CD90 (vfx_particle_system_emitter.inl:734)
    vfx_float_key* get_size_midb_offset_keys();
    void set_size_end_keys(vfx_float_key* _size_end_keys, int _num_size_end_keys); // 0x00C9CDB0 (vfx_particle_system_emitter.inl:745)
    vfx_float_key* get_size_end_keys(); // 0x00C9CDD0 (vfx_particle_system_emitter.inl:751)
    void set_size_end_var_keys(vfx_float_key* _size_end_var_keys, int _num_size_end_var_keys); // 0x00C9CDE0 (vfx_particle_system_emitter.inl:756)
    vfx_float_key* get_size_end_var_keys();
    void set_size_start_variance(float size_start_variance); // 0x00C9CE00 (vfx_particle_system_emitter.inl:774)
    void set_size_end_variance(float size_end_variance); // 0x00C9CE20 (vfx_particle_system_emitter.inl:786)
    float get_size_mida_variance() const;
    void set_size_mida_variance(float size_mida_variance); // 0x00C9CE40 (vfx_particle_system_emitter.inl:798)
    float get_size_midb_variance() const;
    void set_size_midb_variance(float size_midb_variance); // 0x00C9CE60 (vfx_particle_system_emitter.inl:810)
    float get_size_scale() const;
    void set_size_scale(float size_scale); // 0x00C9C7F0 (vfx_particle_system_emitter.inl:142)
    float get_emission_rate_scale() const;
    void set_emission_rate_scale(float emission_rate_scale); // 0x00C9CFF0 (vfx_particle_system_emitter.inl:999)
    float get_lod_emission_distance() const; // 0x00C9D010 (vfx_particle_system_emitter.inl:1005)
    void set_lod_emission_distance(float lod_emission_distance); // 0x00C9D020 (vfx_particle_system_emitter.inl:1010)
    void set_length_start_keys(vfx_float_key* _length_start_keys, int _num_length_start_keys); // 0x00C9CE80 (vfx_particle_system_emitter.inl:820)
    vfx_float_key* get_length_start_keys();
    void set_length_start_var_keys(vfx_float_key* _length_start_var_keys, int _num_length_start_var_keys); // 0x00C9CEA0 (vfx_particle_system_emitter.inl:831)
    vfx_float_key* get_length_start_var_keys();
    void set_length_mida_keys(vfx_float_key* _length_mida_keys, int _num_length_mida_keys); // 0x00C9CEC0 (vfx_particle_system_emitter.inl:842)
    vfx_float_key* get_length_mida_keys();
    void set_length_mida_var_keys(vfx_float_key* _length_mida_var_keys, int _num_length_mida_var_keys); // 0x00C9CEE0 (vfx_particle_system_emitter.inl:853)
    vfx_float_key* get_length_mida_var_keys();
    void set_length_mida_offset_keys(vfx_float_key* _length_mida_offset_keys, int _num_length_mida_offset_keys); // 0x00C9CF00 (vfx_particle_system_emitter.inl:864)
    vfx_float_key* get_length_mida_offset_keys();
    void set_length_midb_keys(vfx_float_key* _length_midb_keys, int _num_length_midb_keys); // 0x00C9CF20 (vfx_particle_system_emitter.inl:875)
    vfx_float_key* get_length_midb_keys();
    void set_length_midb_var_keys(vfx_float_key* _length_midb_var_keys, int _num_length_midb_var_keys); // 0x00C9CF40 (vfx_particle_system_emitter.inl:886)
    vfx_float_key* get_length_midb_var_keys();
    void set_length_midb_offset_keys(vfx_float_key* _length_midb_offset_keys, int _num_length_midb_offset_keys); // 0x00C9CF60 (vfx_particle_system_emitter.inl:897)
    vfx_float_key* get_length_midb_offset_keys();
    void set_length_end_keys(vfx_float_key* _length_end_keys, int _num_length_end_keys); // 0x00C9CF80 (vfx_particle_system_emitter.inl:908)
    vfx_float_key* get_length_end_keys();
    void set_length_end_var_keys(vfx_float_key* _length_end_var_keys, int _num_length_end_var_keys); // 0x00C9CFA0 (vfx_particle_system_emitter.inl:919)
    vfx_float_key* get_length_end_var_keys();
    void set_length_start_variance(float);
    void set_length_end_variance(float);
    float get_length_mida_variance() const;
    void set_length_mida_variance(float);
    float get_length_midb_variance() const;
    void set_length_midb_variance(float);
    void set_animation_duration(float _animation_duration); // 0x00C9CFC0 (vfx_particle_system_emitter.inl:979)
    float get_animation_duration(); // 0x00C9CFE0 (vfx_particle_system_emitter.inl:984)
    rl_renderable_particles* get_renderable_particles() const; // 0x00C9D160 (vfx_particle_system_emitter.inl:1048)
    void set_renderable(rl_renderable_particles* renderable_p); // 0x00C9D170 (vfx_particle_system_emitter.inl:1053)
    bool get_inherit_velocity() const; // 0x00C9D180 (vfx_particle_system_emitter.inl:1059)
    void set_inherit_velocity(bool inherit_velocity); // 0x00C9D190 (vfx_particle_system_emitter.inl:1064)
    void set_emit_node_emitter_list(et_sized_native_pointer<vfx_particle_system_emitter>* emitters, int num_emitters); // 0x00CAF970 (:159)
    const base_array<et_sized_native_pointer<vfx_particle_system_emitter> >& get_emit_node_emitter_list() const; // 0x00C9D1D0 (vfx_particle_system_emitter.inl:1081)
protected:
    virtual int serialize(void* data_p); // vtbl+0x14 0x00C9F2F0 (:151)
private:
    void finalize(); // 0x00C9F0B0 (:127)
    // rfg's weapons.cpp (weapon_create_arc_effect) sets max_particles and lifetime (friends are not recorded in rfg.pdb).
    friend void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    friend void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    // rfg's tracers.cpp primes the tracer effect's emitters and moves its particles directly (friends are not recorded
    // in rfg.pdb).
    friend void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void tracers_process();
    friend void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos);
    // The emitter instance reads the curves and flags directly (its spawn and update code is the emitter's .inl
    // accessors inlined, with direct member reads in between; decompiled).
    friend class vfx_particle_system_emitter_instance;
    // vfx_effect (create_emitter, update_render) reads the weights, render type and parametric curves directly.
    friend class vfx_effect;
    // rfg's object_effect_profile::update reads is_iterative and iterations_per_sec.
    friend class object_effect_profile;

protected:
    bool is_iterative; // 0x08
    rl_particle_base::render_type m_particle_type; // 0x0C
    float iterations_per_sec; // 0x10
    int num_tails; // 0x14
    int max_tails; // 0x18
    bool use_length; // 0x1C
    bool use_length_mid_value; // 0x1D
    bool local_space_emission; // 0x1E
    bool use_weight_variance; // 0x1F
    float m_weight_start; // 0x20
    float m_weight_end; // 0x24
    float m_weight_scale; // 0x28
    bool use_size_mid_value; // 0x2C
    bool use_color_mid_value; // 0x2D
    bool use_color_saturation_value; // 0x2E
    bool use_additive_key_value; // 0x2F
    float density; // 0x30
    float density_variance; // 0x34
    bool use_density_variance; // 0x38
    rl_color_float color_start; // 0x3C
    rl_color_float color_start_variance; // 0x4C
    rl_color_float color_end; // 0x5C
    rl_color_float color_end_variance; // 0x6C
    rl_color_float color_mid_A; // 0x7C
    rl_color_float color_mid_A_variance; // 0x8C
    float color_mid_A_offset; // 0x9C
    rl_color_float color_mid_B; // 0xA0
    rl_color_float color_mid_B_variance; // 0xB0
    float color_mid_B_offset; // 0xC0
    float alpha_mid_A_offset; // 0xC4
    float alpha_mid_B_offset; // 0xC8
    float color_saturation_start; // 0xCC
    float color_saturation_start_variance; // 0xD0
    float color_saturation_end; // 0xD4
    float color_saturation_end_variance; // 0xD8
    float color_saturation_mid_A; // 0xDC
    float color_saturation_mid_A_variance; // 0xE0
    float color_saturation_mid_B; // 0xE4
    float color_saturation_mid_B_variance; // 0xE8
    vector acceleration_start; // 0xEC
    vector acceleration_start_variance; // 0xF8
    vector acceleration_mid; // 0x104
    vector acceleration_mid_variance; // 0x110
    vector acceleration_end; // 0x11C
    vector acceleration_end_variance; // 0x128
    float yaw_variance; // 0x134
    float pitch_variance; // 0x138
    float speed_variance; // 0x13C
    bool is_reverse_render; // 0x140
    bool use_accel_values; // 0x141
    bool use_accel_variance; // 0x142
    bool use_accel_mid_value; // 0x143
    bool use_color_variance; // 0x144
    bool is_color_variance_by_value; // 0x145
    float additive_key_start; // 0x148
    float additive_key_start_variance; // 0x14C
    float additive_key_end; // 0x150
    float additive_key_end_variance; // 0x154
    float additive_key_mid_A; // 0x158
    float additive_key_mid_A_variance; // 0x15C
    float additive_key_mid_B; // 0x160
    float additive_key_mid_B_variance; // 0x164
    bool is_rotation_enabled; // 0x168
    bool is_rotation_dir_dependant; // 0x169
    bool is_random_start_rotation; // 0x16A
    float angular_speed_start_variance; // 0x16C
    float angular_speed_end_variance; // 0x170
    float m_size_scale; // 0x174
    float m_emission_rate_scale; // 0x178
    float m_opacity_scale; // 0x17C
    float m_lod_emission_distance; // 0x180
    bool use_size_variance; // 0x184
    bool use_length_variance; // 0x185
    float bounce; // 0x188
    float bounce_variance; // 0x18C
    bool use_bounce_variance; // 0x190
    int max_emission; // 0x194
    bool use_lifetime_variance; // 0x198
    float lifetime; // 0x19C
    float lifetime_variance; // 0x1A0
    int max_particles; // 0x1A4
private:
    float animation_duration; // 0x1A8
    int num_position_keys; // 0x1AC
    vfx_vector_key* position_keys; // 0x1B0
    int num_rotation_keys; // 0x1B4
    vfx_quat_key* rotation_keys; // 0x1B8
    int num_scale_keys; // 0x1BC
    vfx_vector_key* scale_keys; // 0x1C0
    int num_emission_rate_keys; // 0x1C4
    vfx_float_key* emission_rate_keys; // 0x1C8
    int num_angular_speed_start_keys; // 0x1CC
    vfx_float_key* angular_speed_start_keys; // 0x1D0
    int num_angular_speed_end_keys; // 0x1D4
    vfx_float_key* angular_speed_end_keys; // 0x1D8
    int num_speed_keys; // 0x1DC
    vfx_float_key* speed_keys; // 0x1E0
    int m_num_texture_anim_rate_keys; // 0x1E4
    vfx_float_key* m_texture_anim_rate_keys; // 0x1E8
    int num_size_start_keys; // 0x1EC
    vfx_float_key* size_start_keys; // 0x1F0
    int num_size_start_var_keys; // 0x1F4
    vfx_float_key* size_start_var_keys; // 0x1F8
    int num_size_mida_keys; // 0x1FC
    vfx_float_key* size_mida_keys; // 0x200
    int num_size_mida_var_keys; // 0x204
    vfx_float_key* size_mida_var_keys; // 0x208
    int num_size_mida_offset_keys; // 0x20C
    vfx_float_key* size_mida_offset_keys; // 0x210
    int num_size_midb_keys; // 0x214
    vfx_float_key* size_midb_keys; // 0x218
    int num_size_midb_var_keys; // 0x21C
    vfx_float_key* size_midb_var_keys; // 0x220
    int num_size_midb_offset_keys; // 0x224
    vfx_float_key* size_midb_offset_keys; // 0x228
    int num_size_end_keys; // 0x22C
    vfx_float_key* size_end_keys; // 0x230
    int num_size_end_var_keys; // 0x234
    vfx_float_key* size_end_var_keys; // 0x238
    int num_length_start_keys; // 0x23C
    vfx_float_key* length_start_keys; // 0x240
    int num_length_start_var_keys; // 0x244
    vfx_float_key* length_start_var_keys; // 0x248
    int num_length_mida_keys; // 0x24C
    vfx_float_key* length_mida_keys; // 0x250
    int num_length_mida_var_keys; // 0x254
    vfx_float_key* length_mida_var_keys; // 0x258
    int num_length_mida_offset_keys; // 0x25C
    vfx_float_key* length_mida_offset_keys; // 0x260
    int num_length_midb_keys; // 0x264
    vfx_float_key* length_midb_keys; // 0x268
    int num_length_midb_var_keys; // 0x26C
    vfx_float_key* length_midb_var_keys; // 0x270
    int num_length_midb_offset_keys; // 0x274
    vfx_float_key* length_midb_offset_keys; // 0x278
    int num_length_end_keys; // 0x27C
    vfx_float_key* length_end_keys; // 0x280
    int num_length_end_var_keys; // 0x284
    vfx_float_key* length_end_var_keys; // 0x288
    matrix local_rotation_lazy; // 0x28C
    rl_renderable_particles* m_renderable_p; // 0x2B0
    bool m_inherit_velocity; // 0x2B4
    pool_list<et_sized_native_pointer<vfx_particle_system_emitter> > m_emit_node_emitter_list; // 0x2B8
};

#include "volition/vfxlib/code/vfx_particle_system_emitter.inl"
