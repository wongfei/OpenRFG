// Original: volition/rfg/code/havok/havok_level_shape.cpp
//
// havok_navmesh_shape (stub_impl.md S4), from rfg.exe: a Havok shape collection over the navmesh cells of a path
// finding layer, one triangle child per cell (keyed by cell index). havok_create_navmesh_shape (havok.cpp) wraps it in
// a MOPP tree for the zone's navmesh collision model.

#include "volition/rfg/code/havok/havok_level_shape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"

#include <new>

// 0x00811860 (:46): the AABB of the layer's box (center and half extents, w 0)
havok_navmesh_shape::havok_navmesh_shape(const bb_pfg_layer* _pfg_data)
    : hkpShapeCollection(hkcdShapeType::COLLECTION, hkpShapeCollection::COLLECTION_USER)
{
    pfg_data = _pfg_data;
    const vector& mn = _pfg_data->bbox_min;
    const vector& mx = _pfg_data->bbox_max;
    m_hk_aabb_half_extents.m_quad = _mm_set_ps(0.0f, mx.z - mn.z, mx.y - mn.y, mx.x - mn.x);
    m_hk_aabb_half_extents.m_quad = _mm_mul_ps(m_hk_aabb_half_extents.m_quad, _mm_set1_ps(0.5f));
    m_hk_aabb_center.m_quad = _mm_set_ps(0.0f, mx.z + mn.z, mx.y + mn.y, mx.x + mn.x);
    m_hk_aabb_center.m_quad = _mm_mul_ps(m_hk_aabb_center.m_quad, _mm_set1_ps(0.5f));
}

// 0x00811900
havok_navmesh_shape::~havok_navmesh_shape()
{
}

// 0x00811910 (:67): one child per navmesh cell (the container's default without a layer)
int havok_navmesh_shape::getNumChildShapes() const
{
    if (pfg_data != nullptr)
    {
        return pfg_data->num_nav_cells;
    }
    return hkpShapeContainer::getNumChildShapes();
}

// 0x0081C2F0 (:82)
hkcdShapeType::ShapeTypeEnum havok_navmesh_shape::getType() const
{
    return static_cast<hkcdShapeType::ShapeTypeEnum>(m_type.m_storage);
}

// 0x00811930 (:91)
unsigned int havok_navmesh_shape::getFirstKey() const
{
    return 0;
}

// 0x0081C300 (:100): the next cell, HK_INVALID_SHAPE_KEY past the last
unsigned int havok_navmesh_shape::getNextKey(unsigned int oldKey) const
{
    const unsigned int key = oldKey + 1;
    if ((int)key >= (int)pfg_data->num_nav_cells)
    {
        return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    }
    return key;
}

// 0x0081C320 (:143): the box rotated into world space (|R| * half extents) plus the tolerance, around the
// transformed center
void havok_navmesh_shape::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    const __m128 h = m_hk_aabb_half_extents.m_quad;
    const __m128 c = m_hk_aabb_center.m_quad;
    const __m128 absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    const __m128 c0 = localToWorld.m_rotation.m_col0.m_quad;
    const __m128 c1 = localToWorld.m_rotation.m_col1.m_quad;
    const __m128 c2 = localToWorld.m_rotation.m_col2.m_quad;
    const __m128 ex = _mm_and_ps(_mm_mul_ps(_mm_shuffle_ps(h, h, 0x00), c0), absMask);
    const __m128 ey = _mm_and_ps(_mm_mul_ps(_mm_shuffle_ps(h, h, 0x55), c1), absMask);
    const __m128 ez = _mm_and_ps(_mm_mul_ps(_mm_shuffle_ps(h, h, 0xAA), c2), absMask);
    const __m128 extent = _mm_add_ps(_mm_add_ps(_mm_add_ps(ex, ey), ez), _mm_set1_ps(tolerance));
    const __m128 px = _mm_mul_ps(_mm_shuffle_ps(c, c, 0x00), c0);
    const __m128 py = _mm_mul_ps(_mm_shuffle_ps(c, c, 0x55), c1);
    const __m128 pz = _mm_mul_ps(_mm_shuffle_ps(c, c, 0xAA), c2);
    const __m128 center = _mm_add_ps(_mm_add_ps(_mm_add_ps(px, py), pz), localToWorld.m_translation.m_quad);
    out.m_max.m_quad = _mm_add_ps(center, extent);
    out.m_min.m_quad = _mm_add_ps(_mm_xor_ps(extent, _mm_set1_ps(-0.0f)), center);
}

// 0x00824A50 (:117): the cell's triangle (its three vertices, w 0) built in the caller's buffer with the default
// convex radius (the binary's null check is the placement new's)
const hkpShape* havok_navmesh_shape::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    const nav_cell* cell = &pfg_data->nav_cells.to_pointer()[key];
    const vector* vertices = pfg_data->vertices.to_pointer();
    const vector& a = vertices[cell->vertices[0]];
    const vector& b = vertices[cell->vertices[1]];
    const vector& c = vertices[cell->vertices[2]];
    alignas(16) hkVector4f v0;
    alignas(16) hkVector4f v1;
    alignas(16) hkVector4f v2;
    v0.m_quad = _mm_set_ps(0.0f, a.z, a.y, a.x);
    v1.m_quad = _mm_set_ps(0.0f, b.z, b.y, b.x);
    v2.m_quad = _mm_set_ps(0.0f, c.z, c.y, c.x);
    return new (&buffer) hkpTriangleShape(v0, v1, v2, hkConvexShapeDefaultRadius);
}
