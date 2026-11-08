// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"

class hkpDynamicsContactMgr;
class hkpEntity;

// size 0x70
struct hkpToiEvent
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpToiEvent);
    HK_DECLARE_POD_TYPE();

    float m_time; // 0x00
    float m_seperatingVelocity; // 0x04
    hkBool m_useSimpleHandling; // 0x08
    hkpEntity* m_entities[2]; // 0x0C
    hkpDynamicsContactMgr* m_contactMgr; // 0x14
    hkpContactPointProperties m_properties; // 0x18
    unsigned int m_extendedUserDatas[7]; // 0x2C
    hkContactPoint m_contactPoint; // 0x50
};
