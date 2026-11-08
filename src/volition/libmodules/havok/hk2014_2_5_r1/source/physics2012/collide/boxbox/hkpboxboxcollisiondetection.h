// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcollisiondetection.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4fcomparison.h"

class hkBool;
class hkContactPoint;
struct hkResult;
class hkpBoxBoxManifold;
class hkpCdBody;
class hkpContactMgr;
class hkpFeatureContactPoint;
struct hkpProcessCdPoint;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x130
class hkpBoxBoxCollisionDetection
{
public:
    enum hkpBoxBoxFeature
    {
        HK_BOXBOX_FACE_A_X = 0,
        HK_BOXBOX_FACE_A_Y = 1,
        HK_BOXBOX_FACE_A_Z = 2,
        HK_BOXBOX_FACE_B_X = 4,
        HK_BOXBOX_FACE_B_Y = 5,
        HK_BOXBOX_FACE_B_Z = 6,
        HK_BOXBOX_EDGE_0_0 = 8,
        HK_BOXBOX_EDGE_0_1 = 9,
        HK_BOXBOX_EDGE_0_2 = 10,
        HK_BOXBOX_EDGE_1_0 = 12,
        HK_BOXBOX_EDGE_1_1 = 13,
        HK_BOXBOX_EDGE_1_2 = 14,
        HK_BOXBOX_EDGE_2_0 = 16,
        HK_BOXBOX_EDGE_2_1 = 17,
        HK_BOXBOX_EDGE_2_2 = 18,
        HK_BOXBOX_FEATURE_COUNT = 19,
    };

    enum
    {
        HK_FINDCLOSESTPOINT_NO_VALID_POINT = 0,
        HK_FINDCLOSESTPOINT_POINT_IN_MANIFOLD = 1,
        HK_FINDCLOSESTPOINT_VALID_POINT = 2,
    };

    class hkpFeaturePointCache;
    typedef void (hkpBoxBoxCollisionDetection::*tFcnValidationDataFromFeatureId)(hkpFeaturePointCache&, const hkpFeatureContactPoint&);
    typedef hkBool (hkpBoxBoxCollisionDetection::*tFcnIsValid)(hkpFeaturePointCache&);

    // size 0x60
    class alignas(16) hkpFeaturePointCache
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpFeaturePointCache);

        hkVector4f m_vA; // 0x00
        hkVector4f m_vB; // 0x10
        hkVector4f m_nA; // 0x20
        hkVector4fComparison m_normalIsFlipped; // 0x30
        hkSimdFloat32 m_distance; // 0x40
        int m_featureIndexA; // 0x50
        int m_featureIndexB; // 0x54
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBoxBoxCollisionDetection);
    hkpBoxBoxCollisionDetection(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput* env, hkpContactMgr* mgr, hkpProcessCollisionOutput* result, const hkTransformf& aTb, const hkTransformf& wTa, const hkVector4f& radiusA, const hkTransformf& wTb, const hkVector4f& radiusB, const hkSimdFloat32& tolerance); // 0x00DC34E0
    void calcManifold(hkpBoxBoxManifold& manifold) const; // 0x00DD6090
    hkResult checkIntersection(const hkVector4f& tolerance) const; // 0x00DD4810
    hkBool calculateClosestPoint(hkContactPoint& contact) const; // 0x00DD5EC0
    hkBool getPenetrations(); // 0x00DC3F00
public:
    void initWorkVariables() const; // 0x00DC3E40
    int findClosestPoint(hkpBoxBoxManifold& manifold, hkpFeatureContactPoint& fcp, hkpFeaturePointCache& fpp) const; // 0x00DD5610
    void refreshManifold(hkpBoxBoxManifold& manifold, hkSimdFloat32& minContactPointDistance) const; // 0x00DDA4D0
    void tryToAddPointFaceA(hkpBoxBoxManifold& manifold, const hkpFeatureContactPoint fcpTemplate, unsigned short planeMask, const hkSimdFloat32& closestPointDist) const; // 0x00DD86E0
    void tryToAddPointFaceB(hkpBoxBoxManifold& manifold, const hkpFeatureContactPoint fcpTemplate, unsigned short planeMask, const hkSimdFloat32& closestPointDist) const; // 0x00DD8B40
    void tryToAddPointOnEdge(hkpBoxBoxManifold& manifold, int edgeA, int edgeB, int nextVertA, int nextVertB, const hkVector4f& normalA, const hkVector4f& normalB, const hkSimdFloat32& closestPointDist) const; // 0x00DD5480
    void addAdditionalEdgeHelper(hkpBoxBoxManifold& manifold, hkpFeatureContactPoint& fcp, const hkSimdFloat32& closestPointDist) const; // 0x00DD5100
    void findAdditionalManifoldPoints(hkpBoxBoxManifold& manifold, hkpFeatureContactPoint fcpATemplate) const; // 0x00DD97F0
    void faceAVertexBValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const; // 0x00DD7F40
    void faceBVertexAValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const; // 0x00DD8050
    void edgeEdgeValidationDataFromFeatureId(hkpFeaturePointCache& fpp, const hkpFeatureContactPoint& fcp) const; // 0x00DD7A60
    void faceAVertexBContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const; // 0x00DD7FD0
    void faceBVertexAContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const; // 0x00DD8150
    void edgeEdgeContactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const; // 0x00DD81E0
    void contactPointFromFeaturePointCache(hkpProcessCdPoint& ccpOut, const hkpFeatureContactPoint& fcp, const hkpFeaturePointCache& fpp) const; // 0x00DD8270
    void faceAVertexBValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp, int featureIndex) const; // 0x00DD9010
    void faceBVertexAValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp, int featureIndex) const; // 0x00DD9140
    void edgeEdgeValidationDataFromFeatureIndex(hkpFeaturePointCache& fpp) const; // 0x00DD92C0
    unsigned int isValidFaceAVertexB(const hkpFeaturePointCache& fpp) const; // 0x00DD7E30
    unsigned int isValidFaceBVertexA(const hkpFeaturePointCache& fpp) const; // 0x00DD7EB0
    void calcDistanceFaceAVertexB(hkpFeaturePointCache& fpp) const; // 0x00DD7E60
    void calcDistanceFaceBVertexA(hkpFeaturePointCache& fpp) const; // 0x00DD7EE0
    bool isValidEdgeEdge(hkpFeaturePointCache& fpp) const; // 0x00DD4D10
    void calcManifoldNormal(hkVector4f& manifoldN, const hkpFeatureContactPoint& fcp, hkpFeaturePointCache& fpp, bool isPointValidated) const; // 0x00DD7BC0
    unsigned int queryManifoldNormalConsistency(hkpBoxBoxManifold& manifold) const; // 0x00DD9410
    void checkManifoldNormalConsistency(hkpBoxBoxManifold& manifold) const; // 0x00DD95C0
    void checkCompleteness(hkpBoxBoxManifold& manifold, int planeMaskA, int planeMaskB) const; // 0x00DD4370
    hkResult addPoint(hkpBoxBoxManifold& manifold, const hkpFeaturePointCache& fpp, hkpFeatureContactPoint& fcp) const; // 0x00DD83D0
    void removePoint(hkpBoxBoxManifold& manifold, int index) const; // 0x00DD7B80

    static hkBool m_attemptToFindAllEdges;
    static const float m_manifoldConsistencyCheckAngularCosTolerance; // 0x0135B540
    static const float m_coplanerAngularCosTolerance; // 0x0135B544
    static const float m_contactNormalAngularCosTolerance; // 0x0135B548
    static const float m_edgeEndpointTolerance; // 0x0135B54C
    static const int m_maxFeaturesToReject; // 0x0135B550
    const hkpCdBody& m_bodyA; // 0x00
    const hkpCdBody& m_bodyB; // 0x04
    const hkpProcessCollisionInput* m_env; // 0x08
    hkpContactMgr* m_contactMgr; // 0x0C
    hkpProcessCollisionOutput* m_result; // 0x10
    const hkTransformf& m_wTa; // 0x14
    const hkTransformf& m_wTb; // 0x18
    hkTransformf m_aTb; // 0x20
    hkVector4f m_radiusA; // 0x60
    hkVector4f m_radiusB; // 0x70
    hkVector4f m_tolerance4; // 0x80
    hkVector4f m_keepRadiusA; // 0x90
    hkVector4f m_keepRadiusB; // 0xA0
    hkSimdFloat32 m_boundaryTolerance; // 0xB0
    hkVector4f m_dinA; // 0xC0
    hkVector4f m_dinB; // 0xD0
    hkVector4f m_sepDist[5]; // 0xE0
};
