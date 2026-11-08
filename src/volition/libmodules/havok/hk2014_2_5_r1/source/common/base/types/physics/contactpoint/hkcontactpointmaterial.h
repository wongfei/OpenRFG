// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpointmaterial.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkufloat8.h"

class hkClass;
struct hkInternalClassMember;

// size 0x8
class hkContactPointMaterial
{
public:
    enum FlagEnum
    {
        CONTACT_IS_NEW = 1,
        CONTACT_USES_SOLVER_PATH2 = 2,
        CONTACT_BREAKOFF_OBJECT_ID_SMALLER = 4,
        CONTACT_IS_DISABLED = 8,
    };

    static const hkClass& staticClass(); // 0x00CEB740
    float getFriction() const; // 0x00E03980
    void setFriction(float r); // 0x00E1A430
    float getRestitution() const; // 0x00E03820
    void setRestitution(float r); // 0x00E1B260
    unsigned long getUserData() const;
    void setUserData(unsigned long data); // 0x00E1A440
    unsigned int isPotential();
    void setMaxImpulsePerStep(hkUFloat8);
    float getMaxImpulsePerStep(); // 0x00E30130
    void reset(); // 0x00E19790
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkContactPointMaterial);
public:
    hkContactPointMaterial(); // 0x00DDCC40

    static const hkInternalClassMember Members[]; // 0x0134B590
public:
    unsigned long m_userData; // 0x00
    hkUFloat8 m_friction; // 0x04
    unsigned char m_restitution; // 0x05
public:
    hkUFloat8 m_maxImpulse; // 0x06
    unsigned char m_flags; // 0x07
};
