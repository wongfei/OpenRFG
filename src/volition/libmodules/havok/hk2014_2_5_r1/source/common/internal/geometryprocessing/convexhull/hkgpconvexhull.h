// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhull.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkAabb;
struct hkContainerHeapAllocator;
struct hkGeometry;
class hkMatrix3f;
class hkPseudoRandomGenerator;
class hkSimdFloat32;
class hkSphere;
struct hkStridedVertices;
class hkTransformf;
class hkVector4f;
class hkgpConvexHullImpl;

// size 0x14
class hkgpConvexHull : public hkReferencedObject
{
public:
    enum Inputs
    {
        SOURCE_VERTICES = 0,
        INTERNAL_VERTICES = 1,
    };

    enum Side
    {
        ZERO = 0,
        POSITIVE = 1,
        NEGATIVE = 2,
        CROSSING = 3,
        INVALID = 4,
    };

    // size 0x10
    struct BuildConfig
    {
        BuildConfig(); // 0x00FD1170

        hkBool m_allowLowerDimensions; // 0x00
        hkBool m_alwaysComputeProjectionPlane; // 0x01
        float m_minCosAngle; // 0x04
        hkBool m_setSourceIndices; // 0x08
        hkBool m_buildIndices; // 0x09
        hkBool m_buildMassProperties; // 0x0A
        hkBool m_sortInputs; // 0x0B
        hkBool m_internalInputs; // 0x0C
        hkBool m_checkForDegeneratedMassProperties; // 0x0D
        hkBool m_ensurePlaneEnclosing; // 0x0E
    };

    // size 0x14
    struct SimplifyConfig
    {
        SimplifyConfig(); // 0x00FD11A0

        int m_maxVertices; // 0x00
        hkBool m_ensureContainment; // 0x04
        float m_minArea; // 0x08
        float m_minEdgeLength; // 0x0C
        hkBool m_removeUnreferencedVertices; // 0x10
        hkBool m_forceRebuild; // 0x11
    };

    // size 0xC
    struct AbsoluteScaleConfig
    {
        enum Method
        {
            SKM_VERTICES = 0,
            SKM_PLANES = 1,
            SKM_CLAMPED_PLANES = 2,
            NUM_METHODS = 3,
        };

        AbsoluteScaleConfig(); // 0x00FD11C0

        Method m_method; // 0x00
        float m_minComDistance; // 0x04
        float m_featurePreservationFactor; // 0x08
    };

    // size 0x4
    struct IUserObject
    {
        virtual ~IUserObject(); // vtbl+0x00
        virtual IUserObject* clone() const; // vtbl+0x04
        virtual void append(const IUserObject*); // vtbl+0x08

        // 0x00 vfptr
    };

    // size 0x4
    struct IBooleanFunction
    {
        virtual ~IBooleanFunction(); // vtbl+0x00
        virtual hkBool operator()(const hkgpConvexHull* const *, int) const = 0; // vtbl+0x04

        // 0x00 vfptr
    };

    // size 0x1
    struct Vertex
    {
    };

    // size 0x1
    struct Triangle
    {
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FDEAB0, delete(void* p, unsigned int nbytes) 0x00FE6500
public:
    hkgpConvexHull(const hkgpConvexHull&);
public:
    hkgpConvexHull(); // 0x00FD4520
    virtual ~hkgpConvexHull(); // vtbl+0x00 0x00FD5A40
    unsigned int uid() const; // 0x00FD1220
    void changeUid(unsigned int newUid); // 0x00FD1230
    void reset(); // 0x00FD19E0
    hkgpConvexHull* clone() const; // 0x00FD4580
    void*& userData() const; // 0x00FD11E0
    IUserObject*& userObject() const; // 0x00FD11F0
    void deleteUserObject(); // 0x00FD1200
    int build(const hkStridedVertices& points, const BuildConfig& config); // 0x00FD4620
    int build(const hkVector4f* points, int numPoints, const BuildConfig& config); // 0x00FD1250
    int buildPlanar(const hkStridedVertices& points, const hkVector4f& plane, const BuildConfig& config); // 0x00FD4730
    int buildPlanar(const hkVector4f* points, int numPoints, const hkVector4f& plane, const BuildConfig& config); // 0x00FD1750
    int buildFromPlanes(const hkVector4f* planes, int numPlanes, const hkVector4f& projectionPlane, const BuildConfig& config); // 0x00FD52E0
    int buildFromPlanes(const hkVector4f* planes, int numPlanes, const BuildConfig& config); // 0x00FD4EE0
    int buildFromDiscs(const hkVector4f* planes, hkSphere* boundingSpheres, int numPlanes, const BuildConfig& config); // 0x00FD4850
    hkResult simplify(const SimplifyConfig& config); // 0x00FD6B40
    hkBool ensureContainment(const hkArray<hkVector4f,hkContainerHeapAllocator>& points); // 0x00FD5E30
    hkBool decimateVertices(int numVerticesToKeep, hkBool bEnsureContainment, hkBool useCgo); // 0x00FD5E50
    float collapseEdges(const float tolerance, int maxNumVertices); // 0x00FD9540
    float mergePlanes(const float tolerance); // 0x00FDB440
    hkBool expandByPlanarMargin(const float margin); // 0x00FD5E90
    float absoluteScale(const float offset, const AbsoluteScaleConfig& config); // 0x00FD6B60
    void buildIndices(float minCosAngle, hkBool forceRebuild); // 0x00FD5A90
    hkBool hasIndices() const; // 0x00FD1290
    hkResult buildMassProperties(); // 0x00FD5AC0
    hkBool hasMassProperties() const; // 0x00FD12B0
    hkBool hasValidMassProperties() const; // 0x00FD12D0
    hkBool isVertexUsed(const hkVector4f& point) const; // 0x00FD21D0
    hkBool isPointInside(const hkVector4f& point, hkBool usePlanes, float epsilon) const; // 0x00FD22B0
    hkBool clipLine(const hkVector4f& a, const hkVector4f& b, float& t0, float& t1, float epsilon) const; // 0x00FD22F0
    hkSimdFloat32 maximumDistanceToPlanes(const hkVector4f& point, int* planeIndexOut) const; // 0x00FD2330
    BuildConfig& getConfiguration(); // 0x00FD1310
    const BuildConfig& getConfiguration() const; // 0x00FD1300
    int getDimensions() const; // 0x00FD1320
    hkAabb getBoundingBox(Inputs inputs, float scale, float radialScale) const; // 0x00FD28C0
    void getOrientedBoundingBox(hkVector4f& halfExtents, hkTransformf& worldTransform) const; // 0x00FD9220
    void getOrientedRectangle(const hkVector4f& projectionPlane, hkVector4f& halfExtents, hkTransformf& worldTransform) const; // 0x00FD91D0
    hkVector4f getProjectionAxis() const; // 0x00FD1330
    const hkVector4f& getProjectionPlane() const; // 0x00FD1360
    int getNumPlanes() const; // 0x00FD17A0
    const hkVector4f& getPlane(int index) const; // 0x00FD1840
    int getNumVertices() const; // 0x00FD18E0
    Vertex* getFirstVertex() const; // 0x00FD18F0
    int getIndex(const Triangle* triangle) const; // 0x00FD1420
    int getIndex(const Vertex* vertex) const; // 0x00FD1370
    Triangle* getNext(const Triangle* triangle) const; // 0x00FD1930
    Vertex* getNext(const Vertex* vertex) const; // 0x00FD1900
    hkVector4f getPosition(const Vertex* vertex) const; // 0x00FD2430
    const hkVector4f& getSourcePosition(const Vertex* vertex) const; // 0x00FD1410
    int getNumTriangles() const; // 0x00FD1910
    Triangle* getFirstTriangle() const; // 0x00FD1920
    int getPlaneIndex(const Triangle* triangle) const; // 0x00FD14C0
    hkVector4f getPlaneEquation(const Triangle* triangle) const; // 0x00FD2B50
    Vertex* getVertex(const Triangle* triangle, int index) const; // 0x00FD1940
    Triangle* getNeighbour(Triangle* triangle, int index) const; // 0x00FD55A0
    hkSimdFloat32 getWidth(const hkVector4f& direction) const; // 0x00FD6890
    hkSimdFloat32 getMinWidth() const; // 0x00FD94C0
    hkSimdFloat32 getMaxWidth() const; // 0x00FD9500
    void getWidthBounds(hkSimdFloat32& minWidth, hkSimdFloat32& maxWidth) const; // 0x00FD9230
    hkSimdFloat32 getMinimumDistanceFromPlane(const hkVector4f& plane) const; // 0x00FD5AE0
    hkSimdFloat32 getMaximumDistanceFromPlane(const hkVector4f& plane) const; // 0x00FD5B60
    void getSupportingVertex(const hkVector4f& direction, hkVector4f& vertexOut) const; // 0x00FD55C0
    void getNearestVertex(Inputs inputs, const hkVector4f& point, hkVector4f& vertexOut) const; // 0x00FD56A0
    void getFarthestVertex(Inputs inputs, const hkVector4f& point, hkVector4f& vertexOut) const; // 0x00FD57D0
    const hkVector4f& getCentroid() const; // 0x00FD2490
    hkSimdFloat32 getEdgeLengths() const; // 0x00FD5BC0
    hkSimdFloat32 getSphericity() const; // 0x00FD1D60
    hkSimdFloat32 getVolume() const; // 0x00FD1560
    hkSimdFloat32 getSurfaceArea() const; // 0x00FD1610
    void getLocalInertia(hkTransformf& inertiaSpace, hkVector4f& inertiaDiagonal) const; // 0x00FD1DE0
    const hkMatrix3f& getWorldInertia() const; // 0x00FD16C0
    const hkVector4f& getCenterOfMass() const; // 0x00FD1950
    hkVector4f getMajorPlane() const; // 0x00FD24A0
    void getMajorPlanes(hkVector4f* planes) const; // 0x00FD6910
    hkVector4f getBestProjectionPlane() const; // 0x00FD92F0
    void computeLocalTransform(hkTransformf& localTransform) const; // 0x00FD1EB0
    hkBool rayCast(const hkVector4f& origin, const hkVector4f& direction, float& minFractionInOut, int* planeOut) const; // 0x00FD25D0
    Side classify(const hkVector4f& plane, float thickness, float* boundsR) const; // 0x00FD1F70
    Side splitByPlane(const hkVector4f& plane, hkgpConvexHull** pos, hkgpConvexHull** neg, float thickness) const; // 0x00FD5BE0
    void subdivide(int recursions, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>& output, float thickness) const; // 0x00FD5C10
    void fetchPositions(Inputs inputs, hkArray<hkVector4f,hkContainerHeapAllocator>& positions) const; // 0x00FD5900
    void fetchPlanes(hkArray<hkVector4f,hkContainerHeapAllocator>& planes) const; // 0x00FD2830
    void fetchBevelPlanes(float maxCosAngle, hkArray<hkVector4f,hkContainerHeapAllocator>& bevelPlanes) const; // 0x00FD5D90
    int generateIndexedFaces(Inputs inputs, hkArray<int,hkContainerHeapAllocator>& verticesPerFace, hkArray<int,hkContainerHeapAllocator>& indices, hkBool maximizeArea) const; // 0x00FD5DF0
    int generateIndexedFaces(Inputs inputs, hkArray<unsigned char,hkContainerHeapAllocator>& verticesPerFace, hkArray<unsigned short,hkContainerHeapAllocator>& indices, hkBool maximizeArea) const; // 0x00FD5DB0
    void generateVertexAdjacencyGraph(hkArray<int,hkContainerHeapAllocator>& edges, hkArray<int,hkContainerHeapAllocator>& vertexOffsets) const; // 0x00FD9200
    void generatePlanesPerVertex(hkArray<int,hkContainerHeapAllocator>& startPlaneOffsetOut, hkArray<int,hkContainerHeapAllocator>& planeIndicesOut) const; // 0x00FD9210
    void generateGeometry(Inputs inputs, hkGeometry& geometry, int material, bool planarTwoSided) const; // 0x00FD2C40
    static void booleanSplit(const hkgpConvexHull*, const hkgpConvexHull*, hkgpConvexHull**, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>*, float);
    static hkBool booleanIntersection(const hkgpConvexHull*, const hkgpConvexHull*, hkgpConvexHull*&, float);
    static hkBool booleanSubtraction(const hkgpConvexHull*, const hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static hkBool booleanSubtraction(const hkgpConvexHull*, const hkgpConvexHull*, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static hkSimdFloat32 computeConcavityFromPoints(const hkgpConvexHull*, const hkArray<hkVector4f,hkContainerHeapAllocator>&);
    static hkSimdFloat32 computeConcavityFromGeometry(const hkgpConvexHull*, const hkGeometry&);
    static hkgpConvexHull* volumeSimplify(const hkgpConvexHull*, float, hkBool);
    static hkgpConvexHull* computeMinkowski(const hkStridedVertices&, const hkStridedVertices&, const hkSimdFloat32&);
    static hkSimdFloat32 findBestUnion(const hkgpConvexHull*, const hkgpConvexHull*, const IBooleanFunction&, hkgpConvexHull*&, hkgpConvexHull*&);
    static void merge(hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static hkgpConvexHull* createCompound(Inputs, const hkgpConvexHull*, const hkgpConvexHull*, hkBool);
    static hkgpConvexHull* createCompound(Inputs, const hkgpConvexHull* const *, int, hkBool);
    static void createRandomSamples(const hkgpConvexHull*, hkPseudoRandomGenerator&, int, hkArray<hkVector4f,hkContainerHeapAllocator>&);
    static hkgpConvexHull* createVoronoiCell(const hkgpConvexHull*, const hkArray<hkVector4f,hkContainerHeapAllocator>&, int, float);
    static void createVoronoiCells(const hkgpConvexHull*, const hkArray<hkVector4f,hkContainerHeapAllocator>&, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static void createVoronoiEdges(const hkgpConvexHull*, const hkArray<hkVector4f,hkContainerHeapAllocator>&, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static void createVoronoiFeatures(const hkgpConvexHull*, const hkArray<hkVector4f,hkContainerHeapAllocator>&, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, hkArray<hkgpConvexHull*,hkContainerHeapAllocator>&, float);
    static hkBool createPlanarSlice(const hkgpConvexHull*, const hkVector4f&, hkgpConvexHull*&);
    static void resetUidGenerator(); // 0x00FD1240
public:
    void operator=(const hkgpConvexHull&);

    hkgpConvexHullImpl* m_data; // 0x08
    void* m_userData; // 0x0C
    IUserObject* m_userObject; // 0x10
};
