// Original: volition/vlib/code/memory/scratchpad.h
// Implementation: volition/vlib/code/memory/scratchpad.cpp
//
// A 16 KB stack allocator (Generic_scratchpad) for short-lived buffers: allocations are released in reverse order.
#pragma once

void* scratchpad_allocate(unsigned int bytes); // 0x005ABEC0 (:62)
void scratchpad_release(void* data); // 0x00589A50 (:97)
unsigned int scratchpad_bytes_free(); // 0x00589A90 (:125)
