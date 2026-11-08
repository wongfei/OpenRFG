// Original: volition/rfg/code/gameplay/gameflow/gfm_control_morale.h
// Implementation: volition/rfg/code/gameplay/gameflow/gfm_control_morale.cpp
//
// District EDF control and civilian morale: values, maxima and display values per district, temporary morale, morale
// offsets, and the changes made by kills and propaganda. The destruction code reports EDF control changes.
#pragma once


class district;
class human;

void unused_dcf_civilian_morale_change(); // 0x007AFC80 (:129)
void unused_dcf_set_civilian_morale(); // 0x007AAC40 (:152)
void unused_dcf_edf_control_change(); // 0x007B4700 (:175)
void gfm_control_morale_init(); // 0x0075DA50 (:199)
void gfm_control_morale_level_init_pre(); // 0x0075DA60 (:204)
void gfm_control_morale_do_frame(); // 0x0075DAA0 (:216)
void gfm_control_morale_debug_render(); // 0x007AFD90 (:246)
float gfm_get_effective_morale(); // 0x007AADF0 (:286)
float gfm_get_effective_morale_max(); // 0x007AAF80 (:294)
float gfm_get_effective_morale_percent(); // 0x007AB000 (:302)
void gfm_set_district_morale_max(float max_morale, district* dp); // 0x007A4920 (:307)
void gfm_set_district_morale(float new_morale, district* dp); // 0x007A49F0 (:318)
void gfm_change_district_morale(float delta_morale, district* dp, const char* reason, const wchar_t* icon); // 0x007AB070 (:329)
void gfm_propaganda_morale_change(district* dp); // 0x007AB490 (:422)
int gfm_get_district_morale_display_value(district* dp); // 0x007AB4D0 (:429)
float gfm_get_district_morale(district* dp); // 0x007A4AA0 (:443)
int gfm_get_district_morale_max_display_value(district* dp); // 0x007AB570 (:454)
float gfm_get_district_morale_max(district* dp); // 0x007A4B30 (:459)
float gfm_get_district_morale_percent(district* dp); // 0x007A4B70 (:470)
void gfm_start_temporary_morale(); // 0x0075DAB0 (:482)
void gfm_end_temporary_morale(); // 0x0075DAC0 (:487)
void gfm_set_temporary_morale(float new_morale); // 0x0075DAD0 (:492)
void gfm_change_temporary_morale(float delta_morale); // 0x00773840 (:501)
bool gfm_use_temporary_morale(); // 0x0075DB30 (:506)
float gfm_get_temporary_morale(); // 0x0075DB40 (:511)
void gfm_set_temporary_morale_max(float new_morale_max); // 0x007A4BF0 (:516)
void gfm_set_morale_offset(float new_offset); // 0x0075DB50 (:529)
void gfm_change_morale_offset(float delta_morale); // 0x0075DB60 (:534)
float gfm_get_morale_offset(); // 0x0075DB90 (:539)
void gfm_set_edf_control_max(float max_control, district* dp); // 0x007AFF20 (:544)
bool gfm_set_edf_control(float new_control, district* dp); // 0x007AFFE0 (:555)
void gfm_change_edf_control(float delta_control, district* dp, const char* reason, const wchar_t* icon); // 0x007B00B0 (:569)
int gfm_get_edf_control_display_value(district* dp); // 0x007AB620 (:647)
float gfm_get_edf_control(const district* dp); // 0x007A4CE0 (:661)
float gfm_get_edf_control_max(const district* dp); // 0x007A4D20 (:672)
int gfm_get_edf_control_max_display_value(district* dp); // 0x007A4D60 (:683)
float gfm_get_edf_control_percent(const district* dp); // 0x007A4E10 (:688)
float gfm_get_edf_control_gamewide_percent(); // 0x007A4EE0 (:699)
void gfm_control_morale_human_killed(human* victim, human* attacker); // 0x007AB6E0 (:716)
