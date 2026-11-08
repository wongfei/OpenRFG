// Original: volition/rfg/code/camera/camera_shake.h
// Implementation: volition/rfg/code/camera/camera_shake.cpp
//
// Camera shakes, read from the camera shake table: started by name, from explosions, and from heavy impacts on
// buildings and their stress; the active shakes add up to the camera's shake intensity.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0x30
struct weight_times
{
    float time; // 0x00
    float wobble; // 0x04
    float destable; // 0x08
    float wander; // 0x0C
    float jitter; // 0x10
    float oscillate1; // 0x14
    float oscillate2; // 0x18
    float direct; // 0x1C
    float wander_direct; // 0x20
    float blur; // 0x24
    float controller_vibration_strong; // 0x28
    float controller_vibration_weak; // 0x2C
};

// size 0x280
struct camera_shake
{
    camera_shake(); // 0x006C7020

    char name[32]; // 0x00
    int foley_ref; // 0x20
    int foley_play_id; // 0x24
    vector wobble; // 0x28
    float wobble_variation; // 0x34
    vector destable; // 0x38
    float destable_frequency; // 0x44
    vector wander; // 0x48
    float wander_frequency; // 0x54
    vector jitter; // 0x58
    vector oscillate1; // 0x64
    float oscillate1_frequency; // 0x70
    vector oscillate2; // 0x74
    float oscillate2_frequency; // 0x80
    vector direct; // 0x84
    vector wander_direct; // 0x90
    weight_times weights[10]; // 0x9C
    int num_weights; // 0x27C
};

extern bool Camera_shake_enabled; // 0x01648859

void camera_shake_system_init();// 0x006E0BE0 (:373)
void camera_shake_level_init_post(); // 0x006C6DD0 (:386)
bool camera_shake_play(camera_shake* shake, float intensity, bool ignore_disabled); // 0x006C6E00 (:395)
bool camera_shake_play(const char* name, float intensity, bool ignore_disabled); // 0x006D4230 (:456)
camera_shake* camera_shake_get(const char* name); // 0x006D4290 (:463)
bool camera_shake_is_active(const char* name); // 0x006D42E0 (:471)
bool camera_shake_is_active(const camera_shake* shake); // 0x006C6F60 (:477)
void camera_shake_start(float intensity, float dist_sqr); // 0x006D4340 (:497)
void camera_shake_start_from_explosion(const vector& pos, float magnitude); // 0x006D46E0 (:518)
void camera_shake_start_from_impact(float energy, float dist_sqr); // 0x006D48A0 (:534)
void camera_shake_start_from_stress(float energy, float dist_sqr); // 0x006D4980 (:555)
void camera_shake_stop(const camera_shake* shake); // 0x006C6F90 (:577)
void camera_shake_stop(); // 0x006C7000 (:592)
void camera_shake_process(); // 0x006CCDF0 (:601)
float camera_shake_get_intensity(); // 0x006C7030 (:978)
