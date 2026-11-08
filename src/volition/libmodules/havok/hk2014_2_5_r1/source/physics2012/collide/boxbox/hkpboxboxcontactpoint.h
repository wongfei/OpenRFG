// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcontactpoint.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline constructor (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
// size 0x4
class hkpFeatureContactPoint
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpFeatureContactPoint);
    hkpFeatureContactPoint() { m_contactPointId = 0; } // 0x00DD4180

    unsigned char m_featureIdA; // 0x00
    unsigned char m_featureIdB; // 0x01
    unsigned short m_contactPointId; // 0x02
};
