// Original: volition/render_lib/code/rl_rect.h
// Implementation: volition/render_lib/code/rl_rect.cpp
//
// rl_rect, an integer screen rectangle (viewports, scissor regions).
#pragma once

// size 0x10
class rl_rect
{
public:
    rl_rect(int left, int top, int right, int bottom); // 0x004B6730 (.h:42)
    rl_rect(); // 0x004B6720 (.h:38)
    ~rl_rect(); // 0x004B6750 (.h:50)
    const bool operator==(const rl_rect&) const;
    const bool operator!=(const rl_rect&) const;

    int m_left; // 0x00
    int m_top; // 0x04
    int m_right; // 0x08
    int m_bottom; // 0x0C
};

// ---- inline members

// 0x004B6730 (.h:42)
inline rl_rect::rl_rect(int left, int top, int right, int bottom)
    : m_left(left)
    , m_top(top)
    , m_right(right)
    , m_bottom(bottom)
{
}

// 0x004B6720 (.h:38)
// The rectangle is left uninitialized.
inline rl_rect::rl_rect()
{
}

// 0x004B6750 (.h:50)
inline rl_rect::~rl_rect()
{
}
