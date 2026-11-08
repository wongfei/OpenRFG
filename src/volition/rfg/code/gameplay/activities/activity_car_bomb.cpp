// Original: volition/rfg/code/gameplay/activities/activity_car_bomb.cpp

#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/objects/object_bftp_node.h"
#include "volition/vlib/code/util/string_util.h"

activity_car_bomb_data Car_bomb_data; // 0x01651E00

// 0x0077DD20 (.h:16)
activity_car_bomb_data::activity_car_bomb_data()
    : activity_base_free(ACTIVITY_MODE_CAR_BOMB)
{
}

// 0x00768CB0
activity_car_bomb_data::~activity_car_bomb_data()
{
}

// 0x007572E0 (:27)
void activity_car_bomb_data::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    reset();
}

// 0x00757320 (:34)
void activity_car_bomb_data::reset()
{
    vehicle_handle = 0xFFFFFFFF;
}

// 0x00757330 (:40)
const char* activity_car_bomb_data::get_title() const
{
    return "ACT_BOMB";
}

// 0x00757340 (:53)
const char* activity_car_bomb_data::get_icon_image_name() const
{
    return "ui_hud_diversion_icon_carbomb";
}

// 0x00757350 (:59). Original: answers true with a null buffer too. Magic numbers: the time the HUD shows.
bool activity_car_bomb_data::get_ui_display_info(char* value_text, int& time_til_end, int buffer_size) const
{
    if (value_text != nullptr)
    {
        if (human_in_vehicle(player_get_local_player(), vehicle_handle))
        {
            time_til_end = 500000;
            _snprintf_safe(value_text, buffer_size, "f");
        }
        else
        {
            time_til_end = 1000;
            _snprintf_safe(value_text, buffer_size, "t");
        }
    }
    return true;
}

// 0x007573C0 (:77)
bool activity_car_bomb_data::render_hud(int x_pos, int y_pos)
{
    return false;
}

// 0x0076A220 (:83)
void activity_car_bomb_data::activity_do_frame_active()
{
    if (vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            if (objp->is_killed())
            {
                blast_from_the_past_bomb_destroyed();
                activity_end();
            }
            return;
        }
    }
    activity_end();
}

// 0x007ACF00 (:106)
void activity_car_bomb_data::start_car_bomb(vehicle* alt_vp)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    vehicle* vp = nullptr;
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vp = static_cast<vehicle*>(objp);
        }
    }
    if (vp == nullptr)
    {
        vp = alt_vp;
    }
    if (vp == nullptr)
    {
        return;
    }
    if (vp->m_bomb_status == VEHICLE_BOMB_NONE)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (activity_base_free::activity_start())
    {
        vehicle_handle = vp->get_handle();
    }
}

// 0x00796A80 (:140)
void activity_car_bomb_data::activity_end()
{
    blast_from_the_past_reset_last_bomb();
    activity_base_free::activity_end(); // rfg.exe inlines it
    reset();
}

// 0x00796AB0 (:148)
void activity_car_bomb_data::activity_kill()
{
    if (vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && static_cast<vehicle*>(objp)->m_bomb_status != VEHICLE_BOMB_NONE)
        {
            vehicle_set_bomb_status(static_cast<vehicle*>(objp), VEHICLE_BOMB_NONE);
        }
    }
    blast_from_the_past_reset_last_bomb();
    activity_base_free::activity_kill(); // rfg.exe inlines it
    reset();
}

// 0x007ACF80 (:163)
void activity_car_bomb_start(vehicle* alt_vp)
{
    Car_bomb_data.start_car_bomb(alt_vp);
}

// 0x00796B10 (:168)
void activity_car_bomb_force_kill()
{
    Car_bomb_data.activity_car_bomb_data::activity_kill(); // rfg.exe inlines it
}

// 0x007573D0 (:173)
bool activity_car_bomb_is_active()
{
    return Car_bomb_data.is_active(); // rfg.exe reads the flag
}
