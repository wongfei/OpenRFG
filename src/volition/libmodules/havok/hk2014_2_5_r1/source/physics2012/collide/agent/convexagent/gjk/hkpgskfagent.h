// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"

class hkpCdBody;
class hkpCollisionAgent;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x80
class alignas(16) hkpGskfAgent : public hkpGskBaseAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DD1200, delete(void* p, unsigned int nbytes) 0x00DD1220
    static void initAgentFunc(hkpCollisionDispatcher::AgentFuncs& af); // 0x00DD0F50
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DD0F80
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result); // vtbl+0x1C 0x00DD1030
    virtual void cleanup(hkpConstraintOwner& constraintOwner); // vtbl+0x20 0x00DD0DE0
    virtual void removePoint(unsigned short idToRemove); // vtbl+0x30 0x00DD0E10
    virtual void commitPotential(unsigned short idToCommit); // vtbl+0x34 0x00DD0E50
    virtual void createZombie(unsigned short idTobecomeZombie); // vtbl+0x38 0x00DD0E90
public:
    hkpGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpContactMgr* mgr); // 0x00DD0EC0
    virtual ~hkpGskfAgent(); // vtbl+0x00 0x00DB70D0
    void processCollisionNoTim(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, int explicitlyAllowNewPoint, hkpProcessCollisionOutput& output); // 0x00DD0FB0
public:
    static hkpCollisionAgent* createGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DD0EF0

    hkpGskManifold m_manifold; // 0x30
};
