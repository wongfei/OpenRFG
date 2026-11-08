// Original: volition/vlib/code/os/perfmon.h
// Implementation: volition/vlib/code/os/perfmon.cpp
//
// The performance monitor (retail: empty functions only).
#pragma once

class page_pool;

void perfmon_init(page_pool* page_pool_to_use, char* ip_address, void (*pauseCallback)(bool), bool (*isPausedCallback)()); // 0x0058B680 (:1217)
bool perfmon_needs_pause(); // 0x0058B690 (:1271)
void perfmon_show_regs(bool show_em); // 0x0058B6A0 (:1290)
void perfmon_disable_throttle(); // 0x0058B6B0 (:1304)
