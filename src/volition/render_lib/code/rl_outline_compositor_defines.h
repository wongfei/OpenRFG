// Original: volition/render_lib/code/rl_outline_compositor_defines.h
//
// rl_outline_state and outline layers. Placement of rl_outline_layer inferred.
#pragma once

#include "volition/render_lib/code/rl_color.h"

enum rl_outline_layer
{
    RLOL_DISABLED = 0,
    RLOL_USE_COLOR0 = 1,
    RLOL_USE_COLOR1 = 2,
};

// size 0x30
struct rl_outline_state
{
    rl_outline_state(); // 0x004BBF90 (.h:41)

    rl_color_float colors[2]; // 0x00
    rl_outline_layer layer[4]; // 0x20
};

// 0x004BBF90 (.h:41)
// Notes (decompiled behaviour, kept as is): only layer[0] and layer[1] are initialized.
inline rl_outline_state::rl_outline_state()
{
    for (int i = 0; i < 2; ++i)
    {
        colors[i].set(0.0f, 0.0f, 0.0f, 1.0f);
        layer[i] = RLOL_DISABLED;
    }
}
