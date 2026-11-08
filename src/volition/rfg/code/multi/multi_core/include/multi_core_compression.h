// Original: volition/rfg/code/multi/multi_core/include/multi_core_compression.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_compression.cpp
//
// Compression of network values: the bit packer, quantized floats, vectors and orientations, and the world/local
// conversions of positions relative to a moving system.
#pragma once

#include "volition/vlib/code/util/bitarray.h"

class matrix;
class matrix43;
class quaternion;
class vector;
class vector2;

// size 0x14
class bit_packer
{
public:
    bit_packer(unsigned char* data, unsigned int num_bytes); // 0x009441B0 (:64)
    void clear(); // 0x009441E0 (:72)
    void set_start_bit(unsigned int start_bit_pos); // 0x009441F0 (:81)
    unsigned int get_start_bit() const; // 0x00944200 (:91)
    void set_bits(unsigned int value, unsigned int start_bit, unsigned int num_bits); // 0x00944210 (:102)
    void set_next_bits(unsigned int value, unsigned int num_bits); // 0x00944260 (:119)
    void add_bool(bool value); // 0x009442C0 (:127)
    void add_uint8(unsigned char value); // 0x009442F0 (:135)
    void add_uint16(unsigned short value); // 0x00944330 (:151)
    void add_uint(unsigned int value); // 0x00944370 (:167)
    void add_int(int value); // 0x009443B0 (:172)
    void add_byte(unsigned char value); // 0x009443F0 (:177)
    void add_float(float value); // 0x00944430 (:182)
    void add_vector(const vector& value); // 0x00944470 (:201)
    void add_vector2(const vector2& value); // 0x00944500 (:211)
    unsigned int get_bits(unsigned int start_bit, unsigned int num_bits) const; // 0x00944570 (:228)
    unsigned int get_next_bits(unsigned int num_bits); // 0x009445B0 (:249)
    bool get_bool(); // 0x00944600 (:259)
    unsigned char get_uint8(); // 0x00944620 (:270)
    unsigned short get_uint16(); // 0x00944660 (:289)
    unsigned int get_uint(); // 0x009446A0 (:308)
    int get_int(); // 0x009446E0 (:313)
    unsigned char get_byte(); // 0x00944720 (:318)
    float get_float(); // 0x00944760 (:323)
    void get_vector(vector& vec); // 0x009447A0 (:348)
    void get_vector2(vector2& vec); // 0x009447F0 (:361)
    ~bit_packer(); // 0x006E2110

private:
    bit_array ba; // 0x00
    unsigned int _num_bits; // 0x0C
    unsigned int _start_bit; // 0x10
};

float exp_lerp_factor(float pct_per_sec, float step_time); // 0x00944840 (:398)
void convert_point_direction_from_world_to_local(vector& local_point, vector& local_direction, const vector& world_point, const vector& world_direction, const vector& world_system_origin, const matrix& world_system_orient); // 0x009448B0 (:412)
void convert_point_direction_from_local_to_world(vector& world_point, vector& world_direction, const vector& local_point, const vector& local_direction, const vector& world_system_pos, const matrix& world_system_orient); // 0x00944900 (:423)
void extrapolate_position(vector& extrapolated_pos, const vector& ref_pos, const vector& ref_linear_vel, unsigned int ref_time, unsigned int cur_time); // 0x00944950 (:432)
void estimate_velocity(vector& estimated_velocity, const vector& start_pos, const vector& end_pos, unsigned int ref_time, unsigned int cur_time); // 0x00972000 (:441)
unsigned int compress_float(float value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits); // 0x00944A50 (:467)
double decompress_double(unsigned int value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits); // 0x00944B90 (:520)
float decompress_float(unsigned int value, double min_val, double max_val, double bias_val, double bias_amount, int num_bits); // 0x00944CB0 (:572)
void convert_rectilinear_to_spherical_coords(float& sph_r, float& sph_theta, float& sph_phi, const vector& rect_vec); // 0x00944D00 (:586)
void convert_spherical_to_rectilinear_coords_double(vector& rect_vec, double pol_r, double pol_theta, double pol_phi); // 0x00944E10 (:616)
void convert_spherical_to_rectilinear_coords_float(vector& rect_vec, float pol_r, float pol_theta, float pol_phi); // 0x00944E60 (:641)
bool fl_is_equal_eps(const float f1, const float f2, float epsilon); // 0x00944EE0 (:657)
bool vec_is_equal_eps(const vector& vec1, const vector& vec2, float epsilon); // 0x00944F30 (:670)
bool vec2_is_equal_eps(const vector2& vec1, const vector2& vec2, float epsilon); // 0x00944FF0 (:685)
void mat_ph_get_dpdh(const matrix& mat1, const matrix& mat2, float& dp, float& dh); // 0x00945070 (:691)
bool mat_ph_is_equal_eps(float dp, float dh, float epsilon); // 0x00945100 (:703)
bool mat_ph_is_equal_eps(const matrix& mat1, const matrix& mat2, float epsilon); // 0x00945160 (:716)
bool mat_ph_is_equal_eps(const quaternion& q1, const quaternion& q2, float epsilon); // 0x00945230 (:732)
void delta_mat_get_pbh(const matrix& from, const matrix& to, float& p, float& b, float& h); // 0x00945270 (:748)
bool delta_mat_pbh_is_equal_eps(float p, float b, float h, float epsilon); // 0x009452B0 (:756)
bool delta_mat_pbh_is_equal_eps(const matrix& from, const matrix& to, float epsilon); // 0x00945330 (:774)
bool delta_mat_pbh_is_equal_eps(const quaternion& from_q, const quaternion& to_q, float epsilon); // 0x009453F0 (:796)
bool delta_mat43_pbh_is_equal_eps(const matrix43& from, const matrix43& to, float linear_eps, float euler_eps); // 0x009454E0 (:819)
bool mat43_ph_is_equal_eps(const matrix43& m1, const matrix43& m2, float linear_eps, float euler_eps); // 0x00945640 (:833)
void delta_compress(bit_packer& dest, float from, float to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945700 (:840)
float delta_decompress(bit_packer& src, float from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945780 (:848)
void vec_delta_compress(bit_packer& dest, const vector& from, const vector& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009457F0 (:856)
void vec_delta_decompress(bit_packer& src, vector& result, const vector& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945960 (:865)
void vec2_delta_compress(bit_packer& dest, const vector2& from, const vector2& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945AC0 (:874)
void vec2_delta_decompress(bit_packer& src, vector2& result, const vector2& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945BC0 (:882)
void mat_ph_delta_compress(bit_packer& dest, const matrix& from, const matrix& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945CB0 (:890)
void mat_ph_delta_decompress(bit_packer& src, matrix& result, const matrix& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00945F20 (:914)
void mat_pbh_delta_compress(bit_packer& dest, const matrix& from, const matrix& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946070 (:943)
void mat_pbh_delta_decompress(bit_packer& src, matrix& result, const matrix& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946180 (:963)
void mat_ph_delta_compress(bit_packer& dest, const quaternion& from, const quaternion& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946280 (:980)
void mat_ph_delta_decompress(bit_packer& src, quaternion& result, const quaternion& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946340 (:997)
void mat_pbh_delta_compress(bit_packer& dest, const quaternion& from, const quaternion& to, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009463F0 (:1016)
void mat_pbh_delta_decompress(bit_packer& src, quaternion& result, const quaternion& from, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009464B0 (:1034)
void vec_delta_compress_decompress(unsigned char* pack_buf, int buf_size, const vector& from, const vector& to, vector& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00972140 (:1051)
void vec_delta_compress_decompress(bit_packer& packer, const vector& from, const vector& to, vector& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946560 (:1064)
void vec2_delta_compress_decompress(unsigned char* pack_buf, int buf_size, const vector2& from, const vector2& to, vector2& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946600 (:1082)
void vec2_delta_compress_decompress(bit_packer& packer, const vector2& from, const vector2& to, vector2& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946780 (:1095)
void mat_ph_delta_compress_decompress(bit_packer& packer, const matrix& from, const matrix& to, matrix& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009468D0 (:1114)
void mat_pbh_delta_compress_decompress(bit_packer& packer, const matrix& from, const matrix& to, matrix& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946970 (:1133)
void mat_ph_delta_compress_decompress(bit_packer& packer, const quaternion& from, const quaternion& to, quaternion& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946A10 (:1152)
void mat_pbh_delta_compress_decompress(bit_packer& packer, const quaternion& from, const quaternion& to, quaternion& result, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946AB0 (:1171)
void pbh_compress(bit_packer& dest, float p, float b, float h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946B50 (:1197)
void pbh_compress(bit_packer& dest, const matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946C90 (:1208)
void pbh_compress(bit_packer& dest, const quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946D10 (:1216)
void pbh_decompress(bit_packer& src, float& p, float& b, float& h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946DA0 (:1226)
void pbh_decompress(bit_packer& src, matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946ED0 (:1237)
void pbh_decompress(bit_packer& src, quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946F50 (:1245)
void ph_compress(bit_packer& dest, float p, float h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00946FE0 (:1257)
void ph_compress(bit_packer& dest, const matrix& m, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009470C0 (:1266)
void ph_compress(bit_packer& dest, const quaternion& q, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x009471C0 (:1274)
void ph_decompress(bit_packer& src, float& p, float& h, int pack_size, float pack_amount, float pack_bias_value, float pack_bias); // 0x00947210 (:1284)
