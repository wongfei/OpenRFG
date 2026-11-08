// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): a view of vertex positions (pointer, count, byte stride), with the inline
// functions of hkstridedvertices.h that have out-of-line copies.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// 0x0080F2B0 (.h:28)
hkStridedVertices::hkStridedVertices()
{
    m_numVertices = 0;
}

// 0x00F1F270 (.h:31)
hkStridedVertices::hkStridedVertices(const hkArrayBase<hkVector4f>& vertices)
{
    set(vertices);
}

// 0x00F20850
int hkStridedVertices::getSize() const
{
    return m_numVertices;
}

// 0x00F1EF60
void hkStridedVertices::set(const hkArrayBase<hkVector4f>& vertices)
{
    m_vertices = (const float*)vertices.m_data;
    m_numVertices = vertices.m_size;
    m_striding = sizeof(hkVector4f);
}
