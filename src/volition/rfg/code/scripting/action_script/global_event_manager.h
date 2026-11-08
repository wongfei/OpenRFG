// Original: volition/rfg/code/scripting/action_script/global_event_manager.h
// Implementation: volition/rfg/code/scripting/action_script/global_event_manager.cpp
//
// The global event manager (gem): callbacks registered per event type, called when an event is posted.
#pragma once


struct event_data;
struct notify_info;

void gem_system_init(); // 0x00B1FA90 (:31)
void gem_level_init_pre(); // 0x00B1FAA0 (:37)
void gem_level_shutdown(); // 0x00B3CC60 (:43)
notify_info* gem_register_for_event(const notify_info& info); // 0x00B31480 (:53)
void gem_unregister_for_event_internal(notify_info* handle); // 0x00B37B60 (:70)
void gem_unregister_for_event(notify_info* handle); // 0x00B37BB0 (:81)
void gem_do_frame(); // 0x00B1FAB0 (:90)
void gem_post_event(event_data& data); // 0x00B37C10 (:97)
