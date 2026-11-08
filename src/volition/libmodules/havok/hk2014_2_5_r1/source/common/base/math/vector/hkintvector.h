// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkIntVector.h), checked against rfg.exe.
#ifndef HK_MATH_INTVECTOR_H
#define HK_MATH_INTVECTOR_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

namespace hkCompileError
{
    template <bool b> struct HK_INT_VECTOR_SUBVECTOR_INDEX_OUT_OF_RANGE;
    template <> struct HK_INT_VECTOR_SUBVECTOR_INDEX_OUT_OF_RANGE<true>{ };
    template <bool b> struct HK_INT_VECTOR_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH;
    template <> struct HK_INT_VECTOR_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH<true>{ };
    template <bool b> struct HK_INT_VECTOR_UNSUPPORTED_VECTOR_LENGTH;
    template <> struct HK_INT_VECTOR_UNSUPPORTED_VECTOR_LENGTH<true>{ };
    template <bool b> struct HK_INT_VECTOR_ILLEGAL_VALUE_FOR_IMM_SPLAT;
    template <> struct HK_INT_VECTOR_ILLEGAL_VALUE_FOR_IMM_SPLAT<true>{ };
    template <bool b> struct HK_INT_VECTOR_UNKNOWN_CONSTANT;
    template <> struct HK_INT_VECTOR_UNKNOWN_CONSTANT<true>{ };
}
#define HK_INT_VECTOR_SUBINDEX_CHECK HK_COMPILE_TIME_ASSERT2((I>=0)&&(I<4), HK_INT_VECTOR_SUBVECTOR_INDEX_OUT_OF_RANGE)
#define HK_INT_VECTOR_NOT_IMPLEMENTED HK_COMPILE_TIME_ASSERT2(N==0, HK_INT_VECTOR_NOT_IMPLEMENTED_FOR_THIS_VECTOR_LENGTH)
#define HK_INT_VECTOR_UNSUPPORTED_LENGTH_CHECK HK_COMPILE_TIME_ASSERT2((N>0)&&(N<=4), HK_INT_VECTOR_UNSUPPORTED_VECTOR_LENGTH)
#define HK_INT_VECTOR_IMM_SPLAT_VALUE_CHECK HK_COMPILE_TIME_ASSERT2((VALUE>=-16)&&(VALUE<=15), HK_INT_VECTOR_ILLEGAL_VALUE_FOR_IMM_SPLAT)
#define HK_INT_VECTOR_CONSTANT_CHECK           HK_COMPILE_TIME_ASSERT2(((vectorConstant>HK_QUADINT_BEGIN)&&(vectorConstant<HK_QUADINT_END)),HK_INT_VECTOR_UNKNOWN_CONSTANT)

// Components that aren't loaded during hkIntVector::load() are initialized to this in Debug and Dev configurations.
#define HK_INT_VECTOR_DEBUG_FILL_VALUE (g_intVectorConstants[HK_QUADINT_BEGIN][0])

/*    Endianness, and words such as "left", "right", "head", "tail":

    hkIntVector aims to provide a compatible interface on all platforms, regardless of endian-ness,
    even though some vector units (Xbox360, PS3, other Altivec-based) are big-endian and some (x86)
    are little-endian.

    The hkIntVector API is roughly big-endian (except for type conversions, see below). The elements
    of a vector are referred to by the position in memory they'd have if they were stored out. The
    "left" side of a vector is the one loaded and stored to the *lower* memory address, and the "right"
    side is the one loaded and stored to the *higher* memory address. The "head" is the leftmost half,
    and the "tail" is the rightmost half.

    So, if an aligned array of 8 16-bit numbers is loaded into a vector:
        - the i'th element is array[i]
        - the leftmost element is array[0]
        - the rightmost element in array[7]
        - the head of the vector is array[0..3]
        - the tail of the vector is array[4..7]
        - shifting the entire vector left 32 bits will move array[i] to array[i-2]
          and put zeros in array[6] and array[7].

    Type conversions are the one place where endianness leaks through. If an operation is done on a
    vector as a vector of 32-bit ints, and later operations are done as if it were a vector of 16-bit
    ints, it is platform-dependant which order the 16-bit ints will appear. When doing operations using
    multiple different types for the same vector, you should use the setConvert* and setCombine*
    operations to typecast, as they get the correct byteorder on all platforms.
*/

HK_PASS_IN_REG
class hkIntVector
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkIntVector);
        HK_FORCE_INLINE void setZero();
        template <int I> HK_FORCE_INLINE void zeroComponent32();

        // U32
        HK_FORCE_INLINE void setAll(const int& i);

        // Splat Immediate must be -16 >= VALUE <= 15
        template <int VALUE> HK_FORCE_INLINE void splatImmediate32();
        template <int VALUE> HK_FORCE_INLINE void splatImmediate16();
        template <int VALUE> HK_FORCE_INLINE void splatImmediate8();

        // other components = 0 U32
        HK_FORCE_INLINE void setFirstComponent(int value);

        // other components untouched U32
        template <int I> HK_FORCE_INLINE void setComponent(int value);
        HK_FORCE_INLINE void setComponent(int componentIdx, int value);

        // U32
        HK_FORCE_INLINE void set(int x, int y, int z, int w);

        // alias getU32
        HK_FORCE_INLINE int getComponent(int I) const;
        template <int I> HK_FORCE_INLINE int getComponent() const;
        template <int N> HK_FORCE_INLINE void load(const hkUint32* p);
        template <int N> HK_FORCE_INLINE void loadNotAligned(const hkUint32* p);
        template <int N, hkMathIoMode IO > HK_FORCE_INLINE void load(const hkUint32* p);
        template <int N> HK_FORCE_INLINE void store(hkUint32* p) const;
        template <int N> HK_FORCE_INLINE void store(hkInt32* p) const { store<N>((hkUint32*)p); }
        template <int N> HK_FORCE_INLINE void storeNotAligned(hkUint32* p) const;
        template <int N, hkMathIoMode A> HK_FORCE_INLINE void store(hkUint32* p) const;
        HK_FORCE_INLINE void loadAsFloat32BitRepresentation(const hkVector4f& x);
        HK_FORCE_INLINE void storeAsFloat32BitRepresentation( hkVector4f& out) const;
        HK_FORCE_INLINE void storeInto24LowerBitsOfReal(hkVector4f& vOut);

        // U32
        template <int I> HK_FORCE_INLINE void setBroadcast(hkIntVectorParameter v);
        template<int I> HK_FORCE_INLINE void setBroadcast16(hkIntVectorParameter v);
        template<int I> HK_FORCE_INLINE void setBroadcast8(hkIntVectorParameter v);

            // Logical Ops
        HK_FORCE_INLINE void setNot( hkIntVectorParameter a );
        HK_FORCE_INLINE void setOr( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAnd( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setXor( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAndNot( hkIntVectorParameter a, hkIntVectorParameter b );

            // Comparison.

        // sign bit compare S32
        HK_FORCE_INLINE hkBool32 isNegativeAssumingAllValuesEqual(  ) const;

        // value compare
        HK_FORCE_INLINE const hkVector4Comparison compareLessThanS32(hkIntVectorParameter b) const;
        HK_FORCE_INLINE const hkVector4Comparison compareEqualS32(hkIntVectorParameter b) const;
        HK_FORCE_INLINE const hkVector4Comparison lessZeroS32() const;
        HK_FORCE_INLINE const hkVector4Comparison greaterZeroS32() const;
        HK_FORCE_INLINE const hkVector4Comparison equalZeroS32() const;
        HK_FORCE_INLINE const hkVector4Comparison greaterEqualS32(hkIntVectorParameter b) const;
        HK_FORCE_INLINE const hkVector4fComparison equalS32(hkIntVectorParameter b) const;

        // Will implement these as needed. There are potentially a LOT of functions to implement here
        //HK_FORCE_INLINE void setCompareEqual(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b );
        //HK_FORCE_INLINE void setCompareLessThan(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b );
        //HK_FORCE_INLINE void setCompareLessThanEqual(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b );
        //HK_FORCE_INLINE void setCompareGreaterThan(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b );
        //HK_FORCE_INLINE void setCompareGreaterThanEqual(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b );

        HK_FORCE_INLINE void setCompareGreaterS16(hkIntVectorParameter a, hkIntVectorParameter b);

            // Integer arithmetic
        HK_FORCE_INLINE void setSubU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddU32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubU32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddU64( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubU64( hkIntVectorParameter a, hkIntVectorParameter b );

        // Will implement these as needed.
        //HK_FORCE_INLINE void setAddSaturate(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b )
        //HK_FORCE_INLINE void setSubSaturate(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b )
        HK_FORCE_INLINE void setAddSaturateU32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddSaturateS16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddSaturateU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubSaturateU32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubSaturateS16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubSaturateU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setAddS32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setSubS32( hkIntVectorParameter a, hkIntVectorParameter b );

            // Min / Max
        //HK_FORCE_INLINE void setMin(S/U)(32/16/8)( hkIntVectorParameter a, hkIntVectorParameter b )
        HK_FORCE_INLINE void setMinU8( hkIntVectorParameter a, hkIntVectorParameter b ) ;
        HK_FORCE_INLINE void setMinS32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMaxS32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMinS16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMaxS16( hkIntVectorParameter a, hkIntVectorParameter b );

            // Logical Rotate / Shift
        //HK_FORCE_INLINE void setShiftLeft(32/16/8)( hkIntVectorParameter a, int shift );
        //HK_FORCE_INLINE void setShiftRight(32/16/8)( hkIntVectorParameter a, int shift );
        //HK_FORCE_INLINE void setRotateLeft(32/16/8)( hkIntVectorParameter a, int shift );
        //HK_FORCE_INLINE void setRotateRight(32/16/8)( hkIntVectorParameter a, int shift );

        // immediate bit shift
        template <int shift> HK_FORCE_INLINE void setShiftLeft16( hkIntVectorParameter a );
        template <int shift> HK_FORCE_INLINE void setShiftRight16( hkIntVectorParameter a );
        template <int shift> HK_FORCE_INLINE void setShiftLeft32( hkIntVectorParameter a );
        template <int shift> HK_FORCE_INLINE void setShiftRight32( hkIntVectorParameter a );
        template <int shift> HK_FORCE_INLINE void setShiftLeft128( hkIntVectorParameter a );
        template <int shift> HK_FORCE_INLINE void setShiftRight128( hkIntVectorParameter a );
        template<int shift> HK_FORCE_INLINE void setShiftRightS32( hkIntVectorParameter a );
        template<int shift> HK_FORCE_INLINE void setShiftRightS16( hkIntVectorParameter a );

        // shift by an integer
        HK_FORCE_INLINE void setShiftLeft16( hkIntVectorParameter a, int shift);
        HK_FORCE_INLINE void setShiftLeft32( hkIntVectorParameter a, int shift);
        HK_FORCE_INLINE void setShiftRight16( hkIntVectorParameter a, int shift);
        HK_FORCE_INLINE void setShiftRight32( hkIntVectorParameter a, int shift);
        HK_FORCE_INLINE void setBitShiftLeft128(hkIntVectorParameter a, hkIntVectorParameter shiftSplat);
        HK_FORCE_INLINE void setBitShiftRight128(hkIntVectorParameter a, hkIntVectorParameter shiftSplat);

        // different shifts per component
         HK_FORCE_INLINE void setShiftLeft16( hkIntVectorParameter a, hkIntVectorParameter shift );
        HK_FORCE_INLINE void setShiftRightS16( hkIntVectorParameter a, hkIntVectorParameter shift );
        HK_FORCE_INLINE void setShiftLeft32( hkIntVectorParameter a, hkIntVectorParameter shift );
        HK_FORCE_INLINE void setShiftRight32( hkIntVectorParameter a, hkIntVectorParameter shift );

        // Merge

        HK_FORCE_INLINE void setMergeHead32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMergeTail32( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMergeHead16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMergeTail16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMergeHead8( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setMergeTail8( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setCombineHead16To32( hkIntVectorParameter highShorts, hkIntVectorParameter lowShorts );
        HK_FORCE_INLINE void setCombineTail16To32( hkIntVectorParameter highShorts, hkIntVectorParameter lowShorts );
        HK_FORCE_INLINE void setCombineHead8To16( hkIntVectorParameter highShorts, hkIntVectorParameter lowShorts );
        HK_FORCE_INLINE void setCombineTail8To16( hkIntVectorParameter highShorts, hkIntVectorParameter lowShorts );
        HK_FORCE_INLINE void setCombine16To32( hkIntVectorParameter a );
        HK_FORCE_INLINE void setCombine8To16( hkIntVectorParameter a );
        HK_FORCE_INLINE void setSplit8To32( hkIntVectorParameter bytes );
        HK_FORCE_INLINE void setConvertU32ToU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setConvertLowerU16ToU32(hkIntVectorParameter a);
        HK_FORCE_INLINE void setConvertUpperU16ToU32(hkIntVectorParameter a);
        HK_FORCE_INLINE void setConvertHeadU16ToU32( hkIntVectorParameter a );
        HK_FORCE_INLINE void setConvertSaturateS32ToU16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setConvertSaturateS32ToS16( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setConvertSaturateS16ToU8( hkIntVectorParameter a, hkIntVectorParameter b );
        HK_FORCE_INLINE void setConvertHeadS16ToS32( hkIntVectorParameter a    );
        HK_FORCE_INLINE void setConvertTailU16ToU32( hkIntVectorParameter a );
        HK_FORCE_INLINE void setConvertTailS16ToS32( hkIntVectorParameter a    );
        HK_FORCE_INLINE void setSignExtendS16ToS32( hkIntVectorParameter a );

        // U32
        template <hkVectorPermutation::Permutation P> HK_FORCE_INLINE void setPermutation(hkIntVectorParameter v);
        template<unsigned int i, unsigned int j, unsigned int k, unsigned int l>
        HK_FORCE_INLINE void setPermutation2(hkIntVectorParameter a, hkIntVectorParameter b);

        // U32
        HK_FORCE_INLINE void setSelect( hkVector4fComparisonParameter select, hkIntVectorParameter trueValue, hkIntVectorParameter falseValue );
        template<hkVector4ComparisonMask::Mask M> HK_FORCE_INLINE void setSelect( hkIntVectorParameter trueValue, hkIntVectorParameter falseValue );
        HK_FORCE_INLINE void setPermuteU8(hkIntVectorParameter a, hkIntVectorParameter mask);
        HK_FORCE_INLINE void setPermuteU8(hkIntVectorParameter a, hkIntVectorParameter b, hkIntVectorParameter mask);

            // Convert to from fixed point
        HK_FORCE_INLINE void convertU32ToF32( hkVector4f& vOut ) const;
        HK_FORCE_INLINE void convertS32ToF32( hkVector4f& vOut ) const;
        HK_FORCE_INLINE void setConvertF32toU32( hkVector4fParameter vIn );
        HK_FORCE_INLINE void setConvertF32toS32( hkVector4fParameter vIn );
        template <int I> HK_FORCE_INLINE hkUint8  getU8 () const;
        template <int I> HK_FORCE_INLINE hkUint16 getU16() const;
        template <int I> HK_FORCE_INLINE hkUint32 getU32() const;
        HK_FORCE_INLINE hkUint32 getU32(int idx) const; // todo deprecate
        HK_FORCE_INLINE void broadcastComponentAtVectorMax(hkVector4fParameter v);
        HK_FORCE_INLINE int getComponentAtVectorMax(hkVector4fParameter v) const;
        HK_FORCE_INLINE int getFirstComponentAtVectorMax(hkVector4fParameter v) const;
        template<hkIntVectorConstant vectorConstant>
        HK_FORCE_INLINE static const hkIntVector& HK_CALL getConstant();
        HK_FORCE_INLINE static const hkIntVector& HK_CALL getConstant(hkIntVectorConstant constant);
        HK_FORCE_INLINE void setAbsS32(hkIntVectorParameter v);
        HK_FORCE_INLINE void setFlipSignS32(hkIntVectorParameter v, hkVector4ComparisonParameter mask);
        template <int I>
        HK_FORCE_INLINE void setNegS32(hkIntVectorParameter v);
        template <int N> HK_FORCE_INLINE int horizontalMinS32() const;
        template <int N> HK_FORCE_INLINE int horizontalMaxS32() const;
        template <int N> HK_FORCE_INLINE void setHorizontalMaxS32(hkIntVectorParameter v);
        template <int N> HK_FORCE_INLINE int getIndexOfMinComponent() const;
        template <int N, hkMathSortDir dir>
        HK_FORCE_INLINE void setSortS32(hkIntVectorParameter v);
        template <int N>
        HK_FORCE_INLINE hkInt64 dot(hkIntVectorParameter v) const;
        template <int N>
        HK_FORCE_INLINE int horizontalAddS32() const;
        template <int N>
        HK_FORCE_INLINE int horizontalXorS32() const;
        template <int N>
        HK_FORCE_INLINE int horizontalOrS32() const;
        HK_FORCE_INLINE void setMul(hkIntVectorParameter vA, hkIntVectorParameter vB);

            // Add versions with scaling if needed?

     public:

        HK_ALIGN16( hkQuadUint m_quad );
};

union hkIntVectorConstantU32_
{
    hkQuadUintUnion q;
    hkIntVector v;
};
typedef const hkIntVectorConstantU32_ hkIntVectorConstantU32;
union hkIntVectorConstantU16_
{
    hkQuadUshortUnion q;
    hkIntVector v;
};
typedef const hkIntVectorConstantU16_ hkIntVectorConstantU16;
union hkIntVectorConstantU8_
{
    hkQuadUcharUnion q;
    hkIntVector v;
};
typedef const hkIntVectorConstantU8_ hkIntVectorConstantU8;

#define HK_INT_VECTOR_CONSTANT_U32(a,b,c,d) {{{a,b,c,d}}}
#define HK_INT_VECTOR_CONSTANT_U16(a,b,c,d,e,f,g,h) {{{a,b,c,d,e,f,g,h}}}
#define HK_INT_VECTOR_CONSTANT_U8(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) {{{a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p}}}

typedef hkIntVectorConstantU8 hkIntVectorPermutation;

#define HK_INT_VECTOR_PERMUTATION_U8(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15)\
    HK_INT_VECTOR_CONSTANT_U8(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15)

#define HK_INT_VECTOR_PERMUTATION_U16(p0, p1, p2, p3, p4, p5, p6, p7)\
    HK_INT_VECTOR_CONSTANT_U8((p0)*2,(p0)*2+1,(p1)*2,(p1)*2+1,\
                              (p2)*2,(p2)*2+1,(p3)*2,(p3)*2+1,\
                              (p4)*2,(p4)*2+1,(p5)*2,(p5)*2+1,\
                              (p6)*2,(p6)*2+1,(p7)*2,(p7)*2+1)
#define HK_INT_VECTOR_PERMUTATION_U32(p0, p1, p2, p3)\
    HK_INT_VECTOR_CONSTANT_U8((p0)*4, (p0)*4+1, (p0)*4+2, (p0)*4+3,\
                              (p1)*4, (p1)*4+1, (p1)*4+2, (p1)*4+3,\
                              (p2)*4, (p2)*4+1, (p2)*4+2, (p2)*4+3,\
                              (p3)*4, (p3)*4+1, (p3)*4+2, (p3)*4+3)

#    if HK_CONFIG_SIMD == HK_CONFIG_SIMD_ENABLED
#        if defined(HK_COMPILER_HAS_INTRINSICS_IA32)
#            include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/sse/hksseintvector.inl"
#        elif defined(HK_COMPILER_HAS_INTRINSICS_NEON)
#            include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/neon/hkneonintvector.inl"
#        elif defined(HK_PLATFORM_PS3_PPU) || defined(HK_PLATFORM_PS3_SPU)
#            include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/ps3/hkps3intvector.inl"
#        elif defined(HK_PLATFORM_XBOX360)
#            include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/xbox360/hkxbox360intvector.inl"
#        else
#            define HK_USING_GENERIC_INT_VECTOR_IMPLEMENTATION
#        endif
#    else
#        define HK_USING_GENERIC_INT_VECTOR_IMPLEMENTATION
#    endif
#    include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.inl"

#endif //HK_MATH_INTVECTOR_H
