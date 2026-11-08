// Original: volition/render_lib/code/rl_handle.cpp
//
// Partial: the statics of rl_handle and rl_stamped_handle (the members are inline in rl_handle.h).

#include "volition/render_lib/code/rl_handle.h"

const unsigned int rl_handle::ms_invalid_handle = 0xFFFFFFFF; // 0x012B8060
const unsigned int rl_stamped_handle::ms_invalid_handle = 0xFFFFFFFF; // 0x012B8064
int rl_stamped_handle::ms_current_stamp = 0; // 0x01B7BCF8
