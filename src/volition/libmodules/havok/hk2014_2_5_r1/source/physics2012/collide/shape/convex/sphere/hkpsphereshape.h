// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkSphere;
class hkTransformf;
class hkVector4f;
class hkVector4fComparison;
class hkcdVertex;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x20
class hkpSphereShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 0,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7E690, new(unsigned int nbytes) 0x00814B80, delete(void* p, unsigned int nbytes) 0x00D7BE30
    static const hkClass& staticClass(); // 0x00D7E6A0
    virtual ~hkpSphereShape(); // vtbl+0x00 0x00743440
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D7BDF0
    hkpSphereShape(hkFinishLoadedObjectFlag flag); // 0x00D7B1C0
    hkpSphereShape(float radius); // 0x00D7B190
    hkpSphereShape();
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D7B1E0
    virtual hkVector4fComparison castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const; // vtbl+0x20 0x00D7B4D0
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D7BDB0
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D7BDC0
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D7BDE0
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D7BD80
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D7BEC0
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D7BD70
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D7B180
    virtual int getSize() const; // vtbl+0x4C 0x00D7BD60

    static const hkInternalClassMember Members[];
    unsigned int m_pad16[3]; // 0x14
};
