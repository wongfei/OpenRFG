// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpCollisionEvent;
struct hkpContactPointAddedEvent;
class hkpContactPointEvent;
struct hkpContactPointRemovedEvent;
struct hkpContactProcessEvent;
class hkpEntity;

// size 0x1
class hkpEntityCallbackUtil
{
public:
    static void fireEntityAdded(hkpEntity* entity); // 0x00E15A10
    static void fireEntityRemoved(hkpEntity* entity); // 0x00E15AB0
    static void fireEntityShapeSet(hkpEntity* entity); // 0x00E15CB0
    static void fireEntityDeleted(hkpEntity* entity); // 0x00E15DF0
    static void fireEntitySetMotionType(hkpEntity* entity); // 0x00E15D50
    static void fireContactPointCallback(hkpEntity* entity, hkpContactPointEvent& event); // 0x00E1A110
    static void fireContactConstraintAddedCallback(hkpEntity* entity, hkpCollisionEvent& event); // 0x00E15F40
    static void fireContactConstraintRemovedCallback(hkpEntity* entity, hkpCollisionEvent& event); // 0x00E15FF0
public:
    static void fireContactPointCallbackInternal(hkpEntity* entity, hkpContactPointEvent& event); // 0x00E15E90
public:
    static void fireContactPointAdded(hkpEntity* entity, hkpContactPointAddedEvent& event); // 0x00E1A130
    static void fireContactPointRemoved(hkpEntity* entity, hkpContactPointRemovedEvent& event); // 0x00E1A150
    static void fireContactProcess(hkpEntity* entity, hkpContactProcessEvent& event); // 0x00E1A170
public:
    static void fireContactPointAddedInternal(hkpEntity* entity, hkpContactPointAddedEvent& event); // 0x00E160A0
    static void fireContactPointRemovedInternal(hkpEntity* entity, hkpContactPointRemovedEvent& event); // 0x00E16140
    static void fireContactProcessInternal(hkpEntity* entity, hkpContactProcessEvent& event); // 0x00E161E0

};
