// Original: volition/rfg/code/interface/ingame/ovl_inventory.h
// Implementation: volition/rfg/code/interface/ingame/ovl_inventory.cpp
//
// The inventory overlay: weapon switching (radial and the number keys) and the weapon slots.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/ingame/ovl_base.h"

struct xml_element;

// size 0x4
class ovl_inventory : public ovl_base
{
public:
    ovl_inventory(); // 0x00888180 (:51)
    virtual ~ovl_inventory(); // vtbl+0x00 0x00888190 (:52)
    virtual void enter() const; // vtbl+0x04 0x008AAA00 (:375)
    virtual void exit() const; // vtbl+0x08 0x008AABB0 (:443)
    virtual bool process() const; // vtbl+0x0C 0x008DDC20 (:454)
    virtual void render() const; // vtbl+0x10 0x00888210 (:671)
    int cycle(bool positive) const; // 0x00888220 (:676)

private:
    static int refcount; // 0x02C034A8
};

void parse_inventory_data(xml_element* root, const char* ratio_string); // 0x008881E0 (:279)
void ovl_inventory_system_init(); // 0x008881F0 (:283)
void ovl_inventory_close(); // 0x008BF570 (:326)
void ovl_inventory_update_avail_weapons(); // 0x00888200 (:332)
bool ovl_inventory_open(control_button_action cba, float duration); // 0x008DDB80 (:337)
bool ovl_inventory_is_open(); // 0x008AABD0 (:710)
bool ovl_inventory_is_closing_or_closed(); // 0x008882A0 (:715)
void ovl_inventory_disable(bool disable); // 0x008882B0 (:720)
bool ovl_inventory_is_disabled(); // 0x008882C0 (:725)
