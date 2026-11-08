// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/uuid/hkuuid.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG), from rfg.exe: the random generator singleton and the hkUuid
// members the version patches use (hkxNode_4_to_5). Not ported: setFromString, isValidUuidString, toString, the
// combinations and hkUuidObject (no caller in the port).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/uuid/hkuuid.h"

#include <intrin.h>
#include <time.h>

// hkStopwatch::getTickCounter (0x00D18460) jumps to hkSystemClock::getTickCounter (0x00D1A830): cpuid, then rdtsc.
// Neither class is in the port; written out here.
static hkUint64 getTickCounter()
{
    int info[4];
    __cpuid(info, 0);
    return __rdtsc();
}

// 0x03A270DC
template<> hkUuidPseudoRandomGenerator* hkSingleton<hkUuidPseudoRandomGenerator>::s_instance = nullptr;

// 0x03A270E0 (dynamic initializer 0x01245214 in rfg.exe)
static const hkUuid s_nilUuid(0, 0, 0, 0);

// 0x00D0A4B0 (:32): seeded from _time64 ^ clock ^ the tick counter's low word
hkUuidPseudoRandomGenerator::hkUuidPseudoRandomGenerator() : m_criticalSection(5000)
{
    const hkUint32 t = hkUint32(_time64(nullptr));
    const hkUint32 c = hkUint32(clock());
    const hkUint64 ticks = getTickCounter();
    m_randomGenerator.initialize(hkUint32(ticks) ^ c ^ t);
}

// 0x00D0A2A0 (:42)
void hkUuidPseudoRandomGenerator::getRand128(hkUint32& outValA, hkUint32& outValB, hkUint32& outValC, hkUint32& outValD)
{
    m_criticalSection.enter();
    outValA = m_randomGenerator.getRand();
    outValB = m_randomGenerator.getRand();
    outValC = m_randomGenerator.getRand();
    outValD = m_randomGenerator.getRand();
    m_criticalSection.leave();
}

// 0x00D0A300 (:54)
void hkUuidPseudoRandomGenerator::setSeed(hkUint32 newSeed)
{
    m_criticalSection.enter();
    m_randomGenerator.initialize(newSeed);
    m_criticalSection.leave();
}

// 0x00D0A510 (:59)
static hkReferencedObject* hkUuidPseudoRandomGeneratorcreate()
{
    return new hkUuidPseudoRandomGenerator();
}

// 0x016F4704 (dynamic initializer 0x01245210)
static hkSingletonInitNode hkSingletonRegisterhkUuidPseudoRandomGenerator("hkUuidPseudoRandomGenerator",
    hkUuidPseudoRandomGeneratorcreate, (void**)&hkSingleton<hkUuidPseudoRandomGenerator>::s_instance);

// 0x00D0A330 (:68)
const hkUuid& hkUuid::getNil()
{
    return s_nilUuid;
}

// 0x00D0A340 (:73)
void hkUuid::setNil()
{
    m_data[0] = 0;
    m_data[1] = 0;
    m_data[2] = 0;
    m_data[3] = 0;
}

// 0x00D0AA00 (:127)
hkUuid hkUuid::random()
{
    hkUuid u;
    u.setRandom();
    return u;
}

// 0x00D0A430 (:161)
hkUuid::hkUuid(hkFinishLoadedObjectFlag flag)
{
}

static inline hkUint32 byteSwap32(hkUint32 v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

// 0x00D0A560 (:164): an RFC 4122 version 4 UUID, stored in rfg.exe's byte order: word 1 has its halves swapped and the
// version nibble 4 on top, word 2 is byte-swapped with the variant bits 10 in its low byte, word 3 is byte-swapped.
void hkUuid::setRandom()
{
    hkUint32 a, b, c, d;
    hkUuidPseudoRandomGenerator::getInstance().getRand128(a, b, c, d);
    m_data[0] = a;
    m_data[1] = (((b << 16) | (b >> 16)) & 0x0FFFFFFF) | 0x40000000;
    m_data[2] = (byteSwap32(c) & 0xFFFFFF3F) | 0x80;
    m_data[3] = byteSwap32(d);
}
