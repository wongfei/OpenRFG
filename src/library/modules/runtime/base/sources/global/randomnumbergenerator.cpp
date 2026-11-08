// Original: library/modules/runtime/base/sources/global/randomnumbergenerator.cpp
//
// RandomNumberGenerator: a 32-bit Mersenne twister (MT19937). The state is regenerated one word per draw (not in
// blocks of 624), and the tempering masks are written as the binary has them (pre-shifted), which gives the standard
// MT19937 output.

#include "keen/base/randomnumbergenerator.hpp"

namespace keen
{
    // 0x00C04190 (:9)
    RandomNumberGenerator::RandomNumberGenerator()
    {
        m_index = 0;
        initMersenneTwister(5489); // magic number: the standard MT19937 default seed (0x1571)
    }

    // 0x00C00240 (:15)
    void RandomNumberGenerator::initFromSeed(uint32 value)
    {
        initMersenneTwister(value);
    }

    // 0x00C00250 (:20)
    uint32 RandomNumberGenerator::getNext()
    {
        return generateRandWithMersenneTwister();
    }

    // 0x00BF1340 (:25)
    uint32 RandomNumberGenerator::getRange() const
    {
        return 0xFFFFFFFFu;
    }

    // 0x00C00260 (:30)
    float RandomNumberGenerator::getRandomValue(float min, float max)
    {
        const float value = (float)generateRandWithMersenneTwister();
        return min + (max - min) * value * 2.3283064e-10f; // 2^-32
    }

    // 0x00C002B0 (:38): the draw scaled by 2^-32 (in double, rounded to float), times the range in float, truncated;
    // clamped to range - 1 (the float product can round up to the range)
    uint32 RandomNumberGenerator::getRandomValue(uint32 range)
    {
        const float unit = (float)((double)generateRandWithMersenneTwister() * (1.0 / 4294967296.0));
        const float span = (float)range - 0.0f;
        const float scaled = span * unit + 0.0f;
        uint32 value = (uint32)(sint64)scaled;
        if (value > range - 1u)
        {
            value = range - 1u;
        }
        return value;
    }

    // 0x009702B0 (.h:41): the same as getRandomValue(uint32), inlined into its callers
    uint32 RandomNumberGenerator::getRandomIndex32(uint32 range)
    {
        return getRandomValue(range);
    }

    // 0x00C00340 (:48): draws the save value and reseeds from it, so restoring it continues the same sequence
    uint32 RandomNumberGenerator::getSaveValue()
    {
        const uint32 value = generateRandWithMersenneTwister();
        initMersenneTwister(value);
        return value;
    }

    // 0x00BFA030 (:57)
    void RandomNumberGenerator::restoreFromSaveValue(uint32 value)
    {
        initMersenneTwister(value);
    }

    // 0x00BF1350 (:62)
    void RandomNumberGenerator::initMersenneTwister(uint32 seed)
    {
        m_mt[0] = seed;
        for (uint32 i = 1; i < N; ++i)
        {
            m_mt[i] = (m_mt[i - 1] ^ (m_mt[i - 1] >> 30)) * 0x6C078965u + i; // magic number: MT19937 init multiplier
        }
        m_index = 0;
    }

    // 0x00BFA040 (:76)
    uint32 RandomNumberGenerator::generateRandWithMersenneTwister()
    {
        if (m_index >= N)
        {
            m_index = 0;
        }
        const int index = m_index;
        uint32 y = m_mt[index];
        m_index = index + 1;
        regenerate(index);
        y ^= y >> U;
        y ^= (y & 0xFF3A58ADu) << S; // (y << 7) & 0x9D2C5680
        y ^= (y & 0xFFFFDF8Cu) << T; // (y << 15) & 0xEFC60000
        return y ^ (y >> L);
    }

    // 0x00BF1390 (:97)
    void RandomNumberGenerator::regenerate(int index)
    {
        const uint32 y = (m_mt[index] & (uint32)UMASK) | (m_mt[(index + 1) % N] & LLMASK);
        m_mt[index] = m_mt[(index + M) % N] ^ (y >> 1) ^ ((y & 1u) != 0 ? 0x9908B0DFu : 0u); // magic number: MT19937 matrix A
    }
}
