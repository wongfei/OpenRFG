// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line copies of hkVector4f's transform helpers. As the SDK's
// header says, each calls the forced-inline "_" version (hkvector4f.inl, hkssevector4.inl); rfg.exe's code is the
// same instruction sequence (hkdiff compares them bit for bit).
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// 0x00CF00F0
void hkVector4f::setRotatedDir(const hkMatrix3f& a, hkVector4fParameter b)
{
    _setRotatedDir(a, b);
}

// 0x00CF0130
void hkVector4f::setRotatedInverseDir(const hkMatrix3f& a, hkVector4fParameter b)
{
    _setRotatedInverseDir(a, b);
}

// 0x00CF01E0
void hkVector4f::setTransformedPos(const hkTransformf& a, hkVector4fParameter b)
{
    _setTransformedPos(a, b);
}

// 0x00CF0220
void hkVector4f::setTransformedInversePos(const hkTransformf& a, hkVector4fParameter b)
{
    _setTransformedInversePos(a, b);
}

// 0x00CF0560
void hkVector4f::setRotatedDir(hkQuaternionfParameter quat, hkVector4fParameter direction)
{
    _setRotatedDir(quat, direction);
}

// 0x00CF05E0
void hkVector4f::setRotatedInverseDir(hkQuaternionfParameter quat, hkVector4fParameter direction)
{
    _setRotatedInverseDir(quat, direction);
}

// 0x00CF02D0
void hkVector4f::setTransformedPos(const hkQsTransformf& a, hkVector4fParameter b)
{
    _setTransformedPos(a, b);
}

// 0x00CF0370
void hkVector4f::setTransformedInversePos(const hkQsTransformf& a, hkVector4fParameter b)
{
    _setTransformedInversePos(a, b);
}

// 0x00CF0420
void hkVector4f::setTransformedPos(const hkQTransformf& a, hkVector4fParameter b)
{
    _setTransformedPos(a, b);
}

// 0x00CF04C0
void hkVector4f::setTransformedInversePos(const hkQTransformf& a, hkVector4fParameter b)
{
    _setTransformedInversePos(a, b);
}
