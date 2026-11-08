// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the 1n agent machine, from rfg.exe: a stream of agent3 entries (one per child
// key of a shape collection) in 512-byte sectors, ended by a STREAM_END entry. This file has the track's lifetime,
// tim invalidation and warping, the input helpers of the process functions, the visit of all agents (used by the
// filter updates, which may remove entries: the stream is rewritten into compacted sectors) and the welding of the
// potential contacts. The process functions are in hkpagent1nmachineprocess.cxx. No SDK source exists for
// 2014.2.5; ported from the binary.
//
// Entry layouts (hkpagent1nmachine.h): a padded entry keeps the agent data at +0x10; a tim entry (the agent has a
// separating normal function) keeps its separating normal time at +0x0C, the normal at +0x10 and the agent data at
// +0x20. A bridge entry (STREAM_CALL_AGENT) keeps its agent2 pointer at +0x10.
//
// The shipped build records the "TtConflicts"/"Et" timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.h"

#include <string.h>

using namespace hkGskExact;

namespace
{
inline hkpAgentEntry* hk1nEntryAt(void* p)
{
    return static_cast<hkpAgentEntry*>(p);
}

inline hkpAgentEntry* hk1nNext(hkpAgentEntry* entry)
{
    return hkAddByteOffset(entry, entry->m_size);
}

// The agent data of an entry: after the tim block for tim entries, after the padded header otherwise.
inline hkpAgentData* hk1nData(hkpAgentEntry* entry, int offset)
{
    return hkAddByteOffset(entry, offset);
}

inline hkpAgent1nSector* hk1nNewSector()
{
    hkpAgent1nSector* sector = static_cast<hkpAgent1nSector*>(hkMemoryRouter::getInstance().heap().blockAlloc(hkpAgent1nSector::SECTOR_SIZE));
    sector->m_bytesAllocated = 0;
    return sector;
}

inline void hk1nFreeSector(hkpAgent1nSector* sector)
{
    hkMemoryRouter::getInstance().heap().blockFree(sector, hkpAgent1nSector::SECTOR_SIZE);
}

// Copies an entry 16 bytes at a time (at least once).
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
}

// 0x00DCB7B0
void hkAgent1nMachine_InvalidateTim(hkpAgent1nTrack& agentTrack, const hkpCollisionInput& input)
{
    hkpAgent1nSector* sector = agentTrack.m_sectors[0];
    hkpAgentEntry* entry = hk1nEntryAt(sector->getBegin());
    hkpAgentEntry* end = hk1nEntryAt(sector->getEnd());
    int nextSector = 1;
    for (;;)
    {
        hkpAgentEntry* next = entry;
        switch (entry->m_streamCommand)
        {
        case hkAgent3::STREAM_NULL:
            next = hk1nNext(entry);
            break;
        case hkAgent3::STREAM_END:
            return;
        case hkAgent3::STREAM_CALL:
        case hkAgent3::STREAM_CALL_FLIPPED:
        case hkAgent3::STREAM_CALL_AGENT:
        case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_FLIPPED_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
        {
            next = hk1nNext(entry);
            hkAgent3::InvalidateTimFunc f = input.m_dispatcher.val()->m_agent3Func[entry->m_agentType].m_invalidateTimFunc;
            if (f)
            {
                f(entry, hk1nData(entry, 0x10), input);
            }
            break;
        }
        case hkAgent3::STREAM_CALL_WITH_TIM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED:
        case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED_WITH_TRANSFORM:
        {
            hkpAgent1nMachineTimEntry* e = static_cast<hkpAgent1nMachineTimEntry*>(entry);
            e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
            e->m_separatingNormal.setZero();
            next = hk1nNext(entry);
            hkAgent3::InvalidateTimFunc f = input.m_dispatcher.val()->m_agent3Func[entry->m_agentType].m_invalidateTimFunc;
            if (f)
            {
                f(entry, hk1nData(entry, 0x20), input);
            }
            break;
        }
        default:
            break;
        }
        entry = next;
        if (entry >= end)
        {
            sector = agentTrack.m_sectors[nextSector++];
            entry = hk1nEntryAt(sector->getBegin());
            end = hk1nEntryAt(sector->getEnd());
        }
    }
}

// 0x00DCB890: a tim taken at oldTime moves to newTime; any other is invalidated (its normal kept)
void hkAgent1nMachine_WarpTime(hkpAgent1nTrack& agentTrack, float oldTime, float newTime, const hkpCollisionInput& input)
{
    hkpAgent1nSector* sector = agentTrack.m_sectors[0];
    hkpAgentEntry* entry = hk1nEntryAt(sector->getBegin());
    hkpAgentEntry* end = hk1nEntryAt(sector->getEnd());
    int nextSector = 1;
    for (;;)
    {
        hkpAgentEntry* next = entry;
        int dataOffset = 0;
        switch (entry->m_streamCommand)
        {
        case hkAgent3::STREAM_NULL:
            next = hk1nNext(entry);
            break;
        case hkAgent3::STREAM_END:
            return;
        case hkAgent3::STREAM_CALL:
        case hkAgent3::STREAM_CALL_FLIPPED:
        case hkAgent3::STREAM_CALL_AGENT:
        case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_FLIPPED_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
            dataOffset = 0x10;
            break;
        case hkAgent3::STREAM_CALL_WITH_TIM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED:
        case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED_WITH_TRANSFORM:
        {
            hkpAgent1nMachineTimEntry* e = static_cast<hkpAgent1nMachineTimEntry*>(entry);
            dataOffset = 0x20;
            if (e->m_timeOfSeparatingNormal == oldTime)
            {
                e->m_timeOfSeparatingNormal = newTime;
            }
            else
            {
                e->m_timeOfSeparatingNormal = -1.0f; // 0xBF800000
            }
            break;
        }
        default:
            break;
        }
        if (dataOffset)
        {
            next = hk1nNext(entry);
            hkAgent3::WarpTimeFunc f = input.m_dispatcher.val()->m_agent3Func[entry->m_agentType].m_warpTimeFunc;
            if (f)
            {
                f(entry, hk1nData(entry, dataOffset), oldTime, newTime, input);
            }
        }
        entry = next;
        if (entry >= end)
        {
            sector = agentTrack.m_sectors[nextSector++];
            entry = hk1nEntryAt(sector->getBegin());
            end = hk1nEntryAt(sector->getEnd());
        }
    }
}

// 0x00DCBA30: the input at the start of the step (both bodies' swept transforms at the start time; out's bodies are
// temp's, temp's B shape is left to the caller)
void hkAgent1nMachine_initInputAtTime(hkpAgent3Input& in, hkpAgentNmMachineBodyTemp& temp, hkpAgent3Input& out)
{
    const hkMotionState* msA = static_cast<const hkMotionState*>(in.m_bodyA.val()->getMotionState());
    const hkMotionState* msB = static_cast<const hkMotionState*>(in.m_bodyB.val()->getMotionState());
    hkSweptTransformUtil::lerp2(msA->getSweptTransform(), in.m_input.val()->m_stepInfo.m_startTime.val(), temp.m_transA);
    hkSweptTransformUtil::lerp2(msB->getSweptTransform(), in.m_input.val()->m_stepInfo.m_startTime.val(), temp.m_transB);
    out.m_bodyA = &temp.m_bodyA;
    out.m_bodyB = &temp.m_bodyB;
    out.m_contactMgr = in.m_contactMgr;
    out.m_input = in.m_input;
    temp.m_bodyA.setShape(in.m_bodyA.val()->getShape(), in.m_bodyA.val()->getShapeKey());
    temp.m_bodyA.m_parent = in.m_bodyA;
    temp.m_bodyA.m_motion = &temp.m_transA;
    temp.m_bodyB.m_parent = in.m_bodyB;
    temp.m_bodyB.m_motion = &temp.m_transB;
    out.m_aTb.setMulInverseMul(temp.m_transA, temp.m_transB);
}

// 0x00DCBAE0: A and B swapped (the tim info negated, aTb inverted; the tim distance and the override body are the
// caller's)
void hkAgent1nMachine_flipInput(hkpAgent3ProcessInput& in, hkpAgent3ProcessInput& out)
{
    out.m_bodyA = in.m_bodyB;
    out.m_bodyB = in.m_bodyA;
    out.m_input = in.m_input;
    out.m_contactMgr = in.m_contactMgr;
    out.m_linearTimInfo.m_quad = _mm_xor_ps(in.m_linearTimInfo.m_quad, _mm_castsi128_ps(_mm_set_epi32(0, (int)0x80000000, (int)0x80000000, (int)0x80000000)));
    out.m_aTb.setInverse(in.m_aTb);
}

// 0x00DCBCA0: every agent destroyed, the sectors freed, the sector array deallocated
void hkAgent1nMachine_Destroy(hkpAgent1nTrack& agentTrack, hkpCollisionDispatcher* dispatcher, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkpAgent1nSector** sectors = agentTrack.m_sectors.begin();
    hkpAgentEntry* entry = hk1nEntryAt(sectors[0]->getBegin());
    hkpAgentEntry* end = hk1nEntryAt(sectors[0]->getEnd());
    int sectorIndex = 0;
    for (;;)
    {
        int dataOffset = 0;
        switch (entry->m_streamCommand)
        {
        case hkAgent3::STREAM_NULL:
            entry = hk1nNext(entry);
            break;
        case hkAgent3::STREAM_END:
            if (sectors[sectorIndex])
            {
                hk1nFreeSector(sectors[sectorIndex]);
            }
            agentTrack.m_sectors.clearAndDeallocate();
            return;
        case hkAgent3::STREAM_CALL:
        case hkAgent3::STREAM_CALL_FLIPPED:
        case hkAgent3::STREAM_CALL_AGENT:
        case hkAgent3::STREAM_CALL_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_FLIPPED_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_AGENT_WITH_TRANSFORM:
            dataOffset = 0x10;
            break;
        case hkAgent3::STREAM_CALL_WITH_TIM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED:
        case hkAgent3::STREAM_CALL_WITH_TIM_WITH_TRANSFORM:
        case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED_WITH_TRANSFORM:
            dataOffset = 0x20;
            break;
        default:
            break;
        }
        if (dataOffset)
        {
            const int size = entry->m_size;
            dispatcher->m_agent3Func[entry->m_agentType].m_destroyFunc(entry, hk1nData(entry, dataOffset), mgr, constraintOwner, dispatcher);
            entry = hkAddByteOffset(entry, size);
        }
        if (entry >= end)
        {
            hkpAgent1nSector* sector = sectors[sectorIndex++];
            if (sector)
            {
                hk1nFreeSector(sector);
            }
            sector = agentTrack.m_sectors[sectorIndex];
            entry = hk1nEntryAt(sector->getBegin());
            end = hk1nEntryAt(sector->getEnd());
        }
    }
}

// 0x00DCBDF0: one sector holding the end entry (both keys invalid)
void hkAgent1nMachine_Create(hkpAgent1nTrack& agentTrack)
{
    agentTrack.m_sectors.clear();
    agentTrack.m_sectors.reserve(4);
    hkpAgent1nSector*& slot = agentTrack.m_sectors.expandOne();
    hkpAgent1nSector* sector = hk1nNewSector();
    slot = sector;
    hkpAgent1nMachineEntry* end = static_cast<hkpAgent1nMachineEntry*>(hk1nEntryAt(sector->getBegin()));
    // Original behaviour kept: the agent type and contact point count bytes of the end entry are not written.
    end->m_size = sizeof(hkpAgent1nMachinePaddedEntry);
    end->m_streamCommand = hkAgent3::STREAM_END;
    end->m_shapeKeyPair.m_shapeKeyB = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    end->m_shapeKeyPair.m_shapeKeyA = 0xFFFFFFFF;
    sector->m_bytesAllocated = sizeof(hkpAgent1nMachinePaddedEntry);
}

// 0x00DCC610: every agent entry visited and the stream rewritten from the visitor's output position (the visitor
// returns where the next entry goes: after the entry to keep it, at the entry to drop it). The output goes to new
// sectors (a freed read sector is reused one step later), an entry that does not fit the rest of a sector first to a
// 128-byte overflow buffer.
void hkAgent1nMachine_VisitAllAgents(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin, hkAgent1nMachine_VisitorCallback visitor)
{
    hkpAgent1nSector* freeSector = agentTrack.m_sectors[0]; // the sector being read, freed (or reused) when done
    hkpAgent1nSector* readSector = nullptr;
    hkpAgent1nSector* writeSector = hk1nNewSector();
    char* read = static_cast<char*>(freeSector->getBegin());
    char* readEnd = static_cast<char*>(freeSector->getEnd());
    char* writeLimit = reinterpret_cast<char*>(writeSector + 1);
    int nextReadIndex = 1;
    int writeIndex = 0;
    bool inExtension = false;
    int bytesUsed = 0;
    alignas(16) char extension[hkAgent3::MAX_SIZE];
    char* write = static_cast<char*>(writeSector->getBegin());
    for (;;)
    {
        do
        {
            hk1nCopy(write, read, (unsigned char)read[3]);
            read += (unsigned char)read[3];
            hkpAgent1nMachine_VisitorInput visitorInput = vin;
            hkpAgent1nMachineEntry* entry = reinterpret_cast<hkpAgent1nMachineEntry*>(write);
            int dataOffset = 0x10;
            switch (entry->m_streamCommand & ~hkAgent3::TRANSFORM_FLAG)
            {
            case hkAgent3::STREAM_END:
                write += 0x10;
                break;
            case hkAgent3::STREAM_CALL_WITH_TIM:
            case hkAgent3::STREAM_CALL_WITH_TIM_FLIPPED:
                dataOffset = 0x20;
                // fall through
            case hkAgent3::STREAM_CALL:
            case hkAgent3::STREAM_CALL_FLIPPED:
            case hkAgent3::STREAM_CALL_AGENT:
                write = static_cast<char*>(visitor(visitorInput, entry, write + dataOffset));
                break;
            default:
                break; // a null entry is dropped
            }
            if (read == readEnd)
            {
                if (freeSector)
                {
                    hk1nFreeSector(freeSector);
                }
                freeSector = readSector;
                if (agentTrack.m_sectors.getSize() <= nextReadIndex)
                {
                    // done: the last write sector (with what is in the overflow buffer) closes the track
                    if (inExtension)
                    {
                        const int extensionBytes = (int)(write - extension);
                        hk1nCopy(writeSector->m_data + bytesUsed, extension, extensionBytes);
                        bytesUsed += extensionBytes;
                    }
                    else
                    {
                        bytesUsed = (int)(write - static_cast<char*>(writeSector->getBegin()));
                    }
                    writeSector->m_bytesAllocated = bytesUsed;
                    agentTrack.m_sectors.setSize(writeIndex + 1);
                    agentTrack.m_sectors[writeIndex] = writeSector;
                    if (readSector)
                    {
                        hk1nFreeSector(readSector);
                    }
                    return;
                }
                readSector = agentTrack.m_sectors[nextReadIndex++];
                read = static_cast<char*>(readSector->getBegin());
                readEnd = static_cast<char*>(readSector->getEnd());
            }
        } while (write + hkAgent3::MAX_SIZE <= writeLimit);

        if (!inExtension)
        {
            bytesUsed = (int)(write - static_cast<char*>(writeSector->getBegin()));
            writeLimit = extension + hkAgent3::MAX_SIZE;
            inExtension = true;
            write = extension;
            continue;
        }
        const int extensionBytes = (int)(write - extension);
        if (extensionBytes <= hkpAgent1nSector::NET_SECTOR_SIZE - bytesUsed)
        {
            hk1nCopy(writeSector->m_data + bytesUsed, extension, extensionBytes);
            bytesUsed += extensionBytes;
            if (bytesUsed < hkpAgent1nSector::NET_SECTOR_SIZE)
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
            nextReadIndex++;
            agentTrack.m_sectors.setSizeUnchecked(size + 1);
        }
        agentTrack.m_sectors[writeIndex++] = writeSector;
        hkpAgent1nSector* sector;
        if (freeSector == nullptr)
        {
            sector = hk1nNewSector();
        }
        else
        {
            sector = freeSector;
            freeSector = nullptr;
        }
        writeLimit = reinterpret_cast<char*>(sector + 1);
        write = static_cast<char*>(sector->getBegin());
        writeSector = sector;
        if (inExtension)
        {
            hk1nCopy(write, extension, extensionBytes);
            write += extensionBytes;
            inExtension = false;
        }
    }
}

// 0x00DCC9E0
void hkAgent1nMachine_UpdateShapeCollectionFilter(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin)
{
    hkAgent1nMachine_VisitAllAgents(agentTrack, vin, hkAgent1nMachine_UpdateShapeCollectionFilterVisitor);
}

// 0x00DCCA00
void hkAgentNmMachine_UpdateShapeCollectionFilter(hkpAgent1nTrack& agentTrack, hkpAgent1nMachine_VisitorInput& vin)
{
    hkAgent1nMachine_VisitAllAgents(agentTrack, vin, hkAgentNmMachine_UpdateShapeCollectionFilterVisitor);
}

// 0x00DCBEA0: the potential contacts of a step (points of child agents on a welded collection) against the
// representative contacts: a potential point behind a representative's plane (the convex radius of A allowed) is
// dropped, one sharing a welding edge with an unconfirmed representative is a conflict; the rest are added to the
// contact manager. Conflicting pairs (most opposed normals first) are resolved by a common penetration depth; the
// points left are committed to their agents or removed from the output.
void hkAgent1nMachine_Weld(hkpAgent3Input& input, const hkpShapeContainer* shapeCollection, hkpProcessCollisionOutput& output)
{
    hkpProcessCollisionOutput::PotentialInfo& potentials = *output.m_potentialContacts.val();
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(input.m_bodyA.val()->getShape());
    unsigned short ids[256];
    unsigned short conflicts[256];
    int numConflicts = 0;
    hkpShapeBuffer buffer;
    int index = 0;
    for (hkpProcessCollisionOutput::ContactRef* ref = &potentials.m_potentialContacts[0]; ref < potentials.m_firstFreePotentialContact; ref++, index++)
    {
        const hkpProcessCdPoint* cp = ref->m_contactPoint;
        ids[index] = 0xFFFF;
        bool conflict = false;
        bool rejected = false;
        for (hkpProcessCdPoint** reps = &potentials.m_representativeContacts[0]; reps < potentials.m_firstFreeRepresentativeContact; reps++)
        {
            const hkpProcessCdPoint* rep = *reps;
            if (rep == cp)
            {
                continue;
            }
            const hkpCollisionQualityInfo* quality = input.m_input.val()->m_collisionQualityInfo;
            // three dot products in one transposed sum: cp.n . d, d . rep.n, cp.n . rep.n (d = rep.p - cp.p)
            const Q d = sub(rep->m_contact.getPosition().m_quad, cp->m_contact.getPosition().m_quad);
            const Q nCp = cp->m_contact.getSeparatingNormal().m_quad;
            const Q nRep = rep->m_contact.getSeparatingNormal().m_quad;
            const Q a = _mm_mul_ps(nCp, d);
            const Q b = _mm_mul_ps(d, nRep);
            const Q c = _mm_mul_ps(nCp, nRep);
            const Q lo = _mm_unpacklo_ps(a, b);
            const Q hi = _mm_shuffle_ps(_mm_unpackhi_ps(a, b), c, 0xE4);
            const Q t = _mm_movelh_ps(lo, c);
            const Q dots = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(_mm_movehl_ps(t, lo), c, 0xD4), t), hi);
            const Q radius = _mm_set1_ps(shapeA->m_radius);
            const Q cpDotD = _mm_shuffle_ps(dots, dots, 0x00);
            const Q dDotRep = _mm_shuffle_ps(dots, dots, 0x55);
            const Q cosine = _mm_shuffle_ps(dots, dots, 0xAA);
            const Q slack = _mm_mul_ps(_mm_sub_ps(cosine, hkVector4f::getConstant<HK_QUADREAL_1>().m_quad), radius);
            const Q behind = _mm_add_ps(_mm_sub_ps(_mm_setzero_ps(), dDotRep), slack);
            const Q front = _mm_add_ps(cpDotD, slack);
            const Q radius0 = _mm_add_ps(_mm_set1_ps(0.0f), radius);
            if (_mm_cvtss_f32(behind) > _mm_cvtss_f32(radius0))
            {
                continue; // not in the representative's range
            }
            // 0.9999 (0x3F7FF972): the same direction as an unconfirmed representative
            if (_mm_cvtss_f32(cosine) > 0.9999f && rep->m_contactPointId == 0xFFFF)
            {
                continue;
            }
            Q tolerance = _mm_setzero_ps();
            if (rep->m_contactPointId == 0xFFFF)
            {
                // a welding edge: close (0.1, 0x3DCCCCCD), clearly bent (cosine below 0.8, 0x3F4CCCCD) and both points
                // deeper than twice the radius together
                const Q sumDist = _mm_add_ps(_mm_shuffle_ps(nRep, nRep, 0xFF), _mm_shuffle_ps(nCp, nCp, 0xFF));
                const Q twoRadius = _mm_sub_ps(_mm_setzero_ps(), _mm_add_ps(radius0, radius0));
                const Q close = _mm_cmplt_ps(_mm_add_ps(front, behind), _mm_set1_ps(0.1f));
                const Q bent = _mm_cmplt_ps(cosine, _mm_set1_ps(0.8f));
                const Q deep = _mm_cmplt_ps(sumDist, twoRadius);
                if (_mm_movemask_ps(_mm_and_ps(deep, _mm_and_ps(close, bent))) != 0)
                {
                    conflict = true;
                    continue;
                }
            }
            else
            {
                // 0.05 (0x3D4CCCCD) of the keep distance
                tolerance = _mm_mul_ps(_mm_set1_ps(0.05f), _mm_set1_ps(quality->m_keepContact));
            }
            const Q repDist = _mm_sub_ps(_mm_shuffle_ps(nRep, nRep, 0xFF), tolerance);
            const Q cpDist = _mm_shuffle_ps(nCp, nCp, 0xFF);
            if (_mm_cvtss_f32(cpDist) > _mm_cvtss_f32(repDist))
            {
                rejected = true; // farther than a confirmed point in the same direction
                break;
            }
        }
        if (rejected)
        {
            continue;
        }
        if (conflict)
        {
            conflicts[numConflicts++] = (unsigned short)index;
        }
        // the point goes to the contact manager with the child body of B
        const hkpAgent1nMachineEntry* entry = static_cast<const hkpAgent1nMachineEntry*>(ref->m_agentEntry);
        hkpCdBody childB(input.m_bodyB.val());
        childB.setShape(shapeCollection->getChildShape(entry->m_shapeKeyPair.m_shapeKeyB, buffer), entry->m_shapeKeyPair.m_shapeKeyB);
        const unsigned short id = input.m_contactMgr.val()->addContactPointImpl(*input.m_bodyA.val(), childB, *input.m_input.val(), output, nullptr, ref->m_contactPoint->m_contact);
        ids[index] = id;
        if (id != 0xFFFF)
        {
            input.m_contactMgr.val()->reserveContactPointsImpl(-1);
        }
    }

    hkpCollisionDispatcher* dispatcher = input.m_input.val()->m_dispatcher;
    for (int i = 0; i < numConflicts; i++)
    {
        const hkpProcessCollisionOutput::ContactRef& ref = potentials.m_potentialContacts[conflicts[i]];
        dispatcher->m_agent3Func[ref.m_agentEntry->m_agentType].m_createZombieFunc(ref.m_agentEntry, ref.m_agentData, 0xFFFF);
    }

    hkpShapeBuffer bufferB;
    while (numConflicts >= 2)
    {
        // the pair with the most opposed normals (the lowest normal dot, starting at 2)
        __m128i bestI = _mm_setzero_si128();
        __m128i bestJ = _mm_set1_epi32(1);
        Q minDot = hkVector4f::getConstant<HK_QUADREAL_2>().m_quad;
        __m128i iVec = _mm_setzero_si128();
        for (int i = 0; i < numConflicts; i++)
        {
            const Q ni = potentials.m_potentialContacts[conflicts[i]].m_contactPoint->m_contact.getSeparatingNormal().m_quad;
            __m128i jVec = _mm_add_epi32(iVec, _mm_set1_epi32(1));
            const __m128i nextI = jVec;
            for (int j = i + 1; j < numConflicts; j++)
            {
                const Q nj = potentials.m_potentialContacts[conflicts[j]].m_contactPoint->m_contact.getSeparatingNormal().m_quad;
                const Q prod = _mm_mul_ps(nj, ni);
                const Q dot = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(prod, prod, 0x55), _mm_shuffle_ps(prod, prod, 0x00)), _mm_shuffle_ps(prod, prod, 0xAA));
                const __m128i less = _mm_castps_si128(_mm_cmplt_ps(dot, minDot));
                bestI = _mm_or_si128(_mm_and_si128(less, iVec), _mm_andnot_si128(less, bestI));
                bestJ = _mm_or_si128(_mm_and_si128(less, jVec), _mm_andnot_si128(less, bestJ));
                minDot = _mm_min_ps(dot, minDot);
                jVec = _mm_add_epi32(jVec, _mm_set1_epi32(1));
            }
            iVec = nextI;
        }
        unsigned short* slotI = &conflicts[_mm_cvtsi128_si32(bestI)];
        unsigned short* slotJ = &conflicts[_mm_cvtsi128_si32(bestJ)];
        const hkpProcessCollisionOutput::ContactRef& refI = potentials.m_potentialContacts[*slotI];
        const hkpProcessCollisionOutput::ContactRef& refJ = potentials.m_potentialContacts[*slotJ];
        // only between convex shapes (bit 23: CONVEX among the alternate types)
        const unsigned int* alternate = input.m_input.val()->m_dispatcher.val()->m_hasAlternateType;
        if ((alternate[shapeA->m_type.m_storage] & 0x800000) == 0)
        {
            break;
        }
        const hkpConvexShape* childI = static_cast<const hkpConvexShape*>(shapeCollection->getChildShape(static_cast<const hkpAgent1nMachineEntry*>(refI.m_agentEntry)->m_shapeKeyPair.m_shapeKeyB, buffer));
        if ((alternate[childI->m_type.m_storage] & 0x800000) == 0)
        {
            break;
        }
        const hkpConvexShape* childJ = static_cast<const hkpConvexShape*>(shapeCollection->getChildShape(static_cast<const hkpAgent1nMachineEntry*>(refJ.m_agentEntry)->m_shapeKeyPair.m_shapeKeyB, bufferB));
        if ((alternate[childJ->m_type.m_storage] & 0x800000) == 0)
        {
            break;
        }
        const hkpConvexShape* shapes[2] = { childI, childJ };
        hkContactPoint* points[2] = { &refI.m_contactPoint->m_contact, &refJ.m_contactPoint->m_contact };
        hkCalcMultiPenetrationDepth(input.m_bodyA.val()->getTransform(), shapeA, shapes, 2, input.m_aTb, points);
        *slotJ = conflicts[numConflicts - 1];
        *slotI = conflicts[numConflicts - 2];
        numConflicts -= 2;
    }
    if (numConflicts != 0 && ids[conflicts[0]] != 0xFFFF)
    {
        // a conflict left over: its point is taken back
        input.m_contactMgr.val()->removeContactPointImpl(ids[conflicts[0]], *output.m_constraintOwner.val());
        ids[conflicts[0]] = 0xFFFF;
        input.m_contactMgr.val()->reserveContactPointsImpl(1);
    }

    // the potential points committed to their agents, or removed (the output's last point moved into their slot)
    dispatcher = input.m_input.val()->m_dispatcher;
    const int numPotentials = (int)(potentials.m_firstFreePotentialContact - &potentials.m_potentialContacts[0]);
    hkpProcessCollisionOutput::ContactRef* ref = potentials.m_firstFreePotentialContact;
    for (int i = numPotentials - 1; i >= 0; i--)
    {
        ref--;
        const unsigned short id = ids[i];
        if (id != 0xFFFF)
        {
            dispatcher->m_agent3Func[ref->m_agentEntry->m_agentType].m_commitPotentialFunc(ref->m_agentEntry, ref->m_agentData, id);
            ref->m_contactPoint->m_contactPointId = id;
        }
        else
        {
            input.m_contactMgr.val()->reserveContactPointsImpl(-1);
            dispatcher->m_agent3Func[ref->m_agentEntry->m_agentType].m_removePointFunc(ref->m_agentEntry, ref->m_agentData, 0xFFFF);
            output.m_firstFreeContactPoint = output.m_firstFreeContactPoint.val() - 1;
            *ref->m_contactPoint = *output.m_firstFreeContactPoint.val();
        }
    }
}
