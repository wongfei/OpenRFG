// Original: volition/rfg/code/havok/havok_filter_shape_collection.h
// Implementation: volition/rfg/code/havok/havok_filter_shape_collection.cpp
//
// A list shape that carries a collision filter per child shape, so that parts of one rigid body can collide
// differently (havok_filter_shape_collection_create wraps a base shape; set/get_collision_filter change a range of
// sub-pieces).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/list/hkplistshape.h"

class hkAabb;
struct hkContainerHeapAllocator;
class hkTransformf;
class hkpRigidBody;
class hkpShape;

// size 0x70
class havok_filter_shape_collection : public hkpListShape
{
public:
    havok_filter_shape_collection(const hkpShape* const * shape_array, int num_shapes, const hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters); // 0x0081BA50 (:19)
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x0081BBE0 (:36)
    virtual void set_collision_filter(unsigned int start_index, unsigned int num_sub_pieces, unsigned int collision_filter); // vtbl+0x50 0x008113D0 (:48)
    virtual void get_collision_filter(hkArray<unsigned int,hkContainerHeapAllocator>& out_collision_filter, unsigned int start_index, unsigned int num_sub_pieces) const; // vtbl+0x54 0x0082AFA0 (:74)
    virtual ~havok_filter_shape_collection(); // vtbl+0x00 0x0081BAD0

};

havok_filter_shape_collection* havok_filter_shape_collection_create(const hkpShape* base_shape, const hkArray<unsigned int,hkContainerHeapAllocator>& initial_collision_filters); // 0x00832F80 (:87)
havok_filter_shape_collection* havok_filter_shape_collection_get_from_rigid_body(const hkpRigidBody* rb); // 0x0081BCA0 (:110)
