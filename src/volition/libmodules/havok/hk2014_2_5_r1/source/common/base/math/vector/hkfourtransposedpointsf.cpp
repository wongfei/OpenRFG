// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkFourTransposedPointsf functions of the implementation
// closure, from rfg.exe (the inline ones are in hkfourtransposedpointsf.inl).
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

// 0x00FBB1A0
// Back to one vertex per vector, with the vertex index in the w bits (hkcdVertex::getId). The output is padded to a
// multiple of four with copies of the last vertex.
void HK_CALL hkFourTransposedPointsf::getOriginalVertices(const hkFourTransposedPointsf* verts4in, int numVertices, hkcdVertex* verticesOut)
{
    hkIntVector index = hkIntVector::getConstant<HK_QUADINT_0123>();
    const hkIntVector four = hkIntVector::getConstant<HK_QUADINT_4>();
    const int numBlocks = int(unsigned(numVertices + 3) >> 2);
    for (int i = 0; i < numBlocks; i++)
    {
        hkcdVertex* v = verticesOut + i * 4;
        v[0].m_quad = verts4in[i].m_vertices[0].m_quad;
        v[1].m_quad = verts4in[i].m_vertices[1].m_quad;
        v[2].m_quad = verts4in[i].m_vertices[2].m_quad;
        v[3].m_quad = _mm_castsi128_ps(index.m_quad);
        HK_TRANSPOSE4f(v[0], v[1], v[2], v[3]);
        index.setAddU32(index, four);
    }
    for (int i = numVertices; i < numBlocks * 4; i++)
    {
        verticesOut[i].m_quad = verticesOut[numVertices - 1].m_quad;
    }
}

// 0x00FBB040
void hkFourTransposedPointsf::setTransformedInverseDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v)
{
    _setTransformedInverseDir(m, v);
}

// 0x00FBB100
void hkFourTransposedPointsf::setRotatedDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v)
{
    _setRotatedDir(m, v);
}

// 0x00FBB280
void hkFourTransposedPointsf::setTransformedInversePos(const hkTransformf& m, const hkFourTransposedPointsf& v)
{
    _setTransformedInversePos(m, v);
}
