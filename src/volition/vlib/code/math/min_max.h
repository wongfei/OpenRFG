// Original: volition/vlib/code/math/min_max.h
//
// min/max/CAP templates (function templates; instantiations listed as comments).
#pragma once

// template instance: float min<float>(float a, float b); // 0x0045AA00 (.h:19)
// template instance: float max<float>(float a, float b); // 0x004B18E0 (.h:29)
// template instance: void CAP<float>(float& v, float mn, float mx); // 0x00595D40 (.h:41)
// template instance: void CAP<enum dialogue_priorities>(dialogue_priorities& v, dialogue_priorities mn, dialogue_priorities mx); // 0x00B24F70 (.h:41)
// template instance: void CAP<unsigned short>(unsigned short& v, unsigned short mn, unsigned short mx); // 0x005F8E10 (.h:41)
// template instance: void CAP<short>(short& v, short mn, short mx); // 0x00719750 (.h:41)
// template instance: void CAP<enum multi_core_state_priority>(multi_core_state_priority& v, multi_core_state_priority mn, multi_core_state_priority mx); // 0x0096A720 (.h:41)
// template instance: void CAP<unsigned int>(unsigned int& v, unsigned int mn, unsigned int mx); // 0x00968E30 (.h:41)
// template instance: void CAP<int>(int& v, int mn, int mx); // 0x004D8D30 (.h:41)
// template instance: void CAP<enum gfm_alert_levels>(gfm_alert_levels& v, gfm_alert_levels mn, gfm_alert_levels mx); // 0x00766C80 (.h:41)

// (.h:41)
template<typename T>
inline void CAP(T& v, T mn, T mx)
{
    if (v < mn)
    {
        v = mn;
    }
    else if (v > mx)
    {
        v = mx;
    }
}
