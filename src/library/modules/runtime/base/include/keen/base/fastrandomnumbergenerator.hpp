// Original: library/modules/runtime/base/include/keen/base/fastrandomnumbergenerator.hpp
// Implementation: library/modules/runtime/base/sources/global/fastrandomnumbergenerator.cpp
//
// FastRandomNumberGenerator and FasterRandomNumberGenerator, small linear generators.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x4
    class FastRandomNumberGenerator
    {
    public:
        FastRandomNumberGenerator(); // 0x00BF74C0 (:7)
        void initFromSeed(uint32 value); // 0x00BEF9D0 (:12)
        uint32 getNext(); // 0x00BEFA00 (:26)
        uint32 getRange() const; // 0x00BEF9F0 (:21)
        uint32 getRandomValue(uint32 range); // 0x00BF74D0 (:63)
        float getRandomValue(float min, float max); // 0x00BEFA80 (:54)
        uint32 getRandomIndex32(uint32);
        uint32 getSaveValue() const; // 0x00BEFAD0 (:73)
        void restoreFromSaveValue(uint32 value); // 0x00BEFAE0 (:78)

    private:
        uint32 m_currentValue; // 0x00
    };

    // size 0x8
    class FasterRandomNumberGenerator
    {
    public:
        FasterRandomNumberGenerator(); // 0x00BFCA90 (:83)
        void initFromSeed(uint64 value); // 0x00BF7560 (:88)
        uint32 getRange() const; // 0x00BEFAF0 (:104)
        uint32 getNext();
        float getRandomValue(float, float);
        uint64 getSaveValue() const; // 0x00BEFB00 (:109)
        void restoreFromSaveValue(uint64 value); // 0x00BEFB10 (:115)

    private:
        uint32 m_w; // 0x00
        uint32 m_z; // 0x04
    };
}
