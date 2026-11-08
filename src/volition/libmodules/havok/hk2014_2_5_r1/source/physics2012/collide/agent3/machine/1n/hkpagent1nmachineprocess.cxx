// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx
//
// Havok 2014.2.5 (havok_impl.md HK6): the process functions of the 1n agent machine and its filter update visitors,
// from rfg.exe. hkAgent1nMachine_Process (a body against the children of a collection B) and
// hkAgentNmMachine_Process (children of A against children of B) are the same stream walk, which the SDK builds
// twice from this file; here one template with a policy per machine:
// - the stream of the last step is merged with the sorted new keys (ending with the invalid key, as the end entry):
//   a kept key's entry is processed, a new key gets an agent (or a null entry when the filter disables it), an entry
//   whose key is gone is destroyed; without new keys every entry is processed.
// - each entry is processed by its stream command: the agent's process function (flipped input for the flipped
//   commands, the points flipped back after), the tim test before a tim agent, the bridge's agent2, transform
//   shapes first replaced by their children.
// - the stream is rewritten into fresh sectors as it goes (the input's spare sector, the sectors read so far), an
//   entry that does not fit the rest of a sector first to a 128-byte overflow buffer.
// - the 1n machine welds: with a welding collection and no caller's potential contacts, the agents' potential points
//   are collected and hkAgent1nMachine_Weld settles them at the end.
// No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "MinumTim" monitor value; it is not reproduced.

// The tim entries' dist is set on every path that reads it (by the else branch or by the check). An initializer changes
// the generated code of these bit-exact functions, and C4701 can only be disabled outside a function.
#pragma warning(disable : 4701)

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

namespace
{
typedef hkpProcessCollisionOutput::PotentialInfo PotentialInfo;
typedef hkpProcessCollisionOutput::ContactRef ContactRef;

const int kNetSectorSize = hkpAgent1nSector::NET_SECTOR_SIZE; // 0x1F0
const int kMaxEntrySize = hkAgent3::MAX_SIZE; // 0x80

inline hkpAgent1nSector* hk1nTakeSector(const hkpProcessCollisionInput& input)
{
    hkpAgent1nSector* sector = input.m_spareAgentSector;
    if (sector == nullptr)
    {
        sector = static_cast<hkpAgent1nSector*>(hkMemoryRouter::getInstance().heap().blockAlloc(hkpAgent1nSector::SECTOR_SIZE));
        sector->m_bytesAllocated = 0;
    }
    input.m_spareAgentSector = nullptr;
    return sector;
}

// Copies 16 bytes at a time (at least once).
inline void hk1nCopy(void* to, const void* from, int size)
{
    int blocks = size >> 4;
    const char* src = static_cast<const char*>(from);
    char* dst = static_cast<char*>(to);
    do
    {
        memcpy(dst, src, 16);
        dst += 16;
        src += 16;
    } while (--blocks > 0);
}

// The tim distance at the end of the step: normal.w - linearTimInfo.dot4xyz1(normal), as the SSE sequence sums it.
inline Q hk1nTimDistance(Q n, Q lti)
{
    Q prod = _mm_mul_ps(lti, n);
    prod = _mm_shuffle_ps(prod, _mm_unpackhi_ps(prod, lti), 0xC4); // (x, y, z, lti.w)
    Q sum = _mm_add_ps(_mm_shuffle_ps(prod, prod, 0x4E), prod);
    sum = _mm_add_ps(_mm_shuffle_ps(sum, sum, 0xB1), sum);
    return _mm_sub_ps(_mm_shuffle_ps(n, n, 0xFF), sum);
}

// The points a flipped agent added, back in the unflipped frame: moved along the normal by their distance, the
// normal negated (its distance kept).
inline void hk1nFlipPoints(hkpProcessCdPoint* from, hkpProcessCollisionOutput& output)
{
    for (hkpProcessCdPoint* p = from; p < output.m_firstFreeContactPoint.val(); p++)
    {
        const Q n = p->m_contact.getSeparatingNormal().m_quad;
        const Q pos = p->m_contact.getPosition().m_quad;
        p->m_contact.m_position.m_quad = _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(n, n, 0xFF), n), pos);
        p->m_contact.m_separatingNormal.m_quad = setXYZ_W(_mm_xor_ps(n, _mm_set1_ps(-0.0f)), n);
    }
}

// Moves the potential contacts of entries copied out of the overflow buffer (all of them, or the last one only).
inline void hk1nRelocatePotentials(PotentialInfo* potentials, const char* extension, int extensionBytes, char* to, bool lastOnly)
{
    if (potentials == nullptr || potentials->m_firstFreePotentialContact <= &potentials->m_potentialContacts[0])
    {
        return;
    }
    ContactRef* first = lastOnly ? potentials->m_firstFreePotentialContact - 1 : &potentials->m_potentialContacts[0];
    for (ContactRef* ref = first; ref < potentials->m_firstFreePotentialContact; ref++)
    {
        const int offset = (int)(reinterpret_cast<char*>(ref->m_agentEntry) - extension);
        if (offset >= 0 && offset < extensionBytes)
        {
            ref->m_agentEntry = reinterpret_cast<hkpAgentEntry*>(to + offset);
            ref->m_agentData = static_cast<char*>(ref->m_agentData) + (to - extension);
        }
    }
}

struct TransformBuffers
{
    hkpCdBody m_bodies[4];
    hkMotionState m_motions[4];
};

// ---- the 1n machine: a body against the children of collection B (keys: B's child keys)
struct Machine1n
{
    enum
    {
        WELDING = 1
    };

    const hkpShapeContainer* m_containerB;
    const unsigned int* m_newKeys;
    // Original behaviour kept: the child key is only set from the new keys, so without new keys every entry is
    // processed with the child of the invalid key.
    unsigned int m_key = 0xFFFFFFFF;
    hkpCdBody m_childB;
    hkpShapeBuffer m_bufferB;
    TransformBuffers m_transformB;

    void init(const hkpAgent3ProcessInput& inputIn, hkpAgent3ProcessInput& in)
    {
        m_childB.m_parent = inputIn.m_bodyB;
        m_childB.m_motion = inputIn.m_bodyB.val()->m_motion;
        in.m_bodyB = &m_childB;
    }
    void resetBodies(hkpAgent3ProcessInput& in) { in.m_bodyB = &m_childB; }
    bool hasNewKeys() const { return m_newKeys != nullptr; }
    int compare(const hkpAgent1nMachineEntry* e)
    {
        m_key = *m_newKeys;
        const unsigned int key = e->m_shapeKeyPair.m_shapeKeyB;
        if (m_key == key)
        {
            return 0;
        }
        return m_key <= key ? -1 : 1;
    }
    void advance() { m_newKeys++; }
    void fetch()
    {
        m_childB.m_shape = m_containerB->getChildShape(m_key, m_bufferB);
        m_childB.m_shapeKey = m_key;
    }
    bool isEnabled(const hkpProcessCollisionInput& input, const hkpAgent3ProcessInput& inputIn) const
    {
        const hkpShapeCollectionFilter* filter = input.m_filter.val();
        return filter->isCollisionEnabled(input, *inputIn.m_bodyA.val(), *inputIn.m_bodyB.val(), *m_containerB, m_key).m_bool != 0;
    }
    void writeNull(hkpAgent1nMachineEntry* e) const
    {
        *reinterpret_cast<unsigned int*>(e) = 0;
        e->m_shapeKeyPair.m_shapeKeyA = 0xFFFFFFFF;
        e->m_shapeKeyPair.m_shapeKeyB = m_key;
        e->m_size = 0x10;
    }
    void writeKeys(hkpAgent1nMachineEntry* e) const
    {
        e->m_shapeKeyPair.m_shapeKeyB = m_key;
        e->m_shapeKeyPair.m_shapeKeyA = 0xFFFFFFFF;
    }
    void transform(hkpAgent3ProcessInput& in, hkPadSpu<unsigned char>& flag)
    {
        in.m_bodyB = hkAgentMachine_processTransformedShapes(in.m_bodyB, m_transformB.m_bodies, m_transformB.m_motions, 4, flag);
    }
    static void timTemp(hkpAgentNmMachineBodyTemp& temp, const hkpAgent3ProcessInput& in)
    {
        temp.m_bodyB.setShape(in.m_bodyB.val()->getShape(), in.m_bodyB.val()->getShapeKey());
    }
    static void timTempFlipped(hkpAgentNmMachineBodyTemp& temp, const hkpAgent3ProcessInput& in)
    {
        temp.m_bodyA.setShape(in.m_bodyB.val()->getShape(), in.m_bodyB.val()->getShapeKey());
    }
};

// ---- the Nm machine: children of A against children of B (keys: pairs sorted by B then A); the children are
// fetched again only when their key changes
struct MachineNm
{
    enum
    {
        WELDING = 0
    };

    const hkpShapeContainer* m_containerA;
    const hkpShapeContainer* m_containerB;
    const hkpShapeKeyPair* m_newKeys;
    unsigned int m_keyA = 0xFFFFFFFF;
    unsigned int m_keyB = 0xFFFFFFFF;
    unsigned int m_cachedA = 0xFFFFFFFF;
    unsigned int m_cachedB = 0xFFFFFFFF;
    hkpCdBody m_childA;
    hkpCdBody m_childB;
    hkpShapeBuffer m_bufferA;
    hkpShapeBuffer m_bufferB;
    TransformBuffers m_transformA;
    TransformBuffers m_transformB;

    void init(const hkpAgent3ProcessInput& inputIn, hkpAgent3ProcessInput& in)
    {
        m_childB.m_parent = inputIn.m_bodyB;
        m_childA.m_parent = inputIn.m_bodyA;
        m_childB.m_motion = inputIn.m_bodyB.val()->m_motion;
        in.m_bodyB = &m_childB;
        m_childA.m_motion = inputIn.m_bodyA.val()->m_motion;
        in.m_bodyA = &m_childA;
    }
    void resetBodies(hkpAgent3ProcessInput& in)
    {
        in.m_bodyA = &m_childA;
        in.m_bodyB = &m_childB;
    }
    bool hasNewKeys() const { return m_newKeys != nullptr; }
    int compare(const hkpAgent1nMachineEntry* e)
    {
        m_keyA = m_newKeys->m_shapeKeyA;
        m_keyB = m_newKeys->m_shapeKeyB;
        const unsigned int a = e->m_shapeKeyPair.m_shapeKeyA;
        const unsigned int b = e->m_shapeKeyPair.m_shapeKeyB;
        if (m_keyB == b && m_keyA == a)
        {
            return 0;
        }
        if (m_keyB <= b && (m_keyB != b || m_keyA <= a))
        {
            return -1;
        }
        return 1;
    }
    void advance() { m_newKeys++; }
    void fetch()
    {
        if (m_cachedA != m_keyA)
        {
            m_childA.m_shape = m_containerA->getChildShape(m_keyA, m_bufferA);
            m_cachedA = m_keyA;
            m_childA.m_shapeKey = m_keyA;
        }
        if (m_cachedB != m_keyB)
        {
            m_childB.m_shape = m_containerB->getChildShape(m_keyB, m_bufferB);
            m_cachedB = m_keyB;
            m_childB.m_shapeKey = m_keyB;
        }
    }
    bool isEnabled(const hkpProcessCollisionInput& input, const hkpAgent3ProcessInput& inputIn) const
    {
        const hkpShapeCollectionFilter* filter = input.m_filter.val();
        return filter->isCollisionEnabled(input, *inputIn.m_bodyA.val(), *inputIn.m_bodyB.val(), *m_containerA, *m_containerB, m_newKeys->m_shapeKeyA, m_newKeys->m_shapeKeyB).m_bool != 0;
    }
    void writeNull(hkpAgent1nMachineEntry* e) const
    {
        *reinterpret_cast<unsigned int*>(e) = 0;
        e->m_shapeKeyPair.m_shapeKeyB = m_keyB;
        e->m_shapeKeyPair.m_shapeKeyA = m_keyA;
        e->m_size = 0x10;
    }
    void writeKeys(hkpAgent1nMachineEntry* e) const
    {
        e->m_shapeKeyPair.m_shapeKeyA = m_keyA;
        e->m_shapeKeyPair.m_shapeKeyB = m_keyB;
    }
    void transform(hkpAgent3ProcessInput& in, hkPadSpu<unsigned char>& flag)
    {
        in.m_bodyB = hkAgentMachine_processTransformedShapes(in.m_bodyB, m_transformB.m_bodies, m_transformB.m_motions, 4, flag);
        in.m_bodyA = hkAgentMachine_processTransformedShapes(in.m_bodyA, m_transformA.m_bodies, m_transformA.m_motions, 4, flag);
    }
    static void timTemp(hkpAgentNmMachineBodyTemp& temp, const hkpAgent3ProcessInput& in)
    {
        temp.m_bodyB.setShape(in.m_bodyB.val()->getShape(), in.m_bodyB.val()->getShapeKey());
        temp.m_bodyA.setShape(in.m_bodyA.val()->getShape(), in.m_bodyA.val()->getShapeKey());
    }
    static void timTempFlipped(hkpAgentNmMachineBodyTemp& temp, const hkpAgent3ProcessInput& in)
    {
        temp.m_bodyA.setShape(in.m_bodyB.val()->getShape(), in.m_bodyB.val()->getShapeKey());
        temp.m_bodyB.setShape(in.m_bodyA.val()->getShape(), in.m_bodyA.val()->getShapeKey());
    }
};

// The per-step state of the stream walk.
template<class Machine>
struct Processor
{
    Machine& m;
    const hkpAgent3ProcessInput& inputIn;
    hkpProcessCollisionOutput& output;
    hkpAgent3ProcessInput in;
    hkpCollisionDispatcher* dispatcher;
    bool timInputValid = false;
    bool flippedTimInputValid = false;
    int timRejected = 0; // the "MinumTim" monitor value
    hkpAgentNmMachineBodyTemp timTemp;
    hkpAgentNmMachineBodyTemp flippedTimTemp;
    hkpAgent3Input timInput;
    hkpAgent3Input flippedTimInput;
    hkpAgent3ProcessInput flipped;

    Processor(Machine& machine, const hkpAgent3ProcessInput& input, hkpProcessCollisionOutput& out)
        : m(machine), inputIn(input), output(out), in(input)
    {
        dispatcher = in.m_input.val()->m_dispatcher;
        timInput.m_overrideBodyA = nullptr;
        flippedTimInput.m_overrideBodyA = nullptr;
    }

    const hkpCollisionDispatcher::Agent3FuncsIntern& funcs(const hkpAgentEntry* entry) const { return dispatcher->m_agent3Func[entry->m_agentType]; }

    // A tim entry that stays apart for the step: its distance kept in the normal's w, its points cleaned up.
    char* rejectTim(hkpAgent1nMachineTimEntry* e, const Q dist, hkpContactMgr* mgr)
    {
        e->m_separatingNormal.m_quad = setXYZ_W(e->m_separatingNormal.m_quad, dist);
        timRejected++;
        if (e->m_numContactPoints == 0)
        {
            return reinterpret_cast<char*>(e) + e->m_size;
        }
        char* end = static_cast<char*>(funcs(e).m_cleanupFunc(e, reinterpret_cast<char*>(e) + 0x20, mgr, *output.m_constraintOwner.val()));
        e->m_size = (unsigned char)(end - reinterpret_cast<char*>(e));
        return end;
    }

    // Processes the entry at the output position; returns where the next entry goes.
    char* processEntry(hkpAgent1nMachineEntry* entry)
    {
        unsigned int command = entry->m_streamCommand;
        bool transformed = false;
        for (;;)
        {
            if (command > hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM)
            {
                return reinterpret_cast<char*>(entry);
            }
            switch (command)
            {
            case hkAgent3::STREAM_NULL:
            case hkAgent3::STREAM_END:
                return reinterpret_cast<char*>(entry) + 0x10;
            case hkAgent3::STREAM_CALL:
            {
                m.fetch();
                char* end = static_cast<char*>(funcs(entry).m_processFunc(in, entry, reinterpret_cast<char*>(entry) + 0x10, nullptr, output));
                entry->m_size = (unsigned char)(end - reinterpret_cast<char*>(entry));
                return end;
            }
            case hkAgent3::STREAM_CALL_FLIPPED:
            {
                flipped.m_overrideBodyA = nullptr;
                hkAgent1nMachine_flipInput(in, flipped);
                m.fetch();
                const float toi = output.m_toi.m_time.val();
                hkpProcessCdPoint* first = output.m_firstFreeContactPoint;
                char* end = static_cast<char*>(funcs(entry).m_processFunc(flipped, entry, reinterpret_cast<char*>(entry) + 0x10, nullptr, output));
                entry->m_size = (unsigned char)(end - reinterpret_cast<char*>(entry));
                hk1nFlipPoints(first, output);
                if (!(toi == output.m_toi.m_time.val()))
                {
                    output.m_toi.flip();
                }
                return end;
            }
            case hkAgent3::STREAM_CALL_WITH_TIM:
            {
                hkpAgent1nMachineTimEntry* e = static_cast<hkpAgent1nMachineTimEntry*>(entry);
                const hkpProcessCollisionInput& input = *in.m_input.val();
                const float t = e->m_timeOfSeparatingNormal;
                bool check = t == input.m_stepInfo.m_startTime.val() || t >= input.m_stepInfo.m_endTime.val();
                Q dist;
                if (!check)
                {
                    const hkpCollisionQualityInfo* quality = input.m_collisionQualityInfo;
                    if (quality->m_useContinuousPhysics.val())
                    {
                        if (!timInputValid)
                        {
                            timInputValid = !transformed;
                            hkAgent1nMachine_initInputAtTime(in, timTemp, timInput);
                        }
                        m.fetch();
                        Machine::timTemp(timTemp, in);
                        funcs(entry).m_sepNormalFunc(timInput, entry, reinterpret_cast<char*>(entry) + 0x20, e->m_separatingNormal);
                        check = true;
                    }
                    else
                    {
                        // no separating normal: 0.1 (0x3DCCCCCD) of the minimum separation
                        e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
                        dist = _mm_mul_ps(_mm_set1_ps(0.1f), _mm_set1_ps(quality->m_minSeparation));
                        e->m_separatingNormal.m_quad = setXYZ_W(hkVector4f::getConstant<HK_QUADREAL_0>().m_quad, dist);
                    }
                }
                if (check)
                {
                    e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
                    dist = hk1nTimDistance(e->m_separatingNormal.m_quad, in.m_linearTimInfo.m_quad);
                    if (!lessOrNaN(dist, _mm_set1_ps(input.m_tolerance.val())))
                    {
                        return rejectTim(e, dist, in.m_contactMgr);
                    }
                }
                in.m_distAtT1 = lane0(dist);
                m.fetch();
                char* end = static_cast<char*>(funcs(entry).m_processFunc(in, entry, reinterpret_cast<char*>(entry) + 0x20, &e->m_separatingNormal, output));
                entry->m_size = (unsigned char)(end - reinterpret_cast<char*>(entry));
                return end;
            }
            case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED:
            {
                hkpAgent1nMachineTimEntry* e = static_cast<hkpAgent1nMachineTimEntry*>(entry);
                flipped.m_overrideBodyA = nullptr;
                hkAgent1nMachine_flipInput(in, flipped);
                const hkpProcessCollisionInput& input = *flipped.m_input.val();
                // Original behaviour kept: unlike the unflipped entry, a tim from a later time is not accepted.
                bool check = e->m_timeOfSeparatingNormal == input.m_stepInfo.m_startTime.val();
                Q dist;
                if (!check)
                {
                    const hkpCollisionQualityInfo* quality = input.m_collisionQualityInfo;
                    if (quality->m_useContinuousPhysics.val())
                    {
                        if (!flippedTimInputValid)
                        {
                            flippedTimInputValid = !transformed;
                            hkAgent1nMachine_initInputAtTime(flipped, flippedTimTemp, flippedTimInput);
                            flippedTimTemp.m_bodyB.setShape(flipped.m_bodyB.val()->getShape(), flipped.m_bodyB.val()->getShapeKey());
                        }
                        m.fetch();
                        Machine::timTempFlipped(flippedTimTemp, in);
                        funcs(entry).m_sepNormalFunc(flippedTimInput, entry, reinterpret_cast<char*>(entry) + 0x20, e->m_separatingNormal);
                        check = true;
                    }
                    else
                    {
                        // Original behaviour kept: half (HK_QUADREAL_INV_2) of the minimum separation here, not 0.1.
                        e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
                        dist = _mm_mul_ps(_mm_set1_ps(quality->m_minSeparation), hkVector4f::getConstant<HK_QUADREAL_INV_2>().m_quad);
                        e->m_separatingNormal.m_quad = setXYZ_W(hkVector4f::getConstant<HK_QUADREAL_0>().m_quad, dist);
                    }
                }
                if (check)
                {
                    e->m_timeOfSeparatingNormal = input.m_stepInfo.m_endTime;
                    dist = hk1nTimDistance(e->m_separatingNormal.m_quad, flipped.m_linearTimInfo.m_quad);
                    if (!lessOrNaN(dist, _mm_set1_ps(input.m_tolerance.val())))
                    {
                        return rejectTim(e, dist, flipped.m_contactMgr);
                    }
                }
                flipped.m_distAtT1 = lane0(dist);
                m.fetch();
                hkpProcessCdPoint* first = output.m_firstFreeContactPoint;
                const float toi = output.m_toi.m_time.val();
                char* end = static_cast<char*>(funcs(entry).m_processFunc(flipped, entry, reinterpret_cast<char*>(entry) + 0x20, &e->m_separatingNormal, output));
                entry->m_size = (unsigned char)(end - reinterpret_cast<char*>(entry));
                hk1nFlipPoints(first, output);
                if (!(toi == output.m_toi.m_time.val()))
                {
                    output.m_toi.flip();
                }
                return end;
            }
            case hkAgent3::STREAM_CALL_AGENT:
            {
                m.fetch();
                hkpCollisionAgent* agent = *reinterpret_cast<hkpCollisionAgent**>(reinterpret_cast<char*>(entry) + 0x10);
                agent->processCollision(*in.m_bodyA.val(), *in.m_bodyB.val(), *in.m_input.val(), output);
                return reinterpret_cast<char*>(entry) + 0x20;
            }
            case 7:
            case hkAgent3::TRANSFORM_FLAG | hkAgent3::STREAM_NULL:
            case hkAgent3::TRANSFORM_FLAG | hkAgent3::STREAM_END:
                return reinterpret_cast<char*>(entry); // dropped
            default:
            {
                // a transform shape: its children in moved motion states first
                m.fetch();
                hkPadSpu<unsigned char> flag;
                flag = 0;
                m.transform(in, flag);
                in.m_aTb.setMulInverseMul(in.m_bodyA.val()->getTransform(), in.m_bodyB.val()->getTransform());
                command &= ~hkAgent3::TRANSFORM_FLAG;
                transformed = true;
                flippedTimInputValid = false;
                timInputValid = false;
                continue;
            }
            }
        }
    }

    // An agent for a new key at the output position.
    void create(hkpAgent1nMachineEntry* entry)
    {
        m.fetch();
        hkPadSpu<unsigned char> flag;
        flag = 0;
        m.transform(in, flag);
        in.m_aTb.setMulInverseMul(in.m_bodyA.val()->getTransform(), in.m_bodyB.val()->getTransform());
        m.writeKeys(entry);
        const int typeA = in.m_bodyA.val()->getShape()->m_type.m_storage;
        const int typeB = in.m_bodyB.val()->getShape()->m_type.m_storage;
        const unsigned char agentType = in.m_input.val()->m_createPredictiveAgents.val() == 0 ? dispatcher->m_agent3Types[typeA][typeB] : dispatcher->m_agent3TypesPred[typeA][typeB];
        entry->m_agentType = agentType;
        entry->m_numContactPoints = 0;
        const bool isFlipped = dispatcher->m_agent3Func[agentType].m_symmetric == hkAgent3::IS_NOT_SYMMETRIC_AND_FLIPPED;
        const hkpAgent3Input* createInput = &in;
        if (isFlipped)
        {
            flipped.m_overrideBodyA = nullptr;
            hkAgent1nMachine_flipInput(in, flipped);
            createInput = &flipped;
        }
        const hkpCollisionDispatcher::Agent3FuncsIntern& f = dispatcher->m_agent3Func[entry->m_agentType];
        char* end;
        if (f.m_sepNormalFunc == nullptr)
        {
            entry->m_streamCommand = (unsigned char)((isFlipped + hkAgent3::STREAM_CALL) | flag.val());
            end = static_cast<char*>(f.m_createFunc(*createInput, entry, reinterpret_cast<char*>(entry) + 0x10));
        }
        else
        {
            hkpAgent1nMachineTimEntry* e = static_cast<hkpAgent1nMachineTimEntry*>(static_cast<hkpAgent1nMachineEntry*>(entry));
            e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
            entry->m_streamCommand = (unsigned char)((isFlipped + hkAgent3::STREAM_CALL_WITH_TIM) | flag.val());
            e->m_separatingNormal.setZero();
            end = static_cast<char*>(f.m_createFunc(*createInput, entry, reinterpret_cast<char*>(entry) + 0x20));
        }
        entry->m_size = (unsigned char)(end - reinterpret_cast<char*>(entry));
    }
};

template<class Machine>
void hk1nMachine_process(hkpAgent1nTrack& agentTrack, const hkpAgent3ProcessInput& inputIn, Machine& machine, const hkpShapeContainer* weldingContainer, hkpProcessCollisionOutput& output)
{
    PotentialInfo* callersPotentials = output.m_potentialContacts;
    PotentialInfo localPotentials;
    if (Machine::WELDING)
    {
        if (callersPotentials == nullptr && weldingContainer->isWeldingEnabled())
        {
            localPotentials.reset();
            output.m_potentialContacts = &localPotentials;
        }
        else
        {
            output.m_potentialContacts = nullptr;
        }
    }

    Processor<Machine> p(machine, inputIn, output);
    machine.init(inputIn, p.in);
    const hkpProcessCollisionInput& input = *p.in.m_input.val();

    hkpAgent1nSector* readSector = agentTrack.m_sectors[0];
    hkpAgent1nSector* writeSector = hk1nTakeSector(input);
    char* read = static_cast<char*>(readSector->getBegin());
    char* readEnd = static_cast<char*>(readSector->getEnd());
    char* write = static_cast<char*>(writeSector->getBegin());
    char* writeLimit = reinterpret_cast<char*>(writeSector + 1);
    int nextReadIndex = 1;
    int writeIndex = 0;
    bool inExtension = false;
    int bytesUsed = 0;
    alignas(16) char extension[kMaxEntrySize];

    for (;;)
    {
        do
        {
            p.in.m_aTb = inputIn.m_aTb;
            machine.resetBodies(p.in);
            hkpAgent1nMachineEntry* entry = reinterpret_cast<hkpAgent1nMachineEntry*>(write);
            hkpAgent1nMachineEntry* readEntry = reinterpret_cast<hkpAgent1nMachineEntry*>(read);
            bool process = true;
            if (!machine.hasNewKeys())
            {
                const int size = readEntry->m_size;
                hk1nCopy(write, read, size);
                read += size;
            }
            else
            {
                const int order = machine.compare(readEntry);
                if (order == 0)
                {
                    const int size = readEntry->m_size;
                    hk1nCopy(write, read, size);
                    read += size;
                    machine.advance();
                }
                else if (order < 0)
                {
                    // a new key
                    if (!machine.isEnabled(input, inputIn))
                    {
                        machine.writeNull(entry);
                    }
                    else if (hkOutOfMemoryState == hkMemoryAllocator::MEMORY_STATE_OUT_OF_MEMORY)
                    {
                        process = false;
                    }
                    else
                    {
                        p.create(entry);
                    }
                    machine.advance();
                }
                else
                {
                    // the key is gone
                    hkpAgentData* data = read + ((readEntry->m_streamCommand & 6) == hkAgent3::STREAM_CALL_WITH_TIM ? 0x20 : 0x10);
                    const unsigned char agentType = readEntry->m_agentType;
                    read += readEntry->m_size;
                    p.dispatcher->m_agent3Func[agentType].m_destroyFunc(readEntry, data, p.in.m_contactMgr, *output.m_constraintOwner.val(), p.dispatcher);
                    process = false;
                }
            }
            if (process)
            {
                write = p.processEntry(entry);
            }

            if (read == readEnd)
            {
                // the sector read: the input's spare sector, or freed
                if (input.m_spareAgentSector == nullptr)
                {
                    input.m_spareAgentSector = readSector;
                }
                else
                {
                    hkMemoryRouter::getInstance().heap().blockFree(readSector, hkpAgent1nSector::SECTOR_SIZE);
                }
                if (agentTrack.m_sectors.getSize() <= nextReadIndex)
                {
                    // done: the last write sector (with what is in the overflow buffer) closes the track
                    int total;
                    if (inExtension)
                    {
                        const int extensionBytes = (int)(write - extension);
                        hk1nCopy(writeSector->m_data + bytesUsed, extension, extensionBytes);
                        if (Machine::WELDING)
                        {
                            // Original behaviour kept: only the last potential contact is checked here.
                            hk1nRelocatePotentials(output.m_potentialContacts, extension, extensionBytes, reinterpret_cast<char*>(writeSector->m_data) + bytesUsed, true);
                        }
                        total = bytesUsed + extensionBytes;
                    }
                    else
                    {
                        total = (int)(write - static_cast<char*>(writeSector->getBegin()));
                    }
                    writeSector->m_bytesAllocated = total;
                    agentTrack.m_sectors.setSize(writeIndex + 1);
                    agentTrack.m_sectors[writeIndex] = writeSector;
                    if (Machine::WELDING)
                    {
                        p.in.m_aTb = inputIn.m_aTb;
                        p.in.m_bodyB = inputIn.m_bodyB;
                        PotentialInfo* potentials = output.m_potentialContacts;
                        if (potentials != nullptr && potentials->m_firstFreePotentialContact > &potentials->m_potentialContacts[0])
                        {
                            hkAgent1nMachine_Weld(p.in, weldingContainer, output);
                        }
                        output.m_potentialContacts = callersPotentials;
                    }
                    return;
                }
                readSector = agentTrack.m_sectors[nextReadIndex++];
                read = static_cast<char*>(readSector->getBegin());
                readEnd = static_cast<char*>(readSector->getEnd());
            }
        } while (write + kMaxEntrySize <= writeLimit);

        if (!inExtension)
        {
            bytesUsed = (int)(write - static_cast<char*>(writeSector->getBegin()));
            inExtension = true;
            write = extension;
            writeLimit = extension + kMaxEntrySize;
            continue;
        }
        const int extensionBytes = (int)(write - extension);
        if (extensionBytes <= kNetSectorSize - bytesUsed)
        {
            hk1nCopy(writeSector->m_data + bytesUsed, extension, extensionBytes);
            if (Machine::WELDING)
            {
                hk1nRelocatePotentials(output.m_potentialContacts, extension, extensionBytes, reinterpret_cast<char*>(writeSector->m_data) + bytesUsed, false);
            }
            bytesUsed += extensionBytes;
            if (bytesUsed < kNetSectorSize)
            {
                write = extension;
                continue;
            }
            inExtension = false;
        }
        writeSector->m_bytesAllocated = bytesUsed;
        if (nextReadIndex <= writeIndex)
        {
            // the output caught up with the input: a new slot before the next sector to read
            const int size = agentTrack.m_sectors.getSize();
            agentTrack.m_sectors.reserve(size + 1);
            hkpAgent1nSector** slots = agentTrack.m_sectors.begin() + nextReadIndex;
            memmove(slots + 1, slots, (size - nextReadIndex) * sizeof(hkpAgent1nSector*));
            agentTrack.m_sectors.setSizeUnchecked(size + 1);
            nextReadIndex++;
        }
        agentTrack.m_sectors[writeIndex++] = writeSector;
        writeSector = hk1nTakeSector(input);
        write = static_cast<char*>(writeSector->getBegin());
        writeLimit = reinterpret_cast<char*>(writeSector + 1);
        if (inExtension)
        {
            hk1nCopy(write, extension, extensionBytes);
            if (Machine::WELDING)
            {
                hk1nRelocatePotentials(output.m_potentialContacts, extension, extensionBytes, write, false);
            }
            write += extensionBytes;
            inExtension = false;
        }
    }
}
}

// 0x00DCD0D0
void hkAgent1nMachine_Process(hkpAgent1nTrack& agentTrack, const hkpAgent3ProcessInput& inputIn, const hkpShapeContainer* shapeContainerB, const unsigned int* newKeys, hkpProcessCollisionOutput& output)
{
    Machine1n machine;
    machine.m_containerB = shapeContainerB;
    machine.m_newKeys = newKeys;
    hk1nMachine_process(agentTrack, inputIn, machine, shapeContainerB, output);
}

// 0x00DCE110
void hkAgentNmMachine_Process(hkpAgent1nTrack& agentTrack, const hkpAgent3ProcessInput& inputIn, const hkpShapeContainer* shapeContainerA, const hkpShapeContainer* shapeContainerB, const hkpShapeKeyPair* newKeys, hkpProcessCollisionOutput& output)
{
    MachineNm machine;
    machine.m_containerA = shapeContainerA;
    machine.m_containerB = shapeContainerB;
    machine.m_newKeys = newKeys;
    hk1nMachine_process(agentTrack, inputIn, machine, nullptr, output);
}

// 0x00DCCDD0: an entry whose child the filter now disables is destroyed (the stream continues at the entry); the
// others get the agent's filter update on the (transformed) child
void* hkAgent1nMachine_UpdateShapeCollectionFilterVisitor(hkpAgent1nMachine_VisitorInput& vin, hkpAgent1nMachineEntry* entry, void* agentData)
{
    const unsigned int key = entry->m_shapeKeyPair.m_shapeKeyB;
    const hkpShapeCollectionFilter* filter = vin.m_input->m_filter.val();
    hkpCollisionDispatcher* dispatcher = vin.m_input->m_dispatcher;
    if (filter->isCollisionEnabled(*vin.m_input, *vin.m_bodyA, *vin.m_collectionBodyB, *vin.m_containerShapeB, key).m_bool)
    {
        hkAgent3::UpdateFilterFunc f = dispatcher->m_agent3Func[entry->m_agentType].m_updateFilterFunc;
        if (f)
        {
            hkpShapeBuffer buffer;
            hkpCdBody child(vin.m_collectionBodyB);
            child.setShape(vin.m_containerShapeB->getChildShape(key, buffer), key);
            const hkpCdBody* body = &child;
            TransformBuffers transforms;
            if (entry->m_streamCommand & hkAgent3::TRANSFORM_FLAG)
            {
                hkPadSpu<unsigned char> flag;
                flag = 0;
                body = hkAgentMachine_processTransformedShapes(&child, transforms.m_bodies, transforms.m_motions, 4, flag);
            }
            const int command = entry->m_streamCommand & 7;
            if (command == hkAgent3::STREAM_CALL_FLIPPED || command == hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED)
            {
                f(entry, agentData, *body, *vin.m_bodyA, *vin.m_input, vin.m_contactMgr, *vin.m_constraintOwner);
            }
            else
            {
                f(entry, agentData, *vin.m_bodyA, *body, *vin.m_input, vin.m_contactMgr, *vin.m_constraintOwner);
            }
        }
        return reinterpret_cast<char*>(entry) + entry->m_size;
    }
    dispatcher->m_agent3Func[entry->m_agentType].m_destroyFunc(entry, agentData, vin.m_contactMgr, *vin.m_constraintOwner, dispatcher);
    return entry;
}

// 0x00DCCF10: the same for an Nm entry (both children)
void* hkAgentNmMachine_UpdateShapeCollectionFilterVisitor(hkpAgent1nMachine_VisitorInput& vin, hkpAgent1nMachineEntry* entry, void* agentData)
{
    const unsigned int keyB = entry->m_shapeKeyPair.m_shapeKeyB;
    const unsigned int keyA = entry->m_shapeKeyPair.m_shapeKeyA;
    const hkpShapeCollectionFilter* filter = vin.m_input->m_filter.val();
    if (!filter->isCollisionEnabled(*vin.m_input, *vin.m_bodyA, *vin.m_collectionBodyB, *vin.m_containerShapeA, *vin.m_containerShapeB, keyA, keyB).m_bool)
    {
        hkpCollisionDispatcher* dispatcher = vin.m_input->m_dispatcher;
        dispatcher->m_agent3Func[entry->m_agentType].m_destroyFunc(entry, agentData, vin.m_contactMgr, *vin.m_constraintOwner, dispatcher);
        return entry;
    }
    hkAgent3::UpdateFilterFunc f = vin.m_input->m_dispatcher.val()->m_agent3Func[entry->m_agentType].m_updateFilterFunc;
    if (f)
    {
        hkpShapeBuffer bufferB;
        hkpShapeBuffer bufferA;
        const hkpShape* shapeB = vin.m_containerShapeB->getChildShape(keyB, bufferB);
        const hkpShape* shapeA = vin.m_containerShapeA->getChildShape(keyA, bufferA);
        hkpCdBody childB(vin.m_collectionBodyB);
        childB.setShape(shapeB, keyB);
        hkpCdBody childA(vin.m_bodyA);
        childA.setShape(shapeA, keyA);
        const hkpCdBody* bodyB = &childB;
        const hkpCdBody* bodyA = &childA;
        TransformBuffers transformsB;
        TransformBuffers transformsA;
        if (entry->m_streamCommand & hkAgent3::TRANSFORM_FLAG)
        {
            hkPadSpu<unsigned char> flag;
            flag = 0;
            bodyB = hkAgentMachine_processTransformedShapes(&childB, transformsB.m_bodies, transformsB.m_motions, 4, flag);
            bodyA = hkAgentMachine_processTransformedShapes(&childA, transformsA.m_bodies, transformsA.m_motions, 4, flag);
        }
        const int command = entry->m_streamCommand & 7;
        if (command != hkAgent3::STREAM_CALL_FLIPPED && command != hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED)
        {
            f(entry, agentData, *bodyA, *bodyB, *vin.m_input, vin.m_contactMgr, *vin.m_constraintOwner);
            return reinterpret_cast<char*>(entry) + entry->m_size;
        }
        f(entry, agentData, *bodyB, *bodyA, *vin.m_input, vin.m_contactMgr, *vin.m_constraintOwner);
    }
    return reinterpret_cast<char*>(entry) + entry->m_size;
}
