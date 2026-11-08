// Original: volition/rfg/code/vehicle/vehicle_damage_area.h
//
// A damage area of a vehicle (engine, fuel tank...): hit points and the smoke and fire effect instances shown as it
// takes damage. Placement of vehicle_damage_area_type inferred.
#pragma once

enum vehicle_damage_area_type
{
    VDA_ENGINE = 0,
    NUM_VEHICLE_DAMAGE_AREAS = 1,
};

// size 0x14
class vehicle_damage_area
{
public:
    enum constants
    {
        EFFECT_SMOKE = 0,
        EFFECT_FIRE = 1,
        MAX_EFFECT_INSTANCES = 2,
    };

    void init(vehicle_damage_area_type type, int max_hit_points); // 0x00B62F20 (.h:67)

    vehicle_damage_area_type m_type; // 0x00
    int m_max_hit_points; // 0x04
    int m_hit_points; // 0x08
    unsigned int m_effect_instances[2]; // 0x0C
};
