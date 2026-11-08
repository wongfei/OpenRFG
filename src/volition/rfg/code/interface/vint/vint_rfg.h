// Original: volition/rfg/code/interface/vint/vint_rfg.h
// Implementation: volition/rfg/code/interface/vint/vint_rfg.cpp
//
// The game side of vint: vint_rfg_system_init (Lua on dlmalloc, the callback sets, the Vint memory pool, the project),
// the callbacks themselves (memory, localization, sound, bitmap/font/sound resources, rendering through gr and rl_draw's
// 2D path: bitmaps, circles, text, lines, boxes, stencil and clip, frame buffers, the map and video elements) and the
// input map (vint_rfg_update_input_map).
#pragma once

#include "keen/base/language.hpp"

class vint_variant;

const wchar_t* get_localized_string(const char* _key); // 0x008DD050 (:375)
const wchar_t* get_localized_data(const char* _format, const vint_variant* _data); // 0x008A1D60 (:400)
bool vint_rfg_font_is_reversed(int _handle); // 0x008A2390 (:684)
bool vint_rfg_system_init(); // 0x008DD0E0 (:1370)
void vint_rfg_system_shutdown(); // 0x008A25B0 (:1486)
void vint_rfg_update_input_map(); // 0x008A25D0 (:1498)
void vint_rfg_set_language(keen::LanguageId lang); // 0x008BBF90 (:1518)
