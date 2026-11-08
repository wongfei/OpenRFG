// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/uuid/hkuuid.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/uuid/hkuuid.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG): hkUuid and its random generator singleton, declarations as the
// 2014.1 SDK's hkUuid.h (inline members from hkUuid.inl), layouts from rfg.pdb. Only what the version patches use is
// implemented (the string conversions and combinations are not).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkFinishLoadedObjectFlag;

// size 0x10
class hkUuid
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkUuid);

    // 0x00D0A540 (:152; folded with CRect::CRect in rfg.exe)
    hkUuid(hkUint32 d0 = 0, hkUint32 d1 = 0, hkUint32 d2 = 0, hkUint32 d3 = 0)
    {
        m_data[0] = d0;
        m_data[1] = d1;
        m_data[2] = d2;
        m_data[3] = d3;
    }
    hkUuid(hkFinishLoadedObjectFlag flag); // 0x00D0A430
    static const hkUuid& getNil(); // 0x00D0A330
    static hkUuid random(); // 0x00D0AA00
    void setNil(); // 0x00D0A340
    void setRandom(); // 0x00D0A560

    bool operator==(const hkUuid& rhs) const
    {
        return m_data[0] == rhs.m_data[0] && m_data[1] == rhs.m_data[1] && m_data[2] == rhs.m_data[2] &&
               m_data[3] == rhs.m_data[3];
    }
    bool operator!=(const hkUuid& rhs) const { return !(*this == rhs); }

    template<int I> hkUint32 getData() const { return m_data[I]; } // <0> 0x00F8FFF0, <3> 0x00F90020 (hkuuid.inl:55)
    hkUint32 getData(int i) const { return m_data[i]; }
    template<int I> void setData(hkUint32 v) { m_data[I] = v; }
    void setData(int i, hkUint32 v) { m_data[i] = v; }

protected:
    hkUint32 m_data[4]; // 0x00
};

// size 0x28: the process-wide generator of hkUuid::setRandom (seeded from the time, the clock and the tick counter)
class hkUuidPseudoRandomGenerator : public hkReferencedObject, public hkSingleton<hkUuidPseudoRandomGenerator>
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00D0AB60, delete(void* p, unsigned int nbytes) 0x00D0ABA0
    hkUuidPseudoRandomGenerator(); // 0x00D0A4B0
    virtual ~hkUuidPseudoRandomGenerator() {} // vtbl+0x00 0x00D0AB80 (implicit)
    void getRand128(hkUint32& outValA, hkUint32& outValB, hkUint32& outValC, hkUint32& outValD); // 0x00D0A2A0
    void setSeed(hkUint32 newSeed); // 0x00D0A300

protected:
    hkMT19937RandomGenerator m_randomGenerator; // 0x08
    hkCriticalSection m_criticalSection; // 0x10
};
