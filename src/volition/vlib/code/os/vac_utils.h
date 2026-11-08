// Original: volition/vlib/code/os/vac_utils.h
// Implementation: volition/vlib/code/os/vac_utils.cpp
//
// Synchronization helpers for Valve Anti-Cheat.
#pragma once

bool vac_sync_start(char* name, int timeout, const char* masq); // 0x0058C5C0 (:157)
bool vac_sync_wait(char* name, int timeout); // 0x0058C5D0 (:206)
