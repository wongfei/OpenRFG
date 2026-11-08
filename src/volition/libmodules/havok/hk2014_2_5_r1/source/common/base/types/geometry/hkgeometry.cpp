// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkgeometry.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): a triangle mesh (vertices and index triangles), from rfg.exe. Only the parts the
// inertia computers use (the triangle prism of computeTriangleSurfaceMassProperties).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkgeometry.h"

// 0x00D0BDA0 (stub_impl.md S4 SD, the type info's finish function): the arrays keep the loaded data
hkGeometry::hkGeometry(hkFinishLoadedObjectFlag f)
    : hkReferencedObject(f), m_vertices(f), m_triangles(f)
{
}

// 0x00EDD9F0
hkGeometry::hkGeometry()
{
}

// 0x00D0C630: the triangles are freed before the vertices
hkGeometry::~hkGeometry()
{
}
