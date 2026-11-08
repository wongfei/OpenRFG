// Original: library/modules/runtime/base/include/keen/base/randomnumbergenerator.hpp
// Implementation: library/modules/runtime/base/sources/global/randomnumbergenerator.cpp
//
// RandomNumberGenerator, a Mersenne twister.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x9C4
    class RandomNumberGenerator
    {
    public:
        enum MersenneTwisterFlags
        {
            W = 32,
            N = 624,
            M = 397,
            R = 31,
            A = -1727483681,
            U = 11,
            S = 7,
            B = -1658038656,
            T = 15,
            C = -272236544,
            L = 18,
            LLMASK = 0x7FFFFFFF,
            UMASK = -2147483648,
        };

        RandomNumberGenerator(); // 0x00C04190 (:9)
        void initFromSeed(uint32 value); // 0x00C00240 (:15)
        uint32 getNext(); // 0x00C00250 (:20)
        uint32 getRange() const; // 0x00BF1340 (:25)
        uint32 getRandomValue(uint32 range); // 0x00C002B0 (:38)
        float getRandomValue(float min, float max); // 0x00C00260 (:30)
        uint32 getRandomIndex32(uint32 range); // 0x009702B0 (.h:41)
        uint32 getSaveValue(); // 0x00C00340 (:48)
        void restoreFromSaveValue(uint32 value); // 0x00BFA030 (:57)
    private:
        void initMersenneTwister(uint32 seed); // 0x00BF1350 (:62)
        uint32 generateRandWithMersenneTwister(); // 0x00BFA040 (:76)
        void regenerate(int index); // 0x00BF1390 (:97)

        int m_index; // 0x00
        uint32 m_mt[624]; // 0x04
    };
}
