// Original: volition/vlib/code/input/mouse.h
// Implementation: volition/vlib/code/input/mouse.cpp
//
// Mouse state (Mouse_buttons[5], Mouse_scroll[3][2] in input_internal.h). mouse_poll runs from controls_poll once per
// sim frame; rfg_do_frame calls mouse_flush after the gameseq step (decompiled).
#pragma once

// The ui scale and offset of the scaled mouse position (statics in vlib_unity; the document manager sets them).
extern float Mouse_x_scale; // 0x01518A5C
extern float Mouse_y_scale; // 0x01518B58
extern float Mouse_x_offset; // 0x01CE86E0
extern float Mouse_y_offset; // 0x01CE86E4
extern int Mouse_dx; // 0x01CDAE48
extern int Mouse_dy; // 0x01CDB904
extern int Mouse_wheel; // 0x01CDBA10
extern float Mouse_scaled_x; // 0x01CE2764, the cursor in the 640x480 interface space (inferred)
extern float Mouse_scaled_y; // 0x01CDA5C0
extern int Mouse_dwheel; // 0x01CDA5C4, the wheel's movement this frame (external since UI6)
extern float Mouse_sensitivity; // 0x01518A30, the options' mouse sensitivity (1.0 in .data)
extern float Mouse_pixel_multiplier; // 0x01518A34, analog value per pixel of mouse movement (0.09 in .data)

void reset_scroll(); // 0x00568EA0 (:27)
void mouse_init(); // 0x0059A3F0 (:59)
void mouse_auto_centering(bool center_it); // 0x0059A4C0 (:71)
void mouse_mark_button(unsigned int button_id, bool down); // 0x0059A4D0 (:77)
void mouse_update_from_os(); // 0x00568F10 (:95)
void mouse_flush_from_os(); // 0x00568FF0 (:103)
void mouse_poll(); // 0x005C3660 (:121)
void mouse_get_location(unsigned int& x, unsigned int& y, int& wheel); // 0x00569090 (:166)
void mouse_set_location(unsigned int x, unsigned int y, int wheel); // 0x0059A5B0 (:177)
void mouse_set_wheel_pos(int wheel); // 0x005690C0 (:186)
void mouse_get_deltas(int& x, int& y, int& wheel); // 0x005690D0 (:196)
bool mouse_is_pressed(unsigned int button_id); // 0x00569100 (:205)
bool mouse_did_repeat(unsigned int button_id); // 0x00569110 (:210)
bool mouse_just_pressed(unsigned int button_id); // 0x00569140 (:217)
bool mouse_just_released(unsigned int button_id); // 0x00569150 (:224)
bool mouse_double_clicked(unsigned int button_id); // 0x00569160 (:231)
void mouse_flush(); // 0x00569170 (:261)
bool mouse_is_supported(); // 0x00569210 (:333)
bool mouse_is_connected(); // 0x00569220 (:342)
bool mouse_did_scroll(unsigned int which_axis, bool positive); // 0x00569230 (:353)
bool mouse_just_moved(); // 0x00569250 (:359)
bool mouse_just_scrolled_wheel(); // 0x00569280 (:367)
bool mouse_over_window(); // 0x0059A5D0 (:372)
void mouse_update_visibility(); // 0x005C3790 (:377)

// ---- mouse_generic.cpp, the PC back end behind the functions above (the PDB has no header of its own; placement
// inferred)
void mouse_generic_init(); // 0x00569290 (mouse_generic.cpp:42)
void mouse_generic_set_location(unsigned int x, unsigned int y); // 0x005692A0 (mouse_generic.cpp:46)
void mouse_generic_auto_centering(bool center_it); // 0x005692B0 (mouse_generic.cpp:55)
void mouse_generic_update_visibility(); // 0x005B88D0 (mouse_generic.cpp:60)
void mouse_generic_update_front_thread(); // 0x005692C0 (mouse_generic.cpp:75)
void mouse_generic_poll(); // 0x005B88F0 (mouse_generic.cpp:84)
bool mouse_generic_over_window(); // 0x00569300 (mouse_generic.cpp:147)
