// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/default/hkpvehicledefaulttransmission.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x34
class hkpVehicleDefaultTransmission : public hkpVehicleTransmission
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B71610, delete(void* p, unsigned int nbytes) 0x00B9B0E0
    static const hkClass& staticClass();
    hkpVehicleDefaultTransmission(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultTransmission(); // 0x00F80910
    virtual ~hkpVehicleDefaultTransmission(); // vtbl+0x00 0x00B9B020
    static float calculatePrimaryTransmissionRatio(const float vehicleTopSpeedMPH, const float wheelRadius, const float maxEngineRpm, const float topGearRatio); // 0x00F80590
    static float calculatePrimaryTransmissionRatioKPH(const float vehicleTopSpeedKPH, const float wheelRadius, const float maxEngineRpm, const float topGearRatio); // 0x00F805C0
    virtual void calcTransmission(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut); // vtbl+0x10 0x00F80660
    virtual float calcMainTransmittedTorque(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const; // vtbl+0x14 0x00F806E0
    virtual float calcTransmissionRPM(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const; // vtbl+0x18 0x00F80710
    virtual hkBool calcIsReversing(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const; // vtbl+0x1C 0x00F808E0
    virtual void updateCurrentGear(const float deltaTime, const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut); // vtbl+0x20 0x00F80860
    virtual float getCurrentRatio(const hkpVehicleInstance* vehicle, hkpVehicleTransmission::TransmissionOutput& transmissionOut) const; // vtbl+0x24 0x00F80610

    static const hkInternalClassMember Members[];
    float m_downshiftRPM; // 0x08
    float m_upshiftRPM; // 0x0C
    float m_primaryTransmissionRatio; // 0x10
    float m_clutchDelayTime; // 0x14
    float m_reverseGearRatio; // 0x18
    hkArray<float,hkContainerHeapAllocator> m_gearsRatio; // 0x1C
    hkArray<float,hkContainerHeapAllocator> m_wheelsTorqueRatio; // 0x28
};
