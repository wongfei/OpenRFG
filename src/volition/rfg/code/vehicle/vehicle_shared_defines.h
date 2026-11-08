// Original: volition/rfg/code/vehicle/vehicle_shared_defines.h
//
// Vehicle classification enums and seat indices. Placement inferred.
#pragma once

enum vehicle_class_type
{
    VCL_AUTOMOBILE = 0,
    VCL_FLYER = 1,
    VCL_WALKER = 2,
    NUM_VEHICLE_CLASSES = 3,
};

enum base_vehicle_types
{
    BVT_INVALID = -1,
    BVT_NONE = 0,
    BVT_COURIER = 1,
    BVT_AMBULANCE = 2,
    BVT_TANK = 3,
    BVT_ART_TANK = 4,
    BVT_BOMBER = 5,
    BVT_SPECIAL = 6,
    NUM_BASE_VEHICLE_TYPES = 7,
};

enum vehicle_road_preference
{
    VRP_NONE = 0,
    VRP_HIGHWAY = 1,
    VRP_NO_HIGHWAY = 2,
    NUM_VEHICLE_ROAD_PREFERENCE_TYPES = 3,
};

enum vehicle_seat_index
{
    VSI_BEST = -2,
    VSI_INVALID = -1,
    VSI_DRIVER = 0,
    VSI_PASSENGER_1 = 1,
    VSI_PASSENGER_2 = 2,
    VSI_PASSENGER_3 = 3,
    VSI_PASSENGER_4 = 4,
    VSI_PASSENGER_5 = 5,
    VSI_PASSENGER_6 = 6,
    VSI_PASSENGER_7 = 7,
    VSI_PASSENGER_8 = 8,
    VSI_PASSENGER_9 = 9,
    VSI_PASSENGER_10 = 10,
    NUM_VEHICLE_SEATS = 11,
};
