// Original: volition/vlib/code/time/timestamp_accurate.h
// Implementation: volition/vlib/code/time/timestamp_accurate.cpp
//
// A pausable millisecond clock read from the real-time timer instead of the frame time (Timestamp_accurate_* statics).
#pragma once

void timestamp_accurate_increment(); // 0x0058EAB0 (:52)
int timestamp_accurate_pausable_get_elapsed_ms(int time_previous); // 0x0058EB20 (:66)
int timestamp_accurate_pausable_get_current_ms(); // 0x0058EB40 (:75)
