// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintinfo.h"

class hkpConstraintInstance;

// size 0x18
class hkpConstraintOwner : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E0E930
    void addConstraintInfo(hkpConstraintInstance* constraint, hkpConstraintInfo& delta); // 0x00E0B0C0
    void subConstraintInfo(hkpConstraintInstance* constraint, hkpConstraintInfo& delta); // 0x00E0AC50
    virtual ~hkpConstraintOwner(); // vtbl+0x00 0x00E0E7C0
    virtual void addConstraintToCriticalLockedIsland(hkpConstraintInstance* constraint); // vtbl+0x10 0x00E0E780
    virtual void removeConstraintFromCriticalLockedIsland(hkpConstraintInstance* constraint); // vtbl+0x14 0x00E0E790
    virtual void addCallbackRequest(hkpConstraintInstance* constraint, int request); // vtbl+0x18 0x00E0E7A0
    hkpConstraintOwner(); // 0x00E0E770
    virtual void checkAccessRw(); // vtbl+0x1C 0x00E0E7B0

    hkpConstraintInfo m_constraintInfo; // 0x08
};
