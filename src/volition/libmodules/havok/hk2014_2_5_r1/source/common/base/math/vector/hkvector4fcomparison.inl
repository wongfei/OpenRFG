// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.inl
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkVector4fComparison.inl), checked against rfg.exe.
HK_FORCE_INLINE /*static*/ hkVector4ComparisonMask::Mask HK_CALL hkVector4fComparison::getMaskForComponent(int i)
{
    // note check compile time assert in .cpp file
    HK_MATH_ASSERT(0x2337f8c7, (i>=0) && (i<4), "Component index out of range" );
    return hkVector4ComparisonMask::Mask(1 << i);
}

HK_FORCE_INLINE int hkVector4fComparison::getIndexOfLastComponentSet() const
{
    return hkVector4Comparison_maskToLastIndex[getMask()];
}

HK_FORCE_INLINE int hkVector4fComparison::getIndexOfFirstComponentSet() const
{
    return hkVector4Comparison_maskToFirstIndex[getMask()];
}
