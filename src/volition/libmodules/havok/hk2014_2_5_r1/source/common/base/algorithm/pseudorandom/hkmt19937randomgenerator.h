// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG): the Mersenne Twister of hkUuidPseudoRandomGenerator, layout from
// rfg.pdb.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x8
class hkMT19937RandomGenerator
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMT19937RandomGenerator);
    hkMT19937RandomGenerator(); // 0x00D181C0
    ~hkMT19937RandomGenerator(); // 0x00D183A0
    void initialize(hkUint32 seed); // 0x00D183E0
    hkUint32 getRand(); // 0x00D18350

protected:
    void generateNumbers(); // 0x00D181D0

private:
    hkMT19937RandomGenerator(const hkMT19937RandomGenerator&);
    hkMT19937RandomGenerator& operator=(const hkMT19937RandomGenerator&);

protected:
    hkUint16 m_index; // 0x00
    hkUint32* m_state; // 0x04: 624 words (0x9C0 bytes) on the heap
};
