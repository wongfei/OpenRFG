// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpointmaterial.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolverresults.h"

struct hkpSimpleContactConstraintAtom;

// size 0x14
class hkpContactPointProperties : public hkpSolverResults, public hkContactPointMaterial
{
public:
    bool wasUsed();
    static int getNumExtendedUserDatas(const hkpSimpleContactConstraintAtom*, int);
    unsigned int* getStartOfExtendedUserData(const hkpSimpleContactConstraintAtom* atom); // 0x00E1E2C0
    unsigned int& getExtendedUserData(const hkpSimpleContactConstraintAtom*, int, int);
    hkpContactPointProperties() {} // 0x00DDCFC0 (the solver results zeroed)

    float m_internalDataA; // 0x10
};
