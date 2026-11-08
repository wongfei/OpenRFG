// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/heightfieldagent/hkpheightfieldagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"

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

// size 0x18
class hkpHeightFieldAgent : public hkpCollisionAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00D908A0, delete(void* p, unsigned int nbytes) 0x00D90CF0
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00D90730
    virtual void getPenetrations(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // vtbl+0x10 0x00D906A0
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector); // vtbl+0x14 0x00D906C0
    static void staticGetPenetrations(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector); // 0x00D8FB60
    virtual void linearCast(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // vtbl+0x18 0x00D90580
    static void staticGetClosestPoints(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpCollisionInput& input, hkpCdPointCollector& collector); // 0x00D8FE10
    virtual void processCollision(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& processOutput); // vtbl+0x1C 0x00D90D30
    static void staticLinearCast(const hkpCdBody& csBody, const hkpCdBody& hfBody, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector); // 0x00D901F0
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00D8FB00
public:
    hkpHeightFieldAgent(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D905B0
    virtual ~hkpHeightFieldAgent(); // vtbl+0x00 0x00D90C50
    static hkpCollisionAgent* createHeightFieldAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D906E0
    static hkpCollisionAgent* createHeightFieldBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00D90660

    hkArray<unsigned short,hkContainerHeapAllocator> m_contactPointId; // 0x0C
};
