// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/header/hkmathheadertypes.h
//
// Hand-written (havok_impl.md HK2): what Havok's math headers take from hkBaseDefs.h, hkBaseTypes.h, hkError.h,
// hkConfigSimd.h and Types/Sse/hkSseMathTypes.h (Havok 2014.1), for the release Win32 SSE2 build rfg.exe ships:
// asserts compile to nothing, HK_SSE_VERSION is 0x20.
#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// ---- hkConfigSimd.h: rfg.exe uses SSE2 (no SSE3/SSE4 instruction in the Havok code)
#define HK_SSE_VERSION 0x20
#define HK_COMPILER_HAS_INTRINSICS_IA32
#define HK_PLATFORM_WIN32
#define HK_COMPILER_MSVC
#define HK_ARCH_IA32
#define HK_REAL_IS_FLOAT
#define HK_CONFIG_SIMD_ENABLED 1
#define HK_CONFIG_SIMD HK_CONFIG_SIMD_ENABLED
#define HK_STD_NAMESPACE // hkcmath.h: the C library functions at global scope

// ---- hkBaseDefs.h
#define HK_FORCE_INLINE __forceinline
#define HK_ALWAYS_INLINE __forceinline
#define HK_CALL __cdecl
#define HK_PASS_IN_REG
#define HK_RESTRICT __restrict
#define HK_ALIGN16(DECL) __declspec(align(16)) DECL
#define HK_ALIGN_FLOAT HK_ALIGN16
#define HK_ALIGN_REAL HK_ALIGN_FLOAT
#define HK_ALIGN_DOUBLE HK_ALIGN16
#define HK_ALIGN8(DECL) __declspec(align(8)) DECL
#define HK_ENDIAN_LITTLE 1
#define HK_ENDIAN_BIG 0
#define HK_NULL 0
#define HK_DECLARE_REFLECTION() static const class hkClass& staticClass()
#define HK_PREPROCESSOR_JOIN_TOKEN(A, B) HK_PREPROCESSOR_JOIN_TOKEN2(A, B)
#define HK_PREPROCESSOR_JOIN_TOKEN2(A, B) HK_PREPROCESSOR_JOIN_TOKEN3(A, B)
#define HK_PREPROCESSOR_JOIN_TOKEN3(A, B) A##B

// ---- hkError.h: release build, the checks are compiled out (havok_impl.md "Release semantics")
#define HK_ASSERT(id, a) ((void)0)
#define HK_ASSERT2(id, a, msg) ((void)0)
#define HK_ASSERT3(id, a, msg) ((void)0)
#define HK_MATH_ASSERT(id, a, msg) ((void)0)
#define HK_WARN(id, msg) ((void)0)
#define HK_WARN_ONCE(id, msg) ((void)0)
#define HK_WARN_ONCE_ON_DEBUG_IF(c, id, msg) ((void)0)
#define HK_WARN_ON_DEBUG_IF(c, id, msg) ((void)0)
#define HK_ERROR(id, msg) ((void)0)
#define HK_ON_DEBUG(CODE)
#define HK_ON_CPU(CODE) CODE
#define HK_ON_SPU(CODE)
#define HK_ON_PLATFORM_HAS_SPU(CODE)

namespace hkCompileError
{
template<bool b> struct COMPILE_ASSERTION_FAILURE;
template<> struct COMPILE_ASSERTION_FAILURE<true> {};
template<int x> struct StaticAssertTest {};
}

// As in the SDK: a typedef, so it fires only when the function template using it is instantiated.
#define HK_COMPILE_TIME_ASSERT(...)                                                                                     \
    typedef hkCompileError::StaticAssertTest<sizeof(hkCompileError::COMPILE_ASSERTION_FAILURE<static_cast<bool>(__VA_ARGS__)>)> \
        HK_PREPROCESSOR_JOIN_TOKEN(hkStaticAssertTypedef, __LINE__)
#define HK_COMPILE_TIME_ASSERT2(a, MSG)                                                                                 \
    typedef hkCompileError::StaticAssertTest<sizeof(hkCompileError::MSG<static_cast<bool>(a)>)>                        \
        HK_PREPROCESSOR_JOIN_TOKEN(hkStaticAssert_, HK_PREPROCESSOR_JOIN_TOKEN(MSG, __LINE__))

// ---- Types/Sse/hkSseMathTypes.h
#define HK_TRANSPOSE4f(A, B, C, D) _MM_TRANSPOSE4_PS(A.m_quad, B.m_quad, C.m_quad, D.m_quad)
// transpose 3 hkVector4: w component is undefined
#define HK_TRANSPOSE3f(_a, _b, _c)                                                                                       \
    {                                                                                                                    \
        const hkQuadFloat32 _tmp = _mm_unpacklo_ps(_a.m_quad, _b.m_quad);                                                \
        const hkQuadFloat32 _ctmp = _mm_shuffle_ps(_mm_unpackhi_ps(_a.m_quad, _b.m_quad), _c.m_quad, _MM_SHUFFLE(3, 2, 1, 0)); \
        _a.m_quad = _mm_movelh_ps(_tmp, _c.m_quad);                                                                      \
        _b.m_quad = _mm_shuffle_ps(_mm_movehl_ps(_a.m_quad, _tmp), _c.m_quad, _MM_SHUFFLE(3, 1, 1, 0));                  \
        _c.m_quad = _ctmp;                                                                                               \
    }

typedef __m128 hkQuadFloat32;   // storage of hkVector4f (and hkQuaternionf)
typedef __m128 hkSingleFloat32; // storage of hkSimdFloat32
typedef __m128 hkVector4fMask;  // storage of hkVector4fComparison
typedef __m128i hkQuadUint;     // storage of hkIntVector
typedef __m128i hkSingleInt128;
#define HK_QUADFLOAT_CONSTANT(a, b, c, d) { a, b, c, d }
#define HK_QUADINT_CONSTANT(a, b, c, d) { a, b, c, d }
#define HK_M128(x) x
#define HK_MASK_TO_VECTORf(x) (hkQuadFloat32) x

// argument types
class hkVector4f;
typedef const hkVector4f& hkVector4fParameter;
class hkSimdFloat32;
typedef const hkSimdFloat32& hkSimdFloat32Parameter;
class hkVector4fComparison;
typedef const hkVector4fComparison& hkVector4fComparisonParameter;
class hkQuaternionf;
typedef const hkQuaternionf& hkQuaternionfParameter;
class hkIntVector;
typedef const hkIntVector& hkIntVectorParameter;

// ---- hkMathHeaderForwardDeclarations.h
class hkMatrix3f;
class hkMatrix4f;
class hkRotationf;
class hkTransformf;
class hkQTransformf;
class hkQsTransformf;
class hkHalf;
class hkFloat16;
#define HK_FLOAT_ALIGNMENT 16

// double precision storage (HK_SSE_VERSION < 0x50); Havok's math functions keep double overloads next to the float ones
struct hkQuadDouble64
{
    __m128d xy;
    __m128d zw;
};
typedef __m128d hkSingleDouble64;
typedef hkQuadDouble64 hkVector4dMask;
#define HK_QUADDOUBLE_CONSTANT(a, b, c, d) { { a, b }, { c, d } }
class hkVector4d;
typedef const hkVector4d& hkVector4dParameter;
class hkSimdDouble64;
typedef const hkSimdDouble64& hkSimdDouble64Parameter;

// ---- hkMathHeaderUnions.h: unions to initialize vector constants by their bits
union hkQuadFloat32Union
{
    hkFloat32 r[4];
    hkQuadFloat32 q;
};
union hkQuadIntUnion
{
    hkInt32 u[4];
    hkQuadUint q;
};
union hkQuadUintUnion
{
    hkUint32 u[4];
    hkQuadUint q;
};
union hkQuadShortUnion
{
    hkInt16 u[8];
    hkQuadUint q;
};
union hkQuadUshortUnion
{
    hkUint16 u[8];
    hkQuadUint q;
};
union hkQuadCharUnion
{
    hkUint8 u[16];
    hkQuadUint q;
};
union hkQuadUcharUnion
{
    hkUint8 u[16];
    hkQuadUint q;
};
union hkIntUnion64
{
    hkInt64 i64;
    hkUint64 u64;
    hkInt32 i32[2];
    hkUint32 u32[2];
    hkInt16 i16[4];
    hkUint16 u16[4];
    hkInt8 i8[8];
    hkUint8 u8[8];
};
