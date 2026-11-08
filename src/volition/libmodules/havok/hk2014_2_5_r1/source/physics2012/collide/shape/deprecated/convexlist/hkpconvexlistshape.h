// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/convexlist/hkpconvexlistshape.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkClass;
class hkSphere;
class hkTransformf;
class hkcdVertex;
class hkpCdBody;
class hkpRayHitCollector;
class hkpShape;
struct hkpShapeBufferStorage;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x50
class hkpConvexListShape : public hkpConvexShape, public hkpShapeContainer
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F7A0, delete(void* p, unsigned int nbytes) 0x00DAC380
    enum
    {
        THIS_SHAPE_TYPE = 26,
    };

    static const hkClass& staticClass(); // 0x00D7F7B0
    hkpConvexListShape(hkFinishLoadedObjectFlag flag); // 0x00DABE90
    hkpConvexListShape(const hkpConvexShape* const * shapeArray, int numShapes); // 0x00DAC020
    virtual ~hkpConvexListShape(); // vtbl+0x00 0x00DABEC0
    void setUseCachedAabb(bool useCachedAabb); // 0x00DAB9A0
    bool getUseCachedAabb(); // 0x00DAB820
    const hkpShape* getChildShapeInl(int) const;
    virtual int getNumChildShapes() const; // vtbl+0x04 0x00DAB970
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00DAB830
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x00DAB980
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00DAB840
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00DAB990
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00DABA50
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00DABBA0
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00DABC90
    virtual void getSupportingVertex(const hkVector4f& dir, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00DABD70
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00DAB870
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00DAB8F0
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00DAB920
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00DAB850
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00DAB8E0
public:
    void setShapesAndRadius(const hkpConvexShape* const * shapeArray, int numShapes); // 0x00DABF80

public:
    float m_minDistanceToUseConvexHullForGetClosestPoints; // 0x18
public:
    hkVector4f m_aabbHalfExtents; // 0x20
    hkVector4f m_aabbCenter; // 0x30
    hkBool m_useCachedAabb; // 0x40
public:
    hkArray<hkpConvexShape const*,hkContainerHeapAllocator> m_childShapes; // 0x44
};
