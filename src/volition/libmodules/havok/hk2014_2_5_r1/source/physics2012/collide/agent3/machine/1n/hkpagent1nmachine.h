// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h
//
// Havok 2014.2.5: layouts and functions from rfg.pdb, inline code ported from the 2014.1 SDK header (havok_impl.md HK6).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

class hkMotionState;
struct hkpAgent1nTrack;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpProcessCollisionOutput;
class hkpShapeContainer;

// size 0xC (pad to 16 bytes when using it)
struct hkpAgent1nMachineEntry : public hkpAgentEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nMachineEntry);

    hkpShapeKeyPair m_shapeKeyPair; // 0x04
};

// size 0x10
struct hkpAgent1nMachinePaddedEntry : public hkpAgent1nMachineEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nMachinePaddedEntry);

    unsigned int m_padUpTo16Bytes; // 0x0C
};

// size 0x20
struct hkpAgent1nMachineTimEntry : public hkpAgent1nMachineEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nMachineTimEntry);

    float m_timeOfSeparatingNormal; // 0x0C (only used if tims are enabled)
    hkVector4f m_separatingNormal; // 0x10
};

// size 0x20
struct hkpAgent1nMachine_VisitorInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgent1nMachine_VisitorInput);

    const hkpCdBody* m_bodyA; // 0x00
    const hkpCdBody* m_collectionBodyB; // 0x04
    const hkpShapeContainer* m_containerShapeA; // 0x08
    const hkpShapeContainer* m_containerShapeB; // 0x0C
    const hkpCollisionInput* m_input; // 0x10
    hkpContactMgr* m_contactMgr; // 0x14
    hkpConstraintOwner* m_constraintOwner; // 0x18
    void* m_clientData; // 0x1C
};

typedef hkpAgentData* (*hkAgent1nMachine_VisitorCallback)(hkpAgent1nMachine_VisitorInput& vin, hkpAgent1nMachineEntry* entry, hkpAgentData* agentData);

// size 0xA0
struct hkpAgentNmMachineBodyTemp
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNmMachineBodyTemp);
    // hkpAgentNmMachineBodyTemp 0x00DCF8A0: the bodies' default constructors

    hkpCdBody m_bodyA; // 0x00
    hkpCdBody m_bodyB; // 0x10
    hkTransformf m_transA; // 0x20
    hkTransformf m_transB; // 0x60
};

// size 0x4
class hkpShapeKeyPairLocal1n
{
public:
    // 0x00DCF660
    bool operator==(const hkpShapeKeyPair& p1)
    {
        return m_shapeKeyB == p1.m_shapeKeyB;
    }

    // 0x00DCF680
    bool operator>(const hkpShapeKeyPair& p1)
    {
        return (m_shapeKeyB > p1.m_shapeKeyB);
    }

    void operator=(const hkpShapeKeyPair& p1)
    {
        m_shapeKeyB = p1.m_shapeKeyB;
    }

    // 0x00DCF6A0
    void writeTo(hkpShapeKeyPair& p1)
    {
        p1.m_shapeKeyA = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
        p1.m_shapeKeyB = m_shapeKeyB;
    }

    hkPadSpu<unsigned int> m_shapeKeyB; // 0x00 (used by the 1n and nm machines)
};

// size 0x8
class hkpShapeKeyPairLocalNm
{
public:
    // 0x00DCF6C0
    hkpShapeKeyPairLocalNm()
        : m_shapeKeyA(0xFFFFFFFF), m_shapeKeyB(0xFFFFFFFF)
    {
    }

    // 0x00DCF6D0
    bool operator==(const hkpShapeKeyPair& p1)
    {
        return m_shapeKeyB == p1.m_shapeKeyB && m_shapeKeyA == p1.m_shapeKeyA;
    }

    // 0x00DCF6F0
    bool operator>(const hkpShapeKeyPair& p1)
    {
        return (m_shapeKeyB > p1.m_shapeKeyB) || (m_shapeKeyB == p1.m_shapeKeyB && m_shapeKeyA > p1.m_shapeKeyA);
    }

    void operator=(const hkpShapeKeyPair& p1)
    {
        m_shapeKeyA = p1.m_shapeKeyA;
        m_shapeKeyB = p1.m_shapeKeyB;
    }

    // 0x00DCF740
    void writeTo(hkpShapeKeyPair& p1)
    {
        p1.m_shapeKeyB = m_shapeKeyB;
        p1.m_shapeKeyA = m_shapeKeyA;
    }

    hkPadSpu<unsigned int> m_shapeKeyA; // 0x00 (only used by the nm machine)
    hkPadSpu<unsigned int> m_shapeKeyB; // 0x04 (used by the 1n and nm machines)
};

void hkAgent1nMachine_Destroy(hkpAgent1nTrack& agentTrack, hkpCollisionDispatcher* dispatcher, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner); // 0x00DCBCA0
void hkAgent1nMachine_InvalidateTim(hkpAgent1nTrack& agentTrack, const hkpCollisionInput& input); // 0x00DCB7B0
void hkAgent1nMachine_WarpTime(hkpAgent1nTrack& agentTrack, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DCB890
hkpAgent1nMachineEntry* hkAgent1nMachine_FindAgent(hkpAgent1nTrack& agentTrack, hkpShapeKeyPair keyPair, void** agentDataOut); // 0x00DCB9A0
void hkAgent1nMachine_Create(hkpAgent1nTrack& agentTrack); // 0x00DCBDF0
void hkAgent1nMachine_Weld(hkpAgent3Input& input, const hkpShapeContainer* shapeCollection, hkpProcessCollisionOutput& output); // 0x00DCBEA0
void hkAgent1nMachine_VisitAllAgents(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin, hkAgent1nMachine_VisitorCallback visitor); // 0x00DCC610
void hkAgent1nMachine_initInputAtTime(hkpAgent3Input& in, hkpAgentNmMachineBodyTemp& temp, hkpAgent3Input& out); // 0x00DCBA30
void hkAgent1nMachine_flipInput(hkpAgent3ProcessInput& in, hkpAgent3ProcessInput& out); // 0x00DCBAE0
void hkAgent1nMachine_UpdateShapeCollectionFilter(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin); // 0x00DCC9E0
void hkAgentNmMachine_UpdateShapeCollectionFilter(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin); // 0x00DCCA00
void hkAgentNmMachine_Process(hkpAgent1nTrack& agentTrack, const hkpAgent3ProcessInput& inputIn, const hkpShapeContainer* shapeContainerA, const hkpShapeContainer* shapeContainerB, const hkpShapeKeyPair* newKeys, hkpProcessCollisionOutput& output); // 0x00DCE110
void hkAgent1nMachine_Process(hkpAgent1nTrack& agentTrack, const hkpAgent3ProcessInput& inputIn, const hkpShapeContainer* shapeContainerB, const unsigned int* newKeys, hkpProcessCollisionOutput& output); // 0x00DCD0D0
void* hkAgent1nMachine_UpdateShapeCollectionFilterVisitor(hkpAgent1nMachine_VisitorInput& vin, hkpAgent1nMachineEntry* entry, void* agentData); // 0x00DCCDD0
void* hkAgentNmMachine_UpdateShapeCollectionFilterVisitor(hkpAgent1nMachine_VisitorInput& vin, hkpAgent1nMachineEntry* entry, void* agentData); // 0x00DCCF10
const hkpCdBody* hkAgentMachine_processTransformedShapes(const hkpCdBody* cdBody, hkpCdBody* newCdBodies, hkMotionState* newMotionStates, int numSlots, hkPadSpu<unsigned char>& cdBodyHasTransformFlag); // 0x00DDAF70 (nn machine)
