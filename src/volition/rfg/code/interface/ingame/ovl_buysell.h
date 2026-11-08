// Original: volition/rfg/code/interface/ingame/ovl_buysell.h
// Implementation: volition/rfg/code/interface/ingame/ovl_buysell.cpp
//
// The buy/sell overlay (single player upgrades).
#pragma once

#include "volition/rfg/code/interface/ingame/ovl_base.h"

// size 0x4
class ovl_buysell : public ovl_base
{
public:
    ovl_buysell(); // 0x008AA7F0 (.h:46)
    virtual ~ovl_buysell(); // vtbl+0x00 0x008880E0 (.h:47)
    virtual void enter() const; // vtbl+0x04 0x00888140 (:71)
    virtual void exit() const; // vtbl+0x08 0x00888150 (:75)
    virtual bool process() const; // vtbl+0x0C 0x00888160 (:79)
    virtual void render() const; // vtbl+0x10 0x00888170 (:84)

private:
    static int refcount; // 0x02C034A4
};
