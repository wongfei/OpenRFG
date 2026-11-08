// Original: volition/rfg/code/interface/ingame/overlay.cpp
//
// Recovered whole: the overlay stack. vint-free.

#include "volition/rfg/code/interface/ingame/overlay.h"
#include "volition/rfg/code/interface/ingame/ovl_base.h"

#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/vlib/code/util/varray.h"

// Magic number: 8 overlays deep.
static farray<const ovl_base*, 8> Overlay_stack; // 0x01665D40

// 0x00888130
ovl_base::ovl_base()
{
}

// 0x00888090
ovl_base::~ovl_base()
{
}

// 0x008AA750
void overlay_system_init()
{
    // ovl_inventory_system_init inlined in rfg.exe (it sets ovl_inventory.cpp's Update_hud_timestamp).
    ovl_inventory_system_init();
}

// 0x008880C0
void overlay_level_init_post()
{
}

// 0x008880D0
void overlay_level_shutdown()
{
}

// 0x008BF390
static bool overlay_stack_remove(const ovl_base* op)
{
    if (Overlay_stack.find(op, false) < 0)
    {
        return false;
    }
    Overlay_stack.remove_matching(op);
    return true;
}

// 0x008BF3D0
// Moves an overlay already on the stack to the top.
static bool overlay_stack_push(const ovl_base* op, bool& already_on_stack)
{
    already_on_stack = overlay_stack_remove(op);
    return Overlay_stack.add(op) >= 0;
}

// 0x008BF460
bool overlay_open(const ovl_base* op)
{
    bool already_on_stack;
    const bool pushed = overlay_stack_push(op, already_on_stack);
    if (pushed)
    {
        if (!already_on_stack)
        {
            op->enter();
            return true;
        }
        // rfg.exe sets ovl_inventory.cpp's Update_available_flag here (whatever the overlay): an open overlay asked to
        // open again refreshes the inventory's available weapons.
        ovl_inventory_update_avail_weapons();
    }
    return pushed;
}

// 0x008AA760
bool overlay_is_open()
{
    return Overlay_stack.size() > 0;
}

// 0x008AA770
bool overlay_is_open(const ovl_base* op)
{
    return Overlay_stack.find(op, false) >= 0;
}

// 0x008AA7A0
// Original bug kept: reads elt[-1] with an empty stack.
bool overlay_is_top(const ovl_base* op)
{
    return op == Overlay_stack[Overlay_stack.size() - 1];
}

// 0x008BF4A0
void overlay_close(const ovl_base* op)
{
    if (overlay_stack_remove(op))
    {
        op->exit();
    }
}

// 0x008BF4F0
// From the top down; an overlay whose process returns false is closed (taken off the stack, then exited).
void overlay_process()
{
    for (int i = Overlay_stack.size() - 1; i >= 0; i--)
    {
        const ovl_base* op = Overlay_stack[i];
        if (!op->process())
        {
            overlay_close(op);
        }
    }
}

// 0x008AA7C0
// From the bottom up.
void overlay_render()
{
    for (int i = 0; i < Overlay_stack.size(); i++)
    {
        Overlay_stack[i]->render();
    }
}
