// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/hkpvehiclevelocitydamper.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x14
class hkpVehicleDefaultVelocityDamper : public hkpVehicleVelocityDamper
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleDefaultVelocityDamper); // new(unsigned int nbytes) 0x00B713E0, delete(void* p, unsigned int nbytes) 0x00F80500
    static const hkClass& staticClass();
    hkpVehicleDefaultVelocityDamper(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultVelocityDamper(); // 0x00F80320
    virtual ~hkpVehicleDefaultVelocityDamper(); // vtbl+0x00 0x00F80460
    virtual void applyVelocityDamping(const float deltaTime, hkpVehicleInstance& vehicle); // vtbl+0x10 0x00F80350

    static const hkInternalClassMember Members[];
    float m_normalSpinDamping; // 0x08
    float m_collisionSpinDamping; // 0x0C
    float m_collisionThreshold; // 0x10
};
