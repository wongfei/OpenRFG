// Original: volition/render_lib/code/rl_handle.h
// Implementation: volition/render_lib/code/rl_handle.cpp
//
// rl_handle and rl_stamped_handle, 32-bit handles to render_lib objects (index plus stamp).
#pragma once

// size 0x4
class rl_handle
{
public:
    rl_handle(); // 0x004B7460 (.h:31)
    void set(unsigned int handle_i); // 0x004B7470 (.h:33)
    unsigned int get() const; // 0x004B7480 (.h:38)
    void invalidate(); // 0x004B7490 (.h:42)
    bool is_valid() const; // 0x00741CC0 (.h:46)

private:
    static const unsigned int ms_invalid_handle; // 0x012B8060
    unsigned int m_handle_i; // 0x00
};

// size 0x8
class rl_stamped_handle
{
public:
    rl_stamped_handle(); // 0x004B74A0 (.h:56)
    void set(unsigned int handle_i); // 0x004B74B0 (.h:58)
    unsigned int get() const; // 0x004DCA20 (.h:64)
    void invalidate(); // 0x004B74D0 (.h:69)
    bool is_valid() const; // 0x004B74E0 (.h:74)
    static void update_stamp();

private:
    static const unsigned int ms_invalid_handle; // 0x012B8064
    static int ms_current_stamp; // 0x01B7BCF8
    unsigned int m_handle_i; // 0x00
    int m_stamp; // 0x04
};

// ---- inline members

// 0x004B7460 (.h:31)
inline rl_handle::rl_handle()
    : m_handle_i(0xFFFFFFFF)
{
}

// 0x004B7470 (.h:33)
inline void rl_handle::set(unsigned int handle_i)
{
    m_handle_i = handle_i;
}

// 0x004B7480 (.h:38)
inline unsigned int rl_handle::get() const
{
    return m_handle_i;
}

// 0x004B7490 (.h:42)
inline void rl_handle::invalidate()
{
    m_handle_i = 0xFFFFFFFF;
}

// 0x00741CC0 (.h:46)
inline bool rl_handle::is_valid() const
{
    return m_handle_i != 0xFFFFFFFF;
}

// 0x004B74A0 (.h:56)
inline rl_stamped_handle::rl_stamped_handle()
    : m_handle_i(0xFFFFFFFF)
    , m_stamp(-1)
{
}

// 0x004B74B0 (.h:58)
inline void rl_stamped_handle::set(unsigned int handle_i)
{
    m_handle_i = handle_i;
    m_stamp = ms_current_stamp;
}

// 0x004DCA20 (.h:64)
// A handle from another stamp (see update_stamp) reads as invalid.
inline unsigned int rl_stamped_handle::get() const
{
    if (m_handle_i == 0xFFFFFFFF || m_stamp != ms_current_stamp)
    {
        return 0xFFFFFFFF;
    }
    return m_handle_i;
}

// 0x004B74D0 (.h:69)
inline void rl_stamped_handle::invalidate()
{
    m_handle_i = 0xFFFFFFFF;
    m_stamp = -1;
}

// 0x004B74E0 (.h:74)
inline bool rl_stamped_handle::is_valid() const
{
    return m_handle_i != 0xFFFFFFFF && m_stamp == ms_current_stamp;
}
