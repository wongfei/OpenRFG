// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qstransform/hkqstransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
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
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x20
class hkpConvexTransformShapeBase : public hkpConvexShape
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D74C40
    static const hkClass& staticClass(); // 0x00D7C740
public:
    hkpConvexTransformShapeBase(hkcdShapeType::ShapeTypeEnum type, float radius, const hkpConvexShape* childShape, hkpShapeContainer::ReferencePolicy ref); // 0x00D74AF0
public:
    hkpConvexTransformShapeBase(hkFinishLoadedObjectFlag flag); // 0x00D75210
    hkpConvexTransformShapeBase();
public:
    void getChildShapeFromPpu(int) const;
public:
    virtual ~hkpConvexTransformShapeBase(); // vtbl+0x00 0x0071B5E0

    static const hkInternalClassMember Members[]; // 0x01353844
public:
    hkpSingleShapeContainer m_childShape; // 0x14
    int m_childShapeSizeForSpu; // 0x1C
};

// size 0x60
class hkpConvexTransformShape : public hkpConvexTransformShapeBase
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 11,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F730, new(unsigned int nbytes) 0x0071B610, delete(void* p, unsigned int nbytes) 0x00D76310
    static const hkClass& staticClass(); // 0x00D7F740
    hkpConvexTransformShape(hkFinishLoadedObjectFlag flag); // 0x00D75340
    hkpConvexTransformShape(const hkpConvexShape* childShape, const hkQsTransformf& transform, hkpShapeContainer::ReferencePolicy ref); // 0x00D75FC0
    hkpConvexTransformShape(const hkpConvexShape* childShape, const hkTransformf& transform, hkpShapeContainer::ReferencePolicy ref); // 0x00D752F0
    hkpConvexTransformShape();
    void setTransform(const hkTransformf& transform); // 0x00D760B0
    void setTransform(const hkQsTransformf& transform); // 0x00D75DC0
    void getTransform(hkTransformf*) const;
    const hkQsTransformf& getQsTransform() const; // 0x00E720D0
    const hkVector4f& getExtraScale() const;
    const hkpConvexShape* getChildShape() const; // 0x00A12FD0 (hkpconvextransformshape.inl:10)
    void transformVertex(const hkVector4f& localVertex, hkVector4f* vertexOut) const; // 0x00D76240
    virtual ~hkpConvexTransformShape(); // vtbl+0x00 0x00D76350
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D75370
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D75B00
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D754A0
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D755B0
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D75710
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D75810
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D76300
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D759D0
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00D75290
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D752A0
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D758F0

    static const hkInternalClassMember Members[]; // 0x01356F74
public:
    hkQsTransformf m_transform; // 0x20
    hkVector4f m_extraScale; // 0x50
};
