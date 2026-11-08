// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderconstants.h
//
// Hand-written (havok_impl.md HK0): Havok's SIMD constant tables. Definitions in common/base/math/hkmath.cpp.
#pragma once

#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderconstantdefinitions.h"

extern const __m128 g_vectorfConstants[HK_QUADREAL_END]; // 0x016F2B20 (filled by a dynamic initializer, 0x010911C0)
extern __declspec(align(16)) const unsigned int g_intVectorConstants[HK_QUADINT_END][4]; // 0x014BE530
