// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h
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

// size 0x40
class hkpCapsuleShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 4,
    };

    enum RayHitType
    {
        HIT_CAP0 = 0,
        HIT_CAP1 = 1,
        HIT_BODY = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7FB10, new(unsigned int nbytes) 0x00814B60, delete(void* p, unsigned int nbytes) 0x00D81DD0
    static const hkClass& staticClass(); // 0x00D7FB20
    virtual ~hkpCapsuleShape(); // vtbl+0x00 0x00D81D60
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D81410
    hkpCapsuleShape(hkFinishLoadedObjectFlag flag); // 0x00D812D0
    hkpCapsuleShape(const hkVector4f& vertexA, const hkVector4f& vertexB, float radius); // 0x00D81260
    hkpCapsuleShape();
    static hkpCapsuleShape* createInPlace(unsigned char*&, const hkVector4f&, const hkVector4f&, float);
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D814E0
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D81E60
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D81D90
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D81D70
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D81D40
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D81240
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D81D30
    const hkVector4f* getVertices() const; // 0x0080D5B0 (hkpcapsuleshape.inl:66)
    const hkVector4f& getVertex(int i) const; // 0x0080D5C0 (hkpcapsuleshape.inl:71)
    void setVertex(int i, const hkVector4f& position); // 0x0080D580 (hkpcapsuleshape.inl:40)
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D81230
    static void closestPointLineSeg(const hkVector4f& A, const hkVector4f& B, const hkVector4f& B2, hkVector4f& pt); // 0x00D81360
    static void closestInfLineSegInfLineSeg(const hkVector4f& A, const hkVector4f& dA, const hkVector4f& B, const hkVector4f& dB, float& distSquared, float& t, float& u, hkVector4f& p, hkVector4f& q); // 0x00D812F0
    virtual int getSize() const; // vtbl+0x4C 0x00D81D20

    static const hkInternalClassMember Members[]; // 0x013571D4
public:
    alignas(16) hkVector4f m_vertexA; // 0x20
    hkVector4f m_vertexB; // 0x30
};
