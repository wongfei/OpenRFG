// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasedefs.h
//
// Havok 2014.2.5 (hand-written from the PDB instantiations, havok_impl.md HK0): hkDebugOnlyMember, a member Havok keeps in
// every build of this SDK (hkDebugOnlyMember<int> is 4 bytes in rfg.pdb).
#pragma once

// size sizeof(T)
template<typename T>
struct hkDebugOnlyMember
{
    hkDebugOnlyMember() {} // <int> 0x00D43F90

    T member; // 0x00
};
