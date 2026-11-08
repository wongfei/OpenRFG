// Original: volition/rfg/code/multi/multi_core/global/multi_core_compression.cpp
//
// Decompiled whole (multiplayer.md MP5; the world/local conversions and vec_is_equal_eps came with P6, the raw bit
// access of the bit packer with MP3).
//
// Floating point: rfg.exe evaluates float expressions in double and rounds to float at each store (SSE2 code with
// cvtps2pd/cvtpd2ps around every operation), and the biased curves of compress_float/decompress_double run on x87
// (53-bit precision) with the CRT's _CIpow. The port writes the double steps out where a rounding can differ from
// plain float arithmetic (sums and differences; a product of two floats is exact in double), and calls pow(), which
// may differ from _CIpow in the last bit of a double before the result is rounded to an integer or a float.
// Part of the multi unity TU (rfg_multi_unity.obj).

#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"

#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

#include <math.h>
#include <string.h>

// ---- bit_packer: values packed low bit first into a byte buffer

// 0x009441B0 (:64)
bit_packer::bit_packer(unsigned char* data, unsigned int num_bytes)
{
    _num_bits = num_bytes * 8;
    _start_bit = 0;
    ba.init(data, num_bytes * 8);
}

// 0x006E2110
bit_packer::~bit_packer()
{
}

// 0x009441E0 (:72)
void bit_packer::clear()
{
    ba.clear(false);
}

// 0x009441F0 (:81)
void bit_packer::set_start_bit(unsigned int start_bit_pos)
{
    _start_bit = start_bit_pos;
}

// 0x00944200 (:91)
unsigned int bit_packer::get_start_bit() const
{
    return _start_bit;
}

// 0x00944210 (:102): nothing is written past the end
void bit_packer::set_bits(unsigned int value, unsigned int start_bit, unsigned int num_bits)
{
    if (start_bit + num_bits > _num_bits)
    {
        return;
    }
    for (unsigned int i = 0; i < num_bits; i++)
    {
        ba.set(i + start_bit, value & 1);
        value >>= 1;
    }
}

// 0x00944260 (:119): past the end the position still advances
void bit_packer::set_next_bits(unsigned int value, unsigned int num_bits)
{
    const unsigned int start = _start_bit;
    if (start + num_bits <= _num_bits)
    {
        for (unsigned int i = 0; i < num_bits; i++)
        {
            ba.set(start + i, value & 1);
            value >>= 1;
        }
    }
    _start_bit += num_bits;
}

// The typed adders differ (original): add_bool, add_uint8, add_uint16 and add_float advance only when the value fits;
// add_uint, add_int, add_byte, add_vector and add_vector2 advance past the end too (set_next_bits).

// 0x009442C0 (:127)
void bit_packer::add_bool(bool value)
{
    if (_start_bit + 1 <= _num_bits)
    {
        ba.set(_start_bit, value);
        _start_bit++;
    }
}

// 0x009442F0 (:135)
void bit_packer::add_uint8(unsigned char value)
{
    if (_start_bit + 8 <= _num_bits)
    {
        for (unsigned int i = 0; i < 8; i++)
        {
            ba.set(_start_bit + i, value & 1);
            value >>= 1;
        }
        _start_bit += 8;
    }
}

// 0x00944330 (:151)
void bit_packer::add_uint16(unsigned short value)
{
    if (_start_bit + 16 <= _num_bits)
    {
        unsigned int bits = value;
        for (unsigned int i = 0; i < 16; i++)
        {
            ba.set(_start_bit + i, bits & 1);
            bits >>= 1;
        }
        _start_bit += 16;
    }
}

// 0x00944370 (:167)
void bit_packer::add_uint(unsigned int value)
{
    set_next_bits(value, 32);
}

// 0x009443B0 (:172)
void bit_packer::add_int(int value)
{
    set_next_bits((unsigned int)value, 32);
}

// 0x009443F0 (:177)
void bit_packer::add_byte(unsigned char value)
{
    set_next_bits(value, 8);
}

// 0x00944430 (:182): the float's bit pattern
void bit_packer::add_float(float value)
{
    if (_start_bit + 32 <= _num_bits)
    {
        unsigned int bits;
        memcpy(&bits, &value, sizeof(bits));
        for (unsigned int i = 0; i < 32; i++)
        {
            ba.set(_start_bit + i, bits & 1);
            bits >>= 1;
        }
        _start_bit += 32;
    }
}

// 0x00944470 (:201)
void bit_packer::add_vector(const vector& value)
{
    const float v[3] = {value.x, value.y, value.z};
    for (int i = 0; i < 3; i++)
    {
        unsigned int bits;
        memcpy(&bits, &v[i], sizeof(bits));
        set_next_bits(bits, 32);
    }
}

// 0x00944500 (:211)
void bit_packer::add_vector2(const vector2& value)
{
    const float v[2] = {value.x, value.y};
    for (int i = 0; i < 2; i++)
    {
        unsigned int bits;
        memcpy(&bits, &v[i], sizeof(bits));
        set_next_bits(bits, 32);
    }
}

// 0x00944570 (:228)
unsigned int bit_packer::get_bits(unsigned int start_bit, unsigned int num_bits) const
{
    unsigned int value = 0;
    for (unsigned int i = 0; i < num_bits; i++)
    {
        value |= (unsigned int)ba.get(i + start_bit) << (i & 0x1F);
    }
    return value;
}

// 0x009445B0 (:249)
unsigned int bit_packer::get_next_bits(unsigned int num_bits)
{
    const unsigned int start = _start_bit;
    unsigned int value = 0;
    for (unsigned int i = 0; i < num_bits; i++)
    {
        value |= (unsigned int)ba.get(i + start) << (i & 0x1F);
    }
    _start_bit += num_bits;
    return value;
}

// The getters read past the end without a check (bit_array::get is not bounded either).

// 0x00944600 (:259)
bool bit_packer::get_bool()
{
    const bool value = ba.get(_start_bit) != 0;
    _start_bit++;
    return value;
}

// 0x00944620 (:270)
unsigned char bit_packer::get_uint8()
{
    return (unsigned char)get_next_bits(8);
}

// 0x00944660 (:289)
unsigned short bit_packer::get_uint16()
{
    return (unsigned short)get_next_bits(16);
}

// 0x009446A0 (:308)
unsigned int bit_packer::get_uint()
{
    return get_next_bits(32);
}

// 0x009446E0 (:313)
int bit_packer::get_int()
{
    return (int)get_next_bits(32);
}

// 0x00944720 (:318)
unsigned char bit_packer::get_byte()
{
    return (unsigned char)get_next_bits(8);
}

// 0x00944760 (:323)
float bit_packer::get_float()
{
    const unsigned int bits = get_next_bits(32);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

// 0x009447A0 (:348)
void bit_packer::get_vector(vector& vec)
{
    float* v = &vec.x;
    for (int i = 0; i < 3; i++)
    {
        const unsigned int bits = get_next_bits(32);
        memcpy(&v[i], &bits, sizeof(float));
    }
}

// 0x009447F0 (:361)
void bit_packer::get_vector2(vector2& vec)
{
    float* v = &vec.x;
    for (int i = 0; i < 2; i++)
    {
        const unsigned int bits = get_next_bits(32);
        memcpy(&v[i], &bits, sizeof(float));
    }
}

// ---- helpers

// 0x00944840 (:398): 1 - (1 - pct)^step, pct clamped to [0, 1]; pow on x87 with float stores
float exp_lerp_factor(float pct_per_sec, float step_time)
{
    float pct = pct_per_sec;
    if ((float)((double)pct - 0.0) < 0.0f)
    {
        pct = 0.0f;
    }
    if ((float)((double)pct - 1.0) >= 0.0f)
    {
        pct = 1.0f;
    }
    const float keep = (float)(1.0 - (double)pct);
    const float remaining = (float)pow((double)keep, (double)step_time);
    return (float)(1.0 - (double)remaining);
}

// 0x009448B0 (:412)
void convert_point_direction_from_world_to_local(vector& local_point, vector& local_direction, const vector& world_point, const vector& world_direction, const vector& world_system_origin, const matrix& world_system_orient)
{
    local_point = world_point;
    vec_change_to_local_coords(local_point, world_system_origin, world_system_orient);
    local_direction = world_direction;
    local_direction.rotate(world_system_orient);
}

// 0x00944900 (:423)
void convert_point_direction_from_local_to_world(vector& world_point, vector& world_direction, const vector& local_point, const vector& local_direction, const vector& world_system_pos, const matrix& world_system_orient)
{
    world_point = local_point;
    vec_change_to_global_coords(world_point, world_system_pos, world_system_orient);
    world_direction = local_direction;
    world_direction.unrotate(world_system_orient);
}

// The elapsed network time in seconds (unsigned milliseconds), capped at 2.5 s (extrapolate_position and
// estimate_velocity inline it).
static float multi_elapsed_seconds_capped(unsigned int ref_time, unsigned int cur_time)
{
    float dt = (float)((double)(cur_time - ref_time) / 1000.0);
    if ((float)((double)dt - 2.5) >= 0.0f)
    {
        dt = 2.5f;
    }
    return dt;
}

// 0x00944950 (:432)
void extrapolate_position(vector& extrapolated_pos, const vector& ref_pos, const vector& ref_linear_vel, unsigned int ref_time, unsigned int cur_time)
{
    const float dt = multi_elapsed_seconds_capped(ref_time, cur_time);
    extrapolated_pos.x = (float)((double)ref_pos.x + (double)(ref_linear_vel.x * dt));
    extrapolated_pos.y = (float)((double)ref_pos.y + (double)(ref_linear_vel.y * dt));
    extrapolated_pos.z = (float)((double)ref_pos.z + (double)(ref_linear_vel.z * dt));
}

// 0x00972000 (:441): zero when no time passed
void estimate_velocity(vector& estimated_velocity, const vector& start_pos, const vector& end_pos, unsigned int ref_time, unsigned int cur_time)
{
    estimated_velocity = ZERO_VECTOR;
    const float dt = multi_elapsed_seconds_capped(ref_time, cur_time);
    if (fabsf(dt - 0.0f) >= 0.0001f)
    {
        estimated_velocity.x = (float)((double)(float)((double)end_pos.x - (double)start_pos.x) / (double)dt);
        estimated_velocity.y = (float)((double)(float)((double)end_pos.y - (double)start_pos.y) / (double)dt);
        estimated_velocity.z = (float)((double)(float)((double)end_pos.z - (double)start_pos.z) / (double)dt);
    }
}

// 0x00944A50 (:467)
// The value is clamped to [min_val, max_val] (in float), mapped to [-1, 1] around bias_val (linear when bias_amount
// is 1, else through pow(x, 1 / bias_amount) on x87), scaled to num_bits and rounded. Nothing clamps the low end: a
// value under min_val cannot happen after the clamp.
unsigned int compress_float(float value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits)
{
    const float min_f = (float)min_val;
    if ((float)((double)value - (double)min_f) < 0.0f)
    {
        value = min_f;
    }
    const float max_f = (float)max_val;
    float clamped = max_f;
    if ((float)((double)value - (double)max_f) < 0.0f)
    {
        clamped = value;
    }

    double t;
    if (bias_amount == 1.0)
    {
        if ((double)clamped > bias_val)
        {
            t = ((double)clamped - bias_val) / (max_val - bias_val);
        }
        else
        {
            // Original: negated twice (two xorpd with the sign mask).
            t = -(-(((double)clamped - bias_val) / (bias_val - min_val)));
        }
    }
    else if ((double)clamped > bias_val)
    {
        t = pow(((double)clamped - bias_val) / (max_val - bias_val), 1.0 / bias_amount);
    }
    else
    {
        t = -pow(-(((double)clamped - bias_val) / (bias_val - min_val)), 1.0 / bias_amount);
    }

    double scaled = (double)(1 << (num_bits - 1)) * (t + 1.0);
    const double max_value = (double)((1 << num_bits) - 1);
    if (scaled > max_value)
    {
        scaled = max_value;
    }
    return (unsigned int)fl_round((float)scaled);
}

// 0x00944B90 (:520): the inverse of compress_float
double decompress_double(unsigned int value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits)
{
    const double t = (double)value / (double)(1 << (num_bits - 1)) - 1.0;
    if (bias_amount == 1.0)
    {
        if (t < 0.0)
        {
            return (t + 1.0) * bias_val + -(t * min_val);
        }
        return (1.0 - t) * bias_val + t * max_val;
    }
    if (t < 0.0)
    {
        const double p = -pow(-t, bias_amount);
        return (1.0 + p) * bias_val + -(p * min_val);
    }
    const double p = pow(t, bias_amount);
    return (1.0 - p) * bias_val + p * max_val;
}

// 0x00944CB0 (:572)
// LTCG: num_bits in ECX (the port has no register convention).
float decompress_float(unsigned int value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits)
{
    return (float)decompress_double(value, min_val, max_val, bias_val, bias_amount, num_bits);
}

// 0x00944D00 (:586)
void convert_rectilinear_to_spherical_coords(float& sph_r, float& sph_theta, float& sph_phi, const vector& rect_vec)
{
    // the squares summed in double, rounded to float, then the root
    const float r2 = (float)((double)rect_vec.y * rect_vec.y + (double)rect_vec.x * rect_vec.x + (double)rect_vec.z * rect_vec.z);
    sph_r = (float)sqrt((double)r2);
    if (sph_r == 0.0f)
    {
        sph_theta = 0.0f;
        sph_phi = 0.0f;
        return;
    }
    sph_theta = (float)atan2((double)rect_vec.y, (double)rect_vec.x);
    float cos_phi = rect_vec.z / sph_r;
    if (cos_phi - -1.0f < 0.0f)
    {
        cos_phi = -1.0f;
    }
    if (cos_phi - 1.0f >= 0.0f)
    {
        cos_phi = 1.0f;
    }
    sph_phi = fl_acos(cos_phi);
}

// 0x00944E10 (:616)
void convert_spherical_to_rectilinear_coords_double(vector& rect_vec, double pol_r, double pol_theta, double pol_phi)
{
    const double planar = sin(pol_phi) * pol_r;
    rect_vec.x = (float)(cos(pol_theta) * planar);
    rect_vec.y = (float)(sin(pol_theta) * planar);
    rect_vec.z = (float)(cos(pol_phi) * pol_r);
}

// 0x00944E60 (:641)
void convert_spherical_to_rectilinear_coords_float(vector& rect_vec, float pol_r, float pol_theta, float pol_phi)
{
    const float sin_phi = (float)sin((double)pol_phi);
    rect_vec.x = (float)cos((double)pol_theta) * sin_phi * pol_r;
    rect_vec.y = (float)sin((double)pol_theta) * sin_phi * pol_r;
    rect_vec.z = (float)cos((double)pol_phi) * pol_r;
}

// 0x00944EE0 (:657)
bool fl_is_equal_eps(const float f1, const float f2, float epsilon)
{
    return fabsf(f1 - f2) < epsilon;
}

// 0x00944F30 (:670)
// Each component within epsilon.
bool vec_is_equal_eps(const vector& vec1, const vector& vec2, float epsilon)
{
    return fabsf(vec1.x - vec2.x) <= epsilon && fabsf(vec1.y - vec2.y) <= epsilon && fabsf(vec1.z - vec2.z) <= epsilon;
}

// 0x00944FF0 (:685)
bool vec2_is_equal_eps(const vector2& vec1, const vector2& vec2, float epsilon)
{
    return fabsf(vec1.x - vec2.x) <= epsilon && fabsf(vec1.y - vec2.y) <= epsilon;
}

// 0x00945070 (:691)
void mat_ph_get_dpdh(const matrix& mat1, const matrix& mat2, float& dp, float& dh)
{
    float p1;
    float h1;
    float p2;
    float h2;
    mat1.extract_pitch(p1);
    mat1.extract_heading(h1);
    mat2.extract_pitch(p2);
    mat2.extract_heading(h2);
    dp = (float)((double)p1 - (double)p2);
    dh = fl_angle_between_radians(h1, h2);
}

// 0x00945100 (:703)
bool mat_ph_is_equal_eps(float dp, float dh, float epsilon)
{
    return fabsf(dh) < epsilon && fabsf(dp) < epsilon;
}

// 0x00945160 (:716)
bool mat_ph_is_equal_eps(const matrix& mat1, const matrix& mat2, float epsilon)
{
    float dp;
    float dh;
    mat_ph_get_dpdh(mat1, mat2, dp, dh);
    return mat_ph_is_equal_eps(dp, dh, epsilon);
}

// 0x00945230 (:732)
bool mat_ph_is_equal_eps(const quaternion& q1, const quaternion& q2, float epsilon)
{
    matrix m1;
    matrix m2;
    q1.extract_matrix(m1);
    q2.extract_matrix(m2);
    return mat_ph_is_equal_eps(m1, m2, epsilon);
}

// 0x00945270 (:748): the angles of to * transpose(from)
void delta_mat_get_pbh(const matrix& from, const matrix& to, float& p, float& b, float& h)
{
    const matrix delta = to * from.copy_transpose();
    delta.extract_angles(p, b, h);
}

// 0x009452B0 (:756)
bool delta_mat_pbh_is_equal_eps(float p, float b, float h, float epsilon)
{
    return fabsf(p) < epsilon && fabsf(b) < epsilon && fabsf(h) < epsilon;
}

// 0x00945330 (:774)
bool delta_mat_pbh_is_equal_eps(const matrix& from, const matrix& to, float epsilon)
{
    float p;
    float b;
    float h;
    delta_mat_get_pbh(from, to, p, b, h);
    return delta_mat_pbh_is_equal_eps(p, b, h, epsilon);
}

// 0x009453F0 (:796)
bool delta_mat_pbh_is_equal_eps(const quaternion& from_q, const quaternion& to_q, float epsilon)
{
    matrix from;
    matrix to;
    from_q.extract_matrix(from);
    to_q.extract_matrix(to);
    return delta_mat_pbh_is_equal_eps(from, to, epsilon);
}

// 0x009454E0 (:819)
bool delta_mat43_pbh_is_equal_eps(const matrix43& from, const matrix43& to, float linear_eps, float euler_eps)
{
    return vec_is_equal_eps(from.m_translation, to.m_translation, linear_eps) && delta_mat_pbh_is_equal_eps(from.m_rotation, to.m_rotation, euler_eps);
}

// 0x00945640 (:833)
bool mat43_ph_is_equal_eps(const matrix43& m1, const matrix43& m2, float linear_eps, float euler_eps)
{
    return vec_is_equal_eps(m1.m_translation, m2.m_translation, linear_eps) && mat_ph_is_equal_eps(m1.m_rotation, m2.m_rotation, euler_eps);
}

// ---- delta compression: the difference to a reference, compressed symmetrically in [-pack_amount, pack_amount]

// 0x00945700 (:840)
void delta_compress(bit_packer& dest, float from, float to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    dest.set_next_bits(compress_float((float)((double)to - (double)from), -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size), pack_size);
}

// 0x00945780 (:848)
float delta_decompress(bit_packer& src, float from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int packed = src.get_next_bits(pack_size);
    return (float)((double)(float)decompress_double(packed, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size) + (double)from);
}

// 0x009457F0 (:856)
void vec_delta_compress(bit_packer& dest, const vector& from, const vector& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    delta_compress(dest, from.x, to.x, pack_size, pack_amount, pack_bias_value, pack_bias);
    delta_compress(dest, from.y, to.y, pack_size, pack_amount, pack_bias_value, pack_bias);
    delta_compress(dest, from.z, to.z, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00945960 (:865)
void vec_delta_decompress(bit_packer& src, vector& result, const vector& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    result.x = delta_decompress(src, from.x, pack_size, pack_amount, pack_bias_value, pack_bias);
    result.y = delta_decompress(src, from.y, pack_size, pack_amount, pack_bias_value, pack_bias);
    result.z = delta_decompress(src, from.z, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00945AC0 (:874)
void vec2_delta_compress(bit_packer& dest, const vector2& from, const vector2& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    delta_compress(dest, from.x, to.x, pack_size, pack_amount, pack_bias_value, pack_bias);
    delta_compress(dest, from.y, to.y, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00945BC0 (:882)
void vec2_delta_decompress(bit_packer& src, vector2& result, const vector2& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    result.x = delta_decompress(src, from.x, pack_size, pack_amount, pack_bias_value, pack_bias);
    result.y = delta_decompress(src, from.y, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00945CB0 (:890)
// Pitch and heading only (the bank is dropped): from the identity the angles themselves, else the pitch difference and
// the heading difference wrapped into [-pi, pi).
void mat_ph_delta_compress(bit_packer& dest, const matrix& from, const matrix& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    float to_h;
    float to_p;
    to.extract_heading(to_h);
    to.extract_pitch(to_p);
    vector2 delta;
    if (&from == &IDENTITY_MATRIX)
    {
        delta.x = to_p;
        delta.y = to_h;
    }
    else
    {
        float from_h;
        float from_p;
        from.extract_heading(from_h);
        from.extract_pitch(from_p);
        delta.x = (float)((double)to_p - (double)from_p);
        delta.y = fl_angle_cap_radians((float)((double)to_h - (double)from_h), -3.1415927f);
    }
    vec2_delta_compress(dest, ZERO_VECTOR2, delta, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00945F20 (:914): the result has the reference's bank (none from the identity)
void mat_ph_delta_decompress(bit_packer& src, matrix& result, const matrix& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    vector2 ph;
    float p;
    float b;
    float h;
    if (&from == &IDENTITY_MATRIX)
    {
        vec2_delta_decompress(src, ph, ZERO_VECTOR2, pack_size, pack_amount, pack_bias_value, pack_bias);
        p = ph.x;
        h = ph.y;
        b = 0.0f;
    }
    else
    {
        from.extract_angles(p, b, h);
        const vector2 ref(p, h);
        vec2_delta_decompress(src, ph, ref, pack_size, pack_amount, pack_bias_value, pack_bias);
        p = ph.x;
        h = fl_angle_cap_radians(ph.y, -3.1415927f);
    }
    result.set(p, b, h);
}

// 0x00946070 (:943): the angles of to * transpose(from)
void mat_pbh_delta_compress(bit_packer& dest, const matrix& from, const matrix& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix delta;
    if (&from == &IDENTITY_MATRIX)
    {
        delta = to;
    }
    else
    {
        delta = to * from.copy_transpose();
    }
    vector pbh;
    delta.extract_angles(pbh.x, pbh.y, pbh.z);
    vec_delta_compress(dest, ZERO_VECTOR, pbh, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946180 (:963)
void mat_pbh_delta_decompress(bit_packer& src, matrix& result, const matrix& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    vector pbh;
    vec_delta_decompress(src, pbh, ZERO_VECTOR, pack_size, pack_amount, pack_bias_value, pack_bias);
    const matrix delta(pbh.x, pbh.y, pbh.z);
    if (&from == &IDENTITY_MATRIX)
    {
        result = delta;
        return;
    }
    result = delta * from;
}

// 0x00946280 (:980)
void mat_ph_delta_compress(bit_packer& dest, const quaternion& from, const quaternion& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix to_m;
    to.extract_matrix(to_m);
    if (&from == &IDENTITY_QUATERNION)
    {
        mat_ph_delta_compress(dest, IDENTITY_MATRIX, to_m, pack_size, pack_amount, pack_bias_value, pack_bias);
        return;
    }
    matrix from_m;
    from.extract_matrix(from_m);
    mat_ph_delta_compress(dest, from_m, to_m, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946340 (:997)
void mat_ph_delta_decompress(bit_packer& src, quaternion& result, const quaternion& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix from_m;
    const matrix* ref = &IDENTITY_MATRIX;
    if (&from != &IDENTITY_QUATERNION)
    {
        from.extract_matrix(from_m);
        ref = &from_m;
    }
    matrix result_m;
    mat_ph_delta_decompress(src, result_m, *ref, pack_size, pack_amount, pack_bias_value, pack_bias);
    result.set(result_m);
}

// 0x009463F0 (:1016)
void mat_pbh_delta_compress(bit_packer& dest, const quaternion& from, const quaternion& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix to_m;
    to.extract_matrix(to_m);
    if (&from == &IDENTITY_QUATERNION)
    {
        mat_pbh_delta_compress(dest, IDENTITY_MATRIX, to_m, pack_size, pack_amount, pack_bias_value, pack_bias);
        return;
    }
    matrix from_m;
    from.extract_matrix(from_m);
    mat_pbh_delta_compress(dest, from_m, to_m, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x009464B0 (:1034)
void mat_pbh_delta_decompress(bit_packer& src, quaternion& result, const quaternion& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix from_m;
    const matrix* ref = &IDENTITY_MATRIX;
    if (&from != &IDENTITY_QUATERNION)
    {
        from.extract_matrix(from_m);
        ref = &from_m;
    }
    matrix result_m;
    mat_pbh_delta_decompress(src, result_m, *ref, pack_size, pack_amount, pack_bias_value, pack_bias);
    result.set(result_m);
}

// ---- compress, then decompress what was packed: the value the receiver will see

// 0x00972140 (:1051)
void vec_delta_compress_decompress(unsigned char* pack_buf, int buf_size, const vector& from, const vector& to, vector& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    bit_packer packer(pack_buf, buf_size);
    vec_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(0);
    vec_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946560 (:1064): the packer's position is left after the compressed value
void vec_delta_compress_decompress(bit_packer& packer, const vector& from, const vector& to, vector& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    vec_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    vec_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// 0x00946600 (:1082)
void vec2_delta_compress_decompress(unsigned char* pack_buf, int buf_size, const vector2& from, const vector2& to, vector2& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    bit_packer packer(pack_buf, buf_size);
    vec2_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(0);
    vec2_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946780 (:1095)
void vec2_delta_compress_decompress(bit_packer& packer, const vector2& from, const vector2& to, vector2& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    vec2_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    vec2_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// 0x009468D0 (:1114)
void mat_ph_delta_compress_decompress(bit_packer& packer, const matrix& from, const matrix& to, matrix& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    mat_ph_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    mat_ph_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// 0x00946970 (:1133)
void mat_pbh_delta_compress_decompress(bit_packer& packer, const matrix& from, const matrix& to, matrix& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    mat_pbh_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    mat_pbh_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// 0x00946A10 (:1152)
void mat_ph_delta_compress_decompress(bit_packer& packer, const quaternion& from, const quaternion& to, quaternion& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    mat_ph_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    mat_ph_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// 0x00946AB0 (:1171)
void mat_pbh_delta_compress_decompress(bit_packer& packer, const quaternion& from, const quaternion& to, quaternion& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int start = packer.get_start_bit();
    mat_pbh_delta_compress(packer, from, to, pack_size, pack_amount, pack_bias_value, pack_bias);
    const unsigned int end = packer.get_start_bit();
    packer.set_start_bit(start);
    mat_pbh_delta_decompress(packer, result, from, pack_size, pack_amount, pack_bias_value, pack_bias);
    packer.set_start_bit(end);
}

// ---- absolute angles

// 0x00946B50 (:1197): all three compressed, then all three written
void pbh_compress(bit_packer& dest, float p, float b, float h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int packed_p = compress_float(p, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    const unsigned int packed_b = compress_float(b, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    const unsigned int packed_h = compress_float(h, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    dest.set_next_bits(packed_p, pack_size);
    dest.set_next_bits(packed_b, pack_size);
    dest.set_next_bits(packed_h, pack_size);
}

// 0x00946C90 (:1208)
void pbh_compress(bit_packer& dest, const matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    float p;
    float b;
    float h;
    m.extract_angles(p, b, h);
    pbh_compress(dest, p, b, h, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946D10 (:1216)
void pbh_compress(bit_packer& dest, const quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix m;
    q.extract_matrix(m);
    pbh_compress(dest, m, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00946DA0 (:1226)
void pbh_decompress(bit_packer& src, float& p, float& b, float& h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int packed_p = src.get_next_bits(pack_size);
    const unsigned int packed_b = src.get_next_bits(pack_size);
    const unsigned int packed_h = src.get_next_bits(pack_size);
    p = (float)decompress_double(packed_p, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    b = (float)decompress_double(packed_b, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    h = (float)decompress_double(packed_h, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
}

// 0x00946ED0 (:1237)
void pbh_decompress(bit_packer& src, matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    float p;
    float b;
    float h;
    pbh_decompress(src, p, b, h, pack_size, pack_amount, pack_bias_value, pack_bias);
    m.set(p, b, h);
}

// 0x00946F50 (:1245)
void pbh_decompress(bit_packer& src, quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix m;
    pbh_decompress(src, m, pack_size, pack_amount, pack_bias_value, pack_bias);
    q.set(m);
}

// 0x00946FE0 (:1257)
void ph_compress(bit_packer& dest, float p, float h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int packed_p = compress_float(p, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    const unsigned int packed_h = compress_float(h, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    dest.set_next_bits(packed_p, pack_size);
    dest.set_next_bits(packed_h, pack_size);
}

// 0x009470C0 (:1266): the bank is dropped
void ph_compress(bit_packer& dest, const matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    float p;
    float b;
    float h;
    m.extract_angles(p, b, h);
    ph_compress(dest, p, h, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x009471C0 (:1274)
void ph_compress(bit_packer& dest, const quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    matrix m;
    q.extract_matrix(m);
    ph_compress(dest, m, pack_size, pack_amount, pack_bias_value, pack_bias);
}

// 0x00947210 (:1284)
void ph_decompress(bit_packer& src, float& p, float& h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias)
{
    const unsigned int packed_p = src.get_next_bits(pack_size);
    const unsigned int packed_h = src.get_next_bits(pack_size);
    p = (float)decompress_double(packed_p, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
    h = (float)decompress_double(packed_h, -pack_amount, pack_amount, pack_bias_value, pack_bias, pack_size);
}
