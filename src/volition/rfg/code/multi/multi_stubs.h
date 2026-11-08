// Original: volition/rfg/code/multi/multi_stubs.h
// Implementation: volition/rfg/code/multi/multi_stubs.cpp
//
// Empty stand-ins of the online storage (console) functions on PC, and the client connection denied callback.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"

void multi_game_load_online_storage(); // 0x009679E0 (:18)
void multi_game_invalidate_online_storage(); // 0x009679F0 (:19)
void saveload_storage_device_changed_callback(); // 0x00967A00 (:20)
bool multi_game_is_online_storage_loading(); // 0x00967A10 (:22)
bool multi_game_is_online_storage_saving(); // 0x00967A20 (:23)
void multi_game_register_client_connection_denied_callback(void (*callback)(multi_join_deny_reasons)); // 0x00967A30 (:25)
