// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h
//
// Havok 2014.2.5 (havok_impl.md HK8): the atom of a contact constraint, from rfg.pdb and the Havok 2014.1 header.
// It is allocated by hkpSimpleContactConstraintAtomUtil with room for m_numReservedContactPoints points: the atom
// (0x30), then the hkContactPoints (0x20 each), then the contact point properties (m_contactPointPropertiesStriding
// bytes each: hkpContactPointProperties, 0x14, plus 4 bytes per extended user data of either body).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/atom/hkpconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianschema.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.h"

class hkpContactPointProperties;

// size 0x1: a contact point properties record inside the atom (its size is the atom's striding)
class hkpContactPointPropertiesStream
{
public:
    const hkpContactPointProperties* asProperties() const { return reinterpret_cast<const hkpContactPointProperties*>(this); }
    hkpContactPointProperties* asProperties() { return reinterpret_cast<hkpContactPointProperties*>(this); } // 0x00DFF330
};

// size 0x30
struct hkpSimpleContactConstraintAtom : public hkpConstraintAtom
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleContactConstraintAtom); // new(unsigned int n, void* p) 0x00DEE2E0
    static const hkClass& staticClass(); // 0x00DEE2F0
    // 0x00E23870
    hkpSimpleContactConstraintAtom(hkFinishLoadedObjectFlag f) : hkpConstraintAtom(f), m_info(f)
    {
        if (f.m_finishing)
        {
            afterReflectNew();
        }
    }
    // 0x00E23860: the striding is not serialized; hkpContactPointProperties (5 words) plus the user datas
    void afterReflectNew()
    {
        m_contactPointPropertiesStriding = (unsigned char)((m_numUserDatasForBodyA + m_numUserDatasForBodyB + 5) * 4);
    }
    hkpSimpleContactConstraintAtom() : hkpConstraintAtom(TYPE_CONTACT) {} // 0x00F2EB20

    hkContactPoint* getContactPoints() const { return const_cast<hkContactPoint*>(reinterpret_cast<const hkContactPoint*>(this + 1)); } // 0x00DFF340
    int getContactPointPropertiesStriding() const { return m_contactPointPropertiesStriding; } // 0x00DFF350
    // 0x00E1B280
    hkpContactPointPropertiesStream* getContactPointPropertiesStream() const
    {
        return const_cast<hkpContactPointPropertiesStream*>(reinterpret_cast<const hkpContactPointPropertiesStream*>(getContactPoints() + m_numReservedContactPoints));
    }
    // 0x00DFFC50: the index and the striding are multiplied as 16-bit values (HK_HINT_SIZE16)
    hkpContactPointPropertiesStream* getContactPointPropertiesStream(int i) const
    {
        return reinterpret_cast<hkpContactPointPropertiesStream*>(reinterpret_cast<char*>(getContactPointPropertiesStream()) + int(short(i)) * int(short(getContactPointPropertiesStriding())));
    }
    int numSolverResults() const { return m_numContactPoints + 3; }
    void addToConstraintInfo(hkpConstraintInfo& infoOut) const; // 0x00E30030

    unsigned short m_sizeOfAllAtoms; // 0x02
    unsigned short m_numContactPoints; // 0x04
    unsigned short m_numReservedContactPoints; // 0x06
    unsigned char m_numUserDatasForBodyA; // 0x08
    unsigned char m_numUserDatasForBodyB; // 0x09
    unsigned char m_contactPointPropertiesStriding; // 0x0A
    unsigned short m_maxNumContactPoints; // 0x0C
    hkpSimpleContactConstraintDataInfo m_info; // 0x10
};
