// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"

class hkClass;
struct hkInternalClassMember;
class hkSimdFloat32;

// size 0x20
class alignas(16) hkContactPoint
{
public:
    static const hkClass& staticClass(); // 0x00CEB760
    const hkVector4f& getPosition() const; // 0x00710860 (hkcontactpoint.inl:10)
    void setPosition(const hkVector4f& position); // 0x0080CAA0 (hkcontactpoint.inl:30)
    const hkVector4f& getNormal() const; // 0x00710870 (hkcontactpoint.inl:15)
    const hkVector4f& getSeparatingNormal() const; // 0x00D79610
    void setSeparatingNormal(const hkVector4f& separatingNormal); // 0x00D79620
    void setSeparatingNormal(const hkVector4f& normal, const hkSimdFloat32& dist); // 0x00D8FA80
    void setSeparatingNormal(const hkVector4f& normal, float dist); // 0x00B62D60 (hkcontactpoint.inl:42)
    void setNormalOnly(const hkVector4f& normal); // 0x00B62D80 (hkcontactpoint.inl:105)
    float getDistance() const; // 0x0080CAB0 (hkcontactpoint.inl:62)
    hkSimdFloat32 getDistanceSimdReal() const; // 0x00D7ADD0
    void setDistance(float d); // 0x00710880 (hkcontactpoint.inl:72)
    void setDistanceSimdReal(const hkSimdFloat32& newValue); // 0x0080CAC0 (hkcontactpoint.inl:80)
    void set(const hkVector4f& position, const hkVector4f& normal, float dist); // 0x00F82DF0
    void setPositionNormalAndDistance(const hkVector4f& position, const hkVector4f& normal, const hkSimdFloat32& dist); // 0x00DD1B70
    void setFlipped(const hkContactPoint& point); // 0x00D79740
    void flip(); // 0x00DCF8C0
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkContactPoint);
public:
    static const hkInternalClassMember Members[]; // 0x0134B64C
public:
    hkVector4f m_position; // 0x00
    hkVector4f m_separatingNormal; // 0x10
};

// size 0x30
class hkContactPointPropertiesWithExtendedUserData16 : public hkpContactPointProperties
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkContactPointPropertiesWithExtendedUserData16);
    void set(const hkContactPointPropertiesWithExtendedUserData16& other); // 0x00E87DF0
    hkContactPointPropertiesWithExtendedUserData16() {} // 0x00DDD0E0 (the solver results zeroed)

    unsigned int m_extendedUserDatas[7]; // 0x14
};
