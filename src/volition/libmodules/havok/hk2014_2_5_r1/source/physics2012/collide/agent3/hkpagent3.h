// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h
//
// Havok 2014.2.5: layouts from rfg.pdb, the rest ported from the 2014.1 SDK header (havok_impl.md HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpCdBody;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x4
struct hkpAgentEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentEntry);

    unsigned char m_streamCommand; // 0x00 (hkAgent3::StreamCommand)
    unsigned char m_agentType; // 0x01
    unsigned char m_numContactPoints; // 0x02
    unsigned char m_size; // 0x03 (bytes of the entry including its agent data)
};

typedef void hkpAgentData;

// size 0x60
struct alignas(16) hkpAgent3Input
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent3Input);

    // 0x00DB3A00
    hkpAgent3Input()
    {
        m_overrideBodyA = nullptr;
    }

    hkPadSpu<const hkpCdBody*> m_bodyA; // 0x00
    hkPadSpu<const hkpCdBody*> m_bodyB; // 0x04
    hkPadSpu<const hkpCdBody*> m_overrideBodyA; // 0x08
    hkPadSpu<const hkpProcessCollisionInput*> m_input; // 0x0C
    hkPadSpu<hkpContactMgr*> m_contactMgr; // 0x10
    hkTransformf m_aTb; // 0x20
};

// size 0x80
struct alignas(16) hkpAgent3ProcessInput : public hkpAgent3Input
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent3ProcessInput);

    // 0x00DB3A10
    hkpAgent3ProcessInput()
    {
    }

    hkPadSpu<float> m_distAtT1; // 0x60
    hkVector4f m_linearTimInfo; // 0x70
};

namespace hkAgent3
{
    typedef int AgentType;

    enum
    {
        SIZE_OF_HEADER = 16, // size of the end command (HK_REAL_ALIGNMENT)
        MAX_SIZE = 128,
        MAX_NET_SIZE = MAX_SIZE - (2 * SIZE_OF_HEADER), // size of a tim entry
    };

    enum StreamCommand
    {
        STREAM_NULL = 0, // not defined
        STREAM_END = 1, // end of last sectors
        STREAM_CALL = 2, // call the function the dispatcher declares for m_agentType
        STREAM_CALL_FLIPPED = 3, // the same with flipped input
        STREAM_CALL_WITH_TIM = 4, // the same, checking the tims first
        STREAM_CALL_WITH_TIM_FLIPPED = 5, // STREAM_CALL_WITH_TIM with flipped input
        STREAM_CALL_AGENT = 6, // old style agents (hkAgent3Bridge)

        TRANSFORM_FLAG = 0x08, // at least one body of the entry is a hkpTransformShape

        STREAM_CALL_WITH_TRANSFORM = TRANSFORM_FLAG | STREAM_CALL,
        STREAM_CALL_FLIPPED_WITH_TRANSFORM = TRANSFORM_FLAG | STREAM_CALL_FLIPPED,
        STREAM_CALL_WITH_TIM_WITH_TRANSFORM = TRANSFORM_FLAG | STREAM_CALL_WITH_TIM,
        STREAM_CALL_WITH_TIM_FLIPPED_WITH_TRANSFORM = TRANSFORM_FLAG | STREAM_CALL_WITH_TIM_FLIPPED,
        STREAM_CALL_AGENT_WITH_TRANSFORM = TRANSFORM_FLAG | STREAM_CALL_AGENT,
    };

    enum Symmetric
    {
        IS_SYMMETRIC = 0,
        IS_NOT_SYMMETRIC = 1,
        IS_NOT_SYMMETRIC_AND_FLIPPED = 2,
    };

    typedef hkpAgentData* (*CreateFunc)(const hkpAgent3Input& input, hkpAgentEntry* entry, hkpAgentData* agentData);
    typedef void (*DestroyFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher);
    typedef hkpAgentData* (*ProcessFunc)(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, hkpAgentData* agentData, hkVector4f* separatingNormalOut, hkpProcessCollisionOutput& result);
    typedef void (*SepNormalFunc)(const hkpAgent3Input& input, hkpAgentEntry* entry, hkpAgentData* agentData, hkVector4f& separatingNormalOut);
    typedef hkpAgentData* (*CleanupFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner);
    typedef void (*RemovePointFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, unsigned short idToRemove);
    typedef void (*CommitPotentialFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, unsigned short newId);
    typedef void (*CreateZombieFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, unsigned short idTobecomeZombie);
    typedef void (*UpdateFilterFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner);
    typedef void (*InvalidateTimFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, const hkpCollisionInput& input);
    typedef void (*WarpTimeFunc)(hkpAgentEntry* entry, hkpAgentData* agentData, float oldTime, float newTime, const hkpCollisionInput& input);
}
