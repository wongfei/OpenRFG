// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/default/hkpvehicledefaultanalogdriverinput.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"

class hkClass;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x14
class hkpVehicleDriverInputAnalogStatus : public hkpVehicleDriverInputStatus
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVehicleDriverInputAnalogStatus); // new(unsigned int nbytes) 0x00B71860, delete(void* p, unsigned int nbytes) 0x00B71880
    static const hkClass& staticClass();
    hkpVehicleDriverInputAnalogStatus(hkFinishLoadedObjectFlag);
    hkpVehicleDriverInputAnalogStatus(); // 0x00B634D0 (.h:38)
    virtual ~hkpVehicleDriverInputAnalogStatus(); // vtbl+0x00 0x00B634E0 (.h:41)
    virtual hkpVehicleDriverInputStatus* clone() const; // vtbl+0x10 0x00F7E280

    static const hkInternalClassMember Members[];
    float m_positionX; // 0x08
    float m_positionY; // 0x0C
    hkBool m_handbrakeButtonPressed; // 0x10
    hkBool m_reverseButtonPressed; // 0x11
};

// size 0x18
class hkpVehicleDefaultAnalogDriverInput : public hkpVehicleDriverInput
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71910, delete(void* p, unsigned int nbytes) 0x00B71930
    static const hkClass& staticClass();
    hkpVehicleDefaultAnalogDriverInput(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultAnalogDriverInput(); // 0x00F7E0C0
    virtual ~hkpVehicleDefaultAnalogDriverInput(); // vtbl+0x00 0x00B634F0
    virtual void calcDriverInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& filteredInputOut); // vtbl+0x10 0x00F7DFC0
    virtual float calcAcceleratorInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& input) const; // vtbl+0x14 0x00F7E0F0
    virtual float calcBrakeInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut) const; // vtbl+0x18 0x00F7E150
    virtual hkBool calcTryingToReverse(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut); // vtbl+0x1C 0x00F7E2D0
    virtual float calcSteeringInput(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInputAnalogStatus* deviceStatus, hkpVehicleDriverInput::FilteredDriverInputOutput& FilteredDriverInputOutputOut) const; // vtbl+0x20 0x00F7E1A0

    static const hkInternalClassMember Members[];
    float m_slopeChangePointX; // 0x08
    float m_initialSlope; // 0x0C
    float m_deadZone; // 0x10
    hkBool m_autoReverse; // 0x14
};
