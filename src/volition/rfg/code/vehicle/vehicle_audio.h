// Original: volition/rfg/code/vehicle/vehicle_audio.h
// Implementation: volition/rfg/code/vehicle/vehicle_audio.cpp
//
// Vehicle sounds on top of the audio_lib engines: enter/exit, engine RPM and load, horn, siren, megaphone, chassis
// sounds, wheel foley and pass-bys.
#pragma once

#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"

class automobile;
class human;
class human_voice_persona;
class vector;
class vehicle;

void vehicle_audio_level_init_pre(); // 0x00B64110 (:169)
void automobile_handle_chassis_sounds(automobile* ap); // 0x00B8AC20 (:176)
void vehicle_audio_handle_player_enter(vehicle* vp); // 0x00B64120 (:244)
void vehicle_handle_enter_audio(vehicle* vp, bool player); // 0x00B75C40 (:251)
void vehicle_handle_exit_audio(vehicle* vp, bool stop_engine, bool player, bool streamout); // 0x00B64140 (:287)
void vehicle_audio_engine_update(vehicle* const car); // 0x00B967B0 (:319)
float automobile_audio_get_load_value(const automobile* const ap); // 0x00B75D40 (:482)
void automobile_get_horn_world_pos(vector& pos, const vehicle* vp); // 0x00B64210 (:520)
void automobile_audio_horn_update(automobile* const car); // 0x00B8AFB0 (:525)
void automobile_audio_siren_update(automobile* const car, human* const hp); // 0x00B8B0F0 (:644)
void automobile_audio_megaphone_update(automobile* const car); // 0x00B64300 (:679)
void automobile_audio_chassis_sound_update(automobile* const car); // 0x00B64440 (:694)
void automobile_play_megaphone(voice_line_handle line_handle, human_voice_persona* hvpp, unsigned int a_handle); // 0x00B75EF0 (:703)
void automobile_honk_horn(unsigned int a_handle); // 0x00B75F50 (:728)
void automobile_release_horn(unsigned int a_handle); // 0x00B760C0 (:776)
void vehicle_audio_process_pass_by(vehicle* vp); // 0x00B76130 (:828)
void automobile_process_wheel_foley(automobile* ap, int& play_id, bool play_foley, bool stop_foley, int cue, bool restart, float volume, const vector& pos, const vector& velocity); // 0x00B76490 (:918)
