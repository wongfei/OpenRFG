// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/aerodynamics/hkpvehicleaerodynamics.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleAerodynamics : public hkReferencedObject
{
public:
    // size 0x20
    struct AerodynamicsDragOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(AerodynamicsDragOutput);

        hkVector4f m_aerodynamicsForce; // 0x00
        hkVector4f m_aerodynamicsTorque; // 0x10
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7F650
    static const hkClass& staticClass();
    virtual void calcAerodynamics(const float, const hkpVehicleInstance*, AerodynamicsDragOutput&) = 0; // vtbl+0x10
public:
    hkpVehicleAerodynamics(); // 0x00F7F620
public:
    hkpVehicleAerodynamics(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleAerodynamics(); // vtbl+0x00 0x00F7F630

    static const hkInternalClassMember Members[];
};
