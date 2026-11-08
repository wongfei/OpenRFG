// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleDriverInputStatus : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00B71400
    static const hkClass& staticClass();
    hkpVehicleDriverInputStatus(hkFinishLoadedObjectFlag);
    hkpVehicleDriverInputStatus(); // 0x00B63150 (.h:25)
    virtual ~hkpVehicleDriverInputStatus(); // vtbl+0x00 0x00B63160 (.h:26)
    virtual hkpVehicleDriverInputStatus* clone() const = 0; // vtbl+0x10

    static const hkInternalClassMember Members[];
};

// size 0x8
class hkpVehicleDriverInput : public hkReferencedObject
{
public:
    // size 0x10
    struct FilteredDriverInputOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(FilteredDriverInputOutput);
        FilteredDriverInputOutput(); // 0x00B63B80

        float m_acceleratorPedalInput; // 0x00
        float m_brakePedalInput; // 0x04
        float m_steeringWheelInput; // 0x08
        hkBool m_handbrakeOn; // 0x0C
        hkBool m_tryingToReverse; // 0x0D
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7E5C0
    static const hkClass& staticClass();
    hkpVehicleDriverInput(hkFinishLoadedObjectFlag);
    hkpVehicleDriverInput(); // 0x00F7E5A0
    virtual ~hkpVehicleDriverInput(); // vtbl+0x00 0x00B63170
    virtual void calcDriverInput(const float, const hkpVehicleInstance*, const hkpVehicleDriverInputStatus*, FilteredDriverInputOutput&) = 0; // vtbl+0x10

    static const hkInternalClassMember Members[];
};
