// Original: volition/rfg/code/vehicle_ai/vehicle_idle_ai.cpp

#include "volition/rfg/code/vehicle_ai/vehicle_idle_ai.h"

#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"

// 0x00BCA1E0 (:220)
void vehicle_ai_process_idle(vehicle* vp)
{
    if (vp->vai.vai_flags.m_override_speed)
    {
        human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != NULL && !driver->is_killed())
        {
            automobile_set_handbrake(vp->get_handle(), false);
            vehicle_ai_set_accel_for_speed(&vp->vai, vp->vai.override_speed, automobile_get_speed_ms(vp->get_handle()), false);
            return;
        }
        vp->vai.vai_flags.m_override_speed = 0;
        vp->vai.override_speed = 0.0f;
    }
    automobile_set_controls_when_abandoned(vp->get_handle());
}

// 0x00BC56C0 (:254)
void vehicle_ai_exit_idle(vehicle_ai* vai)
{
}

// 0x00BCA260 (:258)
void vehicle_ai_enter_idle(vehicle_ai* vai)
{
    vai->set_state(VS_AI_IDLE);
}
