// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebase.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"

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
class hkpCdBody;
class hkpConvexShape;
class hkpRayHitCollector;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0xC
class hkpShapeBase : public hkcdShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 34,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7EAF0, delete(void* p, unsigned int nbytes) 0x0071A8D0
    static const hkClass& staticClass(); // 0x00D7EA90
    hkpShapeBase(hkFinishLoadedObjectFlag flag); // 0x00D74400
    hkpShapeBase(hkcdShapeType::ShapeTypeEnum);
    hkpShapeBase();
    void afterReflectNew(); // 0x00D743F0
    virtual ~hkpShapeBase(); // vtbl+0x00 0x007108C0
    bool isBvTree() const; // 0x00DF2D10
    bool isCollection() const;
    virtual bool isConvex() const; // vtbl+0x10 0x0071A8C0 (.h:57)
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& aabbOut) const; // vtbl+0x14 0x00D74350
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& output) const; // vtbl+0x18 0x00D743D0
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D74360
    virtual hkVector4fComparison castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& output, const hkVector4fComparison& mask) const; // vtbl+0x20 0x00D743E0
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D74370
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* vertexArrayOut) const; // vtbl+0x28 0x00D74380
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D74390
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D743A0
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D743B0
    virtual int weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingConvexShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const; // vtbl+0x38 0x00D743C0

    static const hkInternalClassMember Members[];
};
