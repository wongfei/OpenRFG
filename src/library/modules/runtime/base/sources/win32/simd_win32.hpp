// Original: library/modules/runtime/base/sources/win32/simd_win32.hpp
//
// Win32 SIMD wrappers (SSE) of the v*_f32 operations.
#pragma once

#include "keen/base/types.hpp"
#include <emmintrin.h>

namespace keen
{
    __m128i vcast_f32_to_i32(__m128 value); // 0x00CBB100 (.h:22)
    __m128 vcast_i32_to_f32(__m128i value); // 0x00CBB110 (.h:25)
    __m128 vcast_u32_to_f32(__m128i value); // 0x00C59F70 (.h:29)
    __m128i vcast_f32_to_u32(__m128 value); // 0x00C59F80 (.h:36)
    __m128 vset_f32(float value); // 0x00C59F90 (.h:56)
    __m128 vset_f32(float x, float y, float z, float w); // 0x00CBB120 (.h:57)
    __m128 vset_f32u(const float* pValues); // 0x004B1F40 (.h:60)
    __m128 vset_zero_f32(); // 0x00C59FA0 (.h:62)
    __m128 vset_one_f32(); // 0x00CBB150 (.h:63)
    __m128i vset_i32(int value); // 0x00456CF0 (.h:65)
    __m128i vset_u32(uint32 x, uint32 y, uint32 z, uint32 w); // 0x00C59FB0 (.h:73)
    __m128i vset_i16u(const short* pValues); // 0x00CBB160 (.h:82)
    void vget_f32(float* pValues, __m128 value); // 0x004B1F50 (.h:128)
    void vget_f32u(float* pValues, __m128 value); // 0x00C59FE0 (.h:130)
    float vget_f32(__m128 value, uint32 index); // 0x00CBB170 (.h:132)
    float vget_x_f32(__m128 value); // 0x00CBB190 (.h:133)
    float vget_y_f32(__m128 value); // 0x00CBB1B0 (.h:134)
    float vget_z_f32(__m128 value); // 0x00CBB1D0 (.h:135)
    float vget_w_f32(__m128 value); // 0x00CBB1F0 (.h:136)
    __m128 vsplat_x_f32(__m128 value); // 0x00C59FF0 (.h:168)
    __m128 vsplat_y_f32(__m128 value); // 0x00C5A000 (.h:169)
    __m128 vsplat_z_f32(__m128 value); // 0x00C5A010 (.h:170)
    __m128 vsplat_w_f32(__m128 value); // 0x00CBB210 (.h:171)
    __m128 vmergeh_f32(__m128 a, __m128 b); // 0x004B1F60 (.h:185)
    __m128 vmergel_f32(__m128 a, __m128 b); // 0x004B1F70 (.h:193)
    __m128 vselect_f32(__m128 a, __m128 b, __m128i mask); // 0x00C5A020 (.h:204)
    __m128 vand_f32(__m128 a, __m128 b); // 0x00C5A030 (.h:215)
    __m128 vxor_f32(__m128 a, __m128 b); // 0x00CBB220 (.h:218)
    __m128i vsra_i32(__m128i value, int shift); // 0x00CBB230 (.h:263)
    __m128 vadd_f32(__m128 a, __m128 b); // 0x00C5A040 (.h:312)
    __m128 vsub_f32(__m128 a, __m128 b); // 0x00CBB240 (.h:313)
    __m128 vmul_f32(__m128 a, __m128 b); // 0x00C5A050 (.h:314)
    __m128 vdiv_f32(__m128 a, __m128 b); // 0x00CBB250 (.h:315)
    __m128 vmadd_f32(__m128 a, __m128 b, __m128 c); // 0x00C5A060 (.h:317)
    __m128 vmsub_f32(__m128 a, __m128 b, __m128 c); // 0x00CBB260 (.h:319)
    __m128 vnmsub_f32(__m128 a, __m128 b, __m128 c); // 0x00CBB270 (.h:321)
    __m128 vrsqrt_f32(__m128 value); // 0x00C5A070 (.h:327)
    __m128 vdot4_f32(__m128 a, __m128 b); // 0x00C5A080 (.h:341)
    __m128 vconvert_i32_to_f32(__m128i value); // 0x00CBB280 (.h:416)
    __m128i vconvert_i16_to_i32_low(__m128i value); // 0x00CBB290 (.h:426)
    __m128i vconvert_i16_to_i32_hi(__m128i value); // 0x00CBB2A0 (.h:427)
}
