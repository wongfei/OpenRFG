// Original: volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.h
// Implementation: volition/rfg/code/vehicle/havok/hk_custom_vehicle_extra_downforce.cpp
//
// An action that pushes a vehicle's chassis down while enabled: when every wheel has a contact body, applyAction
// applies a force of chassis mass times m_acceleration (decompiled).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"

struct hkContainerHeapAllocator;
class hkStepInfo;
class hk_custom_vehicle_instance;
class hkpAction;
class hkpEntity;
class hkpPhantom;

// size 0x28
class hk_custom_vehicle_extra_downforce : public hkpUnaryAction
{
public:
    hk_custom_vehicle_extra_downforce(hk_custom_vehicle_instance* p_vehicle); // 0x00B63AF0 (:34)
    hk_custom_vehicle_extra_downforce(hkpEntity* hk_entity); // 0x00B63AA0 (:24)
    virtual void applyAction(const hkStepInfo& step_info); // vtbl+0x10 0x00B74440 (:45)
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& new_entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& new_phantoms) const; // vtbl+0x20 0x00B74550 (:106)
    virtual void enable(bool enable); // vtbl+0x24 0x00B63AD0 (.h:68)
    virtual ~hk_custom_vehicle_extra_downforce(); // vtbl+0x00 0x00B63AE0

    float m_acceleration; // 0x1C
protected:
    hk_custom_vehicle_instance* m_vehicle; // 0x20
    bool m_enable; // 0x24
};
