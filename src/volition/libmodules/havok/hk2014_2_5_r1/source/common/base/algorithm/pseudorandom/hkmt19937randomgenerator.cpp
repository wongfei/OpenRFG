// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG), from rfg.exe: the MT19937 generator (624 state words, the
// standard constants; the tempering masks are written as rfg.exe applies them before the shift).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

// 0x00D181C0 (:34)
hkMT19937RandomGenerator::hkMT19937RandomGenerator() : m_state(nullptr)
{
}

// 0x00D183A0 (:37)
hkMT19937RandomGenerator::~hkMT19937RandomGenerator()
{
    if (m_state != nullptr)
    {
        hkMemoryRouter::getInstance().heap().blockFree(m_state, 624 * sizeof(hkUint32));
        m_state = nullptr;
    }
}

// 0x00D183E0 (:47)
void hkMT19937RandomGenerator::initialize(hkUint32 seed)
{
    if (m_state == nullptr)
    {
        m_state = static_cast<hkUint32*>(hkMemoryRouter::getInstance().heap().blockAlloc(624 * sizeof(hkUint32)));
    }
    m_state[0] = seed;
    for (hkUint32 i = 1; i < 624; ++i)
    {
        m_state[i] = (m_state[i - 1] ^ (m_state[i - 1] >> 30)) * 0x6C078965 + i;
    }
    m_index = 0;
}

// 0x00D18350 (:64)
hkUint32 hkMT19937RandomGenerator::getRand()
{
    if (m_index == 0)
    {
        generateNumbers();
    }
    hkUint32 y = m_state[m_index];
    y ^= y >> 11;
    y ^= (y & 0xFF3A58AD) << 7; // (y << 7) & 0x9D2C5680
    y ^= (y & 0xFFFFDF8C) << 15; // (y << 15) & 0xEFC60000
    m_index = hkUint16((m_index + 1) % 624);
    return y ^ (y >> 18);
}

// 0x00D181D0 (:84)
void hkMT19937RandomGenerator::generateNumbers()
{
    for (hkUint32 i = 0; i < 624; ++i)
    {
        const hkUint32 y = (m_state[i] & 0x80000000) | (m_state[(i + 1) % 624] & 0x7FFFFFFF);
        m_state[i] = m_state[(i + 397) % 624] ^ (y >> 1);
        if ((y & 1) != 0)
        {
            m_state[i] ^= 0x9908B0DF;
        }
    }
}
