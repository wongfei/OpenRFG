// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

class hkAabb;
class hkClass;
struct hkInternalClassMember;
class hkSphere;
class hkTransformf;
class hkVector4fComparison;
class hkcdVertex;
struct hkpShapeRayBundleCastInput;
struct hkpShapeRayBundleCastOutput;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x60
class hkpTriangleShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 2,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x0080D770, delete(void* p, unsigned int nbytes) 0x00815B20
    static const hkClass& staticClass(); // 0x00D7E5C0
    virtual ~hkpTriangleShape(); // vtbl+0x00 0x00815B00
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D82A60
    hkpTriangleShape(const hkVector4f& v0, const hkVector4f& v1, const hkVector4f& v2, float radius); // 0x00815A80 (hkptriangleshape.inl:43)
    hkpTriangleShape(float radius, unsigned short weldingInfo, hkpWeldingUtility::WeldingType type); // 0x00D95E60
    hkpTriangleShape(hkFinishLoadedObjectFlag flag); // 0x00D82BB0
    static hkpTriangleShape* createInPlace(unsigned char*& memPtr, float convexRadius, hkpWeldingUtility::WeldingType weldingType, unsigned short weldingInfo); // 0x00ED9E60
    const hkVector4f* getVertices() const; // 0x00B63D90 (hkptriangleshape.inl:57)
    const hkVector4f& getVertex(int i) const; // 0x00D83980
    hkVector4f& getVertex(int i); // 0x00DA6090
    void setVertex(int i, const hkVector4f& vertex); // 0x00DA3FF0
    unsigned short getWeldingInfo() const; // 0x0104EAB0
    void setWeldingInfo(unsigned short info); // 0x0080D780 (hkptriangleshape.inl:16)
    hkpWeldingUtility::WeldingType getWeldingType() const; // 0x00E84320
    void setWeldingType(hkpWeldingUtility::WeldingType);
    bool isExtruded() const; // 0x00E83FC0
    const hkVector4f& getExtrusion() const;
    void setExtrusion(const hkVector4f& extrusion); // 0x00D95EB0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D83170
    virtual hkVector4fComparison castRayBundle(const hkpShapeRayBundleCastInput& input, hkpShapeRayBundleCastOutput& results, const hkVector4fComparison& mask) const; // vtbl+0x20 0x00D82C00
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D82E30
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D82A00
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D82BD0
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00815B10 (hkptriangleshape.inl:139)
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D82B20
    virtual int weldContactPoint(unsigned short* featurePoints, unsigned char& numFeaturePoints, hkVector4f& contactPointWs, const hkTransformf* thisObjTransform, const hkpConvexShape* collidingShape, const hkTransformf* collidingTransform, hkVector4f& separatingNormalInOut) const; // vtbl+0x38 0x00D83390
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00815AF0 (.h:27)
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D829F0
    virtual int getSize() const; // vtbl+0x4C 0x00815AE0 (.h:27)

    static const hkInternalClassMember Members[]; // 0x016F5E90
public:
    unsigned short m_weldingInfo; // 0x14
    hkEnum<enum hkpWeldingUtility::WeldingType,unsigned char> m_weldingType; // 0x16
    unsigned char m_isExtruded; // 0x17
    hkVector4f m_vertexA; // 0x20
    hkVector4f m_vertexB; // 0x30
    hkVector4f m_vertexC; // 0x40
    hkVector4f m_extrusion; // 0x50
};
