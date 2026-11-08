// Original: volition/rfg/code/interface/ingame/ovl_base.h
//
// The base of the in-game overlays (process, render, input, open and close).
#pragma once

// size 0x4
class ovl_base
{
public:
    virtual ~ovl_base(); // vtbl+0x00 0x00888090 (.h:39)
    virtual void enter() const = 0; // vtbl+0x04
    virtual void exit() const = 0; // vtbl+0x08
    virtual bool process() const = 0; // vtbl+0x0C
    virtual void render() const = 0; // vtbl+0x10
    ovl_base(); // 0x00888130

    // 0x00 vfptr
};
