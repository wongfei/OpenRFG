// Original: volition/rfg/code/havok/havok_mesh_shape.cpp
//
// havok_mesh_shape, the extended mesh shape of the stitch pieces (.cstch_pc): one embedded triangles subpart whose
// vertex and index buffers are the shape's own vertex and triangle arrays (16-bit indices). Only loaded from
// tagfiles, so the only constructor is the finishing one. Compiled in rfg_havok_unity.cpp; the reflection
// (staticClass, Members) is generated into reflections/havok_mesh_shapereflection.cpp (stub_impl.md S4 SB).

#include "volition/rfg/code/havok/havok_mesh_shape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include "keen/base/trace.hpp"

// 0x0081C6B0 (:661)
// Every triangle is a child shape.
int havok_mesh_shape::getNumChildShapes() const
{
    return m_triangles.getSize();
}

// 0x00811AD0 (:670)
// Keys are never walked in game.
unsigned int havok_mesh_shape::getFirstKey() const
{
    keen::debug::nativePrintArgs("This can't be called in game!");
    return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
}

// 0x00811AF0 (:698)
unsigned int havok_mesh_shape::getNextKey(unsigned int old_key) const
{
    keen::debug::nativePrintArgs("This can't be called in game!");
    return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
}

// 0x00831710 (.h:82)
// Points the embedded subpart at the loaded vertex and triangle arrays.
havok_mesh_shape::havok_mesh_shape(hkFinishLoadedObjectFlag flag)
    : hkpExtendedMeshShape(flag)
    , m_vertices(flag)
    , m_triangles(flag)
{
    if (flag.m_finishing != 0)
    {
        m_embeddedTrianglesSubpart.m_vertexBase = &m_vertices.m_data->m_x;
        m_embeddedTrianglesSubpart.m_indexBase = m_triangles.m_data;
    }
}

// 0x008317A0
havok_mesh_shape::~havok_mesh_shape()
{
}
