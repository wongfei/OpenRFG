// Original: volition/vfxlib/code/vfx_effect_expressions.cpp
//
// Effect expressions (VFX.md FX1): a template's expressions drive emitter values from game variables (vehicle speed,
// weapon charge, ...). Each variable is an index into Expression_globals (registered by name CRC on first use); each
// expression function, registered by name CRC at start-up, maps its variable from a user range to a value range and
// writes the result into the emitter ("additive" functions set the variance, "scale" functions set the value and scale
// the variance by a factor). vfx_effect::update_expressions runs them on the emitter instances. The parameters are
// [0] the variable, [1] [2] the user range, [3] [4] the value range and [5] the variance factor.

#include "volition/vfxlib/code/vfx_effect.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/hash_table.h"

#include <stdint.h>

typedef vfx_effect_template::expression_param expression_param;

// Registers an expression function by the CRC of its name (one static instance per function).
class expression_function_registrator
{
public:
    expression_function_registrator(const char* str_p, vfx_effect_template::expression_function function_p); // 0x00CB4D10 (:197)
};

// ---- file-static globals
// The game variables. Original quirk kept: Registered_variables holds 100 names and the count is not checked, so a
// 101st variable would index past the array.
static float Expression_globals[100]; // 0x03A23AD0
static int Expression_globals_count; // 0x03A23C8C

// ---- globals
data_hash_table<unsigned int, unsigned int, &uint_hash, 100, unsigned char> Registered_variables(false); // 0x016F0AF0
data_hash_table<unsigned int, vfx_effect_template::expression_function, &uint_hash, 500, unsigned short> Registered_expressions(false); // 0x03A23CB8

// 0x00C9EED0 (:34)
rl_color_float_specialized::rl_color_float_specialized()
{
}

// 0x00C9EEE0 (:37)
rl_color_float_specialized::rl_color_float_specialized(float input_red, float input_green, float input_blue, float input_alpha)
{
    red = input_red;
    green = input_green;
    blue = input_blue;
    alpha = input_alpha;
}

// 0x00C9EF10 (:40)
rl_color_float_specialized::rl_color_float_specialized(const rl_color_float& v)
{
    red = v.red;
    green = v.green;
    blue = v.blue;
    alpha = v.alpha;
}

// 0x00C9EF30 (:46)
rl_color_float_specialized rl_color_float_specialized::operator+(const rl_color_float_specialized& _v) const
{
    return rl_color_float_specialized(_v.red + red, _v.green + green, _v.blue + blue, _v.alpha + alpha);
}

// 0x00C9EFB0 (:51)
rl_color_float_specialized rl_color_float_specialized::operator-(const rl_color_float_specialized& _v) const
{
    return rl_color_float_specialized(red - _v.red, green - _v.green, blue - _v.blue, alpha - _v.alpha);
}

// A parameter's value (a float or a color in the vfx data).
static float expression_param_float(const expression_param* params, int index)
{
    return *(const float*)params[index].m_data_p_index.to_pointer();
}

static const rl_color_float& expression_param_color(const expression_param* params, int index)
{
    return *(const rl_color_float*)params[index].m_data_p_index.to_pointer();
}

// The Expression_globals index of a variable parameter (the template stored it in place of the data pointer).
static int expression_param_variable_index(const expression_param& param)
{
    return (int)(intptr_t)param.m_data_p_index.to_pointer();
}

// clerp<T> (0x00CAB2C0, 0x00CAB090; LTCG, inlined in every function): the variable clamped to the user range, as a
// 0..1 fraction of it.
static float vfx_expression_percent(const expression_param* params)
{
    const float user_val_min = expression_param_float(params, 1);
    const float user_val_max = expression_param_float(params, 2);
    float user_val = Expression_globals[expression_param_variable_index(params[0])];
    if (user_val - user_val_min < 0.0f)
    {
        user_val = user_val_min;
    }
    if (0.0f <= user_val - user_val_max)
    {
        user_val = user_val_max;
    }
    float percent = (user_val - user_val_min) / (user_val_max - user_val_min);
    if (percent - 0.0f < 0.0f)
    {
        percent = 0.0f;
    }
    if (0.0f <= percent - 1.0f)
    {
        percent = 1.0f;
    }
    return percent;
}

// The value range at the variable's fraction.
static float vfx_expression_clerp_float(const expression_param* params)
{
    const float vfx_val_min = expression_param_float(params, 3);
    return (expression_param_float(params, 4) - vfx_val_min) * vfx_expression_percent(params) + vfx_val_min;
}

static rl_color_float vfx_expression_clerp_color(const expression_param* params)
{
    const float percent = vfx_expression_percent(params);
    const rl_color_float& from = expression_param_color(params, 3);
    const rl_color_float& to = expression_param_color(params, 4);
    rl_color_float result;
    result.red = (to.red - from.red) * percent + from.red;
    result.green = (to.green - from.green) * percent + from.green;
    result.blue = (to.blue - from.blue) * percent + from.blue;
    result.alpha = (to.alpha - from.alpha) * percent + from.alpha;
    return result;
}

// A "scale" color: the variance is the value scaled per channel by the factor color.
static rl_color_float vfx_expression_scale_color(const expression_param* params, rl_color_float& variance)
{
    const rl_color_float value = vfx_expression_clerp_color(params);
    const rl_color_float& factor = expression_param_color(params, 5);
    variance.red = factor.red * value.red;
    variance.green = factor.green * value.green;
    variance.blue = factor.blue * value.blue;
    variance.alpha = factor.alpha * value.alpha;
    return value;
}

static vfx_particle_system_emitter* vfx_expression_emitter(void* object_p)
{
    return static_cast<vfx_particle_system_emitter_instance*>(object_p)->get_emitter();
}

// A one-key curve set to value (the first key is rewritten and the curve cut to it).
#define VFX_EXPRESSION_SET_KEYS(EMITTER, NAME, VALUE) \
    do \
    { \
        vfx_float_key* keys_p = (EMITTER)->get_##NAME##_keys(); \
        keys_p->value = (VALUE); \
        (EMITTER)->set_##NAME##_keys(keys_p, 1); \
    } while (0)

// 0x00CAB8F0 (:207)
static void clerp_additive_color_start(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_color_start_variance(vfx_expression_clerp_color(params));
}

// 0x00CABB00 (:207)
static void clerp_scale_color_start(void* object_p, expression_param* params, int num_params)
{
    rl_color_float variance;
    const rl_color_float value = vfx_expression_scale_color(params, variance);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_color_start_variance(variance);
    emitter_p->set_color_start(value);
}

// 0x00CABDB0 (:208)
static void clerp_additive_color_mid_A(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_color_mid_A_variance(vfx_expression_clerp_color(params));
}

// 0x00CABFC0 (:208)
static void clerp_scale_color_mid_A(void* object_p, expression_param* params, int num_params)
{
    rl_color_float variance;
    const rl_color_float value = vfx_expression_scale_color(params, variance);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_color_mid_A_variance(variance);
    emitter_p->set_color_mid_A(value);
}

// 0x00CAC280 (:209)
static void clerp_additive_color_mid_B(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_color_mid_B_variance(vfx_expression_clerp_color(params));
}

// 0x00CAC490 (:209)
static void clerp_scale_color_mid_B(void* object_p, expression_param* params, int num_params)
{
    rl_color_float variance;
    const rl_color_float value = vfx_expression_scale_color(params, variance);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_color_mid_B_variance(variance);
    emitter_p->set_color_mid_B(value);
}

// 0x00CAC750 (:210)
static void clerp_additive_color_end(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_color_end_variance(vfx_expression_clerp_color(params));
}

// 0x00CAC960 (:210)
static void clerp_scale_color_end(void* object_p, expression_param* params, int num_params)
{
    rl_color_float variance;
    const rl_color_float value = vfx_expression_scale_color(params, variance);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_color_end_variance(variance);
    emitter_p->set_color_end(value);
}

// 0x00CACC10 (:211)
static void clerp_opacity_scale(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_opacity_scale(vfx_expression_clerp_float(params));
}

// 0x00CACCF0 (:212)
static void clerp_opacity_start(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_opacity_start(vfx_expression_clerp_float(params));
}

// 0x00CACDD0 (:213)
static void clerp_opacity_mid_A(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_opacity_mid_A(vfx_expression_clerp_float(params));
}

// 0x00CACEB0 (:214)
static void clerp_opacity_mid_B(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_opacity_mid_B(vfx_expression_clerp_float(params));
}

// 0x00CACF90 (:215)
static void clerp_opacity_end(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_opacity_end(vfx_expression_clerp_float(params));
}

// 0x00CAD070 (:216)
static void clerp_additive_lifetime(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_lifetime_variance(vfx_expression_clerp_float(params));
}

// 0x00CAD150 (:216)
static void clerp_scale_lifetime(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_lifetime_variance(value * expression_param_float(params, 5));
    emitter_p->set_lifetime(value);
}

// 0x00CAD260 (:217)
static void clerp_additive_speed(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_speed_variance(vfx_expression_clerp_float(params));
}

// 0x00CAD340 (:217)
static void clerp_scale_speed(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_speed_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, speed, value);
}

// 0x00CAD460 (:218)
static void clerp_emission_rate(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    VFX_EXPRESSION_SET_KEYS(emitter_p, emission_rate, value);
}

// 0x00CAD550 (:219)
static void clerp_emission_rate_scale(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_emission_rate_scale(vfx_expression_clerp_float(params));
}

// 0x00CAD630 (:220)
static void clerp_size_scale(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_size_scale(vfx_expression_clerp_float(params));
}

// 0x00CAD710 (:221)
static void clerp_additive_size_start(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_size_start_variance(vfx_expression_clerp_float(params));
}

// 0x00CAD800 (:221)
static void clerp_scale_size_start(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_size_start_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, size_start, value);
}

// 0x00CAD930 (:222): set_size_mida_variance's key-count bug applies (vfx_particle_system_emitter.inl).
static void clerp_additive_size_mida(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_size_mida_variance(vfx_expression_clerp_float(params));
}

// 0x00CADA20 (:222)
static void clerp_scale_size_mida(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_size_mida_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, size_mida, value);
}

// 0x00CADB50 (:223)
static void clerp_additive_size_midb(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_size_midb_variance(vfx_expression_clerp_float(params));
}

// 0x00CADC40 (:223)
static void clerp_scale_size_midb(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_size_midb_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, size_midb, value);
}

// 0x00CADD70 (:224)
static void clerp_additive_size_end(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_size_end_variance(vfx_expression_clerp_float(params));
}

// 0x00CADE60 (:224)
static void clerp_scale_size_end(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_size_end_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, size_end, value);
}

// 0x00CADF90 (:225): the variance setter stores the absolute value.
static void clerp_additive_angular_speed_start(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_angular_speed_start_variance(vfx_expression_clerp_float(params));
}

// 0x00CAE080 (:225)
static void clerp_scale_angular_speed_start(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_angular_speed_start_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, angular_speed_start, value);
}

// 0x00CAE1B0 (:226)
static void clerp_additive_angular_speed_end(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_angular_speed_end_variance(vfx_expression_clerp_float(params));
}

// 0x00CAE2A0 (:226)
static void clerp_scale_angular_speed_end(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    emitter_p->set_angular_speed_end_variance(value * expression_param_float(params, 5));
    VFX_EXPRESSION_SET_KEYS(emitter_p, angular_speed_end, value);
}

// 0x00CAE3D0 (:227)
static void clerp_scale(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_scale(vfx_expression_clerp_float(params));
}

// 0x00CAE4D0 (:228)
static void clerp_scale_x(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_scale_x(vfx_expression_clerp_float(params));
}

// 0x00CAE5C0 (:229)
static void clerp_scale_y(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_scale_y(vfx_expression_clerp_float(params));
}

// 0x00CAE6B0 (:230)
static void clerp_scale_z(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_scale_z(vfx_expression_clerp_float(params));
}

// 0x00CAE7A0 (:231)
static void clerp_weight_start(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_weight_start(vfx_expression_clerp_float(params));
}

// 0x00CAE880 (:232)
static void clerp_weight_end(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_weight_end(vfx_expression_clerp_float(params));
}

// 0x00CAE960 (:233)
static void clerp_weight_scale(void* object_p, expression_param* params, int num_params)
{
    vfx_expression_emitter(object_p)->set_weight_scale(vfx_expression_clerp_float(params));
}

// 0x00CAEA40 (:234)
static void clerp_texture_anim_rate(void* object_p, expression_param* params, int num_params)
{
    const float value = vfx_expression_clerp_float(params);
    vfx_particle_system_emitter* emitter_p = vfx_expression_emitter(object_p);
    VFX_EXPRESSION_SET_KEYS(emitter_p, texture_anim_rate, value);
}

// 0x00CB4D10 (:197): a name already registered (or a full table) is ignored.
expression_function_registrator::expression_function_registrator(const char* str_p, vfx_effect_template::expression_function function_p)
{
    Registered_expressions.insert_key_data(crc_stri(str_p, 0), function_p);
}

// The registrations (dynamic initializers, in the order of the functions above; Registered_expressions is defined
// earlier in this file, so it is constructed first).
static expression_function_registrator EX_color_start_clerp_additive("clerp_additive_color_start", clerp_additive_color_start);
static expression_function_registrator EX_color_start_clerp_scale("clerp_scale_color_start", clerp_scale_color_start);
static expression_function_registrator EX_color_mid_A_clerp_additive("clerp_additive_color_mid_A", clerp_additive_color_mid_A);
static expression_function_registrator EX_color_mid_A_clerp_scale("clerp_scale_color_mid_A", clerp_scale_color_mid_A);
static expression_function_registrator EX_color_mid_B_clerp_additive("clerp_additive_color_mid_B", clerp_additive_color_mid_B);
static expression_function_registrator EX_color_mid_B_clerp_scale("clerp_scale_color_mid_B", clerp_scale_color_mid_B);
static expression_function_registrator EX_color_end_clerp_additive("clerp_additive_color_end", clerp_additive_color_end);
static expression_function_registrator EX_color_end_clerp_scale("clerp_scale_color_end", clerp_scale_color_end);
static expression_function_registrator EX_opacity_scale_clerp("clerp_opacity_scale", clerp_opacity_scale);
static expression_function_registrator EX_opacity_start_clerp("clerp_opacity_start", clerp_opacity_start);
static expression_function_registrator EX_opacity_mid_A_clerp("clerp_opacity_mid_A", clerp_opacity_mid_A);
static expression_function_registrator EX_opacity_mid_B_clerp("clerp_opacity_mid_B", clerp_opacity_mid_B);
static expression_function_registrator EX_opacity_end_clerp("clerp_opacity_end", clerp_opacity_end);
static expression_function_registrator EX_lifetime_clerp_additive("clerp_additive_lifetime", clerp_additive_lifetime);
static expression_function_registrator EX_lifetime_clerp_scale("clerp_scale_lifetime", clerp_scale_lifetime);
static expression_function_registrator EX_speed_clerp_additive("clerp_additive_speed", clerp_additive_speed);
static expression_function_registrator EX_speed_clerp_scale("clerp_scale_speed", clerp_scale_speed);
static expression_function_registrator EX_emission_rate_clerp("clerp_emission_rate", clerp_emission_rate);
static expression_function_registrator EX_emission_rate_scale_clerp("clerp_emission_rate_scale", clerp_emission_rate_scale);
static expression_function_registrator EX_size_scale_clerp("clerp_size_scale", clerp_size_scale);
static expression_function_registrator EX_size_start_clerp_additive("clerp_additive_size_start", clerp_additive_size_start);
static expression_function_registrator EX_size_start_clerp_scale("clerp_scale_size_start", clerp_scale_size_start);
static expression_function_registrator EX_size_mida_clerp_additive("clerp_additive_size_mida", clerp_additive_size_mida);
static expression_function_registrator EX_size_mida_clerp_scale("clerp_scale_size_mida", clerp_scale_size_mida);
static expression_function_registrator EX_size_midb_clerp_additive("clerp_additive_size_midb", clerp_additive_size_midb);
static expression_function_registrator EX_size_midb_clerp_scale("clerp_scale_size_midb", clerp_scale_size_midb);
static expression_function_registrator EX_size_end_clerp_additive("clerp_additive_size_end", clerp_additive_size_end);
static expression_function_registrator EX_size_end_clerp_scale("clerp_scale_size_end", clerp_scale_size_end);
static expression_function_registrator EX_angular_speed_start_clerp_additive("clerp_additive_angular_speed_start", clerp_additive_angular_speed_start);
static expression_function_registrator EX_angular_speed_start_clerp_scale("clerp_scale_angular_speed_start", clerp_scale_angular_speed_start);
static expression_function_registrator EX_angular_speed_end_clerp_additive("clerp_additive_angular_speed_end", clerp_additive_angular_speed_end);
static expression_function_registrator EX_angular_speed_end_clerp_scale("clerp_scale_angular_speed_end", clerp_scale_angular_speed_end);
static expression_function_registrator EX_scale_clerp("clerp_scale", clerp_scale);
static expression_function_registrator EX_scale_x_clerp("clerp_scale_x", clerp_scale_x);
static expression_function_registrator EX_scale_y_clerp("clerp_scale_y", clerp_scale_y);
static expression_function_registrator EX_scale_z_clerp("clerp_scale_z", clerp_scale_z);
static expression_function_registrator EX_weight_start_clerp("clerp_weight_start", clerp_weight_start);
static expression_function_registrator EX_weight_end_clerp("clerp_weight_end", clerp_weight_end);
static expression_function_registrator EX_weight_scale_clerp("clerp_weight_scale", clerp_weight_scale);
static expression_function_registrator EX_texture_anim_rate_clerp("clerp_texture_anim_rate", clerp_texture_anim_rate);

// 0x00CA44C0 (:238): sets the variable of the expression's first parameter (variable_index is not used).
void vfx_effect::set_variable_value(int expression_index, int variable_index, float variable_value)
{
    (void)variable_index;
    const vfx_effect_template::expression& expression_r = m_effect_template_p->m_vfx_data_p->expressions[expression_index];
    Expression_globals[expression_param_variable_index(expression_r.m_params.to_pointer()[0])] = variable_value;
}

// 0x00CAEB30 (:245): null when no function has that name.
vfx_effect_template::expression_function vfx_effect_template::get_expression_function(unsigned int function_name_crc)
{
    expression_function function_p = nullptr;
    if (Registered_expressions.get(function_name_crc, function_p))
    {
        return function_p;
    }
    return nullptr;
}

// 0x00CB4D50 (:252): a new name takes the next global.
int vfx_effect_template::get_expression_variable_index(unsigned int variable_name_crc)
{
    const int index = Expression_globals_count;
    unsigned int existing_index;
    if (!Registered_variables.get(variable_name_crc, existing_index))
    {
        Registered_variables.insert_key_data(variable_name_crc, (unsigned int)Expression_globals_count);
        Expression_globals_count++;
        return index;
    }
    return (int)existing_index;
}

// 0x00CB4DE0 (:267)
void vfx_expressions_set_global_variable(unsigned int variable_name_crc, float value)
{
    Expression_globals[vfx_effect_template::get_expression_variable_index(variable_name_crc)] = value;
}
