// Original: volition/vlib/code/os/atomic.h
//
// 32-bit interlocked helpers, all defined inline in this header (PDB line info). exchange/add return the previous
// value and increment returns the new one (decompiled); decrement/sub/compare_exchange are assumed to match.
#pragma once

unsigned int atomic_exchange32(volatile unsigned int* dest, unsigned int exchange); // 0x004BA110 (.h:23)
unsigned int atomic_compare_exchange32(volatile unsigned int* dest, unsigned int exchange, unsigned int compare); // 0x004BA120 (.h:31)
unsigned int atomic_increment32(volatile unsigned int* dest); // 0x004BA140 (.h:55)
unsigned int atomic_decrement32(volatile unsigned int* dest); // 0x004BA160 (.h:71)
unsigned int atomic_add32(volatile unsigned int* dest, unsigned int increment); // 0x004BA170 (.h:87)
unsigned int atomic_sub32(volatile unsigned int* dest, unsigned int decrement); // 0x00566B30 (.h:95)
