// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/gsk/hkcdgsk.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

class hkFourTransposedPointsf;

namespace hkcdGsk
{
    enum GetClosestPointStatus
    {
        STATUS_OK_NOT_PENTRATING = 0,
        STATUS_OK_PENETRATING = 1,
        STATUS_PD_HEURISTIC_SAMPLING = 2,
        STATUS_PD_OUT_OF_MEMORY = 3,
        STATUS_OK_FLAG = 4,
        STATUS_PD_DISTANCE_TOO_BIG = 5,
        STATUS_PENETRATING = 6,
        STATUS_PD_UNSOLVABLE = 7,
    };

    // size 0x5
    struct Cache
    {
        typedef unsigned char VertexId;

        void setDims(int dimA, int dimB, int userData); // 0x00EF6F50
        int getDimA() const; // 0x00EF6F70
        int getDimB() const; // 0x00EF6F80
        unsigned char dimBisNot3_orPenetrating();
        void init(); // 0x00EF6F90
        void pack(int dimA, int dimB, int status, const hkcdVertex* verticesOfAinA, const hkcdVertex* verticesOfBinA); // 0x00EFCD10
        const unsigned char* getVertexIdsA() const; // 0x00EF6FA0
        const unsigned char* getVertexIdsB() const; // 0x00EF6FB0

    public:
        unsigned char m_vertexIds[4]; // 0x00
        unsigned char m_dimAb; // 0x04
    };

    // size 0x70
    struct GetClosestPointInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(GetClosestPointInput);
        GetClosestPointInput(); // 0x01010810

        hkTransformf m_aTb; // 0x00
        hkPadSpu<unsigned int> m_handlePenetration; // 0x40
        hkSimdFloat32 m_coreCollisionTolerance; // 0x50
        hkPadSpu<float> m_terminationGrowthRate; // 0x60
    };

    // size 0x30
    struct GetClosestPointOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(GetClosestPointOutput);
        hkSimdFloat32 getDistance(); // 0x010107B0

        hkSimdFloat32 m_distance; // 0x00
        hkVector4f m_normalInA; // 0x10
        hkVector4f m_pointAinA; // 0x20
    };

    // size 0x70
    struct LinearCastInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(LinearCastInput);
        LinearCastInput(float convexRadiusA, float convexRadiusB, float eps); // 0x00D992F0
        const hkSimdFloat32 getRadiusAB() const; // 0x00EFCCD0
        const hkSimdFloat32 getSquaredTerminationEps() const; // 0x00EFCCF0

        hkVector4f m_from; // 0x00
        hkVector4f m_direction; // 0x10
        hkTransformf m_aTb; // 0x20
        hkVector4f m_data; // 0x60
    };

    // size 0x20
    struct LinearCastOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(LinearCastOutput);

        hkSimdFloat32 m_fractionInOut; // 0x00
        hkVector4f m_normalOut; // 0x10
    };

    // size 0x80
    struct RayCastInput : public LinearCastInput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(RayCastInput);
        RayCastInput(float convexRadiusA, float eps); // 0x00D99C20

        hkFlags<enum hkcdRayQueryFlags::Enum,unsigned int> m_flags; // 0x70
    };

    // size 0x20
    struct RayCastOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(RayCastOutput);

        hkVector4f m_normalOut; // 0x00
        hkSimdFloat32 m_fractionInOut; // 0x10
    };
}

// size 0x8
struct hkcdGsk_Vector4Shape
{
    typedef unsigned char VertexId;

    void getSupportingVertex(const hkVector4f& direction, hkcdVertex& vertexOut) const; // 0x00EFD670
    void convertVertexIdsToVertices(const unsigned char* ids, int numVerts, hkcdVertex* verticesOut) const; // 0x00EFCE10

    const hkcdVertex* m_vertices; // 0x00
    int m_numVertices; // 0x04
};

// size 0xC0
class hkcdGskBase
{
public:
    enum
    {
        GSK_NONE_NONE = 0,
        GSK_POINT_POINT = 9,
        GSK_POINT_EDGE = 10,
        GSK_EDGE_POINT = 17,
        GSK_POINT_TRIANGLE = 11,
        GSK_TRIANGLE_POINT = 25,
        GSK_EDGE_TRIANGLE = 19,
        GSK_TRIANGLE_EDGE = 26,
        GSK_POINT_TETRA = 12,
        GSK_TETRA_POINT = 33,
        GSK_EDGE_EDGE = 18,
    };

    enum NextCase
    {
        NEXT_IS_POINT_TRIANGLE = 0,
        NEXT_IS_PENETRATION = 1,
        NEXT_IS_EDGE_EDGE = 2,
    };

    enum SupportTypes
    {
        BUILD_NEG_SUPPORT = -1,
        BUILD_NO_SUPPORT = 0,
        BUILD_POS_SUPPORT = 1,
    };

    enum SupportState
    {
        SUPPORT_CHANGED = 0,
        DONT_CALL_GET_SUPPORTING_VERTEX_A = 1,
        DONT_CALL_GET_SUPPORTING_VERTEX_B = -1,
    };

    enum ReduceDimensionResult
    {
        REDUCE_DIMENSION_OK = 0,
        REDUCE_DIMENSION_PENETRATION = 1,
    };

    typedef void Shape;

    // size 0x4
    class ShapeInterface
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ShapeInterface); // delete(void* p, unsigned int nbytes) 0x00EFCDB0
        virtual ~ShapeInterface(); // vtbl+0x00 0x00EF6FC0
        virtual void getSupportingVertices(const void*, const hkVector4f&, const void*, const hkTransformf&, hkcdVertex*, hkcdVertex*, hkVector4f*) const = 0; // vtbl+0x04
        static unsigned int isImplicit();
        ShapeInterface(); // 0x00EFCB70

        // 0x00 vfptr
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdGskBase);
    void print();
public:
    NextCase _processEdgeTriangle(hkVector4f* vertexA, hkVector4f* vertexB, unsigned int comingFromEdgeEdge, hkVector4f* supportOut); // 0x00EFE590
    ReduceDimensionResult reduceDimension(hkPadSpu<int>& dimA, hkPadSpu<int>& dimB, hkVector4f& support, bool assumeStaticOjbects); // 0x00EFF1B0
    static int findClosestTriangleBackup(const hkVector4f& a0, const hkVector4f* tetra, const hkVector4f& oldDots, const hkSimdFloat32& sign); // 0x00EFD3B0
    static int findClosestTriangle(const hkVector4f& a0, const hkVector4f* tetra, const hkSimdFloat32& sign, bool assumeStaticOjbects); // 0x00EFE170
    hkcdGsk::GetClosestPointStatus handlePenetration(const ShapeInterface* shapeInterface, const void* shapeA, const void* shapeB, const hkTransformf& aTb, hkPadSpu<int>& dimA, hkPadSpu<int>& dimB, hkcdGsk::GetClosestPointOutput& output, hkVector4f* supportOut); // 0x00F0E2F0
    NextCase processEdgeTriangle(hkVector4f* vertexA, hkVector4f* vertexB, unsigned int comingFromEdgeEdge, hkVector4f* supportOut); // 0x00F0D6E0
public:
    hkcdGskBase(); // 0x00EFCF00

public:
    static const hkVector4ComparisonMask::Mask dimToMaskY[4]; // 0x01367AC4
    static const hkVector4ComparisonMask::Mask dimToMaskZ[4]; // 0x01367AD4
    static const signed char maskToIndex[]; // 0x01367AE4
    static const signed char vertexToEdgeLut[]; // 0x01367AEC
    hkVector4f m_checkTriangleDots; // 0x00
    hkPadSpu<int> m_lastDimB; // 0x10
    hkcdVertex m_verticesA[4]; // 0x20
    hkcdVertex m_verticesBinA[4]; // 0x60
    hkVector4f m_support; // 0xA0
    hkVector4f m_closestPointOnEdgeA; // 0xB0
};

namespace hkcdGsk
{
    hkcdGsk::GetClosestPointStatus getClosestPoint(const hkcdVertex* vertsA, int numVertsA, const hkcdVertex* vertsB, int numVertsB, const hkcdGsk::GetClosestPointInput& input, hkcdGsk::Cache* cache, hkcdGsk::GetClosestPointOutput& output); // 0x00EF7030
    hkcdGsk::GetClosestPointStatus getClosestPoint(const void*, const hkcdVertex* vertsA, int numVertsA, const void*, const hkcdVertex* vertsB, int numVertsB, const hkcdGsk::GetClosestPointInput& input, hkcdGsk::Cache* cache, hkcdGsk::GetClosestPointOutput& output); // 0x00EF9D00
    bool linearCast(const hkcdVertex* vertsShape, int numVertsShape, const hkcdVertex* vertsCast, int numVertsCast, const hkcdGsk::LinearCastInput& input, hkcdGsk::Cache* cache, hkcdGsk::LinearCastOutput& output); // 0x00EFC9D0
    bool rayCast(const hkcdVertex* verts, int numVerts, const hkcdGsk::RayCastInput& input, hkcdGsk::RayCastOutput& output); // 0x00EFCA30
    bool rayCast(const hkFourTransposedPointsf* fours, int numUnTransposed, const hkcdGsk::RayCastInput& input, hkcdGsk::RayCastOutput& output); // 0x00EFCA60
}
