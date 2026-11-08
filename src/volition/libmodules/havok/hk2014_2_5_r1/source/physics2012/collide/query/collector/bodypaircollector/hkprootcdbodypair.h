// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkprootcdbodypair.h
//
// Seam only (Havok 2014.2.5).
#pragma once


class hkpCollidable;

// size 0x10
struct hkpRootCdBodyPair
{
    const hkpCollidable* m_rootCollidableA; // 0x00
    unsigned int m_shapeKeyA; // 0x04
    const hkpCollidable* m_rootCollidableB; // 0x08
    unsigned int m_shapeKeyB; // 0x0C
};
