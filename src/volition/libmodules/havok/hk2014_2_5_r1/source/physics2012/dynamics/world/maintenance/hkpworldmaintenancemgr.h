// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/hkpworldmaintenancemgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkStepInfo;
class hkpWorld;

// size 0x8
class hkpWorldMaintenanceMgr : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E1FE50
    virtual ~hkpWorldMaintenanceMgr(); // vtbl+0x00 0x00E1FC90
    virtual void init(hkpWorld*) = 0; // vtbl+0x10
    virtual void performMaintenance(hkpWorld*, hkStepInfo&) = 0; // vtbl+0x14
    hkpWorldMaintenanceMgr(); // 0x00E1FC80
    virtual void performMaintenanceNoSplit(hkpWorld*, hkStepInfo&) = 0; // vtbl+0x18

};
