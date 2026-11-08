// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); hkpExtendedGskOut and the inline constructor
// added (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

class hkBool;
class hkTransformf;
class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// The GSK result with the point on B (in B's space) and the normal in world space (welded and unwelded).
// size 0x60
struct alignas(16) hkpExtendedGskOut : public hkpGskOut
{
    hkpExtendedGskOut() {} // 0x00DD0DC0

    hkVector4f m_pointBinB; // 0x30
    hkVector4f m_normalInWorld; // 0x40
    hkVector4f m_unweldedNormalInWorld; // 0x50
};

// size 0x30
class hkpGskBaseAgent : public hkpIterativeLinearCastAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DC5780, delete(void* p, unsigned int nbytes) 0x00DD0880
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00DCFDD0
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00DD0350
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00DCFFD0
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DCFBD0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00DD0470
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DCFBE0
public:
    hkpGskBaseAgent(hkpContactMgr* mgr) : hkpIterativeLinearCastAgent(mgr) {}
public:
    hkpGskBaseAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* mgr); // 0x00DCFC60
public:
    hkBool _getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpExtendedGskOut& output); // 0x00DD0910
    static void calcSeparatingNormal(const hkpCdBody& bodyA, const hkpCdBody& bodyB, float earlyOutTolerance, hkpGsk& gsk, hkVector4f& separatingNormalOut); // 0x00DCFD40
    hkBool getClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpExtendedGskOut& output); // 0x00DD0170
    static hkBool staticGetClosestPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkTransformf& aTb, const hkpCollisionInput& input, hkpGskCache& cache, hkpExtendedGskOut& output); // 0x00DD0AF0
    virtual ~hkpGskBaseAgent(); // vtbl+0x00 0x00DB70C0
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00DCFC00
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00DCFC20

    hkpGskCache m_cache; // 0x0C
    float m_timeOfSeparatingNormal; // 0x18
    float m_allowedPenetration; // 0x1C
    hkVector4f m_separatingNormal; // 0x20
};
