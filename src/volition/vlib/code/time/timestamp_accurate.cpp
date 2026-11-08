// Original: volition/vlib/code/time/timestamp_accurate.cpp
//
// Recovery stage: internal functions and file-static globals only (rfg.pdb, no public symbol).
// Implementation pending; this file is not compiled yet. vlib_unity.obj is a unity build, so file statics are
// placed by the functions that use them. Initial values are read from the binary's .data section.

#include "volition/vlib/code/time/timestamp_accurate.h"

static int Timestamp_accurate_pausable_ticker = 1; // 0x01519710
static int Timestamp_accurate_last_time = 1; // 0x01519714
