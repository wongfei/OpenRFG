// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactimpulselimitbreachedlistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7, HK8): the listener for contacts whose impulse limit the solver breached, and the
// info it gets (one element: the solver's breach record, or a single TOI/response contact). Virtuals and layout from
// rfg.pdb. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/solve/hkpsolve.h"

class hkContactPoint;
class hkpConstraintInstance;
class hkpContactPointProperties;

// size 0x10
class hkpContactImpulseLimitBreachedListenerInfo
{
public:
    // size 0x10
    struct SingleImpulseElem
    {
        unsigned short m_type; // 0x00
        unsigned short m_isContact; // 0x02
        hkpConstraintInstance* m_constraintInstance; // 0x04
        hkpContactPointProperties* m_properties; // 0x08
        hkContactPoint* m_contactPoint; // 0x0C
    };

    // size 0x10
    union ListenerData
    {
        hkpImpulseLimitBreachedElem m_solver; // 0x00: m_type 0
        SingleImpulseElem m_single; // 0x00: m_type 1 (collision response) or 2 (TOI)
    };

    void set(hkpConstraintInstance* constraintInstance, hkpContactPointProperties* properties, hkContactPoint* cp, bool toi); // 0x00E03840

protected:
    ListenerData m_data; // 0x00
};

// size 0x4
class hkpContactImpulseLimitBreachedListener
{
public:
    virtual ~hkpContactImpulseLimitBreachedListener() {} // vtbl+0x00
    virtual void contactImpulseLimitBreachedCallback(const hkpContactImpulseLimitBreachedListenerInfo* breachedContacts, int numBreachedContacts) = 0; // vtbl+0x04
};
