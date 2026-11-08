// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/default/hkpdefaultworldmaintenancemgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/maintenance/hkpworldmaintenancemgr.h"

class hkStepInfo;
class hkpWorld;

// size 0x10
class hkpDefaultWorldMaintenanceMgr : public hkpWorldMaintenanceMgr
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA9F0, delete(void* p, unsigned int nbytes) 0x00E1FEA0
    hkpDefaultWorldMaintenanceMgr(); // 0x00E1FCC0
    virtual void init(hkpWorld* world); // vtbl+0x10 0x00E1FCA0
    virtual void performMaintenance(hkpWorld* world, hkStepInfo& stepInfo); // vtbl+0x14 0x00E1FCF0
    virtual void performMaintenanceNoSplit(hkpWorld* world, hkStepInfo& stepInfo); // vtbl+0x18 0x00E1FDC0
public:
    void resetWorldTime(hkpWorld* world, hkStepInfo& stepInfo); // 0x00E1FF80
    void markIslandsForDeactivationDeprecated(hkpWorld*, hkStepInfo&);
public:
    virtual ~hkpDefaultWorldMaintenanceMgr(); // vtbl+0x00 0x00E1FE90

public:
    float m_minAllowedTimeValue; // 0x08
    float m_maxAllowedTimeValue; // 0x0C
};
