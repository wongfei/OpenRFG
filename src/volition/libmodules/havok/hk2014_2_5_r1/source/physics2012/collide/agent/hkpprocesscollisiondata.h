// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisiondata.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members of the 2014.1 SDK added by hand
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

class hkpConstraintOwner;
class hkpContactPointProperties;

// size 0x3080
struct hkpProcessCollisionData
{
    // size 0x70
    struct ToiInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ToiInfo);
        // 0x00DDD110
        ToiInfo()
            : m_time(HK_REAL_MAX)
        {
        }

        // 0x00D79690
        void flip()
        {
            hkVector4f norm = m_contactPoint.getSeparatingNormal();
            norm.setNeg<3>(norm);
            m_contactPoint.setSeparatingNormal(norm);
            m_gskCache.flip();
        }

        hkContactPoint m_contactPoint; // 0x00
        hkPadSpu<float> m_time; // 0x20
        hkPadSpu<float> m_seperatingVelocity; // 0x24
        hkGskCache16 m_gskCache; // 0x30
        hkContactPointPropertiesWithExtendedUserData16 m_properties; // 0x40
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpProcessCollisionData);
    int getNumContactPoints() const { return int(m_firstFreeContactPoint.val() - &m_contactPoints[0]); } // 0x00DD7DC0
    hkpProcessCdPoint& getContactPoint(int i) { return m_contactPoints[i]; }
    hkpProcessCdPoint* getFirstContactPoint() { return &m_contactPoints[0]; } // 0x00E1B160
    hkpProcessCdPoint* getEnd() { return m_firstFreeContactPoint; } // 0x00DD7DE0
    hkBool isEmpty() const { return m_firstFreeContactPoint.val() == &m_contactPoints[0]; } // 0x00DDCFA0
    hkBool hasToi() const { return m_toi.m_time.val() != HK_REAL_MAX; } // 0x00DF8D60
    hkContactPoint& getToiContactPoint() { return m_toi.m_contactPoint; }
    float getToi() const { return m_toi.m_time; }
    hkpContactPointProperties& getToiProperties() { return m_toi.m_properties; }
    hkpProcessCollisionData(hkpConstraintOwner* owner) { m_constraintOwner = owner; } // 0x00DDD130

    hkPadSpu<hkpProcessCdPoint*> m_firstFreeContactPoint; // 0x00
    hkPadSpu<hkpConstraintOwner*> m_constraintOwner; // 0x04
    hkpProcessCdPoint m_contactPoints[256]; // 0x10
    ToiInfo m_toi; // 0x3010
};
