// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the Nn agent machine, from rfg.exe: the per-island tracks of agent3 entries
// between pairs of linked collidables (512-byte sectors of fixed-size entries: 64 bytes on the midphase track, 128 on
// the narrowphase track), their creation, removal and relinking, and the processing of one entry or a whole track.
// No SDK source exists for 2014.2.5; ported from the binary.
//
// Entry layouts (hkpagentnnmachine.h): a padded entry keeps the agent data at +0x20; a tim entry (the agent has a
// separating normal function) keeps its separating normal time at +0x18, the normal at +0x20 and the agent data at
// +0x30.
//
// The shipped build records the "TtProcessNnEntry"/"Et" timers and the "MinumTim" value (TLS hkMonitorStream); they
// are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

namespace
{
const int kTimEntryDataOffset = 0x30; // sizeof(hkpAgentNnMachineTimEntry)
const int kPaddedEntryDataOffset = 0x20; // sizeof(hkpAgentNnMachinePaddedEntry)

inline hkpAgentData* hkNnMachine_data(hkpAgentNnEntry* entry, int offset)
{
    return reinterpret_cast<char*>(entry) + offset;
}

inline hkpAgentNnMachineTimEntry* hkNnMachine_tim(hkpAgentNnEntry* entry)
{
    return static_cast<hkpAgentNnMachineTimEntry*>(entry);
}

// hkNextPowerOf2(size + 4) without the - 1 / + 1 pair: the capacity the machine shrinks its arrays to.
inline int hkNnMachine_shrunkCapacity(int size)
{
    unsigned int v = (unsigned int)(size + 3);
    v |= v >> 16;
    v |= v >> 8;
    v |= v >> 4;
    v |= v >> 2;
    return (int)((v >> 1 | v) + 1);
}

// hkInplaceArray::optimizeCapacity(4) of the sector array (inlined): only a heap buffer is shrunk.
void hkNnMachine_optimizeSectors(hkpAgentNnTrack& track)
{
    hkInplaceArray<hkpAgentNnSector*, 1, hkContainerHeapAllocator>& sectors = track.m_sectors;
    if (sectors.m_capacityAndFlags < 0)
    {
        return; // DONT_DEALLOCATE: the inplace storage
    }
    const int capacity = hkNnMachine_shrunkCapacity(sectors.m_size);
    if (capacity < 1)
    {
        hkArrayUtil::_reduce(hkContainerHeapAllocator::s_alloc, &sectors, sizeof(hkpAgentNnSector*), reinterpret_cast<char*>(sectors.m_storage), 1);
    }
    else if (capacity < (sectors.m_capacityAndFlags & hkArrayBase<hkpAgentNnSector*>::CAPACITY_MASK))
    {
        hkArrayUtil::_reduce(hkContainerHeapAllocator::s_alloc, &sectors, sizeof(hkpAgentNnSector*), nullptr, capacity);
    }
}

// hkArray::optimizeCapacity(4) of a collidable's entries (inlined)
void hkNnMachine_optimizeEntries(hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries)
{
    const int capacity = hkNnMachine_shrunkCapacity(entries.m_size);
    if (capacity < (entries.m_capacityAndFlags & hkArrayBase<hkpLinkedCollidable::CollisionEntry>::CAPACITY_MASK))
    {
        hkArrayUtil::_reduce(hkContainerHeapAllocator::s_alloc, &entries, sizeof(hkpLinkedCollidable::CollisionEntry), nullptr, capacity);
    }
}

// The last entry of a track (the one moved into a freed slot).
inline hkpAgentNnEntry* hkNnMachine_lastEntry(hkpAgentNnTrack& track)
{
    hkpAgentNnSector* sector = track.m_sectors[track.m_sectors.getSize() - 1];
    return reinterpret_cast<hkpAgentNnEntry*>(&sector->m_data[track.m_bytesUsedInLastSector - track.getAgentSize()]);
}

// Drops the last entry: frees the last sector when it becomes empty, then shrinks the sector array.
void hkNnMachine_popLastEntry(hkpAgentNnTrack& track)
{
    track.m_bytesUsedInLastSector = (unsigned short)(track.m_bytesUsedInLastSector - track.getAgentSize());
    if (track.m_bytesUsedInLastSector == 0)
    {
        hkpAgentNnSector* sector = track.m_sectors[track.m_sectors.getSize() - 1];
        track.m_sectors.popBack();
        hkMemoryRouter::getInstance().heap().blockFree(sector, HK_AGENT3_SECTOR_SIZE);
        track.m_bytesUsedInLastSector = HK_AGENT3_SECTOR_SIZE;
    }
    hkNnMachine_optimizeSectors(track);
}

// Copies an entry (the track's entry size) and points both collidables' collision entries at the copy.
void hkNnMachine_moveEntry(hkpAgentNnEntry* to, const hkpAgentNnEntry* from, int size)
{
    // the original copies 16 bytes per step
    memcpy(to, from, size);
    to->m_collidable[0]->m_collisionEntries[to->m_agentIndexOnCollidable[0]].m_agentEntry = to;
    to->m_collidable[1]->m_collisionEntries[to->m_agentIndexOnCollidable[1]].m_agentEntry = to;
}

// hkQuaternion::setMul (inlined): xyz = (q0 x q1 + q1 * w0) + q0 * w1, w = w1 * w0 - q0.q1 (hkptransformagent.cpp)
inline Q hkNnMachine_quatMul(Q q0, Q q1)
{
    const Q w0 = bcast<3>(q0);
    const Q w1 = bcast<3>(q1);
    const Q xyz = add(add(cross(q0, q1), mul(q1, w0)), mul(q0, w1));
    const Q w = sub(mul(w1, w0), dot3(q0, q1));
    return setXYZ_W(xyz, w);
}

// The input of an entry's process function: the bodies, the contact manager, the tim info of the step and aTb.
void hkNnMachine_initProcessInput(hkpAgent3ProcessInput& in3, const hkpCdBody* bodyA, const hkpCdBody* bodyB, hkpContactMgr* mgr, const hkpProcessCollisionInput& input)
{
    in3.m_contactMgr = mgr;
    in3.m_overrideBodyA = nullptr;
    in3.m_input = &input;
    const hkMotionState* msA = static_cast<const hkMotionState*>(bodyA->getMotionState());
    const hkMotionState* msB = static_cast<const hkMotionState*>(bodyB->getMotionState());
    hkSweptTransformUtil::calcTimInfo(*msA, *msB, input.m_stepInfo.m_deltaTime.val(), in3.m_linearTimInfo);
    in3.m_bodyA = bodyA;
    in3.m_bodyB = bodyB;
}

// The TIM test of a tim entry before its process call (the shared part of ProcessAgent and ProcessTrack). Returns
// false when the bodies stay apart for the step: the entry's points are then cleaned up and it is not processed.
// Otherwise in3.m_distAtT1 is set.
bool hkNnMachine_checkTim(hkpAgentNnEntry* entry, hkpAgent3ProcessInput& in3, const hkpProcessCollisionInput& input, hkpConstraintOwner* owner)
{
    hkpAgentNnMachineTimEntry* e = hkNnMachine_tim(entry);
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    if (e->m_timeOfSeparatingNormal != input.m_stepInfo.m_startTime.val())
    {
        if (!input.m_collisionQualityInfo->m_useContinuousPhysics.val())
        {
            // no separating normal: process with an unknown distance
            e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
            e->m_separatingNormal.m_quad = setXYZ_W(hkVector4f::getConstant<HK_QUADREAL_0>().m_quad, hkVector4f::getConstant<HK_QUADREAL_MINUS_MAX>().m_quad);
            in3.m_distAtT1 = lane0(hkVector4f::getConstant<HK_QUADREAL_MINUS_MAX>().m_quad);
            return true;
        }
        // the separating normal at the start of the step
        alignas(16) hkTransformf transformA;
        alignas(16) hkTransformf transformB;
        hkpAgent3Input sepInput;
        sepInput.m_overrideBodyA = nullptr;
        const hkpCdBody* bodyA = in3.m_bodyA;
        const hkpCdBody* bodyB = in3.m_bodyB;
        hkSweptTransformUtil::lerp2(static_cast<const hkMotionState*>(bodyA->getMotionState())->getSweptTransform(), input.m_stepInfo.m_startTime.val(), transformA);
        hkSweptTransformUtil::lerp2(static_cast<const hkMotionState*>(bodyB->getMotionState())->getSweptTransform(), in3.m_input->m_stepInfo.m_startTime.val(), transformB);
        hkpCdBody tempA(bodyA, &transformA);
        hkpCdBody tempB(bodyB, &transformB);
        tempA.setShape(bodyA->getShape(), bodyA->getShapeKey());
        tempB.setShape(bodyB->getShape(), bodyB->getShapeKey());
        sepInput.m_contactMgr = in3.m_contactMgr;
        sepInput.m_bodyA = &tempA;
        sepInput.m_input = in3.m_input;
        sepInput.m_bodyB = &tempB;
        sepInput.m_aTb.setMulInverseMul(transformA, transformB);
        dispatcher->m_agent3Func[entry->m_agentType].m_sepNormalFunc(sepInput, entry, hkNnMachine_data(entry, kTimEntryDataOffset), e->m_separatingNormal);
    }
    // the distance at the end of the step: normal.w - linearTimInfo.dot4xyz1(normal)
    e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
    const Q n = e->m_separatingNormal.m_quad;
    const Q lti = in3.m_linearTimInfo.m_quad;
    Q prod = _mm_mul_ps(lti, n);
    prod = _mm_shuffle_ps(prod, _mm_unpackhi_ps(prod, lti), 0xC4); // (x, y, z, lti.w)
    Q sum = _mm_add_ps(_mm_shuffle_ps(prod, prod, 0x4E), prod);
    sum = _mm_add_ps(_mm_shuffle_ps(sum, sum, 0xB1), sum);
    const Q dist = _mm_sub_ps(_mm_shuffle_ps(n, n, 0xFF), sum);
    const Q tolerance = _mm_set1_ps(input.m_tolerance.val());
    if (lessOrNaN(dist, tolerance))
    {
        in3.m_distAtT1 = lane0(dist);
        return true;
    }
    // apart for the whole step: keep the distance in the normal's w
    e->m_separatingNormal.m_quad = setXYZ_W(n, dist);
    if (entry->m_numContactPoints != 0)
    {
        dispatcher->m_agent3Func[entry->m_agentType].m_cleanupFunc(entry, hkNnMachine_data(entry, kTimEntryDataOffset), in3.m_contactMgr, *owner);
    }
    return false;
}

// One entry: transform shapes first (bodies replaced by their children in moved motion states), then the agent.
// The transformed bodies live in the caller's arrays (4 levels of transform shapes per side).
struct NnTransformBuffers
{
    hkpCdBody m_bodiesA[4];
    hkpCdBody m_bodiesB[4];
    hkMotionState m_motionsA[4];
    hkMotionState m_motionsB[4];
};

void hkNnMachine_processEntry(hkpAgentNnEntry* entry, const hkpCdBody* bodyA, const hkpCdBody* bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, NnTransformBuffers& buffers, int* timRejected)
{
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    unsigned int command = entry->m_streamCommand;
    for (;;)
    {
        switch (command)
        {
        case hkAgent3::STREAM_CALL:
        {
            hkpAgent3ProcessInput in3;
            hkNnMachine_initProcessInput(in3, bodyA, bodyB, entry->m_contactMgr, input);
            in3.m_aTb.setMulInverseMul(bodyA->getTransform(), bodyB->getTransform());
            dispatcher->m_agent3Func[entry->m_agentType].m_processFunc(in3, entry, hkNnMachine_data(entry, kPaddedEntryDataOffset), nullptr, output);
            return;
        }
        case hkAgent3::STREAM_CALL_WITH_TIM:
        {
            hkpAgent3ProcessInput in3;
            hkNnMachine_initProcessInput(in3, bodyA, bodyB, entry->m_contactMgr, input);
            if (!hkNnMachine_checkTim(entry, in3, input, output.m_constraintOwner))
            {
                if (timRejected)
                {
                    (*timRejected)++;
                }
                return;
            }
            in3.m_aTb.setMulInverseMul(bodyA->getTransform(), bodyB->getTransform());
            dispatcher->m_agent3Func[entry->m_agentType].m_processFunc(in3, entry, hkNnMachine_data(entry, kTimEntryDataOffset), &hkNnMachine_tim(entry)->m_separatingNormal, output);
            return;
        }
        case hkAgent3::STREAM_CALL_AGENT:
        {
            hkpCollisionAgent* agent = *reinterpret_cast<hkpCollisionAgent**>(hkNnMachine_data(entry, kPaddedEntryDataOffset));
            agent->processCollision(*bodyA, *bodyB, input, output);
            return;
        }
        case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
        {
            hkPadSpu<unsigned char> flag;
            bodyA = hkAgentMachine_processTransformedShapes(bodyA, buffers.m_bodiesA, buffers.m_motionsA, 4, flag);
            bodyB = hkAgentMachine_processTransformedShapes(bodyB, buffers.m_bodiesB, buffers.m_motionsB, 4, flag);
            command &= ~hkAgent3::TRANSFORM_FLAG;
            continue;
        }
        default:
            return;
        }
    }
}
}

// 0x00DDAAB0
void hkAgentNnMachine_InvalidateTimInAgent(hkpAgentNnEntry* entry, const hkpCollisionInput& input)
{
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    switch (entry->m_streamCommand)
    {
    case hkAgent3::STREAM_CALL:
    case hkAgent3::STREAM_CALL_AGENT:
    case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
    case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
    {
        hkAgent3::InvalidateTimFunc f = dispatcher->m_agent3Func[entry->m_agentType].m_invalidateTimFunc;
        if (f)
        {
            f(entry, hkNnMachine_data(entry, kPaddedEntryDataOffset), input);
        }
        break;
    }
    case hkAgent3::STREAM_CALL_WITH_TIM:
    case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
    {
        hkpAgentNnMachineTimEntry* e = hkNnMachine_tim(entry);
        e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
        e->m_separatingNormal.setZero();
        hkAgent3::InvalidateTimFunc f = dispatcher->m_agent3Func[entry->m_agentType].m_invalidateTimFunc;
        if (f)
        {
            f(entry, hkNnMachine_data(entry, kTimEntryDataOffset), input);
        }
        break;
    }
    default:
        break;
    }
}

// 0x00DDAB50: a tim taken at oldTime moves to newTime; any other is invalidated (its normal is kept)
void hkAgentNnMachine_WarpTimeInAgent(hkpAgentNnEntry* entry, float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    switch (entry->m_streamCommand)
    {
    case hkAgent3::STREAM_CALL:
    case hkAgent3::STREAM_CALL_AGENT:
    case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
    case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
    {
        hkAgent3::WarpTimeFunc f = dispatcher->m_agent3Func[entry->m_agentType].m_warpTimeFunc;
        if (f)
        {
            f(entry, hkNnMachine_data(entry, kPaddedEntryDataOffset), oldTime, newTime, input);
        }
        break;
    }
    case hkAgent3::STREAM_CALL_WITH_TIM:
    case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
    {
        hkpAgentNnMachineTimEntry* e = hkNnMachine_tim(entry);
        if (e->m_timeOfSeparatingNormal == oldTime)
        {
            e->m_timeOfSeparatingNormal = newTime;
        }
        else
        {
            e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
        }
        hkAgent3::WarpTimeFunc f = dispatcher->m_agent3Func[entry->m_agentType].m_warpTimeFunc;
        if (f)
        {
            f(entry, hkNnMachine_data(entry, kTimEntryDataOffset), oldTime, newTime, input);
        }
        break;
    }
    default:
        break;
    }
}

// 0x00DDAC40: searched on the collidable with fewer entries
hkpAgentNnEntry* hkAgentNnMachine_FindAgent(const hkpLinkedCollidable* collA, const hkpLinkedCollidable* collB)
{
    const hkpLinkedCollidable* search = collB;
    const hkpLinkedCollidable* partner = collA;
    if (collA->m_collisionEntries.getSize() <= collB->m_collisionEntries.getSize())
    {
        search = collA;
        partner = collB;
    }
    for (int i = 0; i < search->m_collisionEntries.getSize(); i++)
    {
        if (search->m_collisionEntries[i].m_partner == partner)
        {
            return search->m_collisionEntries[i].m_agentEntry;
        }
    }
    return nullptr;
}

// 0x00DDADA0: the agent3 type of the pair (the predictive table when the input asks for predictive agents); a pair
// that only has the agent2 bridge (type 1) is flipped when its agent2 is
void hkAgentNnMachine_GetAgentType(const hkpCdBody* cdBodyA, const hkpCdBody* cdBodyB, const hkpProcessCollisionInput& input, int& agentTypeOut, int& isFlippedOut)
{
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    const int typeA = cdBodyA->getShape()->m_type.m_storage;
    const int typeB = cdBodyB->getShape()->m_type.m_storage;
    const int agentType = input.m_createPredictiveAgents.val() == 0 ? dispatcher->m_agent3Types[typeA][typeB] : dispatcher->m_agent3TypesPred[typeA][typeB];
    bool flipped = dispatcher->m_agent3Func[agentType].m_symmetric == hkAgent3::IS_NOT_SYMMETRIC_AND_FLIPPED;
    if (agentType == 1)
    {
        flipped = dispatcher->m_agent2Func[dispatcher->m_agent2Types[typeA][typeB]].m_isFlipped.m_bool != 0;
    }
    isFlippedOut = flipped;
    if (flipped)
    {
        agentTypeOut = input.m_createPredictiveAgents.val() != 0 ? dispatcher->m_agent3TypesPred[typeB][typeA] : dispatcher->m_agent3Types[typeB][typeA];
        return;
    }
    agentTypeOut = agentType;
}

// 0x00DDAE60: the last entry of the track moves into the freed slot
void hkAgentNnMachine_InternalDeallocateEntry(hkpAgentNnTrack& track, hkpAgentNnEntry* entry)
{
    hkpAgentNnEntry* last = hkNnMachine_lastEntry(track);
    if (entry != last)
    {
        hkNnMachine_moveEntry(entry, last, track.getAgentSize());
    }
    hkNnMachine_popLastEntry(track);
}

// 0x00DDAF70: replaces a chain of transform shapes by its first other shape, each level a body in the caller's
// arrays whose motion state is the parent's moved by the shape's transform (as hkpTransformAgent::processCollision).
// Sets the transform flag of the stream command when a transform shape was found.
const hkpCdBody* hkAgentMachine_processTransformedShapes(const hkpCdBody* cdBody, hkpCdBody* newCdBodies, hkMotionState* newMotionStates, int numSlots, hkPadSpu<unsigned char>& cdBodyHasTransformFlag)
{
    while (cdBody->getShape()->m_type.m_storage == hkcdShapeType::TRANSFORM)
    {
        const hkpTransformShape* shape = static_cast<const hkpTransformShape*>(cdBody->getShape());
        const hkMotionState& parent = *static_cast<const hkMotionState*>(cdBody->getMotionState());
        hkMotionState& motion = *newMotionStates;
        motion.getTransform().setMul(parent.getTransform(), shape->m_transform);
        hkSweptTransformf& swept = motion.m_sweptTransform;
        const hkSweptTransformf& parentSwept = parent.m_sweptTransform;
        swept.m_centerOfMass0 = parentSwept.m_centerOfMass0;
        swept.m_centerOfMass1 = parentSwept.m_centerOfMass1;
        swept.m_rotation0.m_vec.m_quad = hkNnMachine_quatMul(parentSwept.m_rotation0.m_vec.m_quad, shape->m_rotation.m_vec.m_quad);
        swept.m_rotation1.m_vec.m_quad = hkNnMachine_quatMul(parentSwept.m_rotation1.m_vec.m_quad, shape->m_rotation.m_vec.m_quad);
        swept.m_centerOfMassLocal.m_quad = rotateInverse(shape->m_transform.getRotation(), sub(parentSwept.m_centerOfMassLocal.m_quad, shape->m_transform.getTranslation().m_quad));
        motion.m_deltaAngle = parent.m_deltaAngle;
        motion.m_objectRadius = parent.m_objectRadius;
        newCdBodies->m_parent = cdBody;
        newCdBodies->m_motion = &motion;
        newCdBodies->setShape(shape->m_childShape.m_childShape, 0);
        cdBodyHasTransformFlag = hkAgent3::TRANSFORM_FLAG;
        cdBody = newCdBodies;
        newCdBodies++;
        newMotionStates++;
    }
    return cdBody;
}

// 0x00DDB7F0 (LTCG: the track in EDI): a new entry at the end of the track, in a new sector when the last one is full
static hkpAgentNnEntry* hkAgentNnMachine_AllocateEntry(hkpAgentNnTrack& track)
{
    const unsigned int used = track.m_bytesUsedInLastSector;
    if (used < HK_AGENT3_SECTOR_SIZE)
    {
        hkpAgentNnSector* sector = track.m_sectors[track.m_sectors.getSize() - 1];
        track.m_bytesUsedInLastSector = (unsigned short)(used + track.getAgentSize());
        return reinterpret_cast<hkpAgentNnEntry*>(&sector->m_data[used]);
    }
    hkpAgentNnSector* sector = static_cast<hkpAgentNnSector*>(hkMemoryRouter::getInstance().heap().blockAlloc(HK_AGENT3_SECTOR_SIZE));
    track.m_sectors.pushBack(sector);
    track.m_bytesUsedInLastSector = (unsigned short)track.getAgentSize();
    return sector->getBegin();
}

// 0x00DDB880: links the entry into both collidables, picks the quality of the pair and creates the agent (a tim
// entry when the agent has a separating normal function, its tim invalid)
hkpAgentNnEntry* hkAgentNnMachine_CreateAgent(hkpAgentNnTrack& track, hkpLinkedCollidable* collA, const hkpCdBody* firstNonTransformBodyA, hkpLinkedCollidable* collB, const hkpCdBody* firstNonTransformBodyB, unsigned char cdBodyHasTransformFlag, int agentType, const hkpProcessCollisionInput& input, hkpContactMgr* mgr)
{
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    hkpAgentNnEntry* entry = hkAgentNnMachine_AllocateEntry(track);
    entry->m_contactMgr = mgr;
    entry->m_collidable[0] = collA;
    entry->m_nnTrackType = track.m_nnTrackType;
    entry->m_collidable[1] = collB;
    entry->m_agentIndexOnCollidable[0] = (unsigned short)collA->m_collisionEntries.getSize();
    entry->m_agentIndexOnCollidable[1] = (unsigned short)collB->m_collisionEntries.getSize();
    hkpLinkedCollidable::CollisionEntry& entryA = collA->m_collisionEntries.expandOne();
    hkpLinkedCollidable::CollisionEntry& entryB = collB->m_collisionEntries.expandOne();
    entryA.m_agentEntry = entry;
    entryB.m_agentEntry = entry;
    entryA.m_partner = collB;
    entryB.m_partner = collA;

    hkpAgent3Input in3;
    in3.m_contactMgr = entry->m_contactMgr;
    in3.m_bodyB = firstNonTransformBodyB;
    in3.m_overrideBodyA = nullptr;
    in3.m_bodyA = firstNonTransformBodyA;
    in3.m_input = &input;
    in3.m_aTb.setMulInverseMul(firstNonTransformBodyA->getTransform(), firstNonTransformBodyB->getTransform());

    const unsigned char quality = input.m_dispatcher.val()->m_collisionQualityTable[collA->m_broadPhaseHandle.m_objectQualityType][collB->m_broadPhaseHandle.m_objectQualityType];
    entry->m_collisionQualityIndex = quality;
    // Original behaviour kept: the input's quality info is left pointing at this pair's quality.
    const_cast<hkpProcessCollisionInput&>(input).m_collisionQualityInfo = &input.m_dispatcher.val()->m_collisionQualityInfo[(signed char)quality];
    entry->m_agentType = (unsigned char)agentType;
    hkpAgentData* agentData;
    if (dispatcher->m_agent3Func[agentType].m_sepNormalFunc == nullptr)
    {
        entry->m_streamCommand = (unsigned char)(cdBodyHasTransformFlag | hkAgent3::STREAM_CALL);
        agentData = hkNnMachine_data(entry, kPaddedEntryDataOffset);
    }
    else
    {
        hkpAgentNnMachineTimEntry* e = hkNnMachine_tim(entry);
        e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
        entry->m_streamCommand = (unsigned char)(cdBodyHasTransformFlag | hkAgent3::STREAM_CALL_WITH_TIM);
        e->m_separatingNormal.setZero();
        agentData = hkNnMachine_data(entry, kTimEntryDataOffset);
    }
    dispatcher->m_agent3Func[agentType & 0xFF].m_createFunc(in3, entry, agentData);
    entry->m_size = (unsigned char)(track.m_nnTrackType.m_storage << 6);
    return entry;
}

// 0x00DDBA10: destroys the agent, unlinks the entry from both collidables (the last collision entry moves into its
// slot) and frees it
void hkAgentNnMachine_DestroyAgent(hkpAgentNnTrack& track, hkpAgentNnEntry* entry, hkpCollisionDispatcher* dispatcher, hkpConstraintOwner& constraintOwner)
{
    switch (entry->m_streamCommand)
    {
    case hkAgent3::STREAM_CALL:
    case hkAgent3::STREAM_CALL_AGENT:
    case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
    case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
        dispatcher->m_agent3Func[entry->m_agentType].m_destroyFunc(entry, hkNnMachine_data(entry, kPaddedEntryDataOffset), entry->m_contactMgr, constraintOwner, dispatcher);
        break;
    case hkAgent3::STREAM_CALL_WITH_TIM:
    case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        dispatcher->m_agent3Func[entry->m_agentType].m_destroyFunc(entry, hkNnMachine_data(entry, kTimEntryDataOffset), entry->m_contactMgr, constraintOwner, dispatcher);
        break;
    default:
        break;
    }
    for (int side = 0; side < 2; side++)
    {
        hkpLinkedCollidable* coll = entry->m_collidable[side];
        const unsigned short index = entry->m_agentIndexOnCollidable[side];
        hkArray<hkpLinkedCollidable::CollisionEntry, hkContainerHeapAllocator>& entries = coll->m_collisionEntries;
        entries.removeAt(index);
        if (index < entries.getSize())
        {
            hkpAgentNnEntry* moved = entries[index].m_agentEntry;
            moved->m_agentIndexOnCollidable[moved->m_collidable[0] != coll ? 1 : 0] = index;
        }
        hkNnMachine_optimizeEntries(entries);
    }
    hkAgentNnMachine_InternalDeallocateEntry(track, entry);
}

// 0x00DDBB70: the agent's filter update on the entry's (transformed) bodies
void hkAgentNnMachine_UpdateShapeCollectionFilter(hkpAgentNnEntry* entry, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    const hkpCdBody* bodyA = entry->m_collidable[0];
    const hkpCdBody* bodyB = entry->m_collidable[1];
    NnTransformBuffers buffers;
    unsigned int command = entry->m_streamCommand;
    for (;;)
    {
        hkpAgentData* data;
        switch (command)
        {
        case hkAgent3::STREAM_CALL:
        case hkAgent3::STREAM_CALL_AGENT:
            data = hkNnMachine_data(entry, kPaddedEntryDataOffset);
            break;
        case hkAgent3::STREAM_CALL_WITH_TIM:
            data = hkNnMachine_data(entry, kTimEntryDataOffset);
            break;
        case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
        {
            hkPadSpu<unsigned char> flag;
            bodyA = hkAgentMachine_processTransformedShapes(bodyA, buffers.m_bodiesA, buffers.m_motionsA, 4, flag);
            bodyB = hkAgentMachine_processTransformedShapes(bodyB, buffers.m_bodiesB, buffers.m_motionsB, 4, flag);
            command &= ~hkAgent3::TRANSFORM_FLAG;
            continue;
        }
        default:
            return;
        }
        hkAgent3::UpdateFilterFunc f = input.m_dispatcher.val()->m_agent3Func[entry->m_agentType].m_updateFilterFunc;
        if (f)
        {
            f(entry, data, *bodyA, *bodyB, input, entry->m_contactMgr, constraintOwner);
        }
        return;
    }
}

// 0x00DDBC70: a copy of the entry at the end of another track, relinked into its collidables
hkpAgentNnEntry* hkAgentNnMachine_CopyAndRelinkAgentEntry(hkpAgentNnTrack& destTrack, hkpAgentNnEntry* entry)
{
    hkpAgentNnEntry* copy = hkAgentNnMachine_AllocateEntry(destTrack);
    hkNnMachine_moveEntry(copy, entry, destTrack.getAgentSize());
    return copy;
}

// 0x00DDBD10
void hkAgentNnMachine_ProcessAgent(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, hkpContactMgr* mgr)
{
    NnTransformBuffers buffers;
    hkNnMachine_processEntry(entry, entry->m_collidable[0], entry->m_collidable[1], input, output, buffers, nullptr);
}

// 0x00DDC240: moves the appendee's entries into the track: one by one from its end while the track's last sector is
// partly used, then its remaining sectors as a whole
void hkAgentNnMachine_AppendTrack(hkpAgentNnTrack& track, hkpAgentNnTrack& appendee)
{
    while (track.m_bytesUsedInLastSector != HK_AGENT3_SECTOR_SIZE && appendee.m_sectors.getSize() != 0)
    {
        hkAgentNnMachine_CopyAndRelinkAgentEntry(track, hkNnMachine_lastEntry(appendee));
        hkNnMachine_popLastEntry(appendee);
    }
    const int numSectors = appendee.m_sectors.getSize();
    if (numSectors != 0)
    {
        track.m_sectors.insertAt(track.m_sectors.getSize(), appendee.m_sectors.begin(), numSectors);
        track.m_bytesUsedInLastSector = appendee.m_bytesUsedInLastSector;
        appendee.m_sectors.m_size = 0;
        appendee.m_bytesUsedInLastSector = HK_AGENT3_SECTOR_SIZE;
    }
}

// 0x00DDC380: every entry of the track, each into a fresh output that goes to the entry's contact manager when it
// holds points; stops when the heap ran out of memory
void hkAgentNnMachine_ProcessTrack(hkpConstraintOwner* owner, hkpAgentNnTrack& agentTrack, const hkpProcessCollisionInput& input)
{
    hkpProcessCollisionOutput output(owner);
    int timRejected = 0; // the "MinumTim" monitor value
    NnTransformBuffers buffers;
    for (int sectorIndex = 0; sectorIndex < agentTrack.m_sectors.getSize();)
    {
        hkpAgentNnSector* sector = agentTrack.m_sectors[sectorIndex];
        hkpAgentNnEntry* entry = sector->getBegin();
        sectorIndex++;
        hkpAgentNnEntry* end = sectorIndex == agentTrack.m_sectors.getSize() ? hkAddByteOffset(entry, agentTrack.m_bytesUsedInLastSector) : sector->getEnd();
        for (; entry < end; entry = hkAddByteOffset(entry, entry->m_size))
        {
            hkpLinkedCollidable* collA = entry->m_collidable[0];
            hkpLinkedCollidable* collB = entry->m_collidable[1];
            output.reset();
            hkNnMachine_processEntry(entry, collA, collB, input, output, buffers, &timRejected);
            if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
            {
                return;
            }
            if (!output.isEmpty())
            {
                entry->m_contactMgr->processContactImpl(*collA, *collB, input, output);
            }
        }
    }
}
