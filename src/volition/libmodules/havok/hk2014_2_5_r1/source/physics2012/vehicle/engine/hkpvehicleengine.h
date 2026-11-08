// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/hkpvehicleengine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x8
class hkpVehicleEngine : public hkReferencedObject
{
public:
    // size 0x8
    struct EngineOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(EngineOutput);

        float m_torque; // 0x00
        float m_rpm; // 0x04
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F7E8F0
    static const hkClass& staticClass();
    virtual void calcEngineInfo(const float, const hkpVehicleInstance*, const hkpVehicleDriverInput::FilteredDriverInputOutput&, const hkpVehicleTransmission::TransmissionOutput&, EngineOutput&) = 0; // vtbl+0x10
public:
    hkpVehicleEngine(); // 0x00F7E8E0
public:
    hkpVehicleEngine(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleEngine(); // vtbl+0x00 0x00B631A0

    static const hkInternalClassMember Members[];
};
