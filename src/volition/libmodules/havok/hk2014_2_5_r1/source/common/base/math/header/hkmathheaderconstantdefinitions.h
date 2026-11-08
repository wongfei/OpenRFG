// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheaderconstantdefinitions.h
//
// Hand-written (havok_impl.md HK0): the indices of g_vectorfConstants (hkmathheaderconstants.h). Values from rfg.pdb;
// the order must not change, algorithms index the table (hkpBoxShape::castRay takes HK_QUADREAL_1000 + axis).
#pragma once

enum hkVectorConstant
{
    HK_QUADREAL_BEGIN = 0,
    HK_QUADREAL_MINUS1 = 1,
    HK_QUADREAL_0 = 2,
    HK_QUADREAL_1 = 3,
    HK_QUADREAL_2 = 4,
    HK_QUADREAL_3 = 5,
    HK_QUADREAL_4 = 6,
    HK_QUADREAL_5 = 7,
    HK_QUADREAL_6 = 8,
    HK_QUADREAL_7 = 9,
    HK_QUADREAL_8 = 10,
    HK_QUADREAL_15 = 11,
    HK_QUADREAL_16 = 12,
    HK_QUADREAL_255 = 13,
    HK_QUADREAL_256 = 14,
    HK_QUADREAL_2_POW_23 = 15,
    HK_QUADREAL_INV_0 = 16,
    HK_QUADREAL_INV_1 = 17,
    HK_QUADREAL_INV_2 = 18,
    HK_QUADREAL_INV_3 = 19,
    HK_QUADREAL_INV_4 = 20,
    HK_QUADREAL_INV_5 = 21,
    HK_QUADREAL_INV_6 = 22,
    HK_QUADREAL_INV_7 = 23,
    HK_QUADREAL_INV_8 = 24,
    HK_QUADREAL_INV_15 = 25,
    HK_QUADREAL_INV_127 = 26,
    HK_QUADREAL_INV_226 = 27,
    HK_QUADREAL_INV_255 = 28,
    HK_QUADREAL_1000 = 29,
    HK_QUADREAL_0100 = 30,
    HK_QUADREAL_0010 = 31,
    HK_QUADREAL_0001 = 32,
    HK_QUADREAL_MAX = 33,
    HK_QUADREAL_HIGH = 34,
    HK_QUADREAL_EPS = 35,
    HK_QUADREAL_EPS_SQRD = 36,
    HK_QUADREAL_MIN = 37,
    HK_QUADREAL_MINUS_MAX = 38,
    HK_QUADREAL_MINUS_MIN = 39,
    HK_QUADREAL_m11m11 = 40,
    HK_QUADREAL_1m11m1 = 41,
    HK_QUADREAL_1010 = 42,
    HK_QUADREAL_1100 = 43,
    HK_QUADREAL_0011 = 44,
    HK_QUADREAL_1248 = 45,
    HK_QUADREAL_8421 = 46,
    HK_QUADREAL_PACK_HALF = 47,
    HK_QUADREAL_PACK16_UNIT_VEC = 48,
    HK_QUADREAL_UNPACK16_UNIT_VEC = 49,
    HK_QUADREAL_PI = 50,
    HK_QUADREAL_PI_HALF = 51,
    HK_QUADREAL_PI_QUARTER = 52,
    HK_QUADREAL_FOUR_PI_THIRD = 53,
    HK_QUADREAL_TWO_PI = 54,
    HK_QUADREAL_FOUR_PI = 55,
    HK_QUADREAL_DEG2RAD = 56,
    HK_QUADREAL_RAD2DEG = 57,
    HK_QUADREAL_END = 58,
};

// hkSimdFloat32 constants (Havok 2014.1 hkMathHeaderConstantDefinitions.h)
#define hkSimdFloat32_Minus1 (hkSimdFloat32::getConstant<HK_QUADREAL_MINUS1>())
#define hkSimdFloat32_0 (hkSimdFloat32::getConstant<HK_QUADREAL_0>())
#define hkSimdFloat32_1 (hkSimdFloat32::getConstant<HK_QUADREAL_1>())
#define hkSimdFloat32_2 (hkSimdFloat32::getConstant<HK_QUADREAL_2>())
#define hkSimdFloat32_3 (hkSimdFloat32::getConstant<HK_QUADREAL_3>())
#define hkSimdFloat32_4 (hkSimdFloat32::getConstant<HK_QUADREAL_4>())
#define hkSimdFloat32_5 (hkSimdFloat32::getConstant<HK_QUADREAL_5>())
#define hkSimdFloat32_6 (hkSimdFloat32::getConstant<HK_QUADREAL_6>())
#define hkSimdFloat32_7 (hkSimdFloat32::getConstant<HK_QUADREAL_7>())
#define hkSimdFloat32_8 (hkSimdFloat32::getConstant<HK_QUADREAL_8>())
#define hkSimdFloat32_15 (hkSimdFloat32::getConstant<HK_QUADREAL_15>())
#define hkSimdFloat32_16 (hkSimdFloat32::getConstant<HK_QUADREAL_16>())
#define hkSimdFloat32_255 (hkSimdFloat32::getConstant<HK_QUADREAL_255>())
#define hkSimdFloat32_256 (hkSimdFloat32::getConstant<HK_QUADREAL_256>())
#define hkSimdFloat32_Inv0 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_0>())
#define hkSimdFloat32_Inv1 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_1>())
#define hkSimdFloat32_Inv2 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>())
#define hkSimdFloat32_Inv3 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_3>())
#define hkSimdFloat32_Inv4 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_4>())
#define hkSimdFloat32_Inv5 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_5>())
#define hkSimdFloat32_Inv6 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_6>())
#define hkSimdFloat32_Inv7 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_7>())
#define hkSimdFloat32_Inv8 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_8>())
#define hkSimdFloat32_Inv_15 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_15>())
#define hkSimdFloat32_Inv_127 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_127>())
#define hkSimdFloat32_Inv_255 (hkSimdFloat32::getConstant<HK_QUADREAL_INV_255>())
#define hkSimdFloat32_High (hkSimdFloat32::getConstant<HK_QUADREAL_HIGH>())
#define hkSimdFloat32_Max (hkSimdFloat32::getConstant<HK_QUADREAL_MAX>())
#define hkSimdFloat32_Min (hkSimdFloat32::getConstant<HK_QUADREAL_MIN>())
#define hkSimdFloat32_Eps (hkSimdFloat32::getConstant<HK_QUADREAL_EPS>())
#define hkSimdFloat32_EpsSqrd (hkSimdFloat32::getConstant<HK_QUADREAL_EPS_SQRD>())
#define hkSimdFloat32_MinusMax (hkSimdFloat32::getConstant<HK_QUADREAL_MINUS_MAX>())
#define hkSimdFloat32_MinusMin (hkSimdFloat32::getConstant<HK_QUADREAL_MINUS_MIN>())
#define hkSimdFloat32_PackHalf (hkSimdFloat32::getConstant<HK_QUADREAL_PACK_HALF>())
#define hkSimdFloat32_Pack16 (hkSimdFloat32::getConstant<HK_QUADREAL_PACK16_UNIT_VEC>())
#define hkSimdFloat32_Unpack16 (hkSimdFloat32::getConstant<HK_QUADREAL_UNPACK16_UNIT_VEC>())
#define hkSimdFloat32_Pi (hkSimdFloat32::getConstant<HK_QUADREAL_PI>())
#define hkSimdFloat32_PiOver2 (hkSimdFloat32::getConstant<HK_QUADREAL_PI_HALF>())
#define hkSimdFloat32_PiOver4 (hkSimdFloat32::getConstant<HK_QUADREAL_PI_QUARTER>())
#define hkSimdFloat32_4PiOver3 (hkSimdFloat32::getConstant<HK_QUADREAL_FOUR_PI_THIRD>())
#define hkSimdFloat32_TwoPi (hkSimdFloat32::getConstant<HK_QUADREAL_TWO_PI>())
#define hkSimdFloat32_FourPi (hkSimdFloat32::getConstant<HK_QUADREAL_FOUR_PI>())
#define hkSimdFloat32_Deg2Rad (hkSimdFloat32::getConstant<HK_QUADREAL_DEG2RAD>())
#define hkSimdFloat32_Rad2Deg (hkSimdFloat32::getConstant<HK_QUADREAL_RAD2DEG>())
#define hkSimdFloat32_Half (hkSimdFloat32::getConstant<HK_QUADREAL_INV_2>())

// hkIntVector constants (Havok 2014.1 hkMathHeaderConstantDefinitions.h)
enum hkIntVectorConstant
{
    HK_QUADINT_BEGIN, ///< Start marker, do not use

    HK_QUADINT_0,
    HK_QUADINT_1,
    HK_QUADINT_2,
    HK_QUADINT_4,
    HK_QUADINT_0123,
    HK_QUADINT_0123_INT24W,    ///< lower bits hold 0123, upper bits are set, so that reinterpreting this value as float will give a valid floating point number
    HK_QUADINT_3,
    HK_QUADINT_FFFF,        ///< mask used for selecting the lower 16 bit

    // Permutation constants, for Xbox broadcast(i), do not change order!
    HK_QUADINT_PERM_XXXX,
    HK_QUADINT_PERM_YYYY,
    HK_QUADINT_PERM_ZZZZ,
    HK_QUADINT_PERM_WWWW,

    HK_QUADINT_END ///< End marker, do not use
};

// create a custom static constant in code (strict aliasing safe)

#define HK_DEFINE_VECTOR4f_HEX_CONSTANT(name,c0,c1,c2,c3) \
    static HK_ALIGN_FLOAT(const hkUint32 name##Cst[4]) = { c0, c1, c2, c3 }; \
    typedef union { hkUint32 i32[4]; hkFloat32 f32[4]; } name##i2f ; \
    const name##i2f* name = (const name##i2f*)name##Cst;
