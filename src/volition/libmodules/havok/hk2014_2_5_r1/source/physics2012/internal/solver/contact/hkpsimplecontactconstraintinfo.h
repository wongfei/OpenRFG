// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). The info classes from the PDB and Havok 2014.1
// (HK8). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpConstraintQueryIn;
class hkpConstraintQueryOut;
struct hkpSimpleContactConstraintAtom;

// size 0x20: the contact constraint's friction state (what the solver export keeps between steps)
class alignas(16) hkpSimpleContactConstraintDataInfo
{
public:
    enum
    {
        HK_FLAG_OK = 0,
        HK_FLAG_POINT_REMOVED = 1,
        HK_FLAG_AREA_CHANGED = 4,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleContactConstraintDataInfo); // new(unsigned int n, void* p) 0x00E33D40
    static const hkClass& staticClass(); // 0x00E33D10
    // 0x00E2F4A0
    void init()
    {
        m_flags = HK_FLAG_OK;
        m_biNormalAxis = 3;
        m_contactRadius = 0.0f;
        m_internalData1.setZero();
        m_rhsRolling[0].setZero();
        m_rhsRolling[1].setZero();
        m_rollingFrictionMultiplier.setZero();
        for (int i = 0; i < 4; ++i)
        {
            m_data[i] = 0.0f;
        }
    }
    hkpSimpleContactConstraintDataInfo(hkFinishLoadedObjectFlag f) {} // 0x00E23850
    hkpSimpleContactConstraintDataInfo() { init(); } // 0x00F2D1F0

    static const hkInternalClassMember Members[];
    unsigned short m_flags; // 0x00
    unsigned short m_biNormalAxis; // 0x02
    hkHalf m_rollingFrictionMultiplier; // 0x04
    hkHalf m_internalData1; // 0x06
    hkHalf m_rhsRolling[2]; // 0x08
    float m_contactRadius; // 0x0C
    float m_data[4]; // 0x10
};

// size 0x20: the same storage as the solver sees it
class alignas(16) hkpSimpleContactConstraintDataInfoInternal
{
public:
    enum
    {
        HK_FLAG_OK = 0,
        HK_FLAG_POINT_REMOVED = 1,
        HK_FLAG_AREA_CHANGED = 4,
    };

    unsigned short m_flags; // 0x00
    unsigned short m_biNormalAxis; // 0x02: a guess for the friction bi-normal, projected every step
    hkHalf m_rollingFrictionMultiplier; // 0x04
    hkHalf m_rollingImpulseSqr; // 0x06
    hkHalf m_rhsRolling[2]; // 0x08
    float m_contactRadius; // 0x0C
    float m_impulseSqr; // 0x10: linear, linear and angular friction impulses
    float m_rhsAng; // 0x14
    float m_rhsLin[2]; // 0x18
};

void hkSimpleContactConstraintDataBuildJacobian(hkpSimpleContactConstraintAtom* atom, const hkpConstraintQueryIn& in, hkBool writeHeaderSchemas, hkpConstraintQueryOut& out); // 0x00E9A6F0
void hkSimpleContactConstraintDataAddALittleSurfaceViscosity(hkpSimpleContactConstraintDataInfo* infoExt); // 0x00E9A610
