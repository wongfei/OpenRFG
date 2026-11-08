// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkVector4fComparison.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_VECTOR4f_COMPARISON_H)
#define HK_MATH_VECTOR4f_COMPARISON_H

// compile time error checking
namespace hkCompileError
{
    template <bool b> struct HK_VECTORfCOMPARISON_ILLEGAL_MASK_VALUE;
    template <> struct HK_VECTORfCOMPARISON_ILLEGAL_MASK_VALUE<true> {};
    template <bool b> struct HK_VECTORfCOMPARISON_SUBINDEX_OUT_OF_RANGE;
    template <> struct HK_VECTORfCOMPARISON_SUBINDEX_OUT_OF_RANGE<true> {};
}
#define HK_VECTORfCOMPARISON_MASK_CHECK           HK_COMPILE_TIME_ASSERT2((M&0xf)==M, HK_VECTORfCOMPARISON_ILLEGAL_MASK_VALUE)
#define HK_VECTORfCOMPARISON_SUBINDEX_CHECK         HK_COMPILE_TIME_ASSERT2((N>0)&&(N<=4), HK_VECTORfCOMPARISON_SUBINDEX_OUT_OF_RANGE);

HK_PASS_IN_REG
class hkVector4fComparison
{
public:

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkVector4fComparison);
    typedef hkVector4ComparisonMask::Mask Mask;
    HK_FORCE_INLINE static const hkVector4fComparison HK_CALL convert(const hkVector4fMask& x); // 0x0080C6D0 (hkssevector4comparison.inl:31)
    HK_FORCE_INLINE static Mask HK_CALL getMaskForComponent(int i); // 0x00D74700
    HK_FORCE_INLINE int getIndexOfLastComponentSet() const; // 0x00CEEB10
    HK_FORCE_INLINE int getIndexOfFirstComponentSet() const; // 0x00D517A0
    HK_FORCE_INLINE void setAnd( hkVector4fComparisonParameter a, hkVector4fComparisonParameter b ); // 0x00CEE940
    HK_FORCE_INLINE void setAndNot( hkVector4fComparisonParameter a, hkVector4fComparisonParameter b ); // 0x00D35350
    HK_FORCE_INLINE void setXor( hkVector4fComparisonParameter a, hkVector4fComparisonParameter b ); // 0x00D58340
    HK_FORCE_INLINE void setOr( hkVector4fComparisonParameter a, hkVector4fComparisonParameter b ); // 0x00D35370
    HK_FORCE_INLINE void setNot( hkVector4fComparisonParameter a ); // 0x00DD7AC0
    HK_FORCE_INLINE void setSelect( hkVector4fComparisonParameter comp, hkVector4fComparisonParameter trueValue, hkVector4fComparisonParameter falseValue ); // 0x00D58360
    template <Mask M> HK_FORCE_INLINE void setSelect(hkVector4fComparisonParameter trueValue, hkVector4fComparisonParameter falseValue);
    HK_FORCE_INLINE void set( Mask m ); // 0x00CEE960
    template <Mask M> HK_FORCE_INLINE void set();
    HK_FORCE_INLINE hkBool32 allAreSet( Mask m ) const;
    template <Mask M> HK_FORCE_INLINE hkBool32 allAreSet() const;
    HK_FORCE_INLINE hkBool32 allAreSet() const; // 0x00D43CA0
    HK_FORCE_INLINE hkBool32 anyIsSet( Mask m ) const; // 0x00D746F0
    template <Mask M> HK_FORCE_INLINE hkBool32 anyIsSet() const;
    HK_FORCE_INLINE hkBool32 anyIsSet() const; // 0x0080C6E0 (hkssevector4comparison.inl:242)
    HK_FORCE_INLINE Mask getMask() const; // 0x00CEE980
    HK_FORCE_INLINE Mask getMask(Mask m) const;
    template <Mask M> HK_FORCE_INLINE Mask getMask() const;
    template <int N>
    HK_FORCE_INLINE const hkVector4fComparison horizontalOr() const;
    template <int N>
    HK_FORCE_INLINE const hkVector4fComparison horizontalAnd() const;
    static HK_FORCE_INLINE hkUint32 HK_CALL getCombinedMask(hkVector4fComparisonParameter ca, hkVector4fComparisonParameter cb, hkVector4fComparisonParameter cc );
    hkVector4fMask m_mask;
    public: // protected in Havok; public as the PDB-generated headers had it (Volition and the tests read the members)

};

extern const hkUint8 hkVector4Comparison_maskToFirstIndex[16]; ///< selection map to lowest index set
extern const hkUint8 hkVector4Comparison_maskToLastIndex[16];  ///< selection map to highest index set

#endif //HK_MATH_VECTOR4f_COMPARISON_H
