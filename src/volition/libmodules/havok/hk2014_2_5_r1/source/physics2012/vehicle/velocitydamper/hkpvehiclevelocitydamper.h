// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/velocitydamper/hkpvehiclevelocitydamper.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleVelocityDamper : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F80470
    static const hkClass& staticClass();
    virtual void applyVelocityDamping(const float, hkpVehicleInstance&) = 0; // vtbl+0x10
public:
    hkpVehicleVelocityDamper(); // 0x00F80300
public:
    hkpVehicleVelocityDamper(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleVelocityDamper(); // vtbl+0x00 0x00F80310

    static const hkInternalClassMember Members[];
};
