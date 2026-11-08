// Original: volition/rfg/code/nvidia_highlights.h
//
// NVIDIA ShadowPlay highlights (saved kill/death videos and screenshots; nvidia_highlights.cpp, stub_impl.md S2). The
// group and highlight ids are header statics in the original (one copy per including TU; strings read from rfg.exe).
#pragma once

static const char* s_groupIdGameplay = "gameplay";
static const char* s_highlightIdKillingSpree = "killingSpree";
static const char* s_highlightIdKill = "kill";
static const char* s_highlightIdDeath = "death";
static const char* s_highlightIdDestruction = "destruction";
static const char* s_highlightIdExplosion = "explosion";

void nvidia_highlights_save_screenshot(const char* highlightId, const char* groupId); // 0x005D1650 (:447)
void nvidia_highlights_save_video(const char* highlightId, const char* groupId, int startDelta, int endDelta); // 0x005D16C0 (:463)

// orig_main.md OM3: keen::rfg::Application starts, polls and shuts down the highlights.
bool nvidia_highlights_init(); // 0x005D4060 (:131)
void nvidia_highlights_shutdown(); // 0x005D1360 (:177)
void nvidia_highlights_poll(); // 0x005D70D0 (:201)
void nvidia_highlights_ask_user_for_permission(); // 0x005D4170 (:251)

// multiplayer.md MP1: called by the original game loop.
void nvidia_highlights_close_group(const char* groupId, bool destroy); // 0x005D15E0 (:427)
void nvidia_highlights_open_group(const char* groupId); // 0x005D1570 (:408)

bool nvidia_highlights_is_enabled(); // 0x005D13B0 (:280)
unsigned int nvidia_highlights_get_num_highlights(); // 0x005D13F0 (:287)
void nvidia_highlights_configure(); // 0x005D1510 (:391)
void nvidia_highlights_open_summary(const char* groupId); // 0x005D4230 (:481)
void nvidia_highlights_request_num_highlights(const char* groupId); // 0x005D42D0 (:501)
