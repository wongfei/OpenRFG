// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkstridedvertices.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class hkVector4f;

// size 0xC
struct hkStridedVertices
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStridedVertices);
    hkStridedVertices(const hkVector4f*, int);
    hkStridedVertices(const hkArrayBase<hkVector4f>& vertices); // 0x00F1F270
    hkStridedVertices(); // 0x0080F2B0 (.h:28)
    int getSize() const; // 0x00F20850
    void getVertex(int, hkVector4f&) const;
    void set(const hkArrayBase<hkVector4f>& vertices); // 0x00F1EF60

    const float* m_vertices; // 0x00
    int m_numVertices; // 0x04
    int m_striding; // 0x08
};
