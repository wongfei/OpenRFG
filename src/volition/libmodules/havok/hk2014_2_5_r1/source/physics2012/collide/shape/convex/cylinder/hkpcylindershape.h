// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h
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

// size 0x60
class hkpCylinderShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 1,
    };

    enum VertexIdEncoding
    {
        VERTEX_ID_ENCODING_IS_BASE_A_SHIFT = 7,
        VERTEX_ID_ENCODING_SIN_SIGN_SHIFT = 6,
        VERTEX_ID_ENCODING_COS_SIGN_SHIFT = 5,
        VERTEX_ID_ENCODING_IS_SIN_LESSER_SHIFT = 4,
        VERTEX_ID_ENCODING_VALUE_MASK = 15,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F460, new(unsigned int nbytes) 0x00815F30, delete(void* p, unsigned int nbytes) 0x00D92960
    static const hkClass& staticClass(); // 0x00D7F470
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D91D40
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D91FF0
    hkpCylinderShape(hkFinishLoadedObjectFlag flag); // 0x00D91960
    hkpCylinderShape(const hkVector4f& vertexA, const hkVector4f& vertexB, float cylinderRadius, float paddingRadius); // 0x00D92640
    hkpCylinderShape();
    static hkpCylinderShape* createInPlace(unsigned char*& memPtr, const hkVector4f& vertexA, const hkVector4f& vertexB, float cylinderRadius, float paddingRadius); // 0x00ED8FB0
    void afterReflectNew(); // 0x00D91700
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D91990
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D91C00
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D916D0
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D92940
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D91760
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D92930
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D916F0
    const hkVector4f* getVertices() const; // 0x00D915A0
    const hkVector4f& getVertex(int i) const; // 0x00D915B0
    void setVertex(int, const hkVector4f&);
    float getCylinderRadius() const; // 0x00D91620
    void setCylinderRadius(const float radius); // 0x00D916A0
    float getCylinderBaseRadiusFactorForHeightfieldCollisions(float);
    void setCylinderBaseRadiusFactorForHeightfieldCollisions(float);
    void presetPerpendicularVector(); // 0x00D91EB0
    static void setNumberOfVirtualSideSegments(int numSegments); // 0x00D915D0
    virtual int getSize() const; // vtbl+0x4C 0x00D92920
    static void initRoundUpThreshold(); // 0x00D927D0
public:
    void decodeVertexId(unsigned short code, hkVector4f& result) const; // 0x00D929F0
public:
    virtual ~hkpCylinderShape(); // vtbl+0x00 0x00D92950

    static const hkInternalClassMember Members[]; // 0x01356B70
public:
    float m_cylRadius; // 0x14
    float m_cylBaseRadiusFactorForHeightFieldCollisions; // 0x18
    hkVector4f m_vertexA; // 0x20
    hkVector4f m_vertexB; // 0x30
    hkVector4f m_perpendicular1; // 0x40
    hkVector4f m_perpendicular2; // 0x50
public:
    static float s_virtualTesselationParameter; // 0x016F6258
    static float s_virtualTesselationParameterInv; // 0x016F625C
    static float s_intRoundUpThreshold; // 0x016F6260
};
