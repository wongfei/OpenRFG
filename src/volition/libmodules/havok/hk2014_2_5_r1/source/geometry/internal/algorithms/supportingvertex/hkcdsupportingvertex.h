// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/supportingvertex/hkcdsupportingvertex.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the furthest vertex in a direction. Inline only (no PDB records); ported from
// Havok 2014.1 (hkcdSupportingVertex.h/.inl, the SSE path) and checked against the copy inlined in
// hkpConvexVerticesShape::getSupportingVertex (0x00D97930). Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

// The vertex (of numVertexBatches batches of four transposed points) furthest along the direction; ties go to the
// lower index. Its index is stored in w.
HK_FORCE_INLINE void hkcdSupportingVertexPointsTranposed(const hkFourTransposedPointsf* HK_RESTRICT transposedVertices, int numVertexBatches, hkVector4Parameter direction, hkcdVertex* HK_RESTRICT vertexOut)
{
    const hkSimdReal d0 = direction.getComponent<0>();
    const hkSimdReal d1 = direction.getComponent<1>();
    const hkSimdReal d2 = direction.getComponent<2>();

    hkVector4 bestDot;
    {
        hkVector4 x; x.setMul(d0, transposedVertices[0].m_vertices[0]);
        hkVector4 y; y.setMul(d1, transposedVertices[0].m_vertices[1]);
        bestDot.setAdd(x, y);
        hkVector4 z; z.setMul(d2, transposedVertices[0].m_vertices[2]);
        bestDot.add(z);
    }
    hkIntVector curIndices = hkIntVector::getConstant<HK_QUADINT_0123>();
    hkIntVector bestIndices = curIndices;

    // the best dots four at a time
    for (int i = 1; i < numVertexBatches; i++)
    {
        curIndices.setAddS32(curIndices, hkIntVector::getConstant<HK_QUADINT_4>());
        hkVector4 curDot;
        {
            hkVector4 x; x.setMul(d0, transposedVertices[i].m_vertices[0]);
            hkVector4 y; y.setMul(d1, transposedVertices[i].m_vertices[1]);
            curDot.setAdd(x, y);
            hkVector4 z; z.setMul(d2, transposedVertices[i].m_vertices[2]);
            curDot.add(z);
        }
        const hkVector4Comparison comp = curDot.greater(bestDot);
        bestDot.setSelect(comp, curDot, bestDot);
        bestIndices.setSelect(comp, curIndices, bestIndices);
    }

    // the best of the four
    int vertexId = bestIndices.getFirstComponentAtVectorMax(bestDot);
    {
        const hkFourTransposedPointsf* HK_RESTRICT fv = transposedVertices + (unsigned(vertexId) >> 2);
        int a = vertexId & 3;
        (*vertexOut)(0) = fv->m_vertices[0](a);
        (*vertexOut)(1) = fv->m_vertices[1](a);
        (*vertexOut)(2) = fv->m_vertices[2](a);
        (*vertexOut).setInt24W(vertexId);
    }
}
