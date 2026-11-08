// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/mesh/hkpmeshmaterial.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the base of mesh materials (games put their own data after the filter info and
// give the material striding). As in Havok 2014.1 (hkpMeshMaterial.h); hand-written (not in the hdrgen specs).
#pragma once

// size 0x4
class hkpMeshMaterial
{
public:
    unsigned int m_filterInfo; // 0x00
};
