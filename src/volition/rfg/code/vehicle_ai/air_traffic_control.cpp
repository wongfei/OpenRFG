// Original: volition/rfg/code/vehicle_ai/air_traffic_control.cpp
//
// Flyers reserve a height and with it a speed, by position in a list kept in the order of their altitudes. Placement inferred.

#include "volition/rfg/code/vehicle_ai/air_traffic_control.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

// Indexed by reservation_offset + position in altitude_reservations.
static const float Altitude_list[8] = {15.0f, 20.0f, 30.0f, 40.0f, 50.0f, 55.0f, 60.0f, 65.0f}; // 0x016EB810
static const float Speed_list[8] = {30.0f, 50.0f, 30.0f, 15.0f, 10.0f, 10.0f, 5.0f, 5.0f}; // 0x016EB830

air_traffic_control_object Air_traffic_controller; // 0x016EB8B4

// 0x00BCBF00 (:97)
air_traffic_control_object::air_traffic_control_object()
    : reservation_offset(1)
{
}

// 0x00BC8B80
air_traffic_control_object::~air_traffic_control_object()
{
}

// 0x00BC8B90 (:57): drops flyers that are gone, driverless, on fire or with the engine off.
void air_traffic_control_object::clear_stale_reservations()
{
    for (int i = 0; i < altitude_reservations.size(); i++)
    {
        const unsigned int handle = altitude_reservations[i];
        const vehicle* vp = nullptr;
        if (handle != 0xFFFFFFFF)
        {
            const object* objp = World.object_from_handle(handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
            {
                vp = static_cast<const vehicle*>(objp);
            }
        }
        if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr || vehicle_is_on_fire(vp) || !vehicle_engine_is_running(vp))
        {
            altitude_reservations.remove(i);
            i--;
        }
    }
}

// 0x00BC8C50 (:84): 0 from two flyers on, so that one flyer flies at the second height.
void air_traffic_control_object::update_reservation_offset()
{
    reservation_offset = 1 - altitude_reservations.size() / 2;
    if (reservation_offset < 0)
    {
        reservation_offset = 0;
    }
    else if (8 < reservation_offset)
    {
        reservation_offset = 8;
    }
}

// 0x00BCBF30 (:106)
float air_traffic_control_object::request_altitude_clearance(unsigned int flyer_handle, float current_altitude)
{
    clear_stale_reservations();
    if (altitude_reservations.size() == 0)
    {
        altitude_reservations.add(flyer_handle);
        reservation_offset = 1;
        return Altitude_list[1];
    }
    // 8: the farray is full.
    if (altitude_reservations.size() == 8)
    {
        return current_altitude;
    }
    update_reservation_offset();
    int insert_at = -1;
    for (int i = 0; i < altitude_reservations.size(); i++)
    {
        const float altitude = Altitude_list[reservation_offset + i];
        if (altitude_reservations[i] == flyer_handle)
        {
            return altitude;
        }
        if (insert_at == -1 && current_altitude < altitude)
        {
            insert_at = i;
        }
    }
    if (insert_at == -1)
    {
        // Original quirk: higher than every reservation goes to the bottom of the list, not the top.
        insert_at = 0;
    }
    altitude_reservations.add(altitude_reservations[altitude_reservations.size() - 1]);
    for (int i = altitude_reservations.size() - 2; insert_at < i; i--)
    {
        altitude_reservations[i] = altitude_reservations[i - 1];
    }
    altitude_reservations[insert_at] = flyer_handle;
    update_reservation_offset();
    return Altitude_list[reservation_offset + insert_at];
}

// 0x00BC8C80 (:159): 1000 m/s for a flyer without a reservation.
float air_traffic_control_object::request_speed_clearance(unsigned int flyer_handle)
{
    for (int i = 0; i < altitude_reservations.size(); i++)
    {
        if (altitude_reservations[i] == flyer_handle)
        {
            return Speed_list[reservation_offset + i];
        }
    }
    return 1000.0f;
}
