// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkAabb;
class hkClass;
struct hkInternalClassMember;
class hkSphere;
struct hkStridedVertices;
class hkTransformf;
class hkcdVertex;
class hkpConvexVerticesConnectivity;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x70
class alignas(16) hkpConvexVerticesShape : public hkpConvexShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 5,
    };

    // size 0x18
    struct BuildConfig
    {
        BuildConfig();

        hkBool m_createConnectivity; // 0x00
        hkBool m_shrinkByConvexRadius; // 0x01
        hkBool m_useOptimizedShrinking; // 0x02
        float m_convexRadius; // 0x04
        int m_maxVertices; // 0x08
        float m_maxRelativeShrink; // 0x0C
        float m_maxShrinkingVerticesDisplacement; // 0x10
        float m_maxCosAngleForBevelPlanes; // 0x14
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F4D0, new(unsigned int nbytes) 0x00815F80, delete(void* p, unsigned int nbytes) 0x00D99ED0
    static const hkClass& staticClass(); // 0x00D7F4E0
    virtual ~hkpConvexVerticesShape(); // vtbl+0x00 0x00D985E0
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D99EB0
    hkpConvexVerticesShape(hkFinishLoadedObjectFlag flag); // 0x00D985B0
    hkpConvexVerticesShape(float radius); // 0x00D983C0
    hkpConvexVerticesShape(hkFourTransposedPointsf* rotatedVertices, int numVertices, hkVector4f* planes, int numPlanes, const hkAabb& aabb, float radius); // 0x00D98500
    hkpConvexVerticesShape(const hkStridedVertices& vertsIn, const hkArray<hkVector4f,hkContainerHeapAllocator>& planeEquations, float radius); // 0x00D98410
    hkpConvexVerticesShape(const hkStridedVertices&, const BuildConfig&);
    void afterReflectNew(); // 0x00D97860
    virtual void getFirstVertex(hkVector4f& v) const; // vtbl+0x48 0x00D976A0
    static hkpConvexVerticesShape* createInPlace(unsigned char*& memPtr, float convexRadius); // 0x00ED8FF0
    void getOriginalVertices(hkArray<hkVector4f,hkContainerHeapAllocator>& vertices) const; // 0x00D98130
    const hkArray<hkVector4f,hkContainerHeapAllocator>& getPlaneEquations() const; // 0x00D97690
    void setPlaneEquations(const hkArray<hkVector4f,hkContainerHeapAllocator>& planes); // 0x00D98D50
    void setRadiusUnchecked(float);
    void transformVerticesAndPlaneEquations(const hkTransformf& t); // 0x00D98670
    void copyVertexData(const float* vertexIn, int byteStriding, int numVertices); // 0x00D981C0
    const hkpConvexVerticesConnectivity* getConnectivity() const;
    void setConnectivity(const hkpConvexVerticesConnectivity* connect, bool sort); // 0x00D98D10
    virtual int getSize() const; // vtbl+0x4C 0x00D99EA0
public:
    void sortPlanes(); // 0x00D98920
public:
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D97870
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D97A70
    virtual void getSupportingVertex(const hkVector4f& direction, hkcdVertex& supportingVertexOut) const; // vtbl+0x24 0x00D97930
    virtual void convertVertexIdsToVertices(const unsigned short* ids, int numIds, hkcdVertex* verticesOut) const; // vtbl+0x28 0x00D977A0
    virtual void getCentre(hkVector4f& centreOut) const; // vtbl+0x2C 0x00D97850
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00D99EC0
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00D976D0

    static const hkInternalClassMember Members[];
    alignas(16) hkVector4f m_aabbHalfExtents; // 0x20
    hkVector4f m_aabbCenter; // 0x30
    hkArray<hkFourTransposedPointsf,hkContainerHeapAllocator> m_rotatedVertices; // 0x40
    int m_numVertices; // 0x4C
    hkBool m_useSpuBuffer; // 0x50
    hkArray<hkVector4f,hkContainerHeapAllocator> m_planeEquations; // 0x54
    const hkpConvexVerticesConnectivity* m_connectivity; // 0x60
};
