// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); hkpGskManifoldWork and the enums added from the
// PDB (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h"

class hkMotionState;
class hkVector4f;
class hkpCdBody;
class hkpConstraintOwner;
class hkpContactMgr;
class hkpGskCache;
class hkpTriangleShape;
struct hkpGskManifold;
struct hkpProcessCdPoint;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

enum hkpGskManifoldAddStatus
{
    HK_GSK_MANIFOLD_POINT_REPLACED0 = 0,
    HK_GSK_MANIFOLD_POINT_REPLACED1 = 1,
    HK_GSK_MANIFOLD_POINT_REPLACED2 = 2,
    HK_GSK_MANIFOLD_POINT_REPLACED3 = 3,
    HK_GSK_MANIFOLD_POINT_ADDED = 4,
    HK_GSK_MANIFOLD_POINT_REJECTED = 5,
    HK_GSK_MANIFOLD_TWO_POINT2_REJECTED = 6,
};

enum hkpGskManifoldPointExistsFlags
{
    HK_GSK_MANIFOLD_POINT_NOT_IN_MANIFOLD = 0,
    HK_GSK_MANIFOLD_POINT_IN_MANIFOLD = 1,
    HK_GSK_MANIFOLD_FEATURE_WITHIN_KEEP_DISTANCE_REMOVED = 2,
};

enum hkpGskManifoldUtilMgrHandling
{
    HK_GSK_MANIFOLD_CREATE_ID_ALWAYS = 0,
    HK_GSK_MANIFOLD_NO_ID_FOR_POTENTIALS = 1,
};

// The manifold's vertices in world space and the shape data the point conversion needs.
// size 0x140
struct alignas(16) hkpGskManifoldWork
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpGskManifoldWork);
    const hkVector4f& getVertexA(unsigned int vertexIdMult16) const { return *reinterpret_cast<const hkVector4f*>(reinterpret_cast<const char*>(m_vertices) + vertexIdMult16); } // 0x00E83ED0
    const hkVector4f& getVertexB(unsigned int vertexIdMult16) const { return *reinterpret_cast<const hkVector4f*>(reinterpret_cast<const char*>(m_vertices) + vertexIdMult16); } // 0x00E83EE0
    hkpGskManifoldWork() {} // 0x00DC4DB0

    hkcdVertex m_vertices[16]; // 0x00
    hkVector4f m_separatingNormal; // 0x100
    hkVector4f m_weldedNormal; // 0x110
    hkPadSpu<float> m_radiusA; // 0x120
    hkPadSpu<float> m_radiusB; // 0x124
    hkPadSpu<float> m_keepContact; // 0x128
    hkPadSpu<float> m_radiusSumSqrd; // 0x12C
    const hkMotionState* m_debugBodyB; // 0x130
    const hkpTriangleShape* m_debugTriangleShape; // 0x134
};

void hkGskManifold_downshiftVertices(hkpGskManifold& manifold, int oldNumContactPoints, int newNumContactPoints); // 0x00E83D70
void hkGskManifold_upShiftVertices(hkpGskManifold& manifold, int oldNumContactPoints, int newNumContactPoints); // 0x00E83DB0
void hkGskManifold_removePoint(hkpGskManifold& manifold, int index); // 0x00E81D20
int hkGskManifold_verifyAndGetPoints(hkpGskManifold& manifold, const hkpGskManifoldWork& work, int firstPointIndex, hkpProcessCollisionOutput& processOut, hkpContactMgr* contactMgr); // 0x00E83240
hkpGskManifoldPointExistsFlags hkGskManifold_doesPointExistAndResort(hkpGskManifold& manifold, const hkpGskCache& newPoint); // 0x00E815D0
hkpGskManifoldAddStatus hkGskManifold_addPoint(hkpGskManifold& manifold, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache& newPoint, hkpProcessCdPoint* newCdPointInResultArray, hkpProcessCdPoint* resultPointArray, hkpContactMgr* mgr, hkpGskManifoldUtilMgrHandling mgrHandling); // 0x00E821D0
void hkGskManifold_cleanup(hkpGskManifold& manifold, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00E81740
void hkGskManifold_init(const hkpGskManifold& manifold, const hkVector4f& separatingNormal, const hkVector4f& weldedNormal, const hkpCdBody& cA, const hkpCdBody& cB, float keepContactMaxDist, hkpGskManifoldWork& work); // 0x00DC4DD0
