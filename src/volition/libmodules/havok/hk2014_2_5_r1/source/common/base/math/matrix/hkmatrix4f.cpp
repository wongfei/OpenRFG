// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.cpp
//
// Havok 2014.2.5: the out-of-line hkMatrix4f functions the port reaches, from rfg.exe. The inline ones are in
// hkmatrix4f.inl.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.h"

// 0x00FC37E0
// MOVAPS in the binary: p must be aligned to 16 bytes.
void hkMatrix4f::set4x4ColumnMajor(const hkFloat32* p)
{
    m_col0.m_quad = _mm_load_ps(p);
    m_col1.m_quad = _mm_load_ps(p + 4);
    m_col2.m_quad = _mm_load_ps(p + 8);
    m_col3.m_quad = _mm_load_ps(p + 12);
}
