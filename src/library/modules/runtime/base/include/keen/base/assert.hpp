// Original: library/modules/runtime/base/include/keen/base/assert.hpp
//
// Assertion support: haltProgram and rangecheck_cast<To, From> (a checked narrowing cast; its instantiations are
// listed as comments).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // template instance: uint8 rangecheck_cast<uint8,enum InputEventType>(InputEventType value); // 0x00C76720 (.h:251)
    // template instance: short rangecheck_cast<short,int>(int value); // 0x004D9180 (.h:251)
    // template instance: uint32 rangecheck_cast<uint32,uint32>(uint32 value); // 0x00968980 (.h:251)
    // template instance: uint16 rangecheck_cast<uint16,uint32>(uint32 value); // 0x00C60BD0 (.h:251)
    // template instance: uint8 rangecheck_cast<uint8,uint32>(uint32 value); // 0x004D83F0 (.h:251)
    // template instance: uint8 rangecheck_cast<uint8,uint8>(uint8 value); // 0x00CD93B0 (.h:251)
    // template instance: int rangecheck_cast<int,int>(int value); // 0x004D98D0 (.h:251)
    // template instance: unsigned long rangecheck_cast<unsigned long,uint32>(uint32 value); // 0x00BF35E0 (.h:251)
    // template instance: int rangecheck_cast<int,sint64>(int64_t value); // 0x00CD93C0 (.h:251)
    // template instance: uint32 rangecheck_cast<uint32,uint64>(uint64_t value); // 0x00CD93D0 (.h:251)
}
