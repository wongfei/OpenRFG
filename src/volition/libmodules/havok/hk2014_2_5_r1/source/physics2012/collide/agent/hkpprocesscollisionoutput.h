// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisiondata.h"

struct hkpAgentEntry;
class hkpConstraintOwner;
struct hkpProcessCdPoint;

// size 0x3090
struct hkpProcessCollisionOutput : public hkpProcessCollisionData
{
    // size 0xC
    struct ContactRef
    {
        hkpProcessCdPoint* m_contactPoint; // 0x00
        hkpAgentEntry* m_agentEntry; // 0x04
        void* m_agentData; // 0x08
    };

    // size 0x1008
    struct PotentialInfo
    {
        // 0x00DCF640
        void reset()
        {
            m_firstFreePotentialContact = &m_potentialContacts[0];
            m_firstFreeRepresentativeContact = &m_representativeContacts[0];
        }

        ContactRef* m_firstFreePotentialContact; // 0x00
        hkpProcessCdPoint** m_firstFreeRepresentativeContact; // 0x04
        hkpProcessCdPoint* m_representativeContacts[256]; // 0x08
        ContactRef m_potentialContacts[256]; // 0x408
    };

    hkpProcessCollisionOutput(hkpConstraintOwner* owner) : hkpProcessCollisionData(owner) {} // 0x00DDD160

    // 0x00DDCFD0
    void reset()
    {
        m_firstFreeContactPoint = &m_contactPoints[0];
        m_toi.m_time = HK_REAL_MAX;
        m_potentialContacts = nullptr;
    }

    // Original bug kept: nothing bounds-checks the output's 256 contact points or the 256 potential contacts.
    hkpProcessCdPoint* reserveContactPoints(int numberOfPoints = 1) { return m_firstFreeContactPoint; } // 0x00D90CD0
    void commitContactPoints(int numberOfPoints = 1) { m_firstFreeContactPoint = m_firstFreeContactPoint.val() + numberOfPoints; } // 0x00D90CE0
    void uncommitContactPoints(int numberOfPoints = 1) { m_firstFreeContactPoint = m_firstFreeContactPoint.val() - numberOfPoints; } // 0x00DC4DC0
    void abortContactPoints(int numberOfPoints = 1) {} // 0x00D90CB0

    hkPadSpu<PotentialInfo*> m_potentialContacts; // 0x3080
};
