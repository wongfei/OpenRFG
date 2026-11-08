// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/util/hkmemutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

namespace hkMemUtil
{
    // template instance: void memCpyOneAligned<48,16>(void* dst, const void* src); // 0x008138E0 (hkmemutil.inl:15)
    // template instance: void memCpy<16>(void* dst, const void* src, int n); // 0x00813910 (hkmemutil.inl:27)
    // template instance: void memCpyOneAligned<2,2>(void* dst, const void* src); // 0x00D8B180
    // template instance: void memCpyOneAligned<16,16>(void* dst, const void* src); // 0x00D98F90
    // template instance: void memCpyOneAligned<12,4>(void* dst, const void* src); // 0x00E1F000
    // template instance: void memCpyOneAligned<16,8>(void* dst, const void* src); // 0x00F21E40
    // template instance: void memCpyOneAligned<32,16>(void* dst, const void* src); // 0x00DB9350
    // template instance: void memCpyOneAligned<112,16>(void* dst, const void* src); // 0x00D95640
    // template instance: void memCpyOneAligned<64,16>(void* dst, const void* src); // 0x00D956C0
    // template instance: void memCpy<8>(void* dst, const void* src, int n); // 0x00DF4A00
    void memCpy(void* dst, const void* src, int nbytes); // 0x00CF9050
    void memSet(void* dst, const int c, int n); // 0x00CF9060
    void memCpyBackwards(void* dst, const void* src, int nbytes); // 0x00CF9120
    void memMove(void* dst, const void* src, int nbytes); // 0x00CF9210
    // template instance: void memCpyOneAligned<20,4>(void* dst, const void* src); // 0x00D262D0
    // template instance: void memCpyOneAligned<8,4>(void* dst, const void* src); // 0x00D0AD30
    // template instance: void memCpyOneAligned<16,4>(void* dst, const void* src); // 0x00F5ADB0
    // template instance: void memCpyOneAligned<4,4>(void* dst, const void* src); // 0x00CE6790
    // template instance: void memCpy<4>(void* dst, const void* src, int n); // 0x00CFFD70
    // template instance: void memCpy<1>(void* dst, const void* src, int n); // 0x00D159B0
}
