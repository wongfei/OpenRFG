// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/pair/hkppaircollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpCollisionFilter;
struct hkpConstraintBrokenEvent;
class hkpConstraintInstance;
struct hkpConstraintRepairedEvent;
class hkpWorld;

// size 0x44
class hkpConstraintCollisionFilter : public hkpPairCollisionFilter, public hkpConstraintListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEFD60, new(unsigned int nbytes) 0x00815FA0, delete(void* p, unsigned int nbytes) 0x00DEFE90
    static const hkClass& staticClass(); // 0x00DEFD70
    hkpConstraintCollisionFilter(hkFinishLoadedObjectFlag flag); // 0x00DEFDE0
    hkpConstraintCollisionFilter(const hkpCollisionFilter* childFilter); // 0x00E0ECF0
    virtual ~hkpConstraintCollisionFilter(); // vtbl+0x00 0x00E0EA10
    void updateFromWorld(hkpWorld* world); // 0x00E0EAE0
    virtual void constraintAddedCallback(hkpConstraintInstance* constraint); // vtbl+0x04 0x00E0EBB0
    virtual void constraintRemovedCallback(hkpConstraintInstance* constraint); // vtbl+0x08 0x00E0EBF0
    virtual void init(hkpWorld* world); // vtbl+0x10 0x00E0EA80
    virtual void constraintBreakingCallback(const hkpConstraintBrokenEvent& event); // vtbl+0x14 0x00E0EC30
    virtual void constraintRepairedCallback(const hkpConstraintRepairedEvent& event); // vtbl+0x18 0x00E0EC90
    void _constraintBreakingCallbackImmediate(hkpConstraintInstance* instance, hkBool constraintBroken); // 0x00E0EA40

    static const hkInternalClassMember Members[];
};
