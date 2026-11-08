// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldmemoryutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpWorld;

// size 0x1
class hkpWorldMemoryUtil
{
public:
    static void watchHeapMemory(hkpWorld* world); // 0x00E1BB20
    static void checkMemoryForIntegration(hkpWorld* world); // 0x00E1B9A0
    static void tryToRecoverFromMemoryErrors(hkpWorld* world); // 0x00E1BBF0
public:
    static void repeatCollideAndToiHandling(hkpWorld* world); // 0x00E1BAE0

};
