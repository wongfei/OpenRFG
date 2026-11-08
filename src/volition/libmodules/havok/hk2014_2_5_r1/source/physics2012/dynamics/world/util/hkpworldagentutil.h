// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldagentutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


struct hkpAgentNnEntry;
struct hkpCollisionInput;
class hkpEntity;
class hkpLinkedCollidable;
struct hkpProcessCollisionInput;
class hkpSimulationIsland;

// size 0x1
class hkpWorldAgentUtil
{
public:
    static hkpAgentNnEntry* addAgent(hkpLinkedCollidable* collA, hkpLinkedCollidable* collB, const hkpProcessCollisionInput& input); // 0x00E1DF40
    static void removeAgent(hkpAgentNnEntry* agent); // 0x00E1E0B0
    static void removeAgentAndItsToiEvents(hkpAgentNnEntry* agent); // 0x00E1E160
    static hkpSimulationIsland* getIslandFromAgentEntry(hkpAgentNnEntry* entry, hkpSimulationIsland* candidateA, hkpSimulationIsland* candidateB); // 0x00E1DDB0
    static void updateEntityShapeCollectionFilter(hkpEntity* entity, hkpCollisionInput& collisionInput); // 0x00E1E190
    static void invalidateTim(hkpEntity* entity, const hkpCollisionInput& collisionInput); // 0x00E1DE40
    static void warpTime(hkpSimulationIsland* island, float oldTime, float newTime, const hkpCollisionInput& collisionInput); // 0x00E1DE70

};
