// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

// size 0x30
struct hkpProcessCdPoint
{
    enum Type
    {
        TYPE_CLOSEST = 0,
        TYPE_OTHER = 1,
        TYPE_UNSPECIFIED = 2,
    };

    hkContactPoint m_contact; // 0x00
    unsigned int m_contactPointId; // 0x20
    unsigned int m_isShortestPoint; // 0x24
    unsigned int m_padding[2]; // 0x28
};
