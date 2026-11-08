// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpaccurateinertiatensorcomputer.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h"

struct hkMassProperties;
struct hkResult;
struct hkStridedVertices;

// size 0x1
class hkpAccurateInertiaTensorComputer : public hkpInertiaTensorComputer
{
public:
    static hkResult computeConvexHullMassProperties(const hkStridedVertices& vertices, float r, hkMassProperties& result); // 0x00F20440

};
