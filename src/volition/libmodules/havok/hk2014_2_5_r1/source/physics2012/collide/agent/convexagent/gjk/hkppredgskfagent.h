// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.h"

class hkpCdBody;
class hkpCollisionAgent;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x80
class hkpPredGskfAgent : public hkpGskfAgent
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DB70E0, delete(void* p, unsigned int nbytes) 0x00DB7130
    static void registerAgent(hkpCollisionDispatcher* dispatcher); // 0x00DC5820
    virtual ~hkpPredGskfAgent(); // vtbl+0x00 0x00DB7170
    static hkpCollisionAgent* createPredGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr); // 0x00DC57A0
    hkpPredGskfAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& inputIn, hkpContactMgr* mgr); // 0x00DB7100
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output); // vtbl+0x1C 0x00DC5860

};
