// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h
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
class hkpVehicleTransmission : public hkReferencedObject
{
public:
    // size 0x14
    struct TransmissionOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(TransmissionOutput);
        TransmissionOutput(); // 0x00B63B90

        float m_transmissionRPM; // 0x00
        float m_mainTransmittedTorque; // 0x04
        float* m_wheelsTransmittedTorque; // 0x08
        signed char m_numWheelsTramsmittedTorque; // 0x0C
        hkBool m_isReversing; // 0x0D
        signed char m_currentGear; // 0x0E
        hkBool m_delayed; // 0x0F
        float m_clutchDelayCountdown; // 0x10
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F80970
    static const hkClass& staticClass();
    virtual void calcTransmission(const float, const hkpVehicleInstance*, TransmissionOutput&) = 0; // vtbl+0x10
public:
    hkpVehicleTransmission(); // 0x00F80960
public:
    hkpVehicleTransmission(hkFinishLoadedObjectFlag);
    virtual ~hkpVehicleTransmission(); // vtbl+0x00 0x00B63190

    static const hkInternalClassMember Members[];
};
