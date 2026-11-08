// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkQsTransformf functions of the implementation closure, from
// rfg.exe.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h"

// 0x01A354D0 (stub_impl.md S4 SG; hkQsTransformf::getIdentity reads it): translation 0, rotation (0,0,0,1), scale 1
alignas(16) hkFloat32 hkQsTransformf_identityStorage[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                                          1.0f, 1.0f, 1.0f, 1.0f};

// 0x00FB6D30
void hkQsTransformf::setFromTransformNoScale(const hkTransformf& transform)
{
    m_rotation.set(transform.getRotation());
    m_translation = transform.getTranslation();
    m_scale = hkVector4f::getConstant<HK_QUADREAL_1>();
}

// 0x00FB6D60
void hkQsTransformf::copyToTransformNoScale(hkTransformf& transformOut) const
{
    transformOut.getRotation().set(m_rotation);
    transformOut.setTranslation(m_translation);
}

// 0x00FB71A0
// False when the matrix has skew, which this transform cannot hold.
bool hkQsTransformf::set4x4ColumnMajor(const hkFloat32* p)
{
    hkMatrixfDecomposition::Decomposition decomposition;
    hkMatrixfDecomposition::decompose4x4ColTransform(p, decomposition);
    m_translation = decomposition.m_translation;
    m_rotation = decomposition.m_rotation;
    m_scale = decomposition.m_scale;
    return !decomposition.m_hasSkew;
}

// 0x00FB7240
// Only a JMP to set4x4ColumnMajor in the binary: an hkTransformf is 16 floats, column major.
void hkQsTransformf::setFromTransform(const hkTransformf& transform)
{
    set4x4ColumnMajor(reinterpret_cast<const hkFloat32*>(&transform));
}
