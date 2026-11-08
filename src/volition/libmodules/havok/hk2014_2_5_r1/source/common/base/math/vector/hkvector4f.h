// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkVector4f.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_VECTOR4f_H)
#define HK_MATH_VECTOR4f_H

// compile time error checking
namespace hkCompileError
{
    template <bool b> struct HK_VECTOR4f_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED;
    template <> struct HK_VECTOR4f_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED<true>{ };
    template <bool b> struct HK_VECTOR4f_SUBVECTOR_INDEX_OUT_OF_RANGE;
    template <> struct HK_VECTOR4f_SUBVECTOR_INDEX_OUT_OF_RANGE<true>{ };
    template <bool b> struct HK_VECTOR4f_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH;
    template <> struct HK_VECTOR4f_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH<true>{ };
    template <bool b> struct HK_VECTOR4f_UNSUPPORTED_VECTOR_LENGTH;
    template <> struct HK_VECTOR4f_UNSUPPORTED_VECTOR_LENGTH<true>{ };
    template <bool b> struct HK_VECTOR4f_UNKNOWN_CONSTANT;
    template <> struct HK_VECTOR4f_UNKNOWN_CONSTANT<true>{ };
}
#define HK_VECTOR4f_SUBINDEX_CHECK           HK_COMPILE_TIME_ASSERT2((I>=0)&&(I<4), HK_VECTOR4f_SUBVECTOR_INDEX_OUT_OF_RANGE)
#define HK_VECTOR4f_NOT_IMPLEMENTED          HK_COMPILE_TIME_ASSERT2(N==0, HK_VECTOR4f_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH)
#define HK_VECTOR4f_UNSUPPORTED_LENGTH_CHECK HK_COMPILE_TIME_ASSERT2((N>0)&&(N<=4), HK_VECTOR4f_UNSUPPORTED_VECTOR_LENGTH)
#define HK_VECTOR4f_TEMPLATE_CONFIG_NOT_IMPLEMENTED HK_COMPILE_TIME_ASSERT2(A<0, HK_VECTOR4f_TEMPLATE_CONFIGURATION_NOT_IMPLEMENTED)
#define HK_VECTOR4f_CONSTANT_CHECK           HK_COMPILE_TIME_ASSERT2(((vectorConstant>HK_QUADREAL_BEGIN)&&(vectorConstant<HK_QUADREAL_END)),HK_VECTOR4f_UNKNOWN_CONSTANT)

// Components that aren't loaded during hkVector4f::load() are initialized to this in Debug and Dev configurations.
#define HK_VECTOR4f_DEBUG_FILL_VALUE (g_vectorfConstants[HK_QUADREAL_BEGIN])

HK_PASS_IN_REG
class hkVector4f
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkVector4f);
        HK_DECLARE_POD_TYPE();

        // COM-2625: Work around bad code generation in x64, vs2013
#if !defined(HK_DISABLE_MATH_CONSTRUCTORS) || (defined(HK_PLATFORM_X64) && defined(HK_COMPILER_MSVC) && (HK_COMPILER_MSVC_VERSION >= 1800))
        HK_FORCE_INLINE hkVector4f() { } // 0x00710660 (.h:109)
        HK_FORCE_INLINE hkVector4f(hkFloat32 x, hkFloat32 y, hkFloat32 z, hkFloat32 w=hkFloat32(0)); // 0x0080C570 (hkssevector4.inl:22)
        explicit HK_FORCE_INLINE hkVector4f(const hkQuadFloat32& q);
        HK_FORCE_INLINE hkVector4f( const hkVector4f& v); // 0x00742270 (hkssevector4.inl:32)
#endif

        HK_FORCE_INLINE void operator= ( hkVector4fParameter v ); // 0x007107E0 (hkvector4f.inl:559)
        HK_FORCE_INLINE void set( hkSimdFloat32Parameter a, hkSimdFloat32Parameter b, hkSimdFloat32Parameter c, hkSimdFloat32Parameter d ); // 0x00CFB2E0
        HK_FORCE_INLINE void set(hkFloat32 x, hkFloat32 y, hkFloat32 z, hkFloat32 w=hkFloat32(0)); // 0x007106A0 (hkssevector4.inl:38)
        HK_FORCE_INLINE void setAll(hkSimdFloat32Parameter x); // 0x0080C5C0 (hkssevector4.inl:55)
        HK_FORCE_INLINE void setAll(const hkFloat32& x); // 0x0080C5A0 (hkssevector4.inl:50)
        HK_FORCE_INLINE void setZero(); // 0x0074D7A0 (hkssevector4.inl:60)
        template <int I> HK_FORCE_INLINE void zeroComponent();
        HK_FORCE_INLINE void zeroComponent(const int i); // 0x00D8AB70
        HK_FORCE_INLINE void add(hkVector4fParameter a); // 0x007107F0 (hkvector4f.inl:564)
        HK_FORCE_INLINE void sub(hkVector4fParameter a); // 0x0080C7B0 (hkvector4f.inl:569)
        HK_FORCE_INLINE void mul(hkVector4fParameter a); // 0x00941B50 (hkvector4f.inl:574)
        HK_FORCE_INLINE void div(hkVector4fParameter a); // 0x00E12510
        HK_FORCE_INLINE void setAdd(hkVector4fParameter a, hkVector4fParameter b); // 0x007106D0 (hkssevector4.inl:126)
        HK_FORCE_INLINE void setSub(hkVector4fParameter a, hkVector4fParameter b); // 0x0074D7B0 (hkssevector4.inl:136)
        HK_FORCE_INLINE void setMul(hkVector4fParameter a, hkVector4fParameter b); // 0x00941AF0 (hkssevector4.inl:146)
        HK_FORCE_INLINE void setDiv(hkVector4fParameter a, hkVector4fParameter b); // 0x00E124E0
        HK_FORCE_INLINE void mul(hkSimdFloat32Parameter a); // 0x00710800 (hkvector4f.inl:579)
        HK_FORCE_INLINE void setMul(hkVector4fParameter a, hkSimdFloat32Parameter r); // 0x00710710 (hkssevector4.inl:151)
        HK_FORCE_INLINE void setMul(hkSimdFloat32Parameter r, hkVector4fParameter a); // 0x00710810 (hkvector4f.inl:584)
        HK_FORCE_INLINE void setAdd(hkVector4fParameter a, hkSimdFloat32Parameter b); // 0x007106F0 (hkssevector4.inl:131)
        HK_FORCE_INLINE void setSub(hkVector4fParameter a, hkSimdFloat32Parameter b); // 0x00D24DF0
        HK_FORCE_INLINE void setReciprocal(hkVector4fParameter v); // 0x00D761B0
        HK_FORCE_INLINE void setSqrt(hkVector4fParameter a); // 0x00D7BFD0
        HK_FORCE_INLINE void setSqrtInverse(hkVector4fParameter a); // 0x00FB7680
        HK_FORCE_INLINE void addMul(hkVector4fParameter a, hkVector4fParameter b); // 0x00D76010
        HK_FORCE_INLINE void setAddMul(hkVector4fParameter a, hkVector4fParameter b, hkVector4fParameter c); // 0x00D1A800
        HK_FORCE_INLINE void subMul(hkVector4fParameter a, hkVector4fParameter b); // 0x00EA8D70
        HK_FORCE_INLINE void setSubMul(hkVector4fParameter a, hkVector4fParameter b, hkVector4fParameter c); // 0x00E71F50
        HK_FORCE_INLINE void addMul(hkVector4fParameter a, hkSimdFloat32Parameter r); // 0x0080C7F0 (hkvector4f.inl:717)
        HK_FORCE_INLINE void addMul(hkSimdFloat32Parameter r, hkVector4fParameter a); // 0x00941B80 (hkvector4f.inl:722)
        HK_FORCE_INLINE void subMul(hkVector4fParameter a, hkSimdFloat32Parameter r); // 0x00B62C00 (hkvector4f.inl:727)
        HK_FORCE_INLINE void subMul(hkSimdFloat32Parameter r, hkVector4fParameter a); // 0x00941B60 (hkvector4f.inl:599)
        HK_FORCE_INLINE void setAddMul(hkVector4fParameter a, hkVector4fParameter b, hkSimdFloat32Parameter r); // 0x0080C5D0 (hkssevector4.inl:161)
        HK_FORCE_INLINE void setSubMul(hkVector4fParameter a, hkVector4fParameter b, hkSimdFloat32Parameter r); // 0x00941B10 (hkssevector4.inl:156)
        HK_FORCE_INLINE void setCross( hkVector4fParameter a, hkVector4fParameter b ); // 0x0080C5F0 (hkssevector4.inl:176)
        HK_FORCE_INLINE void setInterpolate( hkVector4fParameter a, hkVector4fParameter b, hkSimdFloat32Parameter t ); // 0x0080C7C0 (hkvector4f.inl:645)
        HK_FORCE_INLINE const hkVector4fComparison less(hkVector4fParameter a) const; // 0x00D76160
        HK_FORCE_INLINE const hkVector4fComparison lessEqual(hkVector4fParameter a) const; // 0x00813FB0 (hkssevector4.inl:200)
        HK_FORCE_INLINE const hkVector4fComparison greater(hkVector4fParameter a) const; // 0x00D6C740
        HK_FORCE_INLINE const hkVector4fComparison greaterEqual(hkVector4fParameter a) const; // 0x00CEEFA0
        HK_FORCE_INLINE const hkVector4fComparison equal(hkVector4fParameter a) const; // 0x00B71040 (hkssevector4.inl:185)
        HK_FORCE_INLINE const hkVector4fComparison notEqual(hkVector4fParameter a) const; // 0x00E72B60
        HK_FORCE_INLINE const hkVector4fComparison lessZero() const; // 0x00D82660
        HK_FORCE_INLINE const hkVector4fComparison lessEqualZero() const; // 0x00D808B0
        HK_FORCE_INLINE const hkVector4fComparison greaterZero() const; // 0x00D7BC80
        HK_FORCE_INLINE const hkVector4fComparison greaterEqualZero() const; // 0x00D7BCA0
        HK_FORCE_INLINE const hkVector4fComparison equalZero() const; // 0x00D7BCC0
        HK_FORCE_INLINE const hkVector4fComparison notEqualZero() const; // 0x00D82680
        HK_FORCE_INLINE const hkVector4fComparison signBitSet() const;
        HK_FORCE_INLINE const hkVector4fComparison signBitClear() const;
        template <int N> HK_FORCE_INLINE hkBool32 allComponentsEqual() const;
        template <int N> HK_FORCE_INLINE hkBool32 allExactlyEqual(hkVector4fParameter v) const;
        template <int N> HK_FORCE_INLINE hkBool32 allExactlyEqualZero() const;
        template <int N> HK_FORCE_INLINE hkBool32 allEqual(hkVector4fParameter v, hkSimdFloat32Parameter epsilon) const;
        template <int N> HK_FORCE_INLINE hkBool32 allEqualZero(hkSimdFloat32Parameter epsilon) const;
        template <int N> HK_FORCE_INLINE hkBool32 allLess(hkVector4fParameter a) const;
        template <int N> HK_FORCE_INLINE hkBool32 allLessZero() const;
        HK_FORCE_INLINE void setSelect( hkVector4fComparisonParameter compareMask, hkVector4fParameter trueValue, hkVector4fParameter falseValue ); // 0x0080C620 (hkssevector4.inl:245)
        HK_FORCE_INLINE void zeroIfFalse( hkVector4fComparisonParameter compareMask ); // 0x00CFB310
        HK_FORCE_INLINE void zeroIfTrue( hkVector4fComparisonParameter compareMask ); // 0x00D7B950
        template<hkVector4ComparisonMask::Mask M> HK_FORCE_INLINE void setSelect( hkVector4fParameter trueValue, hkVector4fParameter falseValue );
        template <int N> HK_FORCE_INLINE void setNeg(hkVector4fParameter v);
        HK_FORCE_INLINE void setFlipSign(hkVector4fParameter v, hkVector4fComparisonParameter mask); // 0x00CFC9A0
        HK_FORCE_INLINE void setFlipSign(hkVector4fParameter v, hkVector4fParameter vSign); // 0x00D746D0
        HK_FORCE_INLINE void setFlipSign(hkVector4fParameter v, hkSimdFloat32Parameter sSign); // 0x00CEE920
        HK_FORCE_INLINE void setAbs(hkVector4fParameter v); // 0x00710750 (hkssevector4.inl:367)
        HK_FORCE_INLINE void setMin(hkVector4fParameter a, hkVector4fParameter b); // 0x00B62B20 (hkssevector4.inl:372)
        HK_FORCE_INLINE void setMax(hkVector4fParameter a, hkVector4fParameter b); // 0x00B62B40 (hkssevector4.inl:377)
        HK_FORCE_INLINE void setClamped( hkVector4fParameter a, hkVector4fParameter minVal, hkVector4fParameter maxVal ); // 0x00EF6300
        HK_FORCE_INLINE void setClampedZeroOne( hkVector4fParameter a ); // 0x00EB9F20
        HK_FORCE_INLINE void setClampedToMaxLength(hkVector4fParameter vSrc, hkSimdFloat32Parameter maxLen); // 0x00EADF50
        void setRotatedDir(const hkMatrix3f& a, hkVector4fParameter b); // 0x00CF00F0
        void setRotatedInverseDir(const hkMatrix3f& a, hkVector4fParameter b); // 0x00CF0130
        void setTransformedPos(const hkTransformf& a, hkVector4fParameter b); // 0x00CF01E0
        void setTransformedInversePos(const hkTransformf& a, hkVector4fParameter b); // 0x00CF0220
        void setRotatedDir(hkQuaternionfParameter quat, hkVector4fParameter direction); // 0x00CF0560
        void setRotatedInverseDir(hkQuaternionfParameter quat, hkVector4fParameter direction); // 0x00CF05E0
        void setTransformedPos(const hkQsTransformf& a, hkVector4fParameter b); // 0x00CF02D0
        void setTransformedInversePos(const hkQsTransformf& a, hkVector4fParameter b); // 0x00CF0370
        void setTransformedPos(const hkQTransformf& a, hkVector4fParameter b); // 0x00CF0420
        void setTransformedInversePos(const hkQTransformf& a, hkVector4fParameter b); // 0x00CF04C0
        HK_FORCE_INLINE void _setRotatedDir(const hkMatrix3f& a, hkVector4fParameter b); // 0x0071A700 (hkssevector4.inl:384)
        HK_FORCE_INLINE void _setRotatedInverseDir(const hkMatrix3f& a, hkVector4fParameter b); // 0x00B71060 (hkssevector4.inl:401)
        HK_FORCE_INLINE void _setTransformedPos(const hkTransformf& a, hkVector4fParameter b); // 0x0071A740 (hkvector4f.inl:673)
        HK_FORCE_INLINE void _setTransformedInversePos(const hkTransformf& a, hkVector4fParameter b); // 0x00CF0780
        HK_FORCE_INLINE void _setTransformedPos(const hkQsTransformf& a, hkVector4fParameter b); // 0x00CF0830
        HK_FORCE_INLINE void _setTransformedInversePos(const hkQsTransformf& a, hkVector4fParameter b); // 0x00CF08D0
        HK_FORCE_INLINE void _setTransformedPos(const hkQTransformf& a, hkVector4fParameter b); // 0x00CF0980
        HK_FORCE_INLINE void _setTransformedInversePos(const hkQTransformf& a, hkVector4fParameter b); // 0x00CF0A20
        HK_FORCE_INLINE void _setRotatedDir(hkQuaternionfParameter quat, hkVector4fParameter direction); // 0x00CEF080
        HK_FORCE_INLINE void _setRotatedInverseDir(hkQuaternionfParameter quat, hkVector4fParameter direction); // 0x00CF0700
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 dot(hkVector4fParameter a) const;
        template <int N> HK_FORCE_INLINE void setDot(hkVector4fParameter a, hkVector4fParameter b);
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 horizontalAdd() const;
        template <int N> HK_FORCE_INLINE void setHorizontalAdd(hkVector4fParameter v);
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 horizontalMul() const;
        template <int N> HK_FORCE_INLINE void setHorizontalMul(hkVector4fParameter v);
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 horizontalMax() const;
        template <int N> HK_FORCE_INLINE void setHorizontalMax(hkVector4fParameter v);
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 horizontalMin() const;
        template <int N> HK_FORCE_INLINE void setHorizontalMin(hkVector4fParameter v);
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 length() const;
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 lengthSquared() const;
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 lengthInverse() const;
        template <int N> HK_FORCE_INLINE void normalize();
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 normalizeWithLength();
        template <int N> HK_FORCE_INLINE hkBool32 normalizeIfNotZero();
        template <int N> HK_FORCE_INLINE const hkSimdFloat32 setNormalizedEnsureUnitLength(hkVector4fParameter v);
        HK_FORCE_INLINE void setPlaneConstant(hkVector4fParameter aPointOnPlane );
        HK_FORCE_INLINE const hkSimdFloat32 dot4xyz1(hkVector4fParameter a) const; // 0x00D6C760
        HK_FORCE_INLINE const hkSimdFloat32 distanceTo( hkVector4fParameter p ) const; // 0x00D006D0
        HK_FORCE_INLINE const hkSimdFloat32 distanceToSquared( hkVector4fParameter p ) const; // 0x00D006A0
        HK_FORCE_INLINE void setXYZ_W(hkVector4fParameter xyz, hkVector4fParameter w); // 0x00D75170
        HK_FORCE_INLINE void setXYZ_W(hkVector4fParameter xyz, hkSimdFloat32Parameter w); // 0x00941B30 (hkssevector4.inl:678)
        HK_FORCE_INLINE void setW(hkVector4fParameter v); // 0x00D728C0
        HK_FORCE_INLINE void setW(hkSimdFloat32Parameter w); // 0x0080C790 (hkvector4f.inl:291)
        HK_FORCE_INLINE void setXYZ(hkVector4fParameter v); // 0x0080C670 (hkssevector4.inl:688)
        HK_FORCE_INLINE void setXYZ(hkFloat32 v); // 0x0080C690 (hkssevector4.inl:705)
        HK_FORCE_INLINE void setXYZ(hkSimdFloat32Parameter v); // 0x00D782F0
        HK_FORCE_INLINE void setXYZ_0(hkVector4fParameter xyz); // 0x00710790 (hkssevector4.inl:716)
        HK_FORCE_INLINE void addXYZ(hkVector4fParameter v); // 0x00CFC990
        HK_FORCE_INLINE void subXYZ(hkVector4fParameter v); // 0x00EFCC70
        HK_FORCE_INLINE void setBroadcastXYZ(const int i, hkVector4fParameter v); // 0x00DAB000
        HK_FORCE_INLINE void setInt24W( int value ); // 0x00D7B9B0
        HK_FORCE_INLINE int getInt24W( ) const ; // 0x00D958C0
        HK_FORCE_INLINE int getInt16W( ) const ; // 0x00DAC170
        HK_FORCE_INLINE hkFloat32& operator() (int i); // 0x00710770 (hkssevector4.inl:661)
        HK_FORCE_INLINE const hkFloat32& operator() (int i) const; // 0x00710780 (hkssevector4.inl:667)
        template <int I> HK_FORCE_INLINE const hkSimdFloat32 getComponent() const;
        HK_FORCE_INLINE const hkSimdFloat32 getW() const; // 0x00D761F0
        HK_FORCE_INLINE const hkSimdFloat32 getComponent(const int i) const; // 0x00CEF020
        template <int I> HK_FORCE_INLINE const hkFloat32* getComponentAddress() const;
        template <int I> HK_FORCE_INLINE hkFloat32* getComponentAddress();
        HK_FORCE_INLINE const hkFloat32* getComponentAddress(int i) const;
        HK_FORCE_INLINE hkFloat32* getComponentAddress(int i);
        template <int I> HK_FORCE_INLINE void setComponent(hkSimdFloat32Parameter val);
        HK_FORCE_INLINE void setComponent(const int i, hkSimdFloat32Parameter val); // 0x00D274B0
        template <int N> HK_FORCE_INLINE int findComponent(hkSimdFloat32Parameter value) const;
        template <hkVectorPermutation::Permutation P> HK_FORCE_INLINE void setPermutation(hkVector4fParameter v);
        template <int I> HK_FORCE_INLINE void broadcast();
        HK_FORCE_INLINE void broadcast(int i);
        template <int I> HK_FORCE_INLINE void setBroadcast(hkVector4fParameter v);
        HK_FORCE_INLINE void setBroadcast(const int i, hkVector4fParameter v); // 0x00DAAF20
        template <int N> HK_FORCE_INLINE int getIndexOfMaxAbsComponent() const;
        template <int N> HK_FORCE_INLINE int getIndexOfMaxComponent() const;
        template <int N> HK_FORCE_INLINE int getIndexOfMinAbsComponent() const;
        template <int N> HK_FORCE_INLINE int getIndexOfMinComponent() const;
        template <int N> HK_FORCE_INLINE hkBool32 isOk() const;
        template <int N> HK_FORCE_INLINE bool isNormalized(hkFloat32 epsilon = hkFloat32(1e-4f)) const;
        HK_FORCE_INLINE static const hkVector4f& HK_CALL getZero(); // 0x00B71120 (hkvector4f.inl:522)
        template<int vectorConstant>
        HK_FORCE_INLINE static const hkVector4f& HK_CALL getConstant();
        HK_FORCE_INLINE static const hkVector4f& HK_CALL getConstant(hkVectorConstant constant);
        template<int vectorConstant>
        HK_FORCE_INLINE void setConstant();
        template <int N> HK_FORCE_INLINE void load(const hkDouble64* p);
        template <int N> HK_FORCE_INLINE void load(const hkFloat32* p);
        template <int N> HK_FORCE_INLINE void load(const hkHalf* p);
        template <int N> HK_FORCE_INLINE void load(const hkFloat16* p);
        template <int N> HK_FORCE_INLINE void store(hkDouble64* p) const;
        template <int N> HK_FORCE_INLINE void store(hkFloat32* p) const;
        template <int N> HK_FORCE_INLINE void store(hkHalf* p) const;
        template <int N> HK_FORCE_INLINE void store(hkFloat16* p) const;
        HK_FORCE_INLINE void reduceToHalfPrecision();
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkDouble64* p);
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkFloat32* p);
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkHalf* p);
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void load(const hkFloat16* p);
        template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(hkDouble64* p) const;
        template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(hkFloat32* p) const;
        template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(hkHalf* p) const;
        template <int N, hkMathIoMode A, hkMathRoundingMode R> HK_FORCE_INLINE void store(hkFloat16* p) const;
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(hkDouble64* p) const;
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(hkFloat32* p) const;
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(hkHalf* p) const;
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(hkFloat16* p) const;
        template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void setReciprocal(hkVector4fParameter v);
        template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void setDiv(hkVector4fParameter a, hkVector4fParameter b);
        template <hkMathAccuracyMode A, hkMathDivByZeroMode D> HK_FORCE_INLINE void div(hkVector4fParameter a);
        template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE void setSqrt(hkVector4fParameter a);
        template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE void setSqrtInverse(hkVector4fParameter a);
        template <int N, hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 length() const;
        template <int N, hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 lengthInverse() const;
        template <int N, hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE void normalize();
        template <int N, hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 normalizeWithLength();
        template <int N, hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE hkBool32 normalizeIfNotZero();
        template <hkMathAccuracyMode A, hkMathNegSqrtMode S> HK_FORCE_INLINE const hkSimdFloat32 distanceTo( hkVector4fParameter p ) const;
        template <int N, hkMathAccuracyMode A> HK_FORCE_INLINE const hkSimdFloat32 setNormalizedEnsureUnitLength(hkVector4fParameter v);
        template <int N, hkMathAccuracyMode A>
        HK_FORCE_INLINE void setClampedToMaxLength(hkVector4fParameter vSrc, hkSimdFloat32Parameter maxLen); // 0x00EADF50
        HK_ALIGN_FLOAT( hkQuadFloat32 m_quad );

#ifndef HK_DISABLE_OLD_VECTOR4_INTERFACE

        //
        // old interface
        //

        HK_FORCE_INLINE hkQuadFloat32& getQuad();
        HK_FORCE_INLINE const hkQuadFloat32& getQuad() const;
        HK_FORCE_INLINE void operator= ( const hkQuadFloat32& v );
        HK_FORCE_INLINE void add4(hkVector4fParameter v);
        HK_FORCE_INLINE void sub4(hkVector4fParameter v);
        HK_FORCE_INLINE void mul4(hkVector4fParameter a);
        HK_FORCE_INLINE void mul4(hkSimdFloat32Parameter a);
        HK_FORCE_INLINE void div4(hkVector4fParameter a);
        HK_FORCE_INLINE void div4fast(hkVector4fParameter a);
        HK_FORCE_INLINE hkSimdFloat32 dot3(hkVector4fParameter v) const;
        HK_FORCE_INLINE hkSimdFloat32 dot4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkFloat32 dot3fpu(hkVector4fParameter a) const;
        HK_FORCE_INLINE void setMul4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setMul4(hkSimdFloat32Parameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void _setMul3(const hkMatrix3f& m, hkVector4fParameter v);
        HK_FORCE_INLINE void setMul3(const hkMatrix3f& a, hkVector4fParameter b );
        HK_FORCE_INLINE void subMul4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void subMul4(hkSimdFloat32Parameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setSubMul4(hkVector4fParameter a, hkVector4fParameter x, hkVector4fParameter y);
        HK_FORCE_INLINE void setSubMul4(hkVector4fParameter a, hkVector4fParameter x, hkSimdFloat32Parameter y);
        HK_FORCE_INLINE void setDot3(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setDot4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setSelect4( hkVector4fComparisonParameter comp, hkVector4fParameter trueValue, hkVector4fParameter falseValue);
        HK_FORCE_INLINE void select32( hkVector4fParameter falseValue, hkVector4fParameter trueValue, hkVector4fComparisonParameter comp);
        HK_FORCE_INLINE void setBroadcast(hkVector4fParameter v, int i);
        HK_FORCE_INLINE void setBroadcast3clobberW(hkVector4fParameter v, int i);
        HK_FORCE_INLINE void setXYZW(hkVector4fParameter xyz, hkVector4fParameter w);
        HK_FORCE_INLINE void setXYZW(hkVector4fParameter xyz, hkSimdFloat32Parameter w);
        HK_FORCE_INLINE void setXYZ0(hkVector4fParameter xyz);
        HK_FORCE_INLINE void addMul4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void addMul4(hkSimdFloat32Parameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setZero4();
        HK_FORCE_INLINE void zeroElement( int i );
        HK_FORCE_INLINE void setAll3(hkFloat32 x);
        HK_FORCE_INLINE void setSwapXY(const hkVector4f& x);
        HK_FORCE_INLINE void setNeg3(hkVector4fParameter v);
        HK_FORCE_INLINE void setNeg4(hkVector4fParameter v);
        HK_FORCE_INLINE void setNegMask4(hkVector4fParameter v, int mask);
        HK_FORCE_INLINE void setDiv4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setDiv4fast(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE hkSimdFloat32 getSimdAt(int i) const;
        HK_FORCE_INLINE void normalize3();
        HK_FORCE_INLINE void normalize4();
        HK_FORCE_INLINE void fastNormalize3();
        HK_FORCE_INLINE void setFastNormalize3NonZero(hkVector4fParameter other);
        HK_FORCE_INLINE hkSimdFloat32 normalizeWithLength3();
        HK_FORCE_INLINE hkSimdFloat32 normalizeWithLength4();
        HK_FORCE_INLINE hkSimdFloat32 fastNormalizeWithLength3();
        HK_FORCE_INLINE void fastNormalize3NonZero();
        HK_FORCE_INLINE hkResult normalize3IfNotZero ();
        HK_FORCE_INLINE hkBool isNormalized3(hkFloat32 eps = 1e-4f) const;
        HK_FORCE_INLINE hkBool isNormalized4(hkFloat32 eps = 1e-4f) const;
        HK_FORCE_INLINE hkSimdFloat32 length3() const;
        HK_FORCE_INLINE hkSimdFloat32 length4() const;
        HK_FORCE_INLINE hkSimdFloat32 lengthSquared3() const;
        HK_FORCE_INLINE hkSimdFloat32 lengthSquared4() const;
        HK_FORCE_INLINE hkSimdFloat32 lengthInverse3() const;
        HK_FORCE_INLINE hkSimdFloat32 lengthInverse4() const;
        HK_FORCE_INLINE void setMulSigns4(hkVector4fParameter a, hkVector4fParameter signs);
        HK_FORCE_INLINE void setMulSigns4(hkVector4fParameter a, hkSimdFloat32Parameter sharedSign);
        HK_FORCE_INLINE void setAdd4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setSub4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setSqrtInverse4(hkVector4fParameter v);
        HK_FORCE_INLINE void setSqrtInverse4_7BitAccuracy(hkVector4fParameter v);
        HK_FORCE_INLINE void setReciprocal3(hkVector4fParameter v);
        HK_FORCE_INLINE void setReciprocal4(hkVector4fParameter v);
        HK_FORCE_INLINE void setAddMul4(hkVector4fParameter a, hkVector4fParameter x, hkVector4fParameter y);
        HK_FORCE_INLINE void setAddMul4(hkVector4fParameter a, hkVector4fParameter b, hkSimdFloat32Parameter r);
        HK_FORCE_INLINE void setAbs4(hkVector4fParameter v);
        HK_FORCE_INLINE void setMin4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE void setMax4(hkVector4fParameter a, hkVector4fParameter b);
        HK_FORCE_INLINE hkBool32 equals3(const hkVector4f &a, hkFloat32 epsilon = 1e-3f ) const;
        HK_FORCE_INLINE hkBool32 equals4(const hkVector4f &a, hkFloat32 epsilon = 1e-3f ) const;
        HK_FORCE_INLINE hkVector4fComparison compareEqual4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkVector4fComparison compareLessThan4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkVector4fComparison compareLessThanEqual4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkVector4fComparison compareGreaterThan4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkVector4fComparison compareGreaterThanEqual4(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkVector4fComparison compareLessThanZero4() const;
        HK_FORCE_INLINE hkBool32 allLessThan3(hkVector4fParameter a) const;
        HK_FORCE_INLINE hkBool32 allLessThan4(hkVector4fParameter a) const;
        HK_FORCE_INLINE void setInterpolate4( hkVector4fParameter a, hkVector4fParameter b, hkSimdFloat32Parameter t );
        HK_FORCE_INLINE hkSimdFloat32 distanceTo3( hkVector4fParameter p ) const;
        HK_FORCE_INLINE hkSimdFloat32 distanceToSquared3( hkVector4fParameter p ) const;
        HK_FORCE_INLINE hkSimdFloat32 horizontalAdd3() const;
        HK_FORCE_INLINE void setHorizontalMax4( hkVector4fParameter p);
        HK_FORCE_INLINE hkSimdFloat32 getHorizontalMin3() const;
        HK_FORCE_INLINE hkSimdFloat32 getHorizontalMax3() const;
        HK_FORCE_INLINE void _setMul4xyz1(const hkTransformf& a, hkVector4fParameter b );
        HK_FORCE_INLINE void add3clobberW(hkVector4fParameter a);
        template <int S> HK_FORCE_INLINE void setShuffle(hkVector4fParameter v);
        HK_FORCE_INLINE hkBool isOk3() const;
        HK_FORCE_INLINE hkBool isOk4() const;
        HK_FORCE_INLINE const hkVector4fComparison isNegative() const;
        HK_FORCE_INLINE const hkVector4fComparison isPositive() const;
        HK_FORCE_INLINE void load3(const hkFloat32* p);
        HK_FORCE_INLINE void load4(const hkFloat32* p);
        HK_FORCE_INLINE void load4a(const hkFloat32* p);
        HK_FORCE_INLINE void store3(hkFloat32* p) const;
        HK_FORCE_INLINE void store4(hkFloat32* p) const;
        HK_FORCE_INLINE void store4a(hkFloat32* p) const;
        HK_FORCE_INLINE void storeX( hkFloat32* dest) const;
        HK_FORCE_INLINE void load3(const hkDouble64* p);
        HK_FORCE_INLINE void load4(const hkDouble64* p);
        HK_FORCE_INLINE void load4a(const hkDouble64* p);
        HK_FORCE_INLINE void store3(hkDouble64* p) const;
        HK_FORCE_INLINE void store4(hkDouble64* p) const;
        HK_FORCE_INLINE void store4a(hkDouble64* p) const;
        HK_FORCE_INLINE void storeX( hkDouble64* dest) const;
        HK_FORCE_INLINE hkSimdFloat32 horizontalAdd4() const;
        HK_FORCE_INLINE void setClamped(hkVector4fParameter vSrc, const hkSimdFloat32& maxLen);
        HK_FORCE_INLINE    int getMaxElementIndex4() const;
        HK_FORCE_INLINE    int getMajorAxis3() const;
        HK_FORCE_INLINE void storeUncached( void* dest) const;
        HK_FORCE_INLINE void _setMul4(const hkMatrix3f& a, hkVector4fParameter b );
        HK_FORCE_INLINE void sub3clobberW(hkVector4fParameter a);
        HK_FORCE_INLINE void setAnd( hkVector4fParameter v0, hkVector4fParameter v1 );

#endif

};

#endif //HK_MATH_VECTOR4f_H
