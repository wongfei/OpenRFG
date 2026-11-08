// Original: volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h
// Implementation: volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.cpp
//
// Gameplay mode helpers shared with the interface.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"

const char* multi_gameplay_mode_get_display_name(multi_gameplay_mode mode); // 0x0094EDC0 (:50)
const char* multi_gameplay_mode_get_internal_name(multi_gameplay_mode mode); // 0x0094EDE0 (:60)
multi_gameplay_mode multi_gameplay_mode_from_name(const char* name); // 0x0094EE00 (:72)
