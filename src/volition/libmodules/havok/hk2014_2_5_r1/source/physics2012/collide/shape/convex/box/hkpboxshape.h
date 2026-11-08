// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkSphere;
class hkTransformf;
class hkcdVertex;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x30
class hkpBoxShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 3,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7FBF0, new(unsigned int nbytes) 0x00814B40, delete(void* p, unsigned int nbytes) 0x00D828E0
    static const hkClass& staticClass(); // 0x00D7FC00
    virtual ~hkpBoxShape(); // vtbl+0x00 0x00D82040
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D82060
    hkpBoxShape(hkFinishLoadedObjectFlag flag); // 0x00D82220
    hkpBoxShape(const hkVector4f& halfExtents, float radius); // 0x00D821A0
    hkpBoxShape();
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D822D0
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D82280
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D82100
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D820F0
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D828D0
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D82150
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D828C0
    const hkVector4f& getHalfExtents() const; // 0x00DBED90
    void setHalfExtents(const hkVector4f& halfExtents); // 0x00D82240
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D82050
    virtual int getSize() const; // vtbl+0x4C 0x00D828B0

    static const hkInternalClassMember Members[]; // 0x01357290
public:
    alignas(16) hkVector4f m_halfExtents; // 0x20
};
