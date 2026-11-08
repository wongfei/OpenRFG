// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpsphererepshape.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;

// size 0x14
class hkpConvexShape : public hkpSphereRepShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 23,
    };

    enum WeldResult
    {
        WELD_RESULT_REJECT_CONTACT_POINT = 0,
        WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED = 1,
        WELD_RESULT_ACCEPT_CONTACT_POINT_UNMODIFIED = 2,
    };

    typedef unsigned short VertexId;

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00814AB0
    static const hkClass& staticClass(); // 0x00D7C730
    hkpConvexShape(hkFinishLoadedObjectFlag flag); // 0x00D74A40
public:
    hkpConvexShape(hkcdShapeType::ShapeTypeEnum, float);
public:
    hkpConvexShape();
    virtual ~hkpConvexShape(); // vtbl+0x00 0x007114B0
    virtual bool isConvex() const; // vtbl+0x10 0x0071B6C0 (.h:77)
    virtual void castRayWithCollector(const hkpShapeRayCastInput& inputLocal, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D74A60
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D749F0
    virtual int weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const; // vtbl+0x38 0x00D749B0
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00D74B40
    virtual void getFirstVertex(hkVector4f&) const = 0; // vtbl+0x48
    const float& getRadius() const; // 0x007114C0 (hkpconvexshape.inl:19)
    void setRadius(float radius); // 0x0080D570 (hkpconvexshape.inl:30)
    void setRadiusUnchecked(float radius); // 0x0080D560 (hkpconvexshape.inl:24)
    virtual int getSize() const; // vtbl+0x4C 0x0071B6D0 (.h:83)

    static const hkInternalClassMember Members[]; // 0x0135382C
public:
    float m_radius; // 0x10
};
