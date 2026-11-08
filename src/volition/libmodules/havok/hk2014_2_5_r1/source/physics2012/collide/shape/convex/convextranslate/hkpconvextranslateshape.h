// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

class hkAabb;
class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkSphere;
class hkTransformf;
class hkcdVertex;
class hkpCdBody;
class hkpConvexShape;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x30
class hkpConvexTranslateShape : public hkpConvexTransformShapeBase
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 10,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F6C0, new(unsigned int nbytes) 0x0071B630, delete(void* p, unsigned int nbytes) 0x0071B770
    static const hkClass& staticClass(); // 0x00D7F6D0
    hkpConvexTranslateShape(hkFinishLoadedObjectFlag flag); // 0x00D74F00
    hkpConvexTranslateShape(const hkpConvexShape*, const hkVector4f&, hkpShapeContainer::ReferencePolicy);
    hkpConvexTranslateShape();
    const hkpConvexShape* getChildShape() const; // 0x0071B750 (hkpconvextranslateshape.inl:10)
    virtual ~hkpConvexTranslateShape(); // vtbl+0x00 0x0071B7B0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D74E90
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D74F80
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D75080
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D74D50
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D74D90
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D74DE0
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x0071B760 (hkpconvextranslateshape.inl:71)
    const hkVector4f& getTranslation() const; // 0x0080D5D0 (hkpconvextranslateshape.inl:29)
    hkVector4f& getTranslation(); // 0x0080D5E0 (hkpconvextranslateshape.inl:34)
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D74E40
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00D74D40
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00D74F30
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D74CF0
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D74E10

    static const hkInternalClassMember Members[]; // 0x01356F1C
public:
    hkVector4f m_translation; // 0x20
};
