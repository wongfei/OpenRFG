// The --audio-log test seam of OpenRFG (SFX.md deviation 3.1; new code, not recovered): a tab-separated line per
// audiolib_cue_play result (frame, cue name, play id, error code, 3D position or "2d").
#pragma once

// Opens the log and installs audio_lib's log hooks; false when the file cannot be written or the program has no
// audio library.
bool main_audio_log_start(const char* path);
