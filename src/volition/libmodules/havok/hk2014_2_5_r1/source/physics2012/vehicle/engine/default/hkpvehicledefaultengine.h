// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/default/hkpvehicledefaultengine.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S8).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/driverinput/hkpvehicledriverinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/engine/hkpvehicleengine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/vehicle/transmission/hkpvehicletransmission.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpVehicleInstance;

// size 0x30
class hkpVehicleDefaultEngine : public hkpVehicleEngine
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00B719C0, delete(void* p, unsigned int nbytes) 0x00B719E0
    static const hkClass& staticClass();
    hkpVehicleDefaultEngine(hkFinishLoadedObjectFlag);
    hkpVehicleDefaultEngine(); // 0x00F7E860
    virtual ~hkpVehicleDefaultEngine(); // vtbl+0x00 0x00B63530
    virtual void calcEngineInfo(const float deltaTime, const hkpVehicleInstance* vehicle, const hkpVehicleDriverInput::FilteredDriverInputOutput& filteredDriverInputInfo, const hkpVehicleTransmission::TransmissionOutput& transmissionInfo, hkpVehicleEngine::EngineOutput& engineOutput); // vtbl+0x10 0x00F7E6D0

    static const hkInternalClassMember Members[];
    float m_minRPM; // 0x08
    float m_optRPM; // 0x0C
    float m_maxRPM; // 0x10
    float m_maxTorque; // 0x14
    float m_torqueFactorAtMinRPM; // 0x18
    float m_torqueFactorAtMaxRPM; // 0x1C
    float m_resistanceFactorAtMinRPM; // 0x20
    float m_resistanceFactorAtOptRPM; // 0x24
    float m_resistanceFactorAtMaxRPM; // 0x28
    float m_clutchSlipRPM; // 0x2C
};
