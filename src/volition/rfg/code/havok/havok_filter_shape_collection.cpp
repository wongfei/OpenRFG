// Original: volition/rfg/code/havok/havok_filter_shape_collection.cpp
//
// A list shape with a collision filter per child, so a vehicle's parts collide differently within one rigid body
// (stub_impl.md S9: every vehicle's body is one).

#include "volition/rfg/code/havok/havok_filter_shape_collection.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/rfg/code/havok/havok.h"

#include <math.h>

// 0x0081BA50 (:19)
havok_filter_shape_collection::havok_filter_shape_collection(const hkpShape* const* shape_array, int num_shapes, const hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters)
    : hkpListShape(shape_array, num_shapes, hkpShapeContainer::REFERENCE_POLICY_INCREMENT)
{
    for (int i = 0; i < num_shapes; i++)
    {
        setCollisionFilterInfo(i, initial_collision_filters[i]);
    }
}

// 0x0081BAD0
havok_filter_shape_collection::~havok_filter_shape_collection()
{
}

// 0x0081BBE0 (:36): the list's box transformed, widened by the tolerance.
void havok_filter_shape_collection::getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const
{
    hkVector4f half;
    hkVector4f center;
    const hkVector4f& col0 = localToWorld.getRotation().getColumn(0);
    const hkVector4f& col1 = localToWorld.getRotation().getColumn(1);
    const hkVector4f& col2 = localToWorld.getRotation().getColumn(2);
    const hkVector4f& translation = localToWorld.getTranslation();
    const float he0 = m_aabbHalfExtents.m_quad.m128_f32[0];
    const float he1 = m_aabbHalfExtents.m_quad.m128_f32[1];
    const float he2 = m_aabbHalfExtents.m_quad.m128_f32[2];
    const float c0 = m_aabbCenter.m_quad.m128_f32[0];
    const float c1 = m_aabbCenter.m_quad.m128_f32[1];
    const float c2 = m_aabbCenter.m_quad.m128_f32[2];
    for (int i = 0; i < 4; i++)
    {
        half.m_quad.m128_f32[i] = fabsf(he0 * col0.m_quad.m128_f32[i]) + fabsf(he1 * col1.m_quad.m128_f32[i]) + fabsf(he2 * col2.m_quad.m128_f32[i]) + tolerance;
        center.m_quad.m128_f32[i] = c0 * col0.m_quad.m128_f32[i] + c1 * col1.m_quad.m128_f32[i] + c2 * col2.m_quad.m128_f32[i] + translation.m_quad.m128_f32[i];
        out.m_max.m_quad.m128_f32[i] = center.m_quad.m128_f32[i] + half.m_quad.m128_f32[i];
        out.m_min.m_quad.m128_f32[i] = center.m_quad.m128_f32[i] - half.m_quad.m128_f32[i];
    }
}

// 0x008113D0 (:48)
void havok_filter_shape_collection::set_collision_filter(unsigned int start_index, unsigned int num_sub_pieces, unsigned int collision_filter)
{
    for (unsigned int i = start_index; i < start_index + num_sub_pieces; i++)
    {
        if (collision_filter == COLLIDE_NO_COLLISION)
        {
            disableChild(i);
        }
        else
        {
            enableChild(i);
        }
        setCollisionFilterInfo(i, collision_filter);
    }
}

// 0x0082AFA0 (:74)
void havok_filter_shape_collection::get_collision_filter(hkArray<unsigned int,hkContainerHeapAllocator>& out_collision_filter, unsigned int start_index, unsigned int num_sub_pieces) const
{
    out_collision_filter.clear();
    for (unsigned int i = start_index; i < start_index + num_sub_pieces; i++)
    {
        out_collision_filter.pushBack(getCollisionFilterInfo(i));
    }
}

// 0x00832F80 (:87): a list of the base shape's children, or of the base shape alone.
havok_filter_shape_collection* havok_filter_shape_collection_create(const hkpShape* base_shape, const hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters)
{
    if (base_shape == nullptr)
    {
        return nullptr;
    }
    hkArray<const hkpShape*,hkContainerHeapAllocator> children;
    if (base_shape->getType() == hkcdShapeType::LIST)
    {
        const hkpListShape* list = (const hkpListShape*)base_shape;
        const int count = list->getNumChildShapes() + list->m_numDisabledChildren;
        for (int i = 0; i < count; i++)
        {
            children.pushBack(list->m_childInfo[i].m_shape);
        }
    }
    else
    {
        children.pushBack(base_shape);
    }
    return new havok_filter_shape_collection(children.begin(), children.getSize(), initial_collision_filters);
}

// 0x0081BCA0 (:110)
havok_filter_shape_collection* havok_filter_shape_collection_get_from_rigid_body(const hkpRigidBody* rb)
{
    const hkpShape* shape = rb->getCollidable()->getShape();
    if (shape->getType() == hkcdShapeType::LIST)
    {
        return (havok_filter_shape_collection*)shape;
    }
    if (shape->getType() != hkcdShapeType::MOPP)
    {
        // Original: Int3 critical function here.
        return nullptr;
    }
    return (havok_filter_shape_collection*)((const hkpMoppBvTreeShape*)shape)->getChild();
}
