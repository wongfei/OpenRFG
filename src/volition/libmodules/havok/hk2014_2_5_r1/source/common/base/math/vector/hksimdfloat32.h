// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkSimdFloat32.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_SIMDFLOAT_H)
#define HK_MATH_SIMDFLOAT_H

// compile time error checking
namespace hkCompileError
{
    template <bool b> struct HK_SIMDFLOAT_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED;
    template <> struct HK_SIMDFLOAT_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED<true>{ };
    template <bool b> struct HK_SIMDFLOAT_ILLEGAL_DIMENSION_USED;
    template <> struct HK_SIMDFLOAT_ILLEGAL_DIMENSION_USED<true>{ };
    template <bool b> struct HK_SIMDFLOAT_ILLEGAL_CONSTANT_REQUEST;
    template <> struct HK_SIMDFLOAT_ILLEGAL_CONSTANT_REQUEST<true>{ };
}
#define HK_SIMDFLOAT_DIMENSION_CHECK HK_COMPILE_TIME_ASSERT2(N==1, HK_SIMDFLOAT_ILLEGAL_DIMENSION_USED)
#define HK_SIMDFLOAT_TEMPLATE_CONFIG_NOT_IMPLEMENTED HK_COMPILE_TIME_ASSERT2(A<0, HK_SIMDFLOAT_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED)

HK_PASS_IN_REG
class hkSimdFloat32
{
public:

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSimdFloat32);

#ifndef HK_DISABLE_IMPLICIT_SIMDREAL_FLOAT_CONVERSION
    HK_FORCE_INLINE hkSimdFloat32() {}
    HK_FORCE_INLINE hkSimdFloat32(const hkFloat32& x);
    HK_FORCE_INLINE operator hkFloat32() const;

#endif //DISABLED CONVERSION

    HK_FORCE_INLINE static const hkSimdFloat32 HK_CALL convert(const hkSingleFloat32& x); // 0x007107B0 (hkssesimdreal.inl:11)
    HK_FORCE_INLINE static hkSimdFloat32 HK_CALL fromFloat(const hkFloat32& x); // 0x00B62CD0 (hksimdfloat32.inl:10)
    HK_FORCE_INLINE static hkSimdFloat32 HK_CALL fromHalf(const hkHalf& x);
    HK_FORCE_INLINE static hkSimdFloat32 HK_CALL fromUint16(const hkUint16& x);
    HK_FORCE_INLINE static hkSimdFloat32 HK_CALL fromUint8(const hkUint8& x);
    HK_FORCE_INLINE static hkSimdFloat32 HK_CALL fromInt32(const hkInt32& x); // 0x00D252E0
    HK_FORCE_INLINE void operator= ( hkSimdFloat32Parameter v );
    HK_FORCE_INLINE void setFromFloat(const hkFloat32& x); // 0x00B62B60 (hkssesimdreal.inl:59)
    HK_FORCE_INLINE void setFromFloat(const hkDouble64& x);
    HK_FORCE_INLINE void setFromHalf(const hkHalf& h); // 0x00B62B80 (hkssesimdreal.inl:72)
    HK_FORCE_INLINE void setFromUint16(const hkUint16& x);
    HK_FORCE_INLINE void setFromUint8(const hkUint8& x); // 0x00E98710
    HK_FORCE_INLINE void setFromInt32(const hkInt32&  x); // 0x00D24E50
    HK_FORCE_INLINE void setZero(); // 0x00B62BA0 (hkssesimdreal.inl:101)
    HK_FORCE_INLINE hkFloat32 getReal() const; // 0x0080C700 (hkssesimdreal.inl:54)
    HK_FORCE_INLINE void storeSaturateInt32( hkInt32* out) const; // 0x00D35390
    HK_FORCE_INLINE void storeSaturateUint16(hkUint16* result) const;
    template<int constant>
    HK_FORCE_INLINE static const hkSimdFloat32 HK_CALL getConstant();
    HK_FORCE_INLINE static const hkSimdFloat32 HK_CALL getConstant(hkVectorConstant constant); // 0x00D7B970
    HK_FORCE_INLINE void setMax(  hkSimdFloat32Parameter a, hkSimdFloat32Parameter b ); // 0x00B62BD0 (hkssesimdreal.inl:362)
    HK_FORCE_INLINE void setMin(  hkSimdFloat32Parameter a, hkSimdFloat32Parameter b ); // 0x00D78310
    HK_FORCE_INLINE void setClamped( hkSimdFloat32Parameter a, hkSimdFloat32Parameter minVal, hkSimdFloat32Parameter maxVal ); // 0x00CEEB90
    HK_FORCE_INLINE void setClampedZeroOne( hkSimdFloat32Parameter a ); // 0x00DD37F0
    HK_FORCE_INLINE void setAbs(  hkSimdFloat32Parameter a ); // 0x00CEEA60
    HK_FORCE_INLINE void setFloor(  hkSimdFloat32Parameter a ); // 0x00CEEA80
    HK_FORCE_INLINE void setMod(  hkSimdFloat32Parameter a, hkSimdFloat32Parameter b );
    HK_FORCE_INLINE void setFlipSign(hkSimdFloat32Parameter v, hkSimdFloat32Parameter sSign); // 0x00CEEAD0
    HK_FORCE_INLINE void setFlipSign(hkSimdFloat32Parameter v, hkVector4fComparisonParameter mask); // 0x00CEEAF0
    HK_FORCE_INLINE void setSelect( hkVector4fComparisonParameter comp, hkSimdFloat32Parameter trueValue, hkSimdFloat32Parameter falseValue ); // 0x00CEEA30
    HK_FORCE_INLINE void zeroIfFalse( hkVector4fComparisonParameter compareMask ); // 0x00CFB330
    HK_FORCE_INLINE void zeroIfTrue( hkVector4fComparisonParameter compareMask ); // 0x00D6C620
    template <int N> HK_FORCE_INLINE void store(  hkDouble64 *p ) const;
    template <int N> HK_FORCE_INLINE void store(  hkFloat32 *p ) const;
    template <int N> HK_FORCE_INLINE void store(  hkHalf *p ) const;
    template <int N> HK_FORCE_INLINE void store(  hkFloat16 *p ) const;
    template <int N> HK_FORCE_INLINE void load(const hkDouble64 *p );
    template <int N> HK_FORCE_INLINE void load(const hkFloat32 *p );
    template <int N> HK_FORCE_INLINE void load(const hkHalf *p );
    template <int N> HK_FORCE_INLINE void load(const hkFloat16 *p );
    HK_FORCE_INLINE const hkSimdFloat32 operator-() const;
    HK_FORCE_INLINE const hkSimdFloat32 operator+ (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE const hkSimdFloat32 operator- (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE const hkSimdFloat32 operator* (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE const hkSimdFloat32 operator/ (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE void add(hkSimdFloat32Parameter a); // 0x00CEEB60
    HK_FORCE_INLINE void sub(hkSimdFloat32Parameter a); // 0x00D6C6E0
    HK_FORCE_INLINE void mul(hkSimdFloat32Parameter a); // 0x00B62D20 (hksimdfloat32.inl:85)
    HK_FORCE_INLINE void div(hkSimdFloat32Parameter a); // 0x00D72AB0
    HK_FORCE_INLINE void setAdd(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00CEEB20
    HK_FORCE_INLINE void setSub(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00D6C6C0
    HK_FORCE_INLINE void setMul(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00B62D00 (hksimdfloat32.inl:63)
    HK_FORCE_INLINE void setDiv(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00B710C0 (hkssesimdreal.inl:598)
    HK_FORCE_INLINE void addMul(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00DF5800
    HK_FORCE_INLINE void subMul(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b); // 0x00CEEB70
    HK_FORCE_INLINE void setAddMul(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b, hkSimdFloat32Parameter c); // 0x00DF57E0
    HK_FORCE_INLINE void setSubMul(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b, hkSimdFloat32Parameter c); // 0x00CEEB40
    template <int N>
    HK_FORCE_INLINE void setDot( hkVector4fParameter a, hkVector4fParameter b );
    HK_FORCE_INLINE void setInterpolate( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b, hkSimdFloat32Parameter t ); // 0x00E87D90

#ifdef HK_DISABLE_OLD_VECTOR4_INTERFACE

    HK_FORCE_INLINE bool operator< (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE bool operator<= (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE bool operator> (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE bool operator>= (hkSimdFloat32Parameter r) const;
    HK_FORCE_INLINE bool operator== (hkSimdFloat32Parameter b) const;
    HK_FORCE_INLINE bool operator!= (hkSimdFloat32Parameter b) const;

#endif

    HK_FORCE_INLINE hkBool32 isSignBitSet() const; // 0x00E399E0
    HK_FORCE_INLINE hkBool32 isSignBitClear() const; // 0x00ED91E0
    HK_FORCE_INLINE hkBool32 isLess(hkSimdFloat32Parameter a) const; // 0x0074D7D0 (hkssesimdreal.inl:256)
    HK_FORCE_INLINE hkBool32 isLessEqual(hkSimdFloat32Parameter a) const; // 0x00D6C560
    HK_FORCE_INLINE hkBool32 isGreater(hkSimdFloat32Parameter a) const; // 0x00CFC9C0
    HK_FORCE_INLINE hkBool32 isGreaterEqual(hkSimdFloat32Parameter a) const; // 0x00D6C580
    HK_FORCE_INLINE hkBool32 isEqual(hkSimdFloat32Parameter a) const; // 0x00D92740
    HK_FORCE_INLINE hkBool32 isNotEqual(hkSimdFloat32Parameter a) const; // 0x00CEEA10
    HK_FORCE_INLINE hkBool32 isLessZero() const; // 0x00D7B990
    HK_FORCE_INLINE hkBool32 isLessEqualZero() const; // 0x00D6C5A0
    HK_FORCE_INLINE hkBool32 isGreaterZero() const; // 0x00D6C5C0
    HK_FORCE_INLINE hkBool32 isGreaterEqualZero() const; // 0x00D6C5E0
    HK_FORCE_INLINE hkBool32 isEqualZero() const; // 0x00D6C600
    HK_FORCE_INLINE hkBool32 isNotEqualZero() const; // 0x00D81CE0
    HK_FORCE_INLINE hkBool32 isOk() const;
    HK_FORCE_INLINE const hkVector4fComparison signBitSet() const;
    HK_FORCE_INLINE const hkVector4fComparison signBitClear() const; // 0x00FDD2F0
    HK_FORCE_INLINE const hkVector4fComparison less(hkSimdFloat32Parameter a) const; // 0x00CEE9D0
    HK_FORCE_INLINE const hkVector4fComparison lessEqual(hkSimdFloat32Parameter a) const; // 0x00D6C540
    HK_FORCE_INLINE const hkVector4fComparison greater(hkSimdFloat32Parameter a) const; // 0x00CEE9F0
    HK_FORCE_INLINE const hkVector4fComparison greaterEqual(hkSimdFloat32Parameter a) const; // 0x00D98F30
    HK_FORCE_INLINE const hkVector4fComparison equal(hkSimdFloat32Parameter a) const;
    HK_FORCE_INLINE const hkVector4fComparison notEqual(hkSimdFloat32Parameter a) const; // 0x00D98F50
    HK_FORCE_INLINE const hkVector4fComparison lessZero() const; // 0x0080C740 (hkssesimdreal.inl:206)
    HK_FORCE_INLINE const hkVector4fComparison lessEqualZero() const; // 0x00D81CC0
    HK_FORCE_INLINE const hkVector4fComparison greaterZero() const; // 0x00DC2FA0
    HK_FORCE_INLINE const hkVector4fComparison greaterEqualZero() const; // 0x00D82580
    HK_FORCE_INLINE const hkVector4fComparison equalZero() const; // 0x00D839B0
    HK_FORCE_INLINE const hkVector4fComparison notEqualZero() const; // 0x00D839D0
    HK_FORCE_INLINE bool approxEqual(hkSimdFloat32Parameter v, hkSimdFloat32Parameter epsilon) const; // 0x00E87DC0
    HK_FORCE_INLINE const hkSimdFloat32 sqrt() const; // 0x00D6C7C0
    HK_FORCE_INLINE const hkSimdFloat32 sqrtInverse() const; // 0x00B88470 (hkssesimdreal.inl:702)
    HK_FORCE_INLINE void setReciprocal(hkSimdFloat32Parameter a); // 0x00CEF060
    HK_FORCE_INLINE const hkSimdFloat32 reciprocal() const; // 0x00D6C7A0

#ifndef HK_DISABLE_OLD_VECTOR4_INTERFACE

    // old interface methods
    HK_FORCE_INLINE hkVector4fComparison isNegative() const;
    HK_FORCE_INLINE hkVector4fComparison isPositive() const;
    HK_FORCE_INLINE hkBool32 isZero() const;
    HK_FORCE_INLINE hkVector4fComparison compareLessThanZero() const;
    HK_FORCE_INLINE hkVector4fComparison compareGreaterThanZero() const;
    HK_FORCE_INLINE hkVector4fComparison compareLessThan(hkSimdFloat32Parameter a) const;
    HK_FORCE_INLINE hkVector4fComparison compareLessThanEqual(hkSimdFloat32Parameter a) const;
    HK_FORCE_INLINE hkVector4fComparison compareGreaterThan(hkSimdFloat32Parameter a) const;
    HK_FORCE_INLINE hkVector4fComparison compareGreaterThanEqual(hkSimdFloat32Parameter a) const;

#endif

    //
    // advanced interface
    //

    template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkDouble64 *p );
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkFloat32 *p );
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkHalf *p );
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkFloat16 *p );
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(  hkDouble64 *p ) const;
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(  hkFloat32 *p ) const;
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(  hkHalf *p ) const;
    template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(  hkFloat16 *p ) const;
    template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(  hkDouble64 *p ) const;
    template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(  hkFloat32 *p ) const;
    template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(  hkHalf *p ) const;
    template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(  hkFloat16 *p ) const;
    template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void setReciprocal(hkSimdFloat32Parameter a);
    template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void setDiv(hkSimdFloat32Parameter a, hkSimdFloat32Parameter b);
    template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void div(hkSimdFloat32Parameter a);
    template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 sqrt() const;
    template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 sqrtInverse() const;
    hkSingleFloat32 m_real;
};

#ifndef HK_DISABLE_OLD_VECTOR4_INTERFACE

// old interface static methods

namespace hkMath
{
    HK_FORCE_INLINE int isNegative(hkSimdFloat32Parameter r0)
    {
        return (r0.getReal()<0)? hkVector4ComparisonMask::MASK_X : 0;
    }
    HK_FORCE_INLINE int isLess( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
    {
        hkSimdFloat32 sub = a - b;
        return isNegative( sub );
    }
    HK_FORCE_INLINE int isGreater( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
    {
        hkSimdFloat32 sub = b - a;
        return isNegative( sub );
    }
}

// old interface implemented comparisons using implicit casts

HK_FORCE_INLINE hkBool32 operator<( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
{
    return a.isLess(b);
}

HK_FORCE_INLINE hkBool32 operator<( hkSimdFloat32Parameter a, hkFloat32 b)
{
    return hkBool32( a.getReal() < b );
}

HK_FORCE_INLINE hkBool32 operator<( hkFloat32 a, hkSimdFloat32Parameter b)
{
    return hkBool32( a < b.getReal() );
}

HK_FORCE_INLINE bool operator<=( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
{
    return a.isLessEqual(b);
}

HK_FORCE_INLINE bool operator<=( hkSimdFloat32Parameter a, hkFloat32 b)
{
    return ( a.getReal() <= b );
}

HK_FORCE_INLINE bool operator<=( hkFloat32 a, hkSimdFloat32Parameter b)
{
    return ( a <= b.getReal() );
}

HK_FORCE_INLINE hkBool32 operator>( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
{
    return a.isGreater(b);
}

HK_FORCE_INLINE hkBool32 operator>( hkSimdFloat32Parameter a, hkFloat32 b)
{
    return hkBool32( a.getReal() > b );
}

HK_FORCE_INLINE hkBool32 operator>( hkFloat32 a, hkSimdFloat32Parameter b)
{
    return hkBool32( a > b.getReal() );
}

HK_FORCE_INLINE bool operator>=( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b)
{
    return a.isGreaterEqual(b);
}

HK_FORCE_INLINE bool operator>=( hkSimdFloat32Parameter a, hkFloat32 b)
{
    return ( a.getReal() >= b );
}

HK_FORCE_INLINE bool operator>=( hkFloat32 a, hkSimdFloat32Parameter b)
{
    return ( a >= b.getReal() );
}

#endif

#endif //HK_MATH_SIMDFLOAT_H
