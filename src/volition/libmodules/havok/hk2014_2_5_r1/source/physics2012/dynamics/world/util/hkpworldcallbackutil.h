// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkStepInfo;
class hkpAction;
class hkpCollisionEvent;
struct hkpConstraintBrokenEvent;
class hkpConstraintInstance;
struct hkpConstraintRepairedEvent;
class hkpContactImpulseLimitBreachedListenerInfo;
struct hkpContactPointAddedEvent;
class hkpContactPointEvent;
struct hkpContactPointRemovedEvent;
struct hkpContactProcessEvent;
class hkpEntity;
class hkpPhantom;
class hkpSimulationIsland;
class hkpWorld;

// size 0x1
class hkpWorldCallbackUtil
{
public:
    static void fireActionAdded(hkpWorld* world, hkpAction* action); // 0x00E18200
    static void fireActionRemoved(hkpWorld* world, hkpAction* action); // 0x00E182A0
    static void fireEntityAdded(hkpWorld* world, hkpEntity* entity); // 0x00E18340
    static void fireEntityRemoved(hkpWorld* world, hkpEntity* entity); // 0x00E183E0
    static void fireEntityShapeSet(hkpWorld* world, hkpEntity* entity); // 0x00E18480
    static void fireEntitySetMotionType(hkpWorld* world, hkpEntity* entity); // 0x00E18520
    static void firePhantomAdded(hkpWorld* world, hkpPhantom* Phantom); // 0x00E185C0
    static void firePhantomRemoved(hkpWorld* world, hkpPhantom* Phantom); // 0x00E18660
    static void firePhantomShapeSet(hkpWorld* world, hkpPhantom* Phantom); // 0x00E18700
    static void fireConstraintAdded(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E187A0
    static void fireConstraintRemoved(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E18840
    static void fireConstraintViolated(hkpWorld* world, hkpConstraintInstance* constraint); // 0x00E188E0
    static void fireConstraintBroken(hkpWorld* world, const hkpConstraintBrokenEvent& event); // 0x00E18980
    static void fireConstraintRepaired(hkpWorld* world, const hkpConstraintRepairedEvent& event); // 0x00E18A10
    static void fireContactPointCallback(hkpWorld* world, hkpContactPointEvent& event); // 0x00E18AA0
    static void fireContactConstraintAddedCallback(hkpWorld* world, hkpCollisionEvent& event); // 0x00E18B40
    static void fireContactConstraintRemovedCallback(hkpWorld* world, hkpCollisionEvent& event); // 0x00E18BE0
    static void firePostSimulationCallback(hkpWorld* world); // 0x00E19100
    static void firePostIntegrateCallback(hkpWorld* world, const hkStepInfo& info); // 0x00E191B0
    static void firePostCollideCallback(hkpWorld* world, const hkStepInfo& info); // 0x00E19250
    static void fireIslandActivated(hkpWorld* world, hkpSimulationIsland* island); // 0x00E18DA0
    static void fireIslandDeactivated(hkpWorld* world, hkpSimulationIsland* island); // 0x00E18F50
    static void fireIslandPostIntegrateCallback(hkpWorld* world, hkpSimulationIsland* island, const hkStepInfo& info); // 0x00E192F0
    static void fireIslandPostCollideCallback(hkpWorld* world, hkpSimulationIsland* island, const hkStepInfo& info); // 0x00E19390
    static void fireWorldDeleted(hkpWorld* world); // 0x00E18C80
    static void fireWorldRemoveAll(hkpWorld* world); // 0x00E18D10
    static void fireInactiveEntityMoved(hkpWorld* world, hkpEntity* entity); // 0x00E194D0
    static void fireContactImpulseLimitBreached(hkpWorld* world, const hkpContactImpulseLimitBreachedListenerInfo* breachedContacts, int numBreachedContacts); // 0x00E19430
    static void fireContactPointAdded(hkpWorld* world, hkpContactPointAddedEvent& event); // 0x00E19570
    static void fireContactPointRemoved(hkpWorld* world, hkpContactPointRemovedEvent& event); // 0x00E19610
    static void fireContactProcess(hkpWorld* world, hkpContactProcessEvent& event); // 0x00E196B0

};
