// Original: volition/rfg/code/destruction/rfg_destruction.h
// Implementation: volition/rfg/code/destruction/rfg_destruction.cpp
//
// Frame hooks of the destruction system. rfg_do_frame_pre runs before the simulation (see rfg_mover.h);
// rfg_do_frame_post runs the destruction trails, the shape cutter, the effect candidates and the load balancer
// (rfgl_do_frame) (decompiled). rfg_message_* is an on-screen debug message list.
#pragma once

void rfg_level_init_pre(); // 0x00732490 (:383)
void rfg_level_shutdown(); // 0x0071B8B0 (:403)
void rfg_do_frame_pre(); // 0x0073CDE0 (:416)
void rfg_do_frame_post(); // 0x00737CC0 (:435)
void rfg_message_init_pre(); // 0x007114D0 (:1921)
void rfg_message_add(char* m, bool important); // 0x007114E0 (:1926)
void rfg_message_process_and_render(); // 0x00711520 (:1933)
