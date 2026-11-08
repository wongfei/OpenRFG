// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkTransformf functions of the implementation closure, from
// rfg.exe.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"

// 0x00CF7F00
void hkTransformf::setInverse(const hkTransformf& t)
{
    _setInverse(t);
}

// 0x00CF8550
// this = aTb * bTc. aTb's rotation is copied first; the translation of aTb is read after the columns are written
// (as in the binary).
void hkTransformf::setMul(const hkTransformf& aTb, const hkTransformf& bTc)
{
    const hkRotationf r = aTb.getRotation();
    for (int i = 3; i >= 0; i--)
    {
        getColumn(i)._setRotatedDir(r, bTc.getColumn(i));
    }
    m_translation.add(aTb.getTranslation());
}

// 0x00CF8100
// this = inverse(bTa) * bTc. Not the inline _setMulInverseMul (0x01043590, which goes from the last column down and
// rereads bTa's rotation for each): the transposed rotation is taken once, the rotation columns go up, and the
// translation is rotated last.
void hkTransformf::setMulInverseMul(const hkTransformf& bTa, const hkTransformf& bTc)
{
    hkMatrix3f aT;
    aT._setTranspose(bTa.getRotation());
    for (int i = 0; i < 3; i++)
    {
        const hkVector4f c = bTc.getColumn(i);
        getColumn(i)._setRotatedDir(aT, c);
    }
    hkVector4f h;
    h.setSub(bTc.getTranslation(), bTa.getTranslation());
    m_translation._setRotatedInverseDir(bTa.getRotation(), h);
}
