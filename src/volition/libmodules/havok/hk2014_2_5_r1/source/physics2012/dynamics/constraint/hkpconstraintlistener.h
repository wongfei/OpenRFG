// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkClass;
class hkpConstraintInstance;
class hkpWorld;

// size 0x18 (2014.2.5 layout from rfg.pdb): a breakable constraint broke
struct hkpConstraintBrokenEvent
{
    hkpWorld* m_world; // 0x00
    hkpConstraintInstance* m_constraintInstance; // 0x04
    const hkClass* m_eventSource; // 0x08
    void* m_eventSourceDetails; // 0x0C (hkpConstraintTrackerData*)
    float m_actualImpulse; // 0x10
    float m_impulseLimit; // 0x14
};

// size 0x10: a broken constraint was repaired
struct hkpConstraintRepairedEvent
{
    hkpWorld* m_world; // 0x00
    hkpConstraintInstance* m_constraintInstance; // 0x04
    const hkClass* m_eventSource; // 0x08
    void* m_eventSourceDetails; // 0x0C
};

// size 0x4
class hkpConstraintListener
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpConstraintListener); // delete(void* p, unsigned int nbytes) 0x008147C0
    virtual ~hkpConstraintListener(); // vtbl+0x00 0x0080D430 (.h:71)
    virtual void constraintAddedCallback(hkpConstraintInstance* constraint); // vtbl+0x04 0x0080D440 (.h:74)
    virtual void constraintRemovedCallback(hkpConstraintInstance* constraint); // vtbl+0x08 0x0080D450 (.h:77)
    virtual void constraintDeletedCallback(hkpConstraintInstance* constraint); // vtbl+0x0C 0x0080D460 (.h:80)
    virtual void constraintViolatedCallback(hkpConstraintInstance* constraint); // vtbl+0x10 0x0080D470 (.h:83)
    virtual void constraintBreakingCallback(const hkpConstraintBrokenEvent& event); // vtbl+0x14 0x0080D480 (.h:86)
    virtual void constraintRepairedCallback(const hkpConstraintRepairedEvent& event); // vtbl+0x18 0x0080D490 (.h:89)
    hkpConstraintListener(); // 0x0080DBA0

    // 0x00 vfptr
};
