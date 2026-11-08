// Original: volition/vlib/code/os/perfmon.cpp
//
// The performance monitor connection. Retail compiles it out: these four are the only functions of the file left in
// rfg.exe, all empty.

#include "volition/vlib/code/os/perfmon.h"

// 0x0058B680 (:1217)
void perfmon_init(page_pool* page_pool_to_use, char* ip_address, void (*pauseCallback)(bool), bool (*isPausedCallback)())
{
}

// 0x0058B690 (:1271)
bool perfmon_needs_pause()
{
    return false;
}

// 0x0058B6A0 (:1290)
void perfmon_show_regs(bool show_em)
{
}

// 0x0058B6B0 (:1304)
void perfmon_disable_throttle()
{
}
