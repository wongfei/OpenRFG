// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpaabbcastcollector.h"

class hkBool;
struct hkResult;
class hkTransformf;
class hkVector4f;
class hkpCdBody;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x40
class hkpBvTreeAgent : public hkpCollisionAgent
{
public:
    // size 0xC
    struct hkpBvAgentEntryInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBvAgentEntryInfo);
        unsigned int& getKey(); // 0x00D76470
        void setKey(unsigned int& key); // 0x00D76480

        unsigned int m_key; // 0x00
        int m_userInfo; // 0x04
        hkpCollisionAgent* m_collisionAgent; // 0x08
    };

    // size 0x40
    struct alignas(16) LinearCastAabbCastCollector : public hkpAabbCastCollector
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(LinearCastAabbCastCollector); // delete(void* p, unsigned int nbytes) 0x00DB1950
        LinearCastAabbCastCollector(const hkpCdBody& castBody, const hkpCdBody& bvTreeBody, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // 0x00DB1910
        virtual void addHit(unsigned int key); // vtbl+0x00 0x00D76B70
        virtual ~LinearCastAabbCastCollector(); // vtbl+0x04 0x00DB18E0

        const hkpCdBody& m_castBody; // 0x20
        const hkpCdBody& m_bvTreeBody; // 0x24
        const hkpLinearCastCollisionInput& m_input; // 0x28
        hkpCdPointCollector& m_castCollector; // 0x2C
        hkpCdPointCollector* m_startCollector; // 0x30
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00D78A30, delete(void* p, unsigned int nbytes) 0x00D794D0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00D77FB0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00D780D0
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00D77DC0
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00D76C50
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00D767D0
    virtual void linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00D780A0
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00D77BD0
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00D76DD0
    virtual void cleanup(hkpConstraintOwner& info); // vtbl+0x20 0x00D764B0
    static void setUseAabbCaching(hkBool useIt); // 0x00D764A0
    static hkBool getUseAabbCaching(); // 0x00D76490
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // vtbl+0x24 0x00D76640
    virtual void invalidateTim(const hkpCollisionInput& input); // vtbl+0x28 0x00D76500
    virtual void warpTime(float oldTime, float newTime, const hkpCollisionInput& input); // vtbl+0x2C 0x00D76540
    static hkResult calcAabbAndQueryTree(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkTransformf& bTa, const hkVector4f& linearTimInfo, const hkpProcessCollisionInput& input, hkAabb* cachedAabb, hkArray<unsigned int,hkContainerHeapAllocator>& hitListOut); // 0x00D78AF0
    hkpBvTreeAgent(hkpContactMgr* mgr); // 0x00D76C70
public:
    static hkpCollisionAgent* defaultAgentCreate(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D765E0
    void prepareCollisionPartnersProcess(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpConstraintOwner& constraintOwner); // 0x00D79E30
    static void calcAabbLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkAabb& aabbOut); // 0x00D76A70
    static void staticCalcAabb(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkAabb& aabbOut); // 0x00D76590
    virtual ~hkpBvTreeAgent(); // vtbl+0x00 0x00D79480
    static hkpCollisionAgent* createShapeBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D76CD0
    static hkpCollisionAgent* createBvTreeShapeAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D76D00
    static hkpCollisionAgent* createBvBvAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D76D40

public:
    static hkBool m_useFastUpdate; // 0x03A274E9
public:
    hkArray<hkpBvAgentEntryInfo,hkContainerHeapAllocator> m_collisionPartners; // 0x0C
    hkAabb m_cachedAabb; // 0x20
    static hkBool m_useAabbCaching; // 0x016F5264
};
