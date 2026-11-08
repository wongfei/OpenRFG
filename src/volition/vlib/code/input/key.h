// Original: volition/vlib/code/input/key.h
// Implementation: volition/vlib/code/input/key.cpp
//
// Keyboard state. Key changes are queued (key_mark_change, ring buffer Key_state_changes[32]) and applied to
// Key_states[256] (input_internal.h) by key_poll, which controls_poll calls once per sim frame (inferred from the
// globals each function touches).
#pragma once

extern bool All_keys_masked; // 0x01CE86D6

void key_convert_keen_key_to_text(char* output_buffer, unsigned int output_capacity, unsigned char key_event, bool holding_shift, bool holding_altgr, bool numlock_activated); // 0x00568820 (:168)
bool key_is_german_keyboard(); // 0x005688C0 (:271)
void key_mark_change(unsigned int code, bool down); // 0x00568900 (:295)
void key_mark(unsigned int code, bool down); // 0x0059A110 (:323)
void key_init(); // 0x0059A270 (:396)
void key_flush(); // 0x00568940 (:409)
void key_flush_key(unsigned int scancode); // 0x00568980 (:426)
void key_to_text(char* output_buffer, unsigned int output_capacity, unsigned int keycode); // 0x005689D0 (:443)
unsigned int key_in(); // 0x00568A60 (:472)
unsigned int key_in_peek(); // 0x00568A70 (:477)
bool key_pressed(unsigned int scancode, bool* just_pressed); // 0x00568A90 (:484)
bool key_down(unsigned int scancode); // 0x00568AC0 (:497)
bool key_just_pressed(unsigned int scancode); // 0x00568AF0 (:505)
bool key_pressed_with_mods(unsigned int code, bool* just_pressed); // 0x00568B20 (:510)
bool key_down_with_mods(unsigned int code); // 0x00568C20 (:543)
bool key_just_pressed_with_mods(unsigned int code); // 0x00568D10 (:576)
bool key_query_modifier(unsigned int key); // 0x00568E00 (:615)
void key_poll(); // 0x0059A290 (:633)
void key_mask(unsigned int scancode); // 0x00568E40 (:693)
void key_mask_all(); // 0x00568E60 (:700)
bool key_is_masked(unsigned int scancode); // 0x00568E70 (:706)
