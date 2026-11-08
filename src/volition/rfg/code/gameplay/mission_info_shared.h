// Original: volition/rfg/code/gameplay/mission_info_shared.h
//
// Seam only (M3): mission_handle, the index of a mission in the mission table (missions.h, mission_info.h), held by
// player starts and the activity nodes. The rest of the shared mission data comes with the gameplay tier (M5).
#pragma once

enum mission_handle
{
    INVALID_MISSION_HANDLE = -1,
    MISSION_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};
