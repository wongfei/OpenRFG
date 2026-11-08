// Original: volition/rfg/code/havok/havok_level_shape.h
// Implementation: volition/rfg/code/havok/havok_level_shape.cpp
//
// havok_navmesh_shape, a shape collection over the triangles of a path finding layer (bb_pfg_layer): one child
// triangle shape per navmesh triangle, keyed by triangle index. Used for ray and sphere casts against the navmesh.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"

struct bb_pfg_layer;
class hkAabb;
class hkTransformf;
class hkpShape;
struct hkpShapeBufferStorage;

// size 0x40
class havok_navmesh_shape : public hkpShapeCollection
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 18,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00815A10 (.h:26), delete(void* p, unsigned int nbytes) 0x00815A30 (.h:26)
protected:
    static unsigned int create_shape_key(int triangle_index); // 0x00811850 (:36)
public:
    havok_navmesh_shape(const bb_pfg_layer* _pfg_data); // 0x00811860 (:46)
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00811910 (:67)
    virtual hkcdShapeType::ShapeTypeEnum getType() const; // vtbl+0x50 0x0081C2F0 (:82)
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00811930 (:91)
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x0081C300 (:100)
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x0081C320 (:143)
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00824A50 (:117)
    static int get_triangle_index_from_shape_key(unsigned int key); // 0x00811940 (:155)
    virtual ~havok_navmesh_shape(); // vtbl+0x00 0x00811900

protected:
    const bb_pfg_layer* pfg_data; // 0x18
    hkVector4f m_hk_aabb_center; // 0x20
    hkVector4f m_hk_aabb_half_extents; // 0x30
};
