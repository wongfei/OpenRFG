// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.h
//
// Havok 2014.2.5: layouts and functions from rfg.pdb, the entry types and the loop macro ported from the 2014.1 SDK
// header (havok_impl.md HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnntrack.h"

class hkMotionState;
class hkpCdBody;
class hkpCollisionDispatcher;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
class hkpLinkedCollidable;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0x20
struct hkpAgentNnMachinePaddedEntry : public hkpAgentNnEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNnMachinePaddedEntry);

    unsigned int m_padOutTo16BytesAlignment[2]; // 0x18
};

// size 0x30
struct hkpAgentNnMachineTimEntry : public hkpAgentNnEntry
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpAgentNnMachineTimEntry);

    float m_timeOfSeparatingNormal; // 0x18 (only used if tims are enabled)
    hkVector4f m_separatingNormal; // 0x20
};

// Every entry of a track, sector by sector (the last sector holds m_bytesUsedInLastSector bytes).
#define HK_FOR_ALL_AGENT_ENTRIES_BEGIN(TRACK, ENTRY) \
    { \
        for (int HKLOOP_sectorIndex = 0; HKLOOP_sectorIndex < (TRACK).m_sectors.getSize();) \
        { \
            hkpAgentNnSector* HKLOOP_currentSector = (TRACK).m_sectors[HKLOOP_sectorIndex]; \
            hkpAgentNnEntry* ENTRY = HKLOOP_currentSector->getBegin(); \
            HKLOOP_sectorIndex++; \
            hkpAgentNnEntry* HKLOOP_endEntry = (HKLOOP_sectorIndex == (TRACK).m_sectors.getSize()) \
                ? hkAddByteOffset(ENTRY, (TRACK).m_bytesUsedInLastSector) \
                : HKLOOP_currentSector->getEnd(); \
            for (; ENTRY < HKLOOP_endEntry; ENTRY = hkAddByteOffset(ENTRY, ENTRY->m_size)) \
            {

#define HK_FOR_ALL_AGENT_ENTRIES_END \
            } \
        } \
    }

void hkAgentNnMachine_GetAgentType(const hkpCdBody* cdBodyA, const hkpCdBody* cdBodyB, const hkpProcessCollisionInput& input, int& agentTypeOut, int& isFlippedOut); // 0x00DDADA0
hkpAgentNnEntry* hkAgentNnMachine_CreateAgent(hkpAgentNnTrack& track, hkpLinkedCollidable* collA, const hkpCdBody* firstNonTransformBodyA, hkpLinkedCollidable* collB, const hkpCdBody* firstNonTransformBodyB, unsigned char cdBodyHasTransformFlag, int agentType, const hkpProcessCollisionInput& input, hkpContactMgr* mgr); // 0x00DDB880
void hkAgentNnMachine_DestroyAgent(hkpAgentNnTrack& track, hkpAgentNnEntry* entry, hkpCollisionDispatcher* dispatcher, hkpConstraintOwner& constraintOwner); // 0x00DDBA10
bool hkAgentNnMachine_IsEntryOnTrack(hkpAgentNnTrack& track, hkpAgentNnEntry* entry); // 0x00DDAA60
void hkAgentNnMachine_InternalDeallocateEntry(hkpAgentNnTrack& track, hkpAgentNnEntry* entry); // 0x00DDAE60
void hkAgentNnMachine_UpdateShapeCollectionFilter(hkpAgentNnEntry* entry, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner); // 0x00DDBB70
void hkAgentNnMachine_InvalidateTimInAgent(hkpAgentNnEntry* entry, const hkpCollisionInput& input); // 0x00DDAAB0
void hkAgentNnMachine_WarpTimeInAgent(hkpAgentNnEntry* entry, float oldTime, float newTime, const hkpCollisionInput& input); // 0x00DDAB50
void hkAgentNnMachine_AppendTrack(hkpAgentNnTrack& track, hkpAgentNnTrack& appendee); // 0x00DDC240
hkpAgentNnEntry* hkAgentNnMachine_CopyAndRelinkAgentEntry(hkpAgentNnTrack& destTrack, hkpAgentNnEntry* entry); // 0x00DDBC70
void hkAgentNnMachine_DestroyTrack(hkpAgentNnTrack& track, hkpCollisionDispatcher* dispatcher, hkpConstraintOwner& constraintOwner); // 0x00DDBCC0
void hkAgentNnMachine_ProcessAgent(hkpAgentNnEntry* entry, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, hkpContactMgr* mgr); // 0x00DDBD10
void hkAgentNnMachine_ProcessTrack(hkpConstraintOwner* owner, hkpAgentNnTrack& agentTrack, const hkpProcessCollisionInput& input); // 0x00DDC380
hkpAgentNnEntry* hkAgentNnMachine_FindAgent(const hkpLinkedCollidable* collA, const hkpLinkedCollidable* collB); // 0x00DDAC40
void hkAgentNnMachine_AssertTrackValidity(hkpAgentNnTrack& track); // 0x00DDAC90
const hkpCdBody* hkAgentMachine_processTransformedShapes(const hkpCdBody* cdBody, hkpCdBody* newCdBodies, hkMotionState* newMotionStates, int numSlots, hkPadSpu<unsigned char>& cdBodyHasTransformFlag); // 0x00DDAF70
void hkAgentNnMachine_TouchAgent(hkpAgentEntry* entry, const hkpProcessCollisionInput& input); // 0x00DDA960
