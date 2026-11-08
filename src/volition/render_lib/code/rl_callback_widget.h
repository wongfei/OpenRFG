// Original: volition/render_lib/code/rl_callback_widget.h
// Implementation: volition/render_lib/code/rl_callback_widget.cpp
//
// rl_callback_widget and rl_callback_widget_list, callbacks attached to render objects (for example on destruction).
#pragma once


class rl_callback_widget;

// size 0x4
class rl_callback_widget_list
{
public:
    rl_callback_widget_list(); // 0x004BA4F0 (.h:32)
    ~rl_callback_widget_list(); // 0x004BA8B0 (:15)
    void add_widget(rl_callback_widget* widget_p); // 0x004BA910 (:26)
    void remove_widget(rl_callback_widget* widget_p); // 0x004BA950 (:38)
    void do_callbacks(); // 0x004BA9A0 (:61)
private:
    // Not in the binary: the inlined unlink of the list functions (rl_callback_widget.cpp).
    void unlink(rl_callback_widget* widget_p);

private:
    rl_callback_widget* m_head; // 0x00
};

// size 0xC
class rl_callback_widget
{
    // Access inferred: the list links the widgets through their private links.
    friend class rl_callback_widget_list;
public:
    rl_callback_widget(); // 0x006E13A0 (.h:16)
    virtual ~rl_callback_widget(); // vtbl+0x00 0x006E13C0 (.h:17)
    virtual void on_callback() = 0; // vtbl+0x04

    // 0x00 vfptr
private:
    rl_callback_widget* next; // 0x04
    rl_callback_widget* prev; // 0x08
};
