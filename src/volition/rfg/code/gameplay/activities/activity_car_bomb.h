// Original: volition/rfg/code/gameplay/activities/activity_car_bomb.h
// Implementation: volition/rfg/code/gameplay/activities/activity_car_bomb.cpp
//
// Car bomb activity (a free activity): driving a rigged vehicle to its target; the activity class with its HUD and the
// start / force kill entry points.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"

class vehicle;

// size 0x44C
class activity_car_bomb_data : public activity_base_free
{
public:
    activity_car_bomb_data(); // 0x0077DD20 (.h:16)
    virtual void level_init(); // vtbl+0x24 0x007572E0 (:27)
    virtual void activity_end(); // vtbl+0x3C 0x00796A80 (:140)
    virtual void activity_kill(); // vtbl+0x40 0x00796AB0 (:148)
    virtual void start_car_bomb(vehicle* alt_vp); // vtbl+0xDC 0x007ACF00 (:106)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x0076A220 (:83)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x007573C0 (:77)
    virtual const char* get_title() const; // vtbl+0x80 0x00757330 (:40)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00757340 (:53)
    virtual bool get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const; // vtbl+0xCC 0x00757350 (:59)
    virtual void reset(); // vtbl+0xE0 0x00757320 (:34)
    virtual ~activity_car_bomb_data(); // vtbl+0x00 0x00768CB0

private:
    unsigned int vehicle_handle; // 0x448
};

extern activity_car_bomb_data Car_bomb_data; // 0x01651E00

void activity_car_bomb_start(vehicle* alt_vp); // 0x007ACF80 (:163)
void activity_car_bomb_force_kill(); // 0x00796B10 (:168)
bool activity_car_bomb_is_active(); // 0x007573D0 (:173)
