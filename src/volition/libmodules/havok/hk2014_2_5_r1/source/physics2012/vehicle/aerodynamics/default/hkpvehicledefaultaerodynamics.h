// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/default/hkpvehicledefaultaerodynamics.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/hkpvehicleaerodynamics.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x30
class hkpVehicleDefaultAerodynamics : public hkpVehicleAerodynamics
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B713A0, delete(void* p, unsigned int nbytes) 0x00F7F6E0
    static const hkClass& staticClass();
    hkpVehicleDefaultAerodynamics(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultAerodynamics(); // 0x00F7F450
    virtual ~hkpVehicleDefaultAerodynamics(); // vtbl+0x00 0x00F7F640
    virtual void calcAerodynamics(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleAerodynamics::AerodynamicsDragOutput& dragInfoOut); // vtbl+0x10 0x00F7F4C0
    virtual float calcAerodynamicDrag(float forwardVelocity) const; // vtbl+0x14 0x00F7F480
    virtual float calcAerodynamicLift(float forwardVelocity) const; // vtbl+0x18 0x00F7F400

    static const hkInternalClassMember Members[];
    float m_airDensity; // 0x08
    float m_frontalArea; // 0x0C
    float m_dragCoefficient; // 0x10
    float m_liftCoefficient; // 0x14
    hkVector4f m_extraGravityws; // 0x20
};
