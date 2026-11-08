// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentityaabbutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


struct hkpCollisionInput;
class hkpEntity;

// size 0x1
class hkpEntityAabbUtil
{
public:
    static void entityBatchRecalcAabb(const hkpCollisionInput* collisionInput, hkpEntity* const * entityBatch, int numEntities); // 0x00E13720
    static void entityBatchInvalidateAabb(hkpEntity* const * entityBatch, int numEntities); // 0x00E136F0

};
