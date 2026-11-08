// Original: volition/rfg/code/cutscene/cutscene_audio.h
// Implementation: volition/rfg/code/cutscene/cutscene_audio.cpp
//
// Cutscene audio: priming the cutscene's streamed audio ahead of playback (blocking or with a callback), playing,
// pausing and cancelling it.
#pragma once


struct cutscene_current_info;

void cs_manage_audio_start(); // 0x00705E50 (:106)
bool cutscene_prime_audio(cutscene_current_info* info); // 0x00705E60 (:112)
void cutscene_cancel_audio_prime(cutscene_current_info* info); // 0x00705F00 (:140)
void cutscene_audio_pause(); // 0x00705F30 (:155)
void cutscene_audio_unpause(); // 0x00705F40 (:163)
void cutscene_audio_play(cutscene_current_info* cur_cutscene); // 0x00705F50 (:172)
void cs_audio_prepare_blocking(cutscene_current_info* cur_cutscene); // 0x00705F80 (:188)
