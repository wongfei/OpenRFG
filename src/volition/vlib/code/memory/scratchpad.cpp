// Original: volition/vlib/code/memory/scratchpad.cpp
//
// Recovery stage: internal functions and file-static globals only (rfg.pdb, no public symbol).
// Implementation pending; this file is not compiled yet.

#include "volition/vlib/code/memory/scratchpad.h"

static unsigned char Generic_scratchpad[16384]; // 0x01CDE558
static unsigned int SP_ptr = (unsigned int)Generic_scratchpad; // 0x01519540, next free byte
static unsigned int SP_stack[16]; // 0x01CD2AD8, SP_ptr before each live allocation
static unsigned int SP_stack_size; // 0x01CE86F0
