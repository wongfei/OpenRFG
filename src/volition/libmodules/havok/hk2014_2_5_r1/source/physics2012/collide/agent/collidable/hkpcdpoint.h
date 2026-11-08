// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). Hand-maintained since HK5 (nobuild in the hdrgen
// specs): the constructors are inline, as in the SDK (hkpCdPoint.h).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"

class hkSimdFloat32;
class hkpCdBody;

// size 0x40
struct alignas(16) hkpCdPoint
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCdPoint);
    // 0x00D8FAB0 (.h:25)
    hkpCdPoint(const hkpCdBody& a, const hkpCdBody& b, const hkContactPoint& cp)
        : m_cdBodyA(a), m_cdBodyB(b)
    {
        m_contact = cp;
        m_unweldedNormal = cp.m_separatingNormal;
    }

    // 0x00DB7080 (.h:24): the contact is left uninitialized
    hkpCdPoint(const hkpCdBody& a, const hkpCdBody& b)
        : m_cdBodyA(a), m_cdBodyB(b)
    {
    }
    const hkContactPoint& getContact() const; // 0x0074D820 (.h:26)
    void setContact(const hkVector4f&, const hkVector4f&, const hkVector4f&, float);
    void setContact(const hkVector4f&, const hkVector4f&, float);
    void setContact(const hkContactPoint& cp); // 0x00B63D70 (.h:27)
    void setContactDistance(const hkSimdFloat32&);
    void setContactDistance(float);
    void setContactNormal(const hkVector4f& norm); // 0x00DC00D0
    void setUnweldedNormal(const hkVector4f& norm); // 0x00DBC2A0
    const hkVector4f& getUnweldedNormal() const; // 0x00D81120

public:
    hkContactPoint m_contact; // 0x00
    hkVector4f m_unweldedNormal; // 0x20
public:
    const hkpCdBody& m_cdBodyA; // 0x30
    const hkpCdBody& m_cdBodyB; // 0x34
};

// 0x00DBC2A0
inline void hkpCdPoint::setUnweldedNormal(const hkVector4f& norm)
{
    m_unweldedNormal = norm;
}
